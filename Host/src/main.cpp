#include <Arduino.h>

// TCP/IP Fixes
extern "C" {
// Headers
#include "lwip/ip.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
#include "user_interface.h"
// TCP/IP Max Socket Client Connections
uint8 espconn_tcp_get_max_con(void);
sint8 espconn_tcp_set_max_con(uint8 num);
void tcpFixes() 
{
  uint8 maxtcp = espconn_tcp_get_max_con();
  sint8 maxtcpfix = espconn_tcp_set_max_con(15);
  // Turn Off ESP8266 Sleep Mode
  wifi_set_sleep_type(NONE_SLEEP_T);
}
// TCP/IP Connections Cleanup
void tcpCleanup() 
{ 
  while(tcp_tw_pcbs!=NULL) tcp_abort(tcp_tw_pcbs); 
}
};

// Headers
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

// Libraries
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>

// My Libraries
#include <HAS8266.h>
#include <HLW8012.h>
#include <IniFile.h>
#include <Telnet.h>

// Variables
HAS8266 has8266;
HLW8012 hlw8012;
WiFiClient wifi;
Telnet telnet;             // Telnet on port 23
PubSubClient mqtt(wifi);   // MQTT on port 1883
AsyncWebServer server(80); // Web Server on port 8080
AsyncWebServer ota(8266);  // OTA Updates on port 8266 
DynamicJsonBuffer jsonBuffer;

// Timers
short HELLO_COUNTER = 1;
short ZERO_COUNTER = 1;
short POWER_COUNTER = 1;

// RESET
void software_reset() 
{
  SPIFFS.begin();
  SPIFFS.remove("/setup.ini");
  SPIFFS.end();
  delayMicroseconds(2000); // delay(2000);
  ESP.restart();
}

// WIFI
void wifi_connect()
{
  // Access Point Mode
  if (has8266.getDeviceMode() == AccessPoint)
  {
    char _ssid[sizeof(has8266.getSsid()) + 1];
    has8266.getSsid().toCharArray(_ssid, sizeof(_ssid));
    WiFi.mode(WIFI_AP); // WIFI_AP or WIFI_STA
    WiFi.softAP(_ssid);
    delayMicroseconds(2000); // delay(2000);  
  }

  // Host Mode
  if (has8266.getDeviceMode() == Host)
  {
    IniFile Setup(SPIFFS, "/setup.ini");
    const size_t bufferLen = 80;
    char buffer[bufferLen];

    // INI file: [Section] Name = Value
    if (Setup.getValue("setup", "ssid", buffer, bufferLen)) has8266.setSsid((String)buffer);
    if (Setup.getValue("setup", "pass", buffer, bufferLen)) has8266.setPass((String)buffer);
    if (Setup.getValue("setup", "device", buffer, bufferLen)) has8266.setDevice((String)buffer);

    // WIFI
    char _ssid[sizeof(has8266.getSsid()) + 1], _pass[sizeof(has8266.getPass()) + 1];
    has8266.getSsid().toCharArray(_ssid, sizeof(_ssid));
    has8266.getPass().toCharArray(_pass, sizeof(_pass));
    WiFi.mode(WIFI_STA); // WIFI_AP or WIFI_STA
    WiFi.begin(_ssid, _pass);
    while (WiFi.status() != WL_CONNECTED) delay(1000);
  }

  // TCP/IP Fixes
  tcpFixes();
}

// MQTT
void mqtt_callback(char *mqtt_topic, uint8_t *mqtt_payload, unsigned int mqtt_length)
{
  // Disable WDT
  // wdt_disable();
  
  // LED ON
  digitalWrite(STATUS_PIN, STATUS_ON);

  // Message: Topic and Paylod
  String topic(mqtt_topic), payload = (char *)mqtt_payload;
  payload = payload.substring(0, mqtt_length);

  //  Log
  JsonObject &msg = jsonBuffer.parseObject(payload);
  if (!msg.success())
  {
      digitalWrite(STATUS_PIN, STATUS_OFF);
      return;
  }

  // Message Control
  if (topic == "control")
  {
    String relay = msg["lamp"].as<String>();
    if (relay == "on") digitalWrite(RELAY_PIN, RELAY_ON);
    else digitalWrite(RELAY_PIN, RELAY_OFF);
  }

  // Message Hello
  if (topic == "hello")
  {
    // TODO
  }

  // LED OFF
  digitalWrite(STATUS_PIN, STATUS_OFF);

  // Enable WDT
  // wdt_enable();
}
void mqtt_reconnect()
{
  while (!mqtt.connected())
  {
    if (WiFi.status() != WL_CONNECTED) wifi_connect();
    char _device[sizeof(has8266.getDevice()) + 1];
    has8266.getDevice().toCharArray(_device, sizeof(_device));
    if (mqtt.connect(_device))
    {
      mqtt.subscribe("hello");
      mqtt.subscribe("control");
      break;
    }
    else delay(1000);
  }
}

