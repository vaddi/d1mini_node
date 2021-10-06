/*
  Lolin/WEMOS D1 mini sensor node

  A Prometheus ready Node for scraping Temperature, Humidity and Air Quality

  Install this Libraries to your Arduino IDE:
  - Adafruit Sensor Library (search for "Adafruit Unified Sensor")
  - Adafruit BME280 Library (search for "bme280")
  - MQUnified Library (search for "mq135") 1.9.9
  - OneWire Library ( search for "onewire")
  - DallasTemperature Library (search for "dallas")

  You need to define the right paket sources to have the right boards under Tools!
  Arduino -> Settings -> Additional Boards Manager URLs: http://arduino.esp8266.com/stable/package_esp8266com_index.json

  #
  ## Pin Wiring (left = Sensor pins, right = D1 Mini pins)
  #

  BME280  | D1 Mini (i2c)
  -----------------
  VIN     | 5V
  GND     | GND
  SCL     | Pin 5/D1 *
  SDA     | Pin 4/D2 *
  * i2c

  MQ135   | D1 Mini (Analog in Pin)
  -----------------
  VIN     | 5V
  GND     | GND
  AO      | PIN A0/17 *
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
  Data    | PIN D3/4 *
  * Wiring a 4,7kΩ Resistor between VIN and Data!

  Geigercounter | D1 Mini (Serial)
  -----------------------
  6 green
  5 yellow      | 3/RX
  4 orange      | 1/TX
  3 red         | 3,3V
  2 brown
  1 black       | GND

  MCP3008 | D1 Mini (SPI)
  -------------------
  VIN     | 3,3V
  GND     | GND
  Clock   | D5/14
  Dout    | D6/12
  Din     | D7/13
  CS      | D8/15

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

  # Using curl to ensure auth functionality:
  curl -H "Authorization: Basic XXXXXXXXXX" -X GET http://esp00.local.ip/metrics  // only basic auth
  curl -X GET http://esp00.local.ip/metrics?apikey=XXXXX  // only token auth
  curl -H "Authorization: Basic XXXXXXXXXX" -X GET http://esp00.local.ip/metrics?apikey=XXXXX  // basic auth & apikey
  # to get the Authorization String, open a browser and in them a developer console. Now open the Device page and Login. 
  Under Networks you can see all requests. Click on the last request end take a look into the headers. 
  Search for "Authorization: Basic XXXX" and copy the whole String.
  
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
const byte gcrx     = 3;  // Geigerceounter RX Pin 
const byte gctx     = 1;  // Geigerceounter TX Pin
const byte mqpin    = A0; // MQ135 Sensor (Analog)
const byte mcppin   = D8; // MCP3008 ADC (D7,D6,D5 will also be used for SPI but doesnt neet do defined)
// DS18B20 Sensorpin & Precision
const byte onewire  = D3; // OneWire Pin for ds18b20 Sensors
const int  defpre   = 16;  // Maximum Precision

//
// Default settings
//

// setup your default wifi ssid, passwd and dns stuff. Can overwriten also later from setup website
const int eepromStringSize      = 24;     // maximal byte size for strings (ssid, dnsname, etc.)
const int eepromPassStringSize  = 64;     // special size for long wifi-password, http-password and http-token

char ssid[eepromStringSize]     = "";     // wifi SSID name
char password[eepromPassStringSize] = ""; // wifi wpa2 password
char dnsname[eepromStringSize]  = "esp";  // Uniq Networkname
char place[eepromStringSize]    = "";     // Place of the Device
int  port                       = 80;     // Webserver port (default 80)
bool silent                     = false;  // enable/disable silent mode
bool debug                      = true;   // enable/disable debug mode (should be enabled on first boot)
bool gcsensor                   = false;  // enable/disable mightyohm geigercounter (unplug to flash!)
bool mq135sensor                = false;  // enable/disable mq-sensor. They use Pin A0 and will disable the accurate vcc messuring
int  mqpre                      = 5;      // Precision for the MQ Sensor
bool bme280sensor               = false;  // enable/disable bme280 (on address 0x76). They use Pin 5 (SCL) and Pin 4 (SDA) for i2c
int  bmepre                     = 3;      // Precision for the BME Sensor
bool ds18b20sensor              = false;  // enable/disable ds18b20 temperatur sensors. They use Pin D3 as default for OneWire
const byte ds18pre              = 10;     // Define precision in bit: 9, 10, 11, 12. Will result in 9=0,5°C, 10=0,25°C, 11=0,125°C
                                          // * (keep in mind: higher values will remain in longer read times!)
bool mcp3008sensor              = false;  // enable/disable mcp3008 ADC. Used Pins Clock D5, MISO D6, MOSI D7, CS D8
int  mcpchannels                = 0;      // Amount of visible MCP 300X Channels, MCP3008 = max 8, MCP3004 = max 4
int  mcppre                     = 0;      // Precision for the MCP ADC Values
const char* history             = "4.3";  // Software Version
String      webString           = "";     // String to display
String      ip                  = "127.0.0.1";  // default ip, will overwriten
const int   eepromAddr          = 0;      // default eeprom Address
const int   eepromSize          = 512;    // default eeprom size in kB (see datasheet of your device)
const int   eepromchk           = 48;     // byte buffer for checksum
char        lastcheck[ eepromchk ] = "";  // last created checksum
int         configSize          = 0;      // Configuration Size in kB
String      currentRequest      = "";     // Value for the current request (used in debugOut)
unsigned long previousMillis    = 0;      // flashing led timer value
bool SoftAccOK                  = false;  // value for captvie portal function 
bool captiveCall                = false;  // value to ensure we have a captive request

// basic auth 
bool authentication             = false;  // enable/disable Basic Authentication
char authuser[eepromStringSize] = "";     // user name
char authpass[eepromPassStringSize] = "";     // user password

// token auth
bool tokenauth                  = false;  // enable/disable Tokenbased Authentication
char token[eepromPassStringSize]    = "";     // The token variable

// Secpush
bool secpush                    = true;   // enable/disable secpush (turn off auth after boot for n seconds)
int  secpushtime                = 300;    // default secpush time in Seconds
bool secpushstate               = false;  // current secpush state (false after event has trigger)

// Static IP
bool staticIP = false;                    // use Static IP
IPAddress ipaddr(192, 168, 1, 250);       // Device IP Adress
IPAddress gateway(192, 168, 1, 1);        // Gateway IP Adress
IPAddress subnet(255, 255, 255, 0);       // Network/Subnet Mask
IPAddress dns1(192, 168, 1, 1);           // First DNS
IPAddress dns2(4, 4, 8, 8);               // Second DNS
String dnssearch;                         // DNS Search Domain

// DNS server
const byte DNS_PORT = 53;                 // POrt of the DNS Server
DNSServer dnsServer;                      // DNS Server port

ESP8266WebServer server( port );          // Webserver port

//
// Edit careful after here
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
//String gcraw           = "";
String gcmode          = "";
int gcsize             = 0;
int gcerror            = 0;
int prc                = 0; // Parsing Counter
int src                = 0; // Serial Read Counter
//                         0           1                 2                3                    4                5
String errorcodes[]    = { "no error", "flipped values", "non numerical", "empty serial data", "serial in use", "serial not available" };
boolean isNumber( String tString ) {
  String tBuf;
  boolean decPt = false;
  if( tString.charAt( 0 ) == '+' || tString.charAt( 0 ) == '-' ) tBuf = &tString[1];
  else tBuf = tString; 
  for( int x = 0; x < tBuf.length(); x++ ) {
    if( tBuf.charAt( x ) == '.' ) {
      if( decPt ) return false;
      else decPt = true; 
    }   
    else if( tBuf.charAt( x ) < '0' || tBuf.charAt( x ) > '9' ) return false;
  }
  return true;
}
// read serial data
String readGCSerial() {
  //char dataBuffer[64];
  String data;
  if( mySerial.available() ) {
    delay(3); // time to fill buffer!
    gcerror = 0; // clear serial error
    return mySerial.readStringUntil('\n');
  } else {
    gcerror = 5; // serial not available
  }
  return "";
}
// realy bad serial data parser
void parseGCData() {
  String data = "";
  src = 0;
  if( debug ) { // ensure, serial is not in use by debug
    gcerror = 4; // serial in use
  } else {
    while( data == "" ) {
      src += 1;
      if( src > 2000 ) { // max read tries
        gcerror = 3; // empty serial data
        break; 
      }
      data = readGCSerial();
    }
  }
  if( data != "" ) {
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
//    gcraw = data; // can contains non utf-8 characters, so use only for debug
  }
}
void readGC() {
  prc = 0;
  parseGCData();
  while( ! isNumber( cps ) || ! isNumber( cpm ) || ! isNumber( uSvh ) ) {
    if( prc > 100 ) break;
    parseGCData();
    prc += 1;
  }
  // validate the final result, we only want numeric values
  if( isNumber( cps ) || isNumber( cpm ) || isNumber( uSvh ) ) {
    float tmpUSvh = uSvh.toFloat();
    float tmpCpm = cpm.toFloat();
    if( tmpUSvh > tmpCpm ) {
      gcerror = 1; // flipped numbers
      String tmpVar = cpm;
      cpm = uSvh;
      uSvh = tmpVar;
    } else {
      gcerror = 0; // reset error
    }
  } else {
    gcerror = 2; // non numerical values
    cps = "0.0";
    cpm = "0.0";
    uSvh = "0.0";
  }
}
// Get formatet Geigercounterdata function
String getGCData() {
  unsigned long gccurrentMillis = millis();
  readGC();
  unsigned long GCRuntime = millis() - gccurrentMillis;
  String result = "";
  result += "# HELP esp_mogc_info Geigercounter current read mode\n";
  result += "# TYPE esp_mogc_info gauge\n";
  result += "esp_mogc_info{";
  result += "mode=\"" + gcmode + "\"";
//  // can contain non utf-8 characters, should only used for debug!  
//  gcraw = gcraw.substring( 0, gcraw.length() -1 );
//  result += ", raw=\"" + String( gcraw ) + "\""; 
  result += "} 1\n";
  result += "# HELP esp_mogc_error Geigercounter error code\n";
  result += "# TYPE esp_mogc_error gauge\n";
  result += "esp_mogc_error{";
  result += "errorString=\"" + errorcodes[gcerror] + "\"";
  result += "} " + String( gcerror ) + "\n";
  result += "# HELP esp_mogc_size String length of raw serial data\n";
  result += "# TYPE esp_mogc_size gauge\n";
  result += "esp_mogc_size " + String( gcsize ) + "\n";
  result += "# HELP esp_mogc_cps Geigercounter Counts per Second\n";
  result += "# TYPE esp_mogc_cps gauge\n";
  result += "esp_mogc_cps " + cps + "\n";
  result += "# HELP esp_mogc_cpm Geigercounter Counts per Minute\n";
  result += "# TYPE esp_mogc_cpm gauge\n";
  result += "esp_mogc_cpm " + cpm + "\n";
  result += "# HELP esp_mogc_usvh Geigercounter Micro Sivert per Hour\n";
  result += "# TYPE esp_mogc_usvh gauge\n";
  result += "esp_mogc_usvh " + uSvh + "\n";
  result += "# HELP esp_mogc_src Geigercounter Serial read counts. Amount of reads until data is not empty\n";
  result += "# TYPE esp_mogc_src gauge\n";
  result += "esp_mogc_src " + String( src ) + "\n";
  result += "# HELP esp_mogc_prc Geigercounter Parsing counts. Amount of parsing runs until data is numerical\n";
  result += "# TYPE esp_mogc_prc gauge\n";
  result += "esp_mogc_prc " + String( prc ) + "\n";
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
OneWire oneWire(onewire);
// Declare ds18b20 Sensor on OneWire Bus
DallasTemperature DS18B20(&oneWire);
DeviceAddress tempAdd[0];                     // DS18B20 Address Array
int           tempCount       = 0;            // DS18b20 Arraysize value
String getDS18B20Data() {
  unsigned long dscurrentMillis = millis();
  DS18B20.requestTemperatures();
  unsigned long DS18B20Runtime = millis() - dscurrentMillis;
  String result = "";
  result += "# HELP esp_ds18b20_amount Current active ds18b20 Sensors\n";
  result += "# TYPE esp_ds18b20_amount gauge\n";
  result += "esp_ds18b20_amount " + String( tempCount ) + "\n";
  result += "# HELP esp_ds18b20_parasite parasite status\n";
  result += "# TYPE esp_ds18b20_parasite gauge\n";
  result += "esp_ds18b20_parasite " + String( (int) DS18B20.isParasitePowerMode() ) + "\n";
  result += "# HELP esp_ds18b20_precision Precision of ds18b20 devices\n";
  result += "# TYPE esp_ds18b20_precision gauge\n";
  result += "esp_ds18b20_precision " + String( ds18pre ) + "\n";
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
float         temp[ defpre ]; // temperature value
float         pres[ defpre ]; // pressure value
float         alt[ defpre ];  // altitude calculated by sealevel pressure
float         hum[ defpre ];  // humidy value
void setBMEValues() {
  floatArr( temp, bme.readTemperature() );
  floatArr( pres, ( bme.readPressure() / 100.0F ) );
  floatArr( alt, bme.readAltitude( SEALEVELPRESSURE_HPA ) );
  floatArr( hum, bme.readHumidity() );
  //delay(2);
}
String getBME280Data() {
  unsigned long bmecurrentMillis = millis();
  setBMEValues();
  long BMERuntime = millis() - bmecurrentMillis;
  String result = "";
  result += "# HELP esp_bme_status bme280 onload status, 1 = starting ok, Sensor available after boot\n";
  result += "# TYPE esp_bme_status gauge\n";
  result += "esp_bme_status " + String( BMEstatus ) + "\n";
  result += "# HELP esp_bme_precision Precision of BME280 Sensor Data\n";
  result += "# TYPE esp_bme_precision gauge\n";
  result += "esp_bme_precision " + String( bmepre ) + "\n";
  result += "# HELP esp_bme_temperature Current BME280 Temperature\n";
  result += "# TYPE esp_bme_temperature gauge\n";
  result += "esp_bme_temperature{filled=\"" + String( findFirstZero( temp ) ) + "\",current=\"" + String( ( sizeof( temp ) / sizeof( byte ) ) ) + "\"} " + String( arrAvg( temp ) ) + "\n";
  printArr( temp );
  result += "# HELP esp_bme_humidity Current BME280 Humidity\n";
  result += "# TYPE esp_bme_humidity gauge\n";
  result += "esp_bme_humidity{filled=\"" + String( findFirstZero( hum ) ) + "\"} " + String( arrAvg( hum ) ) + "\n";
  result += "# HELP esp_bme_pressure Current bme280 pressure\n";
  result += "# TYPE esp_bme_pressure gauge\n";
  result += "esp_bme_pressure{filled=\"" + String( findFirstZero( pres ) ) + "\"} " + String( arrAvg( pres ) ) + "\n";
  result += "# HELP esp_bme_altitude Current bme280 altitude, calculated by sealevel pressure\n";
  result += "# TYPE esp_bme_altitude gauge\n";
  result += "esp_bme_altitude{filled=\"" + String( findFirstZero( alt ) ) + "\"} " + String( arrAvg( alt ) ) + "\n";
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
  digitalWrite(mcppin, LOW);
  SPI.transfer(0x01);
  uint8_t msb = SPI.transfer(0x80 + (channel << 4));
  uint8_t lsb = SPI.transfer(0x00);
  digitalWrite(mcppin, HIGH);
  return ((msb & 0x03) << 8) + lsb;
}
void readMCPData() {
  for( size_t i = 0; i < mcpchannels; ++i ) {
    // avg precision?
    uint16_t total = 0;
    for( size_t j = 0; j < mcppre; ++j ) {
      total += mcp3008_read( i );
    }
    channelData[ i ] = total / mcppre;
    //channelData[ i ] = mcp3008_read( i );
  }
}
String getMCPData() {
  unsigned long mcpcurrentMillis = millis();
  readMCPData();
  delay(4); // minimum read time in millis
  unsigned long MCPRuntime = millis() - mcpcurrentMillis;
  String result = "";
  result += "# HELP esp_mcp_info MCP3008 Information\n";
  result += "# TYPE esp_mcp_info gauge\n";
  result += "esp_mcp_info{";
  result += "channels=\"" + String( mcpchannels ) + "\"";
  //result += ",precission=\"" + String( mcppre ) + "\"";
  result += "} 1\n";
  result += "# HELP esp_mpc_precision Precision of MCP 300X Data\n";
  result += "# TYPE esp_mpc_precision gauge\n";
  result += "esp_mpc_precision " + String( mcppre ) + "\n";
  result += "# HELP esp_mcp_data MCP Data\n";
  result += "# TYPE esp_mcp_data gauge\n";
  if( mcpchannels > 0 ) {
    for( size_t i = 0; i < mcpchannels; ++i ) {
      //result += "esp_mcp_data{device=\"" + String( i ) + "\",precission=\"" + String( mcppre ) + "\",filled=\"" + String( findFirstZero( temp ) ) + "\"} " + String( channelData[ i ] ) + "\n";
      result += "esp_mcp_data{device=\"" + String( i ) + "\"} " + String( channelData[ i ] ) + "\n";
      //result += "esp_mcp_data{device=\"" + String( i ) + "\"} " + String( channelData[ i ] ) + "\n";
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
#define placa "Arduino UNO"
#define Voltage_Resolution 5
//#define pin A0                  //Analog input 0 of your arduino
#define type "MQ-2"           // Type - MQ135/MQ-2
#define ADC_Bit_Resolution 10   // For arduino UNO/MEGA/NANO
#define RatioMQ2CleanAir 9.83  //RS / R0 = 3.6 ppm/9.83
float         co[ defpre ];     // CO value
//float         alc[ defpre ];    // Alcohol value
//float         co2[ defpre ];    // CO2 value
//float         dmh[ defpre ];    // Diphenylmethane value
//float         am[ defpre ];     // Ammonium value
//float         ac[ defpre ];     // Acetone value
float         mqvoltage = 0.0;  // MQ Voltage value
float         mqr       = 0.0;  // MQ R0 Resistor value
//Declare Sensor
MQUnifiedsensor MQ2(placa, Voltage_Resolution, ADC_Bit_Resolution, mqpin, type);
void setMQValues() {
//  for( size_t j = 0; j < mqpre; ++j ) {
//    MQ135setRegressionMethod.update(); // Update data, the arduino will be read the voltage from the analog pin
    MQ2.setRegressionMethod(1);
    //mqvoltage = ( (float) MQ135.getVoltage(false) * 10 ) - ( (float) ( ESP.getVcc() / 1024.0 / 10.0 ) );
    //mqvoltage = (float) MQ135.getVoltage(false) * 10.0;
    mqvoltage = (float) MQ2.getVoltage(false);
    mqr = MQ2.getR0(); // R0 Resistor value (indicator for calibration)
    // MQ-2
    MQ2.setA(574.25); MQ2.setB(-2.222);
    MQ2.setR0(9.659574468);
    MQ2.update();
    floatArr( co, MQ2.readSensor() );
    // MQ-135
//    MQ135.setA(605.18); MQ135.setB(-3.937); // Configurate the ecuation values to get CO concentration
//    floatArr( co, MQ135.readSensor() );
//    //co  = MQ135.readSensor(); // CO concentration
//    MQ135.setA(77.255); MQ135.setB(-3.18); // Configurate the ecuation values to get Alcohol concentration
//    floatArr( alc, MQ135.readSensor() );
//    //alc = MQ135.readSensor(); // Alcohole concentration
//    MQ135.setA(110.47); MQ135.setB(-2.862); // Configurate the ecuation values to get CO2 concentration
//    floatArr( co2, MQ135.readSensor() );
//    //co2 = MQ135.readSensor(); // CO2 concentration
//    MQ135.setA(44.947); MQ135.setB(-3.445); // Configurate the ecuation values to get Tolueno concentration
//    floatArr( dmh, MQ135.readSensor() );
//    //dmh = MQ135.readSensor(); // Diphenylmethane concentration
//    MQ135.setA(102.2 ); MQ135.setB(-2.473); // Configurate the ecuation values to get NH4 concentration
//    floatArr( am, MQ135.readSensor() );
//    //am  = MQ135.readSensor(); // NH4 (Ammonium) concentration
//    MQ135.setA(34.668); MQ135.setB(-3.369); // Configurate the ecuation values to get Acetona concentration
//    floatArr( ac, MQ135.readSensor() );
    //ac  = MQ135.readSensor(); // Acetone concentration
//    if( isnan( mqvoltage ) ) mqvoltage = 0.0;
//    if( isnan( mqr ) ) mqr = 0.0;
//    if( isnan( co ) ) co = 0.0;
//    if( isnan( alc ) ) alc = 0.0;
//    if( isnan( co2 ) ) co2 = 0.0;
//    if( isnan( dmh ) ) dmh = 0.0;
//    if( isnan( am ) ) am = 0.0;
//    if( isnan( ac ) ) ac = 0.0;
//  }
  //delay(2);
}
String getMQ135Data() {
  unsigned long mqcurrentMillis = millis();
  setMQValues();
  long MQRuntime = millis() - mqcurrentMillis;
  String result = "";
  //result += "{precission=\"" + String( co ) + "\"} " + String( findFirstZero( co ) ) + "\n";
  result += "# HELP esp_mq_voltage current Voltage metric\n";
  result += "# TYPE esp_mq_voltage gauge\n";
  result += "esp_mq_voltage " + String( mqvoltage ) + "\n";
  result += "# HELP esp_mq_resistor current R0 Resistor metric\n";
  result += "# TYPE esp_mq_resistor gauge\n";
  result += "esp_mq_resistor " + String( mqr ) + "\n";
  result += "# HELP esp_mq_precision MQ Precision\n";
  result += "# TYPE esp_mq_precision gauge\n";
  result += "esp_mq_precision " + String( mqpre ) + "\n";
  result += "# HELP esp_mq_co Current MQ-135 CO Level\n";
  result += "# TYPE esp_mq_co gauge\n";
  // {precission=\"" + String( co ) + "\"} " + String( findFirstZero( co ) ) + "
  result += "esp_mq_co{filled=\"" + String( findFirstZero( co ) ) + "\"} " + String( arrAvg( co ) ) + "\n";
//  result += "# HELP esp_mq_alc Current MQ-135 Alcohol Level\n";
//  result += "# TYPE esp_mq_alc gauge\n";
//  result += "esp_mq_alc{filled=\"" + String( findFirstZero( alc ) ) + "\"} " + String( arrAvg( alc ) ) + "\n";
//  result += "# HELP esp_mq_co2 Current MQ-135 CO2 Level\n";
//  result += "# TYPE esp_mq_co2 gauge\n";
//  result += "esp_mq_co2{filled=\"" + String( findFirstZero( co2 ) ) + "\"} " + String( arrAvg( co2 ) ) + "\n";
//  result += "# HELP esp_mq_dmh Current MQ-135 Diphenylmethane Level\n";
//  result += "# TYPE esp_mq_dmh gauge\n";
//  result += "esp_mq_dmh{filled=\"" + String( findFirstZero( dmh ) ) + "\"} " + String( arrAvg( dmh ) ) + "\n";
//  result += "# HELP esp_mq_am Current MQ-135 Ammonium Level\n";
//  result += "# TYPE esp_mq_am gauge\n";
//  result += "esp_mq_am{filled=\"" + String( findFirstZero( am ) ) + "\"} " + String( arrAvg( am ) ) + "\n";
//  result += "# HELP esp_mq_ac Current MQ-135 Acetone Level\n";
//  result += "# TYPE esp_mq_ac gauge\n";
//  result += "esp_mq_ac{filled=\"" + String( findFirstZero( ac ) ) + "\"} " + String( arrAvg( ac ) ) + "\n";
  result += "# HELP esp_mq_runtime Time in milliseconds to read Date from the MQ Sensor\n";
  result += "# TYPE esp_mq_runtime gauge\n";
  result += "esp_mq_runtime " + String( MQRuntime ) + "\n";
  return result;
}


//// DEPRECATED, just use an mcp3008 and leave A0 as default for mq135 Sensor
//// Capacitive Soil Moisture Sensore v1.0
////
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



//
// Helper Functions
//

// http response handler
void response( String& data, String requesttype ) { // , String addHeaders 
  if( ! silent ) digitalWrite( LED_BUILTIN, LOW );
  // get the pure dns name from requests
  if( dnssearch.length() == 0 ) { // only if not in AP Mode  ! SoftAccOK &&
    if( ! isIp( server.hostHeader() ) ) {
      String fqdn = server.hostHeader(); // esp22.speedport.ip
      int hostnameLength = String( dnsname ).length() + 1; // 
      fqdn = fqdn.substring( hostnameLength );
      String tmpPort = ":" + String( port );
      fqdn.replace( tmpPort, "" );
      dnssearch = fqdn;
    }
  }
//  if( addHeaders != null || addHeaders != "" ) {
//    server.sendHeader("Referrer-Policy","origin"); // Set aditional header data
//    server.sendHeader("Access-Control-Allow-Origin", "*"); // allow 
//  }
  server.send( 200, requesttype, data );
  delay(300);
  if( ! silent ) digitalWrite( LED_BUILTIN, HIGH );
}

// debug output to serial
void debugOut( String caller ) {
//  String callerUpper = caller;
//  callerUpper.toUpperCase();
//  char callerFirst = callerUpper.charAt(0);
//  caller = String( callerFirst ) + caller.substring(1,-1);
//  Serial.println( caller );
  if( debug ) {
    Serial.println( " " ); // Add newline
    Serial.print( "* " );
    Serial.print( server.client().remoteIP() );
    Serial.print( " requesting " );
    Serial.println( server.uri() );
    if( SoftAccOK ) {
      Serial.print( "  Connected Wifi Clients: " );
      Serial.println( (int) WiFi.softAPgetStationNum() );
    }
  }
}


// load config from eeprom
bool loadSettings() {
  if( debug ) Serial.println( "- Functioncall loadSettings()" );
  // define local data structure
  struct { 
    char eepromssid[ eepromStringSize ];
    char eeprompassword[ eepromPassStringSize ];
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
    char eepromauthpass[eepromPassStringSize];
    bool eepromtokenauth = false;
    char eepromtoken[eepromPassStringSize];
    bool eepromsecpush        = false;
    int  eepromsecpushtime    = 0;
    int  eeprommcpchannels    = 0;
    bool eepromsilent         = false;
    bool eepromgcsensor       = false;
    bool eeprommq135sensor    = false;
    int  eeprommqpre          = 0;
    bool eeprombme280sensor   = false;
    int  eeprombmepre         = 0;
    bool eepromds18b20sensor  = false;
    bool eeprommcp3008sensor  = false;
    int  eeprommcppre         = 0;
    char eepromchecksum[ eepromchk ] = "";
  } data;
  // read data from eeprom
  EEPROM.get( eepromAddr, data );
  // validate checksum
  String checksumString = sha1( String( data.eepromssid ) + String( data.eeprompassword ) + String( data.eepromdnsname ) + String( data.eepromplace )
        + String( (int) data.eepromauthentication ) + String( data.eepromauthuser ) + String( data.eepromauthpass )
        + String( (int) data.eepromtokenauth ) + String( data.eepromtoken )
        + String( (int) data.eepromsecpush ) + String( data.eepromsecpushtime )
        + String( data.eeprommcpchannels ) + String( (int) data.eepromsilent ) + String( (int) data.eepromgcsensor ) + String( (int) data.eeprommq135sensor ) + String( data.eeprommqpre )
        + String( (int) data.eeprombme280sensor ) + String( data.eeprombmepre ) + String( (int) data.eepromds18b20sensor ) + String( (int) data.eeprommcp3008sensor ) + String( data.eeprommcppre ) + String( (int) data.eepromdebug )
        + String( (int) data.eepromstaticIP ) + ip2Str( data.eepromipaddr ) + ip2Str( data.eepromgateway ) + ip2Str( data.eepromsubnet ) + ip2Str( data.eepromdns1 ) + ip2Str( data.eepromdns2 )
        );
  char checksum[ eepromchk ];
  checksumString.toCharArray(checksum, eepromchk); // write checksumString into checksum
  if( strcmp( checksum, data.eepromchecksum ) == 0 ) { // compare with eeprom checksum
    if( debug ) Serial.println( "  passed checksum validation" );
    strncpy( lastcheck, checksum, eepromchk );
    configSize = sizeof( data );
    // re-set runtime variables;
    strncpy( ssid, data.eepromssid, eepromStringSize );
    strncpy( password, data.eeprompassword, eepromPassStringSize );
    strncpy( dnsname, data.eepromdnsname, eepromStringSize );
    strncpy( place, data.eepromplace, eepromStringSize );
    strncpy( checksum, data.eepromchecksum, eepromchk );
    staticIP = data.eepromstaticIP;
    debug = data.eepromdebug;
    ipaddr = data.eepromipaddr;
    gateway = data.eepromgateway;
    subnet = data.eepromsubnet;
    dns1 = data.eepromdns1;
    dns2 = data.eepromdns2;
    authentication = data.eepromauthentication;
    strncpy( authuser, data.eepromauthuser, eepromStringSize );
    strncpy( authpass, data.eepromauthpass, eepromPassStringSize );
    tokenauth = data.eepromtokenauth;
    strncpy( token, data.eepromtoken, eepromPassStringSize );
    secpush = data.eepromsecpush;
    secpushtime = data.eepromsecpushtime;
    mcpchannels = data.eeprommcpchannels;
    silent = data.eepromsilent;
    gcsensor = data.eepromgcsensor;
    mq135sensor = data.eeprommq135sensor;
    mqpre = data.eeprommqpre;
    bme280sensor = data.eeprombme280sensor;
    bmepre = data.eeprombmepre;
    ds18b20sensor = data.eepromds18b20sensor;
    mcp3008sensor = data.eeprommcp3008sensor;
    mcppre = data.eeprommcppre;
    return true;
  }
  if( debug ) Serial.println( "  failed checksum validation" );
  return false;
}

// write config to eeprom
bool saveSettings() {
  if( debug ) Serial.println( "- Functioncall saveSettings()" );
  // define local data structure
  struct { 
    char eepromssid[ eepromStringSize ];
    char eeprompassword[ eepromPassStringSize ];
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
    char eepromauthpass[eepromPassStringSize];
    bool eepromtokenauth = false;
    char eepromtoken[eepromPassStringSize];
    bool eepromsecpush        = false;
    int  eepromsecpushtime    = 0;
    int  eeprommcpchannels    = 0;
    bool eepromsilent         = false;
    bool eepromgcsensor       = false;
    bool eeprommq135sensor    = false;
    int  eeprommqpre          = 0;
    bool eeprombme280sensor   = false;
    int  eeprombmepre         = 0;
    bool eepromds18b20sensor  = false;
    bool eeprommcp3008sensor  = false;
    int  eeprommcppre         = 0;
    char eepromchecksum[ eepromchk ] = "";
  } data;
  // write real data into struct elements
  strncpy( data.eepromssid, ssid, eepromStringSize );
  strncpy( data.eeprompassword, password, eepromPassStringSize );
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
  strncpy( data.eepromauthpass, authpass, eepromPassStringSize );
  data.eepromtokenauth = tokenauth;
  strncpy( data.eepromtoken, token, eepromPassStringSize );
  data.eepromsecpush = secpush;
  data.eepromsecpushtime = secpushtime;
  data.eeprommcpchannels = mcpchannels;
  data.eepromsilent = silent;
  data.eepromgcsensor = gcsensor;
  data.eeprommq135sensor = mq135sensor;
  data.eeprommqpre = mqpre;
  data.eeprombme280sensor = bme280sensor;
  data.eeprombmepre = bmepre;
  data.eepromds18b20sensor = ds18b20sensor;
  data.eeprommcp3008sensor = mcp3008sensor;
  data.eeprommcppre = mcppre;
  // create new checksum
  String checksumString = sha1( String( data.eepromssid ) + String( data.eeprompassword ) + String( data.eepromdnsname ) + String( data.eepromplace )
        + String( (int) data.eepromauthentication ) + String( data.eepromauthuser )  + String( data.eepromauthpass )
        + String( (int) data.eepromtokenauth ) + String( data.eepromtoken )
        + String( (int) data.eepromsecpush ) + String( data.eepromsecpushtime )
        + String( data.eeprommcpchannels ) + String( (int) data.eepromsilent )  + String( (int) data.eepromgcsensor ) + String( (int) data.eeprommq135sensor ) + String( data.eeprommqpre )
        + String( (int) data.eeprombme280sensor ) + String( data.eeprombmepre ) + String( (int) data.eepromds18b20sensor ) + String( (int) data.eeprommcp3008sensor ) + String( data.eeprommcppre ) + String( (int) data.eepromdebug )
        + String( (int) data.eepromstaticIP ) + ip2Str( data.eepromipaddr ) + ip2Str( data.eepromgateway ) + ip2Str( data.eepromsubnet ) + ip2Str( data.eepromdns1 ) + ip2Str( data.eepromdns2 )
        );
  char checksum[ eepromchk ];
  checksumString.toCharArray(checksum, eepromchk);
  strncpy( data.eepromchecksum, checksum, eepromchk );
  strncpy( lastcheck, checksum, eepromchk );
  if( debug ) { Serial.print( "  create new config checksum: " ); Serial.println( checksum ); }
  configSize = sizeof( data );
  // save filed struct into eeprom
  EEPROM.put( eepromAddr,data );
  // commit transaction and return the write result state
  bool eepromCommit = EEPROM.commit();
  if( eepromCommit ) {
    if( debug ) Serial.println( "  successfully write config to eeprom" );
  } else {
    if( debug ) Serial.println( "  failed to write config to eeprom" );
  }
  return eepromCommit;
}

// add value to array, until all fields are filled
// then shift all values one step forward and add the given value to the end of the array
void floatArr( float *arrData, float currentValue ) {
  //int arrSize = ( sizeof( arrData ) / sizeof( float ) );
  int arrSize = sizeof( arrData );
  int firstZero = findFirstZero( arrData );
  float tmpArr[ arrSize ];
  initArr( tmpArr ); // fill tmpArr whith zero
  if( firstZero >= arrSize ) {
    if( arrData[ arrSize ] == 0 || arrData[ arrSize ] == 0.0 || arrData[ arrSize ] == 0.00 ) {
      // Array not realy complete filled, missing last
      for( size_t i = 0; i <= arrSize; ++i ) {
        tmpArr[ i ] = arrData[ i ];
      }
      // Replace last nuller in array by Value
      tmpArr[ firstZero ] = currentValue;
    } else {
      // Array is complete filled
      for( size_t i = 0; i <= arrSize; ++i ) {
        if( ( i + 1 ) <= arrSize ) {
          tmpArr[i] = arrData[i + 1];
        } else {
          tmpArr[i] = currentValue;
        }
      }
    }
  } else {
    // Array not complete filled
    for( size_t i = 0; i <= arrSize; ++i ) {
      tmpArr[ i ] = arrData[ i ];
    }
    // Replace last nuller in array by Value
    tmpArr[ firstZero ] = currentValue;
  }

  // write all to destination array
  for( size_t i = 0; i <= arrSize; ++i ) {
    arrData[ i ] = tmpArr[ i ];
  }
}

// get the fill amount of array by run forward (find first 0 value and return the position)
int findFirstZero( float *arrData ) {
  int arrSize = sizeof( arrData );
  for( size_t i = 0; i <= arrSize; ++i ) {
    if( ( arrData[ i ] == 0 || arrData[ i ] == 0.0 || arrData[ i ] == 0.00 ) && ( arrData[ i +1 ] == 0 || arrData[ i +1 ] == 0.0 || arrData[ i +1 ] == 0.00 ) ) return i;
  }
  return arrSize; // return possible maximun, the array size
}

// returns the average of array data
float arrAvg( float *arrData ) {
  float result = 0.00;
  int arrSize = sizeof( arrData );
  int firstZero = findFirstZero( arrData );
  //if( firstZero < arrSize ) {
  if( firstZero < arrSize ) {
    // get avg only until reached first zero in array
    for( size_t i = 0; i < firstZero; ++i ) {
      result += arrData[ i ];
    }
    return result / firstZero;
  } else {
    for( size_t i = 0; i < arrSize; ++i ) {
      result += arrData[ i ];
    }
    return result / arrSize;
  }
}

// get last value
float currentFromArr( float *arrData ) {
  float result = 0.00;
  int arrSize = sizeof( arrData );
  int firstZero = findFirstZero( arrData );
  if( firstZero == 0 ) {
    result = arrData[ 0 ];
  } else if( firstZero == arrSize ) {
    if( arrData[ arrSize ] == 0 || arrData[ arrSize ] == 0.0 || arrData[ arrSize ] == 0.00 ) {
      result = arrData[ arrSize -1 ];
    } else {
      result = arrData[ arrSize ];
    }
  } else {
    result = arrData[ firstZero -1 ];
  }
  return result;
}

// fill all array fields by 0.00
void initArr( float *arrData ) {
  int arrSize = sizeof( arrData );
  for( size_t i = 0; i <= arrSize; ++i ) {
    arrData[ i ] = 0.00;
  }
}

// serial print out a whole array
void printArr( float *arrData ) {
  if( debug ) {
    int arrSize = sizeof( arrData );
    for( size_t i = 0; i <= arrSize; ++i ) {
      Serial.print( i );
      Serial.print( " - " );
      Serial.println( arrData[ i ] );
    }
  }
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

// Disable Authenthification after booting for X seconds
bool securePush() {
  if( secpush && secpushstate ) {
    int tmpPushtime = ( secpushtime * 1000 );
    if( ( (int) secpushtime - ( ( millis() - secpushtime ) / 60 /60 ) ) <= 3 ) {
      secpushstate = false;
      return false;
    }
  }
  return true;
}


bool validToken( String requestToken ) {
  bool result = false;
  if( requestToken == token ) {
    result = true;
  }
  return result;
}

void authorisationHandler() {
  if( ! SoftAccOK ) { // disable validation in AP Mode
    if( ! secpushstate ) { // disable validation when Secpush state is active
      if( authentication || tokenauth ) { // only if authentifications are enabled
        bool authenticated = false;
        // basic auth (on all pages)
        if( authentication ) {
          bool authenticateUser = server.authenticate( authuser, authpass );
          if( ! authenticateUser ) {
            if( debug ) Serial.println( "  basic auth - request username & password" );
            return server.requestAuthentication();
          } else {
            if( debug ) Serial.println( "  basic auth - validation passed." );
            authenticated = true;
          }
        }
        // token auth (only on Special pages)
        if( currentRequest == "metrics" ) { // token auth only on sepecial pages || currentRequest == "restart" || currentRequest == "reset"
          if( tokenauth && ! authenticated ) { // disable, if user authenticated allready by basic auth
            if( server.hasHeader( "X-Api-Key" ) ) {
              String apikey = server.header( "X-Api-Key" );
              if( apikey == "" ) {
                if( debug ) Serial.println( "  tokenauth - empty X-Api-Key, validation failed. Send 401." );
                return server.send(401, "text/plain", "401: Unauthorized, empty X-Api-Key.");
              }
              if( debug ) { Serial.print( "  tokenauth - got X-Api-Key (header): " ); Serial.println( apikey ); }
              if( validToken( apikey ) ) {
                if( debug ) Serial.println( "  tokenauth - validation passed" );
                authenticated = true;
              } else {
                if( debug ) Serial.println( "  tokenauth - invalid X-Api-Key, validation failed" );
                return server.send(401, "text/plain", "401: Unauthorized, wrong X-Api-Key. Send 401.");
              }
            } else if( server.arg( "apikey" ) ) {
              String apikey = server.arg( "apikey" );
              if( apikey == "" ) {
                if( debug ) Serial.println( "  tokenauth - empty apikey, validation failed. Send 401." );
                return server.send(401, "text/plain", "401: Unauthorized, empty apikey.");
              }
              if( debug ) { Serial.print( "  tokenauth - got apikey (arg): " ); Serial.println( apikey ); }
              if( validToken( apikey ) ) {
                if( debug ) Serial.println( "  tokenauth - validation passed." );
                authenticated = true;
              } else {
                if( debug ) Serial.println( "  tokenauth - invalid apikey, validation failed. Send 401." );
                return server.send(401, "text/plain", "401: Unauthorized, wrong apikey.");
              }
            } else {
              if( debug ) Serial.println( "  tokenauth - apikey or  X-Api-Key missing in request. Send 401." );
              return server.send(401, "text/plain", "401: Unauthorized, X-Api-Key or apikey missing.");
            }
          }
        } else { // currentRequested page dont use token auth
          if( debug ) { Serial.print( "  tokenauth - disabled uri: " ); Serial.println( currentRequest ); }
        }
      } else {
        if( debug ) Serial.println( "  authorisation disabled" );
      }
    } else { // secpush
      if( debug ) Serial.println( "  authorisation disabled by SecPush" );
    }
  } else { // softACC
    if( debug ) Serial.println( "  authorisation disabled in AP Mode" );
  }
}


//
// html helper functions
//

// spinner javascript
String spinnerJS() {
  String result( ( char * ) 0 );
  result.reserve( 440 ); // reserve 440 characters of Space into result
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
  String result( ( char * ) 0 );
  result.reserve( 120 );
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
  String result( ( char * ) 0 );
  result.reserve( 2750 );
  //String result = "";
  // javascript to poll wifi signal
  if( ! captiveCall && currentRequest == "/" ) { // deactivate if in setup mode (Captive Portal enabled)
    result += "window.setInterval( function() {\n"; // polling event to get wifi signal strength
    result += "  let xmlHttp = new XMLHttpRequest();\n";
    result += "  xmlHttp.open( 'GET', '/signal', false );\n";
    result += "  xmlHttp.send( null );\n";
    result += "  let signalValue = xmlHttp.responseText;\n";
    result += "  let signal = document.getElementById('signal');\n";
    result += "  signal.innerText = signalValue + ' dBm';\n";
    result += "}, 5000 );\n";
  }
  if( currentRequest == "networksetup" || currentRequest == "devicesetup" || currentRequest == "authsetup" ) {
    //result += "let currentUrl = window.location.pathname;\n";
    //result += "if( currentUrl == '/device' || currentUrl == '/network' || currentUrl == '/auth' ) {\n"; // load this only on setup page!
    result += "  function renderRows( checkbox, classname ) {\n";
    result += "    if( checkbox === null || checkbox === undefined || checkbox === \"\" || classname === null || classname === undefined || classname === \"\" ) return;\n";
    result += "    let rows = document.getElementsByClassName( classname );\n";
    result += "    if( rows === null || rows === undefined || rows === \"\" ) return;\n";
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
    if( currentRequest == "devicesetup" ) {
      result += "  function renderGC() {\n"; // en/disable debug, depending on geigercounter en/disabled and vice versa
      result += "    gcsensorbox = document.getElementById('gcsensor');\n";
      result += "    debugbox = document.getElementById('debug');\n";
      result += "    if( gcsensorbox === null || gcsensorbox === undefined || debugbox === null || debugbox === undefined ) return;\n";
      result += "    if( gcsensorbox.checked ) {\n";
      result += "      debugbox.disabled = true;\n";
      result += "      debugbox.checked = false;\n";
      result += "    } else {\n";
      result += "      debugbox.disabled = false;\n";
      result += "    }\n";
      result += "    if( debugbox.checked ) {\n";
      result += "      gcsensorbox.disabled = true;\n";
      result += "      gcsensorbox.checked = false;\n";
      result += "    } else {\n";
      result += "      gcsensorbox.disabled = false;\n";
      result += "    }\n";
      result += "  }\n";
    }
    if( currentRequest == "authsetup" ) {
      result += "  function createToken( length ) {\n";
      result += "    var result           = '';\n";
      result += "    var characters       = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';\n";
      result += "    var charactersLength = characters.length;\n";
      result += "    for ( var i = 0; i < length; i++ ) {\n";
      result += "      result += characters.charAt(Math.floor(Math.random() * charactersLength));\n";
      result += "    }\n";
      result += "    return result;\n";
      result += "  }\n";
      result += "  function generateToken( tokenFildId ) {\n";
      result += "    let tokenField = document.getElementById( tokenFildId );\n";
      result += "    if( tokenField.value == null || tokenField.value == undefined ) return;\n";
      result += "    if( tokenField.value != \"\" ) {\n";
      result += "      let confirmed = window.confirm('Reset current Token?')\n";
      result += "      if( confirmed ) tokenField.value = createToken( 16 );\n";
      result += "    } else {\n";
      result += "      tokenField.value = createToken( 16 );\n";
      result += "    }\n";
      result += "  }\n";
      result += "  function toggleVisibility( pwFieldId ) {\n";
      result += "    let pwField = document.getElementById( pwFieldId );\n";
      result += "    if( pwField.type == 'text' ) {\n";
      result += "      pwField.type = 'password';\n";
      result += "    } else {\n";
      result += "      pwField.type = 'text';\n";
      result += "    }\n";
      result += "  }\n";
    }
    result += "  window.onload = function () {\n";
    if( currentRequest == "networksetup" ) result += "    renderRows( document.getElementById( 'staticIP'), \"staticIPRow\" );\n";
    if( currentRequest == "devicesetup" ) {
      result += "    renderRows( document.getElementById( 'mq135sensor'), \"mqPreRow\" );\n";
      result += "    renderRows( document.getElementById( 'bme280sensor'), \"bmePreRow\" );\n";
      result += "    renderRows( document.getElementById( 'mcp3008sensor'), \"mcpChannelsRow\" );\n";
      result += "    renderGC();\n";
    }
    if( currentRequest == "authsetup" ) {
      result += "    renderRows( document.getElementById( 'authentication'), \"authRow\" );\n";
      result += "    renderRows( document.getElementById( 'tokenauth'), \"tokenAuthRow\" );\n";
      result += "    renderRows( document.getElementById( 'secpush'), \"secPushRow\" );\n";
      result += "    renderRows( document.getElementById( 'https'), \"httpsRow\" );\n";
    }
    result += "  };\n";
    //result += "};\n";
  }
  return result;
}


// global css
String htmlCSS() {
  //String result = "";
  String result( ( char * ) 0 );
  result.reserve( 700 ); // reseve space for N characters
  result += "@charset 'UTF-8';\n";
  result += "#signalWrap { float: right; }\n";
  result += "#signal { transition: all 500ms ease-out; }\n";
  result += "#mcpChannelsRow, .staticIPRow, .authRow {\n";
  result += "  transition: all 500ms ease-out;\n";
  result += "  display: table-row;\n";
  result += "}\n";
  result += "#footer { text-align:center; }\n";
  result += "#footer .right { color: #ccc; float: right; margin-top: 0; }\n";
  result += "#footer .left { color: #ccc; float: left; margin-top: 0; }\n";
  result += "@media only screen and (max-device-width: 720px) {\n";
  result += "  .links { background: red; }\n";
  result += "  .links tr td { padding-bottom: 15px; }\n";
  result += "  h1 small:before { content: \"\\A\"; white-space: pre; }\n";
  result += "  #signalWrap { margin-top: 15px; }\n";
  result += "  #spinner_wrap { margin-top: 45%; }\n";
  result += "  #links a { background: #ddd; display: block; width: 100%; min-width: 300px; min-height: 40px; text-align: center; vertical-align: middle; padding-top: 20px; border-radius: 10px; }\n";
  result += "  #footer .left, #footer .right { float: none; }\n";
  result += "  #links tr td:nth-child(2n) { display: none; }\n";
  result += "}\n";
  return result;
}

// html head
String htmlHead() {
  String result( ( char * ) 0 );
  result.reserve( 5500 );
  result += "<head>\n";
  result += "  <title>" + String( dnsname ) + "</title>\n";
  result += "  <meta http-equiv='content-type' content='text/html; charset=UTF-8'>\n";
  result += "  <meta name='viewport' content='width=device-width, minimum-scale=1.0, maximum-scale=1.0'>\n";
  result += "  <script type='text/javascript'>\n" + htmlJS() + "\n</script>\n";
  result += "  <style>\n" + htmlCSS() + "\n</style>\n";
  result += "</head>\n";
  return result;
}

// html header
String htmlHeader() {
  String result( ( char * ) 0 );
  result.reserve( 170 );
  result += "<div id='signalWrap'>Signal Strength: <span id='signal'>" + String( WiFi.RSSI() ) + " dBm</span></div>\n"; 
  result += "<h1>" + String( dnsname ) + " <small style='color:#ccc'>a D1Mini Node Exporter</small></h1>\n";
  result += "<hr />\n";
  return result;
}

// html footer
String htmlFooter() {
  String result( ( char * ) 0 );
  result.reserve( 250 );
  result += "<hr style='margin-top:40px;' />\n";
  result += "<div id='footer'>\n";
  result += "  <p class='right'>";
  result += staticIP ? "Static" : "Dynamic";
  result += " IP: " + ip + "</p>\n";
  result += "  <p class='left'><strong>S</strong>imple<strong>ESP</strong> v" + String( history ) + "</p>\n";
  result += "  <p>source: <a href='https://github.com/vaddi/d1mini_node' target='_blank'>github.com</a></p>\n";
  result += "</div>\n";
  return result;
}

// html body (wrapping content)
String htmlBody( String& content ) {
  String result = "";
//  String result( ( char * ) 0 );
//  result.reserve( 10000 );
  result += "<!DOCTYPE html>\n";
  result += "<html lang='en'>\n";
  result += htmlHead();
  result += "<body>\n";
  result += htmlHeader();
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
  if ( captiveHandler() ) { // If captive portal redirect instead of displaying the root page.
    return;
  }
  currentRequest = "/";
  debugOut( currentRequest );
  authorisationHandler();
  String result( ( char * ) 0 );
  result.reserve( 2000 );
  // create current checksum
  String checksumString = sha1( String( ssid ) + String( password ) + String( dnsname ) + String( place )
      + String( (int) authentication ) + String( authuser ) + String( authpass )
      + String( (int) tokenauth ) + String( token )
      + String( (int) secpush ) + String( secpushtime )
      + String( mcpchannels ) + String( (int) silent ) + String( (int) gcsensor ) + String( (int) mq135sensor ) + String( mqpre )
      + String( (int) bme280sensor ) + String( bmepre ) + String( (int) ds18b20sensor ) + String( (int) mcp3008sensor ) + String( mcppre ) + String( (int) debug )
      + String( (int) staticIP ) + ip2Str( ipaddr ) + ip2Str( gateway ) + ip2Str( subnet ) + ip2Str( dns1 ) + ip2Str( dns2 )
      );
  char checksum[ eepromchk ];
  checksumString.toCharArray(checksum, eepromchk); // write checksumString (Sting) into checksum (char Arr)
  result += "<h2>About</h2>\n";
  result += "<p>A Prometheus scrape ready BME280/MQ135/DS18B20/Geigercounter Node Exporter.<br />\n";
  result += "Just add this Node to your prometheus.yml as Target to collect Air Pressure,<br />\n";
  result += "Air Humidity, Temperatures, Radiation and some Air Quality Data like Alcohole,<br />\n";
  result += "CO, CO2, Ammonium, Acetone and Diphenylmethane.</p>\n";
  result += "<h2>Links</h2>\n";
  result += "<table id='links'>\n";
  result += "  <tr><td><a href='/metrics";
  if( tokenauth ) { result += "?apikey=" + String( token ); }
  result += "'>/metrics</a></td><td></td></tr>\n";
  result += "  <tr><td><a href='/network'>/network</a></td><td>(Network Setup)</td></tr>\n";
  result += "  <tr><td><a href='/device'>/device</a></td><td>(Device Setup)</td></tr>\n";
  result += "  <tr><td><a href='/auth'>/auth</a></td><td>(Authentification Setup)</td></tr>\n";
  result += "  <tr><td><a href='/reset";
  if( tokenauth ) { result += "?apikey=" + String( token ); }
  result += "' onclick=\"return confirm('Reset the Device?');\">/reset</a></td><td>Simple reset</td></tr>\n";
  result += "  <tr><td><a href='/restart";
  if( tokenauth ) { result += "?apikey=" + String( token ); }
  result += "' onclick=\"return confirm('Restart the Device?');\">/restart</a></td><td>Simple restart</td></tr>\n";
  result += "  <tr><td><a href='/restart";
  if( tokenauth ) { result += "?apikey=" + String( token ) + "&"; } 
    else { result += "?"; }
  result += "reset=1' onclick=\"return confirm('Reset the Device to Factory Defaults?');\">/restart?reset=1</a></td><td>Factory-Reset*</td></tr>\n";
  result += "</table>\n";
  result += "<p>* Clear EEPROM, will reboot in Setup (Wifi Acces Point) Mode named <strong>esp_setup</strong>!</p>";
  result += "<h2>Enabled Sensors</h2>\n";
  result += "<table>\n";
  if( gcsensor ) {
    result += "  <tr><td>Geigercounter</td><td>MightyOhm Geigercounter</td><td>(RX " + String( (int) gcrx, HEX ) + ", TX " + String( (int) gctx, HEX ) + ")</td></tr>\n";
  }
  if( mq135sensor ) {
    result += "  <tr><td>MQ135</td><td>Air Quality Sensor</td><td>(Pin " + String( mqpin ) + ", Precission " + String( mqpre ) + ")</td></tr>\n";
  }
  if( ds18b20sensor ) {
    result += "  <tr><td>DS18B20</td><td>Temperature Sensor</td><td>(Pin " + String( onewire ) + ", Precission " + String( ds18pre ) + ")</td></tr>\n";
  }
  if( bme280sensor ) {
    //char pinBuffer[7]; // buffer for hex pinsInf
    //sprintf(pinBuffer, "%02x", ( BME280_ADDRESS ) );
    result += "  <tr><td>BME280</td><td>Air Temperature, Humidity and Pressure Sensor</td><td>(Adress " + String( BME280_ADDRESS ) + ", Precission " + String( bmepre ) + ")</td></tr>\n";
  }
  if( mcp3008sensor ) {
    result += "  <tr><td>MCP3008</td><td>MCP3008 Analoge Digital Converter</td><td>Pin " + String( mqpin ) + " (Channels " + String( mcpchannels ) + ", Precission " + String( mcppre ) + ")</td></tr>\n";
  }
  result += "</table>\n";
  result += "<h2>Setup Overview</h2>\n";
  result += "<table>\n";
  result += "  <tr><td>DNS Search Domain: </td><td>" + dnssearch + "</td><td></td></tr>\n";
  // Silent
  if( silent ) {
    result += "  <tr><td>Silent Mode</td><td>Enabled</td><td>(LED disabled)</td></tr>\n";
  } else {
    result += "  <tr><td>Silent Mode</td><td>Disabled</td><td>(LED blink on Request)</td></tr>\n";
  }
  // Debug
  if( debug ) {
    result += "  <tr><td>Debug Mode</td><td>Enabled</td><td>";
    if( gcsensor ) {
      result += " (Warning: Geigercounter is disabled in Debug mode!)";
    } else {
      result += " (Read Serial: \"screen /dev/cu.wchusbserial1420 115200\")";
    }
    result += "</td></tr>\n";
  } else {
    result += "  <tr><td>Debug Mode</td><td>Disabled</td><td></td></tr>\n";
  }
  // Authentification
  if( authentication ) {
    result += "  <tr><td>Basic Auth</td><td>Enabled</td><td></td></tr>\n"; // &#128274;
  } else {
    result += "  <tr><td>Basic Auth</td><td>Disabled</td><td></td></tr>\n"; // &#128275;
  }
  if( tokenauth ) {
    if( authentication ) {
      result += "  <tr><td>Token Auth</td><td>Enabled</td><td>Token: " + String( token ) + "</td></tr>\n"; // &#128273;
    } else {
      result += "  <tr><td>Token Auth</td><td>Enabled</td><td></td></tr>\n"; // &#128273;
    }
  }
  // SecPush
  if( secpush ) {
    result += "  <tr><td>SecPush</td><td>Enabled</td>";
    if( secpushstate ) {
      String currentSecPush = String( ( ( millis() - secpushtime ) / 60 /60 ) );
      currentSecPush += "/" + String( ( (int) secpushtime - ( ( millis() - secpushtime ) / 60 /60 ) ) );
      result += "<td>active (" + currentSecPush + " Seconcs, depart/remain)</td>";      
    } else {
      result += "<td>inactive</td>";
    }
    result += "</tr>\n";
  } else {
    result += "  <tr><td>SecPush</td><td>Disabled</td><td>";
    result += ( secpushstate ) ? "active" : "inactive";
    result += "</td></tr>\n";
  }
  // Checksum
  if( strcmp( checksum, lastcheck ) == 0 ) { // compare checksums
    result += "<tr><td>Checksum</td><td>Valid</td><td><span style='color:#66ff66;'>&#10004;</span></td></tr>\n";
  } else {
    result += "<tr><td>Checksum</td><td>Invalid</td><td><span style='color:#ff6666;'>&#10008;</span> Plaese <a href='/network'>Setup</a> your Device.</td></tr>\n";
  }
  result += "</table>\n";
  result =  htmlBody( result );
  response( result, "text/html" );
}

// redirect client to captive portal after connect to wifi
boolean captiveHandler() {
  if( SoftAccOK && ! isIp( server.hostHeader() ) && server.hostHeader() != ( String( dnsname ) + "." + dnssearch ) ) {
    String redirectUrl = String("http://") + ip2Str( server.client().localIP() ) + String( "/network" );
    server.sendHeader("Location", redirectUrl, false );
    server.send( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    if( debug ) {
      Serial.print( "- Redirect from captiveHandler() to uri: " );
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
  if ( captiveHandler() ) { // If captive portal redirect instead of displaying the error page.
    return;
  }
  currentRequest = "404";
  debugOut( currentRequest );
  String result = "";
  result += "<div>";
  result += "  <p>404 Page not found.</p>";
  result += "  <p>Where sorry, but are unable to find page: <strong>" + String( server.uri() ) + "</strong></p>";
  result += "</div>";
  server.send( 404, "text/html", htmlBody( result ) );
}

// Signal Handler
void signalHandler() {
  //debugOut( "WiFiSignal" );
  webString = String( WiFi.RSSI() );
  response( webString, "text/plain" );
}

// Network Setup Handler
void networkSetupHandler() {
  currentRequest = "networksetup";
  debugOut( currentRequest );
  authorisationHandler();
  char activeState[10];
  String result = "";
  result += "<h2>Network Setup</h2>\n";
  result += "<div>";
  if( SoftAccOK ) {
    result += "  <p>Welcome to the initial Setup. You should tell this device you wifi credentials";
    result += "  and give em a uniqe Name. If no static IP is setup, the device will request an IP from a DHCP Server.</p>";
  } else {
    result += "  <p>Wifi and IP settings. Change the Nodes Wifi credentials or configure a static ip. If no static IP is ";
    result += "  setup, the device will request an IP from a DHCP Server.</p>\n";
  }
  result += "</div>\n";
  result += "<div>";
  result += "<form action='/networkform' method='POST' id='networksetup'>\n";
  result += "<table style='border:none;'>\n";
  result += "<tbody>\n";
  result += "  <tr><td><strong>Wifi Settings</strong></td><td></td></tr>\n"; // head line
  result += "  <tr>\n";
  result += "    <td><label for='ssid'>SSID<span style='color:red'>*</span>: </label></td>\n";
  result += "    <td><input id='ssid' name='ssid' type='text' placeholder='Wifi SSID' value='" + String( ssid ) + "' size='" + String( eepromStringSize ) + "' required /></td>\n";
  result += "  </tr>\n";
  result += "  <tr>\n";
  result += "    <td><label for='password'>Password<span style='color:red'>*</span>: </label></td>\n";
  result += "    <td><input id='password' name='password' type='password' placeholder='Wifi Password' value='" + String( password ) + "' size='" + String( eepromPassStringSize ) + "' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr>\n";
  result += "    <td><label for='dnsname'>DNS Name<span style='color:red'>*</span>: </label></td>\n";
  result += "    <td><input id='dnsname' name='dnsname' type='text' placeholder='DNS Name' value='" + String( dnsname ) + "' size='" + String( eepromStringSize ) + "' required /></td>\n";
  result += "  </tr>\n";
  strcpy( activeState, ( staticIP ? "checked" : "" ) );
  result += "  <tr>\n";
  result += "    <td><label for='staticIP'>Static IP: </label></td>\n";
  result += "    <td><input id='staticIP' name='staticIP' type='checkbox' onclick='renderRows( this, \"staticIPRow\" )' " + String( activeState ) + " /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='staticIPRow'>\n";
  result += "    <td><label for='ipaddr'>IP: </label></td>\n";
  result += "    <td><input id='ipaddr' name='ipaddr' type='text' placeholder='192.168.1.2' value='" + ip2Str( ipaddr ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='staticIPRow'>\n";
  result += "    <td><label for='gateway'>Gateway: </label></td>\n";
  result += "    <td><input id='gateway' name='gateway' type='text' placeholder='192.168.1.1' value='" + ip2Str( gateway ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='staticIPRow'>\n";
  result += "    <td><label for='subnet'>Subnet: </label></td>\n";
  result += "    <td><input id='subnet' name='subnet' type='text' placeholder='255.255.255.0' value='" + ip2Str( subnet ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='staticIPRow'>\n";
  result += "    <td><label for='dns1'>DNS: </label></td>\n";
  result += "    <td><input id='dns1' name='dns1' type='text' placeholder='192.168.1.1' value='" + ip2Str( dns1 ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='staticIPRow'>\n";
  result += "    <td><label for='dns2'>DNS 2: </label></td>\n";
  result += "    <td><input id='dns2' name='dns2' type='text' placeholder='8.8.8.8' value='" + ip2Str( dns2 ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr><td>&nbsp;</td><td></td></tr>\n"; // dummy line
  result += "  <tr>\n";
  result += "    <td></td><td><button name='submit' type='submit'>Submit</button></td>\n";
  result += "  </tr>\n";
  result += "</tbody>\n";
  result += "</table>\n";
  result += "</form>\n";
  result += "</div>\n";
  result += "<div>\n";
  result += "  <p><span style='color:red'>*</span>&nbsp; Required fields!</p>\n";
  result += "</div>\n";
  result += "<a href='/'>Zur&uuml;ck</a>\n";
  result = htmlBody( result );
  response( result, "text/html" );
}


// Network Form Handler
void networkFormHandler() {
  currentRequest = "networkform";
  debugOut( currentRequest );
  authorisationHandler();
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
  passwordString.toCharArray(password, eepromPassStringSize);
  String dnsnameString = server.arg("dnsname");
  dnsnameString.toCharArray(dnsname, eepromStringSize);
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
  // save data into eeprom
  bool result = saveSettings();
  delay(50);
  // send user to restart page
  String location = "/restart";
  if( tokenauth ) { location += "?apikey=" + String( token ); }
  server.sendHeader( "Location",location );
  server.send( 303 );
}

// Device Setup Handler
void deviceSetupHandler() {
  currentRequest = "devicesetup";
  debugOut( currentRequest );
  authorisationHandler();
  char activeState[10];
  String result = "";
  result += "<h2>Device Setup</h2>\n";
  result += "<div>";
  result += "  <p>Here you can configure all device specific stuff like connected Sensors or just set a place description.</p>";
  result += "</div>\n";
  result += "<form action='/deviceform' method='POST' id='devicesetup'>\n";
  result += "<table style='border:none;'>\n";
  result += "<tbody>\n";
  result += "  <tr><td><strong>Device Settings</strong></td><td></td></tr>\n"; // head line
  result += "  <tr>\n";
  result += "    <td><label for='place'>Place: </label></td>\n";
  result += "    <td><input id='place' name='place' type='text' placeholder='Place Description' value='" + String( place ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
  result += "  </tr>\n";
  strcpy( activeState, ( silent ? "checked" : "" ) );
  result += "  <tr>\n";
  result += "    <td><label for='silent'>Silent Mode: </label></td>\n";
  result += "    <td><input id='silent' name='silent' type='checkbox' " + String( activeState ) + "  /></td>\n";
  result += "  </tr>\n";
  strcpy( activeState, ( debug ? "checked" : "" ) );
  result += "  <tr>\n";
  result += "    <td><label for='debug'>Debug Mode: </label></td>\n";
  result += "    <td><input id='debug' name='debug' type='checkbox' onclick='renderGC()' " + String( activeState ) + "  /></td>\n";
  result += "  </tr>\n";
  result += "  <tr><td>&nbsp;</td><td></td></tr>\n"; // dummy line
  result += "  <tr><td><strong>Sensor Setup</strong></td><td></td></tr>\n"; // head line
  strcpy( activeState, ( gcsensor ? "checked" : "" ) );
  result += "  <tr>\n";
  result += "    <td><label for='gcsensor'>Radiation Sensor: </label></td>\n";
  result += "    <td><input id='gcsensor' name='gcsensor' type='checkbox' onclick='renderGC()' " + String( activeState ) + " /> <span style='color:red'>**</span></td>\n";
  result += "  </tr>\n";
  strcpy( activeState, ( mq135sensor ? "checked" : "" ) );
  result += "  <tr>\n";
  result += "    <td><label for='mq135sensor'>MQ-135 Sensors: </label></td>\n";
  result += "    <td><input id='mq135sensor' name='mq135sensor' type='checkbox' " + String( activeState ) + " onclick='renderRows( this, \"mqPreRow\" )' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='mqPreRow'>\n";
  result += "    <td>&#8627;<label for='bmepre'>Precission: </label></td>\n";
  result += "    <td><input id='mqpre' name='mqpre' type='number' min='0' max='" + String( defpre ) + "' value='" + String( mqpre ) + "' /></td>\n";
  result += "  </tr>\n";
  strcpy( activeState, ( bme280sensor ? "checked" : "" ) );
  result += "  <tr>\n";
  result += "    <td><label for='bme280sensor'>BME280 Sensors: </label></td>\n";
  result += "    <td><input id='bme280sensor' name='bme280sensor' type='checkbox' " + String( activeState ) + " onclick='renderRows( this, \"bmePreRow\" )' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='bmePreRow'>\n";
  result += "    <td>&#8627;<label for='bmepre'>Precission: </label></td>\n";
  result += "    <td><input id='bmepre' name='bmepre' type='number' min='0' max='" + String( defpre ) + "' value='" + String( bmepre ) + "' /></td>\n";
  result += "  </tr>\n";
  strcpy( activeState, ( ds18b20sensor ? "checked" : "" ) );
  result += "  <tr>\n";
  result += "    <td><label for='ds18b20sensor'>DS18B20 Sensors: </label></td>\n";
  result += "    <td><input id='ds18b20sensor' name='ds18b20sensor' type='checkbox' " + String( activeState ) + "  /></td>\n";
  result += "  </tr>\n";
  strcpy( activeState, ( mcp3008sensor ? "checked" : "" ) );
  result += "  <tr>\n";
  result += "    <td><label for='mcp3008sensor'>MCP 300X ADC: </label></td>\n";
  result += "    <td><input id='mcp3008sensor' name='mcp3008sensor' type='checkbox' " + String( activeState ) + " onclick='renderRows( this, \"mcpChannelsRow\" )' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='mcpChannelsRow'>\n";
  result += "    <td>&#8627;<label for='mcppre'>Precission: </label></td>\n";
  result += "    <td><input id='mcppre' name='mcppre' type='number' min='0' max='" + String( defpre ) + "' value='" + String( mcppre ) + "' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='mcpChannelsRow'>\n";
  result += "    <td>&#8627;<label for='mcpchannels'>Channels: </label></td>\n";
  result += "    <td><input id='mcpchannels' name='mcpchannels' type='number' min='0' max='8' value='" + String( mcpchannels ) + "' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr><td>&nbsp;</td><td></td></tr>\n"; // dummy line
  result += "  <tr>\n";
  result += "    <td></td><td><button name='submit' type='submit'>Submit</button></td>\n";
  result += "  </tr>\n";
  result += "</tbody>\n";
  result += "</table>\n";
  result += "</form>\n";
  result += "<div>";
  result += "  <p><span style='color:red'>**</span> Enable only if sensor is pluged in!</p>\n";
  result += "</div>\n";
  result += "<a href='/'>Zur&uuml;ck</a>\n";
  result = htmlBody( result );
  response( result, "text/html" );
}

// Device Form Handler
void deviceFormHandler() {
  currentRequest = "deviceform";
  debugOut( currentRequest );
  authorisationHandler();
  // Sytem stuff
  silent = server.arg("silent") == "on" ? true : false;
  debug = server.arg("debug") == "on" ? true : false;
  // Place
  String placeString = server.arg("place");
  placeString.toCharArray(place, eepromStringSize);
  // Sensor stuff
  String mcpchannelsString = server.arg("mcpchannels");
  mcpchannels = mcpchannelsString.toInt();
  gcsensor = server.arg("gcsensor") == "on" ? true : false;
  mq135sensor = server.arg("mq135sensor") == "on" ? true : false;
  String mqpreString = server.arg("mqpre");
  mqpre = mqpreString.toInt();
  bme280sensor = server.arg("bme280sensor") == "on" ? true : false;
  String bmepreString = server.arg("bmepre");
  bmepre = bmepreString.toInt();
  ds18b20sensor = server.arg("ds18b20sensor") == "on" ? true : false;
  mcp3008sensor = server.arg("mcp3008sensor") == "on" ? true : false;
  String mcppreString = server.arg("mcppre");
  mcppre = mcppreString.toInt();
  // save data into eeprom
  bool result = saveSettings();
  delay(50);
  // send user to restart page
  String location = "/restart";
  if( tokenauth ) { location += "?apikey=" + String( token ); }
  server.sendHeader( "Location",location );
  server.send( 303 );
}


// Device Setup Handler
void authSetupHandler() {
  currentRequest = "authsetup";
  debugOut( currentRequest );
  authorisationHandler();
  char activeState[10];
  String result = "";
  result += "<h2>Authorization Setup</h2>\n";
  result += "<div>";
  result += "  <p>Authentification settings, configure Basic Auth and set a combination of username and password or use the Token Auth and place a Token.</p>";
  result += "</div>\n";
  result += "<form action='/authform' method='POST' id='authsetup'>\n";
  result += "<table style='border:none;'>\n";
  result += "<tbody>\n";
  result += "  <tr><td><strong>Basic Authentication</strong></td><td></td></tr>\n"; // head line
  strcpy( activeState, ( authentication ? "checked" : "" ) );
  result += "  <tr>\n";
  result += "    <td><label for='authentication'>Basic Auth: </label></td>\n";
  result += "    <td><input id='authentication' name='authentication' type='checkbox' onclick='renderRows( this, \"authRow\" )' " + String( activeState ) + " /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='authRow'>\n";
  result += "    <td><label for='authuser'>Username: </label></td>\n";
  result += "    <td><input id='authuser' name='authuser' type='text' placeholder='Username' value='" + String( authuser ) + "' size='" + String( eepromStringSize ) + "' /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='authRow'>\n";
  result += "    <td><label for='authpass'>User Password: </label></td>\n";
  result += "    <td><input id='authpass' name='authpass' type='password' placeholder='Password' value='" + String( authpass ) + "' size='" + String( eepromPassStringSize ) + "' />\n";
  result += "    <button name='showpw' type='button' onclick='toggleVisibility( \"authpass\" )'>&#128065;</button></td>\n";
  result += "  </tr>\n";
  result += "  <tr><td>&nbsp;</td><td></td></tr>\n"; // dummy line
  result += "  <tr><td><strong>Token Authentication</strong></td><td></td></tr>\n"; // head line
  strcpy( activeState, ( tokenauth ? "checked" : "" ) );
  result += "  <tr>\n";
  result += "    <td><label for='tokenauth'>Token Auth: </label></td>\n";
  result += "    <td><input id='tokenauth' name='tokenauth' type='checkbox' onclick='renderRows( this, \"tokenAuthRow\" )' " + String( activeState ) + " /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='tokenAuthRow'>\n";
  result += "    <td><label for='token'>Token: </label></td>\n";
  result += "    <td>\n";
  result += "      <input id='token' name='token' type='password' placeholder='' value='" + String( token ) + "' size='" + String( eepromPassStringSize ) + "' /> \n";
  result += "      <button name='showtoken' type='button' onclick='toggleVisibility( \"token\" )'>&#128065;</button>";
  result += "<button name='genToken' type='button' onclick='generateToken( \"token\" )'>Generate Token</button>\n";
  result += "    </td>\n";
  result += "  </tr>\n";
  result += "  <tr><td>&nbsp;</td><td></td></tr>\n"; // dummy line
  result += "  <tr><td><strong>Disable Authentification after Boot</strong></td><td></td></tr>\n"; // head line
  strcpy( activeState, ( secpush ? "checked" : "" ) );
  result += "  <tr>\n";
  result += "    <td><label for='secpush'>SecPush: </label></td>\n";
  result += "    <td><input id='secpush' name='secpush' type='checkbox' onclick='renderRows( this, \"secPushRow\" )' " + String( activeState ) + " /></td>\n";
  result += "  </tr>\n";
  result += "  <tr class='secPushRow'>\n";
  result += "    <td><label for='secpushtime'>SecPush Time (in Seconds): </label></td>\n";
  result += "    <td><input id='secpushtime' name='secpushtime' type='number' placeholder='300' value='" + String( secpushtime ) + "' max='3600' /></td>\n"; // 3600Sec = 1h
  result += "  </tr>\n";
  result += "  <tr><td>&nbsp;</td><td></td></tr>\n"; // dummy line
  result += "  <tr>\n";
  result += "    <td></td><td><button name='submit' type='submit'>Submit</button></td>\n";
  result += "  </tr>\n";
  result += "</tbody>\n";
  result += "</table>\n";
  result += "</form>\n";
  result += "<a href='/'>Zur&uuml;ck</a>\n";
  result = htmlBody( result );
  response( result, "text/html" );
}

// Auth Form Handler
void authFormHandler() {
  currentRequest = "authform";
  debugOut( currentRequest );
  authorisationHandler();
  // Authentication stuff
  authentication = server.arg("authentication") == "on" ? true : false;
  String authuserString = server.arg("authuser");
  authuserString.toCharArray(authuser, eepromStringSize);
  String authpassString = server.arg("authpass");
  authpassString.toCharArray(authpass, eepromPassStringSize);
  // Token based Authentication stuff
  tokenauth = server.arg("tokenauth") == "on" ? true : false;
  String tokenString = server.arg("token");
  tokenString.toCharArray(token, eepromPassStringSize);
  secpush = server.arg("secpush") == "on" ? true : false;
  String secpushtimeString = server.arg("secpushtime");
  secpushtime = secpushtimeString.toInt();
// save data into eeprom
  bool result = saveSettings();
  delay(50);
  // send user to restart page
  String location = "/restart";
  if( tokenauth ) { location += "?apikey=" + String( token ); }
  server.sendHeader( "Location",location );
  server.send( 303 );
}

// Restart Handler
void restartHandler() {
  currentRequest = "restart";
  debugOut( currentRequest );
  authorisationHandler();
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
  currentRequest = "reset";
  debugOut( currentRequest );
  authorisationHandler();
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
  currentRequest = "metrics";
  debugOut( currentRequest );
  authorisationHandler();
  unsigned long currentMillis = millis();
  String checksumString = sha1( String( ssid ) + String( password ) + String( dnsname ) + String( place )
    + String( (int) authentication ) + String( authuser ) + String( authpass )
    + String( (int) tokenauth ) + String( token )
    + String( (int) secpush ) + String( secpushtime )
    + String( mcpchannels ) + String( (int) silent )  + String( (int) gcsensor ) + String( (int) mq135sensor ) + String( mqpre )
    + String( (int) bme280sensor ) + String( bmepre ) + String( (int) ds18b20sensor ) + String( (int) mcp3008sensor ) + String( mcppre ) + String( (int) debug )
    + String( (int) staticIP ) + ip2Str( ipaddr ) + ip2Str( gateway ) + ip2Str( subnet ) + ip2Str( dns1 ) + ip2Str( dns2 )
    );
  char checksum[ eepromchk ];
  checksumString.toCharArray(checksum, eepromchk); // write checksumString (Sting) into checksum (char Arr)
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
  // Authentification
  bool authEnabled = ( authentication || tokenauth ) ? true : false;
  String authType = ( authentication ) ? "BasicAuth" : "";
  if( authentication && tokenauth ) authType += " & ";
  authType += ( tokenauth ) ? "TokenAuth" : "";
  webString += "# HELP esp_device_auth En or Disabled authentification\n";
  webString += "# TYPE esp_device_auth gauge\n";
  webString += "esp_device_auth{";
  webString += "authtype=\"" + authType + "\"";
  webString += "} " + String( (int) authEnabled ) + "\n";
  // SecPush
  webString += "# HELP esp_device_secpush 1 or 0 En or Disabled SecPush\n";
  webString += "# TYPE esp_device_secpush gauge\n";
  webString += "esp_device_secpush{";
  webString += "secpushstate=\"" + String( (int) secpushstate ) + "\"";
  webString += ",secpushtime=\"" + String( (int) secpushtime ) + "\"";
  webString += "} " + String( (int) secpush ) + "\n";
  // WiFi RSSI 
  String wifiRSSI = String( WiFi.RSSI() );
  webString += "# HELP esp_wifi_rssi Wifi Signal Level in dBm\n";
  webString += "# TYPE esp_wifi_rssi gauge\n";
  webString += "esp_wifi_rssi " + wifiRSSI.substring( 1 ) + "\n";
  // SRAM
  webString += "# HELP esp_device_sram SRAM State\n";
  webString += "# TYPE esp_device_sram gauge\n";
  webString += "esp_device_sram{";
  webString += "size=\"" + String( ESP.getFlashChipRealSize() ) + "\"";
  webString += ",id=\"" + String( ESP.getFlashChipId() ) + "\"";
  webString += ",speed=\"" + String( ESP.getFlashChipSpeed() ) + "\"";
  webString += ",mode=\"" + String( ESP.getFlashChipMode() ) + "\"";
  webString += "} " + String( ESP.getFlashChipSize() ) + "\n";
  // DHCP or static IP
  webString += "# HELP esp_device_dhcp Network configured by DHCP\n";
  webString += "# TYPE esp_device_dhcp gauge\n";
  //// ip in info
  webString += "esp_device_dhcp ";
  webString += staticIP ? "0" : "1";
  webString += "\n";
  // silent mode
  webString += "# HELP esp_device_silent Silent Mode enabled = 1 or disabled = 0\n";
  webString += "# TYPE esp_device_silent gauge\n";
  webString += "esp_device_silent ";
  webString += silent ? "1" : "0";
  webString += "\n";
  // debug mode
  webString += "# HELP esp_device_debug Debug Mode enabled = 1 or disabled = 0\n";
  webString += "# TYPE esp_device_debug gauge\n";
  webString += "esp_device_debug ";
  webString += debug ? "1" : "0";
  webString += "\n";
  // eeprom info
  webString += "# HELP esp_device_eeprom_size Size of EEPROM in byte\n";
  webString += "# TYPE esp_device_eeprom_size gauge\n";
  webString += "esp_device_eeprom_size " + String( eepromSize ) + "\n";
  webString += "# HELP esp_device_config_size Size of EEPROM Configuration data in byte\n";
  webString += "# TYPE esp_device_config_size counter\n";
  webString += "esp_device_config_size " + String( configSize ) + "\n";
  webString += "# HELP esp_device_eeprom_free Size of available/free EEPROM in byte\n";
  webString += "# TYPE esp_device_eeprom_free gauge\n"; // 284
  webString += "esp_device_eeprom_free " + String( ( eepromSize - configSize ) ) + "\n";
  // device uptime
  webString += "# HELP esp_device_uptime Uptime of the Device in Secondes \n";
  webString += "# TYPE esp_device_uptime counter\n";
  webString += "esp_device_uptime ";
  webString += millis() / 1000;
  webString += "\n";
  // checksum
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
//    unsigned long mqcurrentMillis = millis();
    webString += getMQ135Data();
//    long MQRuntime = millis() - mqcurrentMillis;
//    webString += "# HELP esp_mq_runtime Time in milliseconds to read Date from the MQ Sensor\n";
//    webString += "# TYPE esp_mq_runtime gauge\n";
//    webString += "esp_mq_runtime " + String( MQRuntime ) + "\n";
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
    while( ! Serial );
    Serial.println( "" );
    Serial.println( ",----------------------." );
    Serial.println( "| esp8266 node startup |" );
    Serial.println( "`----------------------´" );
    Serial.print( "Starting Device: " );
    Serial.println( dnsname );
    Serial.print( "Software Version: " );
    Serial.println( history );
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
//    delay(200);
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
    delay( 50 );
    
    // start in AP mode, without Password
    String appString = String( dnsname ) + "_setup";
    char apname[ eepromStringSize ];
    appString.toCharArray(apname, eepromStringSize);
    SoftAccOK = WiFi.softAP( apname, "" );
    delay( 50 );
    if( SoftAccOK ) {
//    while( WiFi.status() != WL_CONNECTED ) {
      digitalWrite(LED_BUILTIN, LOW);
      delay( 200 );
      digitalWrite(LED_BUILTIN, HIGH);
      delay( 300);
      digitalWrite(LED_BUILTIN, LOW);
      delay( 200 );
      digitalWrite(LED_BUILTIN, HIGH);
      delay( 300);
      digitalWrite(LED_BUILTIN, LOW);
      delay( 200 );
      digitalWrite(LED_BUILTIN, HIGH);
      if( debug ) { Serial.println( "ok" ); }
    } else {
      if( debug ) { Serial.println( "failed" ); }
    }
    //delay(500);
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
    delay(50);
    //WiFi.softAPdisconnect(); // disconnect all previous clients
//    SoftAccOK = true;
//    captiveCall = false;
  }

  if( gcsensor ) {
    if( debug ) {
      Serial.println( "Geigercounter unable to initialise in debuge Mode!" );
    } else {
      // initialise serial connection to geigercounter
      Serial.println( "Initialise Geigercounter Serial connection" );
      mySerial.begin(9600);
    }
  }

  if( mq135sensor ) {
    if( debug ) Serial.println( "initialize MQ135 Analog Sensor" );
    //MQ135.inicializar();
    //MQ135.setVoltResolution( 5 );
    MQ2.setRegressionMethod(1);
    MQ2.init();
    if( debug ) Serial.print("Calibrating please wait.");
    float calcR0 = 0;
    for(int i = 1; i<=10; i ++) {
      MQ2.update(); // Update data, the arduino will be read the voltage on the analog pin
      calcR0 += MQ2.calibrate( RatioMQ2CleanAir );
      if( debug ) Serial.print(".");
    }
    MQ2.setR0(calcR0/10);
    if( debug ) Serial.println("  done!.");
    if( isinf( calcR0 ) ) {
      if( debug ) Serial.println("Warning: Conection issue founded, R0 is infite (Open circuit detected) please check your wiring and supply"); 
      //while(1);
    }
    if( calcR0 == 0 ) {
      if( debug ) Serial.println("Warning: Conection issue founded, R0 is zero (Analog pin with short circuit to ground) please check your wiring and supply");
      //while(1);
    }
  } else {
//    need resistors connectec between gnd - 220kΩ - ADC (A0) - 1000k - VCC (5V) 
//    https://www.letscontrolit.com/forum/viewtopic.php?p=7832&sid=105badced9150691d2b42bcc5f525dc0#p7832    
//    ADC_MODE(ADC_VCC);
//    ADC_VCC compiler replace this
  }

  if( bme280sensor ) {
    if( debug ) Serial.println( "initialize BME280 Sensor" );
    // Wire connection for bme Sensor
    Wire.begin();
    BMEstatus = bme.begin();  
    printArr( temp );
    //if (!BMEstatus) { while (1); }
  }

  if( ds18b20sensor ) {
    // Initialize the ds18b20 Sensors
    if( debug ) Serial.println( "initialize DS18B20 Sensor" );
    DS18B20.begin();
    // Set Precision
    for( int i = 0; i < tempCount; i++ ) {
      DS18B20.setResolution( tempAdd[i], ds18pre );
      delay(50);
    }
    // Get amount of devices
    tempCount = DS18B20.getDeviceCount();
    if( debug ) { Serial.print( "  detected ds18b20 sensors: " ); Serial.println( String( tempCount ) ); }
  }

  if( mcp3008sensor ) {
    if( debug ) Serial.println( "initialize MCP3008 ADC Device" );
    SPI.begin();
    SPI.setClockDivider( SPI_CLOCK_DIV8 );
    pinMode( mcppin, OUTPUT );
    digitalWrite( mcppin, HIGH );
  }

  // secpush
  if( secpush ) secpushstate = true;
  
  // 404 Page
  server.onNotFound( notFoundHandler );

  // default, main page
  server.on( "/", handle_root );

  // ios captive portal detection request
  server.on( "/hotspot-detect.html", captiveHandler );

  // android captive portal detection request
  server.on( "/generate_204", captiveHandler );
  
  // simple wifi signal strength
  server.on( "/signal", HTTP_GET, signalHandler );

  // Network setup page & form target
  server.on( "/network", HTTP_GET, networkSetupHandler );
  server.on( "/networkform", HTTP_POST, networkFormHandler );

  // Device setup page & form target
  server.on( "/device", HTTP_GET, deviceSetupHandler );
  server.on( "/deviceform", HTTP_POST, deviceFormHandler );

  // Authentification page & form target
  server.on( "/auth", HTTP_GET, authSetupHandler );
  server.on( "/authform", HTTP_POST, authFormHandler );

  // restart page
  server.on( "/restart", HTTP_GET, restartHandler );

  // reset page
  server.on( "/reset", HTTP_GET, resetHandler );

  // metrics (simple prometheus ready metrics output)
  server.on( "/metrics", HTTP_GET, metricsHandler );

//  // Example Raw Response
//  server.on( "/url", HTTP_GET, []() {
//    response( htmlBody( result ), "text/html");
//  });

  // Starting the Weberver
  if( debug ) Serial.print( "Starting the Web-Server: " );
  server.begin();
  if( debug ) { Serial.println( "done" ); Serial.println( "waiting for client connections" ); }
  
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

  if( secpushstate ) secpushstate = securePush();

}

// End Of File