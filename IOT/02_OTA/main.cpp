// Headers
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// WiFi Settings
#define SSID   "IBA_L@B"
#define PASS   ""

// Setup
void setup() 
{  
    // Connect to Wireless Network
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID,PASS);
    while (WiFi.status() != WL_CONNECTED) delay(1000);

    // OTA Updates 
    ArduinoOTA.setPort(8266); 
    ArduinoOTA.begin();
}

// Loop
void loop() 
{
    // Handle OTA Updates
    ArduinoOTA.handle();
}