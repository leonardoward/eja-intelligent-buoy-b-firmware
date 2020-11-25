/*LoRa Config
 * This code uses InvertIQ function to create a simple Gateway/Node logic.
  Gateway - Sends messages with enableInvertIQ()
          - Receives messages with disableInvertIQ()
  Node    - Sends messages with disableInvertIQ()
          - Receives messages with enableInvertIQ()
  With this arrangement a Gateway never receive messages from another Gateway
  and a Node never receive message from another Node.
  Only Gateway to Node and vice versa.
  This code receives messages and sends a message every second.
 * 
 */
// Include libraries
#include <WiFi.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <AsyncElegantOTA.h>
#include "SPIFFS.h"
#include <TinyGPS++.h>
#include <SPI.h>
#include <LoRa.h>

#define LED1 5                // ESP32 GPIO connected to LED1
#define LED2 0                // ESP32 GPIO connected to LED2
#define LED_LORA_RX LED1      // Led that notifies LoRa RX
#define LED_LORA_TX LED2      // Led that notifies LoRa TX
#define CS_LORA 2             // LoRa radio chip select
#define RST_LORA 15           // LoRa radio reset
#define IRQ_LORA 13           // LoRa Hardware interrupt pin
#define PERIOD_TX_LORA 1000   // Period between Lora Transmissions
#define PERIOD_ERASE_BUFF 10000 // Period between the erase of terminal msgs
#define DNS_PORT 53           // Port used by the DNS server
#define WEB_SERVER_PORT 80    // Port used by the Asynchronous Web Server
#define PERIOD_TX_LORA 1000   // Period between Lora Transmissions
#define LORA_FREQUENCY 915E6  // Frequency used by the LoRa module
#define GPS_BAUD  9600        // GPS baud rate
#define TIME_PARAM_INPUT_1 "hours"  // Params used to parse the html form submit
#define TIME_PARAM_INPUT_2 "min"    // Params used to parse the html form submit
#define TIME_PARAM_INPUT_3 "sec"    // Params used to parse the html form submit
#define GPS_MESSAGE_ID 0
#define TIMER_MESSAGE_ID 1

// Server credentials
const char* host = "www.buoy_b.eja";
const char* ssid     = "EJA_Buoy_B";
const char* password = "123456789";

// Create AsyncWebServer object on port 80
AsyncWebServer server(WEB_SERVER_PORT);

// DNS Server object
DNSServer dnsServer;

// Buffers that store internal messages
String terminal_messages;
String lora_all_msg;

// The TinyGPS++ object
TinyGPSPlus gps;

// Time (millis) counters for timed operations
unsigned long last_msg_rx = 0;
unsigned long last_lora_tx = 0;
unsigned long last_erase_buffers = 0;
unsigned long last_ap_request = 0;

// JSON with the relevant data
String gps_json;    // GPS data
String timer_json;  // Timer data

//Variables from the GPS
String gps_sattelites;
String gps_hdop;
String gps_lat;
String gps_lng;
String gps_age;
String gps_datetime;
String gps_altitude_meters;
String gps_course_deg;
String gps_speed_kmph;
String gps_course;
String gps_isvalid;
String gps_chars_processed;
String gps_sentences_with_fix;
String gps_failed_checksum;

// Variables of the timer
int timer_end_hours = 0;
int timer_end_min = 0;
int timer_end_sec = 0;
unsigned long timer_end_ts_shared = 0;
unsigned long timer_end_ts_local = 0;
unsigned long timer_current = 0;

int message_selector = GPS_MESSAGE_ID;

bool timer_reached_end = false;

void setup() {
  // Initialize serial communication (used for the GPS)
  Serial.begin(GPS_BAUD);
  // Initialize the output variables as outputs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  // Setup LoRa module
  LoRa.setPins(CS_LORA, RST_LORA, IRQ_LORA);

  // Initialize the LoRa module
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa init failed. Check your connections.");
    lora_all_msg += "LoRa init failed. Check your connections.<br>";
    while (true);                       // if failed, do nothing
  }

  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    terminal_messages += "An Error has occurred while mounting SPIFFS <br>";
    return;
  }

  // Connect to Wi-Fi network with SSID and password
  terminal_messages += "Setting AP (Access Point)…";
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  // Get the IP address
  IPAddress IP = WiFi.softAPIP();
  terminal_messages += "AP IP address: ";
  terminal_messages += IP.toString();

  // Start the dns server
  dnsServer.start(DNS_PORT, host, IP);

  //Add the html, css and js files to the web server
  web_server_config();

  // Start ElegantOTA
  AsyncElegantOTA.begin(&server);

  // Start server
  server.begin();

  // Store LoRa initialization messages
  lora_all_msg += "LoRa init succeeded.<br><br>";
  lora_all_msg += "LoRa Simple Gateway<br>";
  lora_all_msg += "Only receive messages from nodes<br>";
  lora_all_msg += "Tx: invertIQ enable<br>";
  lora_all_msg += "Rx: invertIQ disable<br><br>";

  LoRa.onReceive(onReceiveLora);    // Config LoRa RX routines
  LoRa.onTxDone(onTxDoneLoRa);      // Config LoRa TX routines
  LoRa_rxMode();                // Activate LoRa RX
}

void loop(){
  dnsServer.processNextRequest(); // Process next DNS request
  AsyncElegantOTA.loop();         // Process next OTA request

  // Get the GPS data
  gps_sattelites = getIntGPS(gps.satellites.value(), gps.satellites.isValid(), 5);
  gps_hdop = getIntGPS(gps.hdop.value(), gps.hdop.isValid(), 5);
  gps_lat = getFloatGPS(gps.location.lat(), gps.location.isValid(), 11, 6);
  gps_lng = getFloatGPS(gps.location.lng(), gps.location.isValid(), 12, 6);
  gps_age = getIntGPS(gps.location.age(), gps.location.isValid(), 5);
  gps_datetime = getDateTimeGPS(gps.date, gps.time);
  gps_altitude_meters = getFloatGPS(gps.altitude.meters(), gps.altitude.isValid(), 7, 2);
  gps_course_deg = getFloatGPS(gps.course.deg(), gps.course.isValid(), 7, 2);
  gps_speed_kmph = getFloatGPS(gps.speed.kmph(), gps.speed.isValid(), 6, 2);
  gps_isvalid = getStrGPS(gps.course.isValid() ? TinyGPSPlus::cardinal(gps.course.value()) : "*** ", 6);
  gps_chars_processed = getIntGPS(gps.charsProcessed(), true, 6);
  gps_sentences_with_fix = getIntGPS(gps.sentencesWithFix(), true, 10);
  gps_failed_checksum = getIntGPS(gps.failedChecksum(), true, 9);

  // Create a JSON with the GPS data
  gps_json = "{\"type\": \"gps\",";
  gps_json += " \"sattelites\": \""+ gps_sattelites +"\",";
  gps_json += " \"hdop\": \""+ gps_hdop +"\",";
  gps_json += " \"lat\": \""+ gps_lat +"\",";
  gps_json += " \"lng\": \""+ gps_lng +"\",";
  gps_json += " \"age\": \""+ gps_age +"\",";
  gps_json += " \"datetime\": \""+ gps_datetime +"\",";
  gps_json += " \"altitude\": \""+ gps_altitude_meters +"m\",";
  gps_json += " \"course_deg\": \""+ gps_altitude_meters +"deg\",";
  gps_json += " \"speed_kmph\": \""+ gps_speed_kmph +"Kmph\",";
  gps_json += " \"isValid\": \""+ gps_isvalid+"\"}";

  // Store the GPS data in the terminal buffer
  terminal_messages += "Sattelites: " + gps_sattelites;
  terminal_messages += "HDOP: " + gps_hdop;
  terminal_messages += "Lattitude: " + gps_lat;
  terminal_messages += "Longitude: " + gps_lng;
  terminal_messages += "Age: " + gps_age;
  terminal_messages += "DateTime: " + gps_datetime;
  terminal_messages += "Altitude: " + gps_altitude_meters + " m ";
  terminal_messages += "Course: " + gps_course_deg + " deg ";
  terminal_messages += "Speed: " + gps_speed_kmph + " Kmph ";
  terminal_messages += "isValid: " + gps_isvalid;
  terminal_messages += "Chars Processed: " + gps_chars_processed;
  terminal_messages += "Sentences with FIX: " + gps_sentences_with_fix;
  terminal_messages += "Failed Checksum: " + gps_failed_checksum;
  terminal_messages += "<br>";

  // Create a JSON with the timer data
  timer_json = "{\"type\": \"timer\",";
  timer_json += " \"current_ts\": \"" + String(millis()) + "\",";
  timer_json += " \"end_ts\": \"" + String(timer_end_ts_shared) + "\",";
  timer_json += " \"timer_reached_end\": " + String(int(timer_reached_end)) + "}";
  

  // Apply delay that ensures that the gps object is being "fed".
  smartDelay(1000);

  if (millis() > 5000 && gps.charsProcessed() < 10)
    terminal_messages += "No GPS data received: check wiring <br>";

  // Transmit GPS LoRa msg every PERIOD_TX_LORA millis
  if (runEvery(PERIOD_TX_LORA, &last_lora_tx)) { // repeat every 1000 millis
    switch (message_selector) {
      case GPS_MESSAGE_ID:
        LoRa_sendMessage(gps_json);             // Send a message
        lora_all_msg += "LoRaTX GPS Message<br>";
        message_selector = TIMER_MESSAGE_ID;
        break;
      case TIMER_MESSAGE_ID:
        LoRa_sendMessage(timer_json);             // Send a message
        lora_all_msg += "LoRaTX Timer Message<br>";
        message_selector = GPS_MESSAGE_ID;
        break;
      default:
        // statements
        break;
    }
    
  }

  // Erase terminal buff every PERIOD_ERASE_BUFF millis
  if(runEvery(PERIOD_ERASE_BUFF, &last_erase_buffers)){
    terminal_messages = "";
    lora_all_msg = "";
    Serial.println("Erased terminal buffers");
  }

  if((timer_end_ts_local > 0) && (millis() > timer_end_ts_local)){
    timer_reached_end = true;
    // ------   Activate release mechanism  --------------

    // ---------------------------------------------------
  }
}

/**
 * [runEvery Check if a time interval has passed since the last previousMillis]
 * @param  interval       [Time interval]
 * @param  previousMillis [Time of the previous activation]
 * @return                [True if the time interval has passed since the last
 *                         previousMillis, False if not]
 */
boolean runEvery(unsigned long interval, unsigned long *previousMillis)
{
  unsigned long currentMillis = millis(); // Store the current time
  // Check if a time interval has passed since the last previousMillis
  if (currentMillis - *previousMillis >= interval)
  {
    *previousMillis = currentMillis;      // Update the time of *previousMillis
    return true;
  }
  return false;
}

//########################################
//##      Asynchronous Web Server       ##
//########################################

/**
 * [processor Process requests from web pages and return data]
 * @param  var [description]
 * @return     [description]
 */
String processor(const String& var){
  String result = "";                 // Create a variable to store the result

  Serial.println(var);                // Show the requested var in the serial monitor
  terminal_messages += var + "<br>";  // Add the requested var to the terminal buffer

  if(var == "TERMINAL"){              // return the terminal messages buffer
    result = terminal_messages;
  }else if(var == "TERMINAL_LORA"){   // return the LoRa messages buffer
    result = lora_all_msg;
  }
  return result;
}

//########################################
//##              GPS                   ##
//########################################

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (Serial.available())
      gps.encode(Serial.read());
  } while (millis() - start < ms);
}

static String getFloatGPS(float val, bool valid, int len, int prec)
{
  String result = "";
  if (!valid)
  {
    while (len-- > 1)
      result += "*";
      //Serial.print('*');
    //Serial.print(' ');
    result += " ";
  }
  else
  {
    result += String(val) + " " + String(prec);
    //Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      //Serial.print(' ');
      result += " ";
  }
  smartDelay(0);
  return result;
}

static String getIntGPS(unsigned long val, bool valid, int len)
{
  String result = "";
  char sz[32] = "*****************";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0)
    sz[len-1] = ' ';
  //Serial.print(sz);
  result += "" + String(sz);
  smartDelay(0);
  return result;
}

static String getDateTimeGPS(TinyGPSDate &d, TinyGPSTime &t){
  String result = "";
  if (!d.isValid())
  {
    //Serial.print(F("********** "));
    result += "********** ";
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    //Serial.print(sz);
    result += "" + String(sz);
  }

  if (!t.isValid())
  {
    //Serial.print(F("******** "));
    result += "******** ";
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
    result += "" + String(sz);
  }

  result += getIntGPS(d.age(), d.isValid(), 5);
  smartDelay(0);
  return result;
}

static String getStrGPS(const char *str, int len)
{
  String result;
  int slen = strlen(str);
  for (int i=0; i<len; ++i)
    //Serial.print(i<slen ? str[i] : ' ');
    if(i<slen){
      result += str[i];
    }else{
      result += ' ';
    }
  smartDelay(0);
  return result;
}

//########################################
//##              LoRa                  ##
//########################################

/**
 * [LoRa_rxMode Setup Lora's Receiver Mode]
 */
void LoRa_rxMode(){
  digitalWrite(LED_LORA_RX, HIGH); // turn on the LoRa RX LED
  digitalWrite(LED_LORA_TX, LOW);  // turn off the LoRa TX LED
  LoRa.enableInvertIQ();           // active invert I and Q signals
  LoRa.receive();                  // set receive mode
}

/**
 * [LoRa_txMode Setup Lora's Transmitter Mode]
 */
void LoRa_txMode(){
  digitalWrite(LED_LORA_RX, LOW);  // turn off the LoRa RX LED
  digitalWrite(LED_LORA_TX, HIGH); // turn on the LoRa TX LED
  LoRa.idle();                     // set standby mode
  LoRa.disableInvertIQ();          // normal mode
}

/**
 * [LoRa_sendMessage Transmit a String using LoRa]
 * @param message [String that will be transmitted]
 */
void LoRa_sendMessage(String message) {
  LoRa_txMode();          // set tx mode
  LoRa.beginPacket();     // start packet
  LoRa.print(message);    // add payload
  LoRa.endPacket(true);   // finish packet and send it
}

/**
 * [onReceiveLora Interrupt that activates when LoRa receives a message]
 * @param packetSize [Size of the package received]
 */
void onReceiveLora(int packetSize) {
  String message = "";              //Variable used to store the received message

  while (LoRa.available()) {        // Loop while there is data in the RX buffer
    message += (char)LoRa.read();   // Read a new value from the RX buffer
  }

  // Parse Type of Message
  int init_index = 0;
  int end_index = message.indexOf(':', init_index);
  String json_param = message.substring(message.indexOf('"', init_index) + 1, end_index - 1);
  String json_value = message.substring(message.indexOf('"', end_index) + 1 , message.indexOf(',', end_index) - 1);
  unsigned long onboard_gateway_current_time = 0;
  
  if(json_param.equals("type")){
    if(json_value.equals("timer")){  // Parse timer message   
      init_index = message.indexOf(',', end_index);
      end_index = message.indexOf(':', init_index);
      json_param = message.substring(message.indexOf('"', init_index) + 1, end_index - 1);
      json_value = message.substring(message.indexOf('"', end_index) + 1 , message.indexOf(',', end_index) - 1);
      if(json_param.equals("current_ts")) onboard_gateway_current_time = strtoul(json_value.c_str(), NULL, 10);
      init_index = message.indexOf(',', end_index);
      end_index = message.indexOf(':', init_index);
      json_param = message.substring(message.indexOf('"', init_index) + 1, end_index - 1);
      json_value = message.substring(message.indexOf('"', end_index) + 1 , message.indexOf(',', end_index) - 1);
      if(json_param.equals("end_ts")){
        timer_end_ts_shared = strtoul(json_value.c_str(), NULL, 10);
        if(timer_end_ts_shared > 0 && timer_end_ts_local != timer_end_ts_shared)timer_end_ts_local = millis() + timer_end_ts_shared - onboard_gateway_current_time;
      }
      
      lora_all_msg += "LoRaRX Type : timer";
      lora_all_msg += "LoRaRX Onboard Gateway ts : "+String(onboard_gateway_current_time)+" ms";
      lora_all_msg += "LoRaRX Shared End ts : "+String(timer_end_ts_shared)+" ms";  
      timer_reached_end = false;
    } 
  }
  // Store the message in the string with all messages associated to lora
  lora_all_msg += "LoRaRX Node Received: <br>";
  lora_all_msg += message + "<br>";

  last_msg_rx = millis();           // Store the time of the last reception
}

/**
 * [onTxDoneLoRa Interrupt that activates when LoRa ends transmission]
 */
void onTxDoneLoRa() {
  lora_all_msg += "TxDone<br>"; // Store a message from the Lora process
  LoRa_rxMode();                // Activate lora's reception mode
}

//########################################
//##              Web server            ##
//########################################
void web_server_config(){
  // Route to load the sidebar.js file
  server.on("/sidebar.js", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/sidebar.js", "text/javascript");
  });
  
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  // Route to load sidebar.css file
  server.on("/sidebar.css", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/sidebar.css", "text/css");
  });

  // Route to load header.css file
  server.on("/header.css", HTTP_GET, [](AsyncWebServerRequest *request){
    last_ap_request = millis();
    request->send(SPIFFS, "/header.css", "text/css");
  });

  // Route to load the jquery.min.js file
  server.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/jquery.min.js", "text/javascript");
  });

  // Route to load the lora web page
  server.on("/lora", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/lora.html", String(), false, processor);
  });

  // Route to load a json with the global terminal messages from lora
  server.on("/terminal_messages", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"term\": \""+ terminal_messages+"\"}");
  });

  // Route to load a json with the terminal messages from lora
  server.on("/lora_terminal_messages", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"term\": \""+ lora_all_msg+"\"}");
  });

  // Route to load the terminal web page
  server.on("/terminal", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/terminal.html", String(), false, processor);
  });

  // Route to load the GPS web page
  server.on("/gps", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/gps.html", String(), false, processor);
  });

  // Route to load a json with the GPS data
  server.on("/gps_data", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", gps_json);
  });
  // Route to change the timer (web page)
  server.on("/timer", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(timer_end_ts_local > millis()){
      request->send(SPIFFS, "/gettimer.html", String(), false, processor);
    } else {
      request->send(SPIFFS, "/settimer.html", String(), false, processor);
    }
  });

  // Route to delete the timer (web page)
  server.on("/deletetimer", HTTP_GET, [](AsyncWebServerRequest *request) {
    timer_end_ts_shared = 0; 
    timer_end_ts_local = 0; 
    request->send(SPIFFS, "/settimer.html", String(), false, processor);
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/setTime", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputHours = "0";
    String inputMin = "0";
    String inputSec = "0";
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(TIME_PARAM_INPUT_1)) {
      inputHours = request->getParam(TIME_PARAM_INPUT_1)->value();
      timer_end_hours = inputHours.toInt();
      if (0 > timer_end_hours || 23 < timer_end_hours) timer_end_hours = 0;
    }
    // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
    if (request->hasParam(TIME_PARAM_INPUT_2)) {
      inputMin = request->getParam(TIME_PARAM_INPUT_2)->value();
      timer_end_min = inputMin.toInt();
      if (0 > timer_end_min || 59 < timer_end_min) timer_end_min = 0;
    }
    // GET input3 value on <ESP_IP>/get?input3=<inputMessage>
    if (request->hasParam(TIME_PARAM_INPUT_3)) {
      inputSec = request->getParam(TIME_PARAM_INPUT_3)->value();
      timer_end_sec = inputSec.toInt();
      if (0 > timer_end_sec || 59 < timer_end_sec) timer_end_sec = 0;
    }
    timer_end_ts_local = millis() + timer_end_sec * 1000 + timer_end_min * 60 * 1000 + timer_end_hours * 60 * 60 * 1000;
    //Serial.println("Time Input - Hours : " + inputHours + " - Min : " + inputMin + " - Sec : " + inputSec);
    terminal_messages += "Time Input - H:" + inputHours + " - M:" + inputMin + " - S:" + inputSec;
    //request->send(200, "text/html", "HTTP GET request sent to Buoy B<br>Hours : " + inputHours + "<br>Min : " + inputMin + "<br>Sec : " + inputSec + "<br><a href=\"/\">Return to Home Page</a>");
    if(timer_end_sec > 0 || timer_end_min > 0 || timer_end_hours > 0){
      timer_reached_end = false;
      timer_end_ts_shared = timer_end_ts_local;
      timer_end_hours = 0;
      timer_end_min = 0;
      timer_end_sec = 0;
      request->send(SPIFFS, "/gettimer.html", String(), false, processor);
    } else {
      request->send(SPIFFS, "/settimer.html", String(), false, processor);
    }
  });

  // Route to load a json with the global terminal messages from lora
  server.on("/timer_data", HTTP_GET, [](AsyncWebServerRequest *request) {
    String timer_data = get_remaining_time(timer_end_ts_local);
    request->send(200, "application/json", "{\"remaining_time\": \"" + timer_data +"\"}");
  });
}

String get_remaining_time(unsigned long timer_end_millis){
  unsigned long timer_current = millis();
  String timer_data = "";
  if(timer_end_millis > timer_current){
    int remainig_hours = int((timer_end_millis-timer_current)/(60 * 60 * 1000));
    String remainig_hours_str = String(remainig_hours);
    if(remainig_hours < 10) remainig_hours_str = "0" + remainig_hours_str;
    int remainig_min = int((timer_end_millis-timer_current)/(60 * 1000)) - remainig_hours * 60;
    String remainig_min_str = String(remainig_min);
    if(remainig_min < 10) remainig_min_str = "0" + remainig_min_str;
    int remainig_sec = int((timer_end_millis-timer_current)/(1000)) - remainig_hours * 60 * 60 - remainig_min * 60;
    String remainig_sec_str = String(remainig_sec);
    if(remainig_sec < 10) remainig_sec_str = "0" + remainig_sec_str;
    timer_data = remainig_hours_str + ":" + remainig_min_str + ":" + remainig_sec_str;
  }else{
    timer_data = "00:00:00";
  }
  return timer_data;
}
