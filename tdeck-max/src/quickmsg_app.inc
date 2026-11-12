// Quick messages (services/quick_messages): Settings > Quick messages (section 61) and the picker over a chat. Included by main.cpp.
// Settings keys: W/K up, S/J down, D/L or ENTER activate (D next on a < value > row, H previous; on a message ENTER edits, D flips its
// position pin, U / N move it up / down, X deletes), A back; while editing every key is text, ENTER saves, BACK cancels.
// Picker keys: W/K, S/J move, ENTER or D pick, 1-8 (Sym/Alt + W..I) pick that message, A / BACK close. The Quick key (Alt + Q / Y / U on an
// empty composer) opens it. NVS "mmquick": "list" (the presets), "key", "mode".
#include "services/quick_messages.h"
static mm_quick::List qmList;static mm_quick::Key qmKey=mm_quick::Key::Off;static mm_quick::Mode qmMode=mm_quick::Mode::Send;static bool qmLoaded=false;
static int qmFocus=0,qmScroll=0,qmEditing=-1,qmConfirm=0,qmConfirmFocus=1,qmPickFocus=0;static String qmInput;static bool qmPicking=false;static std::string qmPickNotice;
static void qmEnsure(){if(qmLoaded)return;qmLoaded=true;mm_quick::deserialize(device::prefGetString("mmquick","list").c_str(),qmList);qmKey=mm_quick::keyFrom(device::prefGetInt("mmquick","key",0));qmMode=mm_quick::modeFrom(device::prefGetInt("mmquick","mode",0));}
static bool qmPage(){return !deviceLocked&&screen==Screen::Section&&section==mm::QuickMessagesSection;}
// The list applies only once stored.
static bool qmStore(const mm_quick::List &next){if(!device::prefPutString("mmquick","list",String(mm_quick::serialize(next).c_str()))){notice="Save failed";render();return false;}qmList=next;return true;}
static void qmSync(){int rows=int(qmList.size())+(qmEditing==int(qmList.size())?1:0);if(qmFocus>=2&&qmFocus<2+rows)qmScroll=mm_quick::scrollFor(qmFocus-2,qmScroll,mm::QuickRows,rows);}
static void qmReset(){qmFocus=qmScroll=0;qmEditing=-1;qmConfirm=0;qmConfirmFocus=1;qmInput="";notice="";}
void quickmsgOpen(int id){(void)id;qmEnsure();qmReset();}
void quickmsgLeft(int next){(void)next;qmReset();}
bool quickPickShown(){return qmPicking&&!deviceLocked&&screen==Screen::Chat;}
void quickmsgCompose(mm::State &v){
 qmEnsure();const bool listPage=!deviceLocked&&screen==Screen::Section&&(section==mm::QuickMessagesSection||section==19);
 if(!listPage&&!quickPickShown())return;
 auto &q=v.quick;const int n=int(qmList.size());
 q.summary=n?std::to_string(n)+(n==1?" message / ":" messages / ")+(qmKey==mm_quick::Key::Off?"key off":mm_quick::keyLabel(qmKey)):"No messages";
 q.keyLabel=mm_quick::keyLabel(qmKey);q.modeLabel=mm_quick::modeLabel(qmMode);
 if(!qmPage()&&!quickPickShown())return;
 for(auto &p:qmList)q.rows.push_back({p.text,p.position});
 if(quickPickShown()){q.picker=true;q.pickFocus=qmPickFocus;q.pickNotice=qmPickNotice;return;}
 if(qmEditing==n)q.rows.push_back({"",false});
 q.focus=qmFocus;q.scroll=qmScroll;q.editing=qmEditing;q.confirm=qmConfirm;q.confirmFocus=qmConfirmFocus;q.input=qmInput.c_str();}
