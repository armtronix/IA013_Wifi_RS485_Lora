/*
  ModbusRTU ESP8266/ESP32
  Read multiple coils from slave device example

  (c)2019 Alexander Emelianov (a.m.emelianov@gmail.com)
  https://github.com/emelianov/modbus-esp8266
  This code is licensed under the BSD New License. See LICENSE.txt for more info.
*/

String VERSION = " V0.1";
#define DEBUG

//#define WEBOTA
/*debug added for information, change this according your needs. */

#define ESP32

//#define ESP8266

#ifdef ESP8266
 #define RECEIVE_ATTR ICACHE_RAM_ATTR
#elifdef ESP32
 #define RECEIVE_ATTR IRAM_ATTR
#else
 #define RECEIVE_ATTR
#endif

#ifdef DEBUG
 #define Debug(x)    Serial.print(x)
 #define Debugln(x)  Serial.println(x)
 #define Debugf(...) Serial.printf(__VA_ARGS__)
 #define Debugflush  Serial.flush
#else
 #define Debug(x)    {}
 #define Debugln(x)  {}
 #define Debugf(...) {}
 #define Debugflush  {}
#endif

#ifdef ARDUINO_ARCH_ESP32
 #include <WiFi.h>
 #include <WebServer.h>
 #include <ESPmDNS.h>
 #include "SPIFFS.h"
 #include <Update.h>
 #include "WiFiUdp.h"
#else
 #include <ESP8266WiFi.h>
 #include <ESP8266WebServer.h>
 #include <ESP8266mDNS.h>

extern "C" {
 #include "user_interface.h"                                                                                     // Needed for the reset command.
 }
   
#endif

#include <WiFiClient.h>
//#include <EEPROM.h>
//#include <Ticker.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "FS.h"

/*###################################################### Settings declare ######################################################################################################*/ 
String hostName = "Armtronix";                                                                                   // The MQTT ID -> MAC adress will be added to make it kind of unique.
int iotMode = 0;                                                                                                 // IOT mode: 0 = Web control, 1 = MQTT (No const since it can change during runtime).

//select GPIO's
#ifdef ARDUINO_ARCH_ESP32
#else
#endif

#define UPDATE_TIME            120000                                                                            // 120 seconds //added on 14/03/19 by naren.
#define RESTARTDELAY           3                                                                                 // Minimal time in sec for button press to reset.
#define HUMANPRESSDELAY        50                                                                                // The delay in ms untill the press should be handled as a normal push by human. Button debounce. !!! Needs to be less than RESTARTDELAY & RESETDELAY!!!
#define RESETDELAY             20                                                                                // Minimal time in sec for button press to reset all settings and boot to config mode.

//##### Object instances #####
MDNSResponder mdns;

#ifdef ARDUINO_ARCH_ESP32
 WebServer server(80);
#else
 ESP8266WebServer server(80);
#endif

WiFiClient wifiClient;
PubSubClient mqttClient;
//Ticker btn_timer;
//Ticker otaTickLoop;

/*################################################################# Flags ######################################################################################################*/ 
/*They are needed because the loop needs to continue and cant wait for long tasks!. */
int rstNeed = 0;                                                                                                 // Restart needed to apply new settings.
int toPub = 0;                                                                                                   // Determine if state should be published.
int configToClear = 0;                                                                                           // Determine if config should be cleared.
int otaFlag = 0;
boolean inApMode = 0;

/*############################################################### Global variables ##############################################################################################*/ 
int webtypeGlob;
int otaCount = 300;                                                                                              // Imeout in sec for OTA mode.
unsigned long count = 0;                                                                                         // Button press time counter.
String st;                                                                                                       // WiFi Stations HTML list.

unsigned long CTIME;
unsigned long PTIME = 0;
int Delay = 10000;
String serialReceived = "";
String serialReceived_buf = "";

String mqtt_user = "";
String mqtt_passwd = "";
char* mqtt_will_msg = " disconnected";
String javaScript, XML;

char buf[40];                                                                                                    // For MQTT data recieve.
char* host;                                                                                                      // The DNS hostname.

/* To be read from Config file. */
String esid = "";
String epass = "";
String pubTopic;
String subTopic;
String mqttServer = "";
const char* otaServerIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
String hostsaved;
String pubTopicV         = pubTopic + "/Voltage";
String pubTopicCurrent   = pubTopic + "/Current";
String pubTopicP         = pubTopic + "/Power";
String pubTopicWh        = pubTopic + "/Wh";
String pubTopicF         = pubTopic + "/Frequency";
String pubTopicPF        = pubTopic + "/PF";

char mqtt_buff_Char_1[20];
String i2c_output_Str_1 = "";
String display_buffer_lcd_1 = "";

char mqtt_buff_Char_LCD[24];
String i2c_output_Str_LCD = "";
String display_buffer_lcd = "";

String state;

#ifdef ARDUINO_ARCH_ESP32
 #include "esp_system.h"
#else
#endif

/* Define number of pieces. */
const int numberOfPieces = 28;
String pieces[numberOfPieces];

/* This will be the buffered string from Serial.read()
   up until you hit a \n
   Should look something like "123,456,789,0"
 */
String input = "";

/* Keep track of current position in array. */
int counter = 0;

/* Keep track of the last comma so we know where to start the substring. */
int lastIndex = 0;

float _wattsT, _wattsR, _wattsY, _wattsB, _varT, _varR, _varY, _varB, _pfA, _pfR, _pfY, _pfB, _vaT, _vaR, _vaY, _vaB, _vll, _vry, _vyb, _vbr, _vln, _vrphase, _vyphase, _vbphase, _currentT, _currentR, _currentY, _currentB, _frequency, _power;

/*##########################################################################################################################################################################################*/ 
void setup() 
 {
   Serial.begin(115200);
   delay(100);
   Serial.println("waiting.... ");
   Serial.print("Software version: ");
   Serial.println(VERSION);
   // WiFi.printDiag(Serial);
   // WiFi.disconnect();
   delay(100);

   // btn_timer.attach(0.05, btn_handle);
   Debugln("DEBUG: Entering loadConfig()");
   if(!SPIFFS.begin()) 
    {
      Serial.println("Failed to mount file system");
    }

   uint8_t mac[6];
   #ifdef ARDUINO_ARCH_ESP32
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
   #else
    WiFi.macAddress(mac);
   #endif
   hostName += "-";
   hostName += macToStr(mac);
   String hostTemp = hostName;
   hostTemp.replace(":", "-");
   host = (char*) hostTemp.c_str();
   hostsaved = hostTemp;
   loadConfig();
   // loadConfigOld();
   Debugln("DEBUG: loadConfig() passed");

/* Connect to WiFi network. */
   Debugln("DEBUG: Entering initWiFi()");
   initWiFi();
   Debugln("DEBUG: initWiFi() passed");
   Debug("iotMode:");
   Debugln(iotMode);
   Debug("webtypeGlob:");
   Debugln(webtypeGlob);
   Debug("otaFlag:");
   Debugln(otaFlag);
   pubTopicV = pubTopic + "/Voltage";
   pubTopicCurrent = pubTopic + "/Current";
   pubTopicP = pubTopic + "/Power";
   pubTopicWh = pubTopic + "/Wh";
   pubTopicF = pubTopic + "/Frequency";
   pubTopicPF = pubTopic + "/PF";

   Serial.print("pubTopicV ");
   Serial.println(pubTopicV);
   Serial.print("pubTopicCurrent ");
   Serial.println(pubTopicCurrent);
   Serial.print("pubTopicP ");
   Serial.println(pubTopicP);
   Serial.print("pubTopicWh ");
   Serial.println(pubTopicWh);
   Serial.print("pubTopicF ");
   Serial.println(pubTopicF);
   Serial.print("pubTopicPF ");
   Serial.println(pubTopicPF);
    
   Debugln("DEBUG: Starting the main loop");
 }

