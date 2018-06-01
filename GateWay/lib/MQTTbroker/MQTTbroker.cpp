#include <Arduino.h>
#include "MQTTbroker.h"

// Public: Set CallBack
void MQTTbroker::setCallback(callback_t cb)
{
    callback = cb;
    runCallback = false;
}

// Public: Unset CallBack
void MQTTbroker::unsetCallback(void)
{
    callback = NULL;
    runCallback = false;
}

// Public: Begin
void MQTTbroker::begin(void)
{    
    SERVER = new AsyncServer(1883);
    SERVER->onClient([&](void * arg, AsyncClient * connected)
    {
        if (connected == NULL) return;

        // Locate and Remove Duplicate Client
        for(int index = 0; index < CLIENTS.size(); index++) 
        if( CLIENTS.at(index)->remoteIP() == connected->remoteIP() || // THIS IP
            CLIENTS.at(index)->remoteIP() == IPAddress(0,0,0,0) )     // ZERO IP
        {
            CLIENTS.at(index)->close(true);
            CLIENTS.at(index)->free();
            delete CLIENTS.at(index);
            CLIENTS.clear(index);
        }  
        CLIENTS.push_back(connected);

        // Receive
        connected->onData([&](void *arg, AsyncClient *client, void *buff, size_t len) 
        {
            this->parse(client, (uint8_t *)buff, (uint8_t)len);
        }, NULL);

    }, NULL);
    SERVER->setNoDelay(true);
    SERVER->begin();
}

// Public: Publish
void MQTTbroker::publish(IPAddress ip, String topic, String payload)
{
    uint8_t *_topic = (uint8_t *)topic.c_str();
    uint8_t *_payload = (uint8_t *)payload.c_str();

    // Locate Client and Send Message 
    for(int i = 0; i < CLIENTS.size(); i++) 
    if(ip == CLIENTS.at(i)->remoteIP())
    {
        message(CLIENTS.at(i), _topic, topic.length(), _payload, payload.length());
        break;
    }
}

// Public: Subscribe
void MQTTbroker::subscribe(String topic)
{
  Subscriptions.concat(topic+" ");
}

// Public: Unsubscribe
void MQTTbroker::unsubscribe(String topic)
{  
  Subscriptions.replace(topic,"");
}

// Public: Connected Clients
QList<String> MQTTbroker::clients()
{
    QList<String> result;
    for(int i = 0; i < CLIENTS.size(); i++)  
    result.push_back(CLIENTS.at(i)->remoteIP().toString());
    return result;
}

// Private: Parse
void MQTTbroker::parse(AsyncClient *client, uint8_t *payload, uint8_t length)
{
    switch (*payload >> 4)
    {
    case CONNECT:
    {
        uint8_t variable_header[2];
        uint8_t Protocol_level = payload[8];
        uint8_t Connect_flags = payload[9];
        variable_header[0] = 0x01 & SESSION_PRESENT_ZERO;
        if (Connect_flags & WILL_FLAG)
        {
        }
        if (Connect_flags & WILL_QOS)
        {
        }
        if (Connect_flags & WILL_RETAIN)
        {
        }
        if (Connect_flags & PASSWORD_FLAG)
        {
        }
        if (Connect_flags & USER_NAME_FLAG)
        {
        }
        if (Protocol_level == MQTT_VERSION_3_1_1)
        {
            variable_header[1] = CONNECT_ACCEPTED;
            answer(client, CONNACK, 0, 2, variable_header, 2);
        }
        else
        {
            variable_header[1] = CONNECT_REFUSED_UPV;
            answer(client, CONNACK, 0, 2, variable_header, 2);
        }
    }
    break;
    case PUBLISH:
    {
        uint8_t DUP = (*payload >> 3) & 0x1;
        uint8_t QoS = (*payload >> 1) & 0x3;
        uint8_t RETAIN = (*payload) & 0x1;
        uint8_t Remaining_length = payload[1];
        uint16_t Length_topic_name = MSB_LSB(&payload[2]);
        if (Length_topic_name > 255) Length_topic_name = MQTTBROKER_TOPIC_MAX_LENGTH;
        uint8_t *Packet_identifier = NULL;
        uint8_t Packet_identifier_length = 0;
        if (QoS > 0)
        {
            Packet_identifier = &payload[4 + Length_topic_name];
            Packet_identifier_length = 2;
        }
        topic(client, &payload[4], Length_topic_name, Packet_identifier, &payload[4 + Packet_identifier_length + Length_topic_name], Remaining_length - 2 - Packet_identifier_length - Length_topic_name, RETAIN);
        switch (QoS)
        {
        case 0:
            break;
        case 1:
            answer(client, PUBACK);
            break;
        case 2:
            answer(client, PUBREC);
            answer(client, PUBREL);
            answer(client, PUBCOMP);
            break;
        }
        if (runCallback)
        {
            delay(0);
            callback(client->remoteIP(), DAT_STR(&payload[4], Length_topic_name), &payload[4 + Packet_identifier_length + Length_topic_name], Remaining_length - 2 - Packet_identifier_length - Length_topic_name);
            runCallback = false;
        }
    }
    break;
    case SUBSCRIBE:
    {
        uint16_t Packet_identifier = MSB_LSB(&payload[2]);
        uint16_t Length_MSB_LSB = MSB_LSB(&payload[4]);
        uint8_t Requesteed_QoS = payload[6 + Length_MSB_LSB];
        // TODO: subscribe
        // if (!subscribe(num, &payload[6], Length_MSB_LSB)) Requesteed_QoS = 0x80;
        answer(client, SUBACK, 0, 3, &payload[2], 2, &Requesteed_QoS, 1);
    }
    break;
    case UNSUBSCRIBE:
    {
        uint16_t Packet_identifier = MSB_LSB(&payload[2]);
        uint16_t Length_MSB_LSB = MSB_LSB(&payload[4]);
        // TODO: unsubscribe
        // unsubscribe(num, &payload[6], Length_MSB_LSB);
        answer(client, UNSUBACK, 0, 2, &payload[2], 2);
    }
    break;
    case PINGREQ:
        answer(client, PINGRESP);
        break;
    case DISCONNECT:
        client->close();
        break;
    }
}

