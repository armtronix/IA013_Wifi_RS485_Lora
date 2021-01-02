void initWiFi() 
 {
   Serial.println();
   Serial.println();
   Serial.println("Startup");

   // test esid
   WiFi.disconnect();
   delay(100);
   WiFi.mode(WIFI_STA);
   Serial.print("Connecting to WiFi ");
   Serial.println(esid);
   Debugln(epass);
   WiFi.begin((char*)esid.c_str(), (char*)epass.c_str());
   if(testWifi() == 20) 
    {
      launchWeb(0);
      return;
    }
   else
    {
      Serial.println("Opening AP");
      setupAP();
    }
 }

int testWifi(void) 
 {
   int c = 0;
   Debugln("Wifi test...");
   while(c < 30) 
    {
      if(WiFi.status() == WL_CONNECTED) 
       {
         return (20);
       }
      delay(500);
      Serial.print(".");
      c++;
    }
   Serial.println("WiFi Connect timed out!");
   return (10);
 }

void setupAP(void) 
 {
   WiFi.mode(WIFI_STA);
   WiFi.disconnect();
   delay(100);
   int n = WiFi.scanNetworks();
   Serial.println("scan done");
   if(n == 0) 
    {
      Serial.println("no networks found");
      st = "<b>No networks found:</b>";
    } 
   else 
    {
      Serial.print(n);
      Serial.println(" Networks found");
      st = "<ul>";
      for(int i = 0; i < n; ++i)
       {
/* Print SSID and RSSI for each network found. */
         Serial.print(i + 1);
         Serial.print(": ");
         Serial.print(WiFi.SSID(i));
         Serial.print(" (");
         Serial.print(WiFi.RSSI(i));
         Serial.print(")");
         #ifdef ARDUINO_ARCH_ESP32
          Serial.println((WiFi.encryptionType(i) ==  WIFI_AUTH_OPEN) ? " (OPEN)" : "*");   //esp32
         #else
          Serial.println((WiFi.encryptionType(i) ==  ENC_TYPE_NONE) ? " (OPEN)" : "*");
         #endif

/* Print to web SSID and RSSI for each network found. */
         st += "<li>";
         st += i + 1;
         st += ": ";
         st += WiFi.SSID(i);
         st += " (";
         st += WiFi.RSSI(i);
         st += ")";
         #ifdef ARDUINO_ARCH_ESP32
          st += (WiFi.encryptionType(i) ==  WIFI_AUTH_OPEN) ? " (OPEN)" : "*"; //esp32
         #else
          st += (WiFi.encryptionType(i) ==  ENC_TYPE_NONE) ? " (OPEN)" : "*";
         #endif
         st += "</li>";
         delay(10);
       }
      st += "</ul>";
    }
   Serial.println("");
   WiFi.disconnect();
   delay(100);
   WiFi.mode(WIFI_AP);

   WiFi.softAP(host);
   WiFi.begin(host);                                                                                       // Not sure if need but works.
   Serial.print("Access point started with name ");
   Serial.println(host);
   inApMode = 1;
   launchWeb(1);
 }

