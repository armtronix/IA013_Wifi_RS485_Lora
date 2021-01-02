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
      _wattsT = frgG_DataArray[WATTS_TOTAL];
      _wattsR = frgG_DataArray[WATTS_R_PHASE];
      _wattsY = frgG_DataArray[WATTS_Y_PHASE];
      _wattsB = frgG_DataArray[WATTS_B_PHASE];
      _varT = frgG_DataArray[VAR_TOTAL];
      _varR = frgG_DataArray[VAR_R_PHASE];
      _varY = frgG_DataArray[VAR_Y_PHASE];
      _varB = frgG_DataArray[VAR_B_PHASE];
      _pfA = frgG_DataArray[PF_AVERAGE];
      _pfR = frgG_DataArray[PF_R_PHASE];
      _pfY = frgG_DataArray[PF_Y_PHASE];
      _pfB = frgG_DataArray[PF_B_PHASE];
      _vaT = frgG_DataArray[VA_TOTAL];
      _vaR = frgG_DataArray[VA_R_PHASE];
      _vaY = frgG_DataArray[VA_Y_PHASE];
      _vaB = frgG_DataArray[VA_B_PHASE];
      _vll = frgG_DataArray[VLL_AVERAGE];
      _vry = frgG_DataArray[VRY_PHASE];
      _vyb = frgG_DataArray[VYB_PHASE];
      _vbr = frgG_DataArray[VBR_PHASE];
      _vln = frgG_DataArray[VLN_AVERAGE];
      _vrphase = frgG_DataArray[V_R_PHASE];
      _vyphase = frgG_DataArray[V_Y_PHASE];
      _vbphase = frgG_DataArray[V_B_PHASE];
      _currentT = frgG_DataArray[CURRENT_TOTAL];
      _currentR = frgG_DataArray[CURRENT_R_PHASE];
      _currentY = frgG_DataArray[CURRENT_Y_PHASE];
      _currentB = frgG_DataArray[CURRENT_B_PHASE];
      _frequency = frgG_DataArray[FREQUENCY];
      _power = frgG_DataArray[WH_RECEIVED];

      state = "{ \"state\" : { \"reported\" : { \"Watts_Total\" : " + String(_wattsT) + ", " + "\"Watts_R_phase\" : " + String(_wattsR) + ", \"Watts_Y_phase\" : "
              + String(_wattsY) + ", \"Watts_B_phase\" : " + String(_wattsB) + ", \"VAR_Total\" : " + String(_varT) + ", \"VAR_R_phase\" : " + String(_varR) 
              + ", \"VAR_Y_phase\" : " + String(_varY) + ", \"VAR_B_phase\" : " + String(_varB) + ", \"PF_Ave\" : " + String(_pfA) + ", \"PF_R_phase\" : " 
              + String(_pfR) + ", \"PF_Y_phase\" : " + String(_pfY) + ", \"PF_B_phase\" : " + String(_pfB) + ", \"VA_Total\" : " + String(_vaT) 
              + ", \"VA_R_phase\" : " + String(_vaR) + ", \"VA_Y_phase\" : " + String(_vaY) + ", \"VA_B_phase\" : " + String(_vaB) + ", \"VLL_avg\" : " 
              + String(_vll) + ", \"Vry_phase\" : " + String(_vry) + ", \"Vyb_phase\" : " + String(_vyb) + ", \"Vbr_phase\" : " + String(_vbr) 
              + ", \"VLN_avg\" : " + String(_vln) + ", \"V_R_phase\" : " + String(_vrphase) + ", \"V_Y_phase\" : " + String(_vyphase) + ", \"V_B_phase\" : "
              + String(_vbphase) + ", \"Current_Total\" : " + String(_currentT) + ", \"Current_R_phase\" : " + String(_currentR) + ", \"Current_Y_phase\" : "
              + String(_currentY) + ", \"Current_B_phase\" : " + String(_currentB) + ", \"Freq\" : " + String(_frequency) + ", \"Wh_Received\" : " + String(_power) + "}}}" ;
 
      Serial.println(state);
      
      _wattsT = _wattsT * 100;
      _wattsR = _wattsR * 100;
      _wattsY = _wattsY * 100;
      _wattsB = _wattsB * 100;
      _varT = _varT * 100;
      _varR = _varR * 100;
      _varY = _varY * 100;
      _varB = _varB * 100;
      _pfA = _pfA * 100;
      _pfR = _pfR * 100;
      //_pfY = _pfY * 100;
      //_pfB = _pfB * 100;
      _vaT = _vaT * 100;
      _vaR = _vaR * 100;
      _vaY = _vaY * 100;
      _vaB = _vaB * 100;
      _vll = _vll * 100;
      _vry = _vry * 100;
      _vyb = _vyb * 100;
      _vbr = _vbr * 100;
      _vln = _vln * 100;
      _vrphase = _vrphase * 100;
      //_vyphase = _vyphase * 100;
      //_vbphase = _vbphase * 100;
      _currentT = _currentT * 100;
      _currentR = _currentR * 100;
      //_currentY = _currentY * 100;
      //_currentB = _currentB * 100;
      _frequency = _frequency * 100;
      _power = _power * 100;

      //Serial.println(frequency);
      
      uint32_t watts_total_1 = _wattsT;
      uint32_t watts_r_phase_1 = _wattsR;
      uint32_t watts_y_phase_1 = _wattsY;
      uint32_t watts_b_phase_1 = _wattsB;
      uint32_t var_total_1 = _varT;
      uint32_t var_r_phase_1 = _varR;
      uint32_t var_y_phase_1 = _varY;
      uint32_t var_b_phase_1 = _varB;
      uint32_t pf_average_1 = _pfA;
      uint32_t pf_r_phase_1 = _pfR;
      //uint32_t pf_y_phase_1 = _pfY;
      //uint32_t pf_b_phase_1 = _pfB;
      uint32_t va_total_1 = _vaT;
      uint32_t va_r_phase_1 = _vaR;
      uint32_t va_y_phase_1 = _vaY;
      uint32_t va_b_phase_1 = _vaB;
      uint32_t vll_average_1 = _vll;
      uint32_t vry_phase_1 = _vry;
      uint32_t vyb_phase_1 = _vyb;
      uint32_t vbr_phase_1 = _vbr;
      uint32_t vln_average_1 = _vln;
      uint32_t v_r_phase_1 = _vrphase;
      //uint32_t v_y_phase_1 = _vyphase;
      //uint32_t v_b_phase_1 = _vbphase;
      uint32_t current_total_1 = _currentT;
      uint32_t current_r_phase_1 = _currentR;
      //uint32_t current_y_phase_1 = _currentY;
      //uint32_t current_b_phase_1 = _currentB;
      uint32_t frequency_1 = _frequency;      
      uint32_t wh_received_1 = _power;
      
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

      Serial.println(state);      
      mqttClient.publish((char*)pubTopic.c_str(), (char*) state.c_str());          
      state = " ";

    }
/* Next TX is scheduled after TX_COMPLETE event. */
 }
