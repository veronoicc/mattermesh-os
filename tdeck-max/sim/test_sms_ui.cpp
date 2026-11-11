#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
using namespace mm;
static std::vector<uint8_t> shot(Renderer &r,GFXcanvas1 &canvas,const State &s){r.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+(W+7)/8*H);}
static SmsRow row(const std::string &n,const std::string &t,const std::string &p,bool unread){SmsRow r;r.number=n;r.time=t;r.preview=p;r.unread=unread;return r;}
int main(){
 GFXcanvas1 canvas(W,H);Renderer renderer(canvas);State s;s.page=Page::Sms;
 auto inside=[](Rect r){return r.x>=Margin&&r.x+r.w<=W-Margin&&r.y>=Top&&r.y+r.h<=Bottom;};
 auto apart=[](Rect a,Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 // Touch targets of every mode: inside the content area (the header button inside the header), none overlapping.
 {std::vector<Rect> inbox,read,confirm,compose;
  assert(ComposeButton.y>=StatusH&&ComposeButton.y+ComposeButton.h<=Top&&ComposeButton.x+ComposeButton.w<=W);
  for(int i=0;i<SmsRows;i++)inbox.push_back(smsRow(i));inbox.push_back(SmsPrev);inbox.push_back(SmsNext);
  for(int i=0;i<3;i++)read.push_back(smsReadButton(i));read.push_back(SmsReadUp);read.push_back(SmsReadDown);read.push_back(SmsReadHead);
  confirm={SmsConfirmYes,SmsConfirmNo};compose={SmsNumber,smsTextRect(SmsTextMaxLines),SmsSend,SmsCancel};
  for(auto *page:{&inbox,&read,&confirm,&compose})for(size_t i=0;i<page->size();i++){assert(inside((*page)[i]));for(size_t j=i+1;j<page->size();j++)assert(apart((*page)[i],(*page)[j]));}
  assert(inside(SmsConfirmCard)&&SmsConfirmYes.y>=SmsConfirmCard.y&&SmsConfirmYes.y+SmsConfirmYes.h<=SmsConfirmCard.y+SmsConfirmCard.h&&SmsConfirmNo.y+SmsConfirmNo.h<=SmsConfirmCard.y+SmsConfirmCard.h);
  for(int l=1;l<=SmsTextMaxLines;l++){Rect t=smsTextRect(l);assert(inside(t)&&apart(t,SmsNumber)&&t.y+t.h<=SmsCounterY&&t.y+t.h<=SmsSend.y);}
  assert(SmsCounterY+14<=SmsResultY&&SmsResultY+14<=SmsSend.y);
  assert(SmsReadBody.y>=SmsReadHead.y+SmsReadHead.h&&SmsReadBody.y+SmsReadBody.h<=smsReadButton(0).y&&SmsReadUp.y+SmsReadUp.h==SmsReadDown.y);
  assert(SmsListY+SmsRows*SmsRowH<=SmsPagerY);}
 // Inbox: empty (every modem phase text), unread vs read, paging, long Unicode content.
 auto &v=s.sms;v.mode=SmsMode::Inbox;
 auto empty=shot(renderer,canvas,s);v.status="Modem is off";v.hint="Turn the modem on in Settings > Cellular";auto off=shot(renderer,canvas,s);assert(empty!=off);
 v.status="No SIM card";v.hint="Insert a SIM card, then restart the modem";assert(off!=shot(renderer,canvas,s));v.status="";v.hint="";
 v.rows={row("+393331234567","12/11 09:41","Ciao, ci vediamo alle otto?",false),row("+39 long number "+std::string(60,'9'),"12/11 09:40",std::string(200,'x'),true),row("你好世界你好世界你好世界","11/11 23:59","😀😀😀 perché è così, già più",true),row("","",std::string("à"),false),row("Bank","01/01 00:00","Code 123456",false)};v.total=5;v.focus=0;
 auto read=shot(renderer,canvas,s);assert(read!=empty);
 v.rows[0].unread=true;auto unread=shot(renderer,canvas,s);assert(unread!=read);v.rows[0].unread=false;
 v.focus=2;assert(shot(renderer,canvas,s)!=read);v.focus=0;
 v.total=12;auto paged=shot(renderer,canvas,s);assert(paged!=read);v.start=5;v.focus=5;auto second=shot(renderer,canvas,s);assert(second!=paged);v.start=0;v.total=5;
 v.unread=3;assert(shot(renderer,canvas,s)!=read);v.unread=0;
 s.notice="Message sent";assert(shot(renderer,canvas,s)!=read);s.notice="";
 // Message view: text, scrolling, button focus, the delete dialog.
 v.mode=SmsMode::Read;v.number="+393331234567";v.time="12/11 09:41";v.text="Hello there";v.readFocus=0;
 auto msg=shot(renderer,canvas,s);assert(msg!=read);
 v.readFocus=1;auto del=shot(renderer,canvas,s);assert(del!=msg);v.readFocus=2;assert(shot(renderer,canvas,s)!=del);v.readFocus=0;
 v.text="";for(int i=0;i<40;i++)v.text+="riga "+std::to_string(i)+" perché è così 你好 😀 ";
 auto longText=shot(renderer,canvas,s);assert(longText!=msg);v.scroll=3;auto scrolled=shot(renderer,canvas,s);assert(scrolled!=longText);v.scroll=100;shot(renderer,canvas,s);v.scroll=0;
 v.number=std::string(60,'7');v.time="";shot(renderer,canvas,s);v.number="+393331234567";v.time="12/11 09:41";
 v.mode=SmsMode::Confirm;auto confirm=shot(renderer,canvas,s);assert(confirm!=longText);
 // Compose: empty, focus on each field, typed text growing, valid vs invalid SEND, counter, sending / result.
 v=SmsView();v.mode=SmsMode::Compose;v.counter="0/160";
 auto fresh=shot(renderer,canvas,s);assert(fresh!=msg);
 v.field=1;auto textFocus=shot(renderer,canvas,s);assert(textFocus!=fresh);v.field=2;auto sendFocus=shot(renderer,canvas,s);assert(sendFocus!=textFocus);v.field=3;auto cancelFocus=shot(renderer,canvas,s);assert(cancelFocus!=sendFocus);v.field=0;
 v.to="+39333";auto typed=shot(renderer,canvas,s);assert(typed!=fresh);v.numberBad=true;assert(shot(renderer,canvas,s)!=typed);v.numberBad=false;
 v.to="+393331234567";v.body="Ciao!";v.counter="5/160";auto oneLine=shot(renderer,canvas,s);assert(oneLine!=typed);
 v.canSend=true;auto ready=shot(renderer,canvas,s);assert(ready!=oneLine);
 v.field=1;v.body=std::string("Questo messaggio è lungo e va a capo più volte per provare il campo di testo che cresce con le righe. ")+std::string(200,'z');v.counter="300/160";
 auto many=shot(renderer,canvas,s);assert(many!=ready);v.over=true;assert(shot(renderer,canvas,s)!=many);v.over=false;
 v.body="你好世界😀 perché è così, ";for(int i=0;i<6;i++)v.body+=v.body;v.counter="70/70";shot(renderer,canvas,s);
 v.sending=true;v.canSend=false;auto sending=shot(renderer,canvas,s);v.sending=false;auto idle=shot(renderer,canvas,s);assert(sending!=idle);
 v.result="Network error";assert(shot(renderer,canvas,s)!=idle);
 v.to=std::string(60,'1');v.result=std::string(80,'r');shot(renderer,canvas,s);
 // Other pages are unaffected.
 s.page=Page::Home;renderer.draw(s);
 std::cout<<"PASS: sms ui (touch targets inside the content area and apart in every mode, inbox/read/confirm/compose renders change with state, long and Unicode text)\n";
}