// WEB [POST]
void onPost(AsyncWebServerRequest *request)
{
  // Disable WDT
  wdt_disable();

  // Access Point Mode
  if (has8266.getDeviceMode() == AccessPoint)
  {
    // INI
    File fileIni = SPIFFS.open("/setup.ini", "w+");
    fileIni.print("[setup]\r\n");
    fileIni.print("device = " + request->arg("device") + "\r\n");
    fileIni.print("ssid = " + request->arg("ssid") + "\r\n");
    fileIni.print("pass = " + request->arg("pass") + "\r\n");
    fileIni.close();

    // RESTART
    SPIFFS.end();
    delayMicroseconds(2000); // delay(2000);
    ESP.restart();
  }
  // Host Mode
  else request->send(SPIFFS, "/404.html");
}
// WEB [404]
void on404(AsyncWebServerRequest *request)
{
  request->send(SPIFFS, "/404.html");
}

// HLW8012
// When using interrupts we have to call the library entry point whenever an interrupt is triggered
void ICACHE_RAM_ATTR hlw8012_cf1_interrupt() {
    hlw8012.cf1_interrupt();
}
void ICACHE_RAM_ATTR hlw8012_cf_interrupt() {
    hlw8012.cf_interrupt();
}

// Timer Update 
void TimerUpdate()
{
      // Disable WDT
      // wdt_disable();
  
      // LED ON
      digitalWrite(STATUS_PIN, STATUS_ON);

      // Tripple HELLO
      if (HELLO_COUNTER <= 3)
      {
        String hello = "{\"name\":\"" + has8266.getDevice() + "\"}";
        mqtt.publish("hello", hello.c_str());
        HELLO_COUNTER++;
      }

      // Tripple POWER
      if (digitalRead(RELAY_PIN) == RELAY_ON && POWER_COUNTER <= 3)
      {
        JsonObject &object = jsonBuffer.createObject();
        object.set("power", hlw8012.getActivePower()); // W
        object.set("current", hlw8012.getCurrent());   // A
        object.set("voltage", hlw8012.getVoltage());   // V

        String payload = "";
        object.printTo(payload);
        mqtt.publish("power", payload.c_str());

        // TODO
        telnet.print(hlw8012.getInfo());

        ZERO_COUNTER = 1;
        POWER_COUNTER++;
      }

      // Tripple ZERO
      if (digitalRead(RELAY_PIN) == RELAY_OFF && ZERO_COUNTER <= 3)
      {
        JsonObject &object = jsonBuffer.createObject();
        object.set("power", 0);   // W
        object.set("voltage", 0); // V
        object.set("current", 0); // A        
        String power = "";
        object.printTo(power);
        mqtt.publish("power", power.c_str());

        POWER_COUNTER = 1;
        ZERO_COUNTER++;
      }

      // LED OFF
      digitalWrite(STATUS_PIN, STATUS_OFF);

      // Enable WDT
      // wdt_enable();
}
Ticker timerUpdate(TimerUpdate, 5000); // 5 sec

// Timer ARP Update
extern "C" {
// extern char *netif_list;
uint8_t etharp_request(char *, char *);
}
void TimerARP() 
{
    // Send ARP packet
    char *netif = (char*)netif_list;
    while (netif)
    {
        etharp_request((netif), (netif + 4));
        netif = *((char **)netif);
    }

    // TCP/IP Cleanup
    tcpCleanup();

    // WiFi Reconnect
    // WiFi.reconnect();
}
Ticker timerARP(TimerARP, 600000); // 10 min

