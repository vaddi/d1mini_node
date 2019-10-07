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

  BME280  | D1 Mini
  -----------------
  VIN     | 5V
  GND     | GND
  SCL     | Pin 5 *
  SDA     | Pin 4 *
  * 

  MQ135
  -----------------
  VIN     | 5V
  GND     | GND
  AO      | PIN A0 *
  * When A0 is used, accurate Voltage calculation will be disabled!

  Capacitive Soil Moisture Sensor
  -----------------
  VIN     | 3.3V
  GND     | GND
  AO      | PIN A0 *
  * When A0 is used, accurate Voltage calculation will be disabled!

  ds18b20
  -----------------
  VIN     | 3,3V *
  GND     | GND
  Data    | PIN D3 *
  * Wiring a 4,7kΩ Resistor between VIN and Data!

  Geigercounter | D1 Mini
  -----------------------
  6 green
  5 yellow      | 3
  4 orange      | 1
  3 red         | 3,3V
  2 brown
  1 black       | GND

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
  
*/

// enable/disable sensors
#define SILENT        true  // enable/disable silent mode
#define CPSENSOR      false // enable/disable Capacitive Soil Moisture Sensore v1.0 (only if no MQ Sensor is in use!)
#define MQ135SENSOR   true  // enable/disable mq-sensor. They use Pin A0 and will disable the accurate vcc messuring
#define BME280SENSOR  true  // enable/disable bme280 (on address 0x76). They use Pin 5 (SCL) and Pin 4 (SDA) for i2c
#define DS18B20SENSOR true  // enable/disable ds18b20 temperatur sensors. They use Pin D3 as default for OneWire
#define GCSENSOR      false // enable/disable mightyohm geigercounter

// Declare a OneWire Pin, if DS18B20SENSOR is set to true!
#if DS18B20SENSOR == true
  #define ONE_WIRE_BUS D3
  #define TEMPERATURE_PRECISION 10 // Define precision 9, 10, 11, 12 bit (9=0,5, 10=0,25, 11=0,125)
#endif

// setup your wifi ssid, passwd and dns stuff
const char* ssid        = "********";         // wifi SSID name
const char* password    = "********";         // wifi wpa2 password
const char* dnsname     = "esp01";            // Uniq Networkname
const int   port        = 80;                 // Webserver port (default 80)

//
// Don't edit below Stuff, until you know what youre doing!
//



//
// Set Functions, Values and Libraries depending on sensors enabled/disabled
//

// Geigercounter
//
#if GCSENSOR == true
  #include <SoftwareSerial.h>
  // Serial connection to MightyOhm Geigercounter Device
  SoftwareSerial mySerial( 3, 1, false ); // RX Pin, TX Pin, inverse_output
  String cps             = "0.0";
  String cpm             = "0.0";
  String uSvh            = "0.0";
  String gcmode          = "";
  // Helper function to find string in string
  // read serial data
  String readGCSerial() {
    char dataBuffer[64];
    String data;
    if( mySerial.available() ) {
      mySerial.readBytesUntil( '\n', dataBuffer, 64 );
      data = dataBuffer;
    }
    return data;
  }
  // parse data
  void parseGCData() {
    String data = readGCSerial();
    while( data == "" ) {
      data = readGCSerial();
      delay( 10 );
    }
    int commaLocations[6];
    commaLocations[0] = data.indexOf(',');
    commaLocations[1] = data.indexOf(',',commaLocations[0] + 1);
    commaLocations[2] = data.indexOf(',',commaLocations[1] + 1);
    commaLocations[3] = data.indexOf(',',commaLocations[2] + 1);
    if( data.length() > 53 ) { // 66
      commaLocations[4] = data.indexOf(',',41 + 1);
      commaLocations[5] = data.indexOf(',',49 + 1);
//    } else if( data.length() > 53 ) {
//      commaLocations[4] = data.indexOf(',',41 + 1);
//      commaLocations[5] = data.indexOf(',',49 + 1);
    } else {
      commaLocations[4] = data.indexOf(',',commaLocations[3] + 1);
      commaLocations[5] = data.indexOf(',',commaLocations[4] + 1);
    }  
    cps = data.substring(commaLocations[0] + 2,commaLocations[1]);
    cpm = data.substring(commaLocations[2] + 2, commaLocations[3]);
    uSvh = data.substring(commaLocations[4] + 2, commaLocations[5]);
//    uSvh = uSvh + " (" + data.length() + ")";
    gcmode = data.substring(commaLocations[5] + 2, commaLocations[5] + 3);
  }
  // Get formatet Geigercounterdata function
  String getGCData() {
    unsigned long currentMillis = millis();
    parseGCData();
    long GCRuntime = millis() - currentMillis;
    String result = "";
    result += "# HELP esp_mogc_cps Geigercounter Counts per Second\n";
    result += "# TYPE esp_mogc_cps gauge\n";
    result += "esp_mogc_cps " + cps + "\n";
    result += "# HELP esp_mogc_cpm Geigercounter Counts per Minute\n";
    result += "# TYPE esp_mogc_cpm gauge\n";
    result += "esp_mogc_cpm " + cpm + "\n";
    result += "# HELP esp_mogc_usvh Geigercounter Micro Sivert per Hour\n";
    result += "# TYPE esp_mogc_usvh gauge\n";
    result += "esp_mogc_usvh " + uSvh + "\n";
    result += "# HELP esp_mogc_mode Geigercounter current read mode\n";
    result += "# TYPE esp_mogc_mode gauge\n";
    result += "esp_mogc_mode{mode=\"" + gcmode + "\"} 1\n";
    result += "# HELP esp_mogc_runtime Time in milliseconds to read Geigercounter Values\n";
    result += "# TYPE esp_mogc_runtime gauge\n";
    result += "esp_mogc_runtime " + String( GCRuntime ) + "\n";
    return result;
  }
