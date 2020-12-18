/* *******************************************************************************************************************************************************************************
   Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman

   Permission is hereby granted, free of charge, to anyone
   obtaining a copy of this document and accompanying files,
   to do whatever they want with them without any restriction,
   including, but not limited to, copying, modification and redistribution.
   NO WARRANTY OF ANY KIND IS PROVIDED.

   This example sends a valid LoRaWAN packet with payload "Hello,
   world!", using frequency and encryption settings matching those of
   the The Things Network.

   This uses ABP (Activation-by-personalisation), where a DevAddr and
   Session keys are preconfigured (unlike OTAA, where a DevEUI and
   application key is configured, while the DevAddr and session keys are
   assigned/generated in the over-the-air-activation procedure).

   Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in
   g1, 0.1% in g2), but not the TTN fair usage policy (which is probably
   violated by this sketch when left running for longer)!

   To use this sketch, first register your application and device with
   the things network, to set or generate a DevAddr, NwkSKey and
   AppSKey. Each device should have their own unique values for these
   fields.

   Do not forget to define the radio type correctly in config.h.
 ****************************************************************************************************************************************************************************** */

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <ModbusMaster.h>

#define UART_BAUD_RATE           115200
#define MAX_RANGE                30
#define MAX_SLAVES               2
#define RS485_RX                 PA3
#define RS485_TX                 PA2
#define MAX485_DE                PB0
#define MAX485_RE_NEG            PB1
#define RS485BAUDRATE            9600
#define RS485_REREAD             2
#define RS485_ERROR_REREAD       1
#define txInterval               10
#define RANGE                    60                                                                   // Range of the register addresses we want to read from the energy meter.
#define START_ADDRESS            100                                                                  // Starting address of the energy meter reading(its 100 for WC4400).

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

/*################################################################### MODBUS ###################################################################################################*/
#include <ModbusMaster.h>
uint8_t ucG_Result;
uint16_t data[75];
String state;

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
  Serial1.begin(RS485BAUDRATE, SERIAL_8N1);
  node.begin(1, Serial1);

/*Callbacks allow us to configure the RS485 transceiver correctly*/
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  delay(100);
 }

/* LoRaWAN NwkSKey, network session key.
   This is the default Semtech key, which is used by the early prototype TTN  network.
 */
