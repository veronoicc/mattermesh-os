#include "tracks.h"
#include "location.h"
#include "prefs.h"
#include "sd_replace.h"
#include "clock_limits.h"
#include <algorithm>
#include <time.h>
namespace device {
using namespace mm_track;
constexpr const char *Dir="/mattermesh/tracks";constexpr size_t ThinPoints=256,SyncRows=6;constexpr uint32_t LoopMs=1000;
static fs::FS *storage=nullptr;static bool sdOk=false;static Settings cfg;
// Recording: the file is created at the first kept fix and closed (directory entry updated) every SyncRows points, so a power
// cut loses at most those rows' size update and one cut row, which the next boot closes (tracksBegin).
static bool recording=false,autoArmed=true,needRepair=false,fixNow=false;static File file;static std::string liveName,error;static Sampler sampler;static Stats liveStats;static Thin liveThin(ThinPoints);static std::vector<Point> liveLine;static uint32_t sinceSync=0,liveStart=0;
static TrackView view;static bool viewOn=false;
static std::string path(const std::string &stem,const char *ext){return std::string(Dir)+"/"+stem+ext;}
static bool ensureDir(){return (storage->exists("/mattermesh")||storage->mkdir("/mattermesh"))&&(storage->exists(Dir)||storage->mkdir(Dir));}
// Streams a track file through the row reader (512 byte chunks, any size of file).
template<class F> static bool readRows(const std::string &file,F &&on){
 File f=storage->open(file.c_str(),FILE_READ);if(!f)return false;Reader reader;uint8_t chunk[512];
 for(;;){size_t n=f.read(chunk,sizeof(chunk));if(n==0)break;reader.feed(chunk,n,on);}
 f.close();return true;}
static bool readRow(const std::string &file,bool last,Row &row){
 File f=storage->open(file.c_str(),FILE_READ);if(!f)return false;size_t size=f.size();bool ok=size>=RowBytes&&(!last||size%RowBytes==0);uint8_t raw[RowBytes];
 if(ok){ok=f.seek(last?size-RowBytes:0)&&f.read(raw,RowBytes)==RowBytes&&decodeRow(raw,row);}
 f.close();return ok;}
// A file cut inside a row: complete that row with a dead one so appends start on a boundary again.
static bool repairTail(const std::string &file){
 File f=storage->open(file.c_str(),FILE_READ);if(!f)return false;size_t size=f.size(),rem=size%RowBytes;if(!rem){f.close();return true;}
 uint8_t tail[RowBytes],pad[RowBytes];bool ok=f.seek(size-rem)&&f.read(tail,rem)==rem;f.close();if(!ok)return false;
 size_t n=tornPad(tail,rem,pad);File a=storage->open(file.c_str(),FILE_APPEND);if(!a)return false;ok=a.write(pad,n)==n;a.close();return ok;}
static bool appendSummary(const std::string &file,const Summary &s){
 if(!repairTail(file))return false;File a=storage->open(file.c_str(),FILE_APPEND);if(!a)return false;uint8_t row[RowBytes];encodeSummary(s,row);bool ok=a.write(row,RowBytes)==RowBytes;a.close();return ok;}
static bool lastRowIsSummary(const std::string &file){Row r;return readRow(file,true,r)&&r.kind=='S';}
static std::vector<std::string> trackStems(){
 std::vector<std::string> names;File d=storage->open(Dir);
 if(d&&d.isDirectory())for(File e=d.openNextFile();e;e=d.openNextFile()){std::string n=e.name();size_t slash=n.rfind('/');if(slash!=std::string::npos)n=n.substr(slash+1);if(!e.isDirectory()&&n.size()>4&&n.compare(n.size()-4,4,".mmt")==0)names.push_back(n.substr(0,n.size()-4));e.close();}
 if(d)d.close();std::sort(names.begin(),names.end());return names;}
static void loadSettings(){
 cfg=Settings();cfg.autoStart=prefGetInt("mmtracks","auto",0)!=0;cfg.showOnMap=prefGetInt("mmtracks","map",1)!=0;cfg.keepGps=prefGetInt("mmtracks","gps",1)!=0;
 cfg.intervalS=stepChoice(Intervals,uint16_t(prefGetInt("mmtracks","int",cfg.intervalS)),0);cfg.minDistM=stepChoice(Distances,uint16_t(prefGetInt("mmtracks","dist",cfg.minDistM)),0);}// stepChoice(.,.,0) = the nearest listed value
void tracksBegin(fs::FS &fs,bool sdReady){
 storage=&fs;sdOk=sdReady;loadSettings();if(!sdOk)return;
 // A track left open by a power cut: its cut row closed, the summary appended; one with no point at all is removed.
 for(auto &stem:trackStems()){std::string file=path(stem,".mmt");if(lastRowIsSummary(file))continue;Scan scan;if(!readRows(file,[&](const Row &r){scan.row(r);}))continue;
  if(scan.stats.points==0){storage->remove(file.c_str());continue;}appendSummary(file,scan.summary());}
}
Settings tracksSettings(){return cfg;}
bool tracksSetSettings(const Settings &s){
 Settings next=s;next.intervalS=stepChoice(Intervals,s.intervalS,0);next.minDistM=stepChoice(Distances,s.minDistM,0);
 bool ok=prefPutInt("mmtracks","auto",next.autoStart)&&prefPutInt("mmtracks","map",next.showOnMap)&&prefPutInt("mmtracks","gps",next.keepGps)&&prefPutInt("mmtracks","int",next.intervalS)&&prefPutInt("mmtracks","dist",next.minDistM);
 if(!ok)return false;cfg=next;sampler.cfg=cfg;if(recording)locationHold(cfg.keepGps);return true;}
bool tracksStart(){
 if(!sdOk)return false;if(recording)return true;recording=true;error.clear();sampler=Sampler();sampler.cfg=cfg;liveStats=Stats();liveThin=Thin(ThinPoints);liveLine.clear();liveName.clear();liveStart=0;sinceSync=0;needRepair=false;
 if(cfg.keepGps)locationHold(true);return true;}
void tracksStop(){
 if(!recording)return;recording=false;autoArmed=false;// no auto start again until the fix has been lost once
 if(file)file.close();
 if(!liveName.empty()&&!appendSummary(path(liveName,".mmt"),liveStats.summary()))error="Summary not written";// the next boot writes it
 if(viewOn&&view.live){view.live=false;}
 liveName.clear();locationHold(false);}
static bool openTrack(const Point &p,uint32_t nowMs){
 if(!ensureDir())return false;std::string stem,file;
 for(uint32_t i=0;i<60;i++){stem=trackStem(p.t?p.t+i:0,nowMs+i);file=path(stem,".mmt");if(!storage->exists(file.c_str()))break;}// two tracks in one second: the later one is named a second on
 File f=storage->open(file.c_str(),FILE_WRITE);if(!f)return false;Header h;h.startT=p.t;h.intervalS=cfg.intervalS;h.minDistM=cfg.minDistM;uint8_t row[RowBytes];encodeHeader(h,row);
 bool ok=f.write(row,RowBytes)==RowBytes;f.close();if(!ok){storage->remove(file.c_str());return false;}liveName=stem;liveStart=p.t;return true;}
static bool appendPoint(const Point &p){
 if(needRepair){if(!repairTail(path(liveName,".mmt")))return false;needRepair=false;}
 if(!file){file=storage->open(path(liveName,".mmt").c_str(),FILE_APPEND);if(!file)return false;}
 uint8_t row[RowBytes];encodePoint(p,row);bool ok=file.write(row,RowBytes)==RowBytes;
 if(!ok){file.close();needRepair=true;return false;}
 if(++sinceSync>=SyncRows){file.close();sinceSync=0;}return true;}
void tracksLoop(){
 static uint32_t at=0;uint32_t now=millis();if(!sdOk||uint32_t(now-at)<LoopMs)return;at=now;
 auto loc=locationState();fixNow=loc.fix;if(!loc.fix)autoArmed=true;
 if(!recording&&cfg.autoStart&&loc.fix&&autoArmed)tracksStart();
 if(!recording)return;
 Fix f;f.fix=loc.fix;f.lat=loc.latitude;f.lon=loc.longitude;f.alt=loc.altitude;f.ageMs=loc.fixAgeMs;f.hdop100=locationHdop();f.nowMs=now;time_t clock=time(nullptr);f.epoch=clock>=MM_EPOCH_MIN?uint32_t(clock):0;
 Point p;if(sampler.offer(f,p)!=Reject::None)return;
 if(liveName.empty()&&!openTrack(p,now)){error="Cannot create the track file";return;}
 if(!appendPoint(p)){error="SD write failed";return;}
 error.clear();liveStats.add(p);liveThin.add(p);liveLine=liveThin.points();
 if(viewOn&&view.live){view.stats=liveStats;view.line=liveLine;}}
TracksState tracksState(){
 TracksState s;s.sd=sdOk;s.recording=recording;s.fileOpen=!liveName.empty();s.fixNow=fixNow;s.points=liveStats.points;s.durationS=liveStats.durationS();s.startT=liveStart;s.distanceM=liveStats.distanceM;s.name=liveName;s.error=error;return s;}
// What the list shows for one file: the summary row of a closed track, else its points (cached per file size: a track
// being recorded is not re-read until it grows).
static bool infoFor(const std::string &stem,TrackInfo &info){
 info=TrackInfo();info.name=stem;std::string file=path(stem,".mmt");Row head,tail;if(!readRow(file,false,head)||head.kind!='H')return false;info.startT=head.h.startT;
 if(readRow(file,true,tail)&&tail.kind=='S'){info.sum=tail.s;info.closed=true;return true;}
 static struct {std::string stem;uint32_t size=0;Summary sum;} cache;File f=storage->open(file.c_str(),FILE_READ);uint32_t size=f?f.size():0;if(f)f.close();
 if(cache.stem==stem&&cache.size==size){info.sum=cache.sum;return true;}
 Scan scan;if(!readRows(file,[&](const Row &r){scan.row(r);}))return false;info.sum=scan.summary();cache.stem=stem;cache.size=size;cache.sum=info.sum;return true;}
std::vector<TrackInfo> tracksList(size_t max){
 std::vector<TrackInfo> out;if(!sdOk)return out;auto names=trackStems();
 for(auto it=names.rbegin();it!=names.rend()&&out.size()<max;++it){TrackInfo info;
  if(recording&&*it==liveName){info.name=*it;info.startT=liveStart;info.live=true;auto s=liveStats.summary();info.sum=s;out.push_back(info);continue;}
  if(infoFor(*it,info))out.push_back(info);}
 return out;}
bool tracksSelect(const std::string &name){
 if(!sdOk)return false;viewOn=false;view=TrackView();view.name=name;
 if(recording&&name==liveName){view.live=true;view.stats=liveStats;view.line=liveLine;viewOn=true;return true;}
 Scan scan;Thin thin(ThinPoints);if(!readRows(path(name,".mmt"),[&](const Row &r){scan.row(r);if(r.kind=='P')thin.add(r.p);}))return false;
 view.stats=scan.stats;view.line=thin.points();viewOn=true;return true;}
void tracksDeselect(){viewOn=false;view=TrackView();}
const TrackView *tracksSelected(){return viewOn?&view:nullptr;}
const std::vector<Point> &tracksMapLine(){static const std::vector<Point> none;if(viewOn)return view.line;if(recording&&cfg.showOnMap)return liveLine;return none;}
std::string tracksGpxPath(const std::string &name){return path(name,".gpx");}
bool tracksExportGpx(const std::string &name,std::string &out){
 if(!sdOk){out="No SD card";return false;}
 if(recording&&name==liveName&&file){file.close();sinceSync=0;}// the directory entry of the track being recorded up to date before it is read
 std::string src=path(name,".mmt"),dst=path(name,".gpx"),tmp=dst+".tmp";Row head;if(!readRow(src,false,head)||head.kind!='H'){out="Track unreadable";return false;}
 File w=storage->open(tmp.c_str(),FILE_WRITE);if(!w){out="Cannot write the GPX file";return false;}
 bool ok=true;std::string buf=gpxHead(trackName(head.h.startT),head.h.startT);
 auto flush=[&](){if(buf.empty())return;ok=ok&&w.write(reinterpret_cast<const uint8_t*>(buf.data()),buf.size())==buf.size();buf.clear();};
 ok=readRows(src,[&](const Row &r){if(r.kind!='P')return;buf+=gpxPoint(r.p);if(buf.size()>=1024)flush();})&&ok;
 buf+=gpxTail();flush();w.close();
 if(!ok||!sdReplace(*storage,tmp.c_str(),dst.c_str(),(dst+".bak").c_str(),false)){storage->remove(tmp.c_str());out="Export failed";return false;}
 out=dst;return true;}
bool tracksRemove(const std::string &name){
 if(!sdOk||(recording&&name==liveName))return false;if(viewOn&&view.name==name)tracksDeselect();return storage->remove(path(name,".mmt").c_str());}
}
