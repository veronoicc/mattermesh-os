#pragma once
#include <string>
#include <algorithm>
#include "sms_layout.h"
#include "utf8.h"
#include "services/modem_status.h"
// Pure logic of the SMS app (no Arduino, no modem service): paging, previews, mode changes, the compose fields.
namespace mm {
inline int smsPageStart(int focus,int rows){return focus<0?0:focus/rows*rows;}
inline int smsMove(int focus,int delta,int total){return total<=0?0:std::max(0,std::min(total-1,focus+delta));}
inline int smsPageCount(int total,int rows){return std::max(1,(total+rows-1)/rows);}
// The pager: same place on the next / previous page (the last page lands on its last row); no such page: stay.
inline int smsPageJump(int focus,int dir,int rows,int total){
 if(total<=0)return 0;int page=focus/rows+dir;if(page<0||page>=smsPageCount(total,rows))return focus;return std::min(total-1,page*rows+focus%rows);}
inline std::string smsRange(int start,int rows,int total){if(total<=0)return "";return std::to_string(start+1)+"-"+std::to_string(std::min(total,start+rows))+" of "+std::to_string(total);}
// One line: whitespace runs collapsed to one space, cut on a code point boundary to at most maxBytes with "...".
inline std::string smsPreview(const std::string &text,size_t maxBytes){
 std::string out;bool gap=false;for(char c:text){if(c==' '||c=='\t'||c=='\r'||c=='\n'){gap=!out.empty();continue;}if(gap)out+=' ';gap=false;out+=c;}
 if(out.size()<=maxBytes)return out;size_t keep=maxBytes>3?utf8Floor(out,maxBytes-3):0;return out.substr(0,keep)+"...";}
// Back: Confirm -> message, message / compose -> inbox; false at the inbox (the app closes).
inline bool smsBack(SmsMode &m){if(m==SmsMode::Inbox)return false;m=m==SmsMode::Confirm?SmsMode::Read:SmsMode::Inbox;return true;}
// Compose focus: number 0, text 1, SEND 2, CANCEL 3; the first two take typing.
inline int smsFieldNext(int field,int dir){return (field+(dir<0?3:1))%4;}
inline bool smsFieldEditing(int field){return field==0||field==1;}
inline bool smsCanSend(bool numberOk,size_t units,size_t limit,bool sending){return !sending&&numberOk&&units>0&&units<=limit;}
inline std::string smsCounter(size_t units,size_t limit){return std::to_string(units)+"/"+std::to_string(limit);}
// Digits, a leading '+', at most 20 characters; modem_at::validSmsNumber has the last word.
inline bool smsNumberAccepts(const std::string &cur,char c){return cur.size()<20&&((c>='0'&&c<='9')||(c=='+'&&cur.empty()));}
// Appends whole code points while the text stays within its own limit (the limit drops when the encoding goes from GSM-7
// to UCS-2); control characters and broken UTF-8 are dropped. units / limit: size_t(const std::string&).
template<class U,class L> inline std::string smsAppendFit(std::string cur,const std::string &add,U units,L limit){
 uint32_t cp;for(size_t at=0;at<add.size();){size_t next=utf8Next(add,at,cp);
  if(cp<32||cp==127||(cp=='?'&&add[at]!='?')){at=next;continue;}
  std::string grown=cur+add.substr(at,next-at);if(units(grown)>limit(grown))break;cur=grown;at=next;}
 return cur;}
// First visible line of the message text after moving by delta lines.
inline int smsScroll(int scroll,int delta,int lines,int visible){return std::max(0,std::min(std::max(0,lines-visible),scroll+delta));}
// What the inbox says while the modem cannot receive: a title and a hint ("" / "" when registered).
struct SmsStatusText {const char *title,*hint;};
inline SmsStatusText smsStatusText(device::ModemPhase p){using P=device::ModemPhase;switch(p){
 case P::Off:return {"Modem is off","Turn the modem on in Settings > Cellular"};
 case P::Powering:case P::Starting:return {"Starting the modem","Messages appear once it is ready"};
 case P::NoSim:return {"No SIM card","Insert a SIM card, then restart the modem"};
 case P::PinNeeded:return {"SIM PIN needed","Enter the PIN in Settings > Cellular"};
 case P::PukNeeded:return {"SIM locked (PUK)","Unlock the SIM in a phone"};
 case P::Searching:return {"Searching network","Messages arrive once registered"};
 case P::Registered:return {"",""};
 case P::Denied:return {"Registration denied","Check the SIM and the network"};
 default:return {"Modem not responding","Turn it off and on in Settings > Cellular"};}}
}
