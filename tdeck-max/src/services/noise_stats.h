#pragma once
// Noise Floor app: pure logic (no Arduino): sample ring, stats, interference rule, chart scale, min/max downsampling, CSV.
#include <stdint.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
namespace noise {
constexpr int16_t Gap=INT16_MIN;// a tick without a sample (radio asleep, busy, off)
constexpr int RateCount=3,WindowCount=3,ThresholdMin=3,ThresholdMax=30,ThresholdDefault=10;
inline int stepIndex(int i,int dir){return std::max(0,std::min(2,i+dir));}
inline int rateSeconds(int i){static const int v[RateCount]={1,5,30};return v[std::max(0,std::min(RateCount-1,i))];}
inline int windowSeconds(int i){static const int v[WindowCount]={60,600,3600};return v[std::max(0,std::min(WindowCount-1,i))];}
inline int capacityFor(int rate,int window){return std::max(2,windowSeconds(window)/rateSeconds(rate));}
inline int clampThreshold(int v){return std::max(ThresholdMin,std::min(ThresholdMax,v));}
// 3..10 dB in 1 dB steps, then 5 dB steps up to 30.
inline int stepThreshold(int v,int dir){return clampThreshold(dir>0?(v<10?v+1:(v/5+1)*5):(v<=10?v-1:(v-1)/5*5));}
struct Sample {int16_t rssi=Gap;int8_t floor=0;};// tenths of dBm, calibrated floor in dBm (0 = unknown)
// Oldest first. Fixed capacity (window / rate), the newest sample overwrites the oldest.
class Ring {
 std::vector<Sample> data;size_t head=0,used=0;
public:
 void reset(size_t cap){data.assign(cap,Sample{});head=used=0;}
 void clear(){head=used=0;}
 size_t size()const{return used;}size_t capacity()const{return data.size();}
 void push(Sample s){if(data.empty())return;data[head]=s;head=(head+1)%data.size();if(used<data.size())used++;}
 const Sample &at(size_t i)const{return data[(head+data.size()-used+i)%data.size()];}// 0 = oldest
 // Same samples in a new capacity: the newest ones are kept.
 void resize(size_t cap){std::vector<Sample> keep;size_t n=std::min(used,cap);for(size_t i=used-n;i<used;i++)keep.push_back(at(i));data.assign(cap,Sample{});head=used=0;for(auto &s:keep)push(s);}
};
struct Stats {int valid=0,min=0,max=0,current=0;float avg=0;bool hasCurrent=false;};
inline Stats stats(const Ring &r){
 Stats s;long long sum=0;
 for(size_t i=0;i<r.size();i++){int v=r.at(i).rssi;if(v==Gap)continue;if(!s.valid){s.min=s.max=v;}s.min=std::min(s.min,v);s.max=std::max(s.max,v);sum+=v;s.valid++;}
 if(s.valid)s.avg=float(sum)/s.valid;
 if(r.size()&&r.at(r.size()-1).rssi!=Gap){s.hasCurrent=true;s.current=r.at(r.size()-1).rssi;}
 return s;
}
// Interference: the signal above the floor + threshold for two samples in a row (a gap or a quiet sample resets).
struct Detector {
 int consecutive=0,excess=0;
 bool update(int16_t tenths,int floorDbm,int thresholdDb){
  bool above=tenths!=Gap&&floorDbm<0&&tenths>floorDbm*10+thresholdDb*10;
  if(above){consecutive++;excess=int(std::lround((tenths-floorDbm*10)/10.0));}else{consecutive=0;excess=0;}
  return consecutive>=2;
 }
};
// Chart y range in dBm: 10 dB gridlines inside -130..-40, at least 30 dB tall, data and floor included.
struct Scale {int lo=-120,hi=-90,step=10;};
inline int floorTo10(int v){return int(std::floor(v/10.0))*10;}
inline int ceilTo10(int v){return int(std::ceil(v/10.0))*10;}
inline Scale yScale(bool hasData,int minTenths,int maxTenths,int floorDbm){
 constexpr int Lowest=-130,Highest=-40,MinSpan=30;
 Scale s;bool any=hasData,floorKnown=floorDbm<0;
 if(!any&&!floorKnown)return s;
 double lo=0,hi=0;
 if(any){lo=minTenths/10.0;hi=maxTenths/10.0;}
 if(floorKnown){lo=any?std::min(lo,double(floorDbm)):floorDbm;hi=any?std::max(hi,double(floorDbm)):floorDbm;}
 s.lo=std::max(Lowest,std::min(Highest,floorTo10(int(std::floor(lo)))));s.hi=std::max(Lowest,std::min(Highest,ceilTo10(int(std::ceil(hi)))));
 for(bool top=true;s.hi-s.lo<MinSpan;top=!top){
  if(top&&s.hi<Highest)s.hi+=10;else if(!top&&s.lo>Lowest)s.lo-=10;else if(s.hi<Highest)s.hi+=10;else s.lo-=10;
 }
 return s;
}
// Row for a value (tenths) in a plot `height` px tall: 0 = top (hi), height-1 = bottom (lo); out of range clamps.
inline int yPixel(const Scale &s,int tenths,int height){
 int v=std::max(s.lo*10,std::min(s.hi*10,tenths));return int((long long)(s.hi*10-v)*(height-1)/((s.hi-s.lo)*10));
}
inline std::string ageLabel(int secs){if(secs<=0)return "now";return secs<60?"-"+std::to_string(secs)+" s":secs<3600?"-"+std::to_string(secs/60)+" min":"-"+std::to_string(secs/3600)+" h";}
inline std::string dbmText(int tenths){return std::to_string(int(std::lround(tenths/10.0)));}
inline std::string tenthsText(int tenths){char t[16];int a=tenths<0?-tenths:tenths;snprintf(t,sizeof(t),"%s%d.%d",tenths<0?"-":"",a/10,a%10);return t;}
// One chart column: the lowest / highest sample in it, the first (oldest) and last (newest) one, to draw the line across columns.
struct Column {bool has=false;int16_t lo=0,hi=0,first=0,last=0;};
// Sample j counted from the newest sits at x = width-1 - j*(width-1)/(capacity-1): the window is always the whole chart width.
inline int sampleColumn(size_t fromNewest,size_t capacity,int width){return capacity<2?width-1:width-1-int(fromNewest*(width-1)/(capacity-1));}
inline std::vector<Column> downsample(const Ring &r,int width){
 std::vector<Column> cols(std::max(0,width));if(width<=0)return cols;
 size_t n=r.size(),cap=r.capacity();
 for(size_t i=0;i<n;i++){
  int v=r.at(i).rssi;if(v==Gap)continue;
  Column &c=cols[sampleColumn(n-1-i,cap,width)];
  if(!c.has){c.has=true;c.lo=c.hi=c.first=int16_t(v);}
  c.lo=std::min<int16_t>(c.lo,v);c.hi=std::max<int16_t>(c.hi,v);c.last=int16_t(v);
 }
 return cols;
}
// Columns between two consecutive samples; two columns further apart than this had a gap between them.
inline int linkStride(size_t capacity,int width){return capacity<2?1:std::max(1,int((width-1+capacity-2)/(capacity-1)));}
// x of an event `ageMs` old in a window of `windowSecs` (negative: older than the window).
inline int xForAge(uint32_t ageMs,int windowSecs,int width){long long span=(long long)windowSecs*1000;if(ageMs>span)return -1;return width-1-int((long long)ageMs*(width-1)/span);}
inline std::string csvHeader(){return "epoch,seconds_ago,rssi_dbm,floor_dbm,flag\n";}
inline std::string csvLine(uint32_t epoch,uint32_t secondsAgo,int tenths,int floorDbm,bool flag){
 char t[80];std::string e=epoch?std::to_string(epoch):"",f=floorDbm<0?std::to_string(floorDbm):"";
 snprintf(t,sizeof(t),"%s,%u,%s,%s,%d\n",e.c_str(),(unsigned)secondsAgo,tenthsText(tenths).c_str(),f.c_str(),flag?1:0);return t;
}
}
