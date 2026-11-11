// SMS app (52): inbox, message view, compose. Pages: ui/sms_layout.h / ui/sms_page.h / ui/sms_renderer.inc.
// Included by main.cpp after radio_link_app.inc. The SIM holds the messages; the modem service keeps a RAM copy (smsInbox()),
// copied here once per change of its revision, never per frame. smsBannerFrom / smsBannerBody / smsBannerUntil are declared in main.cpp.
#include "services/modem.h"
#include "services/modem_at.h"
#include "ui/sms_layout.h"
#include "ui/sms_page.h"
static mm::SmsMode smsMode=mm::SmsMode::Inbox;static std::vector<device::SmsMessage> smsCache;
static int smsFocus=0,smsReadFocus=0,smsScrollAt=0,smsReadRows=0,smsComposeField=0,smsOpenAt=-1;
static device::SmsMessage smsOpen;static String smsTo,smsBody;static std::string smsResult;static bool smsSending=false,smsSawSending=false;static uint32_t smsSentAt=0;
static uint32_t smsSeenRevision=0,smsSeenCount=0,smsSeenUnread=0;static bool smsCacheValid=false;
static bool smsPage(){return screen==Screen::Section&&section==mm::SmsSection;}
static int smsTotal(){return int(smsCache.size());}
static int smsUnreadLocal(){int n=0;for(auto &m:smsCache)if(m.unread)n++;return n;}
// Copies the inbox when the modem service says something changed (or when forced); true when the rows changed.
static bool smsRefreshCache(bool force){
 auto st=device::modemStatus();if(!force&&smsCacheValid&&st.revision==smsSeenRevision&&uint32_t(st.smsCount)==smsSeenCount&&uint32_t(st.smsUnread)==smsSeenUnread)return false;
 smsSeenRevision=st.revision;smsSeenCount=st.smsCount;smsSeenUnread=st.smsUnread;smsCacheValid=true;smsCache=device::smsInbox();smsFocus=mm::smsMove(smsFocus,0,smsTotal());return true;}
static void smsToInbox(){smsMode=mm::SmsMode::Inbox;smsRefreshCache(false);}
static void smsOpenMessage(int i){
 if(i<0||i>=smsTotal())return;smsFocus=i;smsOpen=smsCache[i];smsOpenAt=i;smsMode=mm::SmsMode::Read;smsReadFocus=0;smsScrollAt=0;
 mm::Components c(frame);smsReadRows=int(c.lines(smsOpen.text,mm::W-2*mm::Margin-16,mm::Font::ChatBody).size());
 if(smsOpen.unread){device::smsMarkRead(smsOpen.index);smsCache[i].unread=false;}render();}
static void smsStartCompose(){smsMode=mm::SmsMode::Compose;smsComposeField=smsTo.length()?1:0;smsResult="";render();}
static void smsReply(){smsTo=smsOpen.number.c_str();smsBody="";smsStartCompose();}
static void smsDeleteOpen(){device::smsDelete(smsOpen.index);if(smsOpenAt>=0&&smsOpenAt<smsTotal()&&smsCache[smsOpenAt].index==smsOpen.index)smsCache.erase(smsCache.begin()+smsOpenAt);smsOpenAt=-1;smsFocus=mm::smsMove(smsFocus,0,smsTotal());smsMode=mm::SmsMode::Inbox;notice="Message deleted";render();}
static bool smsDraftOk(){return modem_at::validSmsNumber(std::string(smsTo.c_str()));}
static void smsSendNow(){
 std::string body(smsBody.c_str());if(!mm::smsCanSend(smsDraftOk(),modem_at::smsUnits(body),modem_at::smsLimit(body),smsSending)){render();return;}
 std::string reason;smsResult="";if(device::smsSend(std::string(smsTo.c_str()),body,reason)){smsSending=true;smsSawSending=false;smsSentAt=millis();}else smsResult=reason.empty()?"Not sent":reason;render();}
static void smsCancelCompose(){smsTo="";smsBody="";smsResult="";smsToInbox();render();}
void smsCompose(mm::State &v){
 mm::SmsView &w=v.sms;w=mm::SmsView();w.mode=smsMode;smsRefreshCache(false);
 if(smsMode==mm::SmsMode::Inbox){
  w.total=smsTotal();w.focus=smsFocus;w.start=mm::smsPageStart(smsFocus,mm::SmsRows);w.unread=smsUnreadLocal();
  for(int i=w.start;i<w.total&&i<w.start+mm::SmsRows;i++){mm::SmsRow r;r.number=smsCache[i].number;r.time=smsCache[i].time;r.preview=mm::smsPreview(smsCache[i].text,80);r.unread=smsCache[i].unread;w.rows.push_back(r);}
  auto phase=device::modemStatus().phase;if(phase!=device::ModemPhase::Registered){auto t=mm::smsStatusText(phase);w.status=t.title;w.hint=t.hint;}
  return;}
 if(smsMode==mm::SmsMode::Read||smsMode==mm::SmsMode::Confirm){w.number=smsOpen.number;w.time=smsOpen.time;w.text=smsOpen.text;w.scroll=smsScrollAt;w.readFocus=smsReadFocus;return;}
 std::string to(smsTo.c_str()),body(smsBody.c_str());size_t units=modem_at::smsUnits(body),limit=modem_at::smsLimit(body);
 w.to=to;w.body=body;w.field=smsComposeField;w.counter=mm::smsCounter(units,limit);w.over=units>limit;w.sending=smsSending;w.result=smsResult;
 w.canSend=mm::smsCanSend(modem_at::validSmsNumber(to),units,limit,smsSending);w.numberBad=!to.empty()&&!modem_at::validSmsNumber(to)&&smsComposeField!=0;}
String *smsField(){if(!smsPage()||smsMode!=mm::SmsMode::Compose||!mm::smsFieldEditing(smsComposeField))return nullptr;return smsComposeField==0?&smsTo:&smsBody;}
bool smsTyping(){return smsPage()&&smsMode==mm::SmsMode::Compose&&smsComposeField==1;}
// Dictated / accented text (typeText): whole code points while the message fits its limit.
void smsAppend(const std::string &utf8){
 if(!smsTyping())return;std::string grown=mm::smsAppendFit(std::string(smsBody.c_str()),utf8,modem_at::smsUnits,modem_at::smsLimit);
 if(grown!=smsBody.c_str()){smsBody=grown.c_str();smsResult="";repaintSettled();}}
bool smsBack(){
 if(!smsPage())return false;
 if(!mm::smsBack(smsMode))return false;
 if(smsMode==mm::SmsMode::Inbox)smsRefreshCache(false);notice="";render();return true;}
void smsOpened(){device::smsRefresh();smsMode=mm::SmsMode::Inbox;smsFocus=0;smsResult="";smsRefreshCache(true);}
void smsLeft(){smsMode=mm::SmsMode::Inbox;smsReadFocus=0;smsScrollAt=0;}
void smsLoop(){
 device::SmsMessage m;
 while(device::smsTakeIncoming(m)){
  smsBannerFrom=m.number.empty()?"Unknown sender":m.number.c_str();smsBannerBody=mm::smsPreview(m.text,120).c_str();smsBannerUntil=millis()+6000;
  onIncomingMessage(false);markRadioDirty();}
 if(smsSending){auto st=device::smsSendState();
  if(st.phase==device::SmsSendPhase::Sending)smsSawSending=true;/* a result older than the send itself is not ours: wait for Sending, or 1.5 s for a send that finished within one loop */
  else if(!smsSawSending&&millis()-smsSentAt>=1500)smsSawSending=true;
  else if(smsSawSending&&st.phase==device::SmsSendPhase::Sent){smsSending=false;smsTo="";smsBody="";smsResult="";if(smsPage()){smsMode=mm::SmsMode::Inbox;notice="Message sent";}smsRefreshCache(true);markRadioDirty();}
  else if(smsSawSending&&st.phase==device::SmsSendPhase::Failed){smsSending=false;smsResult=st.message.empty()?"Sending failed":st.message;if(smsPage())markRadioDirty();}
  else if(st.phase==device::SmsSendPhase::Idle&&smsSawSending){smsSending=false;if(smsPage())markRadioDirty();}}
 if(smsPage()&&smsMode==mm::SmsMode::Inbox&&smsRefreshCache(false))markRadioDirty();}
