#include <cassert>
#include <iostream>
#include <stdlib.h>
#include "services/rx_log_view.h"
using namespace matter;
static RxPacket packet(uint8_t route,uint8_t type,int hops,int hashSize=2,uint32_t epoch=0,uint32_t up=0){RxPacket p;p.header=route|(type<<2);p.pathLen=((hashSize-1)<<6)|hops;for(int i=0;i<hops*hashSize&&i<RX_LOG_PATH_BYTES;i++)p.path[i]=uint8_t(0xA0+i);p.epoch=epoch;p.uptimeMs=up;p.length=87;p.payloadLength=70;p.rssiTenths=-973;p.snrTenths=55;for(int i=0;i<4;i++)p.hash[i]=uint8_t(0x1A+i*0x10);for(int i=0;i<16;i++)p.payload[i]=uint8_t(i);return p;}
int main(){
 setenv("TZ","UTC",1);tzset();
 // Filters: MeshCore payload types map to the labels; Group takes GRP_TXT and GRP_DATA; everything else is Other.
 assert(RX_FILTER_COUNT==8&&std::string(rxFilterName(RxAll))=="All"&&std::string(rxFilterName(RxAdverts))=="Adverts"&&std::string(rxFilterName(RxOther))=="Other"&&std::string(rxFilterName(99))=="All");
 assert(rxFilterOf(4)==RxAdverts&&rxFilterOf(2)==RxText&&rxFilterOf(5)==RxGroup&&rxFilterOf(6)==RxGroup&&rxFilterOf(3)==RxAck&&rxFilterOf(8)==RxPath&&rxFilterOf(9)==RxTrace);
 for(int t:{0,1,7,10,11,12,13,14,15})assert(rxFilterOf(t)==RxOther);
 for(int t=0;t<16;t++)assert(rxFilterMatch(RxAll,t));
 assert(rxFilterMatch(RxAdverts,4)&&!rxFilterMatch(RxAdverts,2)&&rxFilterMatch(RxGroup,6)&&!rxFilterMatch(RxText,5));
 assert(rxFilterStep(RxAll,1)==RxAdverts&&rxFilterStep(RxAll,-1)==RxOther&&rxFilterStep(RxOther,1)==RxAll);
 // View over a ring: newest first, filter and pause cutoff.
 static RxPacket slots[8];RxLog log;log.attach(slots,8);
 for(int i=0;i<6;i++)log.push(packet(1,i%2?4:2,i));// seq 1..6: odd seq TEXT, even seq ADVERT
 assert(rxViewCount(log,RxAll,RX_LIVE)==6&&rxViewCount(log,RxAdverts,RX_LIVE)==3&&rxViewCount(log,RxText,RX_LIVE)==3&&rxViewCount(log,RxAck,RX_LIVE)==0);
 assert(rxViewAt(log,RxAll,RX_LIVE,0)->sequence==6&&rxViewAt(log,RxAll,RX_LIVE,5)->sequence==1&&!rxViewAt(log,RxAll,RX_LIVE,6)&&!rxViewAt(log,RxAll,RX_LIVE,-1));
 assert(rxViewAt(log,RxAdverts,RX_LIVE,0)->sequence==6&&rxViewAt(log,RxAdverts,RX_LIVE,2)->sequence==2);
 assert(rxViewIndexOf(log,RxAdverts,RX_LIVE,4)==1&&rxViewIndexOf(log,RxAdverts,RX_LIVE,5)==-1&&rxViewIndexOf(log,RxAll,RX_LIVE,9)==-1);
 uint32_t cut=log.total();// paused here
 log.push(packet(1,4,0));log.push(packet(1,2,0));log.push(packet(1,4,0));// seq 7 ADVERT, 8 TEXT, 9 ADVERT while paused
 assert(rxViewCount(log,RxAll,cut)==5&&rxViewAt(log,RxAll,cut,0)->sequence==6&&rxViewCount(log,RxAll,RX_LIVE)==8);// ring holds 8: seq 1 is gone
 assert(rxNewCount(log,RxAll,cut)==3&&rxNewCount(log,RxAdverts,cut)==2&&rxNewCount(log,RxText,cut)==1&&rxNewCount(log,RxAll,RX_LIVE)==0&&rxNewCount(log,RxAll,log.total())==0);
 assert(rxNewCount(log,RxAll,0)==8);
 // Visible window keeps the selection on screen and stays inside the list.
 assert(rxClampFirst(0,3,7,20)==0&&rxClampFirst(0,9,7,20)==3&&rxClampFirst(10,2,7,20)==2&&rxClampFirst(15,-1,7,20)==13&&rxClampFirst(-4,-1,7,20)==0&&rxClampFirst(5,0,7,3)==0&&rxClampFirst(0,19,7,20)==13);
 // Row shapes.
 {auto r=rxRow(packet(1,4,3,2,1700000000,5000),false);assert(r.time=="22:13:20"&&r.type=="ADVERT"&&r.route=="F"&&r.hops=="3h"&&r.rssi=="-97dBm"&&r.snr=="5.5dB"&&r.hash=="1A2A3A4A");}
 {auto r=rxRow(packet(2,5,0,1,0,3723000),false);assert(r.time=="+1:02:03"&&r.type=="GRP_TXT"&&r.route=="D"&&r.hops=="0h"&&r.length=="87B");auto u=rxRow(packet(1,2,1,2,1700000000,3723000),true);assert(u.time=="+1:02:03");}
 for(uint8_t route=0;route<4;route++)assert(rxRow(packet(route,2,0),false).route==(route<=1?"F":"D"));
 {auto n=packet(1,2,0);n.snrTenths=-32;n.rssiTenths=-1206;auto r=rxRow(n,false);assert(r.snr=="-3.2dB"&&r.rssi=="-121dBm");}
 assert(rxTimeText(packet(1,2,0,2,1700000000,0),false,true)=="14/11/2023 22:13:20");
 // Detail: header fields, every path hash with its repeater name (ambiguous / unknown = hash only), transport codes, payload head.
 {auto p=packet(0,4,3,2);p.transport[0]=0x1234;p.transport[1]=0xBEEF;p.sequence=42;
  auto lines=rxDetail(p,false,[](const std::string &hex){return hex=="A0A1"?std::string("North Ridge"):std::string();});
  auto has=[&](const std::string &s){for(auto &l:lines)if(l==s)return true;return false;};
  assert(has("Sequence: 42")&&has("Type: ADVERT (4)")&&has("Route: T-FLOOD")&&has("Hops: 3 x 2-byte hash")&&has("Length: 87 B / payload 70 B")&&has("RSSI: -97.3 dBm")&&has("SNR: 5.5 dB")&&has("Hash: 1A2A3A4A")&&has("Transport: 1234 BEEF"));
  assert(has("1. A0A1 North Ridge")&&has("2. A2A3")&&has("3. A4A5")&&has("00 01 02 03 04 05 06 07")&&has("08 09 0A 0B 0C 0D 0E 0F"));
  auto d=packet(2,2,0);auto dl=rxDetail(d,false,nullptr);bool transport=false,direct=false;for(auto &l:dl){if(l.compare(0,10,"Transport:")==0)transport=true;if(l=="Path: none (direct, no hops)")direct=true;}assert(!transport&&direct);
  auto flood=packet(1,2,0);auto fl=rxDetail(flood,false,nullptr);bool none=false;for(auto &l:fl)if(l=="Path: none (heard directly)")none=true;assert(none);
  auto big=packet(1,2,40,1);assert(rxDetail(big,false,nullptr).size()>40);
  auto shortPayload=packet(1,3,0);shortPayload.payloadLength=3;auto sl=rxDetail(shortPayload,false,nullptr);bool three=false,more=false;for(auto &l:sl){if(l=="00 01 02")three=true;if(l=="03 04 05")more=true;}assert(three&&!more);}
 // CSV: RFC 4180 quoting, header, one line per packet.
 assert(rxCsvField("plain")=="plain"&&rxCsvField("a,b")=="\"a,b\""&&rxCsvField("say \"hi\"")=="\"say \"\"hi\"\"\""&&rxCsvField("a\nb")=="\"a\nb\""&&rxCsvField("")=="");
 assert(rxCsvHeader()=="sequence,epoch,uptime_ms,type,type_name,route,hops,rssi_dbm,snr_db,length,hash,path,payload_hex\r\n");
 {auto p=packet(1,4,2,2,1700000000,5000);p.sequence=7;assert(rxCsvLine(p)=="7,1700000000,5000,4,ADVERT,FLOOD,2,-97.3,5.5,87,1A2A3A4A,A0A1 > A2A3,000102030405060708090A0B0C0D0E0F\r\n");
  auto d=packet(2,2,0);d.sequence=1;d.payloadLength=0;assert(rxCsvLine(d)=="1,0,0,2,TEXT_MSG,DIRECT,0,-97.3,5.5,87,1A2A3A4A,,\r\n");}
 assert(rxCsvName(1700000000,5)=="rxlog-1700000000.csv"&&rxCsvName(0,3723)=="rxlog-up3723.csv");
 std::cout<<"PASS: rx log view filters, pause cutoff, row shapes, detail lines, csv, window\n";
}
