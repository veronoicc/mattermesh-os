#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "services/scheduler.h"
using namespace mm_sched;
// A scheduled message to chat 1 ("Alice", direct, key aa..) or chat 0 ("Public" channel).
static Item make(const std::string &text,Condition c,uint32_t at=0,bool direct=false){Item it;it.text=text;it.condition=c;it.atEpoch=at;it.chat=direct?1:0;it.target.name=direct?"Alice":"Public";it.target.scope="*";it.target.direct=direct;if(direct)it.target.key=std::string(64,'a');return it;}
static Inputs inputs(uint32_t epoch,uint32_t ms){Inputs in;in.clockValid=epoch!=0;in.nowEpoch=epoch;in.nowMs=ms;in.targetPresent=[](const Item &){return true;};return in;}
int main(){
 const uint32_t T=1800000000u;std::string why;
 // ---- add: validation, limits, ids, ordering by time
 {Scheduler s;assert(s.add(make("",Condition::AtTime,T),0,T,why)==0&&why=="Message empty");assert(s.add(make("   ",Condition::AtTime,T),0,T,why)==0);
  assert(s.add(make(std::string(MaxTextBytes+1,'x'),Condition::AtTime,T),0,T,why)==0&&!why.empty());
  assert(s.add(make("hi",Condition::WhenContactHeard),0,T,why)==0&&why=="Contacts only in direct chats");
  assert(s.add(make("hi",Condition::AtTime,0),0,T,why)==0&&why=="Pick a time");
  uint32_t b=s.add(make("later",Condition::AtTime,T+600),0,T,why),a=s.add(make(" early ",Condition::AtTime,T+60),0,T,why),e=s.add(make("evt",Condition::WhenRadioIdle),0,T,why);
  assert(a&&b&&e&&a!=b&&b!=e);assert(s.items.size()==3&&s.items[0].id==a&&s.items[1].id==b&&s.items[2].id==e);assert(s.items[0].text=="early"&&s.items[0].createdEpoch==T);assert(s.dirty);
  Scheduler full;for(size_t i=0;i<MaxPending;i++)assert(full.add(make("m",Condition::WhenRadioIdle),0,T,why));assert(full.add(make("m",Condition::WhenRadioIdle),0,T,why)==0&&why.find("full")!=std::string::npos);}
 // ---- AtTime: fires at the time, never with the clock unset, one dispatch per poll, earliest first
 {Scheduler s;uint32_t a=s.add(make("a",Condition::AtTime,T+100),0,T,why),b=s.add(make("b",Condition::AtTime,T+50),0,T,why);
  assert(s.poll(inputs(T+49,1000))==0);assert(s.poll(inputs(T+50,1000))==b);assert(s.poll(inputs(T+500,1000))==b);// not removed until the result is reported
  Inputs unset=inputs(0,1000);assert(s.poll(unset)==0);
  s.result(b,true,"",inputs(T+500,1000));assert(s.items.size()==1);assert(s.poll(inputs(T+500,2000))==a);}
 // ---- Repeater: only a repeater heard after the item was armed; edits re-arm
 {Scheduler s;uint32_t r=s.add(make("r",Condition::WhenRepeaterHeard),5000,T,why);Inputs in=inputs(T,6000);in.repeaterHeardMs=0;assert(s.poll(in)==0);
  in.repeaterHeardMs=4000;assert(s.poll(in)==0);// an old advert
  in.repeaterHeardMs=5000;assert(s.poll(in)==0);// the same instant is not "after"
  in.repeaterHeardMs=5001;assert(s.poll(in)==r);
  in.nowMs=9000;assert(s.edit(r,"r2",Condition::WhenRepeaterHeard,0,9000,why));in.repeaterHeardMs=8000;assert(s.poll(in)==0);in.repeaterHeardMs=9500;assert(s.poll(in)==r);
  Scheduler boot;boot.items=s.items;for(auto &i:boot.items)i.armedMs=0;in.repeaterHeardMs=3;assert(boot.poll(in)==r);}
 // ---- Idle: only after the radio was quiet for IdleHoldMs
 {Scheduler s;uint32_t i=s.add(make("i",Condition::WhenRadioIdle),0,T,why);Inputs in=inputs(T,1000);in.radioIdleMs=IdleHoldMs-1;assert(s.poll(in)==0);in.radioIdleMs=IdleHoldMs;assert(s.poll(in)==i);}
 // ---- Contact heard: that peer, after arming
 {Scheduler s;uint32_t c=s.add(make("c",Condition::WhenContactHeard,0,true),2000,T,why);Inputs in=inputs(T,3000);std::string asked;in.contactHeardMs=[&](const std::string &k){asked=k;return uint32_t(1500);};assert(s.poll(in)==0&&asked==std::string(64,'a'));
  in.contactHeardMs=[](const std::string &){return uint32_t(2500);};assert(s.poll(in)==c);in.contactHeardMs=nullptr;assert(s.poll(in)==0);}
 // ---- Gates: companion link / full TX queue keep everything pending
 {Scheduler s;uint32_t i=s.add(make("i",Condition::WhenRadioIdle),0,T,why);Inputs in=inputs(T,1000);in.radioIdleMs=10000;in.radioSuspended=true;assert(s.poll(in)==0);in.radioSuspended=false;in.txQueueFree=false;assert(s.poll(in)==0);in.txQueueFree=true;assert(s.poll(in)==i);assert(s.items.size()==1);}
 // ---- Send now: forced regardless of the condition (still gated by the radio)
 {Scheduler s;uint32_t a=s.add(make("a",Condition::AtTime,T+9999),0,T,why);assert(s.poll(inputs(T,1000))==0);assert(s.sendNow(a)&&!s.sendNow(999));assert(s.poll(inputs(T,1000))==a);Inputs in=inputs(0,1000);assert(s.poll(in)==a);in.radioSuspended=true;assert(s.poll(in)==0);}
 // ---- Retry with back-off: three attempts 30 s apart, then Failed with the reason
 {Scheduler s;uint32_t a=s.add(make("a",Condition::WhenRadioIdle,0),0,T,why);Inputs in=inputs(T,1000);in.radioIdleMs=10000;
  assert(s.poll(in)==a);s.result(a,false,"Radio offline",in);assert(s.items.size()==1&&s.items[0].attempts==1&&s.items[0].lastError=="Radio offline");
  in.nowMs=1000+RetryDelayMs-1;assert(s.poll(in)==0);in.nowMs=1000+RetryDelayMs;assert(s.poll(in)==a);s.result(a,false,"Radio offline",in);assert(s.items[0].attempts==2);
  in.nowMs+=RetryDelayMs;assert(s.poll(in)==a);in.nowEpoch=T+90;s.result(a,false,"SD save failed",in);assert(s.items.empty());
  assert(s.history.size()==1&&s.history[0].outcome==Outcome::Failed&&s.history[0].reason=="SD save failed"&&s.history[0].target=="Public"&&s.history[0].text=="a"&&s.history[0].whenEpoch==T+90);
  uint32_t b=s.add(make("b",Condition::WhenRadioIdle),0,T,why);assert(s.sendNow(b));in.nowMs=100;assert(s.poll(in)==b);s.result(b,true,"",in);assert(s.history[0].outcome==Outcome::Sent&&s.history[0].text=="b");}
 // ---- Retry forced by Send now resets the back-off
 {Scheduler s;uint32_t a=s.add(make("a",Condition::WhenRadioIdle),0,T,why);Inputs in=inputs(T,1000);in.radioIdleMs=10000;s.result(a,false,"x",in);assert(s.poll(in)==0);assert(s.sendNow(a));assert(s.items[0].attempts==0);assert(s.poll(in)==a);}
 // ---- Target gone / changed fails safely with a reason; others unaffected
 {Scheduler s;uint32_t a=s.add(make("to alice",Condition::AtTime,T,true),0,T,why),b=s.add(make("to public",Condition::AtTime,T+1),0,T,why);Inputs in=inputs(T+5,1000);
  in.targetPresent=[](const Item &it){return it.target.name!="Alice";};assert(s.poll(in)==b);assert(s.items.size()==1&&s.items[0].id==b);assert(s.history.size()==1&&s.history[0].reason=="Chat removed or changed"&&s.history[0].target=="Alice");(void)a;}
 // ---- Missed while off: overdue by more than the grace fails, inside it fires
 {Scheduler s;uint32_t a=s.add(make("a",Condition::AtTime,T),0,T,why);Inputs in=inputs(T+GraceSeconds,1000);assert(s.poll(in)==a);in.nowEpoch=T+GraceSeconds+1;assert(s.poll(in)==0);assert(s.items.empty()&&s.history.size()==1&&s.history[0].reason=="Missed while off");
  Scheduler u;uint32_t b=u.add(make("b",Condition::AtTime,T),0,T,why);Inputs unset=inputs(0,1000);assert(u.poll(unset)==0&&u.items.size()==1);(void)b;}
 // ---- Edit / remove
 {Scheduler s;uint32_t a=s.add(make("a",Condition::AtTime,T+100),0,T,why),b=s.add(make("b",Condition::AtTime,T+200),0,T,why);
  assert(s.edit(b,"bb",Condition::AtTime,T+50,0,why)&&s.items[0].id==b&&s.items[0].text=="bb");// re-ordered by the new time
  assert(!s.edit(b,"",Condition::AtTime,T+50,0,why)&&why=="Message empty");assert(!s.edit(b,"x",Condition::WhenContactHeard,0,0,why));assert(!s.edit(77,"x",Condition::AtTime,T,0,why));
  s.items[1].attempts=2;s.items[1].lastError="boom";assert(s.edit(a,"a2",Condition::AtTime,T+100,0,why)&&s.find(a)->attempts==0&&s.find(a)->lastError.empty());
  assert(s.remove(a)&&!s.remove(a)&&s.items.size()==1);}
 // ---- History ring: newest first, the last MaxHistory kept
 {Scheduler s;Inputs in=inputs(T,1000);for(int i=0;i<12;i++){uint32_t id=s.add(make("m"+std::to_string(i),Condition::WhenRadioIdle),0,T,why);in.nowEpoch=T+i;for(int k=0;k<(i%2==0?1:MaxAttempts);k++)s.result(id,i%2==0,"bad",in);}
  assert(s.history.size()==MaxHistory&&s.history[0].text=="m11"&&s.history[0].outcome==Outcome::Failed&&s.history[1].outcome==Outcome::Sent&&s.history[7].text=="m4");assert(s.items.empty()&&s.finished==12);}
 // ---- Persistence: round trip incl. UTF-8, direct targets, history, ids
 {Scheduler s;Item d=make("Gr\xC3\xBC\xC3\x9F" "e \xF0\x9F\x91\x8B",Condition::WhenContactHeard,0,true);d.chat=7;uint32_t a=s.add(d,0,T,why);uint32_t b=s.add(make("at",Condition::AtTime,T+60),0,T,why);s.find(b)->attempts=1;s.find(b)->lastError="Radio offline";
  Inputs in=inputs(T+9,0);uint32_t c=s.add(make("gone",Condition::WhenRadioIdle),0,T,why);s.result(c,true,"",in);s.history.push_back({"h2 \xC3\xA4","Alice","nope",Outcome::Failed,T+3});
  std::string blob=s.encode();assert(blob.size()>16&&blob.compare(0,4,"MSCH")==0);
  Scheduler r;assert(r.decode(blob)==Status::Ok);assert(r.items.size()==2&&r.history.size()==2);
  const Item *x=r.find(a);assert(x&&x->text==d.text&&x->chat==7&&x->condition==Condition::WhenContactHeard&&x->target.direct&&x->target.key==std::string(64,'a')&&x->target.name=="Alice"&&x->target.scope=="*"&&x->createdEpoch==T&&x->armedMs==0);
  const Item *y=r.find(b);assert(y&&y->atEpoch==T+60&&y->attempts==1&&y->lastError=="Radio offline");assert(r.history[0].outcome==Outcome::Sent&&r.history[0].whenEpoch==T+9&&r.history[1].text=="h2 \xC3\xA4"&&r.history[1].reason=="nope");
  uint32_t n=r.add(make("n",Condition::WhenRadioIdle),0,T,why);assert(n>c&&n>b&&n>a);assert(r.encode().size()>blob.size());
  Scheduler again;assert(again.decode(s.encode())==Status::Ok&&again.encode()==blob);}
 // ---- Persistence: empty, wrong magic, wrong version, truncation, corruption, bounds
 {Scheduler s;for(int i=0;i<5;i++)s.add(make("message number "+std::to_string(i),Condition::AtTime,T+60*(i+1)),0,T,why);Inputs in=inputs(T,1);uint32_t h=s.add(make("sent",Condition::WhenRadioIdle),0,T,why);s.result(h,true,"",in);std::string blob=s.encode();
  Scheduler e;assert(e.decode("")==Status::Empty&&e.items.empty());
  {std::string bad=blob;bad[0]='X';Scheduler r;assert(r.decode(bad)==Status::BadHeader&&r.items.empty()&&r.history.empty());}
  {std::string bad=blob;bad[4]=char(Version+1);Scheduler r;assert(r.decode(bad)==Status::BadVersion&&r.items.empty());}
  {std::string bad=blob;bad[6]^=0x55;Scheduler r;assert(r.decode(bad)==Status::BadHeader);}// header checksum
  for(size_t cut=0;cut<blob.size();cut++){Scheduler r;Status st=r.decode(blob.substr(0,cut));assert(st!=Status::Ok);assert(r.items.size()<=5&&r.history.size()<=1);for(auto &i:r.items)assert(!i.text.empty()&&i.text.compare(0,15,"message number ")==0);}
  {Scheduler r;r.decode(blob.substr(0,blob.size()-3));assert(r.items.size()==5&&r.history.empty());}// the cut tail only costs the last record
  {std::string bad=blob;size_t at=blob.size()/2;bad[at]^=0x40;Scheduler r;Status st=r.decode(bad);assert(st==Status::Partial&&r.items.size()<5);}
  for(unsigned seed=1;seed<400;seed++){std::string bad=blob;unsigned x=seed*2654435761u;for(int k=0;k<4;k++){x=x*1664525u+1013904223u;bad[x%bad.size()]^=char(1+(x>>24)%255);}Scheduler r;r.decode(bad);assert(r.items.size()<=MaxPending&&r.history.size()<=MaxHistory);for(auto &i:r.items)assert(i.text.size()<=MaxTextBytes&&i.id);}
  std::string junk(3000,'\xAB');Scheduler r;assert(r.decode(junk)==Status::BadHeader);}
 // ---- Date editor (the Date / time page rules)
 {DateEdit d;d.set(31,1,2026,23,59);d.step(1,1);assert(d.f[0]==28&&d.f[1]==2);// February clamps the day
  d.step(4,1);assert(d.f[4]==0);d.step(4,-1);assert(d.f[4]==59);d.set(1,1,2026,0,0);d.step(1,-1);assert(d.f[1]==12);d.step(2,-1);assert(d.f[2]==2025);
  d.set(1,1,2026,0,0);assert(!d.digit(0,3)&&d.f[0]==3);assert(d.digit(0,1)&&d.f[0]==31);// two digits finish a field
  d.set(1,2,2027,0,0);assert(!d.digit(0,3));assert(d.digit(0,5)&&d.f[0]==28);
  d.set(1,1,2026,0,0);assert(!d.digit(2,2)&&!d.digit(2,0)&&!d.digit(2,3)&&d.digit(2,4)&&d.f[2]==2034);
  assert(DateEdit::daysIn(2,2028)==29&&DateEdit::daysIn(2,2100)==28&&DateEdit::daysIn(4,2026)==30);}
 // ---- Default time: 15 minutes ahead rounded up to five
 {assert(defaultAt(T)==((T+900+299)/300)*300&&defaultAt(T)>=T+900&&defaultAt(T)<T+900+300&&defaultAt(T)%300==0);}
 // ---- Condition names
 {assert(std::string(conditionName(Condition::AtTime))=="At time"&&std::string(conditionName(Condition::WhenRepeaterHeard))=="Repeater heard"&&std::string(conditionName(Condition::WhenRadioIdle))=="Radio idle"&&std::string(conditionName(Condition::WhenContactHeard))=="Contact heard");
  assert(nextCondition(Condition::AtTime,1,true)==Condition::WhenRepeaterHeard&&nextCondition(Condition::WhenRadioIdle,1,false)==Condition::AtTime&&nextCondition(Condition::AtTime,-1,true)==Condition::WhenContactHeard&&nextCondition(Condition::AtTime,-1,false)==Condition::WhenRadioIdle&&nextCondition(Condition::WhenRadioIdle,1,true)==Condition::WhenContactHeard);}
 std::cout<<"PASS: scheduler (conditions, clock-unset, gates, retry/back-off, target gone, missed while off, history ring, persistence round trip and corruption, date editor)\n";
}
