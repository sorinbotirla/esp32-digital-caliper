#include <WiFiClient.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebSockets.h>
#include <WebSocketsServer.h>
#include <driver/adc.h>

#define ESP32_DCA_VERSION "1.0.0"


bool client_mode = 0; // 1 if you want to connect to an existing wifi network, 0 if you want to create a hotspot (AP) with the wifi details
bool stream_mode = 1;
volatile bool debug_mode = 0;

// WiFi configuration
const char* ssid = "YOUR_WIFI_SSID"; // change this to your wifi name
const char* password = "YOUR_WIFI_PASSWORD"; // change this to your wifi password

// the digital caliper pinout (with the screen on top)
// GND DATA(SDA) CLOCK(CLK) VCC(1.5V)

// ESP32 Pin Configuration
int dataPin = 2; // data pin
int clockPin = 3; // clock pin


#define ADC_TRESHOLD 800
#define DEBUG_SIGNAL 0
#define BIT_READ_TIMEOUT 100
#define PACKET_READ_TIMEOUT 250
#define PACKET_BITS       24
#define MIN_RANGE -(1<<20)
#define DCA_BUFFER_SIZE  0x1000

// HTML and JS files as string literals in program memory
const char DCA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
   <head>
      <style>
          * {
            font-family: sans-serif, Arial; 
            line-height: 1; 
            box-sizing: border-box;
          }
          html, body {
            padding: 0; 
            margin: 0; 
            background: #f7f7f7;
          }
          #dcaApp {
            position: relative; 
            padding: 20px; 
            text-align: center;
          }
          #dcaValue {
            color:#000;
            background-color:#fff;
            text-align:center;
            display:block;
            width: 100%;
            max-width: 850px;
            font-size: 70px;
            padding: 50px;
            margin: 30px auto;
            border-radius: 10px;
            /* border: 1px solid #ddd; */
            box-shadow: 0 2px 3px 0 rgba(0,0,0,0.1);
          }
      </style>
   </head>
   <body>
      <div id="dcaApp">
        <h1>ESP32 Digital Caliper</h1>
        <div>
          X Axis (mm):
          <span id="dcaValue">No Data (disconnected)</span>
          <br>
        </div>
      </div>
      <script src="/dca.js"></script>
   </body>
</html>
)rawliteral";

// FILTERED JS: show mm, only allow values within 50 units (milimeters) of the last, unless two in a row
const char DCA_JS[] PROGMEM = R"rawliteral(
var ws = null;
var lastValue = null;
var pendingJump = null;

function OpenWebsocket(address) {
    ws = new WebSocket(address);

    ws.onopen = function () {
        document.getElementById("dcaValue").textContent = "Connected";
        lastValue = null;
        pendingJump = null;
    };

    ws.onclose = function () {};

    ws.onerror = function (error) {
        console.log('WebSocket Error: ' + error);
    };

    ws.onmessage = function (event) {
        if (event.data[0] == '{') {
            var msg = JSON.parse(event.data);
            var value = msg.axis0;
            if (typeof value !== "number") return;

            var mmValue = value / 100.0;

            if (lastValue === null) {
                lastValue = value;
                document.getElementById("dcaValue").textContent = mmValue.toFixed(2) + " mm";
                return;
            }

            var delta = value - lastValue;

            if (Math.abs(delta) <= 50) {
                lastValue = value;
                pendingJump = null;
                document.getElementById("dcaValue").textContent = mmValue.toFixed(2) + " mm";
            } else {
                if (pendingJump !== null && Math.abs(value - pendingJump) <= 50) {
                    lastValue = value;
                    pendingJump = null;
                    document.getElementById("dcaValue").textContent = mmValue.toFixed(2) + " mm";
                } else {
                    pendingJump = value;
                }
            }
        }
    }
}

function CloseWebsocket() {
    ws.close();
}

window.onload = function() {    
    OpenWebsocket('ws://' + location.host + ':81');
};
)rawliteral";

// DCA 1-Reading class
struct Reading
{  
    uint32_t timestamp;
    int32_t milimeters;

    Reading(): timestamp(millis()), milimeters(MIN_RANGE)  {}

    Reading& operator=(const Reading& obj) {
      timestamp = obj.timestamp;
      milimeters = obj.milimeters;
      return *this;
    }    
};

// DCA circular buffer
Reading dca_buffer[DCA_BUFFER_SIZE] = {};
size_t dca_index = 0;

unsigned char eepromSignature = 0x5A;


WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void log(const char *fmt, ... )
{  
  char buf[128];
  va_list args;
  va_start (args, fmt);
  vsnprintf(buf, 128, fmt, args);
  va_end (args);
  Serial.println(buf);
}

