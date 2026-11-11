// RPT Scanner app (section 53): the repeaters heard, a zero-hop scan, and per repeater PING, STATUS, LOGIN and an admin console.
// Included by main.cpp after cli_app.inc; the radio work is in the matter::rpt* functions (mesh/service.cpp), answers are polled in rptTick.
#include "services/rpt_rank.h"
#include "ui/rpt_layout.h"
constexpr int RPT_HIDE_HOURS[4]={0,1,6,24},RPT_LISTEN_SEC[3]={5,10,20},RPT_CONSOLE_MAX=100,RPT_PASSWORD_MAX=63,RPT_COMMAND_MAX=120;
constexpr uint32_t RPT_PAINT_MS=1500,RPT_REPLY_WAIT_MS=12000;
static int rptMode=0,rptSort=0,rptDefaultSort=0,rptHide=0,rptListen=1,rptFocus=0,rptFirst=0,rptMenuFocus=0,rptAction=0,rptPickFocus=0,rptPickFirst=0,rptPickTotal=0,rptConsoleScroll=0,rptHidden=0;// mode: 0 list, 1 repeater, 2 password, 3 console
static bool rptMenu=false,rptPick=false,rptRssi=true,rptMasked=true,rptDirty=true,rptWanted=false,rptActionKeys=false,rptScanOn=false,rptDetailScan=false;
static String rptInput;static std::string rptDetailKey,rptDetailDistance,rptMessage,rptNote,rptLoggedKey,rptConsoleKey;static float rptDetailOut=0;
static std::vector<mm::RptNode> rptList;static std::vector<std::string> rptDistance,rptConsole;static mm::RptNode rptDetail;
static std::vector<std::pair<String,std::string>> rptPickList;// key and name of the picker window
static matter::RptResult rptRes;static uint32_t rptRev=0,rptNodeRev=0,rptPaintAt=0,rptRefreshAt=0,rptConsoleWaitUntil=0;
static void rptStore(const char *key,int value){device::prefPutInt("mmrpt",key,value);}
static void rptLoad(){rptDefaultSort=constrain(device::prefGetInt("mmrpt","sort",0),0,mm::RptSortCount-1);rptHide=constrain(device::prefGetInt("mmrpt","hide",0),0,3);rptListen=constrain(device::prefGetInt("mmrpt","listen",1),0,2);rptRssi=device::prefGetInt("mmrpt","rssi",1)!=0;}
// The list: known repeaters plus the scan's responders, hidden when stale, in the chosen order; the open repeater follows its row.
static void rptRefresh(){
 const uint32_t now=millis();const time_t epoch=time(nullptr);std::vector<mm::RptNode> nodes;rptRefreshAt=now;
 for(auto &r:matter::rptRepeaters()){mm::RptNode n;n.key=r.node.key.c_str();n.name=r.node.name.c_str();n.live=r.node.live;n.contact=r.contact;n.snr=r.node.snr;n.rssi=r.node.rssi;n.hops=r.node.hops;
  n.ageS=r.node.live?(now-r.node.seen)/1000:r.node.advertAt&&epoch>=MM_EPOCH_MIN&&uint32_t(epoch)>=r.node.advertAt?uint32_t(epoch)-r.node.advertAt:~0u;
  n.hasPos=r.node.lat||r.node.lon;n.lat=r.node.lat/1e6;n.lon=r.node.lon/1e6;nodes.push_back(n);}
 std::vector<mm::RptHeard> heard;for(auto &f:matter::rptFound()){mm::RptHeard h;h.prefix=f.prefix.c_str();h.snrIn=f.snrIn;h.snrOut=f.snrOut;h.rssi=f.rssi;h.ageS=(now-f.at)/1000;heard.push_back(h);}
 nodes=mm::rptMerge(nodes,heard);
 auto gps=device::locationState();const bool here=(gps.fix||gps.configured)&&(gps.latitude!=0||gps.longitude!=0);
 auto distance=[&](const mm::RptNode &n){return here&&n.hasPos?mm::formatDistance(mm::geoDistanceMeters(gps.latitude,gps.longitude,n.lat,n.lon),mm::geoHeading(gps.latitude,gps.longitude,n.lat,n.lon)):std::string();};
 std::vector<mm::RptNode> shown;std::vector<mm::RptKey> keys;std::vector<std::string> texts;rptHidden=0;
 for(auto &n:nodes){
  if(rptMode>=1&&(mm::rptPrefixOf(rptDetailKey,n.key)||mm::rptPrefixOf(n.key,rptDetailKey))){rptDetail=n;rptDetailKey=n.key;rptDetailDistance=distance(n);rptDetailScan=false;for(auto &h:heard)if(mm::rptPrefixOf(h.prefix,n.key)){rptDetailScan=true;rptDetailOut=h.snrOut;}}
  if(!n.scanned&&mm::rptStale(n.ageS,RPT_HIDE_HOURS[rptHide])){rptHidden++;continue;}
  mm::RptKey k;k.name=mm::rptName(n);k.live=n.live;k.snrKnown=n.live;k.snr=n.snr;k.ageS=n.ageS;
  if(here&&n.hasPos){k.distKnown=true;k.meters=mm::geoDistanceMeters(gps.latitude,gps.longitude,n.lat,n.lon);}
  shown.push_back(n);keys.push_back(k);texts.push_back(distance(n));}
 rptList.clear();rptDistance.clear();for(int i:mm::rptOrder(keys,mm::RptSort(rptSort))){rptList.push_back(shown[i]);rptDistance.push_back(texts[i]);}
 const int last=std::max(0,int(rptList.size())-1);rptFocus=constrain(rptFocus,0,last);rptFirst=constrain(rptFirst,0,std::max(0,int(rptList.size())-mm::RptRows));
 if(rptFocus<rptFirst)rptFirst=rptFocus;if(rptFocus>=rptFirst+mm::RptRows)rptFirst=rptFocus-mm::RptRows+1;}
static void rptSyncResult();
static void rptOpen(){rptLoad();rptMode=0;rptSort=rptDefaultSort;rptFocus=rptFirst=0;rptMenu=rptPick=false;rptMessage="";rptNote="";rptDirty=true;rptWanted=false;rptScanOn=false;rptConsoleWaitUntil=0;rptRev=matter::rptRevision();rptNodeRev=matter::nodesRevision();rptPaintAt=millis();rptConsole.clear();rptLoggedKey="";wipeString(rptInput);}
static void rptClose(){matter::rptReset();rptList=std::vector<mm::RptNode>();rptDistance=std::vector<std::string>();rptConsole=std::vector<std::string>();rptPickList=std::vector<std::pair<String,std::string>>();rptRes=matter::RptResult();wipeString(rptInput);rptLoggedKey="";rptDetailKey="";rptMode=0;rptMenu=rptPick=false;rptConsoleWaitUntil=0;}
static void rptConsolePrint(const std::string &text){size_t at=0;while(at<=text.size()){size_t end=text.find('\n',at);if(end==std::string::npos)end=text.size();rptConsole.push_back(text.substr(at,end-at));at=end+1;}if(int(rptConsole.size())>RPT_CONSOLE_MAX)rptConsole.erase(rptConsole.begin(),rptConsole.begin()+(rptConsole.size()-RPT_CONSOLE_MAX));rptConsoleScroll=0;}
// Console of the open repeater; its lines stay while the same repeater is open again.
static void rptConsoleOpen(const char *greeting){if(rptConsoleKey!=rptDetail.key){rptConsole.clear();rptConsoleKey=rptDetail.key;}if(greeting)rptConsolePrint(greeting);else if(rptConsole.empty())rptConsolePrint("Console of "+mm::rptName(rptDetail));rptMode=3;wipeString(rptInput);rptConsoleScroll=0;}
static void rptSyncResult(){
 rptRes=matter::rptResult();
 if(rptMode==2&&rptRes.op==matter::RptOp::Login&&rptRes.state==2&&rptRes.key==rptDetail.key.c_str()){rptLoggedKey=rptDetail.key;rptConsoleOpen(rptRes.admin?"Logged in as admin":"Logged in as guest: commands need admin");}}
