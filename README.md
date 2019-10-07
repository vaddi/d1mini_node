# D1 Mini Node #

A small Node Exporter to scrape Sensor data from a D1 Mini Board or a ESP8266-01 Board by a Prometheus instance.

Clone this Repository:

    git clone https://github.com/vaddi/d1mini_node.git


## Setup D1 Mini ##

Open the `d1mini_node.ino` File into an Arduino IDE.

Edit the File:

1. Enable/Disable Sensors, depending on which Sensors should be used
2. Set your wifi SSID and Key
3. Setup unique Nodename (dnsname)
4. Install all neccessary Libraries into your Arduino IDE (See the Head of the ino File to find them)
5. Pinout is described into the Sourcecode, just add Sensors to the D1 Mini Board like described


## Setup ESP8266-01 ##

Using DHT Sensorboard (see Links) or just add a Plain DHT Sensor to an ESP8266-01. I've soldering a DHT22 instead of a DHT11 onto my Boards

Open the `esp8266-01_node.ino` File into an Arduino IDE.

Edit the File:

1. Set your wifi SSID and Key
2. Setup unique Nodename (dnsname) and which DHT Sensor should be used (DHT22 or DHT11)
3. Install SimpleDHT Library to your Adruino IDE
4. Flashing your ESP8266 Device
5. Plugin the DHT Sensor, add Power


## Add Targets to Prometheus ##

Just add your device as a Target to your `prometheus.yml` File. I've using here a Network which searchdomain `speedport.ip`, just change to your Network or use IPs instead of DNS Names.

```yaml
  - job_name: 'esps'
    scrape_interval: 5m
    static_configs:
      - targets: ['esp01.speedport.ip:80']
```


## Links ##

* [DHT-Board](https://www.amazon.de/gp/product/B07L2TWS5G)
* [Geigercounter](https://mightyohm.com/geiger)

