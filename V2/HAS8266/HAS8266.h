#ifndef _HAS_8266_H
#define _HAS_8266_H

// Settings
#define DEVICE_NAME     "HAS8266"

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

// Device Mode: AccessPoint, GateWay or Host
enum Mode { AccessPoint, GateWay, Host };

// HAS8266
class HAS8266
{
  private:

    // Device Mode: AccessPoint, GateWay or Host
    Mode mode;

    // Private Members
    String name = DEVICE_NAME; 

  public:

    // Device Mode: AccessPoint, GateWay or Host
    Mode getMode() { return mode; };
    void setMode(String _value) 
    { 
      if(_value == "GW") mode = GateWay;
      else if(_value == "HS") mode = Host;
      else mode = AccessPoint;
    };

    // Settings
    void setName(String _value) { name = _value; };
    String getName() { return name; };

};
#endif
