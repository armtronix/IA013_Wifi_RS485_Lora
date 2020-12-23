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
   String temp_will_msg = (String)hostsaved + (String)mqtt_will_msg;                                      // Added on 28/07/2017.

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
   else if(mqttClient.connect(host))                                                                      // Added on 31/07/18.
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

   if(msgString == "voltage")
    {
      mqttClient.publish((char*)pubTopicV.c_str(), (char*) (String(_vry)+"V").c_str());
    }

   if(msgString == "current")
    {
       mqttClient.publish((char*)pubTopicCurrent.c_str(), (char*) (String(_currentT)+"A").c_str());
    }
  
   if(msgString == "watts")
    {
      mqttClient.publish((char*)pubTopicP.c_str(), (char*) (String(_wattsT)+"W").c_str());
    }
   if(msgString == "energy")
    {
      mqttClient.publish((char*)pubTopicWh.c_str(), (char*) (String(_power)+"Wh").c_str());
    }

   if(msgString == "frequency")
    {
      mqttClient.publish((char*)pubTopicF.c_str(), (char*) (String(_frequency)+"Hz").c_str());
    }

   if(msgString == "pf")
    {
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
