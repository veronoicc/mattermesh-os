// Scheduled messages (services/scheduler.h): the Scheduled app (section 63), Chat options > Send later, the SD file
// /mattermesh/scheduled.bin and the dispatcher run from the main loop. Included by main.cpp.
// Keys: W/K up, S/J down, D/L or ENTER activate (D/L next, H previous on a < value > row or a date field), A back, digits
// type a date field. Touch: the cards themselves. Layout: ui/scheduled_layout.h, drawing: ui/scheduled_renderer.inc.
#include "services/scheduler.h"
static mm_sched::Scheduler schedStore;static bool schedLoaded=false;
static int schedView=0,schedFocus=0,schedTop=0;static uint32_t schedSelected=0;// view: 0 list, 1 message, 2 form, 3 history, 4 delete question
// The form: the target is fixed (a new message: the chat it was started from; an edit: the message's own).
static bool schedFromChat=false,schedPendingForm=false,schedTypingText=false;static uint32_t schedEditId=0;static int schedChatForm=-1;
static String schedText;static mm_sched::Target schedTarget;static mm_sched::Condition schedCond=mm_sched::Condition::AtTime;static mm_sched::DateEdit schedDate;
static uint32_t schedPolledAt=0,schedIdleSince=0,schedSeenFinished=0;
static bool schedPage(){return !deviceLocked&&screen==Screen::Section&&section==mm::ScheduledSection;}
static bool schedClock(){return time(nullptr)>=MM_EPOCH_MIN;}
static std::string schedStamp(uint32_t epoch,const char *format){if(epoch<MM_EPOCH_MIN)return "no clock";time_t t=epoch;struct tm tm;localtime_r(&t,&tm);char b[28];strftime(b,sizeof b,format,&tm);return b;}
static std::string schedLocal(uint32_t epoch){return schedStamp(epoch,"%a %d %b %H:%M");}
static std::string schedIn(uint32_t seconds){if(seconds<90)return "in "+std::to_string(std::max<uint32_t>(1,seconds))+" s";if(seconds<5400)return "in "+std::to_string((seconds+30)/60)+" min";if(seconds<172800)return "in "+std::to_string((seconds+1800)/3600)+" h";return "in "+std::to_string((seconds+43200)/86400)+" d";}
static std::string schedOneLine(std::string text){for(auto &ch:text)if(ch=='\n'||ch=='\r'||ch=='\t')ch=' ';return text;}
// The chat as identified in a message: name, scope and (direct) the peer key; the index alone is never trusted.
static mm_sched::Target schedTargetOf(int chat){mm_sched::Target t;if(chat<0||chat>=chatCount)return t;t.name=chats[chat].name.c_str();t.scope=chats[chat].scope.c_str();t.direct=chats[chat].direct;t.key=t.direct?peerKeys[chat].c_str():"";return t;}
static bool schedSameTarget(const mm_sched::Target &a,const mm_sched::Target &b){return a.name==b.name&&a.scope==b.scope&&a.direct==b.direct&&a.key==b.key;}
static bool schedChatHolds(int chat,const mm_sched::Target &target){return chat>=0&&chat<chatCount&&schedSameTarget(target,schedTargetOf(chat));}
static bool schedPresent(const mm_sched::Item &it){return schedChatHolds(it.chat,it.target);}
static std::string schedWhen(const mm_sched::Item &it){return it.condition==mm_sched::Condition::AtTime?schedLocal(it.atEpoch):std::string(mm_sched::conditionName(it.condition));}
static std::string schedWhenLong(const mm_sched::Item &it){using C=mm_sched::Condition;return it.condition==C::AtTime?schedLocal(it.atEpoch):it.condition==C::WhenRepeaterHeard?std::string("When a repeater is heard"):it.condition==C::WhenRadioIdle?std::string("When the radio is idle"):"When "+it.target.name+" is heard";}
// ---- the file
static bool schedSave(){
 if(!sdReady)return false;if(!SD.exists("/mattermesh")&&!SD.mkdir("/mattermesh"))return false;
 std::string blob=schedStore.encode();File f=SD.open("/mattermesh/scheduled.tmp",FILE_WRITE);if(!f)return false;
 bool ok=f.write(reinterpret_cast<const uint8_t *>(blob.data()),blob.size())==blob.size();f.flush();f.close();if(!ok){SD.remove("/mattermesh/scheduled.tmp");return false;}
 ok=device::sdReplace(SD,"/mattermesh/scheduled.tmp","/mattermesh/scheduled.bin","/mattermesh/scheduled.bak");if(ok)schedStore.dirty=false;return ok;}
