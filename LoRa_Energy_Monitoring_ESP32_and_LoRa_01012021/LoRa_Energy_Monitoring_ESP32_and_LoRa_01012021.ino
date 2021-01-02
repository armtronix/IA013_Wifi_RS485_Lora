/*
  ModbusRTU ESP8266/ESP32
  Read multiple coils from slave device example

  (c)2019 Alexander Emelianov (a.m.emelianov@gmail.com)
  https://github.com/emelianov/modbus-esp8266
  This code is licensed under the BSD New License. See LICENSE.txt for more info.
*/

String VERSION = " V0.1";
#define DEBUG

#define LORA_SCK         05
#define LORA_MOSI        27
#define LORA_MISO        19

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

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <ModbusMaster.h>

#define UART_BAUD_RATE           115200
#define RS485_RX                 21
#define RS485_TX                 22
#define MAX485_DE                13
#define MAX485_RE_NEG            14
#define RS485BAUDRATE            9600
#define RS485_REREAD             2
#define RS485_ERROR_REREAD       1
#define txInterval               10
#define RANGE                    60                                                                              // Range of the register addresses we want to read from the energy meter.
#define START_ADDRESS            100                                                                             // Starting address of the energy meter reading(its 100 for WC4400).

#define WATTS_TOTAL              0
#define WATTS_R_PHASE            1
#define WATTS_Y_PHASE            2
#define WATTS_B_PHASE            3
#define VAR_TOTAL                4
#define VAR_R_PHASE              5
#define VAR_Y_PHASE              6
#define VAR_B_PHASE              7
#define PF_AVERAGE               8
#define PF_R_PHASE               9
#define PF_Y_PHASE               10
#define PF_B_PHASE               11
#define VA_TOTAL                 12
#define VA_R_PHASE               13
#define VA_Y_PHASE               14
#define VA_B_PHASE               15
#define VLL_AVERAGE              16
#define VRY_PHASE                17
#define VYB_PHASE                18
#define VBR_PHASE                19
#define VLN_AVERAGE              20
#define V_R_PHASE                21
#define V_Y_PHASE                22
#define V_B_PHASE                23
#define CURRENT_TOTAL            24
#define CURRENT_R_PHASE          25
#define CURRENT_Y_PHASE          26
#define CURRENT_B_PHASE          27
#define FREQUENCY                28
#define WH_RECEIVED              29 

#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "FS.h"

/*################################################################### MODBUS ###################################################################################################*/
#include <ModbusMaster.h>
uint8_t ucG_Result;
uint16_t data[75];

union
{
  uint32_t i;
  float f;
} _data;

ModbusMaster node;
float frgG_DataArray[30];
String strG_Errorcode_Check;

/*################################################################### Rs485 SERIAL SETUP #######################################################################################*/
void F_Rs485SerialSetup()
 {
   pinMode(MAX485_DE, OUTPUT);
   pinMode(MAX485_RE_NEG, OUTPUT);
   digitalWrite(MAX485_DE, 0);
   digitalWrite(MAX485_RE_NEG, 0);
   Serial1.begin(RS485BAUDRATE, SERIAL_8N1, RS485_RX, RS485_TX);
   node.begin(1, Serial1);

/*Callbacks allow us to configure the RS485 transceiver correctly*/
   node.preTransmission(preTransmission);
   node.postTransmission(postTransmission);
   delay(100);
 }

/*######################################################################## Settings declare ####################################################################################*/ 
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

/*################################################################### Object instances #########################################################################################*/
MDNSResponder mdns;

#ifdef ARDUINO_ARCH_ESP32
 WebServer server(80);
#else
 ESP8266WebServer server(80);
#endif

WiFiClient wifiClient;
PubSubClient mqttClient;

/*################################################################# Flags ######################################################################################################*/ 
/*They are needed because the loop needs to continue and cant wait for long tasks!. */
int rstNeed = 0;                                                                                                 // Restart needed to apply new settings.
int toPub = 0;                                                                                                   // Determine if state should be published.
int configToClear = 0;                                                                                           // Determine if config should be cleared.
int otaFlag = 0;
boolean inApMode = 0;

/*############################################################### Global variables #############################################################################################*/ 
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

/*################################################################### LoRaWAN ##################################################################################################*/
/* LoRaWAN NwkSKey, network session key.
   This is the default Semtech key, which is used by the early prototype TTN  network.
 */
