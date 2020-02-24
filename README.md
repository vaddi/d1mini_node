# D1 Mini Node #

A small Node Exporter to scrape Sensor data from a D1 Mini Board or a ESP8266-01 Board by a Prometheus instance.

Clone this Repository:

    git clone https://github.com/vaddi/d1mini_node.git

Just Copy the Text from Files:

- [d1mini_node][]
- [esp8266-01][]


## Setup on D1 Mini ##

Open the `d1mini_node.ino` File into an Arduino IDE.  
1. Set Board: `tools` -> `Board` -> `LOLIN(WEMOS) D1 R2 & mini`  
2. Set Port: `/dev/cu.wchusbserial1420`*  
3. Compile and Upload the Sketch  
\* For example, depend on your System and can has another path

- Place Device to desired destination. 
- Plugin all Sensors (Pinout in head of *.ino File)
- After plugin the Device will run in Setupmode (On Start LED blinks 3 times). Just grab a Phone and connect to the Wifi named `esp_setup`. 
- After connection, they should open a Captive Page in the Browser in wich you can setup the Device by set SSID, Wifi Password, Hostname (DNS Name) and Other Stuff. 
- After Submit the Device will do a Reboot and should then connected as Client into the desired Wifi Network. 


Edit the *.ino File:

1. Enable/Disable Sensors, depending on which Sensors should be used
2. Set default wifi SSID and Password
3. Setup unique Nodename (dnsname)
4. Install all neccessary Libraries into your Arduino IDE (See the Head of the ino File to find them by name)
5. Pinout is described into the Sourcecode, just add Sensors to the D1 Mini Board like described
6. Plugin Power


### Possible Sensors ###

- MQ135 Air Quality sensor
- DS18B20 Temperatur Sensors
- BME280 Air Temperature, Humidity and Pressure Sensor
- MightyOhm Geigercounter
- MCP 300X ADC Device for 8 Analog Sensorvalues

### Features ###

- LED blink on wifi setup and when request
- Silent Mode, disable LED blink
- Use Sensors by simple enable/disable them into the webgui/code
- Initial Setup via Wifi Access Point whith Captive Portal
- Static IP or DHCP Client Mode
- 

### Hints ###

- There is only one ADC Pin (A0) available which will be used for Voltage detection by default. When using an MQ135 this Pin is used by the Sensor so accurate Voltage messuring will be disabled!
- This code is not perfect, but fits for my needs. Feel free to improve them.


## Setup on ESP8266-01 ##

Using a DHT Sensorboard ([DHT-Board][]) or just add a Plain DHT Sensor to an ESP8266-01. I've soldering a DHT22 instead of a DHT11 onto my Boards to get more accuracy.

Open the `esp8266-01_node.ino` File into an Arduino IDE.

Edit the File:

1. Set your wifi SSID and Key
2. Setup unique Nodename (dnsname) and which DHT Sensor should be used (DHT22 or DHT11)
3. Install SimpleDHT Library to your Adruino IDE
4. Flashing your ESP8266 Device
5. Plugin the DHT Sensor
6. Plugin Power


## Testing ##

Open a Webbrowser and try to connect to your Node:

	http://esp01.speedport.ip/


## Troubleshooting ##

When no connection to the Node come up:

- Try to use the IP instead of DNS Name.
- See in your Router DNS Table to get sure the Node will get an IP Adress.
- try to disable Silent Mode to ensure the Node will connect to the Wifi and doesnt run in Setup Mode (LED blinks 3 times). It will blink on connecting to wifi and when processing a request.
- Check Pins, make sure there are no shortcuits or other pin missplacements.
- Check Pinout of Sensor and the D1 Board.
- 


## Add Targets to Prometheus ##

Just add your device as a Target to your `prometheus.yml` File. I've using here a Network which searchdomain `speedport.ip`, just change to your Network or use IPs instead of DNS Names.

```yaml
  - job_name: 'esps'
    scrape_interval: 5m
    static_configs:
      - targets: ['esp01.speedport.ip:80'],
      - targets: ['esp02.speedport.ip:80'],
      - targets: ['esp03.speedport.ip:80']
      ...
```


## Links ##

* [DHT-Board][]
* [Geigercounter][]


[DHT-Board]: https://www.amazon.de/gp/product/B07L2TWS5G
[Geigercounter]: https://mightyohm.com/geiger
[d1mini_node]: https://github.com/vaddi/d1mini_node/blob/master/d1mini_node/d1mini_node.ino
[esp8266-01]: https://github.com/vaddi/d1mini_node/blob/master/esp8266-01/esp8266-01_node.ino