static void rptStartScan(){String reason;rptMessage=matter::rptScan(RPT_LISTEN_SEC[rptListen]*1000,reason)?"":reason.c_str();rptDirty=true;render();}
static void rptOpenDetail(int index){if(index<0||index>=int(rptList.size()))return;rptFocus=index;rptDetail=rptList[index];rptDetailKey=rptDetail.key;rptDetailDistance=rptDistance[index];rptDetailScan=false;rptMode=1;rptAction=0;rptActionKeys=false;rptNote="";rptSyncResult();rptDirty=true;render();}
static void rptPickLoad(){auto page=matter::contacts(1,"",rptPickFirst,mm::RptPickRows);rptPickTotal=page.total;rptPickList.clear();for(auto &n:page.rows)rptPickList.push_back({n.key,n.name.length()?std::string(n.name.c_str()):std::string("Unknown")});}
static void rptPickMove(int step){rptPickFocus=constrain(rptPickFocus+step,0,std::max(0,rptPickTotal-1));if(rptPickFocus<rptPickFirst)rptPickFirst=rptPickFocus;if(rptPickFocus>=rptPickFirst+mm::RptPickRows)rptPickFirst=rptPickFocus-mm::RptPickRows+1;rptPickLoad();render();}
static void rptPickPage(int dir){rptPickFirst=constrain(rptPickFirst+dir*mm::RptPickRows,0,std::max(0,rptPickTotal-mm::RptPickRows));rptPickFocus=rptPickFirst;rptPickLoad();render();}
// A chosen contact gets this repeater as the one hop of its DM route (Chat options > Path shows it as set by hand).
static void rptPickApply(){
 int i=rptPickFocus-rptPickFirst;if(i<0||i>=int(rptPickList.size()))return;
 const int width=matter::snapshot().config.pathBytes;uint8_t hash[3]={};
 bool ok=int(rptDetail.key.size())>=width*2&&mm::parseHex(rptDetail.key.c_str(),width*2,hash,width)&&matter::dmRouteSet(rptPickList[i].first,hash,1,width);
 rptNote=ok?"DM path to "+rptPickList[i].second+" set via "+mm::rptName(rptDetail)+": used for the next message":"Path not saved";rptPick=false;render();}
static void rptConsoleSend(const std::string &command){
 if(command.empty())return;String reason;
 if(matter::rptCommand(rptDetail.key.c_str(),command.c_str(),reason)){rptConsolePrint("> "+command);rptConsoleWaitUntil=millis()+RPT_REPLY_WAIT_MS;}else rptConsolePrint("! "+std::string(reason.c_str()));
 render();}
static void rptLogin(){
 String reason;rptNote="";if(matter::rptLogin(rptDetail.key.c_str(),rptInput,reason))rptSyncResult();else rptNote=reason.c_str();render();}
// PING, STATUS, LOGIN (or CONSOLE once logged in) and DM PATH of the open repeater.
static void rptDo(int action){
 rptNote="";String reason;
 if(action==3){rptPick=true;rptPickFocus=rptPickFirst=0;rptPickLoad();render();return;}
 if(action==2&&rptLoggedKey==rptDetail.key){rptConsoleOpen(nullptr);render();return;}
 if(!rptDetail.contact){rptNote="Not in contacts yet: wait for its advert";render();return;}
 if(action==2){rptMode=2;wipeString(rptInput);rptMasked=true;render();return;}
 if(!(action==0?matter::rptPing(rptDetail.key.c_str(),reason):matter::rptStatus(rptDetail.key.c_str(),reason)))rptNote=reason.c_str();else rptSyncResult();
 render();}
static void rptMenuChange(int step){
 if(rptMenuFocus==0){rptDefaultSort=(rptDefaultSort+step+mm::RptSortCount)%mm::RptSortCount;rptSort=rptDefaultSort;rptStore("sort",rptDefaultSort);}
 else if(rptMenuFocus==1){rptHide=(rptHide+step+4)%4;rptStore("hide",rptHide);}
 else if(rptMenuFocus==2){rptListen=(rptListen+step+3)%3;rptStore("listen",rptListen);}
 else{rptRssi=!rptRssi;rptStore("rssi",rptRssi);}
 rptDirty=true;render();}
static void rptSetSort(int sort){rptSort=(sort+mm::RptSortCount)%mm::RptSortCount;rptFocus=rptFirst=0;rptDirty=true;render();}
static String *rptField(){return rptMode>=2&&screen==Screen::Section&&section==mm::RptScannerSection?&rptInput:nullptr;}
static const char *rptQuickCommand(int i){static const char *commands[]={"ver","get name","neighbors","clock","advert"};return commands[i];}
// The password page and the console type into rptInput; every key is text there.
static bool rptTextKey(char c){
 if(c=='\n'){if(rptMode==2)rptLogin();else{std::string command=rptInput.c_str();wipeString(rptInput);rptConsoleSend(command);}return true;}
 if(c=='\b'){if(!rptInput.length()){back();return true;}rptInput.remove(rptInput.length()-1);repaintSettled();return true;}
 if(uint8_t(c)>=32&&uint8_t(c)<=126&&int(rptInput.length())<(rptMode==2?RPT_PASSWORD_MAX:RPT_COMMAND_MAX)){rptInput+=c;repaintSettled();}
 return true;}
