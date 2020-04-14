# D1 Mini Node #

A small Node Exporter to serve Sensor data from a D1 Mini Board or a ESP8266-01 Board to a Prometheus instance.


### Installation

Clone this Repository:

    git clone https://github.com/vaddi/d1mini_node.git

Or just copy the plain Text from this Files into you Adruino IDE Editor (Remember to choose the right Board!) and flash your Device:

- [d1mini_node][]  
- [esp8266-01][]  


Pinout for the D1 Mini  
![d1mini_pinout](https://github.com/vaddi/d1mini_node/blob/master/assets/images/d1mini_pinout.png "D1 Mini Pinout")  
There is also a [svg](https://github.com/vaddi/d1mini_node/blob/master/assets/images/d1mini_pinout.svg) Version available.  


## Features ##

- Silent Mode, disable LED blink
- Use Sensors by simple enable/disable them into the webgui/code (you still have to plug them ;)
- Initial Setup via Wifi Access Point with Captive Portal
- DHCP Client or Static IP Modes
- SecPush, Disable all auth for given seconds after booting the device
- Prometheus ready /metrics Endpoint
- Enable Disable Authentification from Webgui (basic auth & token based auth)


## Setup D1 mini ##

Open the `d1mini_node.ino` File into an Arduino IDE.  
1. Set Board: `tools` -> `Board` -> `LOLIN(WEMOS) D1 R2 & mini`  
2. Set Port: `/dev/cu.wchusbserial1420`*  
3. Compile and Upload the Sketch  
4. Place Device to desired destination.  
5. Plugin all Sensors (Pinout in head of *.ino File)  
6. After plugin the Device will run in Setupmode (On Start LED blinks 3 times). Just grab a Phone and connect to the Wifi named `esp_setup`.  
7. After connection, they should open a Captive Page in the Browser in wich you can setup the Device by set SSID, Wifi Password, Hostname (DNS Name) and Other Stuff like connected Sensors.  
8. After Submit the Device will do a Reboot and should then connected as Client into the desired Wifi Network.  

If failed to connect to the Network, Compile by Clear EEPROM (`tools` -> `Erease Flash` -> `All Flash Contents`) and try again. 

\* Device Path For example, depend on your System and can has another path  


### Possible Sensors ###

- MQ135 Air Quality sensor
- DS18B20 Temperatur Sensors
- BME280 Air Temperature, Humidity and Pressure Sensor
- MightyOhm Geigercounter
- MCP 300X ADC Device for 8 Analog Sensorvalues  


### Hints ###

- There is only one ADC Pin (A0) available which will be used for Voltage detection by default. When using an MQ135 this Pin is used by the Sensor so accurate Voltage messuring will be disabled!
- This code is not perfect, but fits for my needs. Feel free to improve them.


## Setup on ESP8266-01 ##

Using a DHT Sensorboard ([DHT-Board][]) or just add a Plain DHT Sensor to an ESP8266-01. I've soldering a DHT22 instead of a DHT11 onto my Boards to get more accuracy.

Open the `esp8266-01_node.ino` File into an Arduino IDE.  
1. Configure DHT11 or DHT22 Sensor in File!  
2. Set Board: `tools` -> `Board` -> `Generic ESP8266 Module`  
3. Set Port: `/dev/cu.wchusbserial1420`*  
4. Compile and Upload the Sketch  
5. Place Device to desired destination.  
6. Plugin all Sensors (Pinout for DHT11 & DHT22 in *.ino File)  
7. After plugin the Device will run in Setupmode (On Start LED blinks 3 times). Just grab a Phone and connect to the Wifi named `esp_setup`.  
8. After connection, they should open a Captive Page in the Browser in wich you can setup the Device by set SSID, Wifi Password, Hostname (DNS Name) and Other Stuff like connected Sensors.  
9. After Submit the Device will do a Reboot and should then connected as Client into the desired Wifi Network.  

If failed to connect to the Network, Compile by Clear EEPROM (`tools` -> `Erease Flash` -> `All Flash Contents`) and try again. 

\* Device Path For example, depend on your System and can has another path  



## Testing ##

Open a Webbrowser and try to connect to your Node:

	http://esp01.speedport.ip/
	
Navigate through the Page, checkout the Metrics or Save some Settings.  


## Troubleshooting ##

When no connection to the Node come up:

- Try to connect to the IP instead of DNS Name to reach the Device.
- See in your Router DNS Table to get sure the Node will get an IP Adress.
- Try to disable Silent Mode to ensure the Node will connect to the Wifi and doesnt run in Setup Mode (LED blinks 3 times). The Device will blink if a client connecting to the wifi and when processing a request.
- Check Pins, make sure there are no shortcuits or other pin missplacements.
- On D1mini Boards you can enable `debug` and plugin via USB. Then read the Debug output by Arduino IDE Serial or via Terminal by `screen /dev/ttyUSB1 9600`.  



## Add Targets to Prometheus ##

Just add your device as a Target to your `prometheus.yml` File. I've using here a Network which searchdomain `speedport.ip`, just change to your Network or use IPs instead of DNS Names. Here is an Example job:

```yaml
...
  - job_name: 'esps'
#    metrics_path: '/metrics'
#    scrape_interval: 5m
#    scrape_timeout: 1m
#    basic_auth:
#      username: 'XXXXX'
#      password: 'XXXXX'
#    params:
#      apikey: ['XXXXXXXXXX']
    static_configs:
      - targets: ['esp01.speedport.ip:80'],
      - targets: ['esp02.speedport.ip:80'],
      - targets: ['esp03.speedport.ip:80']
...
```

## Dashboards ##

I've created some Dashboards, maybe there helpfull when begin to build some own Dashboards.

- ESP Full Dashboard: [dashboard][]
- Dashboard Example: [dashboard-example][]


## Screenshots ##

The Webinterface:  
![webinterface](https://github.com/vaddi/d1mini_node/blob/master/assets/images/webinterface.png "Webinterface")  
Screenshot from the Setuppage. here you can configure you Device.

ESP Full Dashboard:  
![dashboard_full](https://github.com/vaddi/d1mini_node/blob/master/assets/images/dashboard_full.png "Dashboard Full")  
A Dashboard witch has all Metrics available. So you have to just enable a Sensor on your ESP Device to see the Sensordata. The Sensors present will get their Value by they're scrape time.

A Dashboard Example:  
![dashboard_example](https://github.com/vaddi/d1mini_node/blob/master/assets/images/dashboard_example.png "Dashboard Example")  
A Dashboard wich combines some Metrics to get a complete Overview over you flat or other Places. Just place some Sensors and feel free to combine or calc average from them.

Devices and Cases:
![devices](https://github.com/vaddi/d1mini_node/blob/master/assets/images/devices.png "Devices")  
Some Basic Devices, only Temperature, Humidity and with the bme280 Sensor also Pressure. Beside the printed Cases (see [links](https://github.com/vaddi/d1mini_node#links) below).

Bathroom Device:
![devices](https://github.com/vaddi/d1mini_node/blob/master/assets/images/devices_bathroom.png "Bathroom Device" | width=200)  
A Node which get Metrics from my Bathroom. Here also with a printed case and a MQ135 behind.

There are some More, just take a look into the [assets/images](https://github.com/vaddi/d1mini_node/blob/master/assets/images/) Folder.


## Comming Soon ##

Some Ideas which i want to implement in the future.

- Parsing more values from mcp to flatten the output Data
- transport layer security (tls) support to use https
- Two Pins left for more Sensors or other Stuff


## Links ##

##### Boards & other Chips (Amazon): #####

* [D1Mini](https://www.amazon.de/gp/product/B0754W6Z2F)
* [esp8266-01](https://www.amazon.de/gp/product/B074RLG5TC)
* [mcp3008](https://www.amazon.de/gp/product/B0722FJRSQ)

##### Sensors: #####

* [mq135](https://www.amazon.de/gp/product/B07V9WCQL4)
* [bme280](https://www.amazon.de/gp/product/B07HMQMW6M)
* [ds18b20](https://www.amazon.de/gp/product/B07DN47YWW)
 * Analog Sensors which can connected to the MCP3008 ADC
 * [moisture](https://www.amazon.de/gp/product/B07V6M5C4H)
 * [ph-node](https://www.amazon.de/gp/product/B01F1RRFFY)
 * [brightness-meter](https://www.amazon.de/gp/product/B01N5KZCQJ)
 * Find a lot more Sensors on [google](https://www.google.com/search?q=analog+sensor+arduino) and [amazon](https://www.amazon.de/s?k=analog+sensor). Mostly any kind of analoge Sensor should run.
* [DHT-Board][]. You can de-soldering the sensor to a dht22.
* [Geigercounter][]

##### Cases (www.thingiverse.com): #####

* [D1Mini-Case][]
* [ESP8266-01-Case][]
* [MQ135-Case][]


[DHT-Board]: https://www.amazon.de/gp/product/B07L2TWS5G
[Geigercounter]: https://mightyohm.com/geiger
[D1Mini-Case]: https://www.thingiverse.com/thing:3698982
[ESP8266-01-Case]: https://www.thingiverse.com/thing:4096863
[MQ135-Case]: https://www.thingiverse.com/thing:2850597

[d1mini_node]: https://github.com/vaddi/d1mini_node/blob/master/d1mini_node/d1mini_node.ino
[esp8266-01]: https://github.com/vaddi/d1mini_node/blob/master/esp8266-01/esp8266-01_node.ino
[dashboard]: https://github.com/vaddi/d1mini_node/assets/dashboards/dashboard_full.json
[dashboard-example]: https://github.com/vaddi/d1mini_node/assets/dashboards/dashboard_combined.json