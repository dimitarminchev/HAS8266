#include <Arduino.h>
#include "Telnet.h"

void Telnet::begin(void)
{
    Server = new AsyncServer(23);
    Server->onClient([&](void *s, AsyncClient *client) 
    {
        // Connect
        if (client == NULL) return;
        this->print("Connect telnet client #" + client->remoteIP().toString());
        this->Client = client;

        // Disconnect
        client->onDisconnect([&](void *s, AsyncClient *client) 
        {
            client->write("Disconnect telnet client.");
            this->Client = NULL;
        }, NULL);

        // Send Welcome Message
        this->welcome();

        // Send Buffer
        while(Buffer.size() > 0)
        {
            String Message = Buffer.front();
            Buffer.pop_front();
            this->print(Message);
        }

    }, NULL);
    Server->setNoDelay(true);
    Server->begin();
}

void Telnet::print(String Message)
{
    if (this->Client == NULL || !this->Client->connected()) 
    {
        Buffer.push_back(Message);
        return;
    }
    else this->Client->write((const char *)(Message + EndLine).c_str());
}

void Telnet::welcome()
{
    this->print("_|    _|    _|_|      _|_|_|    _|_|      _|_|      _|_|_|    _|_|_|");
    this->print("_|    _|  _|    _|  _|        _|    _|  _|    _|  _|        _|      ");
    this->print("_|_|_|_|  _|_|_|_|    _|_|      _|_|        _|    _|_|_|    _|_|_|  ");
    this->print("_|    _|  _|    _|        _|  _|    _|    _|      _|    _|  _|    _|");
    this->print("_|    _|  _|    _|  _|_|_|      _|_|    _|_|_|_|    _|_|      _|_|  ");
    this->print("");
    this->print("Home Automation System based on ESP8266");
    this->print("Source Code Repository: https://github.com/dimitarminchev/HAS8266");
    this->print("Software: Dimitar Minchev <mitko@bfu.bg>, PhD of Informatics");
    this->print("Hardware: Atanas Dimitrov <atanas@bfu.bg>, PhD of Robots and manipulators");
    this->print("");
}