static bool rptKey(char c){
 if(rptMode>=2)return rptTextKey(c);
 const bool up=c=='w'||c=='k',down=c=='s'||c=='j',go=c=='\n'||c==' '||c=='d'||c=='l';
 if(rptPick){if(up)rptPickMove(-1);else if(down)rptPickMove(1);else if(go)rptPickApply();else if(c=='a')return false;return true;}
 if(rptMode==1){
  if(up||down){rptActionKeys=true;rptAction=(rptAction+(down?1:mm::RptActionCount-1))%mm::RptActionCount;render();}
  else if(go){if(!rptActionKeys){rptActionKeys=true;render();}else rptDo(rptAction);}// the first click shows the focus
  else if(c=='a')return false;
  return true;}
 if(rptMenu){
  if(up||down){rptMenuFocus=(rptMenuFocus+(down?1:mm::RptMenuRows-1))%mm::RptMenuRows;render();}
  else if(go)rptMenuChange(1);else if(c=='h')rptMenuChange(-1);else if(c=='a')return false;
  return true;}
 if(up||down){rptFocus=constrain(rptFocus+(down?1:-1),0,std::max(0,int(rptList.size())-1));if(rptFocus<rptFirst)rptFirst=rptFocus;if(rptFocus>=rptFirst+mm::RptRows)rptFirst=rptFocus-mm::RptRows+1;render();}
 else if(c=='h')rptSetSort(rptSort-1);else if(c=='l')rptSetSort(rptSort+1);
 else if(c=='\n'||c==' '||c=='d')rptOpenDetail(rptFocus);
 else if(c=='S')rptStartScan();
 else if(c=='o'){rptMenu=true;rptMenuFocus=0;render();}
 else if(c=='a')return false;
 return true;}
static bool rptBack();
static bool rptTap(int x,int y){
 if(mm::BackButton.contains(x,y)){back();return true;}
 if(rptMode==2){
  if(mm::RptShow.contains(x,y)){rptMasked=!rptMasked;render();return true;}
  if(mm::RptCancel.contains(x,y)){rptBack();return true;}
  if(mm::RptGo.contains(x,y)){rptLogin();return true;}
  return y>=mm::Top;}
 if(rptMode==3){
  for(int i=0;i<mm::RptQuickCount;i++)if(mm::rptQuick(i).contains(x,y)){rptConsoleSend(rptQuickCommand(i));return true;}
  return y>=mm::Top;}
 if(rptPick){
  for(int i=0;i<int(rptPickList.size());i++)if(mm::rptPickRow(i).contains(x,y)){rptPickFocus=rptPickFirst+i;rptPickApply();return true;}
  if(mm::rptPickButton(0).contains(x,y))rptPickPage(-1);else if(mm::rptPickButton(1).contains(x,y))rptPickPage(1);else if(mm::rptPickButton(2).contains(x,y)){rptPick=false;render();}
  return y>=mm::Top;}
 if(rptMode==1){
  for(int i=0;i<mm::RptActionCount;i++)if(mm::rptAction(i).contains(x,y)){rptAction=i;rptActionKeys=true;rptDo(i);return true;}
  return false;}
 if(rptMenu){
  for(int i=0;i<mm::RptMenuRows;i++)if(mm::rptMenuRow(i).contains(x,y)){rptMenuFocus=i;rptMenuChange(1);return true;}
  if(!mm::RptMenuPopup.contains(x,y)&&y>=mm::Top){rptMenu=false;render();return true;}
  return y>=mm::Top;}
 if(mm::RptMenu.contains(x,y)){rptMenu=true;rptMenuFocus=0;render();return true;}
 for(int i=0;i<mm::RptTabCount;i++)if(mm::rptTabRect(i).contains(x,y)){rptSetSort(i);return true;}
 if(mm::RptScan.contains(x,y)){rptStartScan();return true;}
 for(int i=0;i<mm::RptRows&&rptFirst+i<int(rptList.size());i++)if(mm::rptRowRect(i).contains(x,y)){rptOpenDetail(rptFirst+i);return true;}
 return false;}