String *quickmsgField(){return qmPage()&&qmEditing>=0?&qmInput:nullptr;}
static bool qmSetKey(int dir){auto next=mm_quick::stepKey(qmKey,dir);if(!device::prefPutInt("mmquick","key",int(next))){notice="Save failed";render();return false;}qmKey=next;notice="";render();return true;}
static bool qmSetMode(int dir){auto next=mm_quick::stepMode(qmMode,dir);if(!device::prefPutInt("mmquick","mode",int(next))){notice="Save failed";render();return false;}qmMode=next;notice="";render();return true;}
static void qmStartEdit(int p){if(p<0||p>=int(qmList.size()))return;qmEditing=p;qmInput=qmList[p].text.c_str();qmFocus=2+p;qmSync();notice="";render();}
static void qmStartAdd(){if(qmList.size()>=mm_quick::MaxPresets){notice=mm_quick::ErrFull;render();return;}qmEditing=int(qmList.size());qmInput="";qmFocus=2+qmEditing;qmSync();notice="";render();}
// A refused text (empty) or a failed save keeps the editor open with the reason.
static bool qmCommit(){auto next=qmList;std::string text=qmInput.c_str(),why=qmEditing<int(next.size())?mm_quick::edit(next,qmEditing,text):mm_quick::add(next,text);
 if(!why.empty()){notice=why.c_str();render();return false;}
 if(!qmStore(next))return false;qmEditing=-1;qmInput="";notice="";render();return true;}
static void qmCancelEdit(){qmEditing=-1;qmInput="";notice="";qmFocus=constrain(qmFocus,0,3+int(qmList.size()));qmSync();render();}
static void qmAskDelete(){int p=qmFocus-2;if(p<0||p>=int(qmList.size()))return;qmConfirm=1;qmConfirmFocus=1;notice="";render();}
static void qmDelete(){auto next=qmList;int p=qmFocus-2;qmConfirm=0;if(mm_quick::remove(next,p)&&qmStore(next))qmFocus=next.empty()?2:std::min(qmFocus,1+int(next.size()));qmSync();render();}
static void qmDoReset(){qmConfirm=0;if(qmStore(mm_quick::defaults())){qmFocus=2;qmScroll=0;notice="Defaults restored";}render();}
static void qmMoveRow(int dir){int p=qmFocus-2;if(p<0||p>=int(qmList.size()))return;auto next=qmList;int to=dir<0?mm_quick::moveUp(next,p):mm_quick::moveDown(next,p);if(to==p){render();return;}if(qmStore(next)){qmFocus=2+to;qmSync();notice="";}render();}
static void qmTogglePin(){int p=qmFocus-2;auto next=qmList;if(!mm_quick::togglePosition(next,p))return;if(qmStore(next))notice="";render();}
static void qmFocusMove(int d){qmFocus=constrain(qmFocus+d,0,3+int(qmList.size()));qmSync();notice="";render();}
// The action bar and the keyboard share it: i 0 edit, 1 up, 2 down, 3 pin, 4 delete.
static void qmAction(int i){int p=qmFocus-2;if(p<0||p>=int(qmList.size())){notice="Select a message first";render();return;}
 if(i==0)qmStartEdit(p);else if(i==1)qmMoveRow(-1);else if(i==2)qmMoveRow(1);else if(i==3)qmTogglePin();else qmAskDelete();}
static void qmActivate(){int n=int(qmList.size());
 if(qmFocus==0)qmSetKey(1);else if(qmFocus==1)qmSetMode(1);else if(qmFocus<2+n)qmStartEdit(qmFocus-2);else if(qmFocus==2+n)qmStartAdd();else{qmConfirm=2;qmConfirmFocus=1;notice="";render();}}
// Picks preset i for the active chat: sent now (position text appended when it asks) or inserted at the composer caret. False with the reason in qmPickNotice.
static bool qmUse(int i,bool insert){
 if(i<0||i>=int(qmList.size()))return false;const auto &p=qmList[i];
 auto gps=device::locationState();std::string position=gps.injected?std::string():device::gpsCoordinateMessage(gps.available,gps.enabled,gps.fix,gps.fixAgeMs,gps.latitude,gps.longitude);/* never a "gps inject" test position */
 String lead;if(insert){unsigned at=cursorAt();if(at>0&&input[at-1]!=' '&&input[at-1]!='\n')lead=" ";}
 size_t limit=messageByteLimit(),room=insert?(input.length()<limit?limit-input.length():0):limit;room=room>lead.length()?room-lead.length():0;
 auto r=mm_quick::compose(p,position,room);
 if(!r.ok){qmPickNotice=r.error==mm_quick::ErrNoFix&&gps.available&&!gps.enabled&&device::locationEnabled(true)?"Acquiring GPS position":r.error;render();return false;}
 if(insert){inputInsert(lead+String(r.text.c_str()));qmPicking=false;qmPickNotice.clear();notice="";render();return true;}
 if(!queueRadioMessage(String(r.text.c_str()))){qmPickNotice=notice.c_str();render();return false;}
 qmPicking=false;qmPickNotice.clear();notice="";chatToast(String("Sent: ")+p.text.c_str(),1500);render();return true;}
void quickPickerOpen(){if(deviceLocked||screen!=Screen::Chat)return;qmEnsure();qmPicking=true;qmPickFocus=0;qmPickNotice.clear();chatDetails=false;render();}
// Quick key (called with the raw key letter of a key press): opens the picker, or closes it again; never over another panel or a draft.
bool quickmsgShortcut(char ch){
 qmEnsure();char want=mm_quick::keyChar(qmKey);
 if(!want||ch!=want||deviceLocked||screen!=Screen::Chat||!keyboardModifiers.alt||keyboardModifiers.leftShift||keyboardModifiers.rightShift||radioLinkAsking())return false;
 if(qmPicking){qmPicking=false;render();return true;}
 if(input.length()||chatOverlayOpen()||dictationChat>=0)return false;
 accentTaps=0;quickPickerOpen();return true;}
bool quickmsgBack(){
 if(quickPickShown()){qmPicking=false;qmPickNotice.clear();render();return true;}
 if(!qmPage())return false;
 if(qmConfirm){qmConfirm=0;render();return true;}
 if(qmEditing>=0){qmCancelEdit();return true;}
 return false;}
bool quickmsgKey(char c){
 if(deviceLocked)return false;
 if(quickPickShown()){int n=int(qmList.size());
  if(c=='w'||c=='k'){qmPickFocus=constrain(qmPickFocus-1,0,std::max(0,n-1));qmPickNotice.clear();render();}
  else if(c=='s'||c=='j'){qmPickFocus=constrain(qmPickFocus+1,0,std::max(0,n-1));qmPickNotice.clear();render();}
  else if(c=='\n'||c=='d'||c=='l'){qmUse(qmPickFocus,qmMode==mm_quick::Mode::Insert);}
  else if(c>='1'&&c<='8'){if(c-'1'<n){qmPickFocus=c-'1';qmUse(qmPickFocus,qmMode==mm_quick::Mode::Insert);}}
  else if(c=='a'){qmPicking=false;qmPickNotice.clear();render();}
  return true;}
 if(!qmPage())return false;
 if(qmEditing>=0){if(c=='\n')qmCommit();else if(mm_quick::accept(qmInput.c_str(),c)){qmInput+=c;repaintSettled();}return true;}
 const bool down=c=='s'||c=='j',up=c=='w'||c=='k',next=c=='d'||c=='l',prev=c=='h',ok=c=='\n'||c==' ';
 if(qmConfirm){if(!(down||up||next||prev||ok))return false;if(ok){if(qmConfirmFocus==0){if(qmConfirm==1)qmDelete();else qmDoReset();}else{qmConfirm=0;render();}}else{qmConfirmFocus=1-qmConfirmFocus;render();}return true;}
 const int n=int(qmList.size()),onRow=qmFocus>=2&&qmFocus<2+n;
 if(onRow&&(c=='u'||c=='n'||c=='x')){if(c=='x')qmAskDelete();else qmMoveRow(c=='u'?-1:1);return true;}
 if(!(down||up||next||prev||ok))return false;
 if(down||up){qmFocusMove(down?1:-1);return true;}
 if(qmFocus<2){if(prev)(qmFocus==0?qmSetKey(-1):qmSetMode(-1));else if(qmFocus==0)qmSetKey(1);else qmSetMode(1);return true;}
 if(onRow){if(ok)qmStartEdit(qmFocus-2);else if(next)qmTogglePin();return true;}
 if(!prev)qmActivate();return true;}
