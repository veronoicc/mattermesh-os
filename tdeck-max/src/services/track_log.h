#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "services/geo.h"
// GPS breadcrumb tracks: sampling policy, statistics, the compact crash-safe SD record format, display thinning and the
// GPX 1.1 writer. Pure code, host tested (sim/test_track_log.cpp); services/tracks.cpp is the SD / GPS glue.
namespace mm_track {
constexpr size_t RowBytes=20;
constexpr uint32_t MaxFixAgeMs=3000;// a fix older than this is not a position any more
constexpr int MaxHdop100=500;// GGA HDOP in hundredths: worse than 5.0 is not recorded
constexpr double MaxJumpMps=120;// faster than this between two kept points is a glitch ...
constexpr int MaxJumpRun=3;// ... unless it persists: the receiver really moved, re-anchor after this many in a row
constexpr int ClimbStepM=3;// altitude noise below this never counts as climb
// lat / lon in millionths of a degree (11 cm), altitude in whole metres, hdop in tenths, t UTC seconds (0: clock not set).
struct Point {int32_t lat=0,lon=0;int16_t alt=0;uint32_t t=0;uint8_t hdop=0;};
inline double deg(int32_t v){return v*1e-6;}
inline double distance(const Point &a,const Point &b){return mm::geoDistanceMeters(deg(a.lat),deg(a.lon),deg(b.lat),deg(b.lon));}

// ---- settings (Settings > GPS tracks)
struct Settings {bool autoStart=false,showOnMap=true,keepGps=true;uint16_t intervalS=10,minDistM=10;};
constexpr int Choices=4;constexpr uint16_t Intervals[Choices]={5,10,30,60},Distances[Choices]={5,10,25,50};
// The next (dir 1) or previous (-1) value of a < value > row, wrapping; dir 0: the nearest listed value (for a stored value off the list).
inline uint16_t stepChoice(const uint16_t *list,uint16_t value,int dir){int at=0;for(int i=1;i<Choices;i++)if(std::abs(int(list[i])-int(value))<std::abs(int(list[at])-int(value)))at=i;return list[(at+(dir<0?Choices-1:dir>0?1:0))%Choices];}

// ---- sampling: a point is kept when the interval AND the distance since the last kept point are met
struct Fix {bool fix=false;double lat=0,lon=0,alt=0;uint32_t ageMs=0;int hdop100=-1;uint32_t nowMs=0,epoch=0;};
enum class Reject {None,NoFix,Stale,Hdop,Range,Interval,Distance,Jump};
struct Sampler {
 Settings cfg;bool has=false;Point last;uint32_t lastMs=0;int jumps=0;
 void reset(){has=false;jumps=0;}
 Reject offer(const Fix &f,Point &out){
  if(!f.fix)return Reject::NoFix;if(f.ageMs>MaxFixAgeMs)return Reject::Stale;if(f.hdop100>MaxHdop100)return Reject::Hdop;// unknown HDOP (-1) is accepted
  if(!std::isfinite(f.lat)||!std::isfinite(f.lon)||!std::isfinite(f.alt)||std::fabs(f.lat)>90||std::fabs(f.lon)>180||(f.lat==0&&f.lon==0))return Reject::Range;
  Point p;p.lat=int32_t(std::lround(f.lat*1e6));p.lon=int32_t(std::lround(f.lon*1e6));p.alt=int16_t(std::max(-32768.0,std::min(32767.0,std::round(f.alt))));p.t=f.epoch;p.hdop=uint8_t(std::max(0,std::min(255,f.hdop100/10)));
  if(has){uint32_t dt=f.nowMs-lastMs;if(dt<uint32_t(cfg.intervalS)*1000u)return Reject::Interval;double d=distance(last,p);if(d<cfg.minDistM)return Reject::Distance;
   if(dt>0&&d/(dt/1000.0)>MaxJumpMps&&jumps<MaxJumpRun){jumps++;return Reject::Jump;}}
  out=p;last=p;lastMs=f.nowMs;has=true;jumps=0;return Reject::None;}
};

// ---- statistics
struct Summary {uint32_t points=0,distanceM=0,durationS=0;uint16_t gainM=0,maxSpeedDms=0;};
struct Stats {
 uint32_t points=0,startT=0,endT=0,gainM=0;double distanceM=0,maxSpeedMps=0;Point last;int refAlt=0;
 uint32_t durationS()const{return startT&&endT>=startT?endT-startT:0;}
 void add(const Point &p){
  if(p.t){if(!startT)startT=p.t;endT=p.t;}
  if(points){double d=distance(last,p);distanceM+=d;if(last.t&&p.t>last.t)maxSpeedMps=std::max(maxSpeedMps,d/(p.t-last.t));// no clock or the same second: no speed
   if(p.alt-refAlt>=ClimbStepM){gainM+=p.alt-refAlt;refAlt=p.alt;}else if(refAlt-p.alt>=ClimbStepM)refAlt=p.alt;}// hysteresis: the climb from the last low, not every wobble
  else refAlt=p.alt;
  last=p;points++;}
 Summary summary()const{Summary s;s.points=points;s.distanceM=uint32_t(distanceM+0.5);s.durationS=durationS();s.gainM=uint16_t(std::min<uint32_t>(gainM,65535));s.maxSpeedDms=uint16_t(std::min(65535.0,maxSpeedMps*10+0.5));return s;}
};

// ---- records: fixed RowBytes rows, kind byte first, CRC-8 (init 0xFF, poly 0x07) last, little endian.
// 'H' header (first row), 'P' point, 'S' summary (written when a track is closed). A file is only ever appended to a row
// boundary, so a power cut leaves at most one cut row at the end (never read) or one row with a wrong checksum.
struct Header {uint32_t startT=0;uint16_t intervalS=0,minDistM=0;};
struct Row {char kind=0;Point p;Summary s;Header h;};
inline uint8_t crc8(const uint8_t *d,size_t n){uint8_t c=0xFF;for(size_t i=0;i<n;i++){c^=d[i];for(int k=0;k<8;k++)c=c&0x80?uint8_t(c<<1)^0x07:uint8_t(c<<1);}return c;}
inline void put(uint8_t *o,uint32_t v,int bytes){for(int i=0;i<bytes;i++)o[i]=uint8_t(v>>(8*i));}
inline uint32_t get(const uint8_t *in,int bytes){uint32_t v=0;for(int i=0;i<bytes;i++)v|=uint32_t(in[i])<<(8*i);return v;}
inline void seal(uint8_t *row){row[RowBytes-1]=crc8(row,RowBytes-1);}
inline void encodeHeader(const Header &h,uint8_t *row){memset(row,0,RowBytes);row[0]='H';row[1]=1;put(row+2,h.startT,4);put(row+6,h.intervalS,2);put(row+8,h.minDistM,2);seal(row);}
inline void encodePoint(const Point &p,uint8_t *row){memset(row,0,RowBytes);row[0]='P';put(row+1,p.t,4);put(row+5,uint32_t(p.lat),4);put(row+9,uint32_t(p.lon),4);put(row+13,uint16_t(p.alt),2);row[15]=p.hdop;seal(row);}
inline void encodeSummary(const Summary &s,uint8_t *row){memset(row,0,RowBytes);row[0]='S';put(row+1,s.points,4);put(row+5,s.distanceM,4);put(row+9,s.durationS,4);put(row+13,s.gainM,2);put(row+15,s.maxSpeedDms,2);seal(row);}
inline bool decodeRow(const uint8_t *in,Row &r){
 if(in[RowBytes-1]!=crc8(in,RowBytes-1))return false;r=Row();r.kind=char(in[0]);
 switch(in[0]){
  case 'H':if(in[1]!=1)return false;r.h.startT=get(in+2,4);r.h.intervalS=uint16_t(get(in+6,2));r.h.minDistM=uint16_t(get(in+8,2));return true;
  case 'P':r.p.t=get(in+1,4);r.p.lat=int32_t(get(in+5,4));r.p.lon=int32_t(get(in+9,4));r.p.alt=int16_t(get(in+13,2));r.p.hdop=in[15];return true;
  case 'S':r.s.points=get(in+1,4);r.s.distanceM=get(in+5,4);r.s.durationS=get(in+9,4);r.s.gainM=uint16_t(get(in+13,2));r.s.maxSpeedDms=uint16_t(get(in+15,2));return true;}
 return false;}
// Feeds any chunking of a file; a row that fails its checksum is skipped (bad), bytes after the last whole row are torn().
struct Reader {
 uint8_t buf[RowBytes];size_t fill=0;uint32_t good=0,bad=0;
 template<class F> void feed(const uint8_t *d,size_t n,F &&on){while(n){size_t take=std::min(n,RowBytes-fill);memcpy(buf+fill,d,take);fill+=take;d+=take;n-=take;if(fill==RowBytes){fill=0;Row r;if(decodeRow(buf,r)){good++;on(r);}else bad++;}}}
 size_t torn()const{return fill;}
};
// A file that ends inside a row (tail = those bytes): the bytes that complete it into one dead row (wrong checksum for sure),
// so the next row starts on a boundary again. Returns how many bytes were written to out (RowBytes at most).
inline size_t tornPad(const uint8_t *tail,size_t tailLen,uint8_t *out){
 if(tailLen==0||tailLen>=RowBytes)return 0;uint8_t row[RowBytes]={};memcpy(row,tail,tailLen);row[RowBytes-1]=crc8(row,RowBytes-1)^0xFF;memcpy(out,row+tailLen,RowBytes-tailLen);return RowBytes-tailLen;}
// What a track file says: its summary row when it was closed, else the statistics of its points.
struct Scan {
 Header head;bool hasHead=false,closed=false;uint32_t start=0;Stats stats;Summary closing;
 void row(const Row &r){if(r.kind=='H'&&!hasHead){head=r.h;hasHead=true;start=r.h.startT;}else if(r.kind=='P'&&!closed)stats.add(r.p);else if(r.kind=='S'){closing=r.s;closed=true;}}
 Summary summary()const{return closed?closing:stats.summary();}
};

// ---- display thinning: at most cap points kept for a polyline, whatever the track length (streamed, no full load).
struct Thin {
 size_t cap;std::vector<Point> pts;uint32_t stride=1,seen=0;Point last;bool any=false,lastKept=false;
 explicit Thin(size_t capacity=256):cap(capacity<4?4:capacity&~size_t(1)){pts.reserve(cap);}
 void add(const Point &p){
  last=p;any=true;uint32_t index=seen++;lastKept=false;if(index%stride)return;
  if(pts.size()==cap){for(size_t i=0;i<cap/2;i++)pts[i]=pts[2*i];pts.resize(cap/2);stride*=2;if(index%stride)return;}
  pts.push_back(p);lastKept=true;}
 std::vector<Point> points()const{std::vector<Point> out=pts;if(any&&!lastKept)out.push_back(last);return out;}// the newest point always ends the line
};

// ---- route sketch (track detail): the points scaled into a w x h box with pad inside, north up, true to the ground (longitude * cos(latitude)).
struct ShapePoint {int x,y;};
inline std::vector<ShapePoint> shapeFit(const std::vector<Point> &pts,int w,int h,int pad){
 std::vector<ShapePoint> out;if(pts.empty())return out;
 double lat0=0;for(auto &p:pts)lat0+=deg(p.lat);lat0/=double(pts.size());const double kx=std::cos(lat0*M_PI/180);
 double minX=1e18,maxX=-1e18,minY=1e18,maxY=-1e18;for(auto &p:pts){double X=deg(p.lon)*kx,Y=-deg(p.lat);minX=std::min(minX,X);maxX=std::max(maxX,X);minY=std::min(minY,Y);maxY=std::max(maxY,Y);}
 const double spanX=maxX-minX,spanY=maxY-minY,boxW=w-2*pad,boxH=h-2*pad,eps=1e-8;double scale=0;// both spans nil: every point in the middle
 if(spanX>eps&&spanY>eps)scale=std::min(boxW/spanX,boxH/spanY);else if(spanX>eps)scale=boxW/spanX;else if(spanY>eps)scale=boxH/spanY;
 const double offX=pad+(boxW-spanX*scale)/2,offY=pad+(boxH-spanY*scale)/2;
 for(auto &p:pts)out.push_back({int(std::lround(offX+(deg(p.lon)*kx-minX)*scale)),int(std::lround(offY+(-deg(p.lat)-minY)*scale))});
 return out;}

// ---- GPX 1.1
inline std::string xmlEscape(const std::string &s){std::string o;for(unsigned char c:s){if(c<0x20&&c!='\t'&&c!='\n'&&c!='\r')continue;// not allowed in XML 1.0
  switch(c){case '&':o+="&amp;";break;case '<':o+="&lt;";break;case '>':o+="&gt;";break;case '"':o+="&quot;";break;case '\'':o+="&apos;";break;default:o+=char(c);}}return o;}
struct Civil {int year,month,day,hour,minute,second;};
inline Civil civil(uint32_t t){int64_t days=t/86400,z=days+719468,era=(z>=0?z:z-146096)/146097,doe=z-era*146097,yoe=(doe-doe/1460+doe/36524-doe/146096)/365,doy=doe-(365*yoe+yoe/4-yoe/100),mp=(5*doy+2)/153;
 int month=int(mp<10?mp+3:mp-9);Civil c;c.year=int(yoe+era*400+(month<=2));c.month=month;c.day=int(doy-(153*mp+2)/5+1);uint32_t s=t%86400;c.hour=int(s/3600);c.minute=int(s%3600/60);c.second=int(s%60);return c;}
inline std::string isoTime(uint32_t t){auto c=civil(t);char b[24];snprintf(b,sizeof b,"%04d-%02d-%02dT%02d:%02d:%02dZ",c.year,c.month,c.day,c.hour,c.minute,c.second);return b;}
inline std::string fmtDeg(int32_t v){int64_t a=v<0?-int64_t(v):int64_t(v);char b[24];snprintf(b,sizeof b,"%s%lld.%06lld",v<0?"-":"",(long long)(a/1000000),(long long)(a%1000000));return b;}
// "2026-11-11 15:30 UTC": the track's name in the list and in the GPX file.
inline std::string trackName(uint32_t startT){if(!startT)return "Track without clock";auto c=civil(startT);char b[32];snprintf(b,sizeof b,"%04d-%02d-%02d %02d:%02d UTC",c.year,c.month,c.day,c.hour,c.minute);return b;}
// File name stem, sorting oldest to newest: "trk-20261111-153007", or "trk-n<boot ms>" when the clock was not set.
inline std::string trackStem(uint32_t startT,uint32_t fallback){char b[32];if(!startT){snprintf(b,sizeof b,"trk-n%lu",(unsigned long)fallback);return b;}auto c=civil(startT);snprintf(b,sizeof b,"trk-%04d%02d%02d-%02d%02d%02d",c.year,c.month,c.day,c.hour,c.minute,c.second);return b;}
inline std::string gpxHead(const std::string &name,uint32_t startT){
 std::string n=xmlEscape(name);
 return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<gpx version=\"1.1\" creator=\"MatterMesh OS\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n<metadata><name>"+n+"</name>"+(startT?"<time>"+isoTime(startT)+"</time>":"")+"</metadata>\n<trk><name>"+n+"</name>\n<trkseg>\n";}
inline std::string gpxPoint(const Point &p){return "<trkpt lat=\""+fmtDeg(p.lat)+"\" lon=\""+fmtDeg(p.lon)+"\"><ele>"+std::to_string(p.alt)+"</ele>"+(p.t?"<time>"+isoTime(p.t)+"</time>":"")+"</trkpt>\n";}
inline std::string gpxTail(){return "</trkseg>\n</trk>\n</gpx>\n";}

// ---- labels
inline std::string distanceLabel(double m){char b[24];if(m<1000)snprintf(b,sizeof b,"%d m",int(m+0.5));else if(m<10000)snprintf(b,sizeof b,"%.1f km",m/1000);else snprintf(b,sizeof b,"%d km",int(m/1000+0.5));return b;}
inline std::string durationLabel(uint32_t s){char b[24];if(s<60)snprintf(b,sizeof b,"%u s",unsigned(s));else if(s<3600)snprintf(b,sizeof b,"%u min",unsigned(s/60));else snprintf(b,sizeof b,"%u h %02u min",unsigned(s/3600),unsigned(s%3600/60));return b;}
inline std::string speedLabel(double mps){char b[24];snprintf(b,sizeof b,"%.1f km/h",mps*3.6);return b;}
}