#else
  String getGCData() { return ""; } // dummy function
#endif


// DS18B20
//
#if DS18B20SENSOR == true
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
        result += "esp_ds18b20{device=\"" + String( i ) + "\",deviceId=\"" + String( (int) tempAdd[i],HEX ) + "\"} " + String( temperature ) + "\n";
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
#else
  //String DS18B20 = "";
  String getDS18B20Data() { return ""; } // dummy function
#endif // End ds18b20

// BME280
//
#if BME280SENSOR == true
  #include <Wire.h>
  #include <Adafruit_Sensor.h>
  #include <Adafruit_BME280.h>
  #define SEALEVELPRESSURE_HPA (1013.25) // reference sealeavel
  // Create bme Instance (by i2c address 0x77 and 0x76)
  Adafruit_BME280 bme;
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
#else
  String getBME280Data() { return ""; } // dummy function
#endif // End bme280

// MQ135
//
#if MQ135SENSOR == true
  #include <MQUnifiedsensor.h>
  #define mqpin A0 // MQ Analog Signal Pin
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
#else
  // Capacitive Soil Moisture Sensore v1.0
  //
  #if CPSENSOR == true
    #define cppin A0
    int         moisture  = 0;
    String getMoistureData() {
      unsigned long currentMillis = millis();
      moisture = analogRead( cppin );
      delay(4); // minimum read time in millis
      long MoistureRuntime = millis() - currentMillis;
      String result = "";
      result += "# HELP esp_moisture Current Capacity moisture Sensor Value\n";
      result += "# TYPE esp_moisture gauge\n";
      result += "esp_moisture " + String( moisture ) + "\n";
      result += "# HELP esp_moisture_runtime Time in milliseconds to read Capacitive moisture Sensor\n";
      result += "# TYPE esp_moisture_runtime gauge\n";
      result += "esp_moisture_runtime " + String( MoistureRuntime ) + "\n";
      return result;
    }
  #else
    // Enable Power Messuring (Voltage) only if no mq sensor is used
    ADC_MODE(ADC_VCC);
    String getMQ135Data() { return ""; } // dummy function
    String getMoistureData() { return ""; } // dummy function
  #endif
#endif // end mq135


//
// Default settings
//

// default includes
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Set default Values
const char* history   = "1.9";        // Software Version
String      webString = "";           // String to display
String      ip        = "127.0.0.1";  // default ip, will overwriten from dhcp

// Instanciate webserver listener on given port
ESP8266WebServer server( port );


//
// Helper Functions
//

// http response handler
void response( String data, String requesttype ) {
  #if SILENT == false
    digitalWrite(LED_BUILTIN, LOW);
  #endif
    server.send(200, requesttype, data);
    delay(100);
  #if SILENT == false
    digitalWrite(LED_BUILTIN, HIGH);
  #endif
}

// html head
String htmlHead( String title ) {
  String result = "<head>\n";
  result += "  <title>" + title + "</title>\n";
  result += "</head>\n";
  return result;
}

// html header
String htmlHeader( String device ) {
  String result = "<h1>" + device + " <small style='color:#ccc'>a D1Mini Node Exporter</small></h1>\n";
  result += "<hr />\n";
  return result;
}

// html footer
String htmlFooter() {
  String result = "<hr style='margin-top:40px;' />\n";
  result += "<p style='color:#ccc;float:right'>Node IP: " + ip + "</p>\n";
  result += "<p style='color:#ccc'><strong>S</strong>imple<strong>ESP</strong> v" + String( history ) + "</p>\n";
  return result;
}

// html body
String htmlBody( String content ) {
  String result = "<!doctype html>\n";
  result += htmlHead( String( WiFi.hostname() ) );
  result += "<body>\n";
  result += htmlHeader( String( WiFi.hostname() ) );
  result += content;
  result += htmlFooter();
  result += "</body>\n";
  result += "</html>";
  return result;
}