// Read once, from the main loop, when the card is there (the chats are loaded by then).
static void schedLoad(){
 schedLoaded=true;File f=SD.open(device::sdSavedPath(SD,"/mattermesh/scheduled.bin","/mattermesh/scheduled.bak"));if(!f)return;
 size_t n=f.isDirectory()?0:f.size();if(!n||n>16384){f.close();return;}std::string blob(n,'\0');size_t got=f.read(reinterpret_cast<uint8_t *>(&blob[0]),n);f.close();blob.resize(got);
 auto status=schedStore.decode(blob);schedSeenFinished=schedStore.finished;Serial.printf("SCHED_LOAD status=%d items=%u history=%u\n",int(status),unsigned(schedStore.items.size()),unsigned(schedStore.history.size()));}
// ---- what the dispatcher knows about the radio, the clock and who was heard (millis of this boot)
static mm_sched::Inputs schedInputs(){
 mm_sched::Inputs in;time_t t=time(nullptr);in.clockValid=t>=MM_EPOCH_MIN;in.nowEpoch=in.clockValid?uint32_t(t):0;in.nowMs=millis();in.repeaterHeardMs=matter::lastRepeaterHeardMs();
 auto snap=matter::snapshot();const bool link=radioLinkActive();
 in.radioSuspended=link?!radioLinkConnected():(snap.suspended||!snap.ready);in.txQueueFree=link||snap.queued==0;// not available (asleep, not started): everything waits; one at a time: the previous one must have left
 const bool idle=link||!matter::busy();if(idle){if(!schedIdleSince)schedIdleSince=in.nowMs|1;in.radioIdleMs=in.nowMs-schedIdleSince;}else{schedIdleSince=0;in.radioIdleMs=0;}
 in.contactHeardMs=[](const std::string &key){return matter::keyHeardMs(String(key.c_str()));};in.targetPresent=schedPresent;return in;}
// Messages sent or failed: the toast over any page, the Scheduled page when it is open.
static void schedAnnounce(){
 uint32_t fresh=std::min<uint32_t>(schedStore.finished-schedSeenFinished,uint32_t(schedStore.history.size()));schedSeenFinished=schedStore.finished;
 for(uint32_t i=fresh;i-->0;){const auto &h=schedStore.history[i];Serial.printf("SCHED_RESULT outcome=%s target=%s reason=%s\n",h.outcome==mm_sched::Outcome::Sent?"Sent":"Failed",h.target.c_str(),h.reason.c_str());}
 if(!fresh)return;const auto &h=schedStore.history[0];String line=h.outcome==mm_sched::Outcome::Sent?String("Scheduled message sent to ")+h.target.c_str():String("Scheduled message failed: ")+h.reason.c_str();
 chatToast(line,4000);if(schedPage())notice=line;if(!deviceLocked&&(screen==Screen::Chat||schedPage()))render();}
void schedLoop(){
 if(!schedLoaded){if(sdReady)schedLoad();return;}
 if(schedStore.items.empty()&&!schedStore.dirty)return;
 const uint32_t ms=millis();if(uint32_t(ms-schedPolledAt)<1000)return;schedPolledAt=ms;// about once a second
 if(schedStore.dirty)schedSave();if(schedStore.items.empty())return;
 auto in=schedInputs();uint32_t id=schedStore.poll(in);
 if(id)if(auto *it=schedStore.find(id)){String keep=notice,scope=it->target.scope.c_str();bool ok=queueRadioMessage(it->text.c_str(),&scope,it->chat,nullptr);String why=notice;notice=keep;schedStore.result(id,ok,why.c_str(),in);}
 if(schedStore.finished!=schedSeenFinished)schedAnnounce();
 if(schedStore.dirty)schedSave();}
