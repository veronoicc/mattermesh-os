#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <vector>
namespace matter {
namespace delivery {
// Timeline of one outgoing message: what happened to it and when (ms since it was queued). Pure model, no Arduino types.
enum class Kind : uint8_t {Queued,TxDone,HeardRepeater,AckReceived,NoAck,RetryFlood,Failed,ManualPath};
constexpr uint32_t Unknown=0xFFFFFFFFu;// time of a synthesized (legacy) event
constexpr size_t MaxEvents=12,MaxArg=24,MaxEncoded=1100;
struct Event {uint32_t at=0;Kind kind=Kind::Queued;std::string arg;Event(uint32_t a=0,Kind k=Kind::Queued,std::string s=std::string()):at(a),kind(k),arg(std::move(s)){}};// constructor: the firmware builds as C++11 (no aggregate with member initializers)
inline std::string relative(uint32_t ms){
 if(ms==Unknown)return "--";
 if(ms<1000)return "+0."+std::to_string(ms/100)+" s";
 if(ms<60000)return "+"+std::to_string(ms/1000)+" s";
 if(ms<3600000)return "+"+std::to_string(ms/60000)+" min";
 return "+"+std::to_string(ms/3600000)+" h";
}
// Short column marker of the Delivery view.
inline const char *marker(Kind k){switch(k){case Kind::Queued:return "QUE";case Kind::TxDone:return "TX";case Kind::HeardRepeater:return "RPT";case Kind::AckReceived:return "ACK";case Kind::NoAck:return "NO";case Kind::RetryFlood:return "RTY";case Kind::Failed:return "ERR";case Kind::ManualPath:return "PTH";}return "";}
// label: the repeater's name when one is known (HeardRepeater shows the hex prefix otherwise).
inline std::string describe(const Event &e,const std::string &label=""){
 auto with=[&](const char *head,const char *sep){return e.arg.empty()?std::string(head):std::string(head)+sep+e.arg;};
 switch(e.kind){
  case Kind::Queued:return "Queued";
  case Kind::TxDone:return e.arg.empty()?"Transmitted":"Transmitted ("+e.arg+")";
  case Kind::HeardRepeater:return "Heard by "+(label.empty()?"["+e.arg+"]":label);
  case Kind::AckReceived:return with("ACK received"," via ");
  case Kind::NoAck:return e.arg.empty()?"No ACK":"No ACK ("+e.arg+")";
  case Kind::RetryFlood:return "No ACK, retrying by flood";
  case Kind::Failed:return with("Failed",": ");
  case Kind::ManualPath:return with("Manual path",": ");
 }
 return "";
}
inline bool validPrefix(const std::string &p){if(p.size()<2||p.size()>8||p.size()%2)return false;for(char c:p)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;return true;}
inline std::string lower(std::string s){for(auto &c:s)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return s;}
struct Timeline {
 std::vector<Event> events;
 // false when ignored: a second Queued, a repeater without prefix or one already listed, a repeater with no room left.
 // Full: the oldest heard repeater goes first, then the oldest event after Queued.
 bool add(uint32_t at,Kind kind,const std::string &arg=""){
  if(kind==Kind::Queued&&!events.empty()&&events[0].kind==Kind::Queued)return false;
  if(kind==Kind::HeardRepeater){if(arg.empty())return false;for(auto &e:events)if(e.kind==kind&&e.arg==arg)return false;}
  if(events.size()>=MaxEvents){
   size_t drop=events.size();for(size_t i=0;i<events.size();i++)if(events[i].kind==Kind::HeardRepeater){drop=i;break;}
   if(drop==events.size()){if(kind==Kind::HeardRepeater)return false;drop=!events.empty()&&events[0].kind==Kind::Queued?1:0;}
   events.erase(events.begin()+drop);
  }
  events.push_back({at,kind,arg.substr(0,MaxArg)});return true;
 }
 int heard()const{int n=0;for(auto &e:events)n+=e.kind==Kind::HeardRepeater;return n;}
 std::string route()const{for(auto it=events.rbegin();it!=events.rend();++it)if(it->kind==Kind::AckReceived)return it->arg;return "";}
 // Hops of the route the ACK came back over: Direct 0, "N hop(s)" N, flood or unknown -1.
 int hops()const{auto r=route();if(r=="Direct")return 0;if(r.empty()||r[0]<'0'||r[0]>'9')return -1;return atoi(r.c_str());}
 std::string summary()const{
  if(events.empty())return "No history";
  auto repeaters=[&](int n){return std::to_string(n)+(n==1?" repeater":" repeaters");};
  for(auto &e:events)if(e.kind==Kind::AckReceived){int h=hops();return h==0?"Delivered direct":e.arg.empty()?"Delivered":h<0?"Delivered via "+lower(e.arg):"Delivered via "+e.arg;}
  Kind last=Kind::Queued;std::string arg;for(auto &e:events)if(e.kind!=Kind::HeardRepeater&&e.kind!=Kind::ManualPath){last=e.kind;arg=e.arg;}
  int n=heard();
  if(last==Kind::Failed)return arg.empty()?"Failed":"Failed: "+arg;
  if(last==Kind::NoAck)return n?"No ACK, heard by "+repeaters(n):"No ACK";
  if(last==Kind::RetryFlood)return "Retrying by flood";
  if(n)return "Heard by "+repeaters(n);
  return last==Kind::TxDone?"Transmitted":"Queued";
 }
 // One journal field: `ms,K,arg;` per event, arg percent-escaped (no tab, newline, ; , or %). A torn tail has no `;`.
 std::string encode()const{
  static const char kinds[]="QTHANRFM",hex[]="0123456789ABCDEF";std::string out;
  for(auto &e:events){out+=std::to_string(e.at)+","+kinds[int(e.kind)]+",";
   for(unsigned char c:e.arg){if(c<0x20||c==0x7f||c==';'||c==','||c=='%'){out+='%';out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}out+=';';}
  return out;
 }
 // Anything unreadable is skipped; never throws.
 static Timeline decode(const std::string &text){
  Timeline t;if(text.size()>MaxEncoded)return t;
  size_t start=0,end;
  while(t.events.size()<MaxEvents&&(end=text.find(';',start))!=std::string::npos){
   std::string entry=text.substr(start,end-start);start=end+1;
   size_t c1=entry.find(','),c2=c1==std::string::npos?c1:entry.find(',',c1+1);
   if(c2==std::string::npos||c1==0||c1>10||c2!=c1+2)continue;
   uint64_t at=0;bool ok=true;for(size_t i=0;i<c1;i++){if(entry[i]<'0'||entry[i]>'9'){ok=false;break;}at=at*10+uint64_t(entry[i]-'0');}
   if(!ok||at>=Unknown)continue;
   const char *kinds="QTHANRFM",*at_=nullptr;for(const char *p=kinds;*p;p++)if(*p==entry[c1+1])at_=p;
   if(!at_)continue;
   std::string arg;for(size_t i=c2+1;i<entry.size()&&ok;i++){
    unsigned char c=entry[i];
    if(c=='%'){auto digit=[](char h){return h>='0'&&h<='9'?h-'0':h>='A'&&h<='F'?h-'A'+10:h>='a'&&h<='f'?h-'a'+10:-1;};int hi=i+2<entry.size()?digit(entry[i+1]):-1,lo=hi<0?-1:digit(entry[i+2]);if(lo<0)ok=false;else{arg+=char(hi*16+lo);i+=2;}}
    else arg+=char(c);}
   if(ok)t.add(uint32_t(at),Kind(at_-kinds),arg);
  }
  return t;
 }
 // A message from before timelines: its stored status line and heard prefixes (comma separated), times unknown.
 static Timeline legacy(const std::string &status,int,const std::string &heardCsv){
  Timeline t;t.add(Unknown,Kind::Queued);
  std::string s=lower(status),head=status.substr(0,status.find(':')==std::string::npos?0:status.find(':'));
  auto has=[&](const char *w){return s.find(w)!=std::string::npos;};
  if(has("delivered"))t.add(Unknown,Kind::AckReceived,head);
  else if(has("retrying"))t.add(Unknown,Kind::RetryFlood);
  else if(has("no ack"))t.add(Unknown,Kind::NoAck,head);
  else if(has("not sent")||has("failed")||has("not confirmed"))t.add(Unknown,Kind::Failed,status);
  else if(has("sent"))t.add(Unknown,Kind::TxDone);
  size_t start=0;while(start<=heardCsv.size()){size_t comma=heardCsv.find(',',start);if(comma==std::string::npos)comma=heardCsv.size();auto p=heardCsv.substr(start,comma-start);if(validPrefix(p))t.add(Unknown,Kind::HeardRepeater,lower(p));start=comma+1;}
  return t;
 }
};
}
}