static const PROGMEM u1_t NWKSKEY[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* LoRaWAN AppSKey, application session key.
   This is the default Semtech key, which is used by the early prototype TTN network.
 */
static const u1_t PROGMEM APPSKEY[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* LoRaWAN end-device address (DevAddr) */
static const u4_t DEVADDR = 0x00000000 ;                                                              // <-- Change this address for every node!

/* These callbacks are only used in over-the-air activation, so they are
   left empty here (we cannot leave them out completely unless
   DISABLE_JOIN is set in config.h, otherwise the linker will complain).
 */
 
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

byte myData[49];
//uint8_t voltage[2];
uint8_t meterId = 1;

static osjob_t sendjob;

/* Schedule TX every this many seconds (might become longer due to duty
   cycle limitations).
 */
const unsigned TX_INTERVAL = 30;

/* Pin mapping. */
const lmic_pinmap lmic_pins = {
  .nss = PA4,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = PC13,
  .dio = {PA1, PB13, PB12},
};

void onEvent (ev_t ev) 
 {
   switch (ev) 
    {
      case EV_SCAN_TIMEOUT:
           Serial.println(F("EV_SCAN_TIMEOUT"));
           break;
      case EV_BEACON_FOUND:
           Serial.println(F("EV_BEACON_FOUND"));
           break;
      case EV_BEACON_MISSED:
           Serial.println(F("EV_BEACON_MISSED"));
           break;
      case EV_BEACON_TRACKED:
           Serial.println(F("EV_BEACON_TRACKED"));
           break;
      case EV_JOINING:
           Serial.println(F("EV_JOINING"));
           break;
      case EV_JOINED:
           Serial.println(F("EV_JOINED"));
           break;
      case EV_RFU1:
           Serial.println(F("EV_RFU1"));
           break;
      case EV_JOIN_FAILED:
           Serial.println(F("EV_JOIN_FAILED"));
           break;
      case EV_REJOIN_FAILED:
           Serial.println(F("EV_REJOIN_FAILED"));
           break;
      case EV_TXCOMPLETE:
           //Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
           if(LMIC.txrxFlags & TXRX_ACK)
            Serial.println(F("Received ack"));
           if(LMIC.dataLen) 
            {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
/* Schedule next transmission. */
           os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
           break;
      case EV_LOST_TSYNC:
           Serial.println(F("EV_LOST_TSYNC"));
           break;
      case EV_RESET:
           Serial.println(F("EV_RESET"));
           break;
      case EV_RXCOMPLETE:
/* Data received in ping slot. */
           Serial.println(F("EV_RXCOMPLETE"));
           break;
      case EV_LINK_DEAD:
           Serial.println(F("EV_LINK_DEAD"));
           break;
      case EV_LINK_ALIVE:
           Serial.println(F("EV_LINK_ALIVE"));
           break;
      default:
           Serial.println(F("Unknown event"));
           break;
    }
 }

void do_send(osjob_t* j) 
 {
/* Check if there is not a current TX/RX job running. */
   if(LMIC.opmode & OP_TXRXPEND) 
    {
      Serial.println(F("OP_TXRXPEND, not sending"));
    } 
   else 
    {
      handle_rs485(START_ADDRESS , RANGE);
      delay(1000);
      float watts_total = frgG_DataArray[WATTS_TOTAL];
      float watts_r_phase = frgG_DataArray[WATTS_R_PHASE];
      float watts_y_phase = frgG_DataArray[WATTS_Y_PHASE];
      float watts_b_phase = frgG_DataArray[WATTS_B_PHASE];
      float var_total = frgG_DataArray[VAR_TOTAL];
      float var_r_phase = frgG_DataArray[VAR_R_PHASE];
      float var_y_phase = frgG_DataArray[VAR_Y_PHASE];
      float var_b_phase = frgG_DataArray[VAR_B_PHASE];
      float pf_average = frgG_DataArray[PF_AVERAGE];
      float pf_r_phase = frgG_DataArray[PF_R_PHASE];
      float pf_y_phase = frgG_DataArray[PF_Y_PHASE];
      float pf_b_phase = frgG_DataArray[PF_B_PHASE];
      float va_total = frgG_DataArray[VA_TOTAL];
      float va_r_phase = frgG_DataArray[VA_R_PHASE];
      float va_y_phase = frgG_DataArray[VA_Y_PHASE];
      float va_b_phase = frgG_DataArray[VA_B_PHASE];
      float vll_average = frgG_DataArray[VLL_AVERAGE];
      float vry_phase = frgG_DataArray[VRY_PHASE];
      float vyb_phase = frgG_DataArray[VYB_PHASE];
      float vbr_phase = frgG_DataArray[VBR_PHASE];
      float vln_average = frgG_DataArray[VLN_AVERAGE];
      float v_r_phase = frgG_DataArray[V_R_PHASE];
      float v_y_phase = frgG_DataArray[V_Y_PHASE];
      float v_b_phase = frgG_DataArray[V_B_PHASE];
      float current_total = frgG_DataArray[CURRENT_TOTAL];
      float current_r_phase = frgG_DataArray[CURRENT_R_PHASE];
      float current_y_phase = frgG_DataArray[CURRENT_Y_PHASE];
      float current_b_phase = frgG_DataArray[CURRENT_B_PHASE];
      float frequency = frgG_DataArray[FREQUENCY];
      float wh_received = frgG_DataArray[WH_RECEIVED];

      state = "{\"state\" : {\"reported\" : {\"Watts_Total\" :" + String(watts_total) + " ," + "\"Watts_R_phase\" : " + String(watts_r_phase) + " ,\"Watts_Y_phase\" : " 
              + String(watts_y_phase) + " ,\"Watts_B_phase\" : " + String(watts_b_phase) + " ,\"VAR_Total\" : " + String(var_total) + " ,\"VAR_R_phase\" : " + String(var_r_phase) 
              + " ,\"VAR_Y_phase\" : " + String(var_y_phase) + " ,\"VAR_B_phase\" : " + String(var_b_phase) + " ,\"PF_Ave\" : " + String(pf_average) + " ,\"PF_R_phase\" : " 
              + String(pf_r_phase) + " ,\"PF_Y_phase\" : " + String(pf_y_phase) + " ,\"PF_B_phase\" : " + String(pf_b_phase) + " ,\"VA_Total\" : " + String(va_total) 
              + " ,\"VA_R_phase\" : " + String(va_r_phase) + " ,\"VA_Y_phase\" : " + String(va_y_phase) + " ,\"VA_B_phase\" : " + String(va_b_phase) + " ,\"VLL_avg\" : " 
              + String(vll_average) + " ,\"Vry_phase\" : " + String(vry_phase) + " ,\"Vyb_phase\" : " + String(vyb_phase) + " ,\"Vbr_phase\" : " + String(vbr_phase) 
              + " ,\"VLN_avg\" : " + String(vln_average) + " ,\"V_R_phase\" : " + String(v_r_phase) + " ,\"V_Y_phase\" : " + String(v_y_phase) + " ,\"V_B_phase\" : " 
              + String(v_b_phase) + " ,\"Current_Total\" : " + String(current_total) + " ,\"Current_R_phase\" : " + String(current_r_phase) + " ,\"Current_Y_phase\" : " 
              + String(current_y_phase) + " ,\"Current_B_phase\" : " + String(current_b_phase) + " ,\"Freq\" : " + String(frequency) 
              + " ,\"Wh_Received\" : " + String(wh_received) + "}}}" ;    
      
      Serial.println(state);
      
      watts_total = watts_total*100;
      watts_r_phase = watts_r_phase*100;
      watts_y_phase = watts_y_phase*100;
      watts_b_phase = watts_b_phase*100;
      var_total = var_total*100;
      var_r_phase = var_r_phase*100;
      var_y_phase = var_y_phase*100;
      var_b_phase = var_b_phase*100;
      pf_average = pf_average*100;
      pf_r_phase = pf_r_phase*100;
      //pf_y_phase = pf_y_phase * 100;
      //pf_b_phase = pf_b_phase * 100;
      va_total = va_total * 100;
      va_r_phase = va_r_phase * 100;
      va_y_phase = va_y_phase * 100;
      va_b_phase = va_b_phase * 100;
      vll_average = vll_average * 100;
      vry_phase = vry_phase * 100;
      vyb_phase = vyb_phase * 100;
      vbr_phase = vbr_phase * 100;
      vln_average = vln_average * 100;
      v_r_phase = v_r_phase * 100;
      //v_y_phase = v_y_phase * 100;
      //v_b_phase = v_b_phase * 100;
      current_total = current_total * 100;
      current_r_phase = current_r_phase * 100;
      //current_y_phase = current_y_phase * 100;
      //current_b_phase = current_b_phase * 100;
      frequency = frequency * 100;
      wh_received = wh_received * 100;

      //Serial.println(frequency);
      
      uint32_t watts_total_1 = watts_total;
      uint32_t watts_r_phase_1 = watts_r_phase;
      uint32_t watts_y_phase_1 = watts_y_phase;
      uint32_t watts_b_phase_1 = watts_b_phase;
      uint32_t var_total_1 = var_total;
      uint32_t var_r_phase_1 = var_r_phase;
      uint32_t var_y_phase_1 = var_y_phase;
      uint32_t var_b_phase_1 = var_b_phase;
      uint32_t pf_average_1 = pf_average;
      uint32_t pf_r_phase_1 = pf_r_phase;
      //uint32_t pf_y_phase_1 = pf_y_phase;
      //uint32_t pf_b_phase_1 = pf_b_phase;
      uint32_t va_total_1 = va_total;
      uint32_t va_r_phase_1 = va_r_phase;
      uint32_t va_y_phase_1 = va_y_phase;
      uint32_t va_b_phase_1 = va_b_phase;
      uint32_t vll_average_1 = vll_average;
      uint32_t vry_phase_1 = vry_phase;
      uint32_t vyb_phase_1 = vyb_phase;
      uint32_t vbr_phase_1 = vbr_phase;
      uint32_t vln_average_1 = vln_average;
      uint32_t v_r_phase_1 = v_r_phase;
      //uint32_t v_y_phase_1 = v_y_phase;
      //uint32_t v_b_phase_1 = v_b_phase;
      uint32_t current_total_1 = current_total;
      uint32_t current_r_phase_1 = current_r_phase;
      //uint32_t current_y_phase_1 = current_y_phase;
      //uint32_t current_b_phase_1 = current_b_phase;
      uint32_t frequency_1 = frequency;      
      uint32_t wh_received_1 = wh_received;
      
      myData[0] = highByte(watts_total_1);
      myData[1] = lowByte(watts_total_1);
      myData[2] = highByte(watts_r_phase_1);
      myData[3] = lowByte(watts_r_phase_1);      
      myData[4] = highByte(watts_y_phase_1);
      myData[5] = lowByte(watts_y_phase_1);      
      myData[6] = highByte(watts_b_phase_1);
      myData[7] = lowByte(watts_b_phase_1);      
      myData[8] = highByte(var_total_1);
      myData[9] = lowByte(var_total_1);      
      myData[10] = highByte(var_r_phase_1);
      myData[11] = lowByte(var_r_phase_1);      
      myData[12] = highByte(var_y_phase_1);
      myData[13] = lowByte(var_y_phase_1);      
      myData[14] = highByte(var_b_phase_1);
      myData[15] = lowByte(var_b_phase_1);      
      myData[16] = highByte(pf_average_1);
      myData[17] = lowByte(pf_average_1);      
      myData[18] = highByte(pf_r_phase_1);
      myData[19] = lowByte(pf_r_phase_1);      
      //myData[20] = highByte(pf_y_phase_1);
      //myData[21] = lowByte(pf_y_phase_1);      
      //myData[22] = highByte(pf_b_phase_1);
      //myData[23] = lowByte(pf_b_phase_1);      
      myData[20] = highByte(va_total_1);
      myData[21] = lowByte(va_total_1);      
      myData[22] = highByte(va_r_phase_1);
      myData[23] = lowByte(va_r_phase_1);      
      myData[24] = highByte(va_y_phase_1);
      myData[25] = lowByte(va_y_phase_1);      
      myData[26] = highByte(va_b_phase_1);
      myData[27] = lowByte(va_b_phase_1);      
      myData[28] = highByte(vll_average_1);
      myData[29] = lowByte(vll_average_1);      
      myData[30] = highByte(vry_phase_1);
      myData[31] = lowByte(vry_phase_1);      
      myData[32] = highByte(vyb_phase_1);
      myData[33] = lowByte(vyb_phase_1);      
      myData[34] = highByte(vbr_phase_1);
      myData[35] = lowByte(vbr_phase_1);      
      myData[36] = highByte(vln_average_1);
      myData[37] = lowByte(vln_average_1);      
      myData[38] = highByte(v_r_phase_1);
      myData[39] = lowByte(v_r_phase_1);      
      //myData[44] = highByte(v_y_phase_1);
      //myData[45] = lowByte(v_y_phase_1);      
      //myData[46] = highByte(v_b_phase_1);
      //myData[47] = lowByte(v_b_phase_1);      
      myData[40] = highByte(current_total_1);
      myData[41] = lowByte(current_total_1);      
      myData[42] = highByte(current_r_phase_1);
      myData[43] = lowByte(current_r_phase_1);      
      //myData[52] = highByte(current_y_phase_1);
      //myData[53] = lowByte(current_y_phase_1);      
      //myData[54] = highByte(current_b_phase_1);
      //myData[55] = lowByte(current_b_phase_1);      
      myData[44] = highByte(frequency_1);
      myData[45] = lowByte(frequency_1);      
      myData[46] = highByte(wh_received_1);
      myData[47] = lowByte(wh_received_1);

      LMIC_setTxData2(1, myData, sizeof(myData) - 1, 0);
    }
/* Next TX is scheduled after TX_COMPLETE event. */
 }


void setup() 
 {
/* Init in receive mode. */
   Serial.begin(UART_BAUD_RATE);
   F_Rs485SerialSetup();
   //Serial.println(F("Starting"));

   #ifdef VCC_ENABLE
/* For Pinoccio Scout boards. */
     pinMode(VCC_ENABLE, OUTPUT);
     digitalWrite(VCC_ENABLE, HIGH);
     delay(1000);
   #endif

   os_init();                                                                                         // LMIC init

/* Reset the MAC state. Session and pending data transfers will be discarded. */
   LMIC_reset();

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
     LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                     // g-band
     LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);                     // g-band
     LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                     // g-band
     LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                     // g-band
     LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                     // g-band
     LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                     // g-band
     LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                     // g-band
     LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);                     // g-band
     LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);                     // g2-band

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

void loop() 
 {
   os_runloop_once();
 }
