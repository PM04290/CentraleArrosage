/*

*/
#include <ArduinoJson.h>
#include <FS.h>
#include <Wire.h>
#include <RtcDS3231.h>
#include "Centrale.h"
#include "Zone-html.h"
#include "Chirp.h"

#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266LLMNR.h>
#define WebServer ESP8266WebServer
#endif
#include <PubSubClient.h>

#define APssid      "CHAC"      // WiFi SSID
#define APpassword  "arrosage"  // WiFi password

#define TIMER_INTERVAL_MS       1000

const char Tlun[] = "Lundi";
const char Tmar[] = "Mardi";
const char Tmer[] = "Mercredi";
const char Tjeu[] = "Jeudi";
const char Tven[] = "Vendredi";
const char Tsam[] = "Samedi";
const char Tdim[] = "Dimanche";
const char Tnan[] = "j???";

#ifdef ESP32
const byte pinEV[] = {T0, T1, T2, T3, T4, T5, T6, T7, T8};
const byte RTCpin_SDA = SDA;
const byte RTCpin_SCL = SCL;
const byte pinWifiMode = T9;
#endif
#ifdef ESP8266
const byte pinEV[] = {D5, D6, D7, D8};
const byte RTCpin_SDA  = 2;
const byte RTCpin_SCL = 1;
const byte pinWifiMode = D3;
#endif

// Create a DS1302 object.
RtcDS3231<TwoWire> Rtc(Wire);

Centrale Arrosage;

WebServer  server(80);
WiFiClient espClient;
PubSubClient mqttclient(espClient);

volatile bool newSecond = false;

byte curMoisture = 0; // leave to 0 if no sensor
RtcTemperature curTemp;  // RTC module temperature
int curTempChirp = 20;
int curLightChirp = 0;
byte nbSecond = 0;
bool newScan = false;
const byte scanNbSecond = 20;
extern TFSconfig FSconfig;
bool mqttInitialized = false;
bool chirpInitialized = false;
bool oldEvState[NB_CHANNEL];
void updateZone() {
  if (server.arg("zone") == "rtc") {

    int yr = server.arg("DATE").substring(0, 4).toInt();
    int mt = server.arg("DATE").substring(5, 7).toInt();
    int dy = server.arg("DATE").substring(8).toInt();
    int hr = server.arg("HEURE").substring(0, 2).toInt();
    int mn = server.arg("HEURE").substring(3).toInt();

    RtcDateTime dt = RtcDateTime(yr, mt, dy, hr, mn, 0);
    Rtc.SetDateTime(dt);
    Serial.print(timeToString());

    Serial.println("RTC saved");
  } else if (server.arg("zone") == "wifi") {
    strcpy(FSconfig.data.SSID, server.arg("SSID").c_str());
    strcpy(FSconfig.data.PWD, server.arg("PWD").c_str());
    strcpy(FSconfig.data.mqttserver, server.arg("MQTTSRV").c_str());
    strcpy(FSconfig.data.mqttuser, server.arg("MQTTUSER").c_str());
    strcpy(FSconfig.data.mqttpass, server.arg("MQTTPASS").c_str());
    FSconfig.data.moisturemode = server.arg("HUMMODE").toInt();

    Arrosage.saveConfigFile();

    Serial.println(FSconfig.data.SSID);
    Serial.println(FSconfig.data.PWD);
    Serial.println(FSconfig.data.mqttserver);
    Serial.println("WIFI config saved");
  } else {
    Zone* EV = Arrosage.getZone( server.arg("zone").toInt() );
    byte days = 0;
    bitWrite(days, 0, server.arg("DAY7") == "on");
    bitWrite(days, 1, server.arg("DAY1") == "on");
    bitWrite(days, 2, server.arg("DAY2") == "on");
    bitWrite(days, 3, server.arg("DAY3") == "on");
    bitWrite(days, 4, server.arg("DAY4") == "on");
    bitWrite(days, 5, server.arg("DAY5") == "on");
    bitWrite(days, 6, server.arg("DAY6") == "on");
    EV->setDays(days);
    EV->setMoisture( server.arg("HUM").toInt() );
    EV->setCycle(0, server.arg("C1HD").toInt(), server.arg("C1MD").toInt() , server.arg("C1D").toInt());
    EV->setCycle(1, server.arg("C2HD").toInt(), server.arg("C2MD").toInt() , server.arg("C2D").toInt());
    EV->setCycle(2, server.arg("C3HD").toInt(), server.arg("C3MD").toInt() , server.arg("C3D").toInt());
    EV->setCycle(3, server.arg("C4HD").toInt(), server.arg("C4MD").toInt() , server.arg("C4D").toInt());

    EV->saveConfig();

    Serial.println("Config saved zone " + String(EV->getNum()));
  }
  String json = "{\"text\":\"Données enregistrées\",";
  json += "\"success\":\"1\"}";
  server.send(200, "application/json", json);
}

void updateGpio() {
  String gpio = server.arg("id");
  String etat = server.arg("etat");

  if ( gpio == "EV1" ) {
    Arrosage.getZone(0)->setForced(etat == "1");
  } else if ( gpio == "EV2" ) {
    Arrosage.getZone(1)->setForced(etat == "1");
  } else if ( gpio == "EV3" ) {
    Arrosage.getZone(2)->setForced(etat == "1");
  } else {
    Arrosage.getZone(3)->setForced(etat == "1");
  }

  String json = "{\"gpio\":\"" + String(gpio) + "\",";
  json += "\"etat\":\"" + String(etat) + "\",";
  json += "\"success\":\"1\"}";

  server.send(200, "application/json", json);

  Serial.println("Web zone forced " + gpio);
}

void handleIndex() {
  String html;

#ifdef ESP32
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
#endif
#ifdef ESP8266
  server.chunkedResponseModeStart(200, "text/html");
#endif

  File f = SPIFFS.open("/index.html", "r");
  if (f) {
    while (f.available()) {
      html = f.readStringUntil('\n') + '\n';
      if (html.indexOf('%') > 0) {
        byte days;
        Tcycle cyc;

        if (html.indexOf("%INFOZONE%") > 0) {
          String blocZ;
          server.sendContent("<div class='row'>");
          for (byte z = 0; z < NB_CHANNEL; z++) {
            blocZ = String(info_zone_html);
            blocZ.replace("#Z#", String(z + 1));
            server.sendContent(blocZ);
            if ((z > 0) && (z % 4) == 3) {
              server.sendContent("</div>\n<div class='row'>");
            }
          }
          server.sendContent("</div>");
          html.replace("<!--%INFOZONE%-->", "");
        } else if (html.indexOf("%MANUZONE%") > 0) {
          String blocZ;
          for (byte z = 0; z < NB_CHANNEL; z++) {
            blocZ = String(manu_zone_html);
            blocZ.replace("#Z#", String(z + 1));
            server.sendContent(blocZ);
          }
          html.replace("<!--%MANUZONE%-->", "");
        } else if (html.indexOf("%CNFZONE%") > 0) {
          String blocZ, blocC;
          for (byte z = 0; z < NB_CHANNEL; z++) {
            //Serial.println("ZONE" + String(z));
            blocZ = String(cnf_zone_html);
            blocZ.replace("#Z#", String(z + 1));
            blocZ.replace("#z#", String(z));
            days = Arrosage.getZone(z)->getDays();
            blocZ.replace("%CNFZ_DAY1%", (days & 0x01) > 0 ? "checked" : "");
            blocZ.replace("%CNFZ_DAY2%", (days & 0x02) > 0 ? "checked" : "");
            blocZ.replace("%CNFZ_DAY3%", (days & 0x04) > 0 ? "checked" : "");
            blocZ.replace("%CNFZ_DAY4%", (days & 0x08) > 0 ? "checked" : "");
            blocZ.replace("%CNFZ_DAY5%", (days & 0x10) > 0 ? "checked" : "");
            blocZ.replace("%CNFZ_DAY6%", (days & 0x20) > 0 ? "checked" : "");
            blocZ.replace("%CNFZ_DAY7%", (days & 0x40) > 0 ? "checked" : "");
            blocZ.replace("%CNFZ_HUM%", String(Arrosage.getZone(z)->getMoisture()));
            blocC = "";
            for (byte c = 0; c < MAX_CYCLE_PER_DAY; c++) {
              //Serial.println("  CYCLE" + String(c));
              blocC += String(cnf_zone_cycle_html);
              //Serial.println("  CYCLE>" + String(c));
              blocC.replace("#Z#", String(z + 1)); // Z avant C
              blocC.replace("#C#", String(c + 1));
              cyc = Arrosage.getZone(z)->getCycle(c);
              blocC.replace("%CNFZC_HD%", String(cyc.startHour) );
              blocC.replace("%CNFZC_MD%", String(cyc.startMinute) );
              blocC.replace("%CNFZC_D%", String(cyc.Duration) );
            }
            blocZ.replace("<!--CNFCYCLE-->", blocC);
            server.sendContent(blocZ);
          }
          html.replace("<!--%CNFZONE%-->", "");
        } else if (html.indexOf("%JSZONE%") > 0) {
          String blocZ;
          for (byte z = 0; z < NB_CHANNEL; z++) {
            blocZ = String(zone_js);
            blocZ.replace("#Z#", String(z + 1));
            server.sendContent(blocZ);
          }
          html.replace("%JSZONE%", "");
        } else {
          html.replace("%CNFSSID%", FSconfig.data.SSID);
          html.replace("%CNFPWD%", FSconfig.data.PWD);
          html.replace("%CNFMQTTSRV%", FSconfig.data.mqttserver);
          html.replace("%CNFMQTTUSER%", FSconfig.data.mqttuser);
          html.replace("%CNFMQTTPASS%", FSconfig.data.mqttpass);
          html.replace("%CNFHUM0%", (FSconfig.data.moisturemode == 0) ? "checked" : "");
          html.replace("%CNFHUMC0%", (FSconfig.data.moisturemode == 0) ? "active" : "");
          html.replace("%CNFHUM1%", (FSconfig.data.moisturemode == 1) ? "checked" : "");
          html.replace("%CNFHUMC1%", (FSconfig.data.moisturemode == 1) ? "active" : "");
          html.replace("%CNFHUM2%", (FSconfig.data.moisturemode == 2) ? "checked" : "");
          html.replace("%CNFHUMC2%", (FSconfig.data.moisturemode == 2) ? "active" : "");
        }

      }
      server.sendContent(html);
    }
    f.close();
  }

#ifdef ESP32
  // Send zero length chunk to terminate the HTTP body
  server.sendContent("");
#endif
#ifdef ESP8266
  server.chunkedResponseFinalize();
#endif
}

void listDir(const char * dirname) {
  Serial.printf("Listing directory: %s\n", dirname);

#ifdef ESP8266
  Dir root = SPIFFS.openDir(dirname);
  while (root.next()) {
    File file = root.openFile("r");
    Serial.print("FILE: ");
    Serial.print(root.fileName());
    Serial.print(" SIZE: ");
    Serial.println(file.size());
    file.close();
  }
#endif
#ifdef ESP32
  File root = SPIFFS.open(dirname);
  if (!root.isDirectory()) {
    Serial.println("No Dir");
  }
  File file = root.openNextFile();
  while (file) {
    Serial.print("  FILE: ");
    Serial.print(file.name());
    Serial.print("\tSIZE: ");
    Serial.println(file.size());
    file = root.openNextFile();
  }
#endif
}

#ifdef ESP8266
void ICACHE_RAM_ATTR TimerHandler()
{
  newSecond = true;
}
#endif

#ifdef ESP32
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile SemaphoreHandle_t timerSemaphore;
void IRAM_ATTR TimerHandler() {
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  newSecond = true;
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}
#endif

bool configTimer(float frequency)
{
  bool isOKFlag = true;
#ifdef ESP8266
  // ESP8266 only has one usable timer1, max count is only 8,388,607. So to get longer time, we use max available 256 divider
  // Will use later if very low frequency is needed.
  float _frequency  = 80000000 / 256;
  uint32_t _timerCount = (uint32_t) _frequency / frequency;

  if ( _timerCount > 8388607)
  {
    _timerCount = 8388607;
    // Flag error
    isOKFlag = false;
  }
  Serial.println("TimerInterrupt: _fre = " + String(_frequency) + ", _count = " + String(_timerCount));
  // Clock to timer (prescaler) is always 80MHz, even F_CPU is 160 MHz
  timer1_attachInterrupt(TimerHandler);
  timer1_write(_timerCount);
  // Interrupt on EGDE, autoloop
  timer1_enable(TIM_DIV256, TIM_EDGE, TIM_LOOP);
#endif
#ifdef ESP32
  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();
  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(0, 80, true);
  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &TimerHandler, true);
  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 1000000, true);
  // Start an alarm
  timerAlarmEnable(timer);
#endif
  return isOKFlag;
}

void configWifi() {
  pinMode(pinWifiMode, INPUT_PULLUP);
  if (digitalRead(pinWifiMode) == true && strlen(FSconfig.data.SSID) > 0) {
    // Mode normal
    WiFi.begin (FSconfig.data.SSID, FSconfig.data.PWD);
    int tentativeWiFi = 0;
    // Attente de la connexion au réseau WiFi / Wait for connection
    while ( WiFi.status() != WL_CONNECTED ) {
      delay ( 500 ); Serial.print ( "." );
      tentativeWiFi++;
      if ( tentativeWiFi > 20 ) {
        // TODO ESP.reset();
        while (true)
          delay(1);
      }
    }
    // Connexion WiFi établie / WiFi connexion is OK
    Serial.println ( "" );
    Serial.print(F("Connected to ")); Serial.println ( FSconfig.data.SSID );
    Serial.print(F("IP address: ")); Serial.println ( WiFi.localIP() );
  } else {
    // Mode AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(APssid, APpassword);
    // Default IP Address is 192.168.4.1
    // if you want to change uncomment below
    // softAPConfig (local_ip, gateway, subnet)

    Serial.println();
    Serial.print(F("AP WIFI :")); Serial.println ( APssid );
    Serial.print(F("AP IP Address: ")); Serial.println(WiFi.softAPIP());
  }

#ifdef ESP8266
  LLMNR.begin("chac");
#endif
#ifdef ESP32
  if (MDNS.begin("chac")) {
    MDNS.addService("http", "tcp", 80);
  }
#endif
}

void configWeb() {
  // https://github.com/IU5HKU/ESP8266-ServerSentEvents
  server.on("/publish", handleSSEdata);
  server.on("/gpio", updateGpio);
  server.on("/cnfzone", updateZone);

  server.serveStatic("/js", SPIFFS, "/js");
  server.serveStatic("/fonts", SPIFFS, "/fonts");
  server.serveStatic("/css", SPIFFS, "/css");
  server.serveStatic("/img", SPIFFS, "/img");
  //server.serveStatic("/", SPIFFS, "/index.html");
  server.on("/", handleIndex);

  server.begin();
  Serial.println (F("HTTP server started"));
}

String getStringPartByNr(String data, char separator, int index)
{
  // spliting a string and return the part nr index
  // split by separator

  int stringData = 0;        //variable to count data part nr
  String dataPart = "";      //variable to hole the return text

  for (int i = 0; i < data.length(); i++) { //Walk through the text one letter at a time
    if (data[i] == separator) {
      //Count the number of times separator character appears in the text
      stringData++;
    } else if (stringData == index) {
      //get the text when separator is the rignt one
      dataPart.concat(data[i]);
    } else if (stringData > index) {
      //return text and stop if the next separator appears - to save CPU-time
      return dataPart;
    }
  }
  //return text if this is the last part
  return dataPart;
}

void mqttcallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT => ");
  Serial.print(topic);
  Serial.print(" = ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  //
  String TopicBase = getStringPartByNr(String(topic), '/', 0);
  if (TopicBase == "chac" && length < 20) {
    // Node detection
    String Node = getStringPartByNr(String(topic), '/', 1);
    String Cmd = getStringPartByNr(String(topic), '/', 2);
    if (Node.startsWith("ev")) {
      int zone;
      int n = sscanf(Node.c_str(), "ev%d", &zone);
      if (Cmd == "set" && zone > 0 && zone <= NB_CHANNEL) {
        zone--;
        long value = 0;
        String sval = "";
        char sValue[20];
        strncpy(sValue, (char*)payload, length);
        sValue[length] = 0;
        if ((char)payload[0] >= '0' && (char)payload[0] <= '9') {
          value = atoi((char*)sValue);
        } else {
          sval = String((char*)sValue);
          if (sval == "ON" || sval == "on") {
            value = 1;
          } else if (sval == "OFF" || sval == "off") {
            value = 0;
          }
        }
        Serial.println("=> Zone " + String(zone) + " : " + String(value));
        Arrosage.setZoneForced(zone, value);
      }
    } else if (Node == "moisture" && Cmd == "value" && Arrosage.isMoistureMQTT()) {
      char sValue[20];
      strncpy(sValue, (char*)payload, length);
      sValue[length] = 0;
      curMoisture = atof((char*)sValue);
      Serial.println(curMoisture);
    }
  }
}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttclient.connect("chac", FSconfig.data.mqttuser, FSconfig.data.mqttpass)) {
      Serial.println("connected");

      // subscribe
      mqttclient.subscribe("chac/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(57600);
  while (Serial.availableForWrite() == false) {
    delay(50);
  }
  Serial.println(F("Start debug"));

  if (!SPIFFS.begin()) {
    Serial.println(F("SPIFFS Mount failed"));
  } else {
    Serial.println(F("SPIFFS Mount succesfull"));
    listDir("/");
  }
  delay(50);

  Arrosage.checkConfigFile(false);
  Serial.println("Config file ready");
  for (byte z = 0; z < NB_CHANNEL; z++) {
    Arrosage.addZone(z + 1, pinEV[z]);
    oldEvState[z] = false;
    Serial.println("Zone added " + String(z + 1));
  }

  configWifi();
  configWeb(); // After Rtc.bebin who init Wire

  Rtc.Begin();
  if (Arrosage.isMoistureChirp()) {
    ChirpSetup();
  }
  if (configTimer(1)) {
    Serial.println(F("Starting Timer OK"));
  }
  else {
    Serial.println(F("Can't set Timer correctly."));
  }
  Serial.println(F("CHAC ready !"));
  if (Arrosage.isMoistureChirp()) {
    delay(500);
    ChirpReadMoisture(); // to put Chirp in I2C mode
    chirpInitialized = true;
  }
  if (Arrosage.isMQTT()) {
    mqttclient.setServer(FSconfig.data.mqttserver, 1883);
    mqttclient.setCallback(mqttcallback);
    mqttInitialized = true;
  }
}

bool isTimerFired() {
  // réarme automatiquement
#ifdef ESP8266
  if (newSecond) {
    newSecond = false;
    return true;
  }
#endif
#ifdef ESP32
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {
    // Read the interrupt count and time
    portENTER_CRITICAL(&timerMux);
    newSecond = false;
    portEXIT_CRITICAL(&timerMux);
    return true;
  }
#endif
  return false;
}

void loop() {
  server.handleClient();

  if (mqttInitialized) {
    if (!mqttclient.connected()) {
      mqttReconnect();
    }
    mqttclient.loop();
  }

  if (isTimerFired()) {
    // Every second
    nbSecond++;
    newScan = nbSecond >= scanNbSecond;
  }
  if (newScan) {
    // Every scan period
    curTemp = Rtc.GetTemperature();

    if (chirpInitialized) {
      curMoisture = ChirpReadMoisture();
      //curTempChirp = ChirpReadTemperature();
      //curLightChirp = ChirpReadLight();
    }
    //
    Arrosage.process(Rtc.GetDateTime(), curMoisture);
    if (mqttInitialized) {
      Zone* EV;
      for (byte z = 0; z < NB_CHANNEL; z++) {
        EV = Arrosage.getZone(z);
        bool EvState = EV->getEVstate();
        if (EvState != oldEvState[z]) {
          char topic[20];
          sprintf(topic, "chac/ev%d/state", z + 1);
          mqttclient.publish( topic, EvState ? "on" : "off");
          oldEvState[z] = EvState;
        }
      }
    }
    //
    Serial.println(getAllHeap());
    //
    nbSecond = 0;
    newScan = false;
  }
}

String getAllHeap() {
  char temp[300];
  sprintf(temp, "Heap: Free:%i, Min:%i, Size:%i, Alloc:%i", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap());
  return temp;
}

String timeToString() {
  char datestring[20];
  RtcDateTime dt = Rtc.GetDateTime();
  sprintf_P(datestring,
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Day(),
            dt.Month(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
  return String(datestring);
}

void handleSSEdata() {
  WiFiClient client = server.client();
  if (client) {
    serverSentEventHeader(client);
    String jsonData;

    jsonData = "{\"eltid\":\"horodatage\", \"value\":\"" + timeToString() + "\"}";
    serverSentEventData(client, "info", jsonData);

    jsonData = "{\"eltid\":\"temperature\", \"value\":\"" + String(curTemp.AsFloatDegC(), 1) + "&deg;\"}";
    serverSentEventData(client, "info", jsonData);

    jsonData = "{\"eltid\":\"moisture\", \"value\":\"" + String(curMoisture) + "%\"}";
    serverSentEventData(client, "info", jsonData);

    jsonData = "{\"eltid\":\"info_nextcycle\", \"value\":\"" + displayNextCycle() + "\"}";
    serverSentEventData(client, "info", jsonData);

    for (byte z = 0; z < NB_CHANNEL; z++) {
      String info = displayZoneNextCycle(z);
      jsonData = "{\"eltid\":\"info_zone" + String(z + 1) + "\", \"value\":\"" + info + "\"}";
      serverSentEventData(client, "info", jsonData);
    }
    client.stop();
  }
}

void serverSentEventHeader(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Connection: keep-alive");
  client.println("Content-Type: text/event-stream");
  client.println("Cache-Control: no-cache");
  client.println();
  client.flush();
}

void serverSentEventData(WiFiClient client, String eventName, String data) {
  if (data != NULL) {
    client.println("event: " + eventName);
    client.println("data: " + data);
    client.println();
    client.flush();
  }
}

String displayNextCycle() {
  byte nextZ, nextC, nextD, nextH, nextM;
  byte memoDay, memoHr, memoMn = 0xFF;
  Zone* EV;
  RtcDateTime t = Rtc.GetDateTime();

  if (!Rtc.IsDateTimeValid()) {
    return "<div class='alert alert-danger' role='alert'><span class='glyphicon glyphicon-exclamation-sign' aria-hidden='true'></span>&nbsp;Défaut hodatage</div>";
  }

  int actualTS = ((t.DayOfWeek()) * DAY_MN) + (t.Hour() * HOUR_MN) + t.Minute();
  int dayTS, tmpTS, curTS = OVERLAP_WEEK_TIME;
  for (byte z = 0; z < NB_CHANNEL; z++) {
    EV = Arrosage.getZone(z);
    if (EV->isConfigValid()) {
      // day count since next run
      byte days = EV->getDays();
      for (byte d = (t.DayOfWeek()); d < (t.DayOfWeek() + 7); d++) {
        // found day running
        if (days & (1 << (d % 7))) {
          dayTS = d * DAY_MN;
          Tcycle cyc;
          // find valid cycle
          for (byte c = 0; c < MAX_CYCLE_PER_DAY; c++) {
            cyc = EV->getCycle(c);
            if (cyc.Duration > 0 && cyc.Duration <= 240 && cyc.startHour < 24 && cyc.startMinute < 60) {
              tmpTS = dayTS + ((int)cyc.startHour * HOUR_MN) + cyc.startMinute;
              if (d == (t.DayOfWeek()) && cyc.startHour <= t.Hour() && cyc.startMinute <= t.Minute()) {
                tmpTS += DAY_MN * 7;
              }
              if (tmpTS >= actualTS && tmpTS < curTS) {
                curTS = tmpTS;
                nextZ = z + 1;
                nextC = c + 1;
                nextD = (d % 7);
                nextH = cyc.startHour;
                nextM = cyc.startMinute;
              }
            }
          }
        }
      }
    }
  }
  char txtbuf[500];
  if (curTS < OVERLAP_WEEK_TIME) {
    sprintf(txtbuf, "Zone %d : %3s %2d:%02d", nextZ, dayAsString(nextD), nextH, nextM);
  } else {
    strcpy(txtbuf, "<div class='alert alert-danger' role='alert'><span class='glyphicon glyphicon-exclamation-sign' aria-hidden='true'></span>&nbsp;Aucun cycle actif.</div>");
  }
  return String(txtbuf);
}

String displayZoneNextCycle(byte zone) {
  byte nextC, nextD, nextH, nextM;
  byte memoDay, memoHr, memoMn = 0xFF;
  Zone* EV;
  RtcDateTime t = Rtc.GetDateTime();

  if (!Rtc.IsDateTimeValid()) {
    return "<div class='alert alert-danger' role='alert'><span class='glyphicon glyphicon-exclamation-sign' aria-hidden='true'></span>&nbsp;Défaut hodatage</div>";
  }

  int actualTS = ((t.DayOfWeek()) * DAY_MN) + (t.Hour() * HOUR_MN) + t.Minute();
  int dayTS, tmpTS, curTS = OVERLAP_WEEK_TIME;

  EV = Arrosage.getZone(zone);
  if (EV->isConfigValid()) {
    // day count since next run
    byte days = EV->getDays();
    for (byte d = (t.DayOfWeek()); d < (t.DayOfWeek() + 7); d++) {
      // found day running
      if (days & (1 << (d % 7))) {
        dayTS = d * DAY_MN;
        Tcycle cyc;
        // find valid cycle
        for (byte c = 0; c < MAX_CYCLE_PER_DAY; c++) {
          cyc = EV->getCycle(c);
          if (cyc.Duration > 0 && cyc.Duration <= 240 && cyc.startHour < 24 && cyc.startMinute < 60) {
            tmpTS = dayTS + ((int)cyc.startHour * HOUR_MN) + cyc.startMinute;
            if (d == (t.DayOfWeek()) && cyc.startHour <= t.Hour() && cyc.startMinute <= t.Minute()) {
              tmpTS += DAY_MN * 7;
            }
            if ( tmpTS >= actualTS && tmpTS < curTS) {
              curTS = tmpTS;
              nextC = c + 1;
              nextD = (d % 7);
              nextH = cyc.startHour;
              nextM = cyc.startMinute;
            }
          }
        }
      }
    }
  }
  char txtbuf[500];
  if (curTS < OVERLAP_WEEK_TIME) {
    char buf[10];
    strcpy(buf, dayAsString(nextD));
    buf[3] = 0;
    sprintf(txtbuf, "<div class='alert alert-info' role='alert'><span class='glyphicon glyphicon-time' aria-hidden='true'></span>&nbsp;Prochain cycle : %3s %2d:%02d</div>", buf, nextH, nextM);
  } else {
    strcpy(txtbuf, "<div class='alert alert-danger' role='alert'><span class='glyphicon glyphicon-exclamation-sign' aria-hidden='true'></span>&nbsp;Aucun cycle actif.</div>");
  }
  if (EV->getEVstate()) {
    strcat(txtbuf, "<div class='alert alert-success' role='alert'><span class='glyphicon glyphicon-cloud-download' aria-hidden='true'></span>&nbsp;Marche</div>");
  }
  return String(txtbuf);
}

const char* dayAsString(byte day) {

  switch (day) {
    case 0:
      return Tdim;
    case 1:
      return Tlun;
    case 2:
      return Tmar;
    case 3:
      return Tmer;
    case 4:
      return Tjeu;
    case 5:
      return Tven;
    case 6:
      return Tsam;
  }
  return Tnan;
}