bool quickmsgTap(int x,int y){
 if(deviceLocked)return false;
 if(quickPickShown()){int n=std::min(mm::QuickPickRows,int(qmList.size()));
  if(mm::QuickPickCancel.contains(x,y)||mm::BackButton.contains(x,y)){qmPicking=false;qmPickNotice.clear();render();return true;}
  for(int i=0;i<n;i++)if(mm::quickPickRect(i).contains(x,y)){qmPickFocus=i;qmUse(i,qmMode==mm_quick::Mode::Insert);return true;}
  return true;}
 if(!qmPage()||mm::BackButton.contains(x,y))return false;
 if(qmConfirm){if(mm::QuickConfirmYes.contains(x,y)){if(qmConfirm==1)qmDelete();else qmDoReset();}else if(mm::QuickConfirmNo.contains(x,y)){qmConfirm=0;render();}return true;}
 const int n=int(qmList.size());int hit=-1;for(int i=0;i<mm::QuickRows;i++)if(mm::quickRowRect(i).contains(x,y)&&qmScroll+i<n)hit=qmScroll+i;
 if(qmEditing>=0){bool same=hit==qmEditing||mm::quickActionRect(0).contains(x,y);if(!qmCommit()||same)return true;}
 if(mm::QuickKeyRow.contains(x,y)){qmFocus=0;qmSetKey(mm::stepRowTap(mm::QuickKeyRow,x));return true;}
 if(mm::QuickModeRow.contains(x,y)){qmFocus=1;qmSetMode(mm::stepRowTap(mm::QuickModeRow,x));return true;}
 if(hit>=0){if(qmFocus==2+hit)qmStartEdit(hit);else{qmFocus=2+hit;notice="";render();}return true;}
 for(int i=0;i<mm::QuickActionCount;i++)if(mm::quickActionRect(i).contains(x,y)){qmAction(i);return true;}
 if(mm::QuickAdd.contains(x,y)){qmFocus=2+n;qmStartAdd();return true;}
 if(mm::QuickReset.contains(x,y)){qmFocus=3+n;qmActivate();return true;}
 return true;}
// A swipe pages the list.
bool quickmsgScroll(int dy){
 if(!qmPage()||qmConfirm||qmEditing>=0)return false;
 qmScroll=constrain(qmScroll+(dy<0?mm::QuickRows:-mm::QuickRows),0,std::max(0,int(qmList.size())-mm::QuickRows));render();return true;}
// The picker belongs to the chat: it goes with the screen.
void quickmsgLoop(){if(qmPicking&&(deviceLocked||screen!=Screen::Chat))qmPicking=false;}
// Serial (tests): "quick status|list|picker|reset|add <text>|send <n>|key <n>|mode <n>".
bool quickmsgSerial(const String &s){
 if(!s.startsWith("quick "))return false;qmEnsure();
 if(s=="quick list"){for(size_t i=0;i<qmList.size();i++)Serial.printf("QUICK_ROW n=%u position=%d text=%s\n",unsigned(i+1),qmList[i].position,qmList[i].text.c_str());Serial.printf("QUICK_END count=%u\n",unsigned(qmList.size()));}
 else if(s=="quick picker"){quickPickerOpen();Serial.printf("QUICK_PICKER open=%d\n",quickPickShown());}
 else if(s=="quick reset"){bool ok=qmStore(mm_quick::defaults());Serial.printf("QUICK_RESET ok=%d count=%u\n",ok,unsigned(qmList.size()));}
 else if(s.startsWith("quick add ")){auto next=qmList;std::string why=mm_quick::add(next,s.substring(10).c_str());bool ok=why.empty()&&qmStore(next);Serial.printf("QUICK_ADD ok=%d count=%u error=%s\n",ok,unsigned(qmList.size()),why.c_str());}
 else if(s.startsWith("quick send ")){int n=s.substring(11).toInt();qmPickNotice.clear();bool ok=qmUse(n-1,false);Serial.printf("QUICK_SEND n=%d ok=%d chat=%d error=%s\n",n,ok,activeChat,ok?"":qmPickNotice.c_str());}
 else if(s.startsWith("quick key ")){int v=s.substring(10).toInt();bool ok=v>=0&&v<mm_quick::KeyCount&&device::prefPutInt("mmquick","key",v);if(ok)qmKey=mm_quick::Key(v);Serial.printf("QUICK_KEY ok=%d key=%d label=%s\n",ok,int(qmKey),mm_quick::keyLabel(qmKey));}
 else if(s.startsWith("quick mode ")){int v=s.substring(11).toInt();bool ok=v>=0&&v<mm_quick::ModeCount&&device::prefPutInt("mmquick","mode",v);if(ok)qmMode=mm_quick::Mode(v);Serial.printf("QUICK_MODE ok=%d mode=%d label=%s\n",ok,int(qmMode),mm_quick::modeLabel(qmMode));}
 else Serial.printf("QUICK_STATUS count=%u key=%d mode=%d picker=%d focus=%d editing=%d confirm=%d\n",unsigned(qmList.size()),int(qmKey),int(qmMode),quickPickShown(),qmFocus,qmEditing,qmConfirm);
 return true;}