// Private: Answer
void MQTTbroker::answer(AsyncClient *client, uint8_t fixed_header_comm, uint8_t fixed_header_lsb, uint8_t fixed_header_remaining_length, uint8_t *variable_header, uint8_t variable_header_length, uint8_t *payload, uint8_t payload_length)
{
    uint8_t i, answer_msg[fixed_header_remaining_length + 2];
    delay(0);
    answer_msg[0] = (fixed_header_comm << 4) | fixed_header_lsb;
    answer_msg[1] = fixed_header_remaining_length;
    for (i = 2; i < variable_header_length + 2; i++)
        answer_msg[i] = *(variable_header++);
    switch (fixed_header_comm)
    {
    case CONNACK:
        break;
    case PUBACK:
        break;
    case PUBREC:
        break;
    case PUBREL:
        break;
    case PUBCOMP:
        break;
    case SUBACK:
        answer_msg[i] = *payload;
        break;
    case UNSUBACK:
        break;
    case PINGRESP:
        break;
    default:
        return;
    }
    // TODO: Speed Test
    client->add((const char *)answer_msg, fixed_header_remaining_length + 2);
    client->send();
    // client->write((const char *)answer_msg, fixed_header_remaining_length + 2);
}

// Private: Topic
bool MQTTbroker::topic(AsyncClient *client, uint8_t *topic_name, uint8_t length_topic_name, uint8_t *packet_identifier, uint8_t *payload, uint8_t length_payload, bool retain)
{
    if (length_topic_name > MQTTBROKER_TOPIC_MAX_LENGTH) return false;

    // Subscription
    String topic = (char*)topic_name;
    topic = topic.substring(0, length_topic_name);
    int index = Subscriptions.indexOf(topic);
    if(index < 0) return false;

    if (callback) runCallback = true;
    message(client, topic_name, length_topic_name, payload, length_payload);
    return true;
}

// Private: Message
void MQTTbroker::message(AsyncClient *client, uint8_t *topic_name, uint8_t length_topic_name, uint8_t *payload, uint8_t length_payload)
{
    uint8_t i;
    uint8_t remaining_length = length_topic_name + length_payload + 2;
    uint8_t answer_msg[remaining_length];
    answer_msg[0] = (PUBLISH << 4) | 0x00;
    answer_msg[1] = remaining_length;
    answer_msg[2] = 0;
    answer_msg[3] = length_topic_name;
    for (i = 4; i < length_topic_name + 4; i++) answer_msg[i] = *(topic_name++);
    for (i = length_topic_name + 4; i < remaining_length + 2; i++) answer_msg[i] = *(payload++);
    // TODO: Speed Test
    client->add((const char *)answer_msg, remaining_length + 2);
    client->send();
    // client->write((const char *)answer_msg, remaining_length + 2);
}

// Private: Data to String
String MQTTbroker::DAT_STR(uint8_t *data, uint8_t length)
{
  String str;
  str.reserve(length);
  for (uint8_t i = 0; i < length; i++) str += (char)data[i];
  return str;
}

// Private: MSB to LSB
uint16_t MQTTbroker::MSB_LSB(uint8_t *msb_byte)
{
  return (uint16_t)(*msb_byte << 8) + *(msb_byte + 1);
}