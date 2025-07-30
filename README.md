<h1>ESP32 Digital Caliper</h1>
Read digital calipers values with ESP32. No resistors, transistors or anything else needed, just 3 wires and you're done.<br />
It serves a webpage via WiFI. It outputs the data values in the serial terminal.<br />
Works with any ESP32 (with Wifi). I have used ESP32 Super mini for the size convenience.<br />
This repository is based on the original <a href="https://github.com/MGX3D/EspDRO" target="_blank">EspDRO made by MGX3D</a><br />

<h2>Pinout</h2>
<p>On the digital caliper pcb there are 4 pads. You need to connect them to the ESP32 as following</p>
<br />
<table>
  <tr>
    <td width="50%">
      <img src="https://raw.githubusercontent.com/sorinbotirla/esp32-digital-caliper/refs/heads/main/images/caliper_pcb.jpg" />
    </td>
    <td width="50%">
      <img src="https://raw.githubusercontent.com/sorinbotirla/esp32-digital-caliper/refs/heads/main/images/connector.jpg" />
    </td>
  </tr>
</table>
<br />
<table>
  <tr>
    <td>Digital Caliper pad</td>
    <td>ESP32 PIN</td>
  </tr>
  <tr>
    <td>GND</td>
    <td>GND</td>
  </tr>
  <tr>
    <td>DATA (SDA)</td>
    <td>2</td>
  </tr>
  <tr>
    <td>CLOCK (SCL)</td>
    <td>3</td>
  </tr>
  <tr>
    <td>VCC (1.5V)</td>
    <td>Not used unless you want to supply 1.5V from the ESP32 (with 2 resistors on a voltage divider or a AMS1117 voltage regulator) and quit using the battery</td>
  </tr>
</table>
<br />
<br />
<h2>Important!</h2>
If you do not see the pads, open the case. It may need to cut the case to make room for the wires or the connector.
<br />
<br />
<table>
  <tr>
    <td width="33%">
      <img src="https://raw.githubusercontent.com/sorinbotirla/esp32-digital-caliper/refs/heads/main/images/20250730_120254.jpg" />
    </td>
    <td width="33%">
      <img src="https://raw.githubusercontent.com/sorinbotirla/esp32-digital-caliper/refs/heads/main/images/20250730_120238.jpg" />
    </td>
    <td width="33%">
      <img src="https://raw.githubusercontent.com/sorinbotirla/esp32-digital-caliper/refs/heads/main/images/20250730_095303.jpg" />
    </td>
  </tr>
</table>
<br />
<br />
You can solder thin wires on these pads or you can solder a 4 pin connector as I did.<br /><br />
<table>
  <tr>
    <td>
      <img src="https://raw.githubusercontent.com/sorinbotirla/esp32-digital-caliper/refs/heads/main/images/20250730_085553.jpg" />
    </td>
    <td>
      <img src="https://raw.githubusercontent.com/sorinbotirla/esp32-digital-caliper/refs/heads/main/images/20250730_085558.jpg" />
    </td>
  </tr>
</table>
<br />
<br />
<h2>SETUP</h2>
In the .ino file change the <strong>YOUR_WIFI_SSID</strong> and <strong>YOUR_WIFI_PASSWORD</strong> to your desired WIFI credentials.<br />
The <strong>client_mode</strong> can be either 0 or 1. 
<br />
<br />
0 is for AP mode (ESP32 will start it's own WIFI hotspot), 
<br />
1 is for Client mode (ESP32 will connect to a wifi network you already own).
<br /><br />
Once you connected to the Wifi, you can check the local IP in the serial terminal, and open it in the browser. You will get a simple webpage with the digital values readout.
<br /><br />
<table>
  <tr>
    <td width="50%">
      <img src="https://raw.githubusercontent.com/sorinbotirla/esp32-digital-caliper/refs/heads/main/images/20250730_085548.jpg" />
    </td>
    <td width="50%">
      <img src="https://raw.githubusercontent.com/sorinbotirla/esp32-digital-caliper/refs/heads/main/images/web-app.jpg" />
    </td>
  </tr>
</table>
<br />
<h2>Working Libraries used</h2>
<br />
WiFi at version 3.0.4<br />
Networking at version 3.0.4<br />
EEPROM at version 3.0.4<br />
WebServer at version 3.0.4<br />
FS at version 3.0.4<br />
ESPmDNS at version 3.0.4<br />
WebSockets at version 2.6.1<br />
NetworkClientSecure at version 3.0.4<br />

