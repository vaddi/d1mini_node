/*
  Lolin/WEMOS D1 mini sensor node

  A Prometheus ready Node for scraping Temperature, Humidity and Air Quality

  Install this Libraries to your Arduino IDE:
  - Adafruit Sensor Library (search for "Adafruit Unified Sensor")
  - Adafruit BME280 Library (search for "bme280")
  - MQUnified Library (search for "mq135")
  - OneWire Library ( search for "onewire")
  - DallasTemperature Library (search for "dallas")

  #
  ## Pin Wiring (left = Sensor pins, right = D1 Mini pins)
  #

  BME280  | D1 Mini (i2c)
  -----------------
  VIN     | 5V
  GND     | GND
  SCL     | Pin 5 *
  SDA     | Pin 4 *
  * i2c

  MQ135   | D1 Mini (Analog in Pin)
  -----------------
  VIN     | 5V
  GND     | GND
  AO      | PIN A0 *
  * When A0 is used, accurate Voltage calculation will be disabled!

  Capacitive Soil Moisture Sensor (over mcp3008 port X)
  -----------------
  VIN     | 3.3V
  GND     | GND
  AO      | mcp3008 Port *

  ds18b20 | D1 Mini (OneWire Pin)
  -----------------
  VIN     | 3,3V *
  GND     | GND
  Data    | PIN D3 *
  * Wiring a 4,7kΩ Resistor between VIN and Data!

  Geigercounter | D1 Mini (Serial)
  -----------------------
  6 green
  5 yellow      | 3
  4 orange      | 1
  3 red         | 3,3V
  2 brown
  1 black       | GND

  MCP3008 | D1 Mini (SPI)
  -------------------
  VIN     | 3,3V
  GND     | GND
  Clock   | D5
  Dout    | D6
  Din     | D7
  CS      | D8

  #
  # Usefull links
  #

  Overview of D1 Mini Pin layout:
  - https://escapequotes.net/esp8266-wemos-d1-mini-pins-and-diagram/

  Visit the Librarie Pages for more function and setting possibilities
  - https://github.com/adafruit/Adafruit_Sensor/blob/master/Adafruit_Sensor.h
  - https://github.com/adafruit/Adafruit_BME280_Library/blob/master/Adafruit_BME280.h
  - https://github.com/miguel5612/MQSensorsLib/blob/master/src/MQUnifiedsensor.h
  - https://github.com/Yveaux/esp8266-Arduino/blob/master/esp8266com/esp8266/libraries/OneWire/OneWire.h
  - https://github.com/milesburton/Arduino-Temperature-Control-Library/blob/master/DallasTemperature.h
  - https://www.arduino.cc/en/Reference/Wire
  - https://www.arduino.cc/en/Reference/SoftwareSerial

  Overviews of MQ Sensor Types:
  - https://www.mysensors.org/build/gas
  - https://www.roboter-bausatz.de/301/mq-serie-9er-set-sensoren

  The mightyohm Geigercounter Kit
  - https://mightyohm.com/geiger

  ESP Wifi Overview:
  - https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/station-class.html

  MCP 3008 Overview
  - http://www.esp8266learning.com/wemos-mcp3008-example.php

  #
  # Usefull knowlegde
  #

  Reset the wohle EEROM Storeage from Arduino IDE menu:
  Tools -> Erase Flash -> All Flash Contents
  
*/

// default includes
#include <Hash.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>



//
// Pin Setup
//


// MQ135 Pin
#define mqpin A0 // MQ Analog Signal Pin

// Geigerceounter serial pins
#define gcrx 3  // RX Pin
#define gctx 1  // TX Pin 

// Declare a OneWire Pin
#define ONE_WIRE_BUS D3          // Check for D3 Pin or similary OneWire pins
#define TEMPERATURE_PRECISION 10 // Define precision 9, 10, 11, 12 bit (9=0,5, 10=0,25, 11=0,125)

// MCP3008 ADC
const uint8_t CS = D8; // CS Pin, other go over SPI (MOSI_PIN = D7, MISO_PIN = D6, CLOCK_PIN = D5)



//
// Default settings
//

// setup your default wifi ssid, passwd and dns stuff. Can overwriten also later from setup website
const int eepromStringSize      = 20;     // maximal byte size for strings (ssid, wifi-password, dnsname)

char ssid[eepromStringSize]     = "";     // wifi SSID name
char password[eepromStringSize] = "";     // wifi wpa2 password
char dnsname[eepromStringSize]  = "esp";  // Uniq Networkname
char place[eepromStringSize]    = "";     // Place of the Device
int  port                       = 80;     // Webserver port (default 80)
bool silent                     = false;  // enable/disable silent mode

bool gcsensor                   = false;  // enable/disable mightyohm geigercounter (unplug to flash!)
bool mq135sensor                = false;  // enable/disable mq-sensor. They use Pin A0 and will disable the accurate vcc messuring
bool bme280sensor               = false;  // enable/disable bme280 (on address 0x76). They use Pin 5 (SCL) and Pin 4 (SDA) for i2c
bool ds18b20sensor              = false;  // enable/disable ds18b20 temperatur sensors. They use Pin D3 as default for OneWire
bool mcp3008sensor              = false;  // enable/disable mcp3008 ADC. Used Pins Clock D5, MISO D6, MOSI D7, CS D8
int  mcpchannels                = 0;      // Amount of visible MCP 300X Channels, MCP3008 = max 8, MCP3004 = max 4

const char* history             = "3.0";  // Software Version
String      webString           = "";     // String to display
String      ip                  = "127.0.0.1";  // default ip, will overwriten
const int   eepromAddr          = 0;      // default eeprom Address
const int   eepromSize          = 512;    // default eeprom size in kB (see datasheet of your device)
const int   eepromchk           = 48;     // byte buffer for checksum
char        lastcheck[ eepromchk ] = "";  // last created checksum

unsigned long previousMillis = 0;         // flashing led timer value
bool SoftAccOK  = false;                  // value for captvie portal function 

// basic auth 
bool authentication             = false;  // enable/disable Basic Authentication
char authuser[eepromStringSize] = "";     // user name
char authpass[eepromStringSize] = "";     // user password

bool staticIP = false;                    // use Static IP
IPAddress ipaddr(192, 168, 1, 250);       // Device IP Adress
IPAddress gateway(192, 168, 1, 1);        // Gateway IP Adress
IPAddress subnet(255, 255, 255, 0);       // Network/Subnet Mask
IPAddress dns1(192, 168, 1, 1);           // First DNS
IPAddress dns2(4, 4, 8, 8);               // Second DNS

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;



//
// Edit Carefully after here
//



//
// Set Functions, Values and Libraries depending on sensors enabled/disabled
//