// capped read: -1 (timeout), 0, 1
int getBit() {
    int data;

    if (debug_mode) {
      log("CLK:%6d DATA:%6d\n", analogRead(clockPin), analogRead(dataPin));
    }
        
    int readTimeout = millis() + BIT_READ_TIMEOUT;
    while (analogRead(clockPin) > ADC_TRESHOLD) {
      if (millis() > readTimeout)
        return -1;
    }
    
    while (analogRead(clockPin) < ADC_TRESHOLD) {
      if (millis() > readTimeout)
        return -1;
    }
    
    data = (analogRead(dataPin) > ADC_TRESHOLD)?1:0;
    return data;
}

// read one full packet
long getPacket() 
{
    long packet = 0;
    int readTimeout = millis() + PACKET_READ_TIMEOUT;

    int bitIndex = 0;
    while (bitIndex < PACKET_BITS) {
      int bit = getBit();
      if (bit < 0 ) {
        if (millis() > readTimeout) {
          return -1;
        }
        bitIndex = 0;
        packet = 0;
        continue;
      }

      packet |= (bit & 1)<<bitIndex;
      bitIndex++;
    }
    return packet;
}

// convert a packet to signed milimeters
long getMilimeters(long packet)
{
  if (packet < 0)
    return MIN_RANGE;
    
  long data = (packet & 0xFFFFF)*( (packet & 0x100000)?-1:1);

  if (packet & 0x800000) {
        data = data*254/200;
  }
  return data;
}

void spcTask( void * parameter )
{
    uint32_t lastReadTime = millis();

    for(;;) { 
      long packet = getPacket();
      
      if (packet < 0) {
        if (millis() > lastReadTime + PACKET_READ_TIMEOUT) {
          lastReadTime = millis();
          log("* %d: no SPC data", lastReadTime);
        }
      } else {
        size_t new_dca_index = (dca_index+1) % DCA_BUFFER_SIZE;
        dca_buffer[new_dca_index].timestamp = millis();
        dca_buffer[new_dca_index].milimeters = getMilimeters(packet);
        dca_index = new_dca_index;

        // broadcast to all websocket clients
        char buf[128];
        sprintf(buf, "{\"axis0\":%d,\"ts\":%d}", dca_buffer[new_dca_index].milimeters, dca_buffer[new_dca_index].timestamp);
        webSocket.broadcastTXT(buf);
      }
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            log("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                log("[%u] Connected from %s url: '%s'\n", num, webSocket.remoteIP(num).toString().c_str(), payload);
                webSocket.sendTXT(num, "Connected");
            }
            break;
        case WStype_TEXT:
            log("[%u] get Text: %s\n", num, payload);
            break;
        case WStype_BIN:
            log("[%u] get binary length: %u\n", num, length);
            break;
        case WStype_ERROR:      
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
          break;
    }
}