// ---- the pages
static void schedClamp(){
 const int n=int(schedStore.items.size());if((schedView==1||schedView==4)&&!schedStore.find(schedSelected)){schedView=0;schedFocus=0;}
 if(schedView==0){schedFocus=constrain(schedFocus,0,n);schedTop=constrain(schedTop,0,std::max(0,n-mm::SchedRows));}
 if(schedView==3){int h=int(schedStore.history.size());schedFocus=constrain(schedFocus,0,std::max(0,h-1));schedTop=constrain(schedTop,0,std::max(0,h-mm::SchedHistRows));}}
static std::string schedState(const mm_sched::Item &it){
 if(it.attempts>0)return "Try "+std::to_string(it.attempts)+" of "+std::to_string(mm_sched::MaxAttempts)+" failed: "+it.lastError;if(it.now)return "Sending as soon as the radio is free";
 if(it.condition==mm_sched::Condition::AtTime&&!schedClock())return "Waiting for the clock";return "Waiting";}
// The form's date and time fields as an epoch in the local time zone; 0 when they are not a valid time.
static uint32_t schedFormEpoch(){struct tm tm={};tm.tm_mday=schedDate.f[0];tm.tm_mon=schedDate.f[1]-1;tm.tm_year=schedDate.f[2]-1900;tm.tm_hour=schedDate.f[3];tm.tm_min=schedDate.f[4];tm.tm_isdst=-1;time_t e=mktime(&tm);return e>0?uint32_t(e):0;}
static std::string schedSummary(){
 if(schedCond!=mm_sched::Condition::AtTime)return "";if(!schedClock())return "Set the clock first (Date / time)";
 uint32_t at=schedFormEpoch(),now=uint32_t(time(nullptr));return at<=now?std::string("In the past: pick a later time"):schedLocal(at)+"  ("+schedIn(at-now)+")";}
void schedCompose(mm::State &v){
 if(!schedPage())return;schedClamp();mm::ScheduledView &w=v.sched;w.view=schedView;w.focus=schedFocus;w.scroll=schedTop;w.clock=schedClock();w.total=int(schedStore.items.size());
 if(schedView==0){for(auto &it:schedStore.items){mm::ScheduledView::Row r;r.when=schedWhen(it);r.meta=it.target.name+": "+schedOneLine(it.text);r.retry=it.attempts>0;w.rows.push_back(r);}return;}
 if(schedView==3){for(auto &h:schedStore.history){mm::ScheduledView::Past p;p.sent=h.outcome==mm_sched::Outcome::Sent;p.head=std::string(p.sent?"Sent ":"Failed ")+schedStamp(h.whenEpoch,"%d %b %H:%M")+"  "+h.target;p.meta=p.sent?schedOneLine(h.text):h.reason+": "+schedOneLine(h.text);w.past.push_back(p);}return;}
 if(schedView==2){w.editing=schedEditId!=0;w.typing=schedTypingText;w.direct=schedTarget.direct;w.condition=int(schedCond);for(int i=0;i<5;i++)w.date[i]=schedDate.f[i];w.text=schedText.c_str();w.target=schedTarget.name;w.summary=schedSummary();return;}
 if(auto *it=schedStore.find(schedSelected)){w.itemWhen=schedWhenLong(*it);w.itemTo=it->target.name+(it->target.direct?" (direct)":"");w.itemText=it->text;w.itemState=schedState(*it);}}
void schedOpen(int id){
 (void)id;schedView=0;schedFocus=0;schedTop=0;schedSelected=0;schedTypingText=false;
 if(schedPendingForm){schedPendingForm=false;schedView=2;schedFocus=1;}else{schedFromChat=false;schedEditId=0;}}
void schedLeft(int next){(void)next;schedView=0;schedFocus=0;schedTop=0;schedTypingText=false;schedFromChat=schedPendingForm=false;schedEditId=0;schedText="";schedChatForm=-1;}
// Fills the form: the date fields start at the item's time, else 15 minutes ahead.
static void schedFormFor(int chat,const mm_sched::Target &target,const std::string &text,uint32_t editId,mm_sched::Condition cond,uint32_t at){
 schedChatForm=chat;schedTarget=target;schedText=text.c_str();schedEditId=editId;schedCond=cond;schedTypingText=false;
 uint32_t base=at?at:schedClock()?mm_sched::defaultAt(uint32_t(time(nullptr))):0;
 if(base){time_t t=base;struct tm tm;localtime_r(&t,&tm);schedDate.set(tm.tm_mday,tm.tm_mon+1,tm.tm_year+1900,tm.tm_hour,tm.tm_min);}}
// Chat options > Send later: the composer text becomes a scheduled message for this chat; the form opens to pick when.
void scheduleComposerOpen(){
 String text=input;text.trim();const int chat=activeChat;
 auto refuse=[&](const String &why){notice=why;chatToast(why,3000);render();};
 if(chat<0||chat>=chatCount)return refuse("Conversation unavailable");
 if(!text.length())return refuse("Type the message first");
 if(text.length()>messageByteLimit(chat))return refuse(matter::messageLimitError(messageByteLimit(chat)).c_str());
 if(schedStore.items.size()>=mm_sched::MaxPending)return refuse(String("Scheduled list full (")+int(mm_sched::MaxPending)+")");
 schedFormFor(chat,schedTargetOf(chat),text.c_str(),0,schedClock()?mm_sched::Condition::AtTime:mm_sched::Condition::WhenRepeaterHeard,0);
 schedFromChat=true;schedPendingForm=true;openSection(mm::ScheduledSection);}
static void schedFollow(int index,int rows){if(index<schedTop)schedTop=index;else if(index>=schedTop+rows)schedTop=index-rows+1;}
static void schedOpenItem(int index){if(index<0||index>=int(schedStore.items.size()))return;schedSelected=schedStore.items[index].id;schedView=1;schedFocus=0;notice="";render();}
static void schedEditSelected(){auto *it=schedStore.find(schedSelected);if(!it)return;schedFormFor(it->chat,it->target,it->text,it->id,it->condition,it->atEpoch);schedView=2;schedFocus=1;notice="";render();}
static void schedDelete(){schedStore.remove(schedSelected);schedSave();schedView=0;schedFocus=0;notice="Deleted";schedClamp();render();}
static void schedSendNow(){if(!schedStore.sendNow(schedSelected))return;schedPolledAt=0;schedView=0;schedFocus=0;notice="Sending as soon as the radio is free";render();}
static void schedItemAction(int i){if(i==0)schedEditSelected();else if(i==1)schedSendNow();else{schedView=4;schedFocus=1;notice="";render();}}
// SCHEDULE / SAVE: validates against the live chat and the clock, stores, and (from a chat) empties the draft and goes back.
static void schedCommit(){
 const int chat=schedChatForm;std::string text=mm_sched::trimmed(schedText.c_str()),why;
 if(!schedChatHolds(chat,schedTarget)){notice=mm_sched::ReasonGone;render();return;}
 if(text.size()>messageByteLimit(chat)){notice=matter::messageLimitError(messageByteLimit(chat)).c_str();render();return;}
 uint32_t at=0,now=uint32_t(time(nullptr));
 if(schedCond==mm_sched::Condition::AtTime){if(!schedClock()){notice="Set the clock first (Date / time)";render();return;}at=schedFormEpoch();if(at<=now){notice="Pick a time in the future";render();return;}}
 uint32_t id=schedEditId;
 if(id){if(!schedStore.edit(id,text,schedCond,at,millis(),why)){notice=why.c_str();render();return;}}
 else{mm_sched::Item it;it.chat=chat;it.target=schedTarget;it.text=text;it.condition=schedCond;it.atEpoch=at;id=schedStore.add(it,millis(),schedClock()?now:0,why);if(!id){notice=why.c_str();render();return;}}
 const bool saved=schedSave();auto *item=schedStore.find(id);const std::string when=item?schedWhenLong(*item):"";schedPolledAt=0;
 if(schedFromChat){input="";inputCursor=-1;chatDrafts.clear(chat);schedFromChat=false;schedView=0;chatToast(String(saved?"Scheduled: ":"Scheduled (SD save failed): ")+when.c_str(),3500);back();return;}
 schedSelected=id;schedView=1;schedFocus=0;schedEditId=0;notice=saved?"Saved":"Saved, SD write failed";render();}