// SETUP
void setup()
{
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

  // HAS8266 [RESET]
  pinMode(RESET_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RESET_PIN), software_reset, FALLING);

  // SPIFFS
  if (!SPIFFS.begin())
  {
    SPIFFS.format();
    delayMicroseconds(5000); // delay(5000);
    ESP.restart();
  }

  // HAS8266 [ACCESS POINT MODE] or [HOST MODE]
  if (!SPIFFS.exists("/setup.ini")) has8266.setDeviceMode(AccessPoint);
  else  has8266.setDeviceMode(Host);

  // WIFI
  wifi_connect();

  // Telnet
  telnet.begin();
  telnet.print("TELNET [OK]");

  // OTA Updates  
  ota.begin();
  ArduinoOTA.begin();

  // HAS8266 [ACCESS POINT MODE]
  if (has8266.getDeviceMode() == AccessPoint)
  {
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("setup.html");
    server.on("/setup.html", HTTP_POST, onPost);
    server.onNotFound(on404);
    server.begin();
  }

  // HAS8266 [HOST MODE]
  if (has8266.getDeviceMode() == Host)
  {
    // MDNS
    MDNS.begin("HOST");
    IPAddress mqtt_server; // Locate MQTT Server by mDNS
    int n = 0;
    do
    {
      n = MDNS.queryService("mqtt", "tcp");
      if (n > 0)
      {
        mqtt_server = MDNS.IP(0);
        break;
      }
      delay(1000);
    } while (n == 0);

    // MQTT
    mqtt.setServer(mqtt_server, 1883);
    mqtt.setCallback(mqtt_callback);
    
    /* // HLW8012
     * CF_PIN, CF1_PIN and SEL_PIN are GPIOs to the HLW8012 IC.
     * CURRENT_MODE uses SEL_PIN to select current sampling.
     * Set USE_INTERRUPTS to TRUE to use interrupts to monitor pulse widths.
     * Leave PULSE_TIMEOUT to the default value, recommended when using interrupts. */
    hlw8012.begin(CF_PIN, CF1_PIN, SEL_PIN, CURRENT_MODE, true);
    /* // RESISTORS
     * These values are used to calculate current, voltage and power factors as per datasheet formula.
     * These are the nominal values for the Sonoff POW resistors:
     * - The CURRENT_RESISTOR is the 1 milli Ohm copper-manganese resistor in series with the main line.
     * - The VOLTAGE_RESISTOR_UPSTREAM are the 5470 kOhm resistors in the voltage divider that feeds the V2P pin in the HLW8012.
     * - The VOLTAGE_RESISTOR_DOWNSTREAM is the 1 kOhm resistor in the voltage divider that feeds the V2P pin in the HLW8012. */
    hlw8012.setResistors(CURRENT_RESISTOR, VOLTAGE_RESISTOR_UPSTREAM, VOLTAGE_RESISTOR_DOWNSTREAM);
    // INTERRUPTS
    attachInterrupt(CF1_PIN, hlw8012_cf1_interrupt, FALLING);
    attachInterrupt(CF_PIN, hlw8012_cf_interrupt, FALLING);

    // TIMER
    timerUpdate.start(); 
    timerARP.start(); 
  }
}

// LOOP
void loop()
{
  // OTA Updates
  ArduinoOTA.handle();

  // HAS8266 [HOST MODE]
  if (has8266.getDeviceMode() == Host)
  {
    // Connect or Reconnect and Say Hello
    char _device[sizeof(has8266.getDevice()) + 1];
    has8266.getDevice().toCharArray(_device, sizeof(_device));
    if (!mqtt.loop())
    {
      mqtt.connect(_device);
      HELLO_COUNTER = 1;
      ZERO_COUNTER = 1;
      POWER_COUNTER = 1;
    }
    if (!mqtt.connected())
    {
      mqtt_reconnect();
      HELLO_COUNTER = 1;
      ZERO_COUNTER = 1;
      POWER_COUNTER = 1;
    }

    // TIMER
    timerUpdate.update(); 
    timerARP.update(); 
  }
  
  // RESET WDT
  yield();
}