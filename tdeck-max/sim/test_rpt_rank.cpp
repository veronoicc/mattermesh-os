#include <cassert>
#include <cstring>
#include <iostream>
#include "services/rpt_rank.h"
#include "mesh/rpt_wire.h"
using namespace mm;
static RptKey key(const char *name,bool live,bool snrKnown,float snr,uint32_t age,bool distKnown,double meters){RptKey k;k.name=name;k.live=live;k.snrKnown=snrKnown;k.snr=snr;k.ageS=age;k.distKnown=distKnown;k.meters=meters;return k;}
static std::string names(const std::vector<RptKey> &k,RptSort s){std::string out;for(int i:rptOrder(k,s)){out+=k[i].name;out+=",";}return out;}
int main(){
 // SNR: strongest first, unknown last, live before stale on equal SNR, equal and equal keeps the input order.
 std::vector<RptKey> k={key("a",false,true,5,100,false,0),key("b",true,true,5,50,false,0),key("c",true,false,0,10,false,0),key("d",true,true,9.5f,30,false,0),key("e",false,true,5,90,false,0)};
 assert(names(k,RptSort::Snr)=="d,b,a,e,c,");
 // Last heard: most recent first, unknown age last, live first on equal age.
 k={key("a",false,true,1,100,false,0),key("b",true,true,1,100,false,0),key("c",false,true,1,~0u,false,0),key("d",true,true,1,5,false,0),key("e",false,true,1,100,false,0)};
 assert(names(k,RptSort::Heard)=="d,b,a,e,c,");
 // Distance: nearest first, unknown distance last, live first on equal distance.
 k={key("a",false,true,1,1,true,500),key("b",true,true,1,1,false,0),key("c",true,true,1,1,true,500),key("d",false,true,1,1,true,20),key("e",false,true,1,1,false,0)};
 assert(names(k,RptSort::Distance)=="d,c,a,b,e,");
 // Name: case-insensitive, live first on equal names, stable.
 k={key("beta",false,true,1,1,false,0),key("Alpha",false,true,1,1,false,0),key("ALPHA",true,true,1,1,false,0),key("alpha",false,true,1,1,false,0),key("[00ff00]",false,false,0,~0u,false,0)};
 assert(names(k,RptSort::Name)=="[00ff00],ALPHA,Alpha,alpha,beta,");
 assert(rptOrder({},RptSort::Snr).empty());
 // Stale filter: unknown age counts as stale; zero hours means off.
 assert(!rptStale(10,0)&&!rptStale(~0u,0)&&!rptStale(3600,1)&&rptStale(3601,1)&&rptStale(~0u,6)&&!rptStale(6*3600,6)&&rptStale(24*3600+1,24));
 assert(rptAge(~0u)=="--"&&rptAge(5)=="5s"&&rptAge(125)=="2m"&&rptAge(7300)=="2h"&&rptAge(3*86400)=="3d");
 assert(rptHops(-1)=="flood"&&rptHops(0)=="direct"&&rptHops(1)=="1 hop"&&rptHops(3)=="3 hops");
 assert(rptTag("AB12cdef0123")=="[ab12cd]"&&rptTag("ab")=="[ab]");
 // Merge: a responder in the table is refreshed (zero hops, live, this scan's SNR and RSSI), one not in it is added by key prefix.
 {std::vector<RptNode> nodes(2);nodes[0].key="aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899";nodes[0].name="Hill";nodes[0].ageS=900;nodes[0].snr=1;nodes[0].hops=3;nodes[0].contact=true;
  nodes[1].key="1111111111111111111111111111111111111111111111111111111111111111";nodes[1].name="Far";nodes[1].ageS=50;nodes[1].live=true;nodes[1].snr=7;nodes[1].hops=1;nodes[1].contact=true;
  std::vector<RptHeard> heard(3);heard[0].prefix="AABBCCDDEEFF0011";heard[0].snrIn=6.5f;heard[0].snrOut=4;heard[0].rssi=-97;heard[0].ageS=2;
  heard[1].prefix="2222222222222222";heard[1].snrIn=-3;heard[1].rssi=-120;heard[1].ageS=1;
  heard[2].prefix="1111111111111111";heard[2].snrIn=9;heard[2].rssi=-80;heard[2].ageS=60;// older than the advert: the row keeps its data
  auto out=rptMerge(nodes,heard);assert(out.size()==3);
  assert(out[0].live&&out[0].ageS==2&&out[0].hops==0&&out[0].snr==6.5f&&out[0].rssi==-97&&out[0].scanned&&out[0].name=="Hill");
  assert(out[1].ageS==50&&out[1].snr==7&&out[1].hops==1&&!out[1].scanned);
  assert(out[2].key=="2222222222222222"&&!out[2].contact&&out[2].live&&out[2].hops==0&&out[2].snr==-3&&out[2].scanned&&rptName(out[2])=="[222222]");
  assert(rptName(out[0])=="Hill");nodes[0].name="";assert(rptName(nodes[0])=="[aabbcc]");}
 // Discover request: zero-hop control packet, repeaters only, prefix only replies.
 {uint8_t req[8];size_t n=matter::rptDiscoverRequest(req,0x01020304);assert(n==6&&req[0]==0x81&&req[1]==0x04&&!memcmp(req+2,"\x04\x03\x02\x01",4));}
 // Discover reply: type nibble, SNR the repeater heard x4, the tag back, an 8 or 32 byte key.
 {uint8_t r[6+32];r[0]=0x92;r[1]=uint8_t(int8_t(-14));uint32_t tag=0xA1B2C3D4;memcpy(r+2,&tag,4);for(int i=0;i<32;i++)r[6+i]=uint8_t(i+1);
  matter::RptDiscoverReply d;assert(matter::rptParseDiscover(r,14,tag,d)&&d.keyLen==8&&d.snrOut==-3.5f&&d.key[7]==8);
  assert(matter::rptParseDiscover(r,38,tag,d)&&d.keyLen==32&&d.key[31]==32);
  assert(!matter::rptParseDiscover(r,14,tag+1,d));// another scan's tag
  assert(!matter::rptParseDiscover(r,13,tag,d));// key shorter than a prefix
  assert(!matter::rptParseDiscover(r,20,tag,d));// neither a prefix nor a key
  r[0]=0x93;assert(!matter::rptParseDiscover(r,14,tag,d));// a room server, not a repeater
  r[0]=0x82;assert(!matter::rptParseDiscover(r,14,tag,d));// a request, not a reply
  r[0]=0x92;assert(!matter::rptParseDiscover(r,0,tag,d));}
 // Login reply: tag, RESP_SERVER_LOGIN_OK, legacy keep-alive, admin flag, permissions; the legacy "OK" repeater.
 {uint8_t r[13]={1,2,3,4,0,0,1,3,9,9,9,9,7};bool admin=false;assert(matter::rptParseLogin(r,13,admin)&&admin);r[6]=0;assert(matter::rptParseLogin(r,13,admin)&&!admin);
  uint8_t legacy[6]={1,2,3,4,'O','K'};assert(matter::rptParseLogin(legacy,6,admin)&&admin);
  r[4]=5;assert(!matter::rptParseLogin(r,13,admin));assert(!matter::rptParseLogin(r,4,admin));assert(!matter::rptParseLogin(r,6,admin));}
 // Status reply: MeshCore RepeaterStats (little endian), shorter ones from older firmware parse as far as they go.
 {uint8_t b[4+56]={};uint32_t tag=77;memcpy(b,&tag,4);uint8_t *s=b+4;auto u16=[&](int at,uint16_t v){memcpy(s+at,&v,2);};auto u32=[&](int at,uint32_t v){memcpy(s+at,&v,4);};
  u16(0,4012);u16(2,3);u16(4,uint16_t(int16_t(-112)));u16(6,uint16_t(int16_t(-94)));u32(8,1234);u32(12,567);u32(16,321);u32(20,3*86400+4*3600+5*60+6);u32(24,100);u32(28,200);u32(32,300);u32(36,400);u16(40,2);u16(42,uint16_t(int16_t(34)));u16(44,5);u16(46,6);u32(48,654);u32(52,9);
  matter::RptStats st;assert(matter::rptParseStats(b+4,56,st)&&st.valid&&st.size==56&&st.battMv==4012&&st.txQueue==3&&st.noiseFloor==-112&&st.lastRssi==-94&&st.recv==1234&&st.sent==567&&st.airSecs==321&&st.upSecs==3*86400+4*3600+5*60+6);
  assert(st.sentFlood==100&&st.sentDirect==200&&st.recvFlood==300&&st.recvDirect==400&&st.errEvents==2&&st.lastSnr==8.5f&&st.directDups==5&&st.floodDups==6&&st.rxAirSecs==654&&st.recvErrors==9);
  auto lines=matter::rptStatsLines(st);assert(lines.size()>=5);assert(lines[0]=="Up 3d 4h 05m / 4012 mV");bool noise=false;for(auto &l:lines)if(l.find("-112")!=std::string::npos)noise=true;assert(noise);
  matter::RptStats old;assert(matter::rptParseStats(b+4,40,old)&&old.size==40&&old.recvDirect==400&&old.errEvents==0&&old.rxAirSecs==0);auto oldLines=matter::rptStatsLines(old);assert(oldLines.size()>=3&&oldLines.size()<lines.size());
  matter::RptStats tiny;assert(!matter::rptParseStats(b+4,23,tiny)&&!tiny.valid);assert(matter::rptParseStats(b+4,24,tiny)&&tiny.size==24&&tiny.upSecs==st.upSecs);}
 // Ping: our trace came back with the repeater's SNR as first hop and the same hash; another tag or hash is somebody else's trace.
 {uint8_t target[4]={1,2,3,4},hashes[4]={1,2,3,4},snrs[1]={uint8_t(int8_t(22))};float out=0;
  assert(matter::rptParsePing(9,9,2,snrs,hashes,4,target,out)&&out==5.5f);assert(!matter::rptParsePing(9,8,2,snrs,hashes,4,target,out));
  hashes[3]=9;assert(!matter::rptParsePing(9,9,2,snrs,hashes,4,target,out));hashes[3]=4;assert(!matter::rptParsePing(9,9,2,snrs,hashes,0,target,out));
  uint8_t one[1]={1};assert(matter::rptParsePing(9,9,0,snrs,one,1,target,out));}
 std::cout<<"PASS: repeater sort (SNR, last heard, distance, name; unknown last, live first, stable), stale filter, scan merge, discover/login/status/ping wire formats\n";
}
