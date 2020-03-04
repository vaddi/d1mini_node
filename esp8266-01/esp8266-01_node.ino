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
 * After flashing this sketch, the device will starting in AP Mode
 * connect to the wifi esp_setup and configure the Device.
 */

// Verify that we use DHTesp only by a ESP8266 Board
#ifdef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP8266 ONLY!)
#error Select ESP8266 board.
#endif


// default includes
#include <Hash.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <SimpleDHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>


// setup your wifi ssid, passwd, token and DHTxx Pin

const int eepromStringSize      = 24;     // maximal byte size for strings (ssid, wifi-password, dnsname)

char ssid[eepromStringSize]     = ""; // wifi SSID name
char password[eepromStringSize] = "";     // wifi wpa2 password
char dnsname[eepromStringSize]  = "esp"; // Uniq Networkname
char place[eepromStringSize]    = "";     // Place of the Device
int  port                       = 80;     // Webserver port (default 80)
bool silent                     = false;  // enable/disable silent mode

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

// Value holders
float         temp            = 0.0;          // temperature value 
float         hum             = 0.0;          // humidy value


// Set Pins
#define LEDPIN 1        // internal LED Pin

// Set DTH
#define DHTPIN 2        // dht pin (on most esp8266 setups on GPIO2 -> DHTPIN 2)
#define DHTTYPE "DHT11" // dht type, one of: DHT11 or DHT22 



// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// create DHT Object
SimpleDHT11 dht(DHTPIN);
//SimpleDHT22 dht(DHTPIN);

// run webserver server listener on port X
ESP8266WebServer server( port );

// Enable Power Messuring (Voltage)
ADC_MODE(ADC_VCC);

//
// Helper functions
//