void setup()
{
  pinMode(dataPin, INPUT);     
  pinMode(clockPin, INPUT);

  EEPROM.begin(512);
  delay(20);

  Reading start_reading;
  dca_buffer[dca_index] = start_reading;

  Serial.begin(115200);
  log("ESP32 Digital Caliper %s initialized.", ESP32_DCA_VERSION);

  analogReadResolution(11);
  analogSetAttenuation(ADC_6db);

  String essid = ssid;
  String epassword = password;
  
  if (client_mode) 
  {
    essid = "";
    epassword = "";

    if (EEPROM.read(128) != eepromSignature) {
      log("EEPROM not initialized");
    } else {
      for (int i = 0; i < 32; ++i)
        essid += char(EEPROM.read(i));
      log("EEPROM SSID: %s", essid.c_str());
      for (int i = 32; i < 96; ++i)
        epassword += char(EEPROM.read(i));
    }

    if (essid.length() > 0) {
      // connect to EEPROM settings
    } else {
      essid = ssid;
      epassword = password;
    }
    
    log("\n\rConnecting to '%s'", essid.c_str());
    WiFi.begin(essid.c_str(), epassword.c_str());
    int retries=50;
    while ((WiFi.status() != WL_CONNECTED) && (retries-- > 0)) {
      delay(500);
      Serial.print(".");
    }
    log("\n");
  }
  
  if (WiFi.status() == WL_CONNECTED) {    
    log("\nConnected to '%s', IP: %s", essid.c_str(), WiFi.localIP().toString().c_str());
    if (!MDNS.begin("EspDCA")) {
      log("Error setting up MDNS responder.");
    } else {
      log("MDNS responder started");
    }
  } else {
      log("WiFi connection failed, running in AP mode\n");
      WiFi.softAP(ssid, password);
      log("AP IP address: %s", WiFi.softAPIP().toString().c_str());
  }

  // Serve HTML and JS directly from flash memory
  server.on("/", []() {
    server.sendHeader("Location", "/index.html", true);
    server.send(302, "text/plain", "");
  });

  server.on("/index.html", []() {
    server.send_P(200, "text/html", DCA_HTML);
  });

  server.on("/dca.js", []() {
    server.send_P(200, "application/javascript", DCA_JS);
  });

  // Retain your original API endpoints
  server.on("/peek", [](){    
    String webString = String(dca_buffer[dca_index].milimeters / 100.0, 2); // mm, 2 decimals
    server.send(200, "text/plain", webString); 
    delay(50);
  });

  server.on("/raw", [](){      
    unsigned int ts = 0;
    if(server.hasArg("ts")) {
      ts = server.arg("ts").toInt();
    }

    String rawJSON;
    for(size_t offset = 1; offset <= DCA_BUFFER_SIZE; offset++) {
      if (dca_buffer[(dca_index + offset) % DCA_BUFFER_SIZE].milimeters != MIN_RANGE) {
        float mm = dca_buffer[(dca_index + offset) % DCA_BUFFER_SIZE].milimeters / 100.0;
        rawJSON += "{\"axis0_mm\":";
        rawJSON +=  String(mm, 2);
        rawJSON += ",\"ts\":";
        rawJSON += dca_buffer[(dca_index + offset) % DCA_BUFFER_SIZE].timestamp;
        rawJSON += "}";
        yield();
      }
    }
    server.send(200, "text/plain", rawJSON);
  });

  server.on("/setup", [](){
    String content = "<!DOCTYPE HTML>\r\n<html>";
    content += "WiFi Setup<br></p><form method='get' action='settings'><label>SSID:</label><input name='ssid' length=32><br><label>Password:</label><input name='password' type=\"password\" length=64><br><input type=\"submit\" value=\"Save credentials\"></form><br><br><a href=\"/settings&reset=1\">Reset EEPROM</a></html>";
    server.send(200, "text/html", content);
  });
  
  server.on("/settings", [](){
    String lssid = server.arg("ssid");
    String lpassword = server.arg("password");
    String reset = server.arg("reset");
    if (lssid.length() > 0 && lpassword.length() > 0) {
      log("Clearing eeprom...\n");
      for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
      }
      for (int i = 0; i < lssid.length(); ++i)
        EEPROM.write(i, lssid[i]);
      for (int i = 0; i < lpassword.length(); ++i)
        EEPROM.write(32+i, lpassword[i]);
      EEPROM.write(128, eepromSignature);
      EEPROM.commit();
      log("EEPROM saved.\n");
      server.send(200, "application/json", "{\"Success\":\"WiFi settings saved to EEPROM. Reset to apply.\"}");
    }
    else if (reset.length() > 0) {
      log("Clearing EEPROM...\n");
      for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);
      }
      EEPROM.write(128, ~eepromSignature);      
      EEPROM.commit();
      server.send(200, "application/json", "{\"Success\":\"EEPROM settings cleared.\"}");
    } else {
      server.send(404, "application/json", "{\"Error\":\"404 not found\"}");
    }
  });

  server.begin();
  log("HTTP server started");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);  
  log("WebSockets server started");

  xTaskCreate(spcTask, "spcTask", 10000, NULL, 1, NULL); 
  log("SPC Thread started\n");

  log("Free heap: %u", ESP.getFreeHeap());
  log("Serial commands:\n    stop - stop the streaming of data\n    start - start the streaming of data\n    debug - turn on/off ADC signal debugging\n");
}

size_t last_dca_index = 0;
void loop()
{
  server.handleClient();
  webSocket.loop();

  if(Serial.available() > 0)
  {
      String serial_cmd = Serial.readStringUntil('\n');
      log("Received: cmd='%s'", serial_cmd.c_str());

      if (serial_cmd.startsWith("stop")) {
        stream_mode = 0;
      } else if (serial_cmd.startsWith("start")) {
        stream_mode = 1;
      } else if (serial_cmd.startsWith("debug")) {
        debug_mode = !debug_mode;
      }
  }

  if (stream_mode && (last_dca_index != dca_index)) {
      float mm = dca_buffer[last_dca_index].milimeters / 100.0;
      log("{\"axis0_mm\":%.2f,\"ts\":%d}", mm, dca_buffer[last_dca_index].timestamp);
      last_dca_index = (last_dca_index+1) % DCA_BUFFER_SIZE;
  }
}
