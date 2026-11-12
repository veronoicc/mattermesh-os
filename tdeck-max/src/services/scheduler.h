#pragma once
// Scheduled messages, pure logic (no Arduino): what is waiting to be sent, when it fires, what came of it, and the
// compact blob kept on the SD card (/mattermesh/scheduled.bin). The firmware (scheduled_app.inc) feeds `Inputs` once
// a second and hands the one item `poll` returns to the normal send path; the outcome comes back through `result`.
// Times: `nowEpoch` is the wall clock (valid only with `clockValid`); "heard" values are `millis()` of this boot, so a
// repeater heard before the item was armed (added, edited, or the boot that loaded it) never fires it.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
namespace mm_sched {
enum class Condition:uint8_t{AtTime=0,WhenRepeaterHeard=1,WhenRadioIdle=2,WhenContactHeard=3};
enum class Outcome:uint8_t{Sent=0,Failed=1};
enum class Status{Ok,Empty,BadHeader,BadVersion,Partial};
constexpr size_t MaxPending=16,MaxHistory=8,MaxTextBytes=160,MaxNameBytes=40,MaxReasonBytes=40,KeyHexBytes=64;
constexpr uint8_t Version=1;
constexpr uint32_t GraceSeconds=24*3600,RetryDelayMs=30000,IdleHoldMs=3000;// overdue longer than the grace: missed; the radio must be quiet this long
constexpr int MaxAttempts=3;
constexpr const char *ReasonGone="Chat removed or changed",*ReasonMissed="Missed while off";
inline const char *conditionName(Condition c){return c==Condition::AtTime?"At time":c==Condition::WhenRepeaterHeard?"Repeater heard":c==Condition::WhenRadioIdle?"Radio idle":"Contact heard";}
// The condition after `c` in the form's chooser (dir +1 / -1); "Contact heard" exists only in direct chats.
inline Condition nextCondition(Condition c,int dir,bool direct){int n=direct?4:3,i=(int(c)+(dir<0?n-1:1))%n;return Condition(i);}
// The time proposed for a new At time item: 15 minutes ahead, rounded up to five minutes.
inline uint32_t defaultAt(uint32_t nowEpoch){return (nowEpoch+900+299)/300*300;}
// The chat a message goes to, by identity as well as index: a removed or re-ordered chat is detected, never mis-sent.
struct Target {std::string name,scope,key;bool direct=false;};// key: the peer key (hex) of a direct chat
struct Item {
 uint32_t id=0;int chat=-1;Target target;std::string text;Condition condition=Condition::AtTime;uint32_t atEpoch=0,createdEpoch=0;int attempts=0;std::string lastError;
 uint32_t armedMs=0,retryAtMs=0;bool now=false;// this boot only: when it was armed, next retry, "send now" requested
};
struct History {std::string text,target,reason;Outcome outcome=Outcome::Sent;uint32_t whenEpoch=0;History(std::string t=std::string(),std::string tg=std::string(),std::string r=std::string(),Outcome o=Outcome::Sent,uint32_t w=0):text(std::move(t)),target(std::move(tg)),reason(std::move(r)),outcome(o),whenEpoch(w){}};// constructor: the firmware builds as C++11
struct Inputs {
 uint32_t nowEpoch=0,nowMs=0,repeaterHeardMs=0,radioIdleMs=0;bool clockValid=false,radioSuspended=false,txQueueFree=true;
 std::function<uint32_t(const std::string &)> contactHeardMs;// millis() a contact (peer key hex) was last heard, 0 never
 std::function<bool(const Item &)> targetPresent;// the item's chat still exists unchanged
};
// The date and time fields of the form, with the rules of the Date / time page (day, month, year, hour, minute).
struct DateEdit {
 int f[5]={1,1,2026,12,0},typed=0;
 static int daysIn(int month,int year){static const int d[]={31,28,31,30,31,30,31,31,30,31,30,31};bool leap=(year%4==0&&year%100!=0)||year%400==0;return month==2&&leap?29:d[std::max(1,std::min(12,month))-1];}
 static int lo(int field){return field<=1?1:field==2?2024:0;}
 int hi(int field)const{return field==0?daysIn(f[1],f[2]):field==1?12:field==2?2099:field==3?23:59;}
 void clampDay(){f[0]=std::max(1,std::min(f[0],daysIn(f[1],f[2])));}
 void set(int day,int month,int year,int hour,int minute){f[0]=day;f[1]=month;f[2]=year;f[3]=hour;f[4]=minute;typed=0;clampDay();}
 void step(int field,int delta){int v=f[field]+delta;f[field]=v>hi(field)?lo(field):v<lo(field)?hi(field):v;typed=0;clampDay();}
 // Digits replace the field (two for most, four for the year); true once the field is complete.
 bool digit(int field,int d){int width=field==2?4:2,v=typed?f[field]*10+d:d;typed++;if(typed>=width){f[field]=std::max(lo(field),std::min(v,field==0?31:hi(field)));typed=0;clampDay();return true;}f[field]=v;return false;}
};
inline uint32_t crc32(const uint8_t *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=c&1?0xedb88320u^(c>>1):c>>1;}return ~c;}
inline std::string clipUtf8(std::string s,size_t max){if(s.size()<=max)return s;size_t n=max;while(n>0&&(uint8_t(s[n])&0xC0)==0x80)n--;s.resize(n);return s;}
inline std::string trimmed(const std::string &s){size_t a=0,b=s.size();while(a<b&&(s[a]==' '||s[a]=='\t'||s[a]=='\n'||s[a]=='\r'))a++;while(b>a&&(s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\n'||s[b-1]=='\r'))b--;return s.substr(a,b-a);}
class Scheduler {
 static bool before(const Item &a,const Item &b){bool ta=a.condition==Condition::AtTime,tb=b.condition==Condition::AtTime;if(ta!=tb)return ta;if(ta&&a.atEpoch!=b.atEpoch)return a.atEpoch<b.atEpoch;return a.id<b.id;}
 void sort(){std::stable_sort(items.begin(),items.end(),before);}
 static bool check(const std::string &text,Condition c,uint32_t at,bool direct,std::string &why){
  if(text.empty()){why="Message empty";return false;}if(text.size()>MaxTextBytes){why="Message too long";return false;}
  if(c==Condition::WhenContactHeard&&!direct){why="Contacts only in direct chats";return false;}if(c==Condition::AtTime&&!at){why="Pick a time";return false;}return true;}
 void finish(const Item &it,Outcome o,const std::string &reason,uint32_t when){history.insert(history.begin(),History{it.text,it.target.name,clipUtf8(reason,MaxReasonBytes),o,when});if(history.size()>MaxHistory)history.resize(MaxHistory);finished++;dirty=true;}
 void drop(uint32_t id){items.erase(std::remove_if(items.begin(),items.end(),[&](const Item &i){return i.id==id;}),items.end());}
 static bool ready(const Item &it,const Inputs &in){
  if(it.now)return true;if(it.attempts>0&&int32_t(in.nowMs-it.retryAtMs)<0)return false;
  switch(it.condition){
   case Condition::AtTime:return in.clockValid&&in.nowEpoch>=it.atEpoch;
   case Condition::WhenRepeaterHeard:return in.repeaterHeardMs&&int32_t(in.repeaterHeardMs-it.armedMs)>0;
   case Condition::WhenRadioIdle:return in.radioIdleMs>=IdleHoldMs;
   case Condition::WhenContactHeard:{uint32_t h=in.contactHeardMs?in.contactHeardMs(it.target.key):0;return h&&int32_t(h-it.armedMs)>0;}}
  return false;}
public:
 std::vector<Item> items;std::vector<History> history;uint32_t nextId=1,finished=0;bool dirty=false;// dirty: the persisted state changed since the last save; finished: sent or failed since start (the app announces new ones)
 Item *find(uint32_t id){for(auto &i:items)if(i.id==id)return &i;return nullptr;}
 // New item (armed now); its id, or 0 with the reason.
 uint32_t add(Item it,uint32_t nowMs,uint32_t nowEpoch,std::string &why){
  it.text=trimmed(it.text);it.target.name=clipUtf8(it.target.name,MaxNameBytes);it.target.scope=clipUtf8(it.target.scope,MaxNameBytes);it.target.key=clipUtf8(it.target.key,KeyHexBytes);
  if(!check(it.text,it.condition,it.atEpoch,it.target.direct,why))return 0;
  if(items.size()>=MaxPending){why="Scheduled list full ("+std::to_string(MaxPending)+")";return 0;}
  it.id=nextId++;it.createdEpoch=nowEpoch;it.attempts=0;it.lastError.clear();it.armedMs=nowMs;it.retryAtMs=0;it.now=false;
  items.push_back(it);sort();dirty=true;return it.id;}
 // Text, condition and time of an item; the target stays. Re-arms it (a repeater heard earlier does not fire it).
 bool edit(uint32_t id,const std::string &text,Condition c,uint32_t at,uint32_t nowMs,std::string &why){
  Item *it=find(id);if(!it){why="Message gone";return false;}std::string t=trimmed(text);if(!check(t,c,at,it->target.direct,why))return false;
  it->text=t;it->condition=c;it->atEpoch=c==Condition::AtTime?at:0;it->attempts=0;it->lastError.clear();it->armedMs=nowMs;it->retryAtMs=0;it->now=false;sort();dirty=true;return true;}
 bool remove(uint32_t id){if(!find(id))return false;drop(id);dirty=true;return true;}
 // SEND NOW: due at the next poll whatever its condition (the radio gates still apply).
 bool sendNow(uint32_t id){Item *it=find(id);if(!it)return false;it->now=true;it->attempts=0;it->retryAtMs=0;it->lastError.clear();return true;}
 // Fails the items that can never be sent (chat gone, missed while off), then names the one item to send now, 0 for none.
 uint32_t poll(const Inputs &in){
  std::vector<Item> gone;
  for(auto &it:items){const bool present=!in.targetPresent||in.targetPresent(it);
   if(!present||(it.condition==Condition::AtTime&&!it.now&&in.clockValid&&in.nowEpoch>it.atEpoch&&in.nowEpoch-it.atEpoch>GraceSeconds))gone.push_back(it);}
  for(auto &it:gone){bool present=!in.targetPresent||in.targetPresent(it);finish(it,Outcome::Failed,present?ReasonMissed:ReasonGone,in.clockValid?in.nowEpoch:0);drop(it.id);}
  if(in.radioSuspended||!in.txQueueFree)return 0;
  for(auto &it:items)if(ready(it,in))return it.id;return 0;}
 // The outcome of sending `id`: done and in the history, or another try in RetryDelayMs (MaxAttempts, then Failed).
 void result(uint32_t id,bool ok,const std::string &reason,const Inputs &in){
  Item *it=find(id);if(!it)return;const uint32_t when=in.clockValid?in.nowEpoch:0;
  if(ok){finish(*it,Outcome::Sent,"",when);drop(id);return;}
  it->now=false;it->attempts++;it->lastError=clipUtf8(reason,MaxReasonBytes);it->retryAtMs=in.nowMs+RetryDelayMs;dirty=true;
  if(it->attempts>=MaxAttempts){finish(*it,Outcome::Failed,reason,when);drop(id);}}
 // ---- the blob: "MSCH", version, item count, history count, pad, next id, header CRC; then one record per item and
 // history row: u16 length, payload, CRC of the payload. A damaged record ends the list; what came before is kept.
 std::string encode()const{
  std::string out="MSCH";auto u8=[&](std::string &s,unsigned v){s+=char(v&255);};auto u16=[&](std::string &s,unsigned v){u8(s,v);u8(s,v>>8);};auto u32=[&](std::string &s,uint32_t v){u16(s,v&0xffff);u16(s,v>>16);};
  auto s8=[&](std::string &s,const std::string &v,size_t max){std::string c=clipUtf8(v,max);u8(s,c.size());s+=c;};auto s16=[&](std::string &s,const std::string &v,size_t max){std::string c=clipUtf8(v,max);u16(s,c.size());s+=c;};
  u8(out,Version);u8(out,items.size());u8(out,history.size());u8(out,0);u32(out,nextId);u32(out,crc32(reinterpret_cast<const uint8_t *>(out.data()),out.size()));
  auto record=[&](const std::string &p){u16(out,p.size());out+=p;u32(out,crc32(reinterpret_cast<const uint8_t *>(p.data()),p.size()));};
  for(auto &i:items){std::string p;u32(p,i.id);u8(p,unsigned(i.chat));u8(p,i.target.direct);u8(p,unsigned(i.condition));u8(p,unsigned(i.attempts));u32(p,i.atEpoch);u32(p,i.createdEpoch);s8(p,i.target.name,MaxNameBytes);s8(p,i.target.scope,MaxNameBytes);s8(p,i.target.key,KeyHexBytes);s16(p,i.text,MaxTextBytes);s8(p,i.lastError,MaxReasonBytes);record(p);}
  for(auto &h:history){std::string p;u8(p,unsigned(h.outcome));u32(p,h.whenEpoch);s16(p,h.text,MaxTextBytes);s8(p,h.target,MaxNameBytes);s8(p,h.reason,MaxReasonBytes);record(p);}
  return out;}
 // Replaces the contents with the valid part of `blob`.
 Status decode(const std::string &blob){
  items.clear();history.clear();nextId=1;dirty=false;if(blob.empty())return Status::Empty;
  const uint8_t *d=reinterpret_cast<const uint8_t *>(blob.data());const size_t n=blob.size();
  auto at32=[&](size_t o){return uint32_t(d[o])|uint32_t(d[o+1])<<8|uint32_t(d[o+2])<<16|uint32_t(d[o+3])<<24;};
  if(n<16||memcmp(d,"MSCH",4))return Status::BadHeader;if(d[4]!=Version)return Status::BadVersion;
  if(at32(12)!=crc32(d,12)||d[5]>MaxPending||d[6]>MaxHistory)return Status::BadHeader;
  const size_t nItems=d[5],nHist=d[6];uint32_t maxId=0;nextId=std::max<uint32_t>(1,at32(8));size_t pos=16;
  for(size_t r=0;r<nItems+nHist;r++){
   if(pos+2>n)return Status::Partial;size_t len=d[pos]|d[pos+1]<<8;if(pos+2+len+4>n)return Status::Partial;
   const uint8_t *p=d+pos+2;if(at32(pos+2+len)!=crc32(p,len))return Status::Partial;pos+=2+len+4;
   size_t i=0;bool ok=true;
   auto need=[&](size_t k){if(i+k>len){ok=false;return false;}return true;};
   auto g8=[&]()->unsigned{return need(1)?p[i++]:0;};auto g16=[&]()->unsigned{unsigned a=g8();return a|g8()<<8;};auto g32=[&]()->uint32_t{uint32_t a=g16();return a|uint32_t(g16())<<16;};
   auto gs=[&](size_t count,size_t max)->std::string{if(count>max||!need(count)){ok=false;return "";}std::string s(reinterpret_cast<const char *>(p+i),count);i+=count;return s;};
   if(r<nItems){Item it;it.id=g32();it.chat=int(g8());it.target.direct=g8()!=0;unsigned c=g8();it.attempts=int(g8());it.atEpoch=g32();it.createdEpoch=g32();size_t k=g8();it.target.name=gs(k,MaxNameBytes);k=g8();it.target.scope=gs(k,MaxNameBytes);k=g8();it.target.key=gs(k,KeyHexBytes);k=g16();it.text=gs(k,MaxTextBytes);k=g8();it.lastError=gs(k,MaxReasonBytes);
    if(!ok||c>3||it.text.empty()||!it.id||it.attempts>=MaxAttempts||(it.target.key.size()!=0&&it.target.key.size()!=KeyHexBytes))return Status::Partial;
    it.condition=Condition(c);if(it.condition==Condition::WhenContactHeard&&!it.target.direct)return Status::Partial;
    if(find(it.id))return Status::Partial;maxId=std::max(maxId,it.id);items.push_back(it);}
   else{History h;unsigned o=g8();h.whenEpoch=g32();size_t k=g16();h.text=gs(k,MaxTextBytes);k=g8();h.target=gs(k,MaxNameBytes);k=g8();h.reason=gs(k,MaxReasonBytes);if(!ok||o>1)return Status::Partial;h.outcome=Outcome(o);history.push_back(h);}
  }
  nextId=std::max(nextId,maxId+1);sort();return Status::Ok;}
};
}
