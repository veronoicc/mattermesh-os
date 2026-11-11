#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
// RPT Scanner wire formats, as MeshCore's repeater firmware (examples/simple_repeater/MyMesh.cpp) and the companion UI
// (examples/companion_radio/ui-new/UITask.cpp) use them: the zero-hop discover request and its replies, the login and status
// replies of a repeater and the answer to a trace. Pure, host tested by sim/test_rpt_rank.cpp.
namespace matter {
constexpr uint8_t RPT_CTL_DISCOVER_REQ=0x80,RPT_CTL_DISCOVER_RESP=0x90,RPT_ADV_REPEATER=2;
// {0x80|prefix only}{type filter: one bit per advert type}{tag 4}: the repeaters answer with the first 8 bytes of their key.
inline size_t rptDiscoverRequest(uint8_t *out,uint32_t tag){out[0]=RPT_CTL_DISCOVER_REQ|1;out[1]=uint8_t(1<<RPT_ADV_REPEATER);memcpy(out+2,&tag,4);return 6;}
struct RptDiscoverReply {float snrOut=0;uint8_t key[32]={};size_t keyLen=0;};
// {0x90|type}{SNR x4 at the repeater}{tag 4}{key 8 or 32}; only a repeater's reply to this scan's tag.
inline bool rptParseDiscover(const uint8_t *p,size_t n,uint32_t wantTag,RptDiscoverReply &out){
 if(n<6+8||(p[0]&0xF0)!=RPT_CTL_DISCOVER_RESP||(p[0]&0x0F)!=RPT_ADV_REPEATER)return false;
 uint32_t tag;memcpy(&tag,p+2,4);if(tag!=wantTag)return false;
 size_t key=n-6;if(key!=8&&key!=32)return false;
 out.snrOut=int8_t(p[1])/4.0f;out.keyLen=key;memset(out.key,0,sizeof(out.key));memcpy(out.key,p+6,key);return true;}
// Login reply {tag 4}{0 = RESP_SERVER_LOGIN_OK}{keep alive}{admin}{permissions}...; the oldest repeaters answer {tag 4}"OK".
inline bool rptParseLogin(const uint8_t *data,size_t n,bool &admin){
 if(n>=6&&!memcmp(data+4,"OK",2)){admin=true;return true;}
 if(n>=8&&data[4]==0){admin=data[6]&1;return true;}
 return false;}
// examples/simple_repeater RepeaterStats, little endian; an older firmware sends a shorter one: size is how much was there.
struct RptStats {bool valid=false;int size=0;uint16_t battMv=0,txQueue=0;int16_t noiseFloor=0,lastRssi=0;uint32_t recv=0,sent=0,airSecs=0,upSecs=0,sentFlood=0,sentDirect=0,recvFlood=0,recvDirect=0;uint16_t errEvents=0;float lastSnr=0;uint16_t directDups=0,floodDups=0;uint32_t rxAirSecs=0,recvErrors=0;};
inline bool rptParseStats(const uint8_t *p,size_t n,RptStats &out){
 if(n<24)return false;out=RptStats();out.size=int(n>56?56:n);
 auto u16=[&](size_t at){uint16_t v=0;memcpy(&v,p+at,2);return v;};auto u32=[&](size_t at){uint32_t v=0;memcpy(&v,p+at,4);return v;};
 out.battMv=u16(0);out.txQueue=u16(2);out.noiseFloor=int16_t(u16(4));out.lastRssi=int16_t(u16(6));out.recv=u32(8);out.sent=u32(12);out.airSecs=u32(16);out.upSecs=u32(20);
 if(n>=40){out.sentFlood=u32(24);out.sentDirect=u32(28);out.recvFlood=u32(32);out.recvDirect=u32(36);}
 if(n>=48){out.errEvents=u16(40);out.lastSnr=int16_t(u16(42))/4.0f;out.directDups=u16(44);out.floodDups=u16(46);}
 if(n>=56){out.rxAirSecs=u32(48);out.recvErrors=u32(52);}
 out.valid=true;return true;}
inline std::string rptDuration(uint32_t s){char t[32];if(s>=86400)snprintf(t,sizeof(t),"%lud %luh %02lum",(unsigned long)(s/86400),(unsigned long)(s/3600%24),(unsigned long)(s/60%60));else snprintf(t,sizeof(t),"%luh %02lum %02lus",(unsigned long)(s/3600),(unsigned long)(s/60%60),(unsigned long)(s%60));return t;}
// The status as short lines, only the fields the repeater's firmware sent.
inline std::vector<std::string> rptStatsLines(const RptStats &s){
 std::vector<std::string> out;char t[96];
 snprintf(t,sizeof(t),"Up %s / %s",rptDuration(s.upSecs).c_str(),s.battMv?(std::to_string(s.battMv)+" mV").c_str():"battery n/a");out.push_back(t);
 if(s.size>=48)snprintf(t,sizeof(t),"Noise %d RSSI %d SNR %.1f",s.noiseFloor,s.lastRssi,s.lastSnr);else snprintf(t,sizeof(t),"Noise %d RSSI %d",s.noiseFloor,s.lastRssi);out.push_back(t);
 snprintf(t,sizeof(t),"RX %lu / TX %lu / queue %u",(unsigned long)s.recv,(unsigned long)s.sent,unsigned(s.txQueue));out.push_back(t);
 if(s.size>=56)snprintf(t,sizeof(t),"Air TX %lus / RX %lus",(unsigned long)s.airSecs,(unsigned long)s.rxAirSecs);else snprintf(t,sizeof(t),"Air TX %lus",(unsigned long)s.airSecs);out.push_back(t);
 if(s.size>=40){snprintf(t,sizeof(t),"Flood TX %lu / RX %lu",(unsigned long)s.sentFlood,(unsigned long)s.recvFlood);out.push_back(t);snprintf(t,sizeof(t),"Direct TX %lu / RX %lu",(unsigned long)s.sentDirect,(unsigned long)s.recvDirect);out.push_back(t);}
 if(s.size>=56)snprintf(t,sizeof(t),"Err %u / RX err %lu / dups %u+%u",unsigned(s.errEvents),(unsigned long)s.recvErrors,unsigned(s.directDups),unsigned(s.floodDups));else snprintf(t,sizeof(t),"Err %u / dups %u+%u",unsigned(s.errEvents),unsigned(s.directDups),unsigned(s.floodDups));
 if(s.size>=48)out.push_back(t);
 return out;}
// Our trace came back (the repeater forwarded it): same tag and the repeater's own hash first. snrs holds one byte per hop,
// SNR x4 as that hop heard the packet, so the first one is what the repeater heard of us.
inline bool rptParsePing(uint32_t wantTag,uint32_t tag,uint8_t flags,const uint8_t *snrs,const uint8_t *hashes,uint8_t len,const uint8_t *target,float &snrOut){
 const unsigned size=1u<<(flags&3);if(tag!=wantTag||(len/size)<1||memcmp(hashes,target,size))return false;snrOut=int8_t(snrs[0])/4.0f;return true;}
}
