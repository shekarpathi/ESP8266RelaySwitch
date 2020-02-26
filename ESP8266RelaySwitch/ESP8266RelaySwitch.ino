// --------------------------------------------------------------
// https://github.com/shekarpathi/ESP8266RelaySwitch
// git clone git@github.com:shekarpathi/ESP8266RelaySwitch.git
// git clone https://github.com/shekarpathi/ESP8266RelaySwitch.git
// --------------------------------------------------------------
//  http://worldclockapi.com/api/json/est/now    Clock API
//  http://worldtimeapi.org/api/ip
//  https://arduinojson.org/assistant/
//  http://www.instructables.com/id/ESP8266-Parsing-JSON/
//  https://arduinojson.org/example/http-client/
//  https://ubidots.com/blog/connect-your-esp8266-to-any-available-wi-fi-network/
//  https://tttapa.github.io/ESP8266/Chap11%20-%20SPIFFS.html
//  ArduinoJson Libraries needed - should be installed separately
//  Gsender library included in this repo - no download needed
//  python.exe C:\Users\username\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.4.2/tools/espota.py -i 192.168.5.162 -p 8266 --auth= -f C:\Users\username\AppData\Local\Temp\arduino_build_849613/ESP8266RelaySwitch.ino.bin 

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>   // Include the WebServer library
#include "GmailSender.h"
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFiMulti.h>
#define USE_SERIAL Serial
#include "Secrets.h"

ESP8266WebServer server(80);            // Create a webserver object that listens for HTTP request on port 80
Gsender *gsender = Gsender::Instance(); // Getting pointer to class instance

const int ON_STATE = 1;
const int OFF_STATE = 0;

const int RELAY_PIN = 0;
const int LED_PIN = 2;

String upSince = "";
String emailSubject = "";
String emailContent = "";

ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

void setup(void){
  pinMode(RELAY_PIN, OUTPUT); // initialize digital esp8266 gpio 0, 2 as an output.
  pinMode(LED_PIN,   OUTPUT);
  delay(100);

  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  
  WiFi.mode(WIFI_STA);                    // #1 DO NOT CHAGE the order, this line should come before
  Serial.println(WiFi.hostname(espname)); // #2 This line is next
  Serial.println("Connecting to WiFi..");

  wifiMulti.addAP(WiFiSID1, WiFiPWD1);
  wifiMulti.addAP(WiFiSID2, WiFiPWD2);
  wifiMulti.addAP(WiFiSID3, WiFiPWD3);
  wifiMulti.addAP(WiFiSID4, WiFiPWD4);

  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks listed above
    delay(200);
    Serial.print('.');
  }
  Serial.print("\nConnected to ");
  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer

  if (MDNS.begin(espname.c_str(), WiFi.localIP())) {              // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started " + espname);
  } else {
    Serial.println("Error setting up MDNS responder for " + espname);
  }
 
  server.on("/"                    , HTTP_GET, []() {
    server.send (200, "text/HTML", "<p><font size=\"10\">Your IP: " + getXFFIP() + "<BR>" + "Up since: " + upSince + "<BR>Current time:"+getTime() + "</font></p>");
  });

  server.on ( "/restart", HTTP_GET, []() {
    if (authenticated("/restart")) {
      sendEmail("Authenticated. Restarting " + espname, "Restarting " + espname + " ESP");
      server.send ( 200, "text/html", espname + " ESP is restarting");
      ESP.restart();
    }
  });
  
  server.on("/printRequestHeaders" , HTTP_GET, printRequestHeaders);

  server.on("/Led_On", HTTP_GET, []() {
    if (authenticated("/Led_On")) {
      digitalWrite(LED_PIN, ON_STATE);
      server.send ( 200, "text/plain", "1");
    }
  });
  
  server.on("/Led_Off", HTTP_GET, []() {
    if (authenticated("/Led_Off")) {
      digitalWrite(LED_PIN, OFF_STATE);
      server.send ( 200, "text/plain", "0");
    }
  });
  
  server.on("/Led_Status", HTTP_GET, []() {
    if (authenticated("/Led_Status")) {
      server.send ( 200, "text/plain", String(!digitalRead(LED_PIN)));
    }
  });

  server.on("/Switch_On", HTTP_GET, []() {
    if (authenticated("/Switch_On")) {
      digitalWrite(RELAY_PIN, ON_STATE);
      server.send ( 200, "text/plain", "1");
    }
  });
  
  server.on("/Switch_Off", HTTP_GET, []() {
    if (authenticated("/Switch_Off")) {
      digitalWrite(RELAY_PIN, OFF_STATE);
      server.send ( 200, "text/plain", "0");
    }
  });
  
  server.on("/Switch_Status", HTTP_GET, []() {
    if (authenticated("/Switch_Status")) {
      server.send ( 200, "text/plain", String(digitalRead(RELAY_PIN)));
    }
  });
 

  //here the list of headers to be recorded
  const char * headerkeys[] = {"User-Agent", "secret", "X-Forwarded-For", "host"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  server.collectHeaders(headerkeys, headerkeyssize );
  
  server.begin();                          // Start the synchronous web server
  Serial.println("HTTP server started. http://"+espname+".lan");

  MDNS.addService("http", "tcp", 80);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.setHostname(espname.c_str());
  ArduinoOTA.begin();
 
  // ----------- OTA Stuff End -----------------//

  String emailString;
  emailString = "Startup Complete for "+espname+"<BR>";

  emailString += "<hr><h3>Useful URLs</h3>";
  emailString += "Accessible (.lan) at <A HREF=http://"+espname+".lan>Home Page</A><BR>";
  emailString += "or (.local), at <A HREF=http://"+espname+".local>Home Page</A><BR>";
  emailString += "Connected to " + WiFi.SSID() + "<BR>";
  emailString += "<A HREF=http://" + WiFi.localIP().toString() + ">http://" + WiFi.localIP().toString() + "</A><BR>";

  emailString += "<hr><h3>Switch 0</h3>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET http://"+espname+".lan/Led_On<BR>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET http://"+espname+".lan/Led_Off<BR>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET  http://"+espname+".lan/Led_Status<BR>";
  
  emailString += "<hr><h3>Switch 2</h3>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET http://"+espname+".lan/Switch_On<BR>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET http://"+espname+".lan/Switch_Off<BR>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET  http://"+espname+".lan/Switch_Status<BR>";

  emailString += "<hr><h3>To restart</h3>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET  http://"+espname+".lan/restart<BR>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET  http://"+espname+".local/restart<BR>";
  emailString += "Setting current state to " + String(OFF_STATE) + "<BR>";

  emailString += "<hr>";
  int n = WiFi.scanNetworks();
  emailString += "<h3>WiFi Available in your area</h3>";
  for (int i = 0; i < n; ++i)
  { // Print SSID and RSSI for each network found Serial.print(i + 1);
    emailString += String(i) + ". " + WiFi.SSID(i) + ":" + WiFi.encryptionType(i) + "<BR>";
  }
  digitalWrite(RELAY_PIN, OFF_STATE);
  digitalWrite(LED_PIN, OFF_STATE);
  upSince = getTime();

  if(gsender->Subject(espname + " Started")->Send(emailSendTo, emailString)) {
    char data[50];
    sprintf(data, "Message sent to %s", emailSendTo);
    Serial.println(data);
  }
  // -----END SETUP------
}

void loop(void){
  server.handleClient();
  if (emailSubject != "") {
    sendEmail(emailSubject, emailContent);
    emailSubject = "";
    emailContent = "";
  }
  ArduinoOTA.handle();
  MDNS.update();
}

boolean authenticated(String path) {
  if (server.hasHeader("secret")) { // request HAS secret header
    if (server.header("secret") == switchpassword) {
      emailSubject = "Authenticated " + path + " on " + espname + " Accessed";
      emailContent = getXFFIP();
      return true;
    }
    else {  // invalid password
      emailSubject = "Invalid secret header value provided on " + path + " on " + espname;
      emailContent = getXFFIP();
      return false;  // NOT authenticated
    }
  }
  else { // request has no secret header
      emailSubject = "No secret header value provided on " + path + " on " + espname;
      emailContent = getXFFIP();
      return false; // NOT authenticated
  }
}

void printRequestHeaders() {
  String headers = "";
  Serial.printf("num headers: %d\n",server.headers());
  for(int i=0;i<server.headers();i++) {
    headers = headers + server.headerName(i).c_str() +": "+server.header(i).c_str()+"<BR>\n";
     Serial.printf("header: %s = %s\n", server.headerName(i).c_str(), server.header(i).c_str());
  }
  sendEmail("printRequestHeaders", "printRequestHeaders was requested");
  server.send(200, "text/html", "<p><font size=\"7\">Your IP is: " + getXFFIP() + "<BR>This unauthorized access will be reported to FBI. Your machine has already been compromised and all your personal data exfiltrated.<BR>" + headers + "</font></p>");
  server.client().stop();
}

String getXFFIP() {
  String xffIP = "-- No X-Forwarded-For found in request";
  if (server.hasHeader("X-Forwarded-For")) {
    xffIP  = server.header("X-Forwarded-For"); // request coming from outside as nginx inserts this
  } else {
    return clientIP(); // No X-Forwarded-For from nginx, hence return clientIP
  }
  return xffIP;
}

String clientIP() {
  String clientIP = server.client().remoteIP().toString();
  Serial.println("server.client().remoteIP(): " + clientIP);
  return clientIP;
}

void sendEmail(String subject, String message) {
  subject = subject + " | " + espname + " Accessed from IP: " + getXFFIP() + " | " + getTime();
  if(gsender->Subject(subject)->Send(emailSendTo, message)) {
    char data[50];
    sprintf(data, "Message sent to %s", emailSendTo);
    Serial.println(data);
  } else {
    char data[50];
    sprintf(data, "Error sending message to %s", emailSendTo);
    Serial.println(data);
    Serial.println(gsender->getError());
  }
}

String getTime() {
  // This website gives you the time based on your IP
  const char* datetime;
  char dateAndTimeCharArray[50];
  HTTPClient http;
  http.setTimeout(5000);
  http.begin("http://worldtimeapi.org/api/ip");
  int httpCode = http.GET();
  Serial.print("1... httpCode:");
  Serial.println(httpCode);
  int lenghtOfResponse = http.getSize();
  Serial.println("3... lenghtOfResponse:");
  Serial.println(lenghtOfResponse);
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String jsonTimeResponse = http.getString();
      //payload = "{\"week_number\":\"11\",\"utc_offset\":\"-04:00\",\"unixtime\":\"1552780656\",\"timezone\":\"America/New_York\",\"dst_until\":\"2019-11-03T06:00:00+00:00\",\"dst_from\":\"2019-03-10T07:00:00+00:00\",\"dst\":true,\"day_of_year\":75,\"day_of_week\":6,\"datetime\":\"2019-03-16T19:57:36.931858-04:00\",\"abbreviation\":\"EDT\"}";
      Serial.print("Response: ");
      Serial.println(jsonTimeResponse);

      // Deserialize the JSON document
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, jsonTimeResponse);
      Serial.println("4... Deserialized");

      // Test if parsing succeeds.
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return "error";
      }
      Serial.println("5... No error");
      datetime = doc["datetime"];
      Serial.println(datetime);

      // ------------
      int Year;
      int Month;
      int Day;
      int Hour;
      int Minute;
      int Second;
      sscanf(datetime, "%d-%d-%dT%d:%d:%d", &Year, &Month, &Day, &Hour, &Minute, &Second);
      snprintf ( dateAndTimeCharArray, 50, "%d/%d/%d %d:%02d:%02d", Month, Day, Year, Hour, Minute, Second); 
      Serial.println(String(dateAndTimeCharArray));
      // ------------
    }
  }
  http.end();
  return String(dateAndTimeCharArray);
}