void launchWeb(int webtype) 
 {
   Serial.println("");
   Serial.println("WiFi connected");
   //Start the web server or MQTT
   if(otaFlag == 1 && !inApMode) 
    {
      Serial.println("Starting OTA mode.");
      #ifdef ARDUINO_ARCH_ESP32
      #else
       Serial.printf("Sketch size: %u\n", ESP.getSketchSize());
       Serial.printf("Free size: %u\n", ESP.getFreeSketchSpace());
      #endif

      MDNS.begin(host);
      server.on("/", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/html", otaServerIndex);
       });
      server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        setOtaFlag(0);
        ESP.restart();
       }, []() {
        HTTPUpload& upload = server.upload();
        if(upload.status == UPLOAD_FILE_START) 
         {
           // Serial.setDebugOutput(true);
           #ifdef ARDUINO_ARCH_ESP32
            // WiFiUDP::stop(); //esp32
           #else
            WiFiUDP::stopAll();
           #endif
           Serial.printf("Update: %s\n", upload.filename.c_str());
           otaCount = 300;
           uint32_t maxSketchSpace;                                                                        // = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
           if(!Update.begin(maxSketchSpace))                                                               // Start with max available size.
            { 
              Update.printError(Serial);
            }
         } 
        else if(upload.status == UPLOAD_FILE_WRITE) 
         {
           if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) 
            {
              Update.printError(Serial);
            }
         } 
        else if(upload.status == UPLOAD_FILE_END) 
         {
           if(Update.end(true))                                                                            // True to set the size to the current progress.
            { 
              Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            } 
           else 
            {
              Update.printError(Serial);
            }
           Serial.setDebugOutput(false);
         }
        yield();
       });
      server.begin();
      Serial.printf("Ready! Open http://%s.local in your browser\n", host);
      MDNS.addService("http", "tcp", 80);
      // otaTickLoop.attach(1, otaCountown);
    } 
   else 
    {
      // setOtaFlag(1);
      if(webtype == 1 || iotMode == 0)                                                                     // In config mode or WebControle.
       { 
         if(webtype == 1) 
          {
            webtypeGlob == 1;
            Serial.println(WiFi.softAPIP());
            server.on("/", webHandleConfig);
            server.on("/a", webHandleConfigSave);
            server.on("/gpio", webHandleGpio);
            server.on("/xml", handleXML);
          } 
         else 
          {
/* setup DNS since we are a client in WiFi net. */
            if(!MDNS.begin(host)) 
             {
               Serial.println("Error setting up MDNS responder!");
             } 
            else 
             {
               Serial.println("mDNS responder started");
               MDNS.addService("http", "tcp", 80);
             }
            Serial.println(WiFi.localIP());
            server.on("/", webHandleRoot);
            server.on("/cleareeprom", webHandleClearRom);
            server.on("/gpio", webHandleGpio);
            server.on("/xml", handleXML);
          }
         // server.onNotFound(webHandleRoot);
         server.begin();
         Serial.println("Web server started");
         webtypeGlob = webtype; //Store global to use in loop()
       } 
      else if(webtype != 1 && iotMode == 1) 
       {                                                                                                   // In MQTT and not in config mode.
         // mqttClient.setBrokerDomain((char*) mqttServer.c_str());
         // mqttClient.setPort(1883);
         mqttClient.setServer((char*) mqttServer.c_str(), 1883);
         mqttClient.setCallback(mqtt_arrived);
         mqttClient.setClient(wifiClient);
         if(WiFi.status() == WL_CONNECTED) 
          {
            if(!connectMQTT()) 
             {
               delay(2000);
               if(!connectMQTT()) 
                {
                  Serial.println("Could not connect MQTT.");
                  Serial.println("Starting web server instead.");
                  iotMode = 0;
                  launchWeb(0);
                  webtypeGlob = webtype;
                }
             }
          }
       }
    }
 }

void webHandleConfig() 
 {
   IPAddress ip = WiFi.softAPIP();
   String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
   String s;

   s = "Configuration of " + hostName + " at ";
   s += ipStr;
   s += "<p><a href=\"/gpio\">Control GPIO</a><br />";
   s += st;
   s += "<form method='get' action='a'>";
   s += "<label>SSID: </label><input name='ssid' length=32><label> Pass: </label><input name='pass' type='password' length=64></br>";
   s += "The following is not ready yet!</br>";
   s += "<label>IOT mode: </label><input type='radio' name='iot' value='0'> HTTP<input type='radio' name='iot' value='1' checked> MQTT</br>";
   s += "<label>MQTT Broker IP/DNS: </label><input name='host' length=15></br>";
   s += "<label>MQTT User Name: </label><input name='mqtt_user' length=15></br>";
   s += "<label>MQTT Password: </label><input name='mqtt_passwd' length=15></br>";
   s += "<label>MQTT Publish topic: </label><input name='pubtop' length=64></br>";
   s += "<label>MQTT Subscribe topic: </label><input name='subtop' length=64></br>";
   s += "<input type='submit'></form></p>";
   s += "\r\n\r\n";
   Serial.println("Sending 200");
   server.send(200, "text/html", s);
 }

void webHandleConfigSave() 
 {
   // /a?ssid=blahhhh&pass=poooo
   String s;
   s = "<p>Settings saved to eeprom and reset to boot into new settings</p>\r\n\r\n";
   server.send(200, "text/html", s);
   Serial.println("clearing EEPROM.");
   clearConfig();
   String qsid;
   qsid = server.arg("ssid");
   qsid.replace("%2F", "/");
   Serial.println("Got SSID: " + qsid);
   esid = (char*) qsid.c_str();

   String qpass;
   qpass = server.arg("pass");
   qpass.replace("%2F", "/");
   Serial.println("Got pass: " + qpass);
   epass = (char*) qpass.c_str(); 

   String qiot;
   qiot = server.arg("iot");
   Serial.println("Got iot mode: " + qiot);
   qiot == "0" ? iotMode = 0 : iotMode = 1 ;

   String qsubTop;
   qsubTop = server.arg("subtop");
   qsubTop.replace("%2F", "/");
   Serial.println("Got subtop: " + qsubTop);
   subTopic = (char*) qsubTop.c_str();

   String qpubTop;
   qpubTop = server.arg("pubtop");
   qpubTop.replace("%2F", "/");
   Serial.println("Got pubtop: " + qpubTop);
   pubTopic = (char*) qpubTop.c_str(); 

   mqttServer = (char*) server.arg("host").c_str();
   Serial.print("Got mqtt Server: ");
   Serial.println(mqttServer);

   String qmqtt_user;
   qmqtt_user = server.arg("mqtt_user");
   qmqtt_user.replace("%2F", "/");
   Serial.println("Got mqtt_user: " + qmqtt_user);
   mqtt_user = (char*) qmqtt_user.c_str();

   String qmqtt_passwd;
   qmqtt_passwd = server.arg("mqtt_passwd");
   qmqtt_passwd.replace("%2F", "/");
   Serial.println("Got mqtt_passwd: " + qmqtt_passwd);
   mqtt_passwd = (char*) qmqtt_passwd.c_str(); 

   Serial.print("Settings written ");
   saveConfig() ? Serial.println("sucessfully.") : Serial.println("not succesfully!");;
   Serial.println("Restarting!");
   delay(1000);
   #ifdef ARDUINO_ARCH_ESP32
    esp_restart();
   #else
    ESP.reset();
   #endif
 }
 
void webHandleRoot() 
 {
   String s;
   s = "<!DOCTYPE HTML>\n";
   s += "<p>Wifi Bt Esp32  Board ";
   s += "</p>";
   // s += "<a href=\"/gpio\">Control GPIO</a><br />";
   buildJavascript();
   s += javaScript;
   s += "<BODY onload='process()'>\n";
   s += "<p>Energy monitor\n</p><p>Wattstotal: <A id='runtime1'></A>\n</p>";
   s += "<p>Wattsrphase: <A id='runtime2'></A>\n</p>";
   s += "<p>Wattsyphase: <A id='runtime3'></A>\n</p>";
   s += "<p>Wattsbphase: <A id='runtime4'></A>\n</p>";
   s += "<p>Vartotal: <A id='runtime5'></A>\n</p>";
   s += "<p>Varrphase: <A id='runtime6'></A>\n</p>";
   s += "<p>Varyphase: <A id='runtime7'></A>\n</p>";
   s += "<p>Varbphase: <A id='runtime8'></A>\n</p>";
   s += "<p>Pfaverage: <A id='runtime9'></A>\n</p>";
   s += "<p>Pfrphase: <A id='runtime10'></A>\n</p>";
   s += "<p>Pfyphase: <A id='runtime11'></A>\n</p>";
   s += "<p>Pfbphase: <A id='runtime12'></A>\n</p>";
   s += "<p>Vatotal: <A id='runtime13'></A>\n</p>";
   s += "<p>Varphase: <A id='runtime14'></A>\n</p>";
   s += "<p>Vayphase: <A id='runtime15'></A>\n</p>";
   s += "<p>Vabphase: <A id='runtime16'></A>\n</p>";
   s += "<p>Vllaverage: <A id='runtime17'></A>\n</p>";
   s += "<p>Vryphase: <A id='runtime18'></A>\n</p>";
   s += "<p>Vybphase: <A id='runtime19'></A>\n</p>";
   s += "<p>Vbrphase: <A id='runtime20'></A>\n</p>";
   s += "<p>Vlnaverage: <A id='runtime21'></A>\n</p>";
   s += "<p>Vrphase: <A id='runtime22'></A>\n</p>";
   s += "<p>Vyphase: <A id='runtime23'></A>\n</p>";
   s += "<p>Vbphase: <A id='runtime24'></A>\n</p>";
   s += "<p>Currenttotal: <A id='runtime25'></A>\n</p>";
   s += "<p>Currentrphase: <A id='runtime26'></A>\n</p>";
   s += "<p>Currentyphase: <A id='runtime27'></A>\n</p>";
   s += "<p>Currentbphase: <A id='runtime28'></A>\n</p>";
   s += "<p>Frequency: <A id='runtime29'></A>\n</p>";
   s += "<p>Whreceived: <A id='runtime30'></A>\n</p>";
   s += "</BODY>\n";
   s += "<a href=\"/cleareeprom\">Clear settings an boot into Config mode</a><br />";
   s += "\r\n\r\n";
   s += "</HTML>\n";
   Serial.println("Sending 200");
   server.send(200, "text/html", s);
 }

void webHandleClearRom() 
 {
   String s;
   s = "<p>Clearing the config and reset to configure new wifi<p>";
   s += "</html>\r\n\r\n";
   Serial.println("Sending 200");
   server.send(200, "text/html", s);
   Serial.println("clearing config");
   clearConfig();
   delay(10);
   Serial.println("Done, restarting!");
   #ifdef ARDUINO_ARCH_ESP32
    esp_restart();
   #else
    ESP.reset();
   #endif
 }

void webHandleGpio() 
 {
   String s;

/* Set GPIO according to the request. */
   if(server.arg("reboot") == "1")
    {
      #ifdef ARDUINO_ARCH_ESP32
       esp_restart();
      #else
       ESP.reset();
      #endif
    }

   s += "<p><a href=\"gpio?reboot=1\">Reboot</a></p>";
   s += "</HTML>\n";

   server.send(200, "text/html", s);
 }

void buildJavascript() 
 {
   javaScript = "<Script>\n";
   javaScript += "var xmlHttp=createXmlHttpObject();\n";

   javaScript += "function createXmlHttpObject(){\n";
   javaScript += " if(window.XMLHttpRequest){\n";
   javaScript += "    xmlHttp=new XMLHttpRequest();\n";
   javaScript += " }else{\n";
   javaScript += "    xmlHttp=new ActiveXObject('Microsoft.XMLHTTP');\n";
   javaScript += " }\n";
   javaScript += " return xmlHttp;\n";
   javaScript += "}\n";

   javaScript += "function process(){\n";
   javaScript += " if(xmlHttp.readyState==0 || xmlHttp.readyState==4){\n";
   javaScript += "   xmlHttp.open('PUT','xml',true);\n";
   javaScript += "   xmlHttp.onreadystatechange=handleServerResponse;\n"; // no brackets?????
   javaScript += "   xmlHttp.send(null);\n";
   javaScript += " }\n";
   javaScript += " setTimeout('process()',1000);\n";
   javaScript += "}\n";

   javaScript += "function handleServerResponse(){\n";
   javaScript += " if(xmlHttp.readyState==4 && xmlHttp.status==200){\n";
   javaScript += "   xmlResponse=xmlHttp.responseXML;\n";
   javaScript += "   xmldoc1 = xmlResponse.getElementsByTagName('Wattstotal');\n";
   javaScript += "   xmldoc2 = xmlResponse.getElementsByTagName('Wattsrphase');\n";
   javaScript += "   xmldoc3 = xmlResponse.getElementsByTagName('Wattsyphase');\n";
   javaScript += "   xmldoc4 = xmlResponse.getElementsByTagName('Wattsbphase');\n";
   javaScript += "   xmldoc5 = xmlResponse.getElementsByTagName('Vartotal');\n";
   javaScript += "   xmldoc6 = xmlResponse.getElementsByTagName('Varrphase');\n";
   javaScript += "   xmldoc7 = xmlResponse.getElementsByTagName('Varyphase');\n";
   javaScript += "   xmldoc8 = xmlResponse.getElementsByTagName('Varbphase');\n";
   javaScript += "   xmldoc9 = xmlResponse.getElementsByTagName('Pfaverage');\n";
   javaScript += "   xmldoc10 = xmlResponse.getElementsByTagName('Pfrphase');\n";
   javaScript += "   xmldoc11 = xmlResponse.getElementsByTagName('Pfyphase');\n";
   javaScript += "   xmldoc12 = xmlResponse.getElementsByTagName('Pfbphase');\n";
   javaScript += "   xmldoc13 = xmlResponse.getElementsByTagName('Vatotal');\n";
   javaScript += "   xmldoc14 = xmlResponse.getElementsByTagName('Varphase');\n";
   javaScript += "   xmldoc15 = xmlResponse.getElementsByTagName('Vayphase');\n";
   javaScript += "   xmldoc16 = xmlResponse.getElementsByTagName('Vabphase');\n";
   javaScript += "   xmldoc17 = xmlResponse.getElementsByTagName('Vllaverage');\n";
   javaScript += "   xmldoc18 = xmlResponse.getElementsByTagName('Vryphase');\n";
   javaScript += "   xmldoc19 = xmlResponse.getElementsByTagName('Vybphase');\n";
   javaScript += "   xmldoc20 = xmlResponse.getElementsByTagName('Vbrphase');\n";
   javaScript += "   xmldoc21 = xmlResponse.getElementsByTagName('Vlnaverage');\n";
   javaScript += "   xmldoc22 = xmlResponse.getElementsByTagName('Vrphase');\n";
   javaScript += "   xmldoc23 = xmlResponse.getElementsByTagName('Vyphase');\n";
   javaScript += "   xmldoc24 = xmlResponse.getElementsByTagName('Vbphase');\n";
   javaScript += "   xmldoc25 = xmlResponse.getElementsByTagName('Currenttotal');\n";
   javaScript += "   xmldoc26 = xmlResponse.getElementsByTagName('Currentrphase');\n";
   javaScript += "   xmldoc27 = xmlResponse.getElementsByTagName('Currentyphase');\n";
   javaScript += "   xmldoc28 = xmlResponse.getElementsByTagName('Currentbphase');\n";
   javaScript += "   xmldoc29 = xmlResponse.getElementsByTagName('Frequency');\n";
   javaScript += "   xmldoc30 = xmlResponse.getElementsByTagName('Whreceived');\n";

   javaScript += "   message1 = xmldoc1[0].firstChild.nodeValue;\n";
   javaScript += "   message2 = xmldoc2[0].firstChild.nodeValue;\n";
   javaScript += "   message3 = xmldoc3[0].firstChild.nodeValue;\n";
   javaScript += "   message4 = xmldoc4[0].firstChild.nodeValue;\n";
   javaScript += "   message5 = xmldoc5[0].firstChild.nodeValue;\n";
   javaScript += "   message6 = xmldoc6[0].firstChild.nodeValue;\n";
   javaScript += "   message7 = xmldoc7[0].firstChild.nodeValue;\n";
   javaScript += "   message8 = xmldoc8[0].firstChild.nodeValue;\n";
   javaScript += "   message9 = xmldoc9[0].firstChild.nodeValue;\n";
   javaScript += "   message10 = xmldoc10[0].firstChild.nodeValue;\n";
   javaScript += "   message11 = xmldoc11[0].firstChild.nodeValue;\n";
   javaScript += "   message12 = xmldoc12[0].firstChild.nodeValue;\n";
   javaScript += "   message13 = xmldoc13[0].firstChild.nodeValue;\n";
   javaScript += "   message14 = xmldoc14[0].firstChild.nodeValue;\n";
   javaScript += "   message15 = xmldoc15[0].firstChild.nodeValue;\n";
   javaScript += "   message16 = xmldoc16[0].firstChild.nodeValue;\n";
   javaScript += "   message17 = xmldoc17[0].firstChild.nodeValue;\n";
   javaScript += "   message18 = xmldoc18[0].firstChild.nodeValue;\n";
   javaScript += "   message19 = xmldoc19[0].firstChild.nodeValue;\n";
   javaScript += "   message20 = xmldoc20[0].firstChild.nodeValue;\n";
   javaScript += "   message21 = xmldoc21[0].firstChild.nodeValue;\n";
   javaScript += "   message22 = xmldoc22[0].firstChild.nodeValue;\n";
   javaScript += "   message23 = xmldoc23[0].firstChild.nodeValue;\n";
   javaScript += "   message24 = xmldoc24[0].firstChild.nodeValue;\n";
   javaScript += "   message25 = xmldoc25[0].firstChild.nodeValue;\n";
   javaScript += "   message26 = xmldoc26[0].firstChild.nodeValue;\n";
   javaScript += "   message27 = xmldoc27[0].firstChild.nodeValue;\n";
   javaScript += "   message28 = xmldoc28[0].firstChild.nodeValue;\n";
   javaScript += "   message29 = xmldoc29[0].firstChild.nodeValue;\n";
   javaScript += "   message30 = xmldoc30[0].firstChild.nodeValue;\n"; 

   javaScript += "   document.getElementById('runtime1').innerHTML=message1;\n";
   javaScript += "   document.getElementById('runtime2').innerHTML=message2;\n";
   javaScript += "   document.getElementById('runtime3').innerHTML=message3;\n";
   javaScript += "   document.getElementById('runtime4').innerHTML=message4;\n";
   javaScript += "   document.getElementById('runtime5').innerHTML=message5;\n";
   javaScript += "   document.getElementById('runtime6').innerHTML=message6;\n";
   javaScript += "   document.getElementById('runtime7').innerHTML=message7;\n";
   javaScript += "   document.getElementById('runtime8').innerHTML=message8;\n";
   javaScript += "   document.getElementById('runtime9').innerHTML=message9;\n";
   javaScript += "   document.getElementById('runtime10').innerHTML=message10;\n";
   javaScript += "   document.getElementById('runtime11').innerHTML=message11;\n";
   javaScript += "   document.getElementById('runtime12').innerHTML=message12;\n";
   javaScript += "   document.getElementById('runtime13').innerHTML=message13;\n";
   javaScript += "   document.getElementById('runtime14').innerHTML=message14;\n";
   javaScript += "   document.getElementById('runtime15').innerHTML=message15;\n";
   javaScript += "   document.getElementById('runtime16').innerHTML=message16;\n";
   javaScript += "   document.getElementById('runtime17').innerHTML=message17;\n";
   javaScript += "   document.getElementById('runtime18').innerHTML=message18;\n";
   javaScript += "   document.getElementById('runtime19').innerHTML=message19;\n";
   javaScript += "   document.getElementById('runtime20').innerHTML=message20;\n";
   javaScript += "   document.getElementById('runtime21').innerHTML=message21;\n";
   javaScript += "   document.getElementById('runtime22').innerHTML=message22;\n";
   javaScript += "   document.getElementById('runtime23').innerHTML=message23;\n";
   javaScript += "   document.getElementById('runtime24').innerHTML=message24;\n";
   javaScript += "   document.getElementById('runtime25').innerHTML=message25;\n";
   javaScript += "   document.getElementById('runtime26').innerHTML=message26;\n";
   javaScript += "   document.getElementById('runtime27').innerHTML=message27;\n";
   javaScript += "   document.getElementById('runtime28').innerHTML=message28;\n";
   javaScript += "   document.getElementById('runtime29').innerHTML=message29;\n";
   javaScript += "   document.getElementById('runtime30').innerHTML=message30;\n";

   javaScript += " }\n";
   javaScript += "}\n";
   javaScript += "</Script>\n";
 }

