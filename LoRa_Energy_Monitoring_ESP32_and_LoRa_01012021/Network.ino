void Scan_Wifi_Networks()
 {
/* Set WiFi to station mode and disconnect from an AP if it was previously connected
   Need to be in dicsonected mode to Run network Scan!
 */ 
   WiFi.mode(WIFI_STA);                                                                      
   WiFi.disconnect();                                                                        
   delay(10);                                                                                
   int network_count = WiFi.scanNetworks();

   if(network_count <= 0)
    {
      // Serial.println("no networks found");
    }
   else if(network_count > 0)
    {
      Serial.print(network_count);
      Serial.println(" networks found");
      for(int i = 0; i < network_count; ++i)
       {
         if(esid == WiFi.SSID(i))
          {
            Serial.println("My network has been Found!");
            Do_Connect();
          }
         else
          {
            Serial.println("Not my network... ");
          }
       }
    }
   host= (char*) hostsaved.c_str();                                                          
   Serial.println("");
   WiFi.disconnect();
   delay(10);                                                                                
   WiFi.mode(WIFI_AP);
   WiFi.softAP(host);
   WiFi.begin(host);                                                            
   Serial.print("Searching for network , Also Access point started with name ");
   Serial.println(host);
   inApMode=1;
   launchWeb(1);
 }

void Do_Connect()                                                                            // Try to connect to the Found WIFI Network!.
 {
   #ifdef ARDUINO_ARCH_ESP32
    esp_restart();
   #else
    ESP.reset();
   #endif  
}