// BACK steps out: options, picker, console or password page to the repeater, the repeater to the list, the list leaves the app.
static bool rptBack(){
 if(rptPick){rptPick=false;render();return true;}
 if(rptMenu){rptMenu=false;render();return true;}
 if(rptMode>=2){rptMode=1;wipeString(rptInput);rptNote="";render();return true;}
 if(rptMode==1){rptMode=0;rptDirty=true;render();return true;}
 return false;}
static void rptScroll(int dy){
 if(rptMode==3){rptConsoleScroll=constrain(rptConsoleScroll+(dy<0?-6:6),0,int(rptConsole.size()));return;}
 if(rptPick){rptPickPage(dy<0?1:-1);return;}
 if(rptMode!=0||rptMenu)return;
 rptFirst=constrain(rptFirst+(dy<0?mm::RptRows:-mm::RptRows),0,std::max(0,int(rptList.size())-mm::RptRows));rptFocus=constrain(rptFocus,rptFirst,std::max(rptFirst,std::min(int(rptList.size())-1,rptFirst+mm::RptRows-1)));}
// Live repaints: new answers, replies and nodes, at most every RPT_PAINT_MS (e-paper); the scan countdown ticks the same way.
static void rptTick(){
 if(screen!=Screen::Section||section!=mm::RptScannerSection)return;
 const uint32_t now=millis();String text;
 while(matter::rptReply(text)){rptConsolePrint(text.c_str());rptConsoleWaitUntil=0;rptWanted=true;}
 if(rptConsoleWaitUntil&&int32_t(now-rptConsoleWaitUntil)>=0){rptConsoleWaitUntil=0;rptConsolePrint("(no reply)");rptWanted=true;}
 const uint32_t rev=matter::rptRevision(),nodes=matter::nodesRevision();
 if(rev!=rptRev){rptRev=rev;rptSyncResult();rptDirty=true;rptWanted=true;}
 if(nodes!=rptNodeRev){rptNodeRev=nodes;if(rptMode<2){rptDirty=true;rptWanted=true;}}
 auto scan=matter::rptScanState();
 if(scan.active!=rptScanOn){rptScanOn=scan.active;if(!scan.active)rptMessage="Scan done: "+std::to_string(scan.count)+(scan.count==1?" repeater answered":" repeaters answered");rptDirty=true;rptWanted=true;}
 if(scan.active&&rptMode==0)rptWanted=true;
 if(rptWanted&&now-rptPaintAt>=RPT_PAINT_MS){rptWanted=false;rptPaintAt=now;markRadioDirty();}}
