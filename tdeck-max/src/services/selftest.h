#pragma once
#include <stdint.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <vector>
// Hardware self-test (Self-test app, section 60, and the serial "selftest" command). Pure: no Arduino. A check is one
// callable step(index,phase,elapsedMs): Start prepares and may already finish, Poll runs every main loop pass until the
// check finishes, Abort tells it to stop (timeout or cancel) so it can release what it holds. The runner starts at most
// one check per tick and polls only the current one, so the UI keeps refreshing between ticks.
namespace selftest {
enum class Status:uint8_t {Idle,Running,Input,Pass,Fail,Skip};// Input: waiting for the user (key press, tap)
enum class Phase:uint8_t {Start,Poll,Abort};
constexpr size_t MaxDetail=96;
struct Outcome {Status status;std::string detail;Outcome(Status s=Status::Running,std::string d=""):status(s),detail(std::move(d)){}};// Running / Input: not finished (detail is the prompt for Input)
inline Outcome pass(std::string d=""){return Outcome(Status::Pass,std::move(d));}
inline Outcome fail(std::string d=""){return Outcome(Status::Fail,std::move(d));}
inline Outcome skip(std::string d=""){return Outcome(Status::Skip,std::move(d));}
inline Outcome wait(std::string d=""){return Outcome(Status::Input,std::move(d));}
struct Spec {const char *name,*label;uint32_t timeoutMs;bool interactive,output;};// output: makes sound or vibration (quiet mode skips it); the firmware builds as C++11, so no default member initializers on aggregates
struct Entry {Spec spec;Status status;std::string detail;uint32_t ms;Entry(Spec s=Spec{"","",0,false,false}):spec(s),status(Status::Idle),ms(0){}};
struct Counts {int pass=0,fail=0,skip=0,idle=0;};
struct Options {bool interactive=true,output=true;};
inline const char *statusText(Status s){switch(s){case Status::Running:return "running";case Status::Input:return "input";case Status::Pass:return "pass";case Status::Fail:return "fail";case Status::Skip:return "skip";default:return "idle";}}
// One line, printable ASCII/UTF-8 kept, newlines folded to spaces, cut to MaxDetail bytes.
inline std::string cleanDetail(std::string d){for(auto &ch:d)if(ch=='\n'||ch=='\r'||ch=='\t')ch=' ';if(d.size()>MaxDetail)d.resize(MaxDetail);return d;}
class Runner {
 std::deque<int> queue,done;int cur=-1;uint32_t startedAt=0;
 void finish(int i,Status s,std::string d,uint32_t now){auto &e=entries[i];e.status=s;e.detail=std::move(d);e.ms=now-startedAt;cur=-1;batchDone++;done.push_back(i);}
public:
 std::vector<Entry> entries;Options options;int batchTotal=0,batchDone=0;
 int add(Spec s){entries.push_back({s});return int(entries.size())-1;}
 int find(const std::string &name)const{for(size_t i=0;i<entries.size();i++)if(name==entries[i].spec.name)return int(i);return -1;}
 bool busy()const{return cur>=0||!queue.empty();}
 int current()const{return cur;}
 uint32_t started()const{return startedAt;}
 // Queue every check; those the options switch off are marked skipped at once (reported like any finished check).
 bool runAll(){if(busy())return false;batchTotal=batchDone=0;for(size_t i=0;i<entries.size();i++){auto &e=entries[i];e.detail.clear();e.ms=0;
   if((!options.interactive&&e.spec.interactive)||(!options.output&&e.spec.output)){e.status=Status::Skip;e.detail="disabled in options";done.push_back(int(i));}else{e.status=Status::Idle;queue.push_back(int(i));batchTotal++;}}
  return true;}
 // One check, whatever the options say (the user asked for it by name).
 bool runOne(int i){if(busy()||i<0||i>=int(entries.size()))return false;auto &e=entries[i];e.status=Status::Idle;e.detail.clear();e.ms=0;batchTotal=1;batchDone=0;queue.push_back(i);return true;}
 // The running check is told to stop; the rest of the queue goes back to "not run".
 template<class Step> void cancel(Step &&step){if(cur>=0){step(cur,Phase::Abort,0);auto &e=entries[cur];e.status=Status::Skip;e.detail="cancelled";cur=-1;done.push_back(int(&e-entries.data()));}queue.clear();}
 // Advance one check by one step. True on the tick that ends the batch.
 template<class Step> bool tick(uint32_t now,Step &&step){
  if(cur<0){if(queue.empty())return false;cur=queue.front();queue.pop_front();auto &e=entries[cur];e.status=Status::Running;e.detail.clear();startedAt=now;apply(step(cur,Phase::Start,0),now);return ended();}
  uint32_t elapsed=now-startedAt;auto &e=entries[cur];
  if(elapsed>=e.spec.timeoutMs){step(cur,Phase::Abort,elapsed);int i=cur;if(e.status==Status::Input)finish(i,Status::Skip,"no input in "+std::to_string(e.spec.timeoutMs/1000)+" s",now);else finish(i,Status::Fail,"timed out after "+std::to_string(e.spec.timeoutMs)+" ms",now);return ended();}
  apply(step(cur,Phase::Poll,elapsed),now);return ended();}
 // Finished checks in the order they ended (for the serial echo); -1 when none are waiting.
 int takeDone(){if(done.empty())return -1;int i=done.front();done.pop_front();return i;}
 Counts counts()const{Counts c;for(auto &e:entries){if(e.status==Status::Pass)c.pass++;else if(e.status==Status::Fail)c.fail++;else if(e.status==Status::Skip)c.skip++;else c.idle++;}return c;}
private:
 bool ended()const{return cur<0&&queue.empty();}
 void apply(const Outcome &o,uint32_t now){auto &e=entries[cur];if(o.status==Status::Running||o.status==Status::Input){e.status=o.status;if(!o.detail.empty())e.detail=cleanDetail(o.detail);return;}finish(cur,o.status,cleanDetail(o.detail),now);}
};
// "SELFTEST name=sd result=pass detail=..." (one per check) and the closing "SELFTEST_DONE pass=.. fail=.. skip=.. notrun=..".
inline std::string serialLine(const Entry &e){return std::string("SELFTEST name=")+e.spec.name+" result="+statusText(e.status)+" detail="+cleanDetail(e.detail);}
inline std::string serialSummary(const Counts &c){return "SELFTEST_DONE pass="+std::to_string(c.pass)+" fail="+std::to_string(c.fail)+" skip="+std::to_string(c.skip)+" notrun="+std::to_string(c.idle);}
// The text report written to the SD card: one block per check, then the totals.
inline std::string report(const Runner &r,const std::string &stamp,const std::string &firmware){
 std::string s="MatterMesh OS self-test\n";if(!firmware.empty())s+="Firmware: "+firmware+"\n";if(!stamp.empty())s+="Date: "+stamp+"\n";s+="\n";
 for(auto &e:r.entries){const char *tag=e.status==Status::Pass?"PASS":e.status==Status::Fail?"FAIL":e.status==Status::Skip?"SKIP":"----";bool ran=e.status==Status::Pass||e.status==Status::Fail||e.status==Status::Skip;
  s+=std::string("[")+tag+"] "+e.spec.label+" ("+e.spec.name+")"+(ran?" "+std::to_string(e.ms)+" ms":"")+"\n";if(!e.detail.empty())s+="       "+e.detail+"\n";}
 auto c=r.counts();return s+"\nResult: "+std::to_string(c.pass)+" pass, "+std::to_string(c.fail)+" fail, "+std::to_string(c.skip)+" skip, "+std::to_string(c.idle)+" not run\n";}
inline std::string reportName(bool clock,int y,int mo,int d,int h,int mi,int sec,uint32_t uptimeSecs){char b[40];if(clock)snprintf(b,sizeof(b),"selftest-%04d%02d%02d-%02d%02d%02d.txt",y,mo,d,h,mi,sec);else snprintf(b,sizeof(b),"selftest-up%lu.txt",(unsigned long)uptimeSecs);return b;}
// CRC-32 (IEEE, reflected) for the SD write / read-back check; pass the previous value to continue a stream.
inline uint32_t crc32(const uint8_t *p,size_t n,uint32_t crc=0){crc=~crc;for(size_t i=0;i<n;i++){crc^=p[i];for(int k=0;k<8;k++)crc=(crc>>1)^(0xEDB88320u&(0u-(crc&1)));}return ~crc;}
// Verdicts of the hardware checks.
inline bool rssiSane(float dbm){return std::isfinite(dbm)&&dbm>=-150&&dbm<=0;}// SX1262 instantaneous RSSI
inline const char *hapticChip(uint8_t status){switch(status>>5){case 3:return "DRV2605";case 4:return "DRV2604";case 6:return "DRV2604L";case 7:return "DRV2605L";default:return nullptr;}}// DRV260x STATUS register: device id in bits 7:5
inline bool batterySane(int mv){return mv>=2800&&mv<=4500;}// BQ27220 cell voltage
inline std::string sizeText(uint64_t b){char t[24];if(b>=(1ull<<30))snprintf(t,sizeof(t),"%.1f GB",b/double(1ull<<30));else if(b>=(1ull<<20))snprintf(t,sizeof(t),"%lu MB",(unsigned long)(b>>20));else if(b>=1024)snprintf(t,sizeof(t),"%lu KB",(unsigned long)(b>>10));else snprintf(t,sizeof(t),"%lu B",(unsigned long)b);return t;}
}
