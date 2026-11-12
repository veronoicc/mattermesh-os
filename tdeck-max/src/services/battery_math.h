#pragma once
#include <stdint.h>
#include <string>
#include <vector>
// Pure battery history, graph and drain-estimate math (services/battery_history.cpp samples and stores). No Arduino.
namespace device {
constexpr uint32_t BATTERY_SAMPLE_S=300,BATTERY_WINDOW_S=86400;// one sample every 5 minutes, kept for 24 hours
constexpr int BATTERY_CAPACITY_MAH=1400;// T-Deck Max cell, 3.7 V 1400 mAh (hardware/README.md)
enum SampleFlags:uint8_t {SampleCharging=1,SampleUsb=2};
// One 5-minute interval: gauge readings at its end and the seconds each subsystem was on within it.
struct BatterySample {uint32_t t=0;uint8_t soc=0,flags=0;uint16_t mv=0;int16_t ma=0;uint16_t spanS=0,screenS=0,gpsS=0,modemS=0,radioS=0,sleepS=0,txDs=0;};// t: epoch seconds; ma: BQ27220 current, negative = discharging; radioS: Wi-Fi or BLE on; txDs: LoRa transmit airtime in 100 ms
constexpr int BATTERY_SAMPLE_BYTES=24;
inline void putLe(uint8_t *p,uint32_t v,int n){for(int i=0;i<n;i++)p[i]=uint8_t(v>>(8*i));}
inline uint32_t getLe(const uint8_t *p,int n){uint32_t v=0;for(int i=0;i<n;i++)v|=uint32_t(p[i])<<(8*i);return v;}
inline void packSample(const BatterySample &s,uint8_t *o){putLe(o,s.t,4);o[4]=s.soc;o[5]=s.flags;putLe(o+6,s.mv,2);putLe(o+8,uint16_t(s.ma),2);putLe(o+10,s.spanS,2);putLe(o+12,s.screenS,2);putLe(o+14,s.gpsS,2);putLe(o+16,s.modemS,2);putLe(o+18,s.radioS,2);putLe(o+20,s.sleepS,2);putLe(o+22,s.txDs,2);}
inline bool unpackSample(const uint8_t *i,BatterySample &s){if(i[4]>100)return false;s.t=getLe(i,4);s.soc=i[4];s.flags=i[5];s.mv=uint16_t(getLe(i+6,2));s.ma=int16_t(getLe(i+8,2));s.spanS=uint16_t(getLe(i+10,2));s.screenS=uint16_t(getLe(i+12,2));s.gpsS=uint16_t(getLe(i+14,2));s.modemS=uint16_t(getLe(i+16,2));s.radioS=uint16_t(getLe(i+18,2));s.sleepS=uint16_t(getLe(i+20,2));s.txDs=uint16_t(getLe(i+22,2));return true;}
// The last 24 hours, oldest first, at most Max samples.
class BatteryHistory {
 std::vector<BatterySample> v;
public:
 static constexpr size_t Max=300;
 void reserve(){v.reserve(Max);}
 // false when `s` is not newer than the newest (a clock step back).
 bool add(const BatterySample &s){if(!v.empty()&&s.t<=v.back().t)return false;v.push_back(s);size_t drop=0;while(drop<v.size()&&v[drop].t+BATTERY_WINDOW_S<s.t)drop++;if(v.size()-drop>Max)drop=v.size()-Max;if(drop)v.erase(v.begin(),v.begin()+long(drop));return true;}
 const std::vector<BatterySample> &samples()const{return v;}
};
// Graph column: mean state of charge of the samples in it (-1: none) and whether any was charging.
struct GraphColumn {int soc=-1;bool charging=false;};
inline std::vector<GraphColumn> batteryGraph(const std::vector<BatterySample> &samples,uint32_t now,int columns){
 std::vector<GraphColumn> out(columns>0?size_t(columns):0);if(columns<=0)return out;std::vector<int> sum(size_t(columns),0),n(size_t(columns),0);const uint32_t start=now-BATTERY_WINDOW_S;
 for(auto &s:samples){if(s.t<start||s.t>now)continue;int c=int(uint64_t(s.t-start)*uint64_t(columns)/BATTERY_WINDOW_S);if(c>=columns)c=columns-1;sum[c]+=s.soc;n[c]++;if(s.flags&SampleCharging)out[c].charging=true;}
 for(int c=0;c<columns;c++)if(n[c])out[c].soc=(sum[c]+n[c]/2)/n[c];return out;}
// Mean gauge current of the samples within `windowS` before `now`; false when there are none.
inline bool averageCurrent(const std::vector<BatterySample> &samples,uint32_t now,uint32_t windowS,int &ma){long sum=0;int n=0;for(auto &s:samples)if(s.t<=now&&now-s.t<=windowS){sum+=s.ma;n++;}if(!n)return false;ma=int(sum>=0?(sum+n/2)/n:-((-sum+n/2)/n));return true;}
// BQ27220 TimeToEmpty / TimeToFull registers: minutes, 0xFFFF when it cannot tell.
inline int gaugeMinutes(uint16_t raw){return raw==0xFFFF?-1:int(raw);}
// Fallbacks from the state of charge and the average current (negative = discharging); -1 = unknown.
inline int timeToEmptyMinutes(int soc,int avgMa,int capacityMah=BATTERY_CAPACITY_MAH){if(avgMa>=0)return -1;if(soc<=0)return 0;return int(int64_t(soc)*capacityMah/100*60/(-avgMa));}
inline int timeToFullMinutes(int soc,int avgMa,int capacityMah=BATTERY_CAPACITY_MAH){if(avgMa<=0)return -1;if(soc>=100)return 0;return int(int64_t(100-soc)*capacityMah/100*60/avgMa);}
inline std::string durationText(int minutes){if(minutes<0)return "--";if(minutes<60)return std::to_string(minutes)+" min";if(minutes<1440){int h=minutes/60,m=minutes%60;return std::to_string(h)+" h"+(m?" "+std::to_string(m)+" min":"");}int d=minutes/1440,h=minutes%1440/60;return std::to_string(d)+" d"+(h?" "+std::to_string(h)+" h":"");}
// Seconds each subsystem was on over a window.
struct OnTime {uint32_t windowS=0,screenS=0,gpsS=0,modemS=0,radioS=0,sleepS=0,txMs=0;};
inline OnTime sumOnTime(const std::vector<BatterySample> &samples,uint32_t now,uint32_t windowS){OnTime o;for(auto &s:samples)if(s.t<=now&&now-s.t<=windowS){o.windowS+=s.spanS;o.screenS+=s.screenS;o.gpsS+=s.gpsS;o.modemS+=s.modemS;o.radioS+=s.radioS;o.sleepS+=s.sleepS;o.txMs+=uint32_t(s.txDs)*100u;}return o;}
// Nominal currents in mA (datasheet typicals, not measured on this unit): the table behind the per-subsystem estimate.
//  awake   ESP32-S3 at 80-240 MHz + SX1262 in RX + PSRAM, keyboard and touch idle (ESP32-S3 ~25-45 mA, SX1262 RX 5 mA)
//  sleep   ESP32-S3 light sleep ~1 mA + SX1262 RX ~5 mA + leakage of the rails that stay on
//  screen  front-light and keyboard backlight at their default level (the e-paper itself takes no current when still)
//  gps     MIA-M10Q tracking, continuous
//  modem   A7682E registered, idle with data session (LTE DRX; peaks of 200+ mA are averaged out)
//  radio   Wi-Fi station modem-sleep average, or BLE advertising / connected
//  tx      SX1262 PA at +22 dBm (~118 mA) minus the RX current already counted
constexpr float MA_AWAKE=40,MA_SLEEP=7,MA_SCREEN=15,MA_GPS=12,MA_MODEM=25,MA_RADIO=40,MA_TX=110;
struct DrainItem {const char *name="";float mah=0;uint32_t seconds=0;};
struct Drain {DrainItem item[7];int count=0;float totalMah=0;};
inline Drain estimateDrain(const OnTime &o){
 Drain d;uint32_t awake=o.windowS>o.sleepS?o.windowS-o.sleepS:0;
 auto add=[&](const char *name,float ma,uint32_t seconds){DrainItem &i=d.item[d.count++];i.name=name;i.seconds=seconds;i.mah=ma*float(seconds)/3600.0f;d.totalMah+=i.mah;};
 add("Awake",MA_AWAKE,awake);add("Sleep",MA_SLEEP,o.sleepS);add("Screen on",MA_SCREEN,o.screenS);add("GPS",MA_GPS,o.gpsS);add("Modem",MA_MODEM,o.modemS);add("Wi-Fi / BT",MA_RADIO,o.radioS);
 DrainItem &tx=d.item[d.count++];tx.name="LoRa TX";tx.seconds=(o.txMs+500)/1000;tx.mah=MA_TX*float(o.txMs)/3600000.0f;d.totalMah+=tx.mah;return d;}
inline float averageModelMa(const OnTime &o){return o.windowS?estimateDrain(o).totalMah*3600.0f/float(o.windowS):0;}
inline int sharePercent(float part,float total){if(total<=0)return 0;int p=int(part*100/total+0.5f);return p>100?100:p<0?0:p;}
}