// Typing in the form's text: ASCII keys, and the accented / layout letters (typeText) while it fits the chat's limit.
bool schedTyping(){return schedPage()&&schedView==2&&schedTypingText;}
String *schedField(){return schedTyping()?&schedText:nullptr;}
void schedAppend(const std::string &utf8){
 if(!schedTyping())return;size_t limit=messageByteLimit(schedChatForm);
 if(schedText.length()+utf8.size()>limit){notice=matter::messageLimitError(limit).c_str();render();return;}
 schedText+=utf8.c_str();notice="";repaintSettled();}
bool schedBack(){
 if(!schedPage())return false;
 if(schedView==4){schedView=1;schedFocus=2;notice="";render();return true;}
 if(schedView==3){schedView=0;schedFocus=0;schedTop=0;notice="";render();return true;}
 if(schedView==2){if(schedTypingText){schedTypingText=false;render();return true;}if(schedFromChat)return false;schedView=schedEditId?1:0;schedFocus=0;notice="";render();return true;}// from a chat: the generic Back returns to it, the draft kept
 if(schedView==1){int index=0;for(int i=0;i<int(schedStore.items.size());i++)if(schedStore.items[i].id==schedSelected)index=i;schedView=0;schedFocus=index+1;schedFollow(index,mm::SchedRows);notice="";render();return true;}
 return false;}
static void schedMove(int d){
 if(schedView==0){int n=int(schedStore.items.size());schedFocus=constrain(schedFocus+d,0,n);if(schedFocus>0)schedFollow(schedFocus-1,mm::SchedRows);else schedTop=0;}
 else if(schedView==3){int n=int(schedStore.history.size());schedFocus=constrain(schedFocus+d,0,std::max(0,n-1));schedFollow(schedFocus,mm::SchedHistRows);}
 render();}
// Form focus: text, condition, the date fields (At time only), SCHEDULE, CANCEL.
static void schedFormMove(int d){int f=schedFocus;do f+=d;while(f>=2&&f<=6&&schedCond!=mm_sched::Condition::AtTime);schedFocus=constrain(f,0,mm::SchedFocusCount-1);}
static void schedFormStep(int dir){
 if(schedFocus==1){schedCond=mm_sched::nextCondition(schedCond,dir,schedTarget.direct);notice="";}
 else if(schedFocus>=2&&schedFocus<=6)schedDate.step(schedFocus-2,dir);
 else if(schedFocus>=mm::SchedFocusSave)schedFocus=schedFocus==mm::SchedFocusSave?mm::SchedFocusCancel:mm::SchedFocusSave;}
bool schedKey(char c){
 if(!schedPage())return false;
 if(schedView==2&&schedTypingText){
  if(c=='\n'){schedTypingText=false;render();return true;}
  if(c>=32&&c<=126)schedAppend(std::string(1,c));return true;}// Backspace never gets here: the generic field edit / Back
 const bool down=c=='s'||c=='j',up=c=='w'||c=='k',next=c=='d'||c=='l',prev=c=='h',ok=c=='\n'||c==' ';
 if(schedView==2){
  if(c>='0'&&c<='9'&&schedFocus>=2&&schedFocus<=6){if(schedDate.digit(schedFocus-2,c-'0')&&schedFocus<6)schedFocus++;render();return true;}
  if(!(down||up||next||prev||ok))return false;
  if(down||up)schedFormMove(down?1:-1);
  else if(next||prev)schedFormStep(prev?-1:1);
  else if(schedFocus==0)schedTypingText=true;
  else if(schedFocus==1)schedFormStep(1);
  else if(schedFocus<=6)schedFormMove(1);
  else if(schedFocus==mm::SchedFocusSave){schedCommit();return true;}
  else{back();return true;}
  render();return true;}
 if(!(down||up||next||prev||ok))return false;
 if(schedView==0){if(down||up)schedMove(down?1:-1);else if(schedFocus==0){schedView=3;schedFocus=0;schedTop=0;notice="";render();}else schedOpenItem(schedFocus-1);return true;}
 if(schedView==3){if(down||up)schedMove(down?1:-1);return true;}
 if(schedView==4){if(ok&&schedFocus==0)schedDelete();else if(ok)schedBack();else{schedFocus=1-schedFocus;render();}return true;}
 if(ok){schedItemAction(schedFocus);return true;}
 schedFocus=constrain(schedFocus+((down||next)?1:-1),0,mm::SchedActionCount-1);render();return true;}
