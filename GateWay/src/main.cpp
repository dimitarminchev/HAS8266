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
#include <ESP8266HTTPClient.h>

// Libraries
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <Ticker.h>
#include <ESP8266Ping.h>

// My Libraries
#include <HAS8266.h>
#include <IniFile.h>
#include <MQTTbroker.h>
#include <Telnet.h>
#include <NTPClient.h>

// Variables
HAS8266 has8266;
MQTTbroker mqtt;           // MQTT on port 1883
Telnet telnet;             // Telnet on port 23
AsyncWebServer server(80); // Web Server on port 8080
AsyncWebServer ota(8266);  // OTA Updates on port 8266
WiFiUDP udp;
NTPClient ntp(udp);        // NTP on port 1337

// JSON (Devices & History)
DynamicJsonBuffer jsonBuffer(1024);
// DynamicJsonBuffer jsonBuffer;
JsonObject &devices = jsonBuffer.createObject();
JsonObject &history = jsonBuffer.createObject();

// RESET
void software_reset()
{
    telnet.print("HAS8266 [RESET]");
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
        telnet.print("WIFI [ACCESS POINT MODE]");
        telnet.print("ACCESS POINT IP ADDRESS = " + WiFi.softAPIP().toString());     
    }

    // Host Mode
    if (has8266.getDeviceMode() == GateWay)
    {
        telnet.print("INI [LOAD]");
        IniFile Setup(SPIFFS, "/setup.ini");
        const size_t bufferLen = 80;
        char buffer[bufferLen];

        // INI file: [Section] Name = Value
        if (Setup.getValue("setup", "ssid", buffer, bufferLen)) has8266.setSsid((String)buffer);
        if (Setup.getValue("setup", "pass", buffer, bufferLen)) has8266.setPass((String)buffer);

        telnet.print("WIFI [GATEWAY MODE]");
        telnet.print("SSID: " + has8266.getSsid());
        telnet.print("PASS: " + has8266.getPass());
        telnet.print("WIFI [CONNECT] ");

        // WIFI
        char _ssid[sizeof(has8266.getSsid()) + 1], _pass[sizeof(has8266.getPass()) + 1];
        has8266.getSsid().toCharArray(_ssid, sizeof(_ssid));
        has8266.getPass().toCharArray(_pass, sizeof(_pass));
        WiFi.mode(WIFI_STA); // WIFI_AP or WIFI_STA
        WiFi.begin(_ssid, _pass);
        while (WiFi.status() != WL_CONNECTED) delay(1000);

        telnet.print("GATEWAY IP ADDRESS = " + WiFi.localIP().toString());
    }

    // TCP/IP Fixes
    tcpFixes();
}

// MQTT
void mqtt_callback(IPAddress mqtt_ip, String mqtt_topic, uint8_t *mqtt_payload, uint8_t mqtt_length)
{
    // Disable WDT
    // wdt_disable();

    // LED ON
    digitalWrite(STATUS_PIN, STATUS_ON);

    // IP Address
    String ip = String(mqtt_ip[0]) + String(".") + String(mqtt_ip[1]) + String(".") + String(mqtt_ip[2]) + String(".") + String(mqtt_ip[3]);

    // Log
    JsonObject &payload = jsonBuffer.parseObject(mqtt_payload);
    if (payload.success())
    {
        String msg;
        payload.printTo(msg);
        telnet.print("MQTT [RECEIVE] #" + ip + " [" + mqtt_topic + "] " + msg);
    }
    else
    {
        digitalWrite(STATUS_PIN, STATUS_OFF);
        return;
    }

    // Message Hello
    if (mqtt_topic == "hello")
    {
        JsonObject &object = devices.containsKey(ip) ? devices[ip] : devices.createNestedObject(ip);
        object.set("name", payload["name"].as<String>());
        object.set("power", 0);   // W
        object.set("current", 0); // A
        object.set("voltage", 0); // V
        // Hello
        String hello = "{\"name\":\"" + has8266.getDevice() + "\"}";
        mqtt.publish(mqtt_ip, "hello", hello);
        // MQTT Total Connected Clients
        QList<String> _clients = mqtt.clients();
        telnet.print("MQTT Total Connected Clients = " + (String)_clients.size());
        while(_clients.size() > 0)
        {
           String IP = _clients.front();
           _clients.pop_front();
           telnet.print("#" + IP);
        }
    }

    // Message Power
    if (mqtt_topic == "power")
    {
        JsonObject &object = devices[ip];
        object.set("power", (float)payload["power"]);     // W
        object.set("current", (float)payload["current"]); // A
        object.set("voltage", (float)payload["voltage"]); // V
    }

    // LED OFF
    digitalWrite(STATUS_PIN, STATUS_OFF);

    // Enable WDT
    // wdt_enable();
}

// WEB [SETUP]
void onSetupPost(AsyncWebServerRequest *request)
{
    // Disable WDT
    wdt_disable();

    telnet.print("HTTP [SETUP]");

    // Access Point Mode
    if (has8266.getDeviceMode() == AccessPoint)
    {
        // INI
        File fileIni = SPIFFS.open("/setup.ini", "w+");
        if (!fileIni) telnet.print("INI [ERROR]");
        fileIni.print("[setup]\r\n");
        fileIni.print("ssid = " + request->arg("ssid") + "\r\n");
        fileIni.print("pass = " + request->arg("pass") + "\r\n");
        fileIni.close();
        telnet.print("INI [SAVE]");

        // RESTART
        telnet.print("HAS8266 [RESTART]");
        SPIFFS.end();
        delayMicroseconds(2000); // delay(2000);
        ESP.restart();
    }
    // Host Mode
    else request->send(SPIFFS, "/404.html");
}
// WEB [DEVICES] GET 
void onDevicesGet(AsyncWebServerRequest *request)
{
    telnet.print("HTTP [GET] DEVICES");
    // Async Json Response Stream ( MAX = 4KB )
    AsyncResponseStream *response = request->beginResponseStream("text/json");
    response->addHeader("Access-Control-Allow-Origin", "*");
    // devices.printTo(*response);
    devices.prettyPrintTo(*response);
    request->send(response);
}
// WEB [DEVICES] POST
void onDevicesPost(AsyncWebServerRequest *request)
{
    telnet.print("HTTP [POST] DEVICES");
    JsonObject &object = jsonBuffer.parseObject(request->arg("devices"));
    if (object.success())
    {
        IPAddress ip = has8266.getIP(object["ip"]);
        String payload = "";
        object.printTo(payload);
        mqtt.publish(ip, "control", payload);
        telnet.print("MQTT [SEND] #" + ip.toString() + " [control]: " + payload);
        request->send(200, "text/plain", "MQTT [OK]");
    }
    else request->send(200, "text/plain", "MQTT [ERROR]");
}
// WEB [HISTORY]
void onHistory(AsyncWebServerRequest *request)
{
    telnet.print("HTTP [GET] HISTORY");
    // Async Json Response Stream ( MAX = 4KB )
    AsyncResponseStream *response = request->beginResponseStream("text/json");
    response->addHeader("Access-Control-Allow-Origin", "*");
    // history.printTo(*response);
    history.prettyPrintTo(*response);
    request->send(response);
}
// Page Not Found
void on404(AsyncWebServerRequest *request)
{
    telnet.print("HTTP [404]");
    request->send(SPIFFS, "/404.html");
}

// Update 
void TimerUpdate()
{
    // Disable WDT
    // wdt_disable();
  
    // LED ON
    digitalWrite(STATUS_PIN, STATUS_ON);

    // NTP
    ntp.update();
    String ntpTime = ntp.getFormattedTime();
    telnet.print("NTP [DATE] " + ntpTime);
    telnet.print("FREE HEAP: " + (String)ESP.getFreeHeap() + " B");

    // PING
    // Ping Adds ARP entry and Remove Missing Hosts
    for (auto item : devices) // using C++11 syntax
    {
        IPAddress ip = has8266.getIP(item.key);
        if(!Ping.ping(ip)) 
        {
            telnet.print("Removed host #" + (String)item.key);
            devices.remove(item.key);
        }
    }

    // HISTORY   
    for (auto item : devices) // using C++11 syntax
    {
        bool exist = history.containsKey(ntpTime);
        JsonObject &object = exist ? history[ntpTime] : history.createNestedObject(ntpTime);
        float power = 0, current = 0;
        if(exist)
        {
           power = object["power"].as<float>();
           current = object["current"].as<float>();
        }
        power += devices[item.key]["power"].as<float>();
        current += devices[item.key]["current"].as<float>();
        object.set("power", power);
        object.set("current", current);
    }

    // TCP/IP Cleanup
    tcpCleanup();

    // LED OFF
    digitalWrite(STATUS_PIN, STATUS_OFF);

    // Enable WDT
    // wdt_enable();
}
Ticker timer(TimerUpdate, 600000); // 10 min


