#include <cassert>
#include <cmath>
#include <iostream>
#include "services/noise_stats.h"
using namespace noise;
int main(){
 // Options: rate x window, capacity = window / rate (at least 2), index clamping, step rules.
 assert(capacityFor(0,0)==60&&capacityFor(0,2)==3600&&capacityFor(1,1)==120&&capacityFor(2,0)==2&&capacityFor(2,1)==20);
 assert(rateSeconds(0)==1&&rateSeconds(1)==5&&rateSeconds(2)==30&&rateSeconds(9)==30&&rateSeconds(-4)==1);
 assert(windowSeconds(0)==60&&windowSeconds(1)==600&&windowSeconds(2)==3600);
 assert(stepIndex(0,-1)==0&&stepIndex(0,1)==1&&stepIndex(2,1)==2);
 assert(stepThreshold(10,1)==15&&stepThreshold(10,-1)==9&&stepThreshold(3,-1)==3&&stepThreshold(30,1)==30&&stepThreshold(9,1)==10&&stepThreshold(15,-1)==10);
 assert(clampThreshold(1)==3&&clampThreshold(99)==30&&clampThreshold(12)==12);
 // Ring: newest last, wraps, resize keeps the newest.
 Ring r;r.reset(4);assert(r.size()==0);
 for(int i=1;i<=6;i++)r.push({int16_t(-1000-i),int8_t(-100)});
 assert(r.size()==4&&r.at(0).rssi==-1003&&r.at(3).rssi==-1006);
 r.resize(2);assert(r.size()==2&&r.at(0).rssi==-1005&&r.at(1).rssi==-1006);
 r.resize(5);assert(r.size()==2&&r.capacity()==5);r.push({-900,-100});assert(r.size()==3&&r.at(2).rssi==-900);
 r.clear();assert(r.size()==0&&r.capacity()==5);
 // Stats: gaps ignored; current = newest valid sample only when the newest slot is valid.
 Ring s;s.reset(10);s.push({-1200,-120});s.push({Gap,-120});s.push({-1000,-120});s.push({-800,-120});
 auto st=stats(s);assert(st.valid==3&&st.min==-1200&&st.max==-800&&std::fabs(st.avg-(-1000))<0.01f&&st.hasCurrent&&st.current==-800);
 s.push({Gap,-120});st=stats(s);assert(!st.hasCurrent&&st.valid==3);
 Ring none;none.reset(5);none.push({Gap,0});st=stats(none);assert(st.valid==0&&!st.hasCurrent);
 // Interference: floor + threshold exceeded twice in a row; a gap or a low sample resets; excess in dB.
 Detector d;
 assert(!d.update(-900,-110,10));assert(d.update(-900,-110,10)&&d.excess==20);
 assert(d.update(-1050,-110,10)==false&&d.excess==0);
 d=Detector();d.update(-900,-110,10);d.update(Gap,-110,10);assert(!d.update(-900,-110,10));
 d=Detector();assert(!d.update(-1000,-110,10)&&!d.update(-1000,-110,10));// exactly floor+10 is not above it
 d=Detector();d.update(-999,-110,10);assert(d.update(-999,-110,10));// one tenth above
 d=Detector();d.update(-500,0,10);assert(!d.update(-500,0,10));// no calibrated floor yet: no warning
 // Y scale: -130..-40, 10 dB gridlines, at least 20 dB tall, floor included.
 auto y=yScale(false,0,0,-110);assert(y.lo==-120&&y.hi==-90&&y.step==10);y=yScale(false,0,0,0);assert(y.lo==-120&&y.hi==-90);
 y=yScale(true,-1043,-987,-112);assert(y.lo==-120&&y.hi==-90);
 y=yScale(true,-1043,-987,0);assert(y.lo==-110&&y.hi==-80);
 y=yScale(true,-1500,-300,-110);assert(y.lo==-130&&y.hi==-40);
 y=yScale(true,-700,-700,0);assert(y.hi-y.lo>=20&&y.lo<=-70&&y.hi>=-70&&y.lo%10==0&&y.hi%10==0);
 y=yScale(true,-410,-405,-405);assert(y.hi<=-40&&y.hi-y.lo>=20&&y.lo>=-130);
 assert(yPixel(y,y.hi*10,100)==0&&yPixel(y,y.lo*10,100)==99&&yPixel(y,y.hi*10+500,100)==0&&yPixel(y,y.lo*10-500,100)==99&&yPixel(y,(y.hi+y.lo)*5,101)==50);
 // Labels.
 assert(ageLabel(60)=="-1 min"&&ageLabel(30)=="-30 s"&&ageLabel(600)=="-10 min"&&ageLabel(1800)=="-30 min"&&ageLabel(3600)=="-1 h"&&ageLabel(0)=="now");
 assert(dbmText(-1043)=="-104"&&dbmText(-1048)=="-105"&&dbmText(-1000)=="-100"&&dbmText(-5)=="-1");
 assert(tenthsText(-1043)=="-104.3"&&tenthsText(-1000)=="-100.0"&&tenthsText(-5)=="-0.5");
 // Downsample: more samples than columns keeps min/max per column (a one-sample spike survives), newest at the right edge.
 Ring big;big.reset(100);for(int i=0;i<100;i++)big.push({int16_t(i==50?-400:-1100),-110});
 auto cols=downsample(big,10);assert(cols.size()==10);
 int spikes=0;for(auto &c:cols)if(c.has&&c.hi==-400)spikes++;assert(spikes==1);
 for(auto &c:cols)assert(c.has&&c.lo==-1100);
 assert(cols[9].last==-1100&&cols[0].first==-1100);
 // Fewer samples than columns: right aligned, empty columns between samples, link flag follows the sample stride.
 Ring few;few.reset(5);for(int i=0;i<3;i++)few.push({int16_t(-1000-i*10),-110});
 cols=downsample(few,9);assert(cols.size()==9);assert(cols[8].has&&cols[8].last==-1020&&!cols[0].has);
 int have=0;for(auto &c:cols)if(c.has)have++;assert(have==3);
 // A window that fills only partly: the empty (oldest) side stays empty; capacity 5 over 9 columns => stride 2.
 assert(downsample(few,9).size()==9);
 Ring gapped;gapped.reset(5);gapped.push({-1000,-110});gapped.push({Gap,-110});gapped.push({-1000,-110});
 cols=downsample(gapped,9);int have2=0;for(auto &c:cols)if(c.has)have2++;assert(have2==2);
 assert(linkStride(5,9)==2&&linkStride(100,10)==1&&linkStride(60,190)==4&&linkStride(1,50)==1);
 // x position of an age inside the window.
 assert(xForAge(0,60,190)==189&&xForAge(60000,60,190)==0&&xForAge(30000,60,190)==95&&xForAge(61000,60,190)<0);
 // CSV
 assert(csvHeader()=="epoch,seconds_ago,rssi_dbm,floor_dbm,flag\n");
 assert(csvLine(1700000000,5,-1043,-110,true)=="1700000000,5,-104.3,-110,1\n");
 assert(csvLine(0,0,-900,0,false)==",0,-90.0,,0\n");
 std::cout<<"PASS: noise stats (ring, min/avg/max ignoring gaps, interference rule with 2-sample debounce, y scale and gridlines, min/max downsampling, csv)\n";
}
