#include <cassert>
#include <cmath>
#include <iostream>
#include "services/battery_math.h"
int main(){
 using namespace device;
 // Record round trip (little endian, 24 bytes); a corrupt state of charge is refused.
 {BatterySample s;s.t=1800000000u;s.soc=87;s.flags=SampleCharging|SampleUsb;s.mv=4012;s.ma=-215;s.spanS=300;s.screenS=61;s.gpsS=300;s.modemS=0;s.radioS=299;s.sleepS=120;s.txDs=42;
  uint8_t b[BATTERY_SAMPLE_BYTES];packSample(s,b);assert(BATTERY_SAMPLE_BYTES==24&&b[0]==0x00&&b[3]==0x6B);
  BatterySample r;assert(unpackSample(b,r)&&r.t==s.t&&r.soc==87&&r.flags==3&&r.mv==4012&&r.ma==-215&&r.spanS==300&&r.screenS==61&&r.gpsS==300&&r.radioS==299&&r.sleepS==120&&r.txDs==42);
  b[4]=101;assert(!unpackSample(b,r));}
 // History: ascending time only, 24 h window, bounded size.
 {BatteryHistory h;BatterySample s;s.soc=50;s.t=1000;assert(h.add(s)&&h.samples().size()==1);s.t=1000;assert(!h.add(s));s.t=900;assert(!h.add(s));// a clock step back is not stored
  s.t=1000+BATTERY_SAMPLE_S;assert(h.add(s));
  s.t=1000+BATTERY_WINDOW_S+1;assert(h.add(s)&&h.samples().size()==2&&h.samples().front().t==1000+BATTERY_SAMPLE_S);// the first fell out of the 24 h window
  BatteryHistory big;BatterySample b;b.soc=1;for(uint32_t k=0;k<1000;k++){b.t=100000+k*10;big.add(b);}assert(big.samples().size()<=BatteryHistory::Max);assert(big.samples().back().t==100000+999*10);}
 // Graph: 24 h split in columns, oldest left; empty columns -1; mean state of charge per column; charging flagged.
 {std::vector<BatterySample> v;uint32_t now=200000;BatterySample s;
  s.t=now-BATTERY_WINDOW_S+10;s.soc=100;v.push_back(s);s.t=now-BATTERY_WINDOW_S+20;s.soc=90;s.flags=SampleCharging;v.push_back(s);
  s.flags=0;s.t=now-10;s.soc=40;v.push_back(s);
  auto g=batteryGraph(v,now,24);assert(g.size()==24&&g[0].soc==95&&g[0].charging&&g[1].soc==-1&&g[22].soc==-1&&g[23].soc==40&&!g[23].charging);
  assert(batteryGraph({},now,24).size()==24&&batteryGraph({},now,24)[5].soc==-1);
  s.t=now+50;v.push_back(s);assert(batteryGraph(v,now,24)[23].soc==40);// a future stamp is ignored
  assert(batteryGraph(v,now,0).empty());}
 // Average current over a window; no samples = unknown.
 {std::vector<BatterySample> v;BatterySample s;uint32_t now=100000;int ma=0;assert(!averageCurrent(v,now,3600,ma));
  s.t=now-100;s.ma=-30;v.push_back(s);s.t=now-200;s.ma=-10;v.insert(v.begin(),s);s.t=now-7200;s.ma=-500;v.insert(v.begin(),s);
  assert(averageCurrent(v,now,3600,ma)&&ma==-20);assert(averageCurrent(v,now,86400,ma)&&ma==-180);}
 // Time left: the gauge's own minutes (0xFFFF = unknown), else state of charge over the average current.
 assert(gaugeMinutes(0xFFFF)==-1&&gaugeMinutes(75)==75&&gaugeMinutes(0)==0);
 assert(timeToEmptyMinutes(50,-70)==600);// 700 mAh / 70 mA = 10 h
 assert(timeToEmptyMinutes(50,0)==-1&&timeToEmptyMinutes(50,20)==-1&&timeToEmptyMinutes(0,-70)==0);
 assert(timeToFullMinutes(50,350)==120);// 700 mAh / 350 mA
 assert(timeToFullMinutes(100,350)==0&&timeToFullMinutes(50,-5)==-1&&timeToFullMinutes(50,0)==-1);
 assert(durationText(-1)=="--"&&durationText(0)=="0 min"&&durationText(45)=="45 min"&&durationText(60)=="1 h"&&durationText(200)=="3 h 20 min"&&durationText(2*1440+4*60)=="2 d 4 h"&&durationText(1440)=="1 d");
 // On-time totals of the samples inside a window.
 {std::vector<BatterySample> v;BatterySample s;uint32_t now=100000;s.spanS=300;s.screenS=60;s.gpsS=300;s.modemS=0;s.radioS=100;s.sleepS=120;s.txDs=20;
  s.t=now-50;v.push_back(s);s.t=now-350;v.insert(v.begin(),s);s.t=now-90000;v.insert(v.begin(),s);
  auto o=sumOnTime(v,now,3600);assert(o.windowS==600&&o.screenS==120&&o.gpsS==600&&o.radioS==200&&o.sleepS==240&&o.txMs==4000);
  assert(sumOnTime(v,now,86400).windowS==600);}
 // Drain model: nominal mA table, per subsystem mAh; sleeping replaces the awake current.
 {OnTime o;o.windowS=3600;auto d=estimateDrain(o);assert(d.count==7&&std::fabs(d.totalMah-MA_AWAKE)<0.01f);// one awake hour, nothing else on
  o.sleepS=3600;d=estimateDrain(o);assert(std::fabs(d.totalMah-MA_SLEEP)<0.01f);
  o.sleepS=1800;o.screenS=600;o.gpsS=3600;o.modemS=1800;o.radioS=900;o.txMs=36000;d=estimateDrain(o);
  float want=MA_AWAKE*0.5f+MA_SLEEP*0.5f+MA_SCREEN/6+MA_GPS+MA_MODEM/2+MA_RADIO/4+MA_TX*0.01f;assert(std::fabs(d.totalMah-want)<0.05f);
  float sum=0;for(int k=0;k<d.count;k++)sum+=d.item[k].mah;assert(std::fabs(sum-d.totalMah)<0.01f);
  assert(std::fabs(averageModelMa(o)-want)<0.05f&&averageModelMa(OnTime())==0);
  assert(sharePercent(25,100)==25&&sharePercent(1,0)==0&&sharePercent(200,100)==100);
  assert(d.item[0].seconds==1800&&d.item[1].seconds==1800);}
 std::cout<<"PASS: battery math: sample records, 24 h history, graph columns, average current, time left, on-time totals, drain model\n";
}
