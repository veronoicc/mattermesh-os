#include "services/mqtt_uplink.h"
#include <vector>
#include <cassert>
#include <iostream>
#include <cstring>
using namespace mqtt_uplink;
static Packet mk(const std::vector<uint8_t> &b,uint32_t ep=1742083629,uint16_t ms=191){Packet p;p.epoch=ep;p.ms=ms;p.len=b.size();memcpy(p.raw,b.data(),b.size());for(int i=0;i<8;i++)p.hash[i]=0;return p;}
static const std::string K="A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3D4E5F60718293A4B5C6D7E8F90";
static std::string fmt(const Packet &p,const std::string &o="ag loft rpt"){std::string s="keep";bool ok=format(p,o,K,s);assert(ok);return s;}
int main(){
 // 2025-03-16 00:07:09 UTC = 1742083629
 Packet f;{std::vector<uint8_t> b={0x15,0x00};for(int i=0;i<85;i++)b.push_back(uint8_t(i));f=mk(b);}
 f.snrTenths=40;f.rssiTenths=-930;uint8_t h1[8]={0xAC,0x9D,0x2D,0xDD,0xD8,0x39,0x57,0x12};memcpy(f.hash,h1,8);
 std::string raw1;{static const char*d="0123456789ABCDEF";for(int i=0;i<f.len;i++){raw1+=d[f.raw[i]>>4];raw1+=d[f.raw[i]&15];}}
 assert(fmt(f)=="{\"origin\": \"ag loft rpt\", \"origin_id\": \""+K+"\", \"timestamp\": \"2025-03-16T00:07:09.191000\", \"type\": \"PACKET\", \"direction\": \"rx\", \"time\": \"00:07:09\", \"date\": \"16/3/2025\", \"len\": \"87\", \"packet_type\": \"5\", \"route\": \"F\", \"payload_len\": \"85\", \"raw\": \""+raw1+"\", \"SNR\": \"4\", \"RSSI\": \"-93\", \"score\": \"1000\", \"hash\": \"AC9D2DDDD8395712\"}");
 Packet d;{std::vector<uint8_t> b={0x0A,0x02,0xC2,0xE2};for(int i=0;i<18;i++)b.push_back(0x10+i);d=mk(b);}
 uint8_t h2[8]={0x89,0x0B,0xFA,0x30,0x69,0xFD,0x12,0x50};memcpy(d.hash,h2,8);d.snrTenths=115;d.rssiTenths=-5;
 std::string s=fmt(d);
 assert(s.find("\"len\": \"22\", \"packet_type\": \"2\", \"route\": \"D\", \"payload_len\": \"18\", \"raw\": \"0A02C2E210111213")!=std::string::npos);
 assert(s.find("\"SNR\": \"11.5\", \"RSSI\": \"-0.5\", \"score\": \"1000\", \"hash\": \"890BFA3069FD1250\", \"path\": \"C2 -> E2\"}")!=std::string::npos&&s.back()=='}');
 // 2-byte hashes
 {std::vector<uint8_t> b={0x0A,0x42,0xC2,0x01,0xE2,0x02,0x99};s=fmt(mk(b));assert(s.find("\"payload_len\": \"1\"")!=std::string::npos&&s.find("\"path\": \"C201 -> E202\"")!=std::string::npos);}
 // transport codes
 {std::vector<uint8_t> b={0x10,1,2,3,4,0x00,0xAA,0xBB};s=fmt(mk(b));assert(s.find("\"route\": \"TF\"")!=std::string::npos&&s.find("\"packet_type\": \"4\"")!=std::string::npos&&s.find("\"payload_len\": \"2\"")!=std::string::npos&&s.find("\"path\": \"AA\"")==std::string::npos);
  b={0x13,1,2,3,4,0x00,0xAA};s=fmt(mk(b));assert(s.find("\"route\": \"TD\"")!=std::string::npos&&s.find("\"payload_len\": \"1\"")!=std::string::npos&&s.find("\"path\"")==std::string::npos);
  b={0x10,1,2,3,4};std::string o="x";assert(!format(mk(b),"a",K,o)&&o=="x");}
 // epoch edges
 {Packet e=mk({0x01,0x00,7});e.epoch=0;e.ms=0;s=fmt(e);assert(s.find("\"timestamp\": \"1970-01-01T00:00:00.000000\"")!=std::string::npos&&s.find("\"date\": \"1/1/1970\"")!=std::string::npos&&s.find("\"time\": \"00:00:00\"")!=std::string::npos);
  e.epoch=1709251199;e.ms=999;s=fmt(e);assert(s.find("2024-02-29T23:59:59.999000")!=std::string::npos&&s.find("\"date\": \"29/2/2024\"")!=std::string::npos);
  e.snrTenths=-25;e.rssiTenths=-1005;s=fmt(e);assert(s.find("\"SNR\": \"-2.5\", \"RSSI\": \"-100.5\"")!=std::string::npos);
  e.snrTenths=0;e.rssiTenths=-100;s=fmt(e);assert(s.find("\"SNR\": \"0\", \"RSSI\": \"-10\"")!=std::string::npos);}
 // escaping
 {Packet e=mk({0x01,0x00,7});s=fmt(e,std::string("a\"b\\c\n\t\x01 \xC3\xA9"));assert(s.find("\"origin\": \"a\\\"b\\\\c\\n\\t\\u0001 \xC3\xA9\",")!=std::string::npos);}
 // truncated / invalid
 {std::string o="keep";assert(!format(mk({0x01}),"a",K,o)&&!format(mk({}),"a",K,o)&&!format(mk({0x01,0x02,0xC2}),"a",K,o)&&!format(mk({0x01,0x42,1,2,3}),"a",K,o));
  Packet big=mk({0x01,0x00});big.len=RawMax+1;assert(!format(big,"a",K,o)&&o=="keep");
  Packet mx=mk({0x01,0x00});mx.len=RawMax;assert(format(mx,"a",K,o)&&o.find("\"payload_len\": \"254\"")!=std::string::npos);}
 // topic / status / client id
 assert(topic("SEA","a1b2")=="meshcore/SEA/A1B2/packets"&&topic("SEA","a1b2","status")=="meshcore/SEA/A1B2/status");
 assert(statusJson(true,"n","ab",1742083629)=="{\"status\": \"online\", \"origin\": \"n\", \"origin_id\": \"AB\", \"timestamp\": \"2025-03-16T00:07:09.000000\"}");
 assert(statusJson(false,"n","ab",0)=="{\"status\": \"offline\", \"origin\": \"n\", \"origin_id\": \"AB\"}");
 assert(clientId("A1B2C3D4E5F6")=="mm-a1b2c3d4"&&clientId("AB")=="mm-ab"&&clientId(K).size()<=23);
 // queue
 {Packet st[3];Queue q;assert(q.size()==0&&q.capacity()==0);Packet x;assert(!q.pop(x));assert(!q.push(x)&&q.dropped()==1);
  q.attach(st,3);assert(q.capacity()==3&&q.size()==0);q.clear();
  for(uint32_t i=1;i<=3;i++){Packet p;p.epoch=i;assert(q.push(p));}
  assert(q.size()==3&&q.dropped()==1);
  Packet p;p.epoch=4;assert(!q.push(p));p.epoch=5;assert(!q.push(p));assert(q.size()==3&&q.dropped()==3);
  assert(q.pop(x)&&x.epoch==3);p.epoch=6;assert(q.push(p));
  assert(q.pop(x)&&x.epoch==4&&q.pop(x)&&x.epoch==5&&q.pop(x)&&x.epoch==6&&!q.pop(x)&&q.size()==0);
  p.epoch=9;q.push(p);q.clear();assert(q.size()==0&&q.dropped()==3&&!q.pop(x));
  Queue z;z.attach(st,0);assert(z.capacity()==0&&!z.push(p)&&z.size()==0);}
 // backoff
 {Backoff b;uint32_t e[]={5000,10000,20000,40000,80000,160000,300000,300000,300000};for(uint32_t v:e)assert(b.next()==v);b.reset();assert(b.next()==5000&&b.next()==10000);}
 std::cout<<"PASS: mqtt_uplink format/queue/backoff"<<"\n";}