/*############################################################################################################################################################################################*/ 
void loop()
 {
   static unsigned long last = millis();
   CTIME  = millis();                                                                                            // Added on 14/03/19 by naren.
   unsigned long currentMillis = millis();

   if(CTIME - PTIME >= Delay)
    {    
      // toPub = 1;
      PTIME = CTIME;
    }

   //handle_rs485();

   if(Serial.available() > 0) 
    {
/* Read the first byte and store it as a char. */
      char ch = Serial.read();
/* Do all the processing here since this is the end of a line. */
      if(ch == '\n') 
       {
         for(int i = 0; i < input.length(); i++) 
          {
/* Loop through each character and check if it's a comma. */
            if(input.substring(i, i + 1) == ",") 
             {
/* Grab the piece from the last index up to the current position and store it. */
               pieces[counter] = input.substring(lastIndex, i);
               // Serial.println(pieces[counter]);
/* Update the last position and add 1, so it starts from the next character. */
               lastIndex = i + 1;
/* Increase the position in the array that we store into. */
               counter++;
             }
/* If we're at the end of the string (no more commas to stop us). */
            if(i == input.length() - 1) 
             {
/* Grab the last part of the string from the lastIndex to the end. */
               pieces[counter] = input.substring(lastIndex, i);
               // Serial.println(pieces[counter]);
             } 
          }
/* Clear out string and counters to get ready for the next incoming string. */
         input = "";
         counter = 0;
         lastIndex = 0;
       }
      else 
       {
/* If we havent reached a newline character yet, add the current character to the string. */
         input += ch;
       }    
    }

   if(pieces[0] == "V_C_P_E_F_PF:")
    {
      Serial.println("Data received via uart");            
      _wattsT = pieces[1].toFloat();
      _wattsR = pieces[2].toFloat();
      _wattsY = pieces[3].toFloat();
      _wattsB = pieces[4].toFloat();
      _varT = pieces[5].toFloat();
      _varR = pieces[6].toFloat();
      _varY = pieces[7].toFloat();
      _varB = pieces[8].toFloat();
      _pfA = pieces[9].toFloat();
      _pfR = pieces[10].toFloat();
      _pfY = pieces[11].toFloat();
      _pfB = pieces[12].toFloat();
      _vaT = pieces[13].toFloat();
      _vaR = pieces[14].toFloat();
      _vaY = pieces[15].toFloat();
      _vaB = pieces[16].toFloat();
      _vll = pieces[17].toFloat();
      _vry = pieces[18].toFloat();
      _vyb = pieces[19].toFloat();
      _vbr = pieces[20].toFloat();
      _vln = pieces[21].toFloat();
      _vrphase = pieces[22].toFloat();
      _vyphase = pieces[23].toFloat();
      _vbphase = pieces[24].toFloat();
      _currentT = pieces[25].toFloat();
      _currentR = pieces[26].toFloat();
      _currentY = pieces[27].toFloat();
      _currentB = pieces[28].toFloat();
      _frequency = pieces[29].toFloat();
      _power = pieces[30].toFloat();
      pieces[0] = " ";

      state = "{ \"state\" : { \"reported\" : { \"Watts_Total\" : " + String(_wattsT) + ", " + "\"Watts_R_phase\" : " + String(_wattsR) + ", \"Watts_Y_phase\" : "
              + String(_wattsY) + ", \"Watts_B_phase\" : " + String(_wattsB) + ", \"VAR_Total\" : " + String(_varT) + ", \"VAR_R_phase\" : " + String(_varR) 
              + ", \"VAR_Y_phase\" : " + String(_varY) + ", \"VAR_B_phase\" : " + String(_varB) + ", \"PF_Ave\" : " + String(_pfA) + ", \"PF_R_phase\" : " 
              + String(_pfR) + ", \"PF_Y_phase\" : " + String(_pfY) + ", \"PF_B_phase\" : " + String(_pfB) + ", \"VA_Total\" : " + String(_vaT) 
              + ", \"VA_R_phase\" : " + String(_vaR) + ", \"VA_Y_phase\" : " + String(_vaY) + ", \"VA_B_phase\" : " + String(_vaB) + ", \"VLL_avg\" : " 
              + String(_vll) + ", \"Vry_phase\" : " + String(_vry) + ", \"Vyb_phase\" : " + String(_vyb) + ", \"Vbr_phase\" : " + String(_vbr) 
              + ", \"VLN_avg\" : " + String(_vln) + ", \"V_R_phase\" : " + String(_vrphase) + ", \"V_Y_phase\" : " + String(_vyphase) + ", \"V_B_phase\" : "
              + String(_vbphase) + ", \"Current_Total\" : " + String(_currentT) + ", \"Current_R_phase\" : " + String(_currentR) + ", \"Current_Y_phase\" : "
              + String(_currentY) + ", \"Current_B_phase\" : " + String(_currentB) + ", \"Freq\" : " + String(_frequency) + ", \"Wh_Received\" : " + String(_power) + "}}}" ;
              
      //state = "V:" + String(_voltage) + "volts, C:" + String(_current) + "Amps, W:" + String(watts) + "watts, Wh:" + String(energy) + "Wh, F:" + String(frequency) + "Hz, PF:" + String(pf);
      toPub = 1;
    }
    
   // Debugln("DEBUG: loop() begin");
   if(configToClear == 1)
    {
      // Debugln("DEBUG: loop() clear config flag set!");
      clearConfig() ? Serial.println("Config cleared!") : Serial.println("Config could not be cleared");
      delay(1000);
      #ifdef ARDUINO_ARCH_ESP32
       esp_restart();
      #else
       ESP.reset();
      #endif
    }
   // Debugln("DEBUG: config reset check passed");
   if(WiFi.status() == WL_CONNECTED && otaFlag)
    {
      if(otaCount <= 1)
       {
         Serial.println("OTA mode time out. Reset!");
         setOtaFlag(0);
         #ifdef ARDUINO_ARCH_ESP32
          esp_restart();
         #else
          ESP.reset();
         #endif
         delay(100);
       }
      server.handleClient();
      delay(1);
    }
   else if(WiFi.status() == WL_CONNECTED || webtypeGlob == 1)
    {
      // Debugln("DEBUG: loop - webtype =1");
      // Debugln("DEBUG: loop() wifi connected & webServer ");
      if(iotMode == 0 || webtypeGlob == 1)
       {
         // Debugln("DEBUG: loop() Web mode requesthandling ");
         server.handleClient();
         delay(1);
         if(esid != "" && WiFi.status() != WL_CONNECTED)                                                         // Wifi reconnect part.
          {
            if((millis() - last) > UPDATE_TIME)                                                                  // Added on 14/03/19 by naren.
             {
               Scan_Wifi_Networks();
               last = millis();                                                                                  // Added on 14/03/19 by naren.
             }
          }
       }
      else if(iotMode == 1 && webtypeGlob != 1 && otaFlag != 1)
       {
         // Debugln("DEBUG: loop() MQTT mode requesthandling ");
         if(!connectMQTT()) 
          {
            delay(200);
          }
         if(mqttClient.connected()) 
          {
            // Debugln("mqtt handler");
            mqtt_handler();
          } 
         else 
          {
            Debugln("mqtt Not connected!");
          }
       }
    }
   else
    {
      Debugln("DEBUG: loop - WiFi not connected function initWifi()");
      delay(100);                                                                                                // Eariler it was 1000 changed on 14/03/19.
      initWiFi();                                                                                                // Try to connect again.
    }
   delay(1);
   // Debugln("DEBUG: loop() end");
 }