// initial page
void handle_root() {
  String result = "";
  result += "<h2>Info</h2>\n";
  result += "<p>A Prometheus scrape ready BME280/MQ135/DS18B20/Geigercounter Node Exporter.<br />\n";
  result += "Just add this Node to your prometheus.yml as Target to collect Air Pressure, Air Humidity, Temperatures, Radiation and some Air Quality Data like Alcohole, CO, CO2, Ammonium, Acetone and Diphenylmethane.</p>";
  
  result += "<h2>Metrics Link</h2>\n";
  result += "<table>\n";
  result += "  <tr><td><a href='/metrics'>/metrics</a></td></tr>\n";
  result += "</table>\n";
  result += "<h2>Enabled Sensors</h2>\n";
  result += "<table>\n";
  #if DS18B20SENSOR == true
    result += "<tr><td>DS18B20</td><td>Temperature Sensor</td></tr>\n";
  #endif
  #if BME280SENSOR == true
    result += "<tr><td>BME280</td><td>Air Temperature, Humidity and Pressure Sensor</td></tr>\n";
  #endif
  #if MQ135SENSOR == true
    result += "<tr><td>MQ135</td><td>Air Quality Sensor</td></tr>\n";
  #endif
  #if CPSENSOR == true
    result += "<tr><td>Capacity Moisture</td><td>Capacity Soil Moisture Sensor</td></tr>\n";
  #endif
  #if GCSENSOR == true
    result += "<tr><td>Geigercounter</td><td>MightyOhm Geigercounter</td></tr>\n";
  #endif
  result += "<tr><td></td><td></td></tr>\n";
  #if SILENT == false
    result += "<tr><td>Silent Mode</td><td>Enabled (LED blink on Request)</td></tr>\n";
  #else
    result += "<tr><td>Silent Mode</td><td>Disabled (LED disabled)</td></tr>\n";
  #endif
  result += "<table>\n";
  
  response( htmlBody( result ), "text/html");
}

//
// Setup
//

void setup() {

  #if SILENT == false
    // Initialize the LED_BUILTIN pin as an output
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED off by HIGH!
  #endif

  // Setup Wifi
  WiFi.mode( WIFI_STA );
  
  // set dnshostname #
  WiFi.hostname( dnsname );

  // starting wifi connection
  WiFi.begin( ssid, password );
  while( WiFi.status() != WL_CONNECTED ) {
    #if SILENT == false
      digitalWrite(LED_BUILTIN, LOW);
      delay( 200 );
      digitalWrite(LED_BUILTIN, HIGH);
    #endif
    delay( 200 );
  }
  delay(200);
  // get the local ip adress
  ip = WiFi.localIP().toString();

  #if GCSENSOR == true
    mySerial.begin(9600);//Set up Software Serial Port
  #endif

  #if DS18B20SENSOR == true
    // Initialize the ds18b20 Sensors
    DS18B20.begin();
    // Set Precision
    for( int i = 0; i < tempCount; i++ ) {
      DS18B20.setResolution( tempAdd[i], TEMPERATURE_PRECISION );
      delay(100);
    }
    // Get amount of devices
    tempCount = DS18B20.getDeviceCount();
  #endif

  #if BME280SENSOR == true
    // Wire connection for bme Sensor
    Wire.begin();
    unsigned BMEstatus;
    BMEstatus = bme.begin();  
    if (!BMEstatus) { while (1); }
  #endif

  #if MQ135SENSOR == true
    MQ135.inicializar();
    MQ135.setVoltResolution(5);
  #endif
  
  // default page
  server.on("/", handle_root);

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
    webString += "} 1\n";
    // Voltage depending on what sensors are in use (Pin A0?)
    webString += "# HELP esp_voltage current Voltage metric\n";
    webString += "# TYPE esp_voltage gauge\n";
    #if MQ135SENSOR == true
      webString += "esp_voltage " + String( (float) ( ESP.getVcc() / 1024.0 / 10.0 ) - mqvoltage ) + "\n";
    #else
      #if CPSENSOR == true
        webString += "esp_voltage " + String( (float) ( ESP.getVcc() / 1024.0 / 10.0 ) ) + "\n";
      #else
        webString += "esp_voltage " + String( (float) ESP.getVcc() / 1024.0 ) + "\n";
      #endif
    #endif
    #if DS18B20SENSOR == true
      // DS18B20 Output
      webString += getDS18B20Data();
    #endif
    #if BME280SENSOR == true
      // BME280 Output
      webString += getBME280Data();
    #endif
    #if MQ135SENSOR == true
      // MQ135 Output
      webString += getMQ135Data();
    #endif
    #if CPSENSOR == true
      webString += getMoistureData();
    #endif
    #if GCSENSOR == true
      webString += getGCData();
    #endif
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
  // handle http requests
  server.handleClient();
}

// End Of File
