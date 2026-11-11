#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <memory>
#include <string>
#include <vector>
#include "services/modem.h"
// TCP (optionally TLS) client over the modem's AT sockets. It derives from WiFiClient so HTTPClient::begin(WiFiClient&,...)
// and PubSubClient take it unchanged. TLS runs on the ESP (mbedTLS over the socket), so the certificate chain is checked against
// the same root CAs as the Wi-Fi clients, whatever the module's own SSL stack offers.
namespace device {
class CellClient:public WiFiClient {
public:
 struct Tls;
 CellClient(bool tls,const char *rootCA,unsigned handshakeSeconds);
 ~CellClient()override;
 int connect(const char *host,uint16_t port)override{return connect(host,port,15000);}
 int connect(const char *host,uint16_t port,int32_t timeoutMs)override;
 int connect(IPAddress,uint16_t){return 0;}// numeric addresses are not used: hosts are resolved by the module
 uint8_t connected()override;
 void stop()override;
 int available()override;
 int read()override;
 int read(uint8_t *buf,size_t size)override;
 int peek()override;
 size_t write(uint8_t b)override{return write(&b,1);}
 size_t write(const uint8_t *buf,size_t size)override;
 void flush()override{}
 // Raw socket side used by the TLS layer.
 int rawRead(uint8_t *out,size_t n);// 0 when nothing is waiting
 int rawWrite(const uint8_t *data,size_t n);
 bool rawAlive();// the socket is open or still holds data
private:
 bool tls;const char *ca;unsigned handshakeS;int link=-1;std::vector<uint8_t> plain;size_t plainAt=0;std::unique_ptr<Tls> secure;
 bool fill();// moves waiting data into the application buffer; true if any
};
// The client of the link in use: Wi-Fi (WiFiClientSecure, unchanged behaviour) or cellular, chosen when it is created. An
// MQTT or HTTP session keeps the link it started on. rootCA null with tls: certificate not checked (used by nothing in this firmware).
class NetSecure {
public:
 NetSecure(const char *rootCA,unsigned handshakeSeconds,bool tls=true);
 WiFiClient &client(){return *base;}
 NetPath path()const{return route;}
private:
 NetPath route;std::unique_ptr<WiFiClient> base;
};
}