static const PROGMEM u1_t NWKSKEY[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* LoRaWAN AppSKey, application session key.
   This is the default Semtech key, which is used by the early prototype TTN network.
 */
static const u1_t PROGMEM APPSKEY[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* LoRaWAN end-device address (DevAddr) */
static const u4_t DEVADDR = 0x00000000 ;                                                                         // Change this address for every node!

/* These callbacks are only used in over-the-air activation, so they are
   left empty here (we cannot leave them out completely unless
   DISABLE_JOIN is set in config.h, otherwise the linker will complain).
 */
 
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

byte myData[49];
uint8_t meterId = 1;


static osjob_t sendjob;

/* Schedule TX every this many seconds (might become longer due to duty cycle limitations). */
const unsigned TX_INTERVAL = 30;

/* Pin mapping. */
const lmic_pinmap lmic_pins = {
  .nss = 18,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 23,
  .dio = {26, 33, 32},
};

/*##############################################################################################################################################################################*/ 
void setup() 
 {
   SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI);
   Serial.begin(UART_BAUD_RATE);
   delay(100);
   Serial.println("waiting.... ");
   Serial.print("Software version: ");
   Serial.println(VERSION);
   delay(100);
/* Init in receive mode. */
   F_Rs485SerialSetup();

   #ifdef VCC_ENABLE
/* For Pinoccio Scout boards. */
     pinMode(VCC_ENABLE, OUTPUT);
     digitalWrite(VCC_ENABLE, HIGH);
     delay(1000);
   #endif

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
   
   os_init();                                                                                                    // LMIC init.

/* Reset the MAC state. Session and pending data transfers will be discarded. */
   LMIC_reset();
   Serial.print("pubTopicPF1 ");

/* Set static session parameters. Instead of dynamically establishing a session
   by joining the network, precomputed session parameters are be provided.
 */
   #ifdef PROGMEM
/* On AVR, these values are stored in flash and only copied to RAM once. 
   Copy them to a temporary buffer here, LMIC_setSession will copy them into a buffer of its own again.
 */
     uint8_t appskey[sizeof(APPSKEY)];
     uint8_t nwkskey[sizeof(NWKSKEY)];
     memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
     memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
     LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
   #else
/* If not running an AVR with PROGMEM, just use the arrays directly. */
     LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
   #endif

   #if defined(CFG_eu868)

/* Set up the channels used by the Things Network, which corresponds to the defaults of most gateways. 
   Without this, only three base channels from the LoRaWAN specification are used, which certainly works, 
   so it is good for debugging, but can overload those frequencies, 
   so be sure to configure the full frequency range of your network here (unless your network autoconfigures them).
   Setting up channels should happen after LMIC_setSession, as that configures the minimal channel set.
   NA-US channels 0-71 are configured automatically.
 */
     LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                              // g-band
     LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);                              // g-band
     LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                              // g-band
     LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                              // g-band
     LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                              // g-band
     LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                              // g-band
     LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                              // g-band
     LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                              // g-band
     LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);                              // g2-band

/* TTN defines an additional channel at 869.525Mhz using SF9 for class B devices' ping slots. 
   LMIC does not have an easy way to define set this frequency and support for class B is spotty and untested, 
   so this frequency is not configured here.
 */
   #elif defined(CFG_us915)

/* NA-US channels 0-71 are configured automatically but only one group of 8 should (sub band) should be active.
   TTN recommends the second sub band, 1 in a zero based count.
   https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
 */
     LMIC_selectSubBand(1);
   #endif

/* Disable link check validation. */
   LMIC_setLinkCheckMode(0);

/* TTN uses SF9 for its RX2 window. */
   LMIC.dn2Dr = DR_SF9;

/* Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library). */
   LMIC_setDrTxpow(DR_SF7, 14);

/* Start job. */
   //Serial.println(F("do_send1"));
   do_send(&sendjob);
 
 }

/*##############################################################################################################################################################################*/ 
void loop()
 {
   static unsigned long last = millis();
   CTIME  = millis();                                                                                          
   unsigned long currentMillis = millis();

   if(CTIME - PTIME >= Delay)
    {    
      // toPub = 1;
      PTIME = CTIME;
    }

   if(configToClear == 1)
    {
      clearConfig() ? Serial.println("Config cleared!") : Serial.println("Config could not be cleared");
      delay(1000);
      #ifdef ARDUINO_ARCH_ESP32
       esp_restart();
      #else
       ESP.reset();
      #endif
    }
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
      if(iotMode == 0 || webtypeGlob == 1)
       {
         server.handleClient();
         delay(1);
         if(esid != "" && WiFi.status() != WL_CONNECTED)                                                       // Wifi reconnect part.
          {
            if((millis() - last) > UPDATE_TIME)                                                                
             {
               Scan_Wifi_Networks();
               last = millis();                                                                                
             }
          }
       }
      else if(iotMode == 1 && webtypeGlob != 1 && otaFlag != 1)
       {
         if(!connectMQTT()) 
          {
            delay(200);
          }
         if(mqttClient.connected()) 
          {
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
      delay(100);                                                                                              // Eariler it was 1000 changed on 14/03/19.
      initWiFi();                                                                                              // Try to connect again.
    }
   delay(1);

   os_runloop_once();
 }
