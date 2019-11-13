#ifndef TELNET_H
#define TELNET_H

#include <Arduino.h>
#include <QList.h> 

// Platform ESP8266 or ESP32
#ifdef ESP8266
#include <ESPAsyncTCP.h>
#elif defined ESP32
#include <AsyncTCP.h>
#endif

// Telnet Class
class Telnet
{
   public:
      void begin();
      void print(String message);
    
  private:   
      AsyncServer *Server = NULL;
      AsyncClient *Client = NULL;
      void welcome();
      const String EndLine = "\r\n"; 
      QList<String> Buffer;
};
#endif