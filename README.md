<h1>ESP32 Digital Caliper</h1>
Read digital calipers values with ESP32. No resistors, transistors or anything else needed, just 3 wires and you're done. Serving a webpage via WiFi. Showing data in serial terminal.

<h2>Pinout</h2>
<p>On the digital caliper pcb, with the display facing up (as you would normally use it), on the top right corner are 4 pins. You need to connect them to the ESP32 as following</p>
<br />
<br />
<table>
  <tr>
    <td>Digital Caliper PIN</td>
    <td>ESP32 PIN</td>
  </tr>
  <tr>
    <td>GND</td>
    <td>GND</td>
  </tr>
  <tr>
    <td>DATA</td>
    <td>2</td>
  </tr>
  <tr>
    <td>CLOCK</td>
    <td>3</td>
  </tr>
</table>