void response( String data, String type ) {
  if( ! silent ) {
    digitalWrite(LEDPIN, LOW); // disable interal led
  }
  server.send(200, type, data);
  delay(100);
  if( ! silent ) {
    digitalWrite(LEDPIN, HIGH); // enable interal led
  }
  if( dnssearch.length() == 0 && ! isIp( server.hostHeader() ) ) { // only if not in AP Mode  ! SoftAccOK &&
    String fqdn = server.hostHeader(); // esp22.speedport.ip
    int hostnameLength = String( dnsname ).length() + 1;
    dnssearch = fqdn.substring( hostnameLength );
  }
}

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
    char eepromchecksum[ eepromchk ] = "";
  } data;
  // read data from eeprom
  EEPROM.get( eepromAddr, data );
  // validate checksum
  String checksumString = sha1( String( data.eepromssid ) + String( data.eeprompassword ) + String( data.eepromdnsname ) + String( data.eepromplace ) 
        + String( (int) data.eepromauthentication ) + String( data.eepromauthuser )  + String( data.eepromauthpass )
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
    ipaddr = data.eepromipaddr;
    gateway = data.eepromgateway;
    subnet = data.eepromsubnet;
    dns1 = data.eepromdns1;
    dns2 = data.eepromdns1;
    authentication = data.eepromauthentication;
    strncpy( authuser, data.eepromauthuser, eepromStringSize );
    strncpy( authpass, data.eepromauthpass, eepromStringSize );
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
  // create new checksum
  String checksumString = sha1( String( data.eepromssid ) + String( data.eeprompassword ) + String( data.eepromdnsname ) + String( data.eepromplace ) 
        + String( (int) data.eepromauthentication ) + String( data.eepromauthuser )  + String( data.eepromauthpass )
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
  result += ".staticIPRow, .authRow {\n";
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

String htmlHeader( String device ) {
  String result = "<div id='signalWrap'>Signal Strength: <span id='signal'>" + String( WiFi.RSSI() ) + " dBm</span></div>"; 
  result += "<h1>" + device + " <small style='color:#ccc'>esp8266 DHT11 sensor node</small></h1>\n";
  result += "<hr />\n";
  return result;
}

String htmlFooter() {
  String result = "<hr style='margin-top:40px;' />\n";
  result += "<p style='color:#ccc;float:right'>";
  result += staticIP ? "Static " : "";
  result += "Node IP: " + ip + "</p>\n";
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



//
// html handler
//

// initial page
void handle_root() {
  if ( captiveHandler() ) { // If captive portal redirect instead of displaying the root page.
    return;
  }
  String result = "";
  // create current checksum
  String checksumString = sha1( String( ssid ) + String( password ) + String( dnsname ) + String( place ) 
      + String( (int) authentication ) + String( authuser )  + String( authpass )
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

  result += "  <tr><td></td><td></td></tr>\n"; // space line
  result += "  <tr><td>DNS Search Domain: </td><td>" + dnssearch + "</td></tr>\n";
  if( silent ) {
    result += "  <tr><td>Silent Mode</td><td>Enabled (LED disabled)</td></tr>\n";
  } else {
    result += "  <tr><td>Silent Mode</td><td>Disabled (LED blink on Request)</td></tr>\n";
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
  // Authentication stuff
  authentication = server.arg("authentication") == "on" ? true : false;
  String authuserString = server.arg("authuser");
  authuserString.toCharArray(authuser, eepromStringSize);
  String authpassString = server.arg("authpass");
  authpassString.toCharArray(authpass, eepromStringSize);
  // save data into eeprom
  saveSettings();
  delay(50);
  // send user to restart page
  server.sendHeader( "Location","/restart" );
  server.send( 303 );
}

// Restart Handler
void restartHandler() {
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

void handle_metrics() {
  String checksumString = sha1( String( ssid ) + String( password ) + String( dnsname ) + String( place ) 
    + String( (int) authentication ) + String( authuser )  + String( authpass )
    + String( (int) staticIP ) + ip2Str( ipaddr ) + ip2Str( gateway ) + ip2Str( subnet ) + ip2Str( dns1 ) + ip2Str( dns2 ) 
    );
  char checksum[ eepromchk ];
  checksumString.toCharArray(checksum, eepromchk); // write checksumString (Sting) into checksum (char Arr)
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
  webString += ",nodeplace=\"" + String( place ) + "\"";
  webString += ",dhttype=\"" + String( DHTTYPE ) + "\"";
  webString += ",dhtpin=\"" + String( DHTPIN ) + "\"";
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
  // Total Runtime
  long TOTALRuntime = millis() - currentMillis;
  webString += "# HELP esp_total_runtime in milli Seconds to collect data from all Sensors\n";
  webString += "# TYPE esp_total_runtime gauge\n";
  webString += "esp_total_runtime " + String( TOTALRuntime ) + "\n";
  response( webString, "text/plain" );
}


// Setup
void setup() {

  if( ! silent ) {
    // Initialize the LED_BUILTIN pin as an output
    pinMode(LEDPIN, OUTPUT);
    digitalWrite(LEDPIN, HIGH); // Turn the LED off by HIGH!
  }

  // Start eeprom 
  EEPROM.begin( eepromSize );
  // load data from eeprom
  bool loadingSetup = loadSettings(); 



  // start Wifi mode depending on Load Config (no valid config = start in AP Mode)
  if( loadingSetup ) {

    // WiFI Client Mode

    // use a static IP?
    if( staticIP ) {
      WiFi.config( ipaddr, gateway, subnet, dns1, dns2 ); // static ip adress
    }
    // Setup Wifi
    WiFi.mode( WIFI_STA );
    // set dnshostname
    WiFi.hostname( dnsname );
    // starting wifi connection
    WiFi.begin( ssid, password );
    while( WiFi.status() != WL_CONNECTED ) {
      if( !silent ) {
        digitalWrite(LEDPIN, LOW); // disable interal led (wifi connected)
        delay( 200 );
        digitalWrite(LEDPIN, HIGH); // disable interal led (wifi connected)
      }
      delay( 200 );
    }
    ip = WiFi.localIP().toString();
    SoftAccOK = false;
    captiveCall = false;

  } else {

    // WiFI soft AP Mode

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
      digitalWrite(LEDPIN, LOW);
      delay( 200 );
      digitalWrite(LEDPIN, HIGH);
      delay( 400);
      digitalWrite(LEDPIN, LOW);
      delay( 200 );
      digitalWrite(LEDPIN, HIGH);
      delay( 400);
      digitalWrite(LEDPIN, LOW);
      delay( 200 );
      digitalWrite(LEDPIN, HIGH);
    }
    delay(500);
    ip = WiFi.softAPIP().toString();
    
    /* Setup the DNS server redirecting all the domains to the apIP */
    // https://github.com/tzapu/WiFiManager
    dnsServer.setErrorReplyCode( DNSReplyCode::NoError );
    dnsServer.start( DNS_PORT, "*", WiFi.softAPIP() );
    delay(500);
    
  }






  
  
  
  

  

  // 404 Page
  server.onNotFound( notFoundHandler );

  // default page
  server.on("/", handle_root);

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
  server.on( "/metrics", HTTP_GET, handle_metrics );

  // Starting the Webserver
  server.begin();
}

void loop() {

  // DNS redirect clients
  if( SoftAccOK ) {
    dnsServer.processNextRequest();
  }

  // handle requests
  server.handleClient();

}

// End of File