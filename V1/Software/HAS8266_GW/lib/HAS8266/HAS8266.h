#ifndef _HAS_8266_H
#define _HAS_8266_H

// Headers
#include <Arduino.h>

// Settings
#define DEVICE_NAME     "GATEWAY"
#define SSID_NAME       "HAS8266"

#define RESET_PIN  	    0
#define STATUS_PIN		  16
#define STATUS_ON       LOW
#define STATUS_OFF      HIGH

// Device Mode: AccessPoint or GateWay
enum Mode { AccessPoint, GateWay };

// HAS8266
class HAS8266
{
  private:

    // Device Mode: AccessPoint or GateWay
    Mode deviceMode;

    // Private Members
    String device = DEVICE_NAME; 
    String ssid = SSID_NAME;  
    String password;           

  public:

    // Device Mode: AccessPoint or Host
    void setDeviceMode(Mode _value) {  deviceMode = _value; };
    Mode getDeviceMode() { return deviceMode; };

    // Settings
    void setSsid(String _value) { ssid = _value; };
    void setPass(String _value) { password = _value; };
    void setDevice(String _value) { device = _value; };
    String getSsid() { return ssid; };
    String getPass() { return password; };
    String getDevice() { return device; };

    // Convert String to IP Address
    IPAddress getIP(String IP) 
    {
      int Parts[4] = {0, 0, 0, 0};
      int Part = 0;
      for ( int i = 0; i < IP.length(); i++ )
      {
        char c = IP[i];
        if ( c == '.' )
        {
          Part++;
          continue;
        }
        Parts[Part] *= 10;
        Parts[Part] += c - '0';
      }
      IPAddress _ip( Parts[0], Parts[1], Parts[2], Parts[3] );
      return _ip;
    }

};
#endif