void buildXML() 
 {
   XML = "<?xml version='1.0'?>";
   XML = "<Title>";
   XML += "<wattT>";
   XML += Satus_wattT();
   XML += "</wattT>";
   
   XML += "<wattsR>";
   XML += Satus_wattsR();
   XML += "</wattsR>";
   
   XML += "<wattsY>";
   XML += Satus_wattsY();
   XML += "</wattsY>";
   
   XML += "<wattsB>";
   XML += Satus_wattsB();
   XML += "</wattsB>"; 

   XML += "<varT>";
   XML += Satus_varT();
   XML += "</varT>";

   XML += "<varR>";
   XML += Satus_varR();
   XML += "</varR>";
  
   XML += "<varY>";
   XML += Satus_varY();
   XML += "</varY>";

   XML += "<varB>";
   XML += Satus_varB();
   XML += "</varB>"; 

   XML += "<pfA>";
   XML += Satus_pfA();
   XML += "</pfA>"; 

   XML += "<pfR>";
   XML += Satus_pfR();
   XML += "</pfR>"; 

   XML += "<pfY>";
   XML += Satus_pfY();
   XML += "</pfY>";

   XML += "<pfB>";
   XML += Satus_pfB();
   XML += "</pfB>"; 

   XML += "<vaT>";
   XML += Satus_vaT();
   XML += "</vaT>";

   XML += "<vaR>";
   XML += Satus_vaR();
   XML += "</vaR>";

   XML += "<vaY>";
   XML += Satus_vaY();
   XML += "</vaY>";

   XML += "<vaB>";
   XML += Satus_vaB();
   XML += "</vaB>";

   XML += "<vll>";
   XML += Satus_vll();
   XML += "</vll>";

   XML += "<vry>";
   XML += Satus_vry();
   XML += "</vry>";

   XML += "<vyb>";
   XML += Satus_vyb();
   XML += "</vyb>";
  
   XML += "<vbr>";
   XML += Satus_vbr();
   XML += "</vbr>";
  
   XML += "<vln>";
   XML += Satus_vln();
   XML += "</vln>";
   
   XML += "<vrphase>";
   XML += Satus_vrphase();
   XML += "</vrphase>"; 

   XML += "<vyphase>";
   XML += Satus_vyphase();
   XML += "</vyphase>";

   XML += "<vbphase>";
   XML += Satus_vbphase();
   XML += "</vbphase>";

   XML += "<currentT>";
   XML += Satus_currentT();
   XML += "</currentT>";

   XML += "<currentR>";
   XML += Satus_currentR();
   XML += "</currentR>";

   XML += "<currentY>";
   XML += Satus_currentY();
   XML += "</currentY>";

   XML += "<currentB>";
   XML += Satus_currentB();
   XML += "</currentB>";

   XML += "<frequency>";
   XML += Satus_frequency();
   XML += "</frequency>";
  
   XML += "<power>";
   XML += Satus_power();
   XML += "</power>";

   XML += "</Title>"; 

 }