// SETUP
void setup()
{
    telnet.print("HAS8266 [START]");

    // HAS8266 [HEART BEAT]
    pinMode(STATUS_PIN, OUTPUT);
    digitalWrite(STATUS_PIN, STATUS_OFF);

    // HAS8266 [RESET]
    pinMode(RESET_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(RESET_PIN), software_reset, FALLING);

    // SPIFFS
    if (!SPIFFS.begin())
    {
        telnet.print("SPIFFS [ERROR]");
        SPIFFS.format();
        delayMicroseconds(5000); // delay(5000);
        ESP.restart();
    }
    else telnet.print("SPIFFS [OK]");

    // HAS8266 [ACCESS POINT MODE] or [GATEWAY MODE]
    if (!SPIFFS.exists("/setup.ini")) 
    {        
        has8266.setDeviceMode(AccessPoint);
        telnet.print("HAS8266 [ACCESS POINT MODE]");
    }
    else 
    {
        has8266.setDeviceMode(GateWay);
        telnet.print("HAS8266 [GATEWAY MODE]");
    }

    // WIFI
    wifi_connect();

    // Telnet
    telnet.begin();
    telnet.print("TELNET [OK]");

    // OTA Updates
    ota.begin();
    ArduinoOTA.onStart([]() { telnet.print("Starting OTA Updates ..."); });
    ArduinoOTA.onEnd([]() { telnet.print("OTA Updates Finished"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) 
    { 
      int status = progress / (total / 100);
      telnet.print((String)status + "%"); 
    });
    ArduinoOTA.onError([](ota_error_t error) { telnet.print("OTA Update Error = " + (String)error); });
    ArduinoOTA.begin();
    telnet.print("OTA [OK]");

    // HAS8266 [ACCESS POINT MODE]
    if (has8266.getDeviceMode() == AccessPoint)
    {
        // MDNS
        if (MDNS.begin("HAS8266"))
        {
            MDNS.addService("http", "tcp", 80);
            telnet.print("MDNS [OK]");
        }
        else telnet.print("MDNS [ERROR]");

        // HTTP
        server.serveStatic("/", SPIFFS, "/").setDefaultFile("setup.html");
        server.on("/setup.html", HTTP_POST, onSetupPost);
        server.onNotFound(on404);
        server.begin();
        telnet.print("HTTP [OK]");
    }

    // HAS8266 [GATEWAY MODE]
    if (has8266.getDeviceMode() == GateWay)
    {        
        // NTP
        telnet.print("NTP ");
        ntp.begin();
        do
        {
           ntp.update(); 
           delayMicroseconds(1000); // delay(1000);
           telnet.print(".");
        }  
        while(ntp.getFormattedShortTime() == "19700101");        
        telnet.print("[OK]\nDATE = " + ntp.getFormattedTime());

        // MDNS
        if (MDNS.begin("HAS8266"))
        {
            MDNS.addService("http", "tcp", 80);
            MDNS.addService("mqtt", "tcp", 1883);
            telnet.print("MDNS [OK]");
        }
        else telnet.print("MDNS [ERROR]");

        // MQTT
        mqtt.begin();
        mqtt.setCallback(mqtt_callback);
        mqtt.subscribe("hello");
        mqtt.subscribe("power");
        telnet.print("MQTT [OK]");

        // HTTP
        server.serveStatic("/", SPIFFS, "/").setDefaultFile("devices.html").setCacheControl("max-age=3600");
        server.on("/devices.json", HTTP_GET, onDevicesGet);
        server.on("/devices.json", HTTP_POST, onDevicesPost);
        server.on("/history.json", HTTP_GET, onHistory);
        server.onNotFound(on404);
        server.begin();
        telnet.print("HTTP [OK]");
    }

    // TIMER
    timer.start(); 
}

// LOOP
void loop()
{
  // OTA Updates
  ArduinoOTA.handle();

  // TIMER
  timer.update(); 

  // RESET WDT
  yield();
}