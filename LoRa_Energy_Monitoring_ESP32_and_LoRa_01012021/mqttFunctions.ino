boolean connectMQTT() 
 {
   if(mqttClient.connected()) 
    {
      return true;
    }

   Serial.print("Connecting to MQTT server ");
   Serial.print(mqttServer);
   Serial.print(" as ");
   Serial.println(host);
   String temp_will_msg = (String)hostsaved + (String)mqtt_will_msg;                                      

   if(mqttClient.connect(host, (char*)mqtt_user.c_str(), (char*)mqtt_passwd.c_str(), (char*)pubTopic.c_str(), 0, 0, (char*)temp_will_msg.c_str())) //added on 28/07/18
    {
      Serial.println("Connected to MQTT broker with authentication");
      if(mqttClient.subscribe((char*)subTopic.c_str()))
       {
         Serial.println("Subsribed to topic.1");
       }
      else
       {
         Serial.println("NOT subsribed to topic 1");
       }
      mqttClient.loop();
      return true;
    }
   else if(mqttClient.connect(host))                                                                      
    {
      Serial.println("Connected to MQTT broker without authentication");
      if(mqttClient.subscribe((char*)subTopic.c_str()))
       {
         Serial.println("Subsribed to topic.1");
       }
    }
   else
    {
      Serial.println("MQTT connect failed! ");
      return false;
    }
 }

void disconnectMQTT() 
 {
   mqttClient.disconnect();
 }

void mqtt_handler() 
 {
   if(toPub == 1) 
    {
      // Debugln("DEBUG: Publishing state via MQTT");
      if(pubState()) 
       {
         toPub = 0;
       }
    }
   mqttClient.loop();
   delay(100);                                                                                            // Let things happen in background.
 }

void mqtt_arrived(char* subTopic, byte* payload, unsigned int length) 
 {                                                                                                        // Handle messages arrived.
   int i = 0;
   Serial.print("MQTT message arrived:  topic: " + String(subTopic));
   handle_rs485(START_ADDRESS , RANGE);
   delay(1000);      

/* Create character buffer with ending null terminator (string). */
   for(i = 0; i < length; i++) 
    {
      buf[i] = payload[i];
    }
   buf[i] = '\0';
   String msgString = String(buf);
   Serial.println(" message: " + msgString);

   if(msgString == "Reset")
    {
      clearConfig();
      delay(10);
      Serial.println("Done, restarting!");
      #ifdef ARDUINO_ARCH_ESP32
       esp_restart();
      #else
       ESP.reset();
      #endif
    }

   if(msgString == "All")
    {
      Serial.println("Data received via uart");            
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
      delay(100);
      Serial.println(state);      
      mqttClient.publish((char*)pubTopic.c_str(), (char*) state.c_str());          
      state = " ";
      //toPub = 1;
    }

   if(msgString == "voltage")
    {
      _vry = frgG_DataArray[VRY_PHASE];
      mqttClient.publish((char*)pubTopicV.c_str(), (char*) (String(_vry)+"V").c_str());
    }

   if(msgString == "current")
    {
      _currentT = frgG_DataArray[CURRENT_TOTAL];
      mqttClient.publish((char*)pubTopicCurrent.c_str(), (char*) (String(_currentT)+"A").c_str());
    }
  
   if(msgString == "watts")
    {
      _wattsT = frgG_DataArray[WATTS_TOTAL];
      mqttClient.publish((char*)pubTopicP.c_str(), (char*) (String(_wattsT)+"W").c_str());
    }
   if(msgString == "energy")
    {
      _power = frgG_DataArray[WH_RECEIVED];
      mqttClient.publish((char*)pubTopicWh.c_str(), (char*) (String(_power)+"Wh").c_str());
    }

   if(msgString == "frequency")
    {
      _frequency = frgG_DataArray[FREQUENCY];
      mqttClient.publish((char*)pubTopicF.c_str(), (char*) (String(_frequency)+"Hz").c_str());
    }

   if(msgString == "pf")
    {
      _pfA = frgG_DataArray[PF_AVERAGE];
      mqttClient.publish((char*)pubTopicPF.c_str(), (char*) (String(_pfA)).c_str());
    }
   if(msgString == "status")
    {
      toPub = 1;
    }
 }

/* Publish the current state of the light. */
boolean pubState() 
 { 
   boolean publish_ok;
   if(!connectMQTT()) 
    {
      delay(100);
      if(!connectMQTT) 
       {
         Serial.println("Could not connect MQTT.");
         Serial.println("Publish state NOK");
         publish_ok = false;
         return publish_ok;
       }
    }
   if(mqttClient.connected()) 
    {
      //Serial.println(state);
      if(mqttClient.publish((char*)pubTopic.c_str(), (char*) state.c_str()) &&     
         mqttClient.publish((char*)pubTopicV.c_str(), (char*) (String(_vry)+"V").c_str()) &&
         mqttClient.publish((char*)pubTopicCurrent.c_str(), (char*) (String(_currentT)+"A").c_str()) &&
         mqttClient.publish((char*)pubTopicP.c_str(), (char*) (String(_wattsT)+"W").c_str()) &&
         mqttClient.publish((char*)pubTopicWh.c_str(), (char*) (String(_power)+"Wh").c_str()) &&
         mqttClient.publish((char*)pubTopicF.c_str(), (char*) (String(_frequency)+"Hz").c_str()) &&
         mqttClient.publish((char*)pubTopicPF.c_str(), (char*) (String(_pfA)).c_str())
        )
       {
         toPub = 0;
         //Serial.println("Publish 1 state OK");
         return true;
       }
      else
       {
         //Serial.println("Publish 1 state NOK");
         return false;
       }
      mqttClient.loop();
    }
   else
    {
      Serial.println("Publish state NOK");
      Serial.println("No MQTT connection.");
    }
 }