// Geigercounter
//
#include <SoftwareSerial.h>
// Serial connection to MightyOhm Geigercounter Device
SoftwareSerial mySerial( gcrx, gctx, false ); // RX Pin, TX Pin, inverse_output
String cps             = "0.0";
String cpm             = "0.0";
String uSvh            = "0.0";
String gcraw           = "";
String gcmode          = "";
int gcsize             = 0;
// Helper function to find string in string
// read serial data
String readGCSerial() {
  //char dataBuffer[64];
  String data;
  if( mySerial.available() ) {
    //mySerial.readBytesUntil( '\n', dataBuffer, 64 );
    //data = dataBuffer;
    return mySerial.readStringUntil('\n');
  }
  return "";
}
// realy bad serial data parser :/
void parseGCData() {
  String data = "";
  while( data == "" ) {
    data = readGCSerial();
    delay( 5 );
  }
  int commaLocations[6];
  commaLocations[0] = data.indexOf(',');
  commaLocations[1] = data.indexOf(',',commaLocations[0] + 1);
  commaLocations[2] = data.indexOf(',',commaLocations[1] + 1);
  commaLocations[3] = data.indexOf(',',commaLocations[2] + 1);
  if( data.length() > 57 ) {
    commaLocations[4] = data.indexOf(',',41 + 1);
    commaLocations[5] = data.indexOf(',',49 + 1);
  } else {
    commaLocations[4] = data.indexOf(',',commaLocations[3] + 1);
    commaLocations[5] = data.indexOf(',',commaLocations[4] + 1);
  }  
  cps = data.substring(commaLocations[0] + 2,commaLocations[1]);
  cpm = data.substring(commaLocations[2] + 2, commaLocations[3]);
  uSvh = data.substring(commaLocations[4] + 2, commaLocations[5]);
  gcsize = data.length();
  gcmode = data.substring(commaLocations[5] + 2, commaLocations[5] + 3);
  gcraw = data;
}
// Get formatet Geigercounterdata function
String getGCData() {
  unsigned long currentMillis = millis();
  parseGCData();
  long GCRuntime = millis() - currentMillis;
  String result = "";
  result += "# HELP esp_mogc_info Geigercounter current read mode\n";
  result += "# TYPE esp_mogc_info gauge\n";
  result += "esp_mogc_info{";
  result += "mode=\"" + gcmode + "\"";
  result += ", size=\"" + String( gcsize ) + "\"";
  gcraw = gcraw.substring( 0, gcraw.length() -1 );
  result += ", raw=\"" + String( gcraw ) + "\"";
  result += "} 1\n";
  result += "# HELP esp_mogc_cps Geigercounter Counts per Second\n";
  result += "# TYPE esp_mogc_cps gauge\n";
  result += "esp_mogc_cps " + cps + "\n";
  result += "# HELP esp_mogc_cpm Geigercounter Counts per Minute\n";
  result += "# TYPE esp_mogc_cpm gauge\n";
  result += "esp_mogc_cpm " + cpm + "\n";
  result += "# HELP esp_mogc_usvh Geigercounter Micro Sivert per Hour\n";
  result += "# TYPE esp_mogc_usvh gauge\n";
  result += "esp_mogc_usvh " + uSvh + "\n";
  result += "# HELP esp_mogc_runtime Time in milliseconds to read Geigercounter Values\n";
  result += "# TYPE esp_mogc_runtime gauge\n";
  result += "esp_mogc_runtime " + String( GCRuntime ) + "\n";
  return result;
}


// DS18B20
//
#include <OneWire.h>
#include <DallasTemperature.h>
// Declare OneWire Bus
OneWire oneWire(ONE_WIRE_BUS);
// Declare ds18b20 Sensor on OneWire Bus
DallasTemperature DS18B20(&oneWire);
DeviceAddress tempAdd[0];                     // DS18B20 Address Array
int           tempCount       = 0;            // DS18b20 Arraysize value
String getDS18B20Data() {
  unsigned long currentMillis = millis();
  DS18B20.requestTemperatures();
  long DS18B20Runtime = millis() - currentMillis;
  String result = "";
  result += "# HELP esp_ds18b20 Current ds18b20 Temperatures and Addresses (deviceIds will updated on each restart!)\n";
  result += "# TYPE esp_ds18b20 gauge\n";
  if( tempCount > 0 ) {
    for( int i = 0; i < tempCount; i++ ) {
      float temperature = DS18B20.getTempCByIndex( i );
      result += "esp_ds18b20{device=\"" + String( i ) + "\",deviceId=\"";
      result += String( (int) tempAdd[i],HEX ) + "\"} " + String( temperature ) + "\n";
    }
  } else {
    result += "esp_ds18b20{device=\"99\",deviceId=\"0000000000000000\"} 0.0\n";
  }
  result += "# HELP esp_ds18b20_amount Current active ds18b20 Sensors\n";
  result += "# TYPE esp_ds18b20_amount gauge\n";
  result += "esp_ds18b20_amount " + String( tempCount ) + "\n";   
  result += "# HELP esp_ds18b20_parasite parasite status\n";
  result += "# TYPE esp_ds18b20_parasite gauge\n";
  result += "esp_ds18b20_parasite " + String( (int) DS18B20.isParasitePowerMode() ) + "\n";
  result += "# HELP esp_ds18b20_runtime Time in milliseconds for read All DS 18B20 Sensors\n";
  result += "# TYPE esp_ds18b20_runtime gauge\n";
  result += "esp_ds18b20_runtime " + String( DS18B20Runtime ) + "\n";
  return result;
}


// BME280
//
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#define SEALEVELPRESSURE_HPA (1013.25) // reference sealeavel
// Create bme Instance (by i2c address 0x77 and 0x76)
Adafruit_BME280 bme;
unsigned      BMEstatus;
float         temp            = 0.0;          // temperature value 
float         pres            = 0.0;          // pressure value
float         alt             = 0.0;          // altitude calculated by sealevel pressure
float         hum             = 0.0;          // humidy value
void setBMEValues() {
  temp = bme.readTemperature();
  pres = bme.readPressure() / 100.0F;
  alt = bme.readAltitude(SEALEVELPRESSURE_HPA);
  hum = bme.readHumidity();
}
String getBME280Data() {
  unsigned long currentMillis = millis();
  setBMEValues();
  long BMERuntime = millis() - currentMillis;
  String result = "";
  result += "# HELP esp_bme_status bme280 onload status, 1 = starting ok, Sensor available after boot\n";
  result += "# TYPE esp_bme_status gauge\n";
  result += "esp_bme_status " + String( BMEstatus ) + "\n";
  result += "# HELP esp_bme_temperature Current BME280 Temperature\n";
  result += "# TYPE esp_bme_temperature gauge\n";
  result += "esp_bme_temperature " + String( temp ) + "\n";
  result += "# HELP esp_bme_humidity Current BME280 Humidity\n";
  result += "# TYPE esp_bme_humidity gauge\n";
  result += "esp_bme_humidity " + String( hum ) + "\n";
  result += "# HELP esp_bme_pressure Current bme280 pressure\n";
  result += "# TYPE esp_bme_pressure gauge\n";
  result += "esp_bme_pressure " + String( pres ) + "\n";
  result += "# HELP esp_bme_altitude Current bme280 altitude, calculated by sealevel pressure\n";
  result += "# TYPE esp_bme_altitude gauge\n";
  result += "esp_bme_altitude " + String( alt ) + "\n";
  result += "# HELP esp_bme_runtime Time in milliseconds to read Date from the BME280 Sensor\n";
  result += "# TYPE esp_bme_runtime gauge\n";
  result += "esp_bme_runtime " + String( BMERuntime ) + "\n";
  return result;
}


// MCP3008 ADC
//
#include <SPI.h>
uint16_t channelData[8];
uint16_t mcp3008_read(uint8_t channel) {
  digitalWrite(CS, LOW);
  SPI.transfer(0x01);
  uint8_t msb = SPI.transfer(0x80 + (channel << 4));
  uint8_t lsb = SPI.transfer(0x00);
  digitalWrite(CS, HIGH);
  return ((msb & 0x03) << 8) + lsb;
}
void readMCPData() {
  for( size_t i = 0; i < mcpchannels; ++i ) {
    channelData[ i ] = mcp3008_read( i );
  }
}
String getMCPData() {
  unsigned long currentMillis = millis();
  readMCPData();
  delay(4); // minimum read time in millis
  long MCPRuntime = millis() - currentMillis;
  String result = "";
  result += "# HELP esp_mcp_info MCP3008 Information\n";
  result += "# TYPE esp_mcp_info gauge\n";
  result += "esp_mcp_info{";
  result += "channels=\"" + String( mcpchannels ) + "\"";
  result += "} 1\n";
  result += "# HELP esp_mcp_data MCP Data\n";
  result += "# TYPE esp_mcp_data gauge\n";
  for( size_t i = 0; i < mcpchannels; ++i ) {
    result += "esp_mcp_data{device=\"" + String( i ) + "\",} " + String( channelData[ i ] ) + "\n";
  }
  result += "# HELP esp_mcp_runtime Time in milliseconds to read MCP Data\n";
  result += "# TYPE esp_mcp_runtime gauge\n";
  result += "esp_mcp_runtime " + String( MCPRuntime ) + "\n";
  return result;
}


// MQ135
//
#include <MQUnifiedsensor.h>
#define mqtype 135 //MQ135
float         co              = 0.0;          // CO value
float         alc             = 0.0;          // Alcohol value
float         co2             = 0.0;          // CO2 value
float         dmh             = 0.0;          // Diphenylmethane value
float         am              = 0.0;          // Ammonium value
float         ac              = 0.0;          // Acetone value
float         mqvoltage       = 0.0;          // MQ Voltage value
float         mqr             = 0.0;          // MQ R0 Resistor value
//Declare Sensor
MQUnifiedsensor MQ135(mqpin, mqtype); 
void setMQValues() {
  MQ135.update(); // Update data, the arduino will be read the voltage from the analog pin
  MQ135.calibrate( false ); // calibrate the MQ Sensor
  //mqvoltage = ( (float) MQ135.getVoltage(false) * 10 ) - ( (float) ( ESP.getVcc() / 1024.0 / 10.0 ) );
  mqvoltage = (float) MQ135.getVoltage(false) * 10;
  mqr = MQ135.getR0(); // R0 Resistor value (indicator for calibration)
  co  = MQ135.readSensor("CO"); // CO concentration
  alc = MQ135.readSensor("Alcohol"); // Alcohole concentration
  co2 = MQ135.readSensor("CO2"); // CO2 concentration
  dmh = MQ135.readSensor("Tolueno"); // Diphenylmethane concentration
  am  = MQ135.readSensor("NH4"); // NH4 (Ammonium) concentration
  ac  = MQ135.readSensor("Acetona"); // Acetone concentration 
}
String getMQ135Data() {
  unsigned long currentMillis = millis();
  setMQValues();
  long MQRuntime = millis() - currentMillis;
  String result = "";
  result += "# HELP esp_mq_voltage current Voltage metric\n";
  result += "# TYPE esp_mq_voltage gauge\n";
  result += "esp_mq_voltage " + String( mqvoltage ) + "\n";
  result += "# HELP esp_mq_resistor current R0 Resistor metric\n";
  result += "# TYPE esp_mq_resistor gauge\n";
  result += "esp_mq_resistor " + String( mqr ) + "\n";
  result += "# HELP esp_mq_co Current MQ-135 CO Level\n";
  result += "# TYPE esp_mq_co gauge\n";
  result += "esp_mq_co " + String( co ) + "\n";
  result += "# HELP esp_mq_alc Current MQ-135 Alcohol Level\n";
  result += "# TYPE esp_mq_alc gauge\n";
  result += "esp_mq_alc " + String( alc ) + "\n";
  result += "# HELP esp_mq_co2 Current MQ-135 CO2 Level\n";
  result += "# TYPE esp_mq_co2 gauge\n";
  result += "esp_mq_co2 " + String( co2 ) + "\n";
  result += "# HELP esp_mq_dmh Current MQ-135 Diphenylmethane Level\n";
  result += "# TYPE esp_mq_dmh gauge\n";
  result += "esp_mq_dmh " + String( dmh ) + "\n";
  result += "# HELP esp_mq_am Current MQ-135 Ammonium Level\n";
  result += "# TYPE esp_mq_am gauge\n";
  result += "esp_mq_am " + String( am ) + "\n";
  result += "# HELP esp_mq_ac Current MQ-135 Acetone Level\n";
  result += "# TYPE esp_mq_ac gauge\n";
  result += "esp_mq_ac " + String( ac ) + "\n";
  result += "# HELP esp_mq_runtime Time in milliseconds to read Date from the MQ Sensor\n";
  result += "# TYPE esp_mq_runtime gauge\n";
  result += "esp_mq_runtime " + String( MQRuntime ) + "\n";
  return result;
}


//// DEPRECATED, just use an mcp3008 and leave A0 as default for mq135 Sensor
//// Capacitive Soil Moisture Sensore v1.0
////
//#if CPSENSOR == true
//  int         moisture  = 0;
//  String getMoistureData() {
//    unsigned long currentMillis = millis();
//    moisture = analogRead( cppin );
//    delay(4); // minimum read time in millis
//    long MoistureRuntime = millis() - currentMillis;
//    String result = "";
//    result += "# HELP esp_moisture Current Capacity moisture Sensor Value\n";
//    result += "# TYPE esp_moisture gauge\n";
//    result += "esp_moisture " + String( moisture ) + "\n";
//    result += "# HELP esp_moisture_runtime Time in milliseconds to read Capacitive moisture Sensor\n";
//    result += "# TYPE esp_moisture_runtime gauge\n";
//    result += "esp_moisture_runtime " + String( MoistureRuntime ) + "\n";
//    return result;
//  }
//#else
//  // Enable Power Messuring (Voltage) only if no mq sensor is used
//  ADC_MODE(ADC_VCC);
//  //String getMQ135Data() { return ""; } // dummy function
//  //String getMoistureData() { return ""; } // dummy function
//#endif






// Instanciate webserver listener on given port
ESP8266WebServer server( port );


//
// Helper Functions
//

