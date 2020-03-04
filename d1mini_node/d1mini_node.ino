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

  Overview of D1 Mini Pin layout (and the Arduino IDE mapping):
  - https://escapequotes.net/esp8266-wemos-d1-mini-pins-and-diagram/
  - https://github.com/esp8266/Arduino/blob/master/variants/d1_mini/pins_arduino.h#L49-L61

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

  Shild Size: 8 x 10 Platine
  Shild Layout: comming soon
  
*/

// default includes
#include <Hash.h>
#include <EEPROM.h>
#include <DNSServer.h>
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
const int eepromStringSize      = 24;     // maximal byte size for strings (ssid, wifi-password, dnsname)

char ssid[eepromStringSize]     = ""; // wifi SSID name
char password[eepromStringSize] = "";     // wifi wpa2 password
char dnsname[eepromStringSize]  = "esp"; // Uniq Networkname
char place[eepromStringSize]    = "";     // Place of the Device
int  port                       = 80;     // Webserver port (default 80)
bool silent                     = false;  // enable/disable silent mode
bool debug                      = true;   // enable/disable silent mode

bool gcsensor                   = false;   // enable/disable mightyohm geigercounter (unplug to flash!)
bool mq135sensor                = false;   // enable/disable mq-sensor. They use Pin A0 and will disable the accurate vcc messuring
bool bme280sensor               = false;   // enable/disable bme280 (on address 0x76). They use Pin 5 (SCL) and Pin 4 (SDA) for i2c
bool ds18b20sensor              = false;   // enable/disable ds18b20 temperatur sensors. They use Pin D3 as default for OneWire
bool mcp3008sensor              = false;  // enable/disable mcp3008 ADC. Used Pins Clock D5, MISO D6, MOSI D7, CS D8
int  mcpchannels                = 0;      // Amount of visible MCP 300X Channels, MCP3008 = max 8, MCP3004 = max 4

const char* history             = "3.1";  // Software Version
String      webString           = "";     // String to display
String      ip                  = "127.0.0.1";  // default ip, will overwriten
const int   eepromAddr          = 0;      // default eeprom Address
const int   eepromSize          = 512;    // default eeprom size in kB (see datasheet of your device)
const int   eepromchk           = 48;     // byte buffer for checksum
char        lastcheck[ eepromchk ] = "";  // last created checksum
int         configSize          = 0;