void handleXML() 
 {
   buildXML();
   server.send(200, "text/xml", XML);
 }

String Satus_wattT()
 {
   return (String)_wattsT+"W";
 }

String Satus_wattsR()
 {
   return (String)_wattsR + "W" ;
 }

String Satus_wattsY()
 {
   return (String)_wattsY + "W";
 }

String Satus_wattsB()
 {
   return (String)_wattsB + "W";
 }

String Satus_varT()
 {
   return (String)_varT + "V";
 }

String Satus_varR()
 {
   return (String)_varR + "V";
 }

String Satus_varY()
 {
   return (String)_varY + "V";
 }

String Satus_varB()
 {
   return (String)_varB + "V";
 }

String Satus_pfA()
 {
   return (String)_pfA;
 }

String Satus_pfR()
 {
   return (String)_pfR;
 }

String Satus_pfY()
 {
   return (String)_pfY;
 }

String Satus_pfB()
 {
   return (String)_pfB;
 }

String Satus_vaT()
 {
   return (String)_vaT + "V";
 }

String Satus_vaR()
 {
   return (String)_vaR + "V";
 }

String Satus_vaY()
 {
   return (String)_vaY + "V";
 }

String Satus_vaB()
 {
   return (String)_vaB + "V";
 }

String Satus_vll()
 {
   return (String)_vll + "V";
 }

String Satus_vry()
 {
   return (String)_vry + "V";
 }

String Satus_vyb()
 {
   return (String)_vyb + "V";
 }

String Satus_vbr()
 {
   return (String)_vbr + "V";
 }

String Satus_vln()
 {
   return (String)_vln + "V";
 }

String Satus_vrphase()
 {
   return (String)_vrphase + "V";
 }

String Satus_vyphase()
 {
   return (String)_vyphase + "V";
 }

String Satus_vbphase()
 {
   return (String)_vbphase + "V";
 }

String Satus_currentT()
 {
   return (String)_currentT + "A";
 }

String Satus_currentR()
 { 
   return (String)_currentR + "A";
 }

String Satus_currentY()
 {
   return (String)_currentY + "A";
 }

String Satus_currentB()
 {
   return (String)_currentB + "A";
 }

String Satus_frequency()
 {
   return (String)_frequency + "Hz";
 }

String Satus_power()
 {
   return (String)_power + "Wh";
 }
