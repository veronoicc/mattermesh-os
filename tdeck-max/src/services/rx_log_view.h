#pragma once
#include <stdio.h>
#include <algorithm>
#include <stdint.h>
#include <time.h>
#include <functional>
#include <string>
#include <vector>
#include "mesh/rx_log.h"
// RX Log app logic (no Arduino): type filter, the frozen view of a paused list, row / detail text and CSV lines.
namespace matter {
constexpr int RX_FILTER_COUNT=8;
enum RxFilter {RxAll,RxAdverts,RxText,RxGroup,RxAck,RxPath,RxTrace,RxOther};
constexpr uint32_t RX_LIVE=0xFFFFFFFFu;// view cutoff of a list that is not paused: every sequence shows
inline const char *rxFilterName(int f){static const char *n[]={"All","Adverts","Text","Group","ACK","Path","Trace","Other"};return n[f>=0&&f<RX_FILTER_COUNT?f:0];}
inline int rxFilterStep(int f,int d){return ((f+d)%RX_FILTER_COUNT+RX_FILTER_COUNT)%RX_FILTER_COUNT;}
// MeshCore payload types: ADVERT 4, TXT_MSG 2, GRP_TXT 5 / GRP_DATA 6, ACK 3, PATH 8, TRACE 9; the rest is Other.
inline int rxFilterOf(uint8_t type){switch(type){case 4:return RxAdverts;case 2:return RxText;case 5:case 6:return RxGroup;case 3:return RxAck;case 8:return RxPath;case 9:return RxTrace;}return RxOther;}
inline bool rxFilterMatch(int f,uint8_t type){return f==RxAll||rxFilterOf(type)==f;}
// The list the screen shows: packets of the filter up to sequence `cut` (RX_LIVE or the sequence at the pause), newest first.
inline int rxViewCount(const RxLog &l,int f,uint32_t cut){int n=0;for(int i=0;i<l.count();i++){auto p=l.newest(i);if(p->sequence<=cut&&rxFilterMatch(f,p->type()))n++;}return n;}
inline const RxPacket *rxViewAt(const RxLog &l,int f,uint32_t cut,int index){if(index<0)return nullptr;for(int i=0,n=0;i<l.count();i++){auto p=l.newest(i);if(p->sequence<=cut&&rxFilterMatch(f,p->type())&&n++==index)return p;}return nullptr;}
inline int rxViewIndexOf(const RxLog &l,int f,uint32_t cut,uint32_t sequence){for(int i=0,n=0;i<l.count();i++){auto p=l.newest(i);if(p->sequence>cut||!rxFilterMatch(f,p->type()))continue;if(p->sequence==sequence)return n;n++;}return -1;}
// Packets of the filter that arrived after the pause.
inline int rxNewCount(const RxLog &l,int f,uint32_t cut){int n=0;for(int i=0;i<l.count();i++){auto p=l.newest(i);if(p->sequence<=cut)break;if(rxFilterMatch(f,p->type()))n++;}return n;}
// First visible row: keeps `selected` (-1 = none) inside the `rows` window and the window inside the list.
inline int rxClampFirst(int first,int selected,int rows,int count){if(selected>=0){if(selected<first)first=selected;if(selected>=first+rows)first=selected-rows+1;}int most=count>rows?count-rows:0;return first<0?0:first>most?most:first;}
// "22:13:20" local clock (full: "14/11/2023 22:13:20"); "+1:02:03" since boot when the clock was not set or `uptime`.
inline std::string rxTimeText(const RxPacket &p,bool uptime,bool full=false){
 char text[24];if(p.epoch&&!uptime){time_t t=p.epoch;struct tm local;localtime_r(&t,&local);strftime(text,sizeof(text),full?"%d/%m/%Y %H:%M:%S":"%H:%M:%S",&local);}
 else{uint32_t s=p.uptimeMs/1000;snprintf(text,sizeof(text),"+%lu:%02lu:%02lu",(unsigned long)(s/3600),(unsigned long)(s/60%60),(unsigned long)(s%60));}
 return text;}
struct RxRow {std::string time,type,route,hops,rssi,snr,hash,length;};
inline RxRow rxRow(const RxPacket &p,bool uptime){
 RxRow r;char text[16];r.time=rxTimeText(p,uptime);r.type=rxTypeName(p.type());r.route=p.flood()?"F":"D";
 snprintf(text,sizeof(text),"%dh",p.hops());r.hops=text;snprintf(text,sizeof(text),"%.0fdBm",p.rssi());r.rssi=text;snprintf(text,sizeof(text),"%.1fdB",p.snr());r.snr=text;
 r.hash=rxHex(p.hash,4);snprintf(text,sizeof(text),"%uB",unsigned(p.length));r.length=text;return r;}
// Detail lines of one packet; `name` resolves a path hash (upper-case hex) to a repeater name, "" = hash only.
using RxNamer=std::function<std::string(const std::string &hex)>;
inline std::vector<std::string> rxDetail(const RxPacket &p,bool uptime,const RxNamer &name){
 std::vector<std::string> out;char text[64];auto add=[&](const char *label,const std::string &value){out.push_back(std::string(label)+value);};
 snprintf(text,sizeof(text),"%lu",(unsigned long)p.sequence);add("Sequence: ",text);
 add("Time: ",rxTimeText(p,uptime,true));
 snprintf(text,sizeof(text),"%s (%u)",rxTypeName(p.type()),p.type());add("Type: ",text);
 add("Route: ",rxRouteName(p.route()));
 snprintf(text,sizeof(text),"%u",p.version());add("Version: ",text);
 snprintf(text,sizeof(text),"%u B / payload %u B",p.length,p.payloadLength);add("Length: ",text);
 snprintf(text,sizeof(text),"%.1f dBm",p.rssi());add("RSSI: ",text);snprintf(text,sizeof(text),"%.1f dB",p.snr());add("SNR: ",text);
 add("Hash: ",rxHex(p.hash,4));
 if(p.transportCodes()){snprintf(text,sizeof(text),"%04X %04X",p.transport[0],p.transport[1]);add("Transport: ",text);}
 int hops=p.hops();
 if(hops){snprintf(text,sizeof(text),"%d x %d-byte hash",hops,p.hashSize());add("Hops: ",text);for(int i=0;i<hops;i++){std::string hex=rxHop(p,i),n=name?name(hex):std::string();out.push_back(std::to_string(i+1)+". "+hex+(n.empty()?"":" "+n));}}
 else{add("Hops: ","0");out.push_back(p.flood()?"Path: none (heard directly)":"Path: none (direct, no hops)");}
 int head=std::min<int>(p.payloadLength,RX_LOG_PAYLOAD_HEAD);
 if(head>0){snprintf(text,sizeof(text),"Payload head (%d of %u B):",head,p.payloadLength);out.push_back(text);for(int i=0;i<head;i+=8)out.push_back(mm::hexString(p.payload+i,size_t(std::min(8,head-i)),true,' '));}
 return out;}
// CSV (RFC 4180): fields with a comma, quote or line break are quoted, quotes doubled; lines end CRLF.
inline std::string rxCsvField(const std::string &s){if(s.find_first_of(",\"\r\n")==std::string::npos)return s;std::string out="\"";for(char c:s){if(c=='"')out+='"';out+=c;}return out+"\"";}
inline std::string rxCsvHeader(){return "sequence,epoch,uptime_ms,type,type_name,route,hops,rssi_dbm,snr_db,length,hash,path,payload_hex\r\n";}
inline std::string rxCsvLine(const RxPacket &p){
 char head[96],tail[48];snprintf(head,sizeof(head),"%lu,%lu,%lu,%u,",(unsigned long)p.sequence,(unsigned long)p.epoch,(unsigned long)p.uptimeMs,p.type());snprintf(tail,sizeof(tail),"%d,%.1f,%.1f,%u,",p.hops(),p.rssi(),p.snr(),unsigned(p.length));
 return head+rxCsvField(rxTypeName(p.type()))+","+rxCsvField(rxRouteName(p.route()))+","+tail+rxHex(p.hash,4)+","+rxCsvField(rxPathSummary(p,p.hops(),0))+","+rxHex(p.payload,std::min<int>(p.payloadLength,RX_LOG_PAYLOAD_HEAD))+"\r\n";}
// File name of an export: the clock when it was set, else seconds since boot.
inline std::string rxCsvName(uint32_t epoch,uint32_t uptimeSeconds){return epoch?"rxlog-"+std::to_string(epoch)+".csv":"rxlog-up"+std::to_string(uptimeSeconds)+".csv";}
}