// http response handler
void response( String data, String requesttype ) {
  if( !silent ) {
    digitalWrite(LED_BUILTIN, LOW);
  }
  server.send(200, requesttype, data);
  delay(100);
  if( !silent ) {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

// flashing led
// flashing( LED_BUILTIN, 300, 500);
void flashing( int led, int tOn, int tOff ) {
  static int timer = tOn;
  if( millis() - previousMillis >= timer ) {
    if( digitalRead( led ) == HIGH ) {
      timer = tOff;
    } else {
      timer = tOn;
    }
    digitalWrite( led, ! digitalRead( led ) );
    previousMillis = millis();
  } 
}


// load config from eeprom
bool loadSettings() {
  // define local data structure
  struct { 
    char eepromssid[ eepromStringSize ];
    char eeprompassword[ eepromStringSize ];
    char eepromdnsname[ eepromStringSize ];
    char eepromplace[ eepromStringSize ];
    bool eepromstaticIP       = false;
    IPAddress eepromipaddr;
    IPAddress eepromgateway;
    IPAddress eepromsubnet;
    IPAddress eepromdns1;
    IPAddress eepromdns2;
    bool eepromauthentication = false;
    char eepromauthuser[eepromStringSize];
    char eepromauthpass[eepromStringSize];
    int  eeprommcpchannels    = 0;
    bool eepromsilent         = false;
    bool eepromgcsensor       = false;
    bool eeprommq135sensor    = false;
    bool eeprombme280sensor   = false;
    bool eepromds18b20sensor  = false;
    bool eeprommcp3008sensor  = false;
    char eepromchecksum[ eepromchk ] = "";
  } data;
  // read data from eeprom
  EEPROM.get( eepromAddr, data );
  // validate checksum
  String checksumString = sha1( String( data.eepromssid ) + String( data.eeprompassword ) + String( data.eepromdnsname ) + String( data.eepromplace ) 
        + String( (int) data.eepromauthentication ) + String( data.eepromauthuser )  + String( data.eepromauthpass )
        + String( data.eeprommcpchannels ) + String( (int) data.eepromsilent )  + String( (int) data.eepromgcsensor ) + String( (int) data.eeprommq135sensor ) 
        + String( (int) data.eeprombme280sensor ) + String( (int) data.eepromds18b20sensor ) + String( (int) data.eeprommcp3008sensor ) 
        + String( (int) data.eepromstaticIP ) + ip2Str( data.eepromipaddr ) + ip2Str( data.eepromgateway ) + ip2Str( data.eepromsubnet ) + ip2Str( data.eepromdns1 ) + ip2Str( data.eepromdns2 ) 
        );
  char checksum[ eepromchk ];
  checksumString.toCharArray(checksum, eepromchk); // write checksumString into checksum
  if( strcmp( checksum, data.eepromchecksum ) == 0 ) { // compare with eeprom checksum
    strncpy( lastcheck, checksum, eepromchk );
    // re-set runtime variables;
    strncpy( ssid, data.eepromssid, eepromStringSize );
    strncpy( password, data.eeprompassword, eepromStringSize );
    strncpy( dnsname, data.eepromdnsname, eepromStringSize );
    strncpy( place, data.eepromplace, eepromStringSize );
    strncpy( checksum, data.eepromchecksum, eepromchk );
    staticIP = data.eepromstaticIP;
    ipaddr = data.eepromipaddr;
    gateway = data.eepromgateway;
    subnet = data.eepromsubnet;
    dns1 = data.eepromdns1;
    dns2 = data.eepromdns1;
    authentication = data.eepromauthentication;
    strncpy( authuser, data.eepromauthuser, eepromStringSize );
    strncpy( authpass, data.eepromauthpass, eepromStringSize );
    mcpchannels = data.eeprommcpchannels;
    silent = data.eepromsilent;
    gcsensor = data.eepromgcsensor;
    mq135sensor = data.eeprommq135sensor;
    bme280sensor = data.eeprombme280sensor;
    ds18b20sensor = data.eepromds18b20sensor;
    mcp3008sensor = data.eeprommcp3008sensor;
    return true;
  }
  return false;
}

// write config to eeprom
bool saveSettings() {
  // define local data structure
  struct { 
    char eepromssid[ eepromStringSize ];
    char eeprompassword[ eepromStringSize ];
    char eepromdnsname[ eepromStringSize ];
    char eepromplace[ eepromStringSize ];
    bool eepromstaticIP       = false;
    IPAddress eepromipaddr;
    IPAddress eepromgateway;
    IPAddress eepromsubnet;
    IPAddress eepromdns1;
    IPAddress eepromdns2;
    bool eepromauthentication = false;
    char eepromauthuser[eepromStringSize];
    char eepromauthpass[eepromStringSize];
    int  eeprommcpchannels    = 0;
    bool eepromsilent         = false;
    bool eepromgcsensor       = false;
    bool eeprommq135sensor    = false;
    bool eeprombme280sensor   = false;
    bool eepromds18b20sensor  = false;
    bool eeprommcp3008sensor  = false;
    char eepromchecksum[ eepromchk ] = "";
  } data;
  // write real data into struct elements
  strncpy( data.eepromssid, ssid, eepromStringSize );
  strncpy( data.eeprompassword, password, eepromStringSize );
  strncpy( data.eepromdnsname, dnsname, eepromStringSize );
  strncpy( data.eepromplace, place, eepromStringSize );
  data.eepromstaticIP = staticIP;
  data.eepromipaddr = ipaddr;
  data.eepromgateway = gateway;
  data.eepromsubnet = subnet;
  data.eepromdns1 = dns1;
  data.eepromdns2 = dns2;
  data.eepromauthentication = authentication;
  strncpy( data.eepromauthuser, authuser, eepromStringSize );
  strncpy( data.eepromauthpass, authpass, eepromStringSize );
  data.eeprommcpchannels = mcpchannels;
  data.eepromsilent = silent;
  data.eepromgcsensor = gcsensor;
  data.eeprommq135sensor = mq135sensor;
  data.eeprombme280sensor = bme280sensor;
  data.eepromds18b20sensor = ds18b20sensor;
  data.eeprommcp3008sensor = mcp3008sensor;
  // create new checksum
  String checksumString = sha1( String( data.eepromssid ) + String( data.eeprompassword ) + String( data.eepromdnsname ) + String( data.eepromplace ) 
        + String( (int) data.eepromauthentication ) + String( data.eepromauthuser )  + String( data.eepromauthpass )
        + String( data.eeprommcpchannels ) + String( (int) data.eepromsilent )  + String( (int) data.eepromgcsensor ) + String( (int) data.eeprommq135sensor ) 
        + String( (int) data.eeprombme280sensor ) + String( (int) data.eepromds18b20sensor ) + String( (int) data.eeprommcp3008sensor ) 
        + String( (int) data.eepromstaticIP ) + ip2Str( data.eepromipaddr ) + ip2Str( data.eepromgateway ) + ip2Str( data.eepromsubnet ) + ip2Str( data.eepromdns1 ) + ip2Str( data.eepromdns2 ) 
        );
  char checksum[ eepromchk ];
  checksumString.toCharArray(checksum, eepromchk);
  strncpy( data.eepromchecksum, checksum, eepromchk );
  strncpy( lastcheck, checksum, eepromchk );
  // save struct into eeprom
  EEPROM.put( eepromAddr,data );
  // commit transaction and return the write result state
  return EEPROM.commit();
}


// IPAdress 2 charArray
String ip2Str( IPAddress ip ) {
  String s="";
  for (int i=0; i<4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
}

// parsing Strings for bytes, str2ip
// https://stackoverflow.com/a/35236734
// parseBytes(String, '.', byte[], 4, 10);
void parseBytes( const char* str, char sep, byte* bytes, int maxBytes, int base ) {
  for( int i = 0; i < maxBytes; i++ ) {
    bytes[ i ] = strtoul( str, NULL, base );
    str = strchr( str, sep );
    if( str == NULL || *str == '\0' ) {
      break;
    }
    str++;
  }
}


// redirect client to captive portal after connect to wifi
boolean captivePortal() {
  server.sendHeader("Location", String("http://") + ip2Str( server.client().localIP() ) + String("/setup") );
  server.send ( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.client().stop(); // Stop is needed because we sent no content length
  return true;
}


//
// html helper functions
//

// spinner javascript
String spinnerJS() {
  String result = "";
  result += "<script type='text/javascript'>\n";
  result += "var duration = 300,\n";
  result += "    element,\n";
  result += "    step,\n";
  result += "    frames = \"-\\\\/\".split('');\n";
  result += "step = function (timestamp) {\n";
  result += "  var frame = Math.floor( timestamp * frames.length / duration ) % frames.length;\n";
  result += "  if( ! element ) {\n";
  result += "    element = window.document.getElementById( 'spinner' );\n";
  result += "  }\n";
  result += "  element.innerHTML = frames[ frame ];\n";
  result += "  return window.requestAnimationFrame( step );\n";
  result += "}\n";
  result += "window.requestAnimationFrame( step );\n";
  result += "</script>\n";
  return result;
}


// spinner css
String spinnerCSS() {
  String result = "";
  result += "<style>\n";
  result += "#spinner_wrap {\n";
  result += "  margin-top: 30%;\n";
  result += "}\n";
  result += "#spinner {\n";
  result += "  font-size: 72px;\n";
  result += "  transition: all 500ms ease-in;\n";
  result += "}\n";
  result += "</style>\n";
  return result;
}


// global javascript
String htmlJS() {
  String result = "";
  result += "<script type='text/javascript'>\n";
  // javascript to poll wifi signal
  result += "window.setInterval( function() {\n"; // polling event to get wifi signal strength
  result += "  let xmlHttp = new XMLHttpRequest();\n";
  result += "  xmlHttp.open( 'GET', '/signal', false );\n";
  result += "  xmlHttp.send( null );\n";
  result += "  let signalValue = xmlHttp.responseText;\n";
  result += "  let signal = document.getElementById('signal');\n";
  result += "  signal.innerText = signalValue + ' dBm';\n";
  result += "}, 5000 );\n";
  result += "if( window.location.pathname == '/setup' ) {\n"; // load only on setup page
  result += "  function renderRows( checkbox, classname ) {\n";
  result += "    let rows = document.getElementsByClassName( classname );\n";
  result += "    if( checkbox.checked == true ) {\n";
  result += "      for (i = 0; i < rows.length; i++) {\n";
  result += "        rows[i].style.display = 'table-row';\n";
  result += "      }\n";
  result += "    } else {\n";
  result += "      for (i = 0; i < rows.length; i++) {\n";
  result += "        rows[i].style.display = 'none';\n";
  result += "      }\n";
  result += "    }\n";
  result += "  }\n";
  result += "  window.onload = function () {\n";
  result += "    renderRows( document.getElementById('authentication'), \"authRow\" );\n";
  result += "    renderRows( document.getElementById('mcp3008sensor'), \"mcpChannelsRow\" );\n";
  result += "    renderRows( document.getElementById('staticIP'), \"staticIPRow\" );\n";
  result += "  };\n";
  result += "};\n";
  result += "</script>\n";
  return result;
}


// global css
String htmlCSS() {
  String result = "";
  result += "<style>\n";
  result += "\n";
  result += "#signalWrap {\n";
  result += "  float: right;\n";
  result += "}\n";
  result += "#signal {\n";
  result += "  transition: all 500ms ease-out;\n";
  result += "}\n";
  result += "#mcpChannelsRow, .staticIPRow, .authRow {\n";
  result += "  transition: all 500ms ease-out;\n";
  result += "  display: table-row;\n";
  result += "}\n";
  result += "@media only screen and (max-device-width: 720px) {\n";
  result += "  .links { background: red; }\n";
  result += "  .links tr td { padding-bottom: 15px; }\n";
  result += "  h1 small:before { content: \"\\A\"; }\n";
  result += "}\n";
  result += "</style>\n";
  return result;
}

// html head
String htmlHead( String title ) {
  String result = "<head>\n";
  result += "  <title>" + title + "</title>\n";
  result += "<meta http-equiv='content-type' content='text/html; charset=UTF-8' />\n";
  result += "<meta name='viewport' content='width=device-width, minimum-scale=1.0, maximum-scale=1.0' />\n";
//  result += "<link id='favicon' rel='shortcut icon' type='image/png' href='data:image/png;base64,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' />\n";
  result += "<link id='favicon' rel='shortcut icon' type='image/png' href='data:image/png;base64,AAABAAEAMDAAAAEAIAAyBQAAFgAAAIlQTkcNChoKAAAADUlIRFIAAAAwAAAAMAgGAAAAVwL5hwAABPlJREFUaIG92WmoVVUUB/Cf+crqpYWmNFFaUmEZaUQFiUVlEUFlAxI2iBVNUIRBQcSiMIogm6GiCRtIGpSygeqDhWKkNlhGFBpGRGrmUGb1rD7s+47X6zn3nXvvef1hce69e+2117p7n3X+ax2qRthTmCAMavi9X9DVDzaPxod4S1iGbXhc+Kkf1jKgMkthHzyNITitYfR5XCn8Wdl6NexSoa1BOMXOzm/Dssz5KGGpjE4NVe7ALngI1zeMbMYMLBK+bJgzHGdgBNZgubC8lWWrDGBPvIlTc0b/xu/YiAX4AIdjOoZi15rOFnyLW4QF2U6EQlQXQFpoGKbiYNzcobXXcI2wtplSdfdAIPwiPIhvKrA4GSuEszL7OahmByKTg/AMJkrHogr8jmOElXmDne1A1F3D2fgOp6vOeejGzKLBqgK4FfNpePpWh9FFA+0HENn1GtzVtp1yuLtooL0AIpPDcJ/+oSQ9+AFThdeLbuJOF34Ue3VooxGbJC41F3OEzeiHLBT2o2OCthUrsBarpAfhJ9gkbK2t0/RB1slNfFQHc+FFjMSXmC9cK8zHmsx5+uRF5QKIXEOTS81N+Dvnt7OwFJdhlnDFDuuVRGtHKEzDDdgPw5XP9/Ml/nN/E50eLBBOb8WlvncgsuvVEt8fjwO09rA6Hi9JPOlHydlGdGFYCzZRNoDENDshZyPwLt7BKFwlny+dna1ZEsVHKHTjCWzAOtxR3mwhVmCCsL6WxZ6SnP5ICu4BbKkqgGH4GIe17W4+XsU0afcH4iS8J/zVjrH8IxQSNU4Z4rd2DOeg99xPxgtSYbMKw4W/2u1aNNuB/bFIKk4aA12MMVIB3xe24gLp5s/jTMuE48o4m4fiHUj//Nc5oz1S7TuhhP0PpX/4LSn75OHOujVbRrMjtLnG8V9uGO3CCcIXeKXA7jY8JkwU2RE8t0D3XmFMX5ShCMVpdLuxm9ipGhpXy1L3Fcxeh9vrbI2VqrR69GA9jsDbwl7tBFDMRiOTNXozUdgDj0gPsX+kCmwlDm2Y3Y0T8Xbt+3opqN7M0y21W46vBbBP3U61hPJUIuy8xaELT1LHY7ZjA2bhYeHXWu0wXHL+YhyDS4RVmf36a0l0Qqd75SLMVlxOrsaNwty6uV1SHbFR+LdtH1TRlQgD8IXU1G2uyUyhp90bNg+dBRCZjJUKkb6K+tmYLnLpdVvovCuRZLnU1O2LDlwqZbXK0HlnLjJZKKXKVX3MuE7Yt+N1a6imtRiZLMY43IM/CrRHShmoElTdGyVsxG1S23xTgfakqpZtxoV2/twXdpy3UHrhkYdDWrDaFM240IXCgLbSXWTyqZSdGlFZC7LZEZqhtc5DEfJq58pe+DULYB3OEwa2vAuRyWgcm6OxsEWLhWgWwAeYQotUNzLZG3NyNLZKdUIlaBbAPImtvi8MLhVEZLKbVPuOy9F6FT/3L5VIxgfhWWkX1uNMLM3IV+yg2/t5VynHz8OBOZbXYpLwWUde16E5FwpjpJbHUOlVz5N4Vvi8Qe8QnCzVvucXWNuCKcIb/y+ZC+OxpE53g/S6dLV0BA/AYKnA363Ayvc4R/iqZvN/CiAyGSW1vo9U/um9TQr0OdyatU6iLT8LUWYHeqUbl+NCqRm1e472JqnEXCI1xeYK63awUzHK1QP1i6dAhkj9zv1rv26RjtQf+BO/iVojq35uP+A/urI6Gkydy9MAAAAASUVORK5CYII=' />\n";
  result += htmlJS() + "\n";
  result += htmlCSS() + "\n";
  result += "</head>\n";
  return result;
}

// html header
String htmlHeader( String device ) {
  String result = "<div id='signalWrap'>Signal Strength: <span id='signal'>" + String( WiFi.RSSI() ) + " dBm</span></div>"; 
  result += "<h1>" + device + " <small style='color:#ccc'>a D1Mini Node Exporter</small></h1>\n";
  result += "<hr />\n";
  return result;
}

// html footer
String htmlFooter() {
  String result = "<hr style='margin-top:40px;' />\n";
  result += "<p style='color:#ccc;float:right'>";
  result += staticIP ? "Static" : "";
  result += "Node IP: " + ip + "</p>\n";
  result += "<p style='color:#ccc'><strong>S</strong>imple<strong>ESP</strong> v" + String( history ) + "</p>\n";
  return result;
}

// html body
String htmlBody( String content ) {
  String result = "<!doctype html>\n";
  result += htmlHead( String( dnsname ) );
  result += "<body>\n";
  result += htmlHeader( String( dnsname ) );
  result += content;
  result += htmlFooter();
  result += "</body>\n";
  result += "</html>";
  return result;
}

// initial page
void handle_root() {
  String result = "";
  // create current checksum
  String checksumString = sha1( String( ssid ) + String( password ) + String( dnsname ) + String( place ) 
      + String( (int) authentication ) + String( authuser )  + String( authpass )
      + String( mcpchannels ) + String( (int) silent )  + String( (int) gcsensor ) + String( (int) mq135sensor ) 
      + String( (int) bme280sensor ) + String( (int) ds18b20sensor ) + String( (int) mcp3008sensor ) 
      + String( (int) staticIP ) + ip2Str( ipaddr ) + ip2Str( gateway ) + ip2Str( subnet ) + ip2Str( dns1 ) + ip2Str( dns2 ) 
      );
  char checksum[ eepromchk ]; 
  checksumString.toCharArray(checksum, eepromchk); // write checksumString (Sting) into checksum (char Arr)
  result += "<h2>Info</h2>\n";
  result += "<p>A Prometheus scrape ready BME280/MQ135/DS18B20/Geigercounter Node Exporter.<br />\n";
  result += "Just add this Node to your prometheus.yml as Target to collect Air Pressure,<br />\n";
  result += "Air Humidity, Temperatures, Radiation and some Air Quality Data like Alcohole,<br />\n";
  result += "CO, CO2, Ammonium, Acetone and Diphenylmethane.</p>\n";
  result += "<h2>Links</h2>\n";
  result += "<table id='links'>\n";
  result += "  <tr><td><a href='/metrics'>/metrics</a></td><td></td></tr>\n";
  result += "  <tr><td><a href='/setup'>/setup</a></td><td></td></tr>\n";
  result += "  <tr><td><a href='/restart' onclick=\"return confirm('Restart the Device?');\">/restart</a></td><td></td></tr>\n";
  result += "  <tr><td><a href='/restart?reset=1' onclick=\"return confirm('Reset the Device to Factory Defaults?');\">/reset</a></td><td>*</td></tr>\n";
  result += "</table>\n";
  result += "<p>* Clear EEPROM, will reboot in Setup (Wifi Acces Point) Mode named <strong>" + String( dnsname ) + "_setup</strong>!</p>";
  result += "<h2>Enabled Sensors</h2>\n";
  result += "<table>\n";
  if( gcsensor ) {
    result += "<tr><td>Geigercounter</td><td>MightyOhm Geigercounter</td><td>(RX " + String( gcrx ) + ", TX " + String( gctx ) + ")</td></tr>\n";
  }
  if( mq135sensor ) {
    result += "<tr><td>MQ135</td><td>Air Quality Sensor</td><td>(Pin " + String( mqpin ) + ")</td></tr>\n";
  }
  if( ds18b20sensor ) {
    result += "<tr><td>DS18B20</td><td>Temperature Sensor</td><td>(Pin " + String( ONE_WIRE_BUS ) + ")</td></tr>\n";
  }
  if( bme280sensor ) {
    result += "<tr><td>BME280</td><td>Air Temperature, Humidity and Pressure Sensor</td><td>(Adress " + String( BME280_ADDRESS ) + ")</td></tr>\n";
  }
  if( mcp3008sensor ) {
    result += "<tr><td>MCP3008</td><td>MCP3008 Analoge Digital Converter</td><td>(Channels " + String( mcpchannels ) + ")</td></tr>\n";
  }
  result += "<tr><td></td><td></td></tr>\n"; // space
  if( silent ) {
    result += "<tr><td>Silent Mode</td><td>Enabled (LED disabled)</td></tr>\n";
  } else {
    result += "<tr><td>Silent Mode</td><td>Disabled (LED blink on Request)</td></tr>\n";
  }
  if( strcmp( checksum, lastcheck ) == 0 ) { // compare checksums
      result += "<tr><td>Checksum</td><td><span style='color:green;'>&#10004;</span></td></tr>\n";
    } else {
      result += "<tr><td>Checksum</td><td><span style='color:red;'>&#10008;</span> Invalid, plaese <a href='/setup'>Setup</a> your Device.</td></tr>\n";
    }
  result += "<table>\n";
  response( htmlBody( result ), "text/html");
}


//
// End helper functions
//


//
// Setup
//

void setup() {

  if( ! silent ) { 
    // Initialize the LED_BUILTIN pin as an output
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED off by HIGH!
  }

  // Start eeprom 
  EEPROM.begin( eepromSize );
  // load data from eeprom
  bool loadingSetup = loadSettings(); // comment out to keep initialize data after crash, reboot and uncomment

  // start Wifi mode depending on Load Config (no valid config = start in AP Mode)
  if( loadingSetup ) {
    // use a static IP?
    if( staticIP ) {
      WiFi.config( ipaddr, gateway, subnet, dns1, dns2 ); // static ip adress
    }
    // Setup Wifi mode
    // WIFI_AP = Wifi Accesspoint
    // WIFI_STA = Wifi Client
    // WIFI_AP_STA = Wifi Accesspoint Client (Mesh)
    // WIFI_OFF = Wifi off
    WiFi.mode( WIFI_STA );
    // set dnshostname
    WiFi.hostname( dnsname );
    // wifi connect to AP
    WiFi.begin( ssid, password );
    while( WiFi.status() != WL_CONNECTED ) {
      if( !silent ) {
        digitalWrite(LED_BUILTIN, LOW);
        delay( 200 );
        digitalWrite(LED_BUILTIN, HIGH);
      }
      delay( 200 );
    }
    delay(200);
    // get the local ip adress
    ip = WiFi.localIP().toString();

  } else {

    // Setup Wifi mode
    WiFi.mode( WIFI_AP );

    // set dnshostname
    WiFi.hostname( dnsname );
    // start in AP mode, without Password
    String appString = String( dnsname ) + "_setup";
    char apname[ eepromStringSize ];
    appString.toCharArray(apname, eepromStringSize);
    SoftAccOK = WiFi.softAP( apname, "" );
//    while( WiFi.status() != WL_CONNECTED ) {
    digitalWrite(LED_BUILTIN, LOW);
    delay( 200 );
    digitalWrite(LED_BUILTIN, HIGH);
    delay( 750);
    digitalWrite(LED_BUILTIN, LOW);
    delay( 200 );
    digitalWrite(LED_BUILTIN, HIGH);
    delay( 750);
    digitalWrite(LED_BUILTIN, LOW);
    delay( 200 );
    digitalWrite(LED_BUILTIN, HIGH);
//    }
    delay(200);
    // enable flashing led
    //flashing( LED_BUILTIN, 300, 300 );
    ip = WiFi.softAPIP().toString();
    /* Setup the DNS server redirecting all the domains to the apIP */
    // https://github.com/tzapu/WiFiManager
    dnsServer.setErrorReplyCode( DNSReplyCode::NoError );
    dnsServer.start( DNS_PORT, "*", WiFi.softAPIP() );
    captivePortal();
    delay(200);
  }
  
  if( gcsensor ) {
    mySerial.begin(9600); //Set up Software Serial Port
  }

  if( mq135sensor ) {
    MQ135.inicializar();
    MQ135.setVoltResolution( 5 );
  } else {
//    ADC_MODE(ADC_VCC);
  }

  if( bme280sensor ) {
    // Wire connection for bme Sensor
    Wire.begin();
    BMEstatus = bme.begin();  
    if (!BMEstatus) { while (1); }
  }

  if( ds18b20sensor ) {
    // Initialize the ds18b20 Sensors
    DS18B20.begin();
    // Set Precision
    for( int i = 0; i < tempCount; i++ ) {
      DS18B20.setResolution( tempAdd[i], TEMPERATURE_PRECISION );
      delay(100);
    }
    // Get amount of devices
    tempCount = DS18B20.getDeviceCount();
  }

  if( mcp3008sensor ) {
    SPI.begin();
    SPI.setClockDivider( SPI_CLOCK_DIV8 );
    pinMode( CS, OUTPUT );
    digitalWrite( CS, HIGH );
  }

  // 404 Page
  server.onNotFound( []() {
    // server.header("User-Agent"); // get user agent String
    // server.uri(); // get requested uri
    String result = "";
    result += "<div>";
    result += "  <p>404 Page not found.</p>";
    result += "  <p>Where sorry, but are unable to find page: <strong>" + String( server.uri() ) + "</strong></p>";
    result += "</div>";
    response( htmlBody( result ), "text/html");
  });

  // default, main page
  server.on("/", handle_root );

  // ios captive portal detection request
  server.on("/hotspot-detect.html", captivePortal );

  // android captive portal detection request
  server.on("/generate_204", captivePortal );

  // simple wifi signal strength
  server.on("/signal", HTTP_GET, []() {
    response( String( WiFi.RSSI() ), "text/plain");
  });

  // form page
  server.on("/setup", HTTP_GET, []() {
    if( authentication && ! SoftAccOK ) {
      if( ! server.authenticate( authuser, authpass ) ) {
        return server.requestAuthentication();
      }
    }
    String result = "";
    String checked = silent ? "checked" : "";
    result += "<h2>Setup</h2>\n";
    result += "<div id='msg'></div>\n";
    result += "<div>";
    result += "  <p>After Submit, the System will save Configuration into EEPROM, ";
    result += "  do a reboot and reload config from EEPROM<span style='color:red'>!</span></p>";
    result += "</div>\n";
    result += "<form action='/form' method='POST' id='setup'>\n";
    result += "<table border='0'>\n";
    result += "<tbody>\n";
    result += "  <tr><td><strong>Wifi Settings</strong></td><td></td></tr>\n"; // head line
    result += "  <tr>\n";
    result += "    <td><label for='ssid'>SSID<span style='color:red'>*</span>: </label></td>\n";
    result += "    <td><input id=''ssid name='ssid' type='text' placeholder='Wifi SSID' value='" + String( ssid ) + "' size='" + String( eepromStringSize ) + "' required /></td>\n";
    result += "  </tr>\n";
    result += "  <tr>\n";
    result += "    <td><label for='password'>Password<span style='color:red'>*</span>: </label></td>\n";
    result += "    <td><input id='password' name='password' type='password' placeholder='Wifi Password' value='" + String( password ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
    result += "  </tr>\n";
    result += "  <tr>\n";
    result += "    <td><label for='dnsname'>DNS Name<span style='color:red'>*</span>: </label></td>\n";
    result += "    <td><input id='dnsname' name='dnsname' type='text' placeholder='DNS Name' value='" + String( dnsname ) + "' size='" + String( eepromStringSize ) + "' required /></td>\n";
    result += "  </tr>\n";
    String staticIPChecked = staticIP ? "checked" : "";
    result += "  <tr>\n";
    result += "    <td><label for='staticIP'>Static IP: </label></td>\n";
    result += "    <td><input id='staticIP' name='staticIP' type='checkbox' onclick='renderRows( this, \"staticIPRow\" )' " + staticIPChecked + " /></td>\n";
    result += "  </tr>\n";
    result += "  <tr class='staticIPRow'>\n";
    result += "    <td><label for='ipaddr'>IP: </label></td>\n";
    result += "    <td><input id='ipaddr' name='ipaddr' type='text' placeholder='192.168.1.X' value='" + ip2Str( ipaddr ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
    result += "  </tr>\n";
    result += "  <tr class='staticIPRow'>\n";
    result += "    <td><label for='gateway'>Gateway: </label></td>\n";
    result += "    <td><input id='gateway' name='gateway' type='text' placeholder='192.168.1.X' value='" + ip2Str( gateway ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
    result += "  </tr>\n";
    result += "  <tr class='staticIPRow'>\n";
    result += "    <td><label for='subnet'>Subnet: </label></td>\n";
    result += "    <td><input id='subnet' name='subnet' type='text' placeholder='255.255.255.0' value='" + ip2Str( subnet ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
    result += "  </tr>\n";
    result += "  <tr class='staticIPRow'>\n";
    result += "    <td><label for='dns1'>DNS: </label></td>\n";
    result += "    <td><input id='dns1' name='dns1' type='text' placeholder='192.168.1.X' value='" + ip2Str( dns1 ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
    result += "  </tr>\n";
    result += "  <tr class='staticIPRow'>\n";
    result += "    <td><label for='dns2'>DNS 2: </label></td>\n";
    result += "    <td><input id='dns2' name='dns2' type='text' placeholder='8.8.8.8' value='" + ip2Str( dns2 ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
    result += "  </tr>\n";
    result += "  <tr><td>&nbsp;</td><td></td></tr>\n"; // dummy line
    result += "  <tr><td><strong>Device Settings</strong></td><td></td></tr>\n"; // head line
    result += "  <tr>\n";
    result += "    <td><label for='place'>Place: </label></td>\n";
    result += "    <td><input id='place' name='place' type='text' placeholder='Place Description' value='" + String( place ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
    result += "  </tr>\n";
    result += "  <tr>\n";
    result += "    <td><label for='silent'>Silent Mode: </label></td>\n";
    result += "    <td><input id='silent' name='silent' type='checkbox' " + checked + "  /></td>\n";
    result += "  </tr>\n";
    result += "  <tr><td>&nbsp;</td><td></td></tr>\n"; // dummy line

    result += "  <tr><td><strong>Basic Authentication</strong></td><td></td></tr>\n"; // head line
    String authenticationChecked = authentication ? "checked" : "";
    result += "  <tr>\n";
    result += "    <td><label for='authentication'>Basic Auth: </label></td>\n";
    result += "    <td><input id='authentication' name='authentication' type='checkbox' onclick='renderRows( this, \"authRow\" )' " + authenticationChecked + " /></td>\n";
    result += "  </tr>\n";
    result += "  <tr class='authRow'>\n";
    result += "    <td><label for='authuser'>Username: </label></td>\n";
    result += "    <td><input id='authuser' name='authuser' type='text' placeholder='Username' value='" + String( authuser ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
    result += "  </tr>\n";
    result += "  <tr class='authRow'>\n";
    result += "    <td><label for='authpass'>User Password: </label></td>\n";
    result += "    <td><input id='authpass' name='authpass' type='password' placeholder='Password' value='" + String( authpass ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
    result += "  </tr>\n";
    result += "  <tr><td>&nbsp;</td><td></td></tr>\n"; // dummy line
    
    result += "  <tr><td><strong>Sensor Setup</strong></td><td></td></tr>\n"; // head line
    String gcChecked = gcsensor ? "checked" : "";
    result += "  <tr>\n";
    result += "    <td><label for='gcsensor'>Radiation Sensor: </label></td>\n";
    result += "    <td><input id='gcsensor' name='gcsensor' type='checkbox' " + gcChecked + "  /> <span style='color:red'>**</span></td>\n";
    result += "  </tr>\n";
    String mq135Checked = mq135sensor ? "checked" : "";
    result += "  <tr>\n";
    result += "    <td><label for='mq135sensor'>MQ-135 Sensors: </label></td>\n";
    result += "    <td><input id='mq135sensor' name='mq135sensor' type='checkbox' " + mq135Checked + "  /></td>\n";
    result += "  </tr>\n";
    String bme280Checked = bme280sensor ? "checked" : "";
    result += "  <tr>\n";
    result += "    <td><label for='bme280sensor'>BME280 Sensors: </label></td>\n";
    result += "    <td><input id='bme280sensor' name='bme280sensor' type='checkbox' " + bme280Checked + "  /> <span style='color:red'>**</span></td>\n";
    result += "  </tr>\n";
    String ds18b20Checked = ds18b20sensor ? "checked" : "";
    result += "  <tr>\n";
    result += "    <td><label for='ds18b20sensor'>DS18B20 Sensors: </label></td>\n";
    result += "    <td><input id='ds18b20sensor' name='ds18b20sensor' type='checkbox' " + ds18b20Checked + "  /></td>\n";
    result += "  </tr>\n";
    String mcp3008Checked = mcp3008sensor ? "checked" : "";
    result += "  <tr>\n";
    result += "    <td><label for='mcp3008sensor'>MCP 300X ADC: </label></td>\n";
    result += "    <td><input id='mcp3008sensor' name='mcp3008sensor' type='checkbox' " + mcp3008Checked + " onclick='renderRows( this, \"mcpChannelsRow\" )' /></td>\n";
    result += "  </tr>\n";
    result += "  <tr class='mcpChannelsRow'>\n";
    result += "    <td><label for='mcpchannels'>MCP Channels: </label></td>\n";
    result += "    <td><input id='mcpchannels' name='mcpchannels' type='number' min='0' max='8' value='" + String( mcpchannels ) + "' /></td>\n";
    result += "  </tr>\n";
    result += "  <tr><td>&nbsp;</td><td></td></tr>\n"; // dummy line
    result += "  <tr>\n";
    result += "    <td></td><td><button name='submit' type='submit'>Submit</button></td>\n";
    result += "  </tr>\n";
    result += "</tbody>\n";
    result += "</form>\n";
    result += "</table>\n";
    result += "<div>";
    result += "  <p><span style='color:red'>*</span>&nbsp; Required fields!</p>\n";
    result += "  <p><span style='color:red'>**</span> Enable only if sensor is pluged in!</p>\n";
    result += "</div>\n";
    result += "<a href='/'>Zur&uuml;ck</a>\n";
    response( htmlBody( result ), "text/html");
  });

  // form target 
  server.on("/form", HTTP_POST, []() {
    // validate required args are set
    if( ! server.hasArg( "ssid" ) || ! server.hasArg( "password" ) || ! server.hasArg( "dnsname" ) ) {
      server.send(400, "text/plain", "400: Invalid Request, Missing one of required form field field (ssid, password or dnsname).");
      return;
    }
    // validate args are not empty
    if( server.arg( "ssid" ) == "" || server.arg( "password" ) == "" || server.arg( "dnsname" ) == "" ) {
      server.send(400, "text/plain", "400: Invalid Request, Empty form field value detected.");
      return;
    }
    // write new values to current setup
    // Wifi Settings
    String ssidString = server.arg("ssid");
    ssidString.toCharArray(ssid, eepromStringSize);
    String passwordString = server.arg("password");
    passwordString.toCharArray(password, eepromStringSize);
    String dnsnameString = server.arg("dnsname");
    dnsnameString.toCharArray(dnsname, eepromStringSize);
    String placeString = server.arg("place");
    // Place
    placeString.toCharArray(place, eepromStringSize);
    // Static IP stuff
    staticIP = server.arg("staticIP") == "on" ? true : false;
    String ipaddrString = server.arg("ipaddr");
    const char* ipStr = ipaddrString.c_str();
    byte tmpip[4];
    parseBytes(ipStr, '.', tmpip, 4, 10);
    ipaddr = tmpip;
    String gatewayString = server.arg("gateway");
    const char* gatewayStr = gatewayString.c_str();
    byte tmpgateway[4];
    parseBytes(gatewayStr, '.', tmpgateway, 4, 10);
    gateway = tmpgateway;
    String subnetString = server.arg("subnet");
    const char* subnetStr = subnetString.c_str();
    byte tmpsubnet[4];
    parseBytes(subnetStr, '.', tmpsubnet, 4, 10);
    subnet = tmpsubnet;
    String dns1String = server.arg("dns1");
    const char* dns1Str = dns1String.c_str();
    byte tmpdns1[4];
    parseBytes(dns1Str, '.', tmpdns1, 4, 10);
    dns1 = tmpdns1;
    String dns2String = server.arg("dns2");
    const char* dns2Str = dns2String.c_str();
    byte tmpdns2[4];
    parseBytes(dns2Str, '.', tmpdns2, 4, 10);
    dns2 = tmpdns2;
    // Authentication stuff
    authentication = server.arg("authentication") == "on" ? true : false;
    String authuserString = server.arg("authuser");
    authuserString.toCharArray(authuser, eepromStringSize);
    String authpassString = server.arg("authpass");
    authpassString.toCharArray(authpass, eepromStringSize);
    // Sensor stuff
    String mcpchannelsString = server.arg("mcpchannels");
    mcpchannels = mcpchannelsString.toInt();
    silent = server.arg("silent") == "on" ? true : false; // on = true
    gcsensor = server.arg("gcsensor") == "on" ? true : false;
    mq135sensor = server.arg("mq135sensor") == "on" ? true : false;
    bme280sensor = server.arg("bme280sensor") == "on" ? true : false;
    ds18b20sensor = server.arg("ds18b20sensor") == "on" ? true : false;
    mcp3008sensor = server.arg("mcp3008sensor") == "on" ? true : false;
    // save data into eeprom
    saveSettings();
    delay(50);
    // send user to restart page
    server.sendHeader( "Location","/restart" );
    server.send( 303 );
//    String webString = "";
//    webString += "See Serial Console for more info!";
//    Serial.println( "debug infos" );
//    response( webString, "text/html" );
  });

  // form page
  server.on("/restart", HTTP_GET, []() {
    String webString = "<html>\n";
    webString += "<head>\n";
    webString += "  <meta http-equiv='Refresh' content='4; url=/' />\n";
    webString += spinnerJS();
    webString += spinnerCSS();
    webString += "</head>\n";
    webString += "<body style=\"text-align:center;\">\n";
    webString += "  <div id='spinner_wrap'><span id='spinner'>Loading...</span></div>\n";
    webString += "</body>\n";
    webString += "</html>\n";
    response( webString, "text/html" );
    if( server.hasArg( "reset" ) ) {
      // clear eeprom
      for (int i = 0 ; i < EEPROM.length() ; i++) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
    }
    delay(50);
    ESP.restart();
  });

  // form page
  server.on("/reset", HTTP_GET, []() {
    String webString = "<html>\n";
    webString += "<head>\n";
    webString += "  <meta http-equiv='Refresh' content='4; url=/' />\n";
    webString += spinnerJS();
    webString += spinnerCSS();
    webString += "</head>\n";
    webString += "<body style=\"text-align:center;\">\n";
    webString += "  <div id='spinner_wrap'><span id='spinner'>Loading...</span></div>\n";
    webString += "</body>\n";
    webString += "</html>\n";
    response( webString, "text/html" );
    delay(50);
    ESP.reset();
  });

  // all (txt, prometheus metrics)
  server.on( "/metrics", HTTP_GET, []() {
    //Serial.println( server.client().remoteIP() ); // Client IP
    //Serial.println( WiFi.localIP().toString() ); // Host IP
    unsigned long currentMillis = millis();
    webString = "";
    // Info and Voltage 
    webString += "# HELP esp_firmware_build_info A metric with a constant '1' value labeled by version, board type, dhttype and dhtpin\n";
    webString += "# TYPE esp_firmware_build_info gauge\n";
    webString += "esp_firmware_build_info{";
    webString += "version=\"" + String( history ) + "\"";
    webString += ",board=\"" + String( ARDUINO_BOARD ) + "\"";
    webString += ",nodename=\"" + String( dnsname ) + "\"";
    webString += ",nodeplace=\"" + String( place ) + "\"";
    webString += "} 1\n";
    String wifiRSSI = String( WiFi.RSSI() );
    webString += "# HELP esp_wifi_rssi Wifi Signal Level in dBm\n";
    webString += "# TYPE esp_wifi_rssi gauge\n";
    webString += "esp_wifi_rssi " + wifiRSSI.substring( 1 ) + "\n";
    webString += "# HELP esp_mode_dhcp Network configured by DHCP Request\n";
    webString += "# TYPE esp_mode_dhcp gauge\n";
    webString += "esp_mode_dhcp ";
    webString += staticIP ? "0" : "1";
    webString += "\n";
    webString += "# HELP esp_mode_silent Silent Mode enabled = 1, disabled = 0\n";
    webString += "# TYPE esp_mode_silent gauge\n";
    webString += "esp_mode_silent ";
    webString += silent ? "1" : "0";
    webString += "\n";
    // Voltage depending on what sensors are in use (Pin A0?)
    webString += "# HELP esp_voltage current Voltage metric\n";
    webString += "# TYPE esp_voltage gauge\n";
    if( mq135sensor ) {
      webString += "esp_voltage " + String( (float) ( ESP.getVcc() / 1024.0 / 10.0 ) - mqvoltage ) + "\n";
    } else {
      webString += "esp_voltage " + String( (float) ESP.getVcc() / 1024.0 ) + "\n"; // 1024.0 / 10.0
    }
    if( gcsensor ) {
      // Geigercounter Output
      webString += getGCData();
    }
    if( ds18b20sensor ) {
      // DS18B20 Output
      webString += getDS18B20Data();
    }
    if( bme280sensor ) {
      // BME280 Output
      webString += getBME280Data();
    }
    if( mcp3008sensor ) {
      // MCP3008 Output
      webString += getMCPData();
    }
    if( mq135sensor ) {
      // MQ135 Output
      webString += getMQ135Data();
    }
    // Total Runtime
    long TOTALRuntime = millis() - currentMillis;
    webString += "# HELP esp_total_runtime in milli Seconds to collect data from all Sensors\n";
    webString += "# TYPE esp_total_runtime gauge\n";
    webString += "esp_total_runtime " + String( TOTALRuntime ) + "\n";
    // End of data, response the request
    response( webString, "text/plain" );
  });

  // Starting the server
  server.begin();
  
} // end setup

//
// main loop (handle http requests)
//

void loop() {

  // DNS redirect clients
  if( SoftAccOK ) {
    dnsServer.processNextRequest();
  }

  // handle http requests
  server.handleClient();

}

// End Of File