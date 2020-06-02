// HEADERS
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <FS.h>

// EXTERNAL LIBRARIES
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <uMQTTBroker.h>
#include <ArduinoJson.h>

// INTERNAL LIBRARIES
#include "HAS8266.h"
#include "HLW8012.h"

// VARIABLES
HAS8266 has8266;
HLW8012 hlw8012;
AsyncWebServer server(80); // WEB SERVER ON PORT 8080

// COUNTERS
byte SEND_HELLO = false;
byte SEND_POWER = false;
byte POWER = 3;

// GATEWAY
DynamicJsonDocument devices(4096);

// MQTT GATEWAY ON PORT 1883
class MQTTBroker: public uMQTTBroker 
{
  // ON CONNECT
  public: virtual bool onConnect(IPAddress addr, uint16_t client_count) 
  {
    // IP
    String ip = addr.toString();
    Serial.println("Connected MQTT Client IP: " + ip);

    // JSON
    JsonObject object;
    if (devices.containsKey(ip)) object = devices[ip];
    else object = devices.createNestedObject(ip);

    // 1. HELLO
    String msg_hello = "{\"from\":\""+WiFi.softAPIP().toString()+"\",\"to\":\""+ip+"\",\"name\":\""+has8266.getName()+"\"}";
    this->publish("hello", msg_hello);

    return true;
  }

  // ON DATA RECEIVED
  virtual void onData(String mqtt_topic, const char * mqtt_payload, uint32_t length) 
  {
    // Log
    StaticJsonDocument<1024> payload;
    DeserializationError error = deserializeJson(payload, mqtt_payload);
    if (error) return;
    
    // STOP SELF SENDING
    String ip_from = payload["from"];   
    String ip_to = payload["to"];  
    if(ip_to != WiFi.softAPIP().toString()) return;

    // DEBUG
    String msg;
    serializeJson(payload, msg);
    Serial.println("MQTT [" + mqtt_topic + "]: " + msg);

    // HELLO
    if (mqtt_topic == "hello") 
    {
      JsonObject object;
      if (devices.containsKey(ip_from)) object = devices[ip_from];
      else object = devices.createNestedObject(ip_from);

      object["name"] = payload["name"];
      object["power"] = 0; // W
      object["current"] = 0; // A
      object["voltage"] = 0; // V

      // STOP
      String msg_stop = "{\"from\":\"" + WiFi.softAPIP().toString() + "\",\"to\":\"" + ip_from + "\"}";
      this->publish("stop", msg_stop);
    }

    // POWER
    if (mqtt_topic == "power") 
    {
      JsonObject object = devices[ip_from];
      object["power"] = payload["power"]; // W
      object["current"] = payload["current"]; // A
      object["voltage"] = payload["voltage"]; // V

      // STOP
      if(POWER <= 0)
      {
        String msg_stop = "{\"from\":\"" + WiFi.softAPIP().toString() + "\",\"to\":\"" + ip_from + "\"}";
        this->publish("stop", msg_stop);
        POWER = 3;
      } else POWER--;
    }
  }

  // POST
  virtual void post(String mqtt_topic, String mqtt_payload) 
  {
     Serial.println("MQTT [" + mqtt_topic + "]: " + mqtt_payload );
     this->publish(mqtt_topic, mqtt_payload.c_str());
  }
};
MQTTBroker gw_mqtt;

// MQTT HOST ON PORT 1883
WiFiClient hs_wifi;
PubSubClient hs_mqtt(hs_wifi); 

// MQTT CALLBACK
void hs_mqtt_callback(char * mqtt_topic, uint8_t * mqtt_payload, unsigned int mqtt_length) 
{
  // Message: Topic and Paylod
  String topic(mqtt_topic), payload = (char*)mqtt_payload;
  payload = payload.substring(0, mqtt_length);

  //  Log
  DynamicJsonDocument json(1024);
  DeserializationError error = deserializeJson(json, payload);
  if (error) return;

  // STOP SELF SENDING

  String ip_from = json["from"];   
  String ip_to = json["to"];  
  if(ip_to != WiFi.localIP().toString()) return;
  Serial.println("MQTT [" + topic + "]: " + payload);

  // HELLO
  if (topic == "hello") 
  {
    SEND_HELLO = true;
    SEND_POWER = false;
  }
  
  // STOP
  if (topic == "stop") 
  {    
    SEND_HELLO = false;
    SEND_POWER = false;
  }

  // CONTROL
  if (topic == "control") 
  {
    String relay = json["relay"];
    if (relay == "1") digitalWrite(RELAY_PIN, RELAY_ON);
    else digitalWrite(RELAY_PIN, RELAY_OFF);
    SEND_HELLO = false;
    SEND_POWER = true;
  }
}

