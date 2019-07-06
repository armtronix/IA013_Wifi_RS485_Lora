
void initWiFi() {
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
  if ( testWifi() == 20 ) {
    launchWeb(0);
    return;
  }
  else
  {
    Serial.println("Opening AP");
    setupAP();
  }
}

int testWifi(void) {
  int c = 0;
  Debugln("Wifi test...");
  while ( c < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      return (20);
    }
    delay(500);
    Serial.print(".");
    c++;
  }
  Serial.println("WiFi Connect timed out!");
  return (10);
}


void setupAP(void) {

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
    st = "<b>No networks found:</b>";
  } else {
    Serial.print(n);
    Serial.println(" Networks found");
    st = "<ul>";
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
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

      // Print to web SSID and RSSI for each network found
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
  WiFi.begin(host); // not sure if need but works
  Serial.print("Access point started with name ");
  Serial.println(host);
  inApMode = 1;
  launchWeb(1);
}

void launchWeb(int webtype) {
  Serial.println("");
  Serial.println("WiFi connected");
  //Start the web server or MQTT
  if (otaFlag == 1 && !inApMode) {
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
      if (upload.status == UPLOAD_FILE_START) {
        //Serial.setDebugOutput(true);
#ifdef ARDUINO_ARCH_ESP32
        //WiFiUDP::stop(); //esp32
#else
        WiFiUDP::stopAll();
#endif
        Serial.printf("Update: %s\n", upload.filename.c_str());
        otaCount = 300;
        uint32_t maxSketchSpace;// = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();
    });
    server.begin();
    Serial.printf("Ready! Open http://%s.local in your browser\n", host);
    MDNS.addService("http", "tcp", 80);
    //      otaTickLoop.attach(1, otaCountown);
  } else {
    //setOtaFlag(1);
    if (webtype == 1 || iotMode == 0) { //in config mode or WebControle
      if (webtype == 1) {
        webtypeGlob == 1;
        Serial.println(WiFi.softAPIP());
        server.on("/", webHandleConfig);
        server.on("/a", webHandleConfigSave);
        server.on("/gpio", webHandleGpio);
        server.on("/xml", handleXML);
      } else {
        //setup DNS since we are a client in WiFi net
        if (!MDNS.begin(host)) {
          Serial.println("Error setting up MDNS responder!");
        } else {
          Serial.println("mDNS responder started");
          MDNS.addService("http", "tcp", 80);
        }
        Serial.println(WiFi.localIP());
        server.on("/", webHandleRoot);
        server.on("/cleareeprom", webHandleClearRom);
        server.on("/gpio", webHandleGpio);
        server.on("/xml", handleXML);
      }
      //server.onNotFound(webHandleRoot);
      server.begin();
      Serial.println("Web server started");
      webtypeGlob = webtype; //Store global to use in loop()
    } else if (webtype != 1 && iotMode == 1) { // in MQTT and not in config mode
      //mqttClient.setBrokerDomain((char*) mqttServer.c_str());//naren
      //mqttClient.setPort(1883);//naren
      mqttClient.setServer((char*) mqttServer.c_str(), 1883);
      mqttClient.setCallback(mqtt_arrived);
      mqttClient.setClient(wifiClient);
      if (WiFi.status() == WL_CONNECTED) {
        if (!connectMQTT()) {
          delay(2000);
          if (!connectMQTT()) {
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


void webHandleConfig() {
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


void webHandleConfigSave() {
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

  String qmqtt_user;//qmqtt_user
  qmqtt_user = server.arg("mqtt_user");
  qmqtt_user.replace("%2F", "/");
  Serial.println("Got mqtt_user: " + qmqtt_user);
  mqtt_user = (char*) qmqtt_user.c_str();

  String qmqtt_passwd;//qmqtt_passwd
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
void webHandleRoot() {
  String s;
  s = "<!DOCTYPE HTML>\n";
  s += "<p>Wifi Bt Esp32  Board ";
  s += "</p>";
  //s += "<a href=\"/gpio\">Control GPIO</a><br />";
  buildJavascript();
  s += javaScript;
  s += "<BODY onload='process()'>\n";
   s += "<p>Energy monitor\n</p><p>Voltage: <A id='runtime1'></A>\n</p>";
  s += "<p>Current: <A id='runtime2'></A>\n</p>";
  s += "<p>Power: <A id='runtime3'></A>\n</p>";
  s += "<p>Energy: <A id='runtime4'></A>\n</p>";
  s += "<p> Powerfactor: <A id='runtime5'></A>\n</p>";
  s += "<p>Frequency: <A id='runtime6'></A>\n</p>";
  s += "</BODY>\n";
  s += "<a href=\"/cleareeprom\">Clear settings an boot into Config mode</a><br />";
  s += "\r\n\r\n";
   s += "</HTML>\n";
  Serial.println("Sending 200");
  server.send(200, "text/html", s);
}

void webHandleClearRom() {
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


void webHandleGpio() {
  String s;
  // Set GPIO according to the request



  if (server.arg("reboot") == "1")
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


void buildJavascript() {
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
  javaScript += "   xmldoc1 = xmlResponse.getElementsByTagName('volt');\n";
  javaScript += "   xmldoc2 = xmlResponse.getElementsByTagName('amps');\n";
  javaScript += "   xmldoc3 = xmlResponse.getElementsByTagName('watt');\n";
  javaScript += "   xmldoc4 = xmlResponse.getElementsByTagName('watth');\n";
  javaScript += "   xmldoc5 = xmlResponse.getElementsByTagName('Pf');\n";
  javaScript += "   xmldoc6 = xmlResponse.getElementsByTagName('freq');\n";
  javaScript += "   message1 = xmldoc1[0].firstChild.nodeValue;\n";
  javaScript += "   message2 = xmldoc2[0].firstChild.nodeValue;\n";
  javaScript += "   message3 = xmldoc3[0].firstChild.nodeValue;\n";
  javaScript += "   message4 = xmldoc4[0].firstChild.nodeValue;\n";
  javaScript += "   message5 = xmldoc5[0].firstChild.nodeValue;\n";
  javaScript += "   message6 = xmldoc6[0].firstChild.nodeValue;\n";
  javaScript += "   document.getElementById('runtime1').innerHTML=message1;\n";
  javaScript += "   document.getElementById('runtime2').innerHTML=message2;\n";
  javaScript += "   document.getElementById('runtime3').innerHTML=message3;\n";
  javaScript += "   document.getElementById('runtime4').innerHTML=message4;\n";
  javaScript += "   document.getElementById('runtime5').innerHTML=message5;\n";
  javaScript += "   document.getElementById('runtime6').innerHTML=message6;\n";
  javaScript += " }\n";
  javaScript += "}\n";
  javaScript += "</Script>\n";
}

void buildXML() {
  XML = "<?xml version='1.0'?>";
  XML = "<Title>";
  XML += "<volt>";
  XML += Satus_volt();
  XML += "</volt>";
  XML += "<amps>";
  XML += Satus_amps();
  XML += "</amps>";
  XML += "<watt>";
  XML += Satus_watt();
  XML += "</watt>";
  XML += "<watth>";
  XML += Satus_watth();
  XML += "</watth>";

  XML += "<Pf>";
  XML += Satus_pf();
  XML += "</Pf>";

  XML += "<freq>";
  XML += Satus_freq();
  XML += "</freq>";
  XML += "</Title>";

}

void handleXML() {
  buildXML();
  server.send(200, "text/xml", XML);
}

String Satus_volt()
{
  return (String)voltage+"V";
}

String Satus_amps()
{
  return (String)current + "A" ;
}

String Satus_watt()
{
  return (String)watts + "W";
}

String Satus_watth()
{
  return (String)energy + "Wh";
}

String Satus_pf()
{
  return (String)pf;
}


String Satus_freq()
{
  return (String)frequency+"Hz";
}