static void rptCompose(mm::State &v){
 if(rptDirty||millis()-rptRefreshAt>=5000){rptDirty=false;rptRefresh();}// the ages in the rows follow the clock
 auto &r=v.rpt;auto radio=matter::snapshot();char text[96];
 r.radioOn=radio.ready&&!radio.suspended;r.mode=rptMode;r.sort=rptSort;r.defaultSort=rptDefaultSort;r.menu=rptMenu;r.picker=rptPick;r.menuFocus=rptMenuFocus;r.hideHours=RPT_HIDE_HOURS[rptHide];r.listenSec=RPT_LISTEN_SEC[rptListen];r.showRssi=rptRssi;
 r.focus=rptFocus;r.first=rptFirst;r.total=int(rptList.size());r.masked=rptMasked;
 if(rptMode==0){
  auto scan=matter::rptScanState();r.scanning=scan.active;
  if(scan.active){int left=int((scan.listenMs-std::min<uint32_t>(scan.listenMs,millis()-scan.startedAt)+999)/1000);snprintf(text,sizeof(text),"Scanning %ds / %d found",left,scan.count);r.status=text;}
  else if(rptMessage.length())r.status=rptMessage;
  else{r.status=std::to_string(rptList.size())+(rptList.size()==1?" repeater":" repeaters");if(rptHidden)r.status+=" / "+std::to_string(rptHidden)+" stale hidden";}
  for(int i=rptFirst;i<int(rptList.size())&&i<rptFirst+mm::RptRows;i++){
   auto &n=rptList[i];mm::RptRowView row;row.name=mm::rptName(n);row.distance=rptDistance[i];row.live=n.live;row.contact=n.contact;row.scanned=n.scanned;
   if(n.live){snprintf(text,sizeof(text),"SNR %.1f",n.snr);row.signal=text;if(rptRssi){snprintf(text,sizeof(text)," / %.0f dBm",n.rssi);row.signal+=text;}row.age=mm::rptHops(n.hops)+" / "+mm::rptAge(n.ageS);}
   else{row.signal=n.contact?"Not heard since boot":"Not saved";row.age=mm::rptAge(n.ageS);}
   r.rows.push_back(row);}
  return;}
 if(rptMode==1){
  r.name=mm::rptName(rptDetail);r.key=rptDetail.key;r.contact=rptDetail.contact;r.loggedIn=rptLoggedKey==rptDetail.key;r.actionFocus=rptActionKeys?rptAction:-1;
  if(rptDetail.hasPos){snprintf(text,sizeof(text),"%.5f, %.5f",rptDetail.lat,rptDetail.lon);r.position=text;if(rptDetailDistance.length())r.position+=" / "+rptDetailDistance;}else r.position="No position in its advert";
  r.advert=rptDetail.ageS==~0u?"Never heard":"Heard "+mm::rptAge(rptDetail.ageS)+" ago"+(rptDetail.live?" / "+mm::rptHops(rptDetail.hops):"");
  if(rptDetailScan){snprintf(text,sizeof(text),"SNR %.1f in / %.1f out / RSSI %.0f",rptDetail.snr,rptDetailOut,rptDetail.rssi);r.signal=text;}
  else if(rptDetail.live){snprintf(text,sizeof(text),"SNR %.1f dB / RSSI %.0f dBm",rptDetail.snr,rptDetail.rssi);r.signal=text;}else r.signal="No signal data this boot";
  uint8_t encoded=mm::DirectPathUnknown,bytes[64];int hops=-1;
  if(rptDetail.contact&&rptDetail.key.size()==64&&matter::contactPath(rptDetail.key.c_str(),encoded,bytes,sizeof(bytes))){hops=mm::directPathHops(encoded);r.path=hops<0?"Path: none learned (flood)":hops==0?"Path: direct":"Path: "+std::to_string(hops)+(hops==1?" hop ":" hops ")+mm::directPathHashes(encoded,bytes);}
  else r.path=rptDetail.contact?"Path: unknown":"Path: not a contact";
  if(rptNote.length())r.answer.push_back(rptNote);
  if(rptRes.key==rptDetail.key.c_str()&&rptRes.op!=matter::RptOp::None){
   r.answerState=rptRes.state;
   if(rptRes.state==3)r.answer.push_back(rptRes.text.c_str());
   else if(rptRes.op==matter::RptOp::Ping){
    if(rptRes.state==1)r.answer.push_back("Ping sent, listening for the echo");
    else if(rptRes.state==2){snprintf(text,sizeof(text),"Echo in %.2f s",rptRes.roundTripMs/1000.0);r.answer.push_back(text);snprintf(text,sizeof(text),"SNR out %.1f dB / in %.1f dB",rptRes.snrOut,rptRes.snrIn);r.answer.push_back(text);}}
   else if(rptRes.op==matter::RptOp::Status){
    if(rptRes.state==1)r.answer.push_back("Status requested");else if(rptRes.state==2)for(auto &l:matter::rptStatsLines(rptRes.stats))r.answer.push_back(l);}
   else if(rptRes.op==matter::RptOp::Login&&rptRes.state==1)r.answer.push_back("Login sent");}
  if(r.answer.empty())r.answer.push_back(rptDetail.contact?"PING tests the direct link, STATUS reads its counters, LOGIN opens the admin console.":"Not in contacts yet: wait for its advert to ask it. DM PATH works.");
  if(rptPick){r.pickFocus=rptPickFocus;r.pickFirst=rptPickFirst;r.pickTotal=rptPickTotal;for(auto &p:rptPickList)r.pickRows.push_back(p.second);}
  return;}
 r.name=mm::rptName(rptDetail);r.input=rptInput.c_str();
 if(rptMode==2){r.waiting=rptRes.op==matter::RptOp::Login&&rptRes.state==1&&rptRes.key==rptDetail.key.c_str();r.note=rptNote.length()?rptNote:rptRes.op==matter::RptOp::Login&&rptRes.state==3&&rptRes.key==rptDetail.key.c_str()?std::string(rptRes.text.c_str()):std::string();return;}
 r.console=rptConsole;r.consoleScroll=rptConsoleScroll;r.waiting=rptConsoleWaitUntil!=0;}