// HOST MQTT RECONNECT
void hs_mqtt_reconnect() 
{
  while (!hs_mqtt.connected()) 
  {
    if (hs_mqtt.connect(has8266.getName().c_str())) 
    {
      hs_mqtt.subscribe("hello");
      hs_mqtt.subscribe("control");
      hs_mqtt.subscribe("stop");
      SEND_HELLO = true;
      SEND_POWER = false;
      return;
    } 
    else delay(250);
  }
}

// WEB [SETUP]
void onSetupPost(AsyncWebServerRequest * request) 
{
  // ACCESS POINT MODE
  if (has8266.getMode() == AccessPoint) 
  {
    // SPIFFS
    SPIFFS.begin();

    // INI
    File file1 = SPIFFS.open("/NAME", "w");
    file1.print(request->arg("name"));
    file1.close();

    File file2 = SPIFFS.open("/MODE", "w");
    file2.print(request->arg("mode"));
    file2.close();

    // RESTART
    SPIFFS.end();
    delay(2000);
    ESP.restart();
  }
  // HOST MODE
  else request->send(SPIFFS, "/404.html");
}
// WEB [DEVICES] GET
void onDevicesGet(AsyncWebServerRequest * request) 
{
  // Async Json Response Stream ( MAX = 4KB )
  AsyncResponseStream * response = request->beginResponseStream("text/json");
  response->addHeader("Access-Control-Allow-Origin", "*");
  String json;
  serializeJson(devices, json);
  response->print(json);
  request->send(response);
}
// WEB [DEVICES] POST
void onDevicesPost(AsyncWebServerRequest * request) 
{
  DynamicJsonDocument json(1024);
  DeserializationError error = deserializeJson(json, request->arg("devices"));
  if (error) request->send(200, "text/plain", "MQTT [ERROR]");
  String payload;
  serializeJson(json, payload); 
  gw_mqtt.post("control", payload); 
  request->send(200, "text/plain", "MQTT [OK]");
}
// PAGE NOT FOUND
void on404(AsyncWebServerRequest * request) 
{
  request->send(SPIFFS, "/404.html");
}

// SOFTWARE RESET
void ICACHE_RAM_ATTR software_reset() 
{
  SPIFFS.begin();
  SPIFFS.remove("/NAME");
  SPIFFS.remove("/MODE");
  SPIFFS.end();
  delay(2000);
  ESP.restart();
}

// HLW8012
// When using interrupts we have to call the library entry point whenever an interrupt is triggered
void ICACHE_RAM_ATTR hlw8012_cf1_interrupt() {
  hlw8012.cf1_interrupt();
}
void ICACHE_RAM_ATTR hlw8012_cf_interrupt() {
  hlw8012.cf_interrupt();
}

// SETUP
void setup() 
{
  // DEBUG
  Serial.begin(115200);
  Serial.println("\n\n");

  // LED (GPIO14)
  pinMode(STATUS_PIN, OUTPUT);
  digitalWrite(STATUS_PIN, STATUS_OFF);
  // RELAY (GPIO10)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  // CF (GPIO13)
  pinMode(CF_PIN, INPUT_PULLUP);
  // CF1 (GPIO12)
  pinMode(CF1_PIN, INPUT_PULLUP);
  // SEL (GPIO16)
  pinMode(SEL_PIN, OUTPUT);
  digitalWrite(SEL_PIN, LOW);

  // SOFTWARE RESET
  pinMode(RESET_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RESET_PIN), software_reset, FALLING);

  // SPIFFS
  if (!SPIFFS.begin()) 
  {
    SPIFFS.format();
    delay(5000);
    ESP.restart();
  }

  // ACCESS POINT MODE
  if (!SPIFFS.exists("/NAME") || !SPIFFS.exists("/MODE")) 
  {
    // MODE
    has8266.setMode("AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("HAS8266");

    // DEBUG
    // WiFi.printDiag(Serial);
    Serial.print("Access Point IP Address: ");
    Serial.println(WiFi.softAPIP());

    // WEB SERVER
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("setup.html");
    server.on("/setup.html", HTTP_POST, onSetupPost);
    server.onNotFound(on404);
    server.begin();
  }

  // SETTINGS
  else 
  {
    // NAME
    File file1 = SPIFFS.open("/NAME", "r");
    String buffer1 = "";
    while (file1.available()) buffer1 += file1.readString();
    file1.close();
    has8266.setName(buffer1);
    Serial.println("DEVICE: " + buffer1);

    // MODE
    File file2 = SPIFFS.open("/MODE", "r");
    String buffer2 = "";
    while (file2.available()) buffer2 += file2.readString();
    file2.close();
    has8266.setMode(buffer2);
    Serial.println("MODE: " + buffer2);
    Serial.println();
  }

  // GATEWAY
  if (has8266.getMode() == GateWay) 
  {
    // MODE    
    // WiFi.mode(WIFI_AP_STA);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("HAS8266");
   
    // DEBUG
    WiFi.printDiag(Serial);
    Serial.print("GateWay IP address: ");
    Serial.println(WiFi.softAPIP());

    // MQTT
    gw_mqtt.init();
    gw_mqtt.subscribe("hello");
    gw_mqtt.subscribe("power");

    // WEB SERVER
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("devices.html").setCacheControl("max-age=3600");
    server.on("/devices.json", HTTP_GET, onDevicesGet);
    server.on("/devices.json", HTTP_POST, onDevicesPost);
    server.onNotFound(on404);
    server.begin();
  }

  // HOST
  if (has8266.getMode() == Host) 
  {
    // MODE
    WiFi.mode(WIFI_STA);
    WiFi.begin("HAS8266");
    while (WiFi.status() != WL_CONNECTED) delay(1000);

    // DEBUG
    WiFi.printDiag(Serial);
    Serial.print("Host IP address: ");
    Serial.println(WiFi.localIP());

    // MQTT
    byte gw_mqtt_server[] = { 192, 168, 4, 1 };
    hs_mqtt.setServer(gw_mqtt_server, 1883);
    hs_mqtt.setCallback(hs_mqtt_callback);
    hs_mqtt_reconnect();

    /* // HLW8012
     * CF_PIN, CF1_PIN and SEL_PIN are GPIOs to the HLW8012 IC.
     * CURRENT_MODE uses SEL_PIN to select current sampling.
     * Set USE_INTERRUPTS to TRUE to use interrupts to monitor pulse widths.
     * Leave PULSE_TIMEOUT to the default value, recommended when using interrupts. */
    hlw8012.begin(CF_PIN, CF1_PIN, SEL_PIN, CURRENT_MODE, true);
    /* // RESISTORS
     * These values are used to calculate current, voltage and power factors as per datasheet
     * formula.
     * These are the nominal values for the Sonoff POW resistors:
     * - The CURRENT_RESISTOR is the 1 milli Ohm copper-manganese resistor in series with the
     * main line.
     * - The VOLTAGE_RESISTOR_UPSTREAM are the 5470 kOhm resistors in the voltage divider that
     * feeds the V2P pin in the HLW8012.
     * - The VOLTAGE_RESISTOR_DOWNSTREAM is the 1 kOhm resistor in the voltage divider that
     * feeds the V2P pin in the HLW8012. */
    hlw8012.setResistors(CURRENT_RESISTOR, VOLTAGE_RESISTOR_UPSTREAM, VOLTAGE_RESISTOR_DOWNSTREAM);
    // INTERRUPTS
    attachInterrupt(CF1_PIN, hlw8012_cf1_interrupt, FALLING);
    attachInterrupt(CF_PIN, hlw8012_cf_interrupt, FALLING);
  }

  // OTA UPDATES
  ArduinoOTA.setPort(8266);
  ArduinoOTA.begin();
}

// LOOP
void loop() 
{
  // HEARTBEAT
  digitalWrite(STATUS_PIN, STATUS_ON);
  
  // OTA 
  ArduinoOTA.handle();

  // HOST
  if (has8266.getMode() == Host) 
  {
    // MQTT
    if (!hs_mqtt.connected()) hs_mqtt_reconnect();
    hs_mqtt.loop();

    // HELLO
    if (SEND_HELLO) 
    {
      String hello = "{\"from\":\"" + WiFi.localIP().toString() + "\",\"to\":\"192.168.4.1\",\"name\":\"" + has8266.getName() + "\"}";     
      hs_mqtt.publish("hello", hello.c_str());
    }

    // POWER
    if (SEND_POWER) 
    {
      String payload = "";
      DynamicJsonDocument doc(1024);
      doc["from"] = WiFi.localIP().toString();
      doc["to"] = "192.168.4.1";
      doc["power"] = 0; // W
      doc["current"] = 0; // A
      doc["voltage"] = 0; // V
      if(digitalRead(RELAY_PIN) == RELAY_ON)
      {
        delay(1000); 
        doc["power"] = hlw8012.getActivePower(); // W
        doc["current"] = hlw8012.getCurrent(); // A
        doc["voltage"] = hlw8012.getVoltage(); // V
      }
      serializeJson(doc, payload);
      hs_mqtt.publish("power", payload.c_str());
    }
  }

  // WDT RESET
  delay(250);
  digitalWrite(STATUS_PIN, STATUS_OFF);
  delay(250);
}
