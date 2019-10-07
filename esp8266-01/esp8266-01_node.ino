/*
 *  This sketch prints DHTxx data via HTTP.
 *  
 *  Get ESP8266 lib into your Arduino IDE by place this "Board URL" in Settings 
 *  http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *  
 *  1. Install ESP8266 lib to your Arduino IDE (http://esp8266.github.io/Arduino/versions/2.0.0/doc/installing.html)
 *  2. Install simpeDHT lib to your Arduino IDE (https://github.com/winlinvip/SimpleDHT)
 *  3. Hardwre Setup:
 *  
 * PIN  | GPIO   | GPIO   | PIN   
 * ------------------------------   ,---------------.
 * 1    | TX     | GND    |         | o o  ,-.-----.|
 *      | CH_PD  | GPIO2  | 2       | o o  |_| ,---'| 
 *      | RST    | GPIO0  | 0       | o o  ,-. `---.|
 *      | VCC 5V | RX     | 3       | o o  `-' :   ||
 *                                  `---------------'
 * GPIO3 - THRPIN - OneWire Bus
 * GPIO0 - SECPIN - SDA pin of the I2C port
 * GPIO2 - DHTPIN - SCL pin of the I2C port
 * GPIO1 - LEDPIN - Internal LED
 * 
 */

// Verify that we use DHTesp only by a ESP8266 Board
#ifdef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP8266 ONLY!)
#error Select ESP8266 board.
#endif

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SimpleDHT.h>

// definitions
#define SILENT  true // turn LED into silent mode

// setup your wifi ssid, passwd, token and DHTxx Pin
const char* ssid        = "********";         // wifi SSID name
const char* password    = "********";         // wifi wpa2 password
const char* dnsname	    = "esp01";            // Uniq Networkname
const char* history     = "1.8";              // Versions number
const int   port        = 80;                 // Webserver port (default 80)

// Value holders
float         temp            = 0.0;          // temperature value 
float         hum             = 0.0;          // humidy value
String        webString       = "";           // String to display
String        ip              = "127.0.0.1";  // default ip, will overwriten from dhcp

// Set Pins
#define LEDPIN 1        // internal LED Pin

// Set DTH
#define DHTPIN 2        // dht pin (on most esp8266 setups on GPIO2 -> DHTPIN 2)
#define DHTTYPE "DHT22" // dht type, one of: DHT11 or DHT22 

// create DHT Object
//SimpleDHT11 dht(DHTPIN);
SimpleDHT22 dht(DHTPIN);

// run webserver server listener on port X
ESP8266WebServer server( port );

// Enable Power Messuring (Voltage)
ADC_MODE(ADC_VCC);

//
// Helper functions
//

void gettemperature() {
  int err = SimpleDHTErrSuccess;
  if( DHTTYPE == "DHT22" ) {
    // DHT22 Sensor
    float temperature = 0;
    float humidity = 0;
    if ((err = dht.read2(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
      return;
    }
    temp = temperature;
    hum = humidity;
  } else {
    // DHT11 Sensor
    byte temperature = 0;
    byte humidity = 0;
    if ((err = dht.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
      return;
    }
    temp = temperature;
    hum = humidity;
  }
}

String htmlHead( String title ) {
  String result = "<head>\n";
  result += "  <title>" + title + "</title>\n";
  result += "</head>\n";
  return result;
}

String htmlHeader( String device ) {
  String result = "<h1>";
  result += device;
  result += " <small style='color:#ccc'>esp8266 DHT11 sensor node</small>\n";
  result += "</h1>\n";
  result += "<hr />\n";
  return result;
}

String htmlFooter() {
  String result = "<hr style='margin-top:40px;' />\n";
  result += "<p style='color:#ccc'><strong>S</strong>imple<strong>ESP</strong> v" + String( history ) + "</p>\n";
  return result;
}

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
  String result = "<h2>Overview</h2>\n";
  result += "<table>\n";
  result += "  <tr><td><a href='/metrics'>/metrics</a></td></tr>\n";
  result += "</table>\n";
  result += "<p>Just call one of the above URL from scipt or application.</p>";
  response( htmlBody( result ), "text/html");
}

void response( String data, String type ) {
  #if SILENT == false
    digitalWrite(LEDPIN, LOW); // disable interal led
  #endif
  server.send(200, type, data);
  delay(100);
  #if SILENT == false
    digitalWrite(LEDPIN, HIGH); // enable interal led
  #endif
}

// Setup
void setup() {

  #if SILENT == false
    // Initialize the LED_BUILTIN pin as an output
    pinMode(LEDPIN, OUTPUT);
    digitalWrite(LEDPIN, HIGH); // Turn the LED off by HIGH!
  #endif
  
  // Setup Wifi
  WiFi.mode( WIFI_STA );
  
  // set dnshostname
  WiFi.hostname( dnsname );

  // starting wifi connection
  WiFi.begin( ssid, password );
  while( WiFi.status() != WL_CONNECTED ) {
    #if SILENT == false
      digitalWrite(LEDPIN, LOW); // disable interal led (wifi connected)
      delay( 200 );
      digitalWrite(LEDPIN, HIGH); // disable interal led (wifi connected)
    #endif
    delay( 200 );
  }
  
  // default page
  server.on("/", handle_root);

  // all (txt, prometheus metrics)
  server.on( "/metrics", []() {
    unsigned long currentMillis = millis();
    gettemperature();
    long DHTRuntime = millis() - currentMillis;
    webString = "";
    webString += "# HELP esp_firmware_build_info A metric with a constant '1' value labeled by version, board type, dhttype and dhtpin\n";
    webString += "# TYPE esp_firmware_build_info gauge\n";
    webString += "esp_firmware_build_info{";
    webString += "version=\"" + String( history ) + "\"";
    webString += ",board=\"" + String( ARDUINO_BOARD ) + "\"";
    webString += ",nodename=\"" + String( dnsname ) + "\"";
    webString += ",dhttype=\"" + String( DHTTYPE ) + "\"";
    webString += ",dhtpin=\"" + String( DHTPIN ) + "\"";
    webString += "} 1\n";
    webString += "# HELP esp_voltage current Voltage metric\n";
    webString += "# TYPE esp_voltage gauge\n";
    webString += "esp_voltage " + String( (float) ESP.getVcc() / 1024.0 ) + "\n";
    webString += "# HELP esp_dht_temperature Current DHT Temperature\n";
    webString += "# TYPE esp_dht_temperature gauge\n";
    webString += "esp_dht_temperature " + String( temp ) + "\n";
    webString += "# HELP esp_dht_humidity Current DHT Humidity\n";
    webString += "# TYPE esp_dht_humidity gauge\n";
    webString += "esp_dht_humidity " + String( hum ) + "\n";
    webString += "# HELP esp_dht_runtime Time in milliseconds to read Data from the DHT Sensor\n";
    webString += "# TYPE esp_dht_runtime gauge\n";
    webString += "esp_dht_runtime " + String( DHTRuntime ) + "\n";
    response( webString, "text/plain" );
  });
  
  server.begin();
}

void loop() {

  // handle requests
  server.handleClient();

}

// End of File
