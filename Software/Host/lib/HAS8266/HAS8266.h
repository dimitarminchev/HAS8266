#ifndef _HAS_8266_H
#define _HAS_8266_H

// Headers
#include <Arduino.h>

// Settings
#define DEVICE_NAME     "HOST"
#define SSID_NAME       "HAS8266"

#define RESET_PIN       0
#define STATUS_PIN      16
#define STATUS_ON       LOW
#define STATUS_OFF      HIGH

#define RELAY_PIN       4
#define RELAY_ON        HIGH
#define RELAY_OFF       LOW

// HLW8012
#define CF1_PIN         12
#define CF_PIN          13
#define SEL_PIN         14
#define CURRENT_MODE    HIGH

// These are the nominal values for the resistors in the circuit
#define CURRENT_RESISTOR                0.001   // 0.001
#define VOLTAGE_RESISTOR_UPSTREAM       2330000 // 5*470000 // Real: 2280k
#define VOLTAGE_RESISTOR_DOWNSTREAM     1000    // 1000 // Real 1.009k

// Device Mode: AccessPoint or Host
enum Mode { AccessPoint, Host };

// HAS8266
class HAS8266
{
  private:

    // Device Mode: AccessPoint or Host
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

};
#endif

