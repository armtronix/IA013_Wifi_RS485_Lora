
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
//debug added for information, change this according your needs


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


#include <ModbusRTU.h>
#ifdef ESP8266
#include <SoftwareSerial.h>
SoftwareSerial S(D1, D2, false, 256);
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
#include "user_interface.h" //Needed for the reset command
}
#endif


#include <WiFiClient.h>
//#include <EEPROM.h>
//#include <Ticker.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "FS.h"





//***** Settings declare *********************************************************************************************************
String hostName = "Armtronix"; //The MQTT ID -> MAC adress will be added to make it kind of unique
int iotMode = 0; //IOT mode: 0 = Web control, 1 = MQTT (No const since it can change during runtime)
//select GPIO's
#ifdef ARDUINO_ARCH_ESP32
#define RS485_RX 21
#define RS485_TX 22
#define RS485_DE 13


#else

#endif


#define UPDATE_TIME   120000 //120 seconds //added on 14/03/19 by naren
#define RESTARTDELAY 3 //minimal time in sec for button press to reset
#define HUMANPRESSDELAY 50 // the delay in ms untill the press should be handled as a normal push by human. Button debounce. !!! Needs to be less than RESTARTDELAY & RESETDELAY!!!
#define RESETDELAY 20 //Minimal time in sec for button press to reset all settings and boot to config mode

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

//##### Flags ##### They are needed because the loop needs to continue and cant wait for long tasks!
int rstNeed = 0; // Restart needed to apply new settings
int toPub = 0; // determine if state should be published.
int configToClear = 0; // determine if config should be cleared.
int otaFlag = 0;
boolean inApMode = 0;

//##### Global vars #####
int webtypeGlob;
int otaCount = 300; //imeout in sec for OTA mode
unsigned long count = 0; //Button press time counter
String st; //WiFi Stations HTML list

unsigned long CTIME;
unsigned long PTIME = 0;
int Delay = 10000;


String mqtt_user = "";
String mqtt_passwd = "";
char* mqtt_will_msg = " disconnected";
String javaScript, XML;

char buf[40]; //For MQTT data recieve
char* host; //The DNS hostname
//To be read from Config file
String esid = "";
String epass = "";
String pubTopic;
String subTopic;
String mqttServer = "";
const char* otaServerIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
String hostsaved;
String pubTopicV = pubTopic + "/Voltage";
String pubTopicCurrent = pubTopic + "/Current";
String pubTopicP = pubTopic + "/Power";
String pubTopicWh = pubTopic + "/Wh";
String pubTopicF = pubTopic + "/Frequency";
String pubTopicPF = pubTopic + "/PF";


char mqtt_buff_Char_1[20];
String i2c_output_Str_1 = "";
String display_buffer_lcd_1 = "";

char mqtt_buff_Char_LCD[24];
String i2c_output_Str_LCD = "";
String display_buffer_lcd = "";

String state;

#ifdef ARDUINO_ARCH_ESP32
#include "esp_system.h"
//String getMacAddress() {
//  uint8_t baseMac[6];
//  // Get MAC address for WiFi station
//  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
//  char baseMacChr[18] = {0};
//  sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
//  return String(baseMacChr);
//}
#else
#endif





//-----------------------------MODBUS--------------------------------


ModbusRTU mb;

#define RANGE 10
#define START_ADDRESS 0x0000


float current, voltage, watts, energy, frequency, pf;

uint16_t coils[RANGE];

//callback function
bool cbWrite(Modbus::ResultCode event, uint16_t transactionId, void* data) {
  Serial.printf_P("Request result: 0x%02X, Mem: %d\n", event, ESP.getFreeHeap());
  //  Serial.println(&data);
  //Serial.println();
  return true;
}


float hex_to_floatingP(uint16_t Data2, uint16_t Data1)
{
  union
  {
    uint32_t i;
    float f;
  } _data;

  String _tempdata;

  _tempdata = String(Data1, HEX) + String(Data2, HEX);
  _data.i = strtoul((char*)_tempdata.c_str(), NULL, 16);
  //Serial.print("Converted to ");
  //Serial.println(_data.f, 4);
  return (_data.f);
}


uint32_t uint_Add16(uint16_t high, uint16_t low )
{
  uint32_t num = (((unsigned long)high << 16) | low);
  return num;
}