bool schedTap(int x,int y){
 if(!schedPage()||mm::BackButton.contains(x,y))return false;
 const int n=int(schedStore.items.size());
 if(schedView==0){
  if(mm::SchedHistoryCard.contains(x,y)){schedFocus=0;schedView=3;schedTop=0;notice="";render();return true;}
  for(int i=0;i<mm::SchedRows;i++)if(mm::schedRowRect(i).contains(x,y)&&schedTop+i<n){schedFocus=schedTop+i+1;schedOpenItem(schedTop+i);return true;}
  return true;}
 if(schedView==3)return true;
 if(schedView==4){if(mm::SchedDeleteYes.contains(x,y))schedDelete();else if(mm::SchedDeleteNo.contains(x,y))schedBack();return true;}
 if(schedView==1){for(int i=0;i<mm::SchedActionCount;i++)if(mm::schedActionRect(i).contains(x,y)){schedFocus=i;schedItemAction(i);return true;}return true;}
 // The form: a tap on the text types, anywhere else ends typing first.
 const bool wasTyping=schedTypingText;schedTypingText=false;
 if(mm::SchedFormText.contains(x,y)){schedFocus=0;schedTypingText=!wasTyping;render();return true;}
 if(mm::SchedFormCond.contains(x,y)){schedFocus=1;schedFormStep(mm::stepRowTap(mm::SchedFormCond,x));render();return true;}
 if(schedCond==mm_sched::Condition::AtTime){
  for(int i=0;i<5;i++)if(mm::schedFieldRect(i).contains(x,y)){schedFocus=2+i;render();return true;}
  if(mm::SchedFormMinus.contains(x,y)||mm::SchedFormPlus.contains(x,y)){if(schedFocus<2||schedFocus>6)schedFocus=2;schedDate.step(schedFocus-2,mm::SchedFormPlus.contains(x,y)?1:-1);render();return true;}}
 if(mm::SchedFormSave.contains(x,y)){schedFocus=mm::SchedFocusSave;schedCommit();return true;}
 if(mm::SchedFormCancel.contains(x,y)){back();return true;}
 if(wasTyping)render();return true;}
// A swipe pages the list or the history.
bool schedScroll(int dy){
 if(!schedPage()||(schedView!=0&&schedView!=3))return false;
 if(schedView==0){int n=int(schedStore.items.size());schedTop=constrain(schedTop+(dy<0?mm::SchedRows:-mm::SchedRows),0,std::max(0,n-mm::SchedRows));if(n)schedFocus=schedTop+1;}
 else{int n=int(schedStore.history.size());schedTop=constrain(schedTop+(dy<0?mm::SchedHistRows:-mm::SchedHistRows),0,std::max(0,n-mm::SchedHistRows));schedFocus=schedTop;}
 render();return true;}
