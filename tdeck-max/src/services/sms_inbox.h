#pragma once
#include <stdint.h>
#include <algorithm>
#include <string>
#include <vector>
#include "services/modem_at.h"
#include "services/modem_status.h"
// RAM view of the SIM's messages. The module marks a message read as soon as AT+CMGL lists it (the A76xx has no peek), so
// "unread" is kept here: a message the module listed as unread is remembered by a hash of its number, time and text until
// the user opens it. The hashes live in NVS (services/modem.cpp) so unread survives a reboot.
namespace device {
constexpr size_t SmsUnreadMax=64;
inline uint32_t smsHash(const std::string &number,const std::string &scts,const std::string &text){uint32_t h=2166136261u;for(const std::string *s:{&number,&scts,&text}){for(unsigned char c:*s){h^=c;h*=16777619u;}h^=0xFF;h*=16777619u;}return h?h:1;}
// yymmddhhmmss of "26/11/11,10:20:30+04" (0 when malformed), so newest messages sort first whatever their SIM slot.
inline uint64_t smsSortKey(const std::string &scts){if(modem_at::smsTimeLabel(scts).empty())return 0;std::string d;for(size_t i=0;i<17;i++)if(scts[i]>='0'&&scts[i]<='9')d+=scts[i];return d.size()==12?std::stoull(d):0;}
class SmsInbox {
public:
 std::vector<SmsMessage> list;// newest first
 std::vector<uint32_t> unreadHashes;// persisted by the caller whenever it changes
 // Merge a whole AT+CMGL listing. Returns the messages that arrived unread since the last merge (empty on the first listing
 // after boot: messages that came while the device was off stay unread but are not announced).
 std::vector<SmsMessage> merge(const std::vector<modem_at::ParsedSms> &parsed){
  struct Row {SmsMessage m;uint32_t hash;uint64_t key;};std::vector<Row> rows;std::vector<SmsMessage> fresh;
  for(auto &p:parsed){Row r;r.hash=smsHash(p.number,p.scts,p.text);r.key=smsSortKey(p.scts);r.m.index=p.index;r.m.number=p.number;r.m.text=p.text;r.m.time=modem_at::smsTimeLabel(p.scts);
   bool isNew=p.unread&&!has(r.hash);if(isNew)unreadHashes.push_back(r.hash);r.m.unread=has(r.hash);if(isNew&&primed)fresh.push_back(r.m);rows.push_back(r);}
  std::stable_sort(rows.begin(),rows.end(),[](const Row &a,const Row &b){return a.key!=b.key?a.key>b.key:a.m.index>b.m.index;});
  list.clear();hashes.clear();std::vector<uint32_t> keep;
  for(auto &r:rows){list.push_back(r.m);hashes.push_back(r.hash);if(r.m.unread&&std::find(keep.begin(),keep.end(),r.hash)==keep.end())keep.push_back(r.hash);}
  while(keep.size()>SmsUnreadMax)keep.pop_back();unreadHashes=keep;primed=true;return fresh;}
 // The user opened message `index`: true when it was unread (the caller saves the hashes).
 bool markRead(int index){for(size_t i=0;i<list.size();i++)if(list[i].index==index){if(!list[i].unread)return false;list[i].unread=false;drop(hashes[i]);return true;}return false;}
 void remove(int index){for(size_t i=0;i<list.size();i++)if(list[i].index==index){drop(hashes[i]);list.erase(list.begin()+long(i));hashes.erase(hashes.begin()+long(i));return;}}
 int unreadCount()const{int n=0;for(auto &m:list)if(m.unread)n++;return n;}
private:
 bool primed=false;std::vector<uint32_t> hashes;// per list row
 bool has(uint32_t h)const{return std::find(unreadHashes.begin(),unreadHashes.end(),h)!=unreadHashes.end();}
 void drop(uint32_t h){unreadHashes.erase(std::remove(unreadHashes.begin(),unreadHashes.end(),h),unreadHashes.end());}
};
}