bool smsKey(char c){
 if(c=='\b'||!smsPage())return false;
 if(smsMode==mm::SmsMode::Inbox){
  if(c=='j'||c=='s'||c=='l'){smsFocus=mm::smsMove(smsFocus,1,smsTotal());render();return true;}if(c=='k'||c=='w'||c=='h'){smsFocus=mm::smsMove(smsFocus,-1,smsTotal());render();return true;}
  if(c==']'||c=='.'){smsFocus=mm::smsPageJump(smsFocus,1,mm::SmsRows,smsTotal());render();return true;}if(c=='['||c==','){smsFocus=mm::smsPageJump(smsFocus,-1,mm::SmsRows,smsTotal());render();return true;}
  if(c=='n'||c=='c'){smsStartCompose();return true;}
  if(c=='\n'||c==' '){smsOpenMessage(smsFocus);return true;}
  return false;}
 if(smsMode==mm::SmsMode::Confirm){if(c=='\n')smsDeleteOpen();return c!='a';}
 if(smsMode==mm::SmsMode::Read){
  if(c=='j'||c=='s'){smsScrollAt=mm::smsScroll(smsScrollAt,1,smsReadRows,mm::SmsReadLines);render();return true;}if(c=='k'||c=='w'){smsScrollAt=mm::smsScroll(smsScrollAt,-1,smsReadRows,mm::SmsReadLines);render();return true;}
  if(c=='l'||c=='\t'){smsReadFocus=(smsReadFocus+1)%3;render();return true;}if(c=='h'){smsReadFocus=(smsReadFocus+2)%3;render();return true;}
  if(c=='r'){smsReply();return true;}if(c=='x'){smsMode=mm::SmsMode::Confirm;render();return true;}
  if(c=='\n'||c==' '){if(smsReadFocus==0)smsReply();else if(smsReadFocus==1){smsMode=mm::SmsMode::Confirm;render();}else{smsMode=mm::SmsMode::Inbox;render();}return true;}
  return false;}
 // Compose: the first two fields take typing; TAB cycles number -> text -> SEND -> CANCEL.
 if(c=='\t'){smsComposeField=mm::smsFieldNext(smsComposeField,1);render();return true;}
 if(mm::smsFieldEditing(smsComposeField)){
  if(c=='\n'){if(smsComposeField==0){smsComposeField=1;render();}return true;}
  if(c>=32&&c<=126){if(smsComposeField==0){if(mm::smsNumberAccepts(std::string(smsTo.c_str()),c)){smsTo+=c;smsResult="";repaintSettled();}}else smsAppend(std::string(1,c));}
  return true;}
 if(c=='j'||c=='s'||c=='l'){smsComposeField=mm::smsFieldNext(smsComposeField,1);render();return true;}if(c=='k'||c=='w'||c=='h'){smsComposeField=mm::smsFieldNext(smsComposeField,-1);render();return true;}
 if(c=='\n'||c==' '){if(smsComposeField==2)smsSendNow();else smsCancelCompose();return true;}
 return false;}
bool smsTap(int x,int y){
 if(!smsPage())return false;if(mm::BackButton.contains(x,y))return false;
 if(smsMode==mm::SmsMode::Inbox){
  if(mm::ComposeButton.contains(x,y)){smsStartCompose();return true;}
  int start=mm::smsPageStart(smsFocus,mm::SmsRows);
  if(start>0&&mm::SmsPrev.contains(x,y)){smsFocus=mm::smsPageJump(smsFocus,-1,mm::SmsRows,smsTotal());render();return true;}
  if(start+mm::SmsRows<smsTotal()&&mm::SmsNext.contains(x,y)){smsFocus=mm::smsPageJump(smsFocus,1,mm::SmsRows,smsTotal());render();return true;}
  for(int i=0;i<mm::SmsRows&&start+i<smsTotal();i++)if(mm::smsRow(i).contains(x,y)){smsOpenMessage(start+i);return true;}
  return true;}
 if(smsMode==mm::SmsMode::Confirm){if(mm::SmsConfirmYes.contains(x,y))smsDeleteOpen();else if(mm::SmsConfirmNo.contains(x,y)){smsMode=mm::SmsMode::Read;render();}return true;}
 if(smsMode==mm::SmsMode::Read){
  for(int i=0;i<3;i++)if(mm::smsReadButton(i).contains(x,y)){smsReadFocus=i;if(i==0)smsReply();else if(i==1){smsMode=mm::SmsMode::Confirm;render();}else{smsMode=mm::SmsMode::Inbox;render();}return true;}
  if(mm::SmsReadUp.contains(x,y)){smsScrollAt=mm::smsScroll(smsScrollAt,-3,smsReadRows,mm::SmsReadLines);render();return true;}
  if(mm::SmsReadDown.contains(x,y)){smsScrollAt=mm::smsScroll(smsScrollAt,3,smsReadRows,mm::SmsReadLines);render();return true;}
  return true;}
 if(mm::SmsNumber.contains(x,y)){smsComposeField=0;render();return true;}
 if(mm::smsTextRect(mm::SmsTextMaxLines).contains(x,y)){smsComposeField=1;render();return true;}
 if(mm::SmsSend.contains(x,y)){smsComposeField=2;smsSendNow();return true;}
 if(mm::SmsCancel.contains(x,y)){smsCancelCompose();return true;}
 return true;}