// Serial (tests): "sched status|list|history|send <id>|del <id>", "sched add <chat> <secondsFromNow> <text>" (At time) and
// "sched add repeater|idle|contact <chat> <text>"; <chat> is the conversation number or its name.
static int schedChatByToken(const std::string &t){if(t.empty())return -1;if(t[0]>='0'&&t[0]<='9'){int i=atoi(t.c_str());return i<chatCount?i:-1;}for(int i=0;i<chatCount;i++)if(!strcasecmp(chats[i].name.c_str(),t.c_str()))return i;return -1;}
bool schedSerial(const String &s){
 if(!s.startsWith("sched"))return false;
 std::string line=s.c_str();size_t pos=5;
 auto tok=[&](){while(pos<line.size()&&line[pos]==' ')pos++;size_t b=pos;while(pos<line.size()&&line[pos]!=' ')pos++;return line.substr(b,pos-b);};
 auto rest=[&](){while(pos<line.size()&&line[pos]==' ')pos++;return line.substr(pos);};
 const std::string cmd=tok();
 if(cmd=="status")Serial.printf("SCHED_STATUS loaded=%d items=%u history=%u finished=%lu dirty=%d view=%d focus=%d form=%d sd=%d clock=%d\n",schedLoaded,unsigned(schedStore.items.size()),unsigned(schedStore.history.size()),(unsigned long)schedStore.finished,schedStore.dirty,schedView,schedFocus,schedView==2,sdReady,schedClock());
 else if(cmd=="list"){for(auto &it:schedStore.items)Serial.printf("SCHED_ITEM id=%lu chat=%d target=%s condition=%s at=%lu attempts=%d state=%s text=%s\n",(unsigned long)it.id,it.chat,it.target.name.c_str(),mm_sched::conditionName(it.condition),(unsigned long)it.atEpoch,it.attempts,schedState(it).c_str(),it.text.c_str());Serial.println("SCHED_END");}
 else if(cmd=="add"){
  std::string kind=tok(),chatToken,text;using C=mm_sched::Condition;C cond=C::AtTime;uint32_t secs=0;
  if(kind=="repeater"||kind=="idle"||kind=="contact"){cond=kind=="repeater"?C::WhenRepeaterHeard:kind=="idle"?C::WhenRadioIdle:C::WhenContactHeard;chatToken=tok();text=rest();}
  else{chatToken=kind;secs=uint32_t(atol(tok().c_str()));text=rest();}
  int chat=schedChatByToken(chatToken);std::string why;uint32_t id=0;
  if(chat<0)why="No such chat";else if(cond==C::AtTime&&!schedClock())why="Set the clock first";
  else{mm_sched::Item it;it.chat=chat;it.target=schedTargetOf(chat);it.text=text;it.condition=cond;it.atEpoch=cond==C::AtTime?uint32_t(time(nullptr))+secs:0;
   if(text.size()>messageByteLimit(chat))why="Message too long";else id=schedStore.add(it,millis(),schedClock()?uint32_t(time(nullptr)):0,why);}
  if(id){schedSave();schedPolledAt=0;}
  Serial.printf("SCHED_ADD ok=%d id=%lu reason=%s\n",id!=0,(unsigned long)id,why.c_str());}
 else if(cmd=="send"){uint32_t id=uint32_t(atol(tok().c_str()));bool ok=schedStore.sendNow(id);schedPolledAt=0;Serial.printf("SCHED_SEND ok=%d id=%lu\n",ok,(unsigned long)id);}
 else if(cmd=="del"){uint32_t id=uint32_t(atol(tok().c_str()));bool ok=schedStore.remove(id);if(ok)schedSave();Serial.printf("SCHED_DEL ok=%d id=%lu\n",ok,(unsigned long)id);}
 else if(cmd=="history"){for(auto &h:schedStore.history)Serial.printf("SCHED_HISTORY outcome=%s when=%lu target=%s reason=%s text=%s\n",h.outcome==mm_sched::Outcome::Sent?"Sent":"Failed",(unsigned long)h.whenEpoch,h.target.c_str(),h.reason.c_str(),h.text.c_str());Serial.printf("SCHED_HISTORY_END count=%u\n",unsigned(schedStore.history.size()));}
 else Serial.println("SCHED usage: status | list | history | add <chat> <secs> <text> | add repeater|idle|contact <chat> <text> | send <id> | del <id>");
 if(schedPage()){schedClamp();render();}
 return true;}
