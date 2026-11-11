#include <cassert>
#include <iostream>
#include "ui/sms_page.h"
using namespace mm;
int main(){
 // Row paging: pages of SmsRows, j/k clamp, pager keeps the offset inside the page, the last page lands on the last row.
 assert(smsPageStart(0,5)==0&&smsPageStart(4,5)==0&&smsPageStart(5,5)==5&&smsPageStart(-1,5)==0);
 assert(smsMove(0,-1,12)==0&&smsMove(0,1,12)==1&&smsMove(11,1,12)==11&&smsMove(3,5,0)==0);
 assert(smsPageJump(2,1,5,12)==7&&smsPageJump(7,1,5,12)==11&&smsPageJump(11,1,5,12)==11&&smsPageJump(7,-1,5,12)==2&&smsPageJump(2,-1,5,12)==2&&smsPageJump(0,1,5,0)==0);
 assert(smsPageCount(0,5)==1&&smsPageCount(5,5)==1&&smsPageCount(6,5)==2&&smsPageCount(12,5)==3);
 assert(smsRange(0,5,12)=="1-5 of 12"&&smsRange(10,5,12)=="11-12 of 12"&&smsRange(0,5,0)=="");
 // Preview: one line, whitespace collapsed, cut on a UTF-8 boundary with "...".
 assert(smsPreview("hello",20)=="hello"&&smsPreview("  a\r\n b\t c  ",20)=="a b c");
 assert(smsPreview("abcdefghij",8)=="abcde...");
 {std::string p=smsPreview("ciao perché è così",9);assert(p.size()<=9&&p.substr(p.size()-3)=="...");assert(smsPreview("àààààààà",7)=="àà...");}
 {std::string p=smsPreview("你好世界你好世界",11);assert(p=="你好...");p=smsPreview("😀😀😀😀😀",9);assert(p=="😀...");}
 assert(smsPreview("",10).empty()&&smsPreview("   ",10).empty());
 // Mode transitions: Back leaves Confirm for Read, Read and Compose for the inbox, and is not consumed by the inbox.
 {SmsMode m=SmsMode::Confirm;assert(smsBack(m)&&m==SmsMode::Read);assert(smsBack(m)&&m==SmsMode::Inbox);m=SmsMode::Compose;assert(smsBack(m)&&m==SmsMode::Inbox);assert(!smsBack(m)&&m==SmsMode::Inbox);}
 // Compose focus cycle: number -> text -> SEND -> CANCEL -> number (and back); the first two are editors.
 assert(smsFieldNext(0,1)==1&&smsFieldNext(1,1)==2&&smsFieldNext(2,1)==3&&smsFieldNext(3,1)==0&&smsFieldNext(0,-1)==3&&smsFieldNext(2,-1)==1);
 assert(smsFieldEditing(0)&&smsFieldEditing(1)&&!smsFieldEditing(2)&&!smsFieldEditing(3));
 // Draft validity and counter.
 assert(smsCanSend(true,5,160,false)&&!smsCanSend(false,5,160,false)&&!smsCanSend(true,0,160,false)&&!smsCanSend(true,161,160,false)&&smsCanSend(true,160,160,false)&&!smsCanSend(true,5,160,true));
 assert(smsCounter(0,160)=="0/160"&&smsCounter(71,70)=="71/70");
 // Number field: digits, a leading '+', at most 20 characters.
 assert(smsNumberAccepts("",'+')&&!smsNumberAccepts("1",'+')&&smsNumberAccepts("",'3')&&!smsNumberAccepts("12",'a')&&!smsNumberAccepts("12",' ')&&!smsNumberAccepts(std::string(20,'1'),'1')&&smsNumberAccepts(std::string(19,'1'),'1'));
 // Text append stops at the limit, never splits a code point, drops control characters.
 {auto units=[](const std::string &s){return s.size();};auto limit=[](const std::string&){return size_t(10);};
  assert(smsAppendFit("abc","def\nghi",units,limit)=="abcdefghi");assert(smsAppendFit("abcdefghi","jklm",units,limit)=="abcdefghij");
  assert(smsAppendFit("abcdefghi","é",units,limit)=="abcdefghi");assert(smsAppendFit("ab","é😀",units,limit)=="abé😀"&&smsAppendFit("abcdef","é😀",units,limit)=="abcdefé");
  assert(smsAppendFit("a","\x01\x7f\n\r",units,limit)=="a");assert(smsAppendFit("a","\xff\xc3",units,limit)=="a");}
 // The limit may change with the text (GSM-7 -> UCS-2): the candidate is judged by its own limit.
 {auto units=[](const std::string &s){return s.size();};auto limit=[](const std::string &s){return s.find("é")!=std::string::npos?size_t(4):size_t(10);};
  assert(smsAppendFit("abcde","é",units,limit)=="abcde"&&smsAppendFit("a","é",units,limit)=="aé");}
 // Read view scrolling by lines.
 assert(smsScroll(0,1,10,4)==1&&smsScroll(5,1,10,4)==6&&smsScroll(6,1,10,4)==6&&smsScroll(0,-1,10,4)==0&&smsScroll(2,-4,10,4)==0&&smsScroll(0,3,2,4)==0&&smsScroll(9,0,10,4)==6);
 // What the inbox says for every modem phase: only a registered modem has nothing to say.
 for(int p=0;p<=int(device::ModemPhase::Failed);p++){auto t=smsStatusText(device::ModemPhase(p));if(device::ModemPhase(p)==device::ModemPhase::Registered)assert(!*t.title&&!*t.hint);else assert(*t.title&&*t.hint);}
 assert(std::string(smsStatusText(device::ModemPhase::Off).hint).find("Settings > Cellular")!=std::string::npos);
 std::cout<<"PASS: sms page logic (paging, UTF-8 previews, mode transitions, field cycle, canSend/counter, number filter, limit-aware append, scrolling, modem status texts)\n";
}
