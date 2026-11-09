#pragma once
// Shared PC simulation for the companion emulators (sim/companion*_emulator.cpp): the real MeshCore core on an
// in-memory broadcast radio with a simulated clock. One translation unit per emulator binary.
#include <Arduino.h>
#include <helpers/BaseChatMesh.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/SimpleMeshTables.h>
#include <cassert>
#include <deque>
#include <random>
#include <string>
#include <vector>
static unsigned long simMs=1000;unsigned long millis(){return simMs;}
namespace {
struct Ms:mesh::MillisecondClock {unsigned long getMillis()override{return simMs;}};
struct Rng:mesh::RNG {std::mt19937 g{12345};void random(uint8_t *d,size_t n)override{for(size_t i=0;i<n;i++)d[i]=uint8_t(g());}};
struct Clock:mesh::RTCClock {uint32_t t=1790000000;uint32_t getCurrentTime()override{return t+simMs/1000;}void setCurrentTime(uint32_t v)override{t=v;}};
// Broadcast radio: what a node sends reaches the radios it can hear.
struct Air;struct Radio:mesh::Radio {Air *air=nullptr;std::deque<std::vector<uint8_t>> in;std::vector<Radio*> hears;bool busy=false;
 int recvRaw(uint8_t *d,int n)override{if(in.empty())return 0;auto b=in.front();in.pop_front();if(int(b.size())>n)return 0;memcpy(d,b.data(),b.size());return int(b.size());}
 uint32_t getEstAirtimeFor(int len)override{return 20+len;}float packetScore(float,int)override{return 1;}
 bool startSendRaw(const uint8_t *b,int n)override{for(auto *r:hears)r->in.emplace_back(b,b+n);busy=true;return true;}
 bool isSendComplete()override{return busy;}void onSendFinished()override{busy=false;}bool isInRecvMode()const override{return true;}
 float getLastSNR()const override{return 9.5f;}float getLastRSSI()const override{return -71;}};
void link(Radio &a,Radio &b){a.hears.push_back(&b);b.hears.push_back(&a);}
// The phone app's side of the link: frames written by the companion.
struct App {std::vector<std::vector<uint8_t>> frames;void write(const uint8_t *f,size_t n){frames.emplace_back(f,f+n);}};
// Radio, clocks and tables built before the mesh base class, whose constructor already uses them.
struct Parts {Radio radio;Ms ms;Rng rng;Clock clock;StaticPoolPacketManager pool{16};SimpleMeshTables tables;};
// Advance the simulated clock, looping every node every 5 ms.
template<class... N> void run(int ms,N&... nodes){for(int t=0;t<ms;t+=5){simMs+=5;(nodes.loop(),...);}}
}
