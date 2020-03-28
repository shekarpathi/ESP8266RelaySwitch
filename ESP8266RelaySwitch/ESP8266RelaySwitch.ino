// --------------------------------------------------------------
// https://github.com/shekarpathi/ESP8266RelaySwitch
// git clone git@github.com:shekarpathi/ESP8266RelaySwitch.git
// git clone https://github.com/shekarpathi/ESP8266RelaySwitch.git
// --------------------------------------------------------------
//  https://ubidots.com/blog/connect-your-esp8266-to-any-available-wi-fi-network/
//  https://tttapa.github.io/ESP8266/Chap11%20-%20SPIFFS.html

// --------------------------------------------------------------------------------------
// REM OTA flash from the windows command line
// REM 1. Make sure you enable and disable network and wifi adapters before this
// REM 2. Make sure your WINDOWS INBOUND firewall rules allow python3

// set FILE=C:\Users\LoggedInUser\Documents\ESP8266RelaySwitch.ino.bin

// copy /B /Y C:\Users\LoggedInUser\AppData\Local\Temp\arduino_build_nnnnnn\ESP8266RelaySwitch.ino.bin %FILE%

// set PYEXE=C:\Users\LoggedInUser\AppData\Local\Arduino15\packages\esp8266\tools\python3\3.7.2-post1\python3
// set PY=C:\Users\LoggedInUser\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.6.3\tools\espota.py

// %PYEXE% %PY% -i Plug1.lan   -p 8266 --auth= -f %FILE%
// %PYEXE% %PY% -i Plug2.lan   -p 8266 --auth= -f %FILE%
// %PYEXE% %PY% -i Switch1.lan -p 8266 --auth= -f %FILE%
// %PYEXE% %PY% -i Switch2.lan -p 8266 --auth= -f %FILE%
// --------------------------------------------------------------------------------------

//  If OTA port does not show up or
//  If OTA port shows up but does not upload do these following
//  These apply only to windows / 10
//  (1.) Disable IPV6 for all network adapters
//  (2.) Exit IDE
//  (3.) Disable and Enable all Network adapters
//  (4.) Relaunch IDE
//  Also in Windows firewall (5.) Set a new rule for INBOUND firewall for "C:\dddd\vvv\Python.exe"
//  Do these and it is guaranteed to work

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>   // Include the WebServer library
#include <ArduinoOTA.h>
#include <ESP8266WiFiMulti.h>
#define USE_SERIAL Serial
#include "Secrets.h"
#include <EEPROM.h>
#include "EMailSender.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClientIST(ntpUDP, "in.pool.ntp.org", 5.5*60*60);
NTPClient timeClientEST(ntpUDP, "2.pool.ntp.org", -5*60*60);
NTPClient timeClientEDT(ntpUDP, "3.pool.ntp.org", -4*60*60);

ESP8266WebServer server(80);            // Create a webserver object that listens for HTTP request on port 80
EMailSender emailSend(EMAIL_FROM, EMAIL_PASSWORD);

const int ON_STATE = 1;
const int OFF_STATE = 0;

const int RELAY_PIN = 0;
const int LED_PIN = 2;

String upSince = "";
String emailSubject = "";
String emailContent = "";
String emailString;

long previousMillis = millis();   // will store current time in milli seconds
long interval = 1000*60*60*24;    // interval at which ESP8266 will restart in milliseconds

ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

  uint addr = 0;

  // structure to hold our data
  struct { 
    char ESPHostname[20] = "";
    char ssid[20] = "";
    char wifiPassword[40] = "";
    char switchPassword[40] = "";
  } data;

void setup(void){
  pinMode(RELAY_PIN, OUTPUT); // initialize digital esp8266 gpio 0, 2 as an output.
  pinMode(LED_PIN,   OUTPUT);
  delay(100);

  Serial.begin(115200);         // Start the Serial communication to send messages to the computer

  EEPROM.begin(512); // Menu -> Tools -> Erase Flash is set to "Sketch Only"  Otherwise it WILL get overwritten every time
//  initializeEeprom();
  // 1#) Read EEPROM
  EEPROM.get(addr,data);
  Serial.println("Existing values are: "+String(data.ESPHostname)+", "+String(data.ssid)+", "+String(data.wifiPassword)+", "+String(data.switchPassword));

  // 2#) Check if they are valid
  //     If not, read from the secrets file and save them back to EEPROM
  if (!isDataValid(data.ESPHostname)) {
    Serial.println("EEPROM data.ESPHostname: "+String(data.ESPHostname)+" is not valid. Copying from secrets.h");
    strncpy(data.ESPHostname, espname.c_str(), 20);
  }
  if (!isDataValid(data.ssid)) {
    Serial.println("EEPROM data.ssid: "+String(data.ssid)+" is not valid. Copying from secrets.h");
    strncpy(data.ssid, WiFiSID1, 20);
  }
  if (!isDataValid(data.wifiPassword)) {
    Serial.println("EEPROM data.wifiPassword: "+String(data.wifiPassword)+" is not valid. Copying from secrets.h");
    strncpy(data.wifiPassword, WiFiPWD1, 40);
  }
  if (!isDataValid(data.switchPassword)) {
    Serial.println("EEPROM data.switchPassword: "+String(data.switchPassword)+" is not valid. Copying from secrets.h");
    strncpy(data.switchPassword, switchpassword.c_str(), 40);
  }
  Serial.println("1. New values are: "+String(data.ESPHostname)+", "+String(data.ssid)+", "+String(data.wifiPassword)+", "+String(data.switchPassword));
  EEPROM.put(addr,data);
  EEPROM.commit();
  EEPROM.get(addr,data);
  Serial.println("2. New values are: "+String(data.ESPHostname)+", "+String(data.ssid)+", "+String(data.wifiPassword)+", "+String(data.switchPassword));

  // 3#) Write the values back from the structure/EEPROM to the ESP8266's memory
  // EEPROM contains the authoratitive data.  EEPROM and the memory variables will be in sync after this
  espname = data.ESPHostname;
  WiFiSID1 = data.ssid;
  WiFiPWD1 = data.wifiPassword;
  switchpassword = data.switchPassword;
 
  WiFi.mode(WIFI_STA);                             // #1 DO NOT CHAGE the order, this line should come before
  int setHostnameStatus = WiFi.hostname(espname);  // #2 This line is next
  if (setHostnameStatus == 1) {
    Serial.println("You will be able reference this ESP by the DNS name from other machines using http://"+espname+".lan");
  } else {
    Serial.println("You will NOT be able reference this ESP by the DNS name from other machines. You will be able to access only using it's IP address");
  }

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
      delay(3000);
      ESP.restart();
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
 
  server.on("/Switch_On", HTTP_OPTIONS, sendCORSHeaders);
  server.on("/Switch_Off", HTTP_OPTIONS, sendCORSHeaders);
  server.on("/Switch_Status", HTTP_OPTIONS, sendCORSHeaders);
 
  //here the list of headers to be recorded
  const char * headerkeys[] = {"User-Agent", "secret", "X-Forwarded-For", "host", "newSwitchPassword"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  server.collectHeaders(headerkeys, headerkeyssize );
  
  server.begin();                          // Start the synchronous web server
  Serial.println("HTTP server started. http://"+espname+".lan");

  MDNS.addService("http", "tcp", 80);

  // ----------- OTA Stuff Begin -----------------//
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

  // Set the defaults to ON_STATE and override them based on the type of connection this is used for
  // ON_STATE for plug and OFF_STATE for switch.
  digitalWrite(RELAY_PIN, ON_STATE);
  digitalWrite(LED_PIN, ON_STATE);

  // Plugs have to be wired in series and switches in parallel
  if (espname.indexOf("Switch") > 0) {
    Serial.println("This is a switch");
    // If you are wiring a switch in parallel set it to OFF_STATE
    digitalWrite(RELAY_PIN, OFF_STATE);
    digitalWrite(LED_PIN, OFF_STATE);
  }
  if (espname.indexOf("Plug") > 0) {
    Serial.println("This is a Plug");
    // If you are wiring a plug in series set it to ON_STATE
    digitalWrite(RELAY_PIN, ON_STATE);
    digitalWrite(LED_PIN, ON_STATE);
  }

  timeClientIST.begin();
  timeClientEDT.begin();
  timeClientEST.begin();

  upSince = getTime();

  EMailSender::EMailMessage message;
  message.subject = espname + " - Setup complete - Rebooted";
  message.message = "Started";
  EMailSender::Response resp = emailSend.send(emailSendTo, message);
  Serial.println("Sending status: ");
  Serial.println(message.message);
  Serial.println(resp.status);
  Serial.println(resp.code);
  Serial.println(resp.desc);
  // -----END SETUP------
}

void loop(void){
  // 1
  server.handleClient();
  // 2
  ArduinoOTA.handle();
  // 3
  MDNS.update();
  // 4 - Send any pending emails, this is not the most accurate logic, some emails may NOT get sent
  if (emailSubject != "") {
    sendEmail(emailSubject, emailContent);
    emailSubject = "";
    emailContent = "";
  }

  Serial.println(getCurrentIST());
  Serial.println(getCurrentEDT());
  Serial.println(getCurrentEST());
  Serial.println("------");
  delay(5000);
}

boolean authenticated(String path) {
  if (server.hasHeader("secret")) { // request HAS secret header
    if (server.header("secret") == switchpassword) {
      if (path.indexOf("Status") <= 0) {
        emailSubject = "Authenticated " + path + " on " + espname + " Accessed";
        emailContent = getXFFIP();
      } else {  // path contains Status - Don't send unncessary emails
        emailSubject = "";
        emailContent = "";
      }
      return true; // authenticated
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

  EMailSender::EMailMessage emessage;
  emessage.subject = subject;
  emessage.message = message;
  EMailSender::Response resp = emailSend.send(emailSendTo, emessage);
  Serial.println("Sending status: ");
  Serial.println(resp.status);
  Serial.println(resp.code);
  Serial.println(resp.desc);
}

void initializeEeprom()
{
//  if (!isAscii(EEPROM.read(0))) {
    Serial.println("Initializing..");
    for(int i=0; i<=510; i++)
    {
      EEPROM.write(i,'\0');
      Serial.println(EEPROM.read(i));
    }
    Serial.print(EEPROM.commit());
    Serial.println("   1=Success 0=Fail");
//  }
}

boolean isDataValid(String i)
{
  Serial.print("Size of string being validated is ");
  Serial.println(i.length());
  if (i.length() == 0) {
    Serial.println("Size of string is 0, Invalid");
    Serial.println("");
    return false;
  }
  for(int t=0; t < i.length(); t++)
  {
    char k = i[t];
    Serial.print(t);
    Serial.print(" : ");
    Serial.print(k);
    Serial.print(" > ");
    int r = i[t];
    Serial.println(r);
    if ((r <=32) || (r >= 127)) {
      Serial.println("At least one non ASCII, Invalid");
      Serial.println("");
      return false;
    }
  }
  Serial.println("All non ASCII, valid");
  Serial.println("");
  return true;
}

String getStartupEmailString() {
  emailString = "<H2>Startup Complete for "+espname+"</H2><BR>";

  emailString += "<hr><h3>Useful URLs</h3>";
  emailString += "Accessible via<BR>";
  emailString += "(.lan)   at <A HREF=http://"+espname+".lan>Home Page</A><BR>";
  emailString += "(.local) at <A HREF=http://"+espname+".local>Home Page</A><BR>";
  emailString += "<A HREF=http://" + WiFi.localIP().toString() + ">http://" + WiFi.localIP().toString() + "</A><BR>";
  emailString += "Connected to SID " + WiFi.SSID() + "<BR>";

  emailString += "<hr><h3>Switch 2 Connected to the relay</h3>";
  emailString += "<h4>Using DNS names .lan</h4>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET http://"+espname+".lan/Switch_On<BR>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET http://"+espname+".lan/Switch_Off<BR>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET http://"+espname+".lan/Switch_Status<BR>";
  emailString += "<h4>Using IP</h4>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET http://"+WiFi.localIP().toString()+"/Switch_On<BR>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET http://"+WiFi.localIP().toString()+"/Switch_Off<BR>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET http://"+WiFi.localIP().toString()+"/Switch_Status<BR>";

  emailString += "<hr><h3>Utility URLs. Do NOT access these from internet</h3>";
  emailString += "<h4>Using DNS names .lan</h4>";
  emailString += "curl                                   -X GET  http://"+espname+".lan/emailSwitchPassword<BR>";
  emailString += "curl -H \"secret: " + switchpassword + "\" -X GET  http://"+espname+".lan/ChangeHostname?hostname=MyNewHostName<BR>";
  emailString += "curl -H \"secret: " + switchpassword + "\" -H \"newSwitchPassword: <b>newSwitchPassword</b>\" -X POST http://"+espname+".lan/SetSwitchPassword<BR>";
  emailString += "<h4>Using IP Address</h4>";
  emailString += "curl                                   -X GET  http://"+WiFi.localIP().toString()+"/emailSwitchPassword<BR>";
  emailString += "curl -H \"secret: " + switchpassword + "\" -X GET  http://"+WiFi.localIP().toString()+"/ChangeHostname?hostname=MyNewHostName<BR>";
  emailString += "curl -H \"secret: " + switchpassword + "\" -H \"newSwitchPassword: <b>newSwitchPassword</b>\" -X POST  http://"+WiFi.localIP().toString()+"/SetSwitchPassword<BR>";

  emailString += "<hr><h3>To restart</h3>";
  emailString += "<h4>.local  .lan  IP Address</h4>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET  http://"+espname+".lan/restart<BR>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET  http://"+espname+".local/restart<BR>";
  emailString += "curl -H \"secret: "+switchpassword+"\" -X GET  http://"+WiFi.localIP().toString()+"/restart<BR>";
  emailString += "<h4>Default Switch State</h4>";
  emailString += "Setting current state of the switch to " + String(OFF_STATE) + "<BR>";

  return emailString;
}

void sendCORSHeaders(){
  server.sendHeader("Access-Control-Allow-Headers", "User-Agent, secret, X-Forwarded-For, host, newSwitchPassword");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS, POST");
  server.sendHeader("Access-Control-Allow-Origin", "null");
  server.sendHeader("Access-Control-Max-Age", "60000");
  server.send(204);
}

String getCurrentIST() {
 timeClientIST.update();
  return timeClientIST.getFormattedTime();
}

String getCurrentEST() {
  timeClientEST.update();
  return timeClientEST.getFormattedTime();
}

String getCurrentEDT() {
  timeClientEDT.update();
  return timeClientEDT.getFormattedTime();
}

String getTime() {
  getCurrentEDT();
}