void handle_rs485()
{
  if (!mb.slave()) {
    mb.readIreg(1, START_ADDRESS, coils, RANGE, cbWrite); // Last '3' is device id


    //    current = (hex_to_floatingP(coils[50], coils[51]));
    //    Serial.print("curr:");
    //    Serial.println(current);
    //
    //
    //    voltage = (hex_to_floatingP(coils[42], coils[43]));
    //    Serial.print("volts:");
    //    Serial.println(voltage);
    //
    //    watts = (hex_to_floatingP(coils[0], coils[1]));
    //    Serial.print("watts:");
    //    Serial.println(watts);
    //    state = "V:"+String(voltage)+"volts, C:"+String(current)+"Amps, W:"+String(watts)+"watts";


    Serial.print("Voltage:");
    voltage = coils[0] / 10.0;
    Serial.println(voltage);
    Serial.print("Current:");
    current = uint_Add16(coils[2], coils[1]) / 1000.0;
    Serial.println(current);
    Serial.print("Power:");
    watts = uint_Add16(coils[4], coils[3]) / 10.0;
    Serial.println(watts);
    Serial.print("Energy:");
    energy = uint_Add16(coils[6], coils[5]) / 1000.0;
    Serial.println(energy);
    Serial.print("Frequency:");
    frequency = coils[7] / 10.0;
    Serial.println(frequency);
    Serial.print("PF:");
    pf = coils[8] / 100.0;
    Serial.println(pf);

    state = "V:" + String(voltage) + "volts, C:" + String(current) + "Amps, W:" + String(watts) + "watts, Wh:" + String(energy) + "Wh, F:" + String(frequency) + "Hz, PF:" + String(pf);
    //Serial.println(state);

  }

  mb.task();

}




//--------------------------------------------------------------



/* commented by naren , esp32 fails if enabled on 13/03/19
  #ifdef ARDUINO_ARCH_ESP32
  static volatile uint16_t intTriggeredCount=0;
  void IRAM_ATTR isr(){  //IRAM_ATTR tells the complier, that this code Must always be in the
  // ESP32's IRAM, the limited 128k IRAM.  use it sparingly.
  intTriggeredCount++;
  //input_int;
  }
  #else
  #endif
*/

//-------------- void's -------------------------------------------------------------------------------------






void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("waiting.... ");
  Serial.print("Software version: ");
  Serial.println(VERSION);
  // WiFi.printDiag(Serial);
  //WiFi.disconnect();
  delay(100);

#ifdef ESP8266
  S.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
  mb.begin(&S);
#else
  Serial1.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
  mb.begin(&Serial1, RS485_DE);
#endif

  mb.master();
#ifdef ARDUINO_ARCH_ESP32

#else


#endif

  //  btn_timer.attach(0.05, btn_handle);
  Debugln("DEBUG: Entering loadConfig()");
  if (!SPIFFS.begin()) {
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
  //loadConfigOld();
  Debugln("DEBUG: loadConfig() passed");

  // Connect to WiFi network
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
  Debugln("DEBUG: Starting the main loop");

}







//######################################################################################################################
void loop()
{
  static unsigned long last = millis();
  CTIME  = millis();//added on 14/03/19 by naren
  unsigned long currentMillis = millis();



  if (CTIME - PTIME >= Delay)
  {
    toPub = 1;
    PTIME = CTIME;
  }



  handle_rs485();






  //Debugln("DEBUG: loop() begin");
  if (configToClear == 1)
  {
    //Debugln("DEBUG: loop() clear config flag set!");
    clearConfig() ? Serial.println("Config cleared!") : Serial.println("Config could not be cleared");
    delay(1000);
#ifdef ARDUINO_ARCH_ESP32
    esp_restart();
#else
    ESP.reset();
#endif
  }
  //Debugln("DEBUG: config reset check passed");
  if (WiFi.status() == WL_CONNECTED && otaFlag)
  {
    if (otaCount <= 1)
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
  else if (WiFi.status() == WL_CONNECTED || webtypeGlob == 1)
  {
    //Debugln("DEBUG: loop - webtype =1");
    //Debugln("DEBUG: loop() wifi connected & webServer ");
    if (iotMode == 0 || webtypeGlob == 1)
    {
      //Debugln("DEBUG: loop() Web mode requesthandling ");
      server.handleClient();
      delay(1);
      if (esid != "" && WiFi.status() != WL_CONNECTED) //wifi reconnect part
      {
        if ((millis() - last) > UPDATE_TIME) //added on 14/03/19 by naren
        {
          Scan_Wifi_Networks();
          last = millis(); //added on 14/03/19 by naren
        }
      }
    }
    else if (iotMode == 1 && webtypeGlob != 1 && otaFlag != 1)
    {
      //Debugln("DEBUG: loop() MQTT mode requesthandling ");
      if (!connectMQTT()) {
        delay(200);
      }
      if (mqttClient.connected()) {
        //Debugln("mqtt handler");
        mqtt_handler();
      } else {
        Debugln("mqtt Not connected!");
      }
    }
  }
  else
  {
    Debugln("DEBUG: loop - WiFi not connected function initWifi()");
    delay(100);//eariler it was 1000 changedon 14/03/19
    initWiFi(); //Try to connect again
  }
  delay(1);
  //Debugln("DEBUG: loop() end");
}