unsigned long previousMillis = 0;         // flashing led timer value
bool SoftAccOK    = false;                // value for captvie portal function 
bool captiveCall  = false;                // value to ensure we have a captive request

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
String dnssearch;               // DNS Search Domain

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
int gcerror            = 0;
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
  int parserCounter = 0;
  if( ! debug ) { // ensure, serial is not in use!
    while( data == "" ) {
      parserCounter += 1;
      if( parserCounter > 100 ) { 
        gcerror = 1;
        break; 
      }
      data = readGCSerial();
      delay( 50 );
    }
  } else {
    gcerror = 2;
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
  result += ", error=\"" + String( gcerror ) + "\"";
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
  if( mcpchannels > 0 ) {
    for( size_t i = 0; i < mcpchannels; ++i ) {
      result += "esp_mcp_data{device=\"" + String( i ) + "\"} " + String( channelData[ i ] ) + "\n";
    }
  } else {
    result += "esp_mcp_data{device=\"99\"} 0\n";
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
  if( dnssearch.length() == 0 && ! isIp( server.hostHeader() ) ) { // only if not in AP Mode  ! SoftAccOK &&
    String fqdn = server.hostHeader(); // esp22.speedport.ip
    int hostnameLength = String( dnsname ).length() + 1;
    dnssearch = fqdn.substring( hostnameLength );
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

// debug output to serial
void debugOut( String caller ) {
  if( debug ) {
    Serial.print( "* Request (" );
    Serial.print( caller );
    Serial.print( "): " );
    Serial.println( server.uri() );
    if( SoftAccOK ) {
      Serial.print( "  Remote IP: " );
      Serial.println( server.client().remoteIP() ); // Connected Client IP
      Serial.print( "  Local IP: " );
      Serial.println( ip ); // Device (Server) IP
    } else {
      Serial.print( "  Remote IP: " );
      Serial.println( server.client().remoteIP() ); // Connected Client IP
      Serial.print( "  Local IP: " );
      Serial.println( server.client().localIP() ); // Device (Server) IP
    }
//    Serial.print( "  Warning: " );
//    Serial.println( server.header("Warning") );
//    Serial.print( "  Range: " );
//    Serial.println( server.header("Range") );
//    Serial.print( "  Pragma: " );
//    Serial.println( server.header("Pragma") );
//    Serial.print( "  Cache-Control: " );
//    Serial.println( server.header("Cache-Control") );
//    Serial.print( "  Connection: " );
//    Serial.println( server.header("Connection") );
//    Serial.print( "  Forwarded: " );
//    Serial.println( server.header("Forwarded") );
    Serial.print( "  Host Header: " );
    Serial.println( server.hostHeader() );
//    Serial.print( "  Host Setup: " );
//    Serial.println( ( String( dnsname ) + "." + String( dnssearch ) ) );
//    Serial.print( "  DNS Search Length: " );
//    Serial.println( dnssearch.length() );
//    Serial.print( "  Referer: " );
//    Serial.println( server.header("Referer") );
//    Serial.print( "  Accept: " );
//    Serial.println( server.header("Accept") );
//    Serial.print( "  UA String: " );
//    Serial.println( server.header("User-Agent") );
//    Serial.print( "  Captive Mode: " );
//    Serial.println( (int) SoftAccOK );
//    Serial.print( "  Captive Call: " );
//    Serial.println( (int) captiveCall );
    if( SoftAccOK ) {
      Serial.print( "  Connected Wifi Clients: " );
      Serial.println( (int) WiFi.softAPgetStationNum() );
    }
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
    bool eepromdebug          = false;
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
        + String( (int) data.eeprombme280sensor ) + String( (int) data.eepromds18b20sensor ) + String( (int) data.eeprommcp3008sensor ) + String( (int) data.eepromdebug ) 
        + String( (int) data.eepromstaticIP ) + ip2Str( data.eepromipaddr ) + ip2Str( data.eepromgateway ) + ip2Str( data.eepromsubnet ) + ip2Str( data.eepromdns1 ) + ip2Str( data.eepromdns2 ) 
        );
  char checksum[ eepromchk ];
  checksumString.toCharArray(checksum, eepromchk); // write checksumString into checksum
  if( strcmp( checksum, data.eepromchecksum ) == 0 ) { // compare with eeprom checksum
    strncpy( lastcheck, checksum, eepromchk );
    configSize = sizeof( data );
    // re-set runtime variables;
    strncpy( ssid, data.eepromssid, eepromStringSize );
    strncpy( password, data.eeprompassword, eepromStringSize );
    strncpy( dnsname, data.eepromdnsname, eepromStringSize );
    strncpy( place, data.eepromplace, eepromStringSize );
    strncpy( checksum, data.eepromchecksum, eepromchk );
    staticIP = data.eepromstaticIP;
    debug = data.eepromdebug;
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
    bool eepromdebug          = false;
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
  data.eepromdebug = debug;
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
        + String( (int) data.eeprombme280sensor ) + String( (int) data.eepromds18b20sensor ) + String( (int) data.eeprommcp3008sensor ) + String( (int) data.eepromdebug ) 
        + String( (int) data.eepromstaticIP ) + ip2Str( data.eepromipaddr ) + ip2Str( data.eepromgateway ) + ip2Str( data.eepromsubnet ) + ip2Str( data.eepromdns1 ) + ip2Str( data.eepromdns2 ) 
        );
  char checksum[ eepromchk ];
  checksumString.toCharArray(checksum, eepromchk);
  strncpy( data.eepromchecksum, checksum, eepromchk );
  strncpy( lastcheck, checksum, eepromchk );
  configSize = sizeof( data );
  // save struct into eeprom
  EEPROM.put( eepromAddr,data );
  // commit transaction and return the write result state
  return EEPROM.commit();
}

// is String an ip?
bool isIp(String str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

// IPAdress 2 charArray
String ip2Str( IPAddress ip ) {
  String s="";
  for (int i=0; i<4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
}

// parsing Strings for bytes, str2ip or str2arp
// https://stackoverflow.com/a/35236734
// parseBytes(String, '.', byte[], 4, 10); // ip adress
// parseBytes(String, ':', byte[], 8, 16); // mac adress
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
  result += "  margin-top: 25%;\n";
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
  if( ! captiveCall ) { // deactivate if in setup mode (Captive Portal enabled)
    result += "window.setInterval( function() {\n"; // polling event to get wifi signal strength
    result += "  let xmlHttp = new XMLHttpRequest();\n";
    result += "  xmlHttp.open( 'GET', '/signal', false );\n";
    result += "  xmlHttp.send( null );\n";
    result += "  let signalValue = xmlHttp.responseText;\n";
    result += "  let signal = document.getElementById('signal');\n";
    result += "  signal.innerText = signalValue + ' dBm';\n";
    result += "}, 5000 );\n";
  }
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
  result += "  h1 small:before { content: \"\\A\"; white-space: pre; }\n";
  result += "  #signalWrap { margin-top: 15px; }\n";
  result += "  #spinner_wrap { margin-top: 45%; }\n";
  result += "  #links a { background: #ccc; display: block; width: 100%; min-width: 300px; min-height: 40px; text-align: center; vertical-align: middle; padding-top: 20px; border-radius: 10px; }\n";
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

//
// HTML Handler Functions
//

// initial page
void handle_root() {
  debugOut( "/" );
  if ( captiveHandler() ) { // If captive portal redirect instead of displaying the root page.
    return;
  }
  String result = "";
  // create current checksum
  String checksumString = sha1( String( ssid ) + String( password ) + String( dnsname ) + String( place ) 
      + String( (int) authentication ) + String( authuser )  + String( authpass )
      + String( mcpchannels ) + String( (int) silent )  + String( (int) gcsensor ) + String( (int) mq135sensor ) 
      + String( (int) bme280sensor ) + String( (int) ds18b20sensor ) + String( (int) mcp3008sensor ) + String( (int) debug )  
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
  result += "<p>* Clear EEPROM, will reboot in Setup (Wifi Acces Point) Mode named <strong>esp_setup</strong>!</p>";
  result += "<h2>Enabled Sensors</h2>\n";
  result += "<table>\n";
  if( gcsensor ) {
    result += "  <tr><td>Geigercounter</td><td>MightyOhm Geigercounter</td><td>(RX " + String( gcrx ) + ", TX " + String( gctx ) + ")</td></tr>\n";
  }
  if( mq135sensor ) {
    result += "  <tr><td>MQ135</td><td>Air Quality Sensor</td><td>(Pin " + String( mqpin ) + ")</td></tr>\n";
  }
  if( ds18b20sensor ) {
    result += "  <tr><td>DS18B20</td><td>Temperature Sensor</td><td>(Pin " + String( ONE_WIRE_BUS ) + ")</td></tr>\n";
  }
  if( bme280sensor ) {
    result += "  <tr><td>BME280</td><td>Air Temperature, Humidity and Pressure Sensor</td><td>(Adress " + String( BME280_ADDRESS ) + ")</td></tr>\n";
  }
  if( mcp3008sensor ) {
    result += "  <tr><td>MCP3008</td><td>MCP3008 Analoge Digital Converter</td><td>(Channels " + String( mcpchannels ) + ")</td></tr>\n";
  }
  result += "  <tr><td></td><td></td></tr>\n"; // space line
  result += "  <tr><td>DNS Search Domain: </td><td>" + dnssearch + "</td></tr>\n";
  if( silent ) {
    result += "  <tr><td>Silent Mode</td><td>Enabled (LED disabled)</td></tr>\n";
  } else {
    result += "  <tr><td>Silent Mode</td><td>Disabled (LED blink on Request)</td></tr>\n";
  }
  if( debug ) {
    result += "  <tr><td>Debug Mode</td><td>Enabled";
    if( gcsensor ) {
      result += " (Warning: Geigercounter is disabled in Debug mode!)";
    } else {
      result += " (Read Serial: \"screen /dev/cu.wchusbserial1420 115200\")";
    }
    result += "</td></tr>\n";
  } else {
    result += "  <tr><td>Debug Mode</td><td>Disabled</td></tr>\n";
  }
  if( strcmp( checksum, lastcheck ) == 0 ) { // compare checksums
      result += "<tr><td>Checksum</td><td><span style='color:#66ff66;'>&#10004;</span></td></tr>\n";
    } else {
      result += "<tr><td>Checksum</td><td><span style='color:#ff6666;'>&#10008;</span> Invalid, plaese <a href='/setup'>Setup</a> your Device.</td></tr>\n";
    }
  result += "<table>\n";
  response( htmlBody( result ), "text/html");
}

// redirect client to captive portal after connect to wifi
boolean captiveHandler() {
  if( SoftAccOK && ! isIp( server.hostHeader() ) && server.hostHeader() != ( String( dnsname ) + "." + dnssearch ) ) { // && server.hostHeader() != ( String( myHostname ) + ".local" )
    String redirectUrl = String("http://") + ip2Str( server.client().localIP() ) + String( "/setup" );
    server.sendHeader("Location", redirectUrl, false );
    server.send( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    if( debug ) { // enrich Debug output
      Serial.print( "- Redirect from captureHanlder to uri: " );
      Serial.println( redirectUrl );
    }
    server.client().stop(); // Stop is needed because we sent no content length
    captiveCall = true;
    return true;
  }
  captiveCall = false;
  return false;
}

// 404 Handler
void notFoundHandler() {
  debugOut( "404" );
  if ( captiveHandler() ) { // If captive portal redirect instead of displaying the error page.
    return;
  }
  String result = "";
  result += "<div>";
  result += "  <p>404 Page not found.</p>";
  result += "  <p>Where sorry, but are unable to find page: <strong>" + String( server.uri() ) + "</strong></p>";
  result += "</div>";
  server.send(404, "text/html", htmlBody( result ) );
}

// Dummy favicon Handler
void faviconHandler() {
  //debugOut( "Favicon" );
  response( "", "image/ico");
}

// Signal Handler
void signalHandler() {
  //debugOut( "Signal" );
  response( String( WiFi.RSSI() ), "text/plain");
}

// Setup Handler
void setupHandler() {
  if( ! SoftAccOK && authentication ) {
    if( ! server.authenticate( authuser, authpass ) ) {
      return server.requestAuthentication();
    }
  }
  String result = "";
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
  String silentChecked = silent ? "checked" : "";
  result += "  <tr>\n";
  result += "    <td><label for='silent'>Silent Mode: </label></td>\n";
  result += "    <td><input id='silent' name='silent' type='checkbox' " + silentChecked + "  /></td>\n";
  result += "  </tr>\n";
  String debugChecked = debug ? "checked" : "";
  result += "  <tr>\n";
  result += "    <td><label for='debug'>Debug Mode: </label></td>\n";
  result += "    <td><input id='debug' name='debug' type='checkbox' " + debugChecked + "  /></td>\n";
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
  debugOut( "Setup" );
}

// Form Handler
void formHandler() {
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
  // Place
  String placeString = server.arg("place");
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
  // Sytem stuff
  silent = server.arg("silent") == "on" ? true : false;
  debug = server.arg("debug") == "on" ? true : false;
  // Authentication stuff
  authentication = server.arg("authentication") == "on" ? true : false;
  String authuserString = server.arg("authuser");
  authuserString.toCharArray(authuser, eepromStringSize);
  String authpassString = server.arg("authpass");
  authpassString.toCharArray(authpass, eepromStringSize);
  // Sensor stuff
  String mcpchannelsString = server.arg("mcpchannels");
  mcpchannels = mcpchannelsString.toInt();
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
  debugOut( "Submit" );
}

// Restart Handler
void restartHandler() {
  debugOut( "Restart" );
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
}

// Reset Handler
void resetHandler() {
  debugOut( "Reset" );
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
}

// Metrics Handler
void metricsHandler() {
  debugOut( "Metrics" );
  String checksumString = sha1( String( ssid ) + String( password ) + String( dnsname ) + String( place ) 
    + String( (int) authentication ) + String( authuser )  + String( authpass )
    + String( mcpchannels ) + String( (int) silent )  + String( (int) gcsensor ) + String( (int) mq135sensor ) 
    + String( (int) bme280sensor ) + String( (int) ds18b20sensor ) + String( (int) mcp3008sensor ) + String( (int) debug ) 
    + String( (int) staticIP ) + ip2Str( ipaddr ) + ip2Str( gateway ) + ip2Str( subnet ) + ip2Str( dns1 ) + ip2Str( dns2 ) 
    );
  char checksum[ eepromchk ];
  checksumString.toCharArray(checksum, eepromchk); // write checksumString (Sting) into checksum (char Arr)
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
  webString += "# HELP esp_device_dhcp Network configured by DHCP\n";
  webString += "# TYPE esp_device_dhcp gauge\n";
  webString += "esp_device_dhcp ";
  webString += staticIP ? "0" : "1";
  webString += "\n";
  webString += "# HELP esp_device_silent Silent Mode enabled = 1 or disabled = 0\n";
  webString += "# TYPE esp_device_silent gauge\n";
  webString += "esp_device_silent ";
  webString += silent ? "1" : "0";
  webString += "\n";
  webString += "# HELP esp_device_debug Debug Mode enabled = 1 or disabled = 0\n";
  webString += "# TYPE esp_device_debug gauge\n";
  webString += "esp_device_debug ";
  webString += debug ? "1" : "0";
  webString += "\n";
  webString += "# HELP esp_device_eeprom_size Size of EEPROM in byte\n";
  webString += "# TYPE esp_device_eeprom_size gauge\n";
  webString += "esp_device_eeprom_size " + String( eepromSize ) + "\n";
  webString += "# HELP esp_device_config_size Size of EEPROM Configuration data in byte\n";
  webString += "# TYPE esp_device_config_size counter\n";
  webString += "esp_device_config_size " + String( configSize ) + "\n";
  webString += "# HELP esp_device_eeprom_free Size of available/free EEPROM in byte\n";
  webString += "# TYPE esp_device_eeprom_free gauge\n"; // 284
  webString += "esp_device_eeprom_free " + String( ( eepromSize - configSize ) ) + "\n";
  webString += "# HELP esp_device_uptime Uptime of the Device in Secondes \n";
  webString += "# TYPE esp_device_uptime counter\n";
  webString += "esp_device_uptime ";
  webString += millis() / 1000;
  webString += "\n";
  webString += "# HELP esp_mode_checksum Checksum validation: 1 = valid, 0 = invalid (need to run Setup again)\n";
  webString += "# TYPE esp_mode_checksum gauge\n";
  webString += "esp_mode_checksum ";
  if( strcmp( checksum, lastcheck ) == 0 ) {
    webString += "1";
  } else {
    webString += "0";
  }
  webString += "\n";
  // Voltage depending on what sensors are in use (Pin A0?)
  webString += "# HELP esp_voltage current Voltage metric\n";
  webString += "# TYPE esp_voltage gauge\n";
  if( mq135sensor ) {
    webString += "esp_voltage " + String( (float) ( ESP.getVcc() / 1024.0 / 10.0 ) - mqvoltage ) + "\n";
  } else {
    webString += "esp_voltage " + String( (float) ESP.getVcc() / 1024.0 / 10.0 ) + "\n"; // 1024.0 / 10.0
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
  bool loadingSetup = loadSettings(); 

  if( debug ) {
    Serial.begin(115200);
    Serial.println( "" );
    Serial.println( "esp8266 node startup" );
  }
  
  // start Wifi mode depending on Load Config (no valid config = start in AP Mode)
  if( loadingSetup ) {

    if( debug ) { Serial.println( "Starting WiFi Client Mode" ); }
    
    // use a static IP?
    if( staticIP ) {
      if( debug ) { Serial.print( "Using static IP: " ); Serial.println( ip ); }
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
    if( debug ) { Serial.print( "Connect to AP " + String( ssid ) +  ": " ); }
    WiFi.begin( ssid, password );
    while( WiFi.status() != WL_CONNECTED ) {
      if( !silent ) {
        digitalWrite(LED_BUILTIN, LOW);
        delay( 200 );
        digitalWrite(LED_BUILTIN, HIGH);
      }
      delay( 200 );
      if( debug ) { Serial.print( "." ); }
    }
    delay(200);
    if( debug ) { Serial.println( " ok" ); }
    // get the local ip adress
    ip = WiFi.localIP().toString();
    SoftAccOK = false;
    captiveCall = false;
    if( debug ) { Serial.print( "Local Device IP: " ); Serial.println( ip ); }

  } else {

    if( debug ) { Serial.print( "Starting in AP Mode: " ); }
    
    // Setup Wifi mode
    WiFi.mode( WIFI_AP );

    // set dnshostname
    WiFi.hostname( dnsname );
    
    // Setup AP Config
    IPAddress lip(192,168,4,1);
    IPAddress lgw(192,168,4,1);
    IPAddress lsub(255,255,255,0);
    WiFi.softAPConfig( lip, lgw, lsub );
    delay( 500 );
    
    // start in AP mode, without Password
    String appString = String( dnsname ) + "_setup";
    char apname[ eepromStringSize ];
    appString.toCharArray(apname, eepromStringSize);
    SoftAccOK = WiFi.softAP( apname, "" );
    delay( 500 );
    if( SoftAccOK ) {
//    while( WiFi.status() != WL_CONNECTED ) {
      digitalWrite(LED_BUILTIN, LOW);
      delay( 200 );
      digitalWrite(LED_BUILTIN, HIGH);
      delay( 400);
      digitalWrite(LED_BUILTIN, LOW);
      delay( 200 );
      digitalWrite(LED_BUILTIN, HIGH);
      delay( 400);
      digitalWrite(LED_BUILTIN, LOW);
      delay( 200 );
      digitalWrite(LED_BUILTIN, HIGH);
      if( debug ) { Serial.println( "ok" ); }
    } else {
      if( debug ) { Serial.println( "failed" ); }
    }
    delay(500);
    ip = WiFi.softAPIP().toString();
    
    /* Setup the DNS server redirecting all the domains to the apIP */
    // https://github.com/tzapu/WiFiManager
    dnsServer.setErrorReplyCode( DNSReplyCode::NoError );
    bool dnsStart = dnsServer.start( DNS_PORT, "*", WiFi.softAPIP() );
    if( debug ) {
      Serial.print( "starting dns server for captive Portal: " );
      if( dnsStart ) {
        Serial.println( "ok" );
      } else {
        Serial.println( "failed" );
      }
    }
    delay(500);
    //WiFi.softAPdisconnect(); // disconnect all previous clients
//    SoftAccOK = true;
//    captiveCall = false;
  }
  
  if( gcsensor ) {
    if( debug ) {
      Serial.println( "Geigercounter unable to initialise in debuge Mode!" );
    } else {
      // initialise serial connection to geigercounter
      mySerial.begin(9600); //Set up Software Serial Port
    }
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
  server.onNotFound( notFoundHandler );

  // default, main page
  server.on("/", handle_root );

  // ios captive portal detection request
  server.on("/hotspot-detect.html", captiveHandler );

  // android captive portal detection request
  server.on("/generate_204", captiveHandler );

  // dummy favicon.ico
  server.on("/favicon.ico", HTTP_GET, faviconHandler );

  // simple wifi signal strength
  server.on("/signal", HTTP_GET, signalHandler );

  // form page
  server.on("/setup", HTTP_GET, setupHandler );

  // form target 
  server.on("/form", HTTP_POST, formHandler );

  // restart page
  server.on("/restart", HTTP_GET, restartHandler );

  // reset page
  server.on("/reset", HTTP_GET, resetHandler );

  // metrics (simple prometheus ready metrics output)
  server.on( "/metrics", HTTP_GET, metricsHandler );

//  // Example Raw Response
//  server.on( "/url", HTTP_GET, []() {
//    response( htmlBody( result ), "text/html");
//  });

  // Starting the Weberver
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