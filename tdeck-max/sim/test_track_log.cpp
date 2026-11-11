#include <cassert>
#include <cmath>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include "services/track_log.h"
using namespace mm_track;
// Minimal XML well-formedness check: balanced tags, quoted attributes, no raw '&' / '<' in text, one root.
static bool wellFormed(const std::string &x){
 std::vector<std::string> stack;size_t i=0;int roots=0;
 if(x.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?>",0)!=0)return false;
 i=x.find("?>")+2;
 while(i<x.size()){
  if(x[i]=='<'){size_t e=x.find('>',i);if(e==std::string::npos)return false;std::string tag=x.substr(i+1,e-i-1);
   if(tag.empty())return false;
   if(tag[0]=='/'){if(stack.empty()||stack.back()!=tag.substr(1))return false;stack.pop_back();}
   else{bool self=tag.back()=='/';if(self)tag.pop_back();size_t sp=tag.find(' ');std::string name=tag.substr(0,sp);
    if(sp!=std::string::npos){std::string attrs=tag.substr(sp);size_t q=0;int quotes=0;while((q=attrs.find('"',q))!=std::string::npos){quotes++;q++;}if(quotes%2)return false;
     // every '=' must be followed by a quote; a quote may only open after '='
     for(size_t k=0;k<attrs.size();k++)if(attrs[k]=='='&&(k+1>=attrs.size()||attrs[k+1]!='"'))return false;}
    if(stack.empty()){if(++roots>1)return false;}
    if(!self)stack.push_back(name);}
   i=e+1;}
  else{if(x[i]=='&'){size_t semi=x.find(';',i);if(semi==std::string::npos||semi-i>6)return false;std::string ent=x.substr(i,semi-i+1);if(ent!="&amp;"&&ent!="&lt;"&&ent!="&gt;"&&ent!="&quot;"&&ent!="&apos;")return false;}
   if(stack.empty()&&x[i]!='\n')return false;i++;}
 }
 return stack.empty()&&roots==1;
}
static Fix fix(double lat,double lon,uint32_t nowMs,double alt=100){Fix f;f.fix=true;f.lat=lat;f.lon=lon;f.alt=alt;f.ageMs=500;f.hdop100=120;f.nowMs=nowMs;f.epoch=1000+nowMs/1000;return f;}
int main(){
 // ---- sampling policy: both the interval and the distance must be met; bad fixes never recorded
 {Sampler s;s.cfg.intervalS=10;s.cfg.minDistM=25;Point p;
  assert(s.offer(fix(46,7,0),p)==Reject::None&&p.lat==46000000&&p.lon==7000000&&p.alt==100&&p.hdop==12);// the first fix is kept
  assert(s.offer(fix(46.001,7,5000),p)==Reject::Interval);// 111 m but only 5 s
  assert(stepChoice(Distances,7,0)==5&&stepChoice(Distances,40,0)==50&&stepChoice(Intervals,30,0)==30);
  assert(s.offer(fix(46.00001,7,12000),p)==Reject::Distance);// 12 s but 1 m
  assert(s.offer(fix(46.001,7,12000),p)==Reject::None);// both met: kept, clocks restart from this point
  assert(s.offer(fix(46.002,7,15000),p)==Reject::Interval);
  Fix f=fix(46.003,7,30000);f.fix=false;assert(s.offer(f,p)==Reject::NoFix);
  f=fix(46.003,7,30000);f.ageMs=MaxFixAgeMs+1;assert(s.offer(f,p)==Reject::Stale);
  f=fix(46.003,7,30000);f.hdop100=MaxHdop100+1;assert(s.offer(f,p)==Reject::Hdop);
  f=fix(46.003,7,30000);f.hdop100=-1;assert(s.offer(f,p)==Reject::None&&p.hdop==0);// unknown HDOP is not a reason to drop the point
  f=fix(0,0,60000);assert(s.offer(f,p)==Reject::Range);f=fix(91,7,60000);assert(s.offer(f,p)==Reject::Range);f=fix(NAN,7,60000);assert(s.offer(f,p)==Reject::Range);
  f=fix(46.003,181,60000);assert(s.offer(f,p)==Reject::Range);
  f=fix(48,7,60000);assert(s.offer(f,p)==Reject::Jump);// 220 km in 30 s: a glitch
  assert(s.offer(fix(48,7,70000),p)==Reject::Jump&&s.offer(fix(48,7,80000),p)==Reject::Jump);
  assert(s.offer(fix(48,7,90000),p)==Reject::None);// the receiver really is there: re-anchor after three in a row
  Sampler r;r.cfg.intervalS=60;r.cfg.minDistM=50;assert(r.offer(fix(1,1,0,-12.4),p)==Reject::None&&p.alt==-12);// negative altitude rounds, stays signed
  assert(r.offer(fix(1.01,1,59999),p)==Reject::Interval&&r.offer(fix(1.01,1,60000),p)==Reject::None);
  Fix hi=fix(1,1,0,99999);Sampler q;assert(q.offer(hi,p)==Reject::None&&p.alt==32767);
 }
 // ---- settings choices
 {static_assert(Choices==4,"four steps");assert(stepChoice(Intervals,5,1)==10&&stepChoice(Intervals,60,1)==5&&stepChoice(Intervals,5,-1)==60&&stepChoice(Intervals,30,-1)==10);
  assert(stepChoice(Distances,5,1)==10&&stepChoice(Distances,50,1)==5&&stepChoice(Distances,7,1)==10/* unknown: from the nearest */&&stepChoice(Distances,25,-1)==10);}
 // ---- stats: distance, duration, elevation gain (3 m hysteresis), max speed
 {Stats st;Point p;p.lat=46000000;p.lon=7000000;p.alt=100;p.t=1000;st.add(p);
  assert(st.points==1&&st.distanceM==0&&st.durationS()==0&&st.gainM==0&&st.maxSpeedMps==0);
  p.lat=46001000;p.alt=101;p.t=1010;st.add(p);// 111 m / 10 s = 11 m/s, +1 m is GPS noise
  assert(std::fabs(st.distanceM-111.2)<0.5&&st.gainM==0&&std::fabs(st.maxSpeedMps-11.12)<0.1);
  p.lat=46002000;p.alt=110;p.t=1030;st.add(p);// +9 m from the reference, 111 m / 20 s
  assert(st.gainM==10&&std::fabs(st.maxSpeedMps-11.12)<0.1);// climb counted from the reference low (100 m): 110-100
  p.lat=46003000;p.alt=104;p.t=1040;st.add(p);p.lat=46004000;p.alt=110;p.t=1050;st.add(p);// down 6 m, up again 6 m
  assert(st.gainM==16);
  p.lat=46005000;p.alt=111;p.t=1060;st.add(p);assert(st.gainM==16);
  assert(st.points==6&&st.durationS()==60&&std::fabs(st.distanceM-5*111.2)<2);
  auto sum=st.summary();assert(sum.points==6&&sum.durationS==60&&sum.gainM==16&&sum.distanceM==uint32_t(st.distanceM+0.5)&&sum.maxSpeedDms==uint16_t(st.maxSpeedMps*10+0.5));
  Stats nt;Point a,b;a.lat=1000000;b.lat=1001000;nt.add(a);nt.add(b);assert(nt.distanceM>100&&nt.durationS()==0&&nt.maxSpeedMps==0);// no clock yet: no speed, no duration
  Stats back;Point u;u.lat=1000000;u.t=500;back.add(u);u.lat=1001000;u.t=500;back.add(u);assert(back.maxSpeedMps==0);// same second: no divide by zero
 }
 // ---- rows: round trip, checksums, torn tail
 {Point p;p.lat=-33868820;p.lon=151209290;p.alt=-12;p.t=1794000000;p.hdop=17;uint8_t row[RowBytes];encodePoint(p,row);Row r;
  assert(decodeRow(row,r)&&r.kind=='P'&&r.p.lat==p.lat&&r.p.lon==p.lon&&r.p.alt==p.alt&&r.p.t==p.t&&r.p.hdop==17);
  for(size_t i=0;i<RowBytes;i++){uint8_t bad[RowBytes];memcpy(bad,row,RowBytes);bad[i]^=0x10;assert(!decodeRow(bad,r));}// any flipped byte is caught
  uint8_t zero[RowBytes]={};assert(!decodeRow(zero,r));uint8_t ff[RowBytes];memset(ff,0xFF,RowBytes);assert(!decodeRow(ff,r));
  Header h;h.startT=1794000000;h.intervalS=30;h.minDistM=25;uint8_t hr[RowBytes];encodeHeader(h,hr);assert(decodeRow(hr,r)&&r.kind=='H'&&r.h.startT==h.startT&&r.h.intervalS==30&&r.h.minDistM==25);
  Summary s;s.points=7;s.distanceM=1234;s.durationS=999;s.gainM=42;s.maxSpeedDms=117;uint8_t sr[RowBytes];encodeSummary(s,sr);assert(decodeRow(sr,r)&&r.kind=='S'&&r.s.points==7&&r.s.distanceM==1234&&r.s.durationS==999&&r.s.gainM==42&&r.s.maxSpeedDms==117);
  // A power cut mid-row: the file ends inside a row. Reader skips it; the pad turns it into one dead row; appends stay aligned.
  std::vector<uint8_t> file;file.insert(file.end(),hr,hr+RowBytes);for(int i=0;i<3;i++){p.t+=10;encodePoint(p,row);file.insert(file.end(),row,row+RowBytes);}
  size_t whole=file.size();file.insert(file.end(),row,row+13);// the 4th point cut after 13 bytes
  {Reader rd;int pts=0;rd.feed(file.data(),file.size(),[&](const Row &x){if(x.kind=='P')pts++;});assert(pts==3&&rd.good==4&&rd.bad==0&&rd.torn()==13);}
  uint8_t pad[RowBytes];size_t padLen=tornPad(file.data()+whole,file.size()-whole,pad);assert(padLen==RowBytes-13);
  file.insert(file.end(),pad,pad+padLen);assert(file.size()%RowBytes==0);encodeSummary(s,sr);file.insert(file.end(),sr,sr+RowBytes);
  {Reader rd;int pts=0,sums=0;for(size_t off=0;off<file.size();off+=7)rd.feed(file.data()+off,std::min<size_t>(7,file.size()-off),[&](const Row &x){if(x.kind=='P')pts++;if(x.kind=='S')sums++;});// odd chunking
   assert(pts==3&&sums==1&&rd.bad==1&&rd.torn()==0);}
  assert(tornPad(file.data(),0,pad)==0);
  // The padded dead row can never pass the checksum, whatever the cut bytes were.
  for(size_t cut=1;cut<RowBytes;cut++){uint8_t full[RowBytes];encodePoint(p,full);uint8_t dead[RowBytes];memcpy(dead,full,cut);size_t n=tornPad(full,cut,dead+cut);assert(n==RowBytes-cut);assert(!decodeRow(dead,r));}
  // Garbage rows in the middle (sector lost) are skipped, the rows after still read.
  std::vector<uint8_t> hole;hole.insert(hole.end(),hr,hr+RowBytes);hole.insert(hole.end(),RowBytes,0);encodePoint(p,row);hole.insert(hole.end(),row,row+RowBytes);
  {Reader rd;int pts=0;rd.feed(hole.data(),hole.size(),[&](const Row &x){if(x.kind=='P')pts++;});assert(pts==1&&rd.bad==1&&rd.good==2);}
 }
 // ---- scan: summary of an open track from its points, closed track from its summary row
 {Scan sc;Header h;h.startT=2000;uint8_t row[RowBytes];Reader rd;auto on=[&](const Row &x){sc.row(x);};encodeHeader(h,row);rd.feed(row,RowBytes,on);
  Point p;p.lat=46000000;p.lon=7000000;p.t=2000;for(int i=0;i<5;i++){p.lat+=1000;p.t+=10;encodePoint(p,row);rd.feed(row,RowBytes,on);}
  assert(!sc.closed&&sc.start==2000&&sc.summary().points==5&&sc.summary().durationS==40&&sc.summary().distanceM>=440&&sc.summary().distanceM<=450);
  Summary s;s.points=99;s.distanceM=5000;s.durationS=3600;encodeSummary(s,row);rd.feed(row,RowBytes,on);assert(sc.closed&&sc.summary().points==99&&sc.summary().distanceM==5000);}
 // ---- decimation for the display: bounded memory, ends kept, order kept
 {Thin t(64);Point p;for(int i=0;i<10000;i++){p.lat=46000000+i*10;p.lon=7000000;p.t=1000+i;t.add(p);}
  auto v=t.points();assert(v.size()<=65&&v.size()>=32&&v.front().t==1000&&v.back().t==1000+9999);for(size_t i=1;i<v.size();i++)assert(v[i].t>v[i-1].t);
  Thin small(64);for(int i=0;i<10;i++){p.t=i+1;small.add(p);}assert(small.points().size()==10);
  Thin none(64);assert(none.points().empty());Thin one(64);p.t=5;one.add(p);assert(one.points().size()==1);
  Thin cap(64);for(int i=0;i<64;i++){p.t=i+1;cap.add(p);}assert(cap.points().size()==64);p.t=65;cap.add(p);assert(cap.points().back().t==65&&cap.points().size()<=65);}
 // ---- GPX 1.1
 {assert(xmlEscape("A&B <c> \"d\" 'e'")=="A&amp;B &lt;c&gt; &quot;d&quot; &apos;e&apos;");assert(xmlEscape(std::string("a\x01" "b\x7f"))=="ab\x7f");
  struct tm tm={};tm.tm_year=2026-1900;tm.tm_mon=10;tm.tm_mday=11;tm.tm_hour=15;tm.tm_min=30;tm.tm_sec=7;time_t t=timegm(&tm);
  assert(isoTime(uint32_t(t))=="2026-11-11T15:30:07Z"&&isoTime(0)=="1970-01-01T00:00:00Z"&&isoTime(951782400)=="2000-02-29T00:00:00Z");
  Point a;a.lat=46123456;a.lon=-7000001;a.alt=1234;a.t=uint32_t(t);Point b;b.lat=-5;b.lon=7;b.alt=-3;b.t=0;
  assert(gpxPoint(a)=="<trkpt lat=\"46.123456\" lon=\"-7.000001\"><ele>1234</ele><time>2026-11-11T15:30:07Z</time></trkpt>\n");
  assert(gpxPoint(b)=="<trkpt lat=\"-0.000005\" lon=\"0.000007\"><ele>-3</ele></trkpt>\n");// no clock: no <time>
  std::string x=gpxHead("Hike & \"views\" <1>",uint32_t(t))+gpxPoint(a)+gpxPoint(b)+gpxTail();assert(wellFormed(x));
  assert(x.find("<name>Hike &amp; &quot;views&quot; &lt;1&gt;</name>")!=std::string::npos&&x.find("version=\"1.1\"")!=std::string::npos&&x.find("xmlns=\"http://www.topografix.com/GPX/1/1\"")!=std::string::npos&&x.find("<trkseg>")!=std::string::npos);
  assert(wellFormed(gpxHead("",0)+gpxTail()));assert(!wellFormed("<?xml version=\"1.0\" encoding=\"UTF-8\"?><a><b></a></b>"));assert(!wellFormed("<?xml version=\"1.0\" encoding=\"UTF-8\"?><a>x & y</a>"));
  assert(trackName(uint32_t(t))=="2026-11-11 15:30 UTC"&&trackName(0)=="Track without clock");
  assert(trackStem(uint32_t(t),77)=="trk-20261111-153007"&&trackStem(0,77)=="trk-n77");
 }
 // ---- route sketch: points scaled into a w x h box (pad inside), north up, aspect true to the ground
 {auto at=[](double lat,double lon){Point p;p.lat=int32_t(lat*1e6);p.lon=int32_t(lon*1e6);return p;};
  auto ew=shapeFit({at(60,10),at(60,10.02)},100,60,4);assert(ew.size()==2&&ew[0].x==4&&ew[1].x==96&&ew[0].y==30&&ew[1].y==30);// west-east, centred vertically
  auto ns=shapeFit({at(60,10),at(60.01,10)},100,60,4);assert(ns[0].x==50&&ns[1].x==50&&ns[0].y==56&&ns[1].y==4);// north is up
  auto sq=shapeFit({at(60,10),at(60,10.02),at(60.01,10.02),at(60.01,10)},100,60,4);// 1.1 km x 1.1 km on the ground: square on screen, centred
  int minX=1000,maxX=-1,minY=1000,maxY=-1;for(auto q:sq){minX=std::min(minX,q.x);maxX=std::max(maxX,q.x);minY=std::min(minY,q.y);maxY=std::max(maxY,q.y);}
  assert(std::abs((maxX-minX)-(maxY-minY))<=1&&maxY-minY==52&&std::abs((minX+maxX)/2-50)<=1);
  auto one=shapeFit({at(46,7)},100,60,4);assert(one.size()==1&&one[0].x==50&&one[0].y==30);auto same=shapeFit({at(46,7),at(46,7)},100,60,4);assert(same.size()==2&&same[1].x==50&&same[1].y==30);
  assert(shapeFit({},100,60,4).empty());
  std::vector<Point> many;for(int i=0;i<200;i++)many.push_back(at(46+0.0001*i*((i*7)%5),7+0.0002*i));for(auto q:shapeFit(many,100,60,4)){assert(q.x>=4&&q.x<=96&&q.y>=4&&q.y<=56);}}
 // ---- labels
 {assert(distanceLabel(0)=="0 m"&&distanceLabel(850)=="850 m"&&distanceLabel(2412)=="2.4 km"&&distanceLabel(12600)=="13 km"&&distanceLabel(999.6)=="1000 m");
  assert(durationLabel(0)=="0 s"&&durationLabel(45)=="45 s"&&durationLabel(720)=="12 min"&&durationLabel(3900)=="1 h 05 min"&&durationLabel(7200)=="2 h 00 min");
  assert(speedLabel(0)=="0.0 km/h"&&speedLabel(11.12)=="40.0 km/h");}
 std::cout<<"PASS: track log: sampling policy (interval AND distance, HDOP/age/range/jump rejects), stats, crash-safe rows with torn-tail pad, display thinning, GPX 1.1 well-formed with escaping and UTC times\n";
}
