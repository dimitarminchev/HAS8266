#ifndef MQTT_BROKER_H
#define MQTT_BROKER_H

#include <Arduino.h>
#include <QList.h> 

// Platform ESP8266 or ESP32
#ifdef ESP8266
#include <ESPAsyncTCP.h>
#elif defined ESP32
#include <AsyncTCP.h>
#endif

// MQTT
#define MQTT_VERSION_3_1_1                        4    
#define MQTTBROKER_TOPIC_MAX_LENGTH               64
#define MQTTBROKER_PAYLOAD_MAX_LENGTH             255

enum {
  CONNECT = 1,
  CONNACK = 2,
  PUBLISH = 3,
  PUBACK = 4,
  PUBREC = 5,
  PUBREL = 6,
  PUBCOMP = 7,
  SUBSCRIBE = 8,
  SUBACK = 9,
  UNSUBSCRIBE = 10,
  UNSUBACK = 11,
  PINGREQ = 12,
  PINGRESP = 13,
  DISCONNECT = 14,
}; 

enum {
  CLEAN_SESSION = 0x02,
  WILL_FLAG = 0x04,
  WILL_QOS = 0x18,
  WILL_RETAIN = 0x20,
  PASSWORD_FLAG = 0x40,
  USER_NAME_FLAG = 0x80,
};
 
enum {
  SESSION_PRESENT_ZERO = 0,
  SESSION_PRESENT_ONE = 1,
};

enum {
  CONNECT_ACCEPTED = 0,
  CONNECT_REFUSED_UPV = 1,
  CONNECT_REFUSED_IR = 2,
  CONNECT_REFUSED_SU = 3,
  CONNECT_REFUSED_BUP = 4,
  CONNECT_REFUSED_NA = 5,
};

// MQTT Call Back
typedef void(*callback_t)(IPAddress ip, String topic_name, uint8_t * payload, uint8_t length_payload);

// MQTT Broker Class
class MQTTbroker
{
  public:
    void setCallback(callback_t cb);
    void unsetCallback(void);
    void begin(void);
    void publish(IPAddress ip, String topic, String payload);
    void subscribe(String topic);
    void unsubscribe(String topic);
    QList<String> clients();

  private:   
    AsyncServer *SERVER = NULL; 
    QList<AsyncClient*> CLIENTS;
    String Subscriptions = String();
    callback_t callback;
    bool runCallback;
    void parse(AsyncClient *client, uint8_t * payload, uint8_t length);    
    void answer(AsyncClient *client, uint8_t fixed_header_comm, uint8_t fixed_header_lsb = 0, uint8_t fixed_header_remaining_length = 0, uint8_t * variable_header = NULL, uint8_t variable_header_length = 0, uint8_t * payload = NULL, uint8_t payload_length = 0);
    bool topic(AsyncClient *client, uint8_t* topic_name, uint8_t length_topic_name, uint8_t * packet_identifier, uint8_t * payload, uint8_t length_payload, bool retain = false);
    void message(AsyncClient *client, uint8_t * topic_name, uint8_t length_topic_name, uint8_t * payload, uint8_t length_payload);
    String DAT_STR(uint8_t * data, uint8_t length);
    uint16_t MSB_LSB(uint8_t * msb_byte);
};
#endif