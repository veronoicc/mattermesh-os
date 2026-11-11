// RX Log app (RxLogSection): live list of the received LoRa packets (matter::rxLog()), pause, type filter, packet detail,
// CSV export to the SD card and an options sheet. Included by main.cpp after cli_app.inc.
#include "services/rx_log_view.h"
#include "ui/rx_log_layout.h"
static int rxlFilter=0,rxlDefaultFilter=0,rxlOptionFocus=0,rxlDetailScroll=0,rxlFirstShown=0;
static bool rxlUptime=false,rxlAutoPause=true,rxlPaused=false,rxlOptions=false,rxlClearArmed=false,rxlDetail=false,rxlDetailPaused=false;// rxlDetailPaused: opening the detail paused the list, closing it resumes
static uint32_t rxlCut=matter::RX_LIVE,rxlTop=0,rxlSel=0,rxlSeen=0,rxlPaintedAt=0;// view cutoff (the sequence at the pause); sequence of the first visible / selected packet (0 = the newest, follows it)
static std::vector<std::string> rxlDetailLines;static std::string rxlDetailTitle;
static const matter::RxLog &rxlLog(){return matter::rxLog();}
static int rxlCount(){return matter::rxViewCount(rxlLog(),rxlFilter,rxlCut);}
static void rxlHome(){rxlTop=rxlSel=0;}
// Selected row of the view; a packet that left the ring or the filter falls back to the newest.
static int rxlSelIndex(int n){if(!n)return -1;if(!rxlSel)return 0;int i=matter::rxViewIndexOf(rxlLog(),rxlFilter,rxlCut,rxlSel);if(i<0)rxlSel=0;return i<0?0:i;}
static void rxlSelect(int index){auto p=matter::rxViewAt(rxlLog(),rxlFilter,rxlCut,index);rxlSel=p&&index>0?p->sequence:0;}
static void rxlMove(int delta){int n=rxlCount(),at=rxlSelIndex(n);if(at>=0)rxlSelect(constrain(at+delta,0,n-1));}
// First visible row: anchored to the packet at the top so a live list does not slide under the selection.
static int rxlFirst(int sel,int n){int first=rxlTop?matter::rxViewIndexOf(rxlLog(),rxlFilter,rxlCut,rxlTop):0;if(first<0)first=0;first=matter::rxClampFirst(first,sel,mm::RxLogRows,n);auto p=first>0?matter::rxViewAt(rxlLog(),rxlFilter,rxlCut,first):nullptr;rxlTop=p?p->sequence:0;return first;}
static void rxlSetPaused(bool pause){if(pause==rxlPaused)return;rxlPaused=pause;rxlCut=pause?rxlLog().total():matter::RX_LIVE;}
static void rxlStepFilter(int step){rxlFilter=matter::rxFilterStep(rxlFilter,step);rxlHome();}
// Repeater behind a path hash (type 2 = repeaters): its name when exactly one matches, else "" (hash only).
static std::string rxlRepeaterName(const std::string &hex){String prefix=hex.c_str();prefix.toLowerCase();int matches=0;String name=matter::nodeNameForPrefix(prefix,matches,2);return matches==1&&name.length()?mm::senderDisplayName(name.c_str()):std::string();}
static void rxlOpenDetail(){
 auto p=rxlCount()?matter::rxViewAt(rxlLog(),rxlFilter,rxlCut,rxlSelIndex(rxlCount())):nullptr;if(!p)return;
 matter::RxPacket packet=*p;if(rxlAutoPause&&!rxlPaused){rxlSetPaused(true);rxlDetailPaused=true;}
 rxlSel=packet.sequence;rxlDetailLines=matter::rxDetail(packet,rxlUptime,rxlRepeaterName);rxlDetailTitle="Packet #"+std::to_string(packet.sequence)+" "+matter::rxTypeName(packet.type());rxlDetailScroll=0;rxlDetail=true;
}
static void rxlCloseDetail(){rxlDetail=false;rxlDetailLines.clear();rxlDetailLines.shrink_to_fit();if(rxlDetailPaused){rxlDetailPaused=false;rxlSetPaused(false);}}
// The packets of the current view (filter and pause cutoff) as CSV in the SD root (listed by Files), oldest first, written in 1 KB chunks.
static void rxlExport(){
 if(!sdReady){notice="SD unavailable";return;}
 auto &log=rxlLog();std::vector<const matter::RxPacket*> list;for(int i=0;i<log.count();i++){auto p=log.newest(i);if(p->sequence<=rxlCut&&matter::rxFilterMatch(rxlFilter,p->type()))list.push_back(p);}
 if(list.empty()){notice="Nothing to export";return;}
 time_t now=time(nullptr);std::string name=matter::rxCsvName(now>=MM_EPOCH_MIN?uint32_t(now):0,millis()/1000),stem=name.substr(0,name.size()-4);
 String path=String("/")+name.c_str();for(int k=2;k<100&&SD.exists(path);k++)path=String("/")+stem.c_str()+"-"+String(k)+".csv";
 File f=SD.open(path,FILE_WRITE);if(!f){notice="Cannot create file / check SD";return;}
 std::string chunk=matter::rxCsvHeader();bool ok=true;
 for(auto it=list.rbegin();it!=list.rend()&&ok;++it){chunk+=matter::rxCsvLine(**it);if(chunk.size()>=1024){ok=f.write(reinterpret_cast<const uint8_t*>(chunk.data()),chunk.size())==chunk.size();chunk.clear();}}
 if(ok&&!chunk.empty())ok=f.write(reinterpret_cast<const uint8_t*>(chunk.data()),chunk.size())==chunk.size();
 f.close();if(!ok){SD.remove(path);notice="Export failed / SD full?";return;}
 notice=path.substring(path.lastIndexOf('/')+1)+" ("+String(int(list.size()))+")";
}
// Options sheet rows: 0 default type, 1 time format, 2 auto-pause, 3 export, 4 clear (tap twice), 5 done.
static void rxlOptionApply(int row,int step){
 rxlOptionFocus=row;if(row!=4)rxlClearArmed=false;
 if(row==0){rxlDefaultFilter=matter::rxFilterStep(rxlDefaultFilter,step);rxlFilter=rxlDefaultFilter;rxlHome();device::prefPutInt("mmrxlog","filter",rxlDefaultFilter);}
 else if(row==1){rxlUptime=!rxlUptime;device::prefPutInt("mmrxlog","uptime",rxlUptime);}
 else if(row==2){rxlAutoPause=!rxlAutoPause;device::prefPutInt("mmrxlog","apause",rxlAutoPause);}
 else if(row==3){rxlOptions=false;rxlExport();}
 else if(row==4){if(rxlClearArmed){rxlClearArmed=false;rxlOptions=false;matter::rxLogClear();rxlHome();notice="RX log cleared";}else rxlClearArmed=true;}
 else rxlOptions=false;
 render();
}
static void rxlReset(){rxlPaused=rxlOptions=rxlClearArmed=rxlDetail=rxlDetailPaused=false;rxlCut=matter::RX_LIVE;rxlHome();rxlDetailLines.clear();rxlDetailLines.shrink_to_fit();}
static void rxLogOpen(){rxlDefaultFilter=constrain(device::prefGetInt("mmrxlog","filter",0),0,matter::RX_FILTER_COUNT-1);rxlUptime=device::prefGetInt("mmrxlog","uptime",0)!=0;rxlAutoPause=device::prefGetInt("mmrxlog","apause",1)!=0;rxlFilter=rxlDefaultFilter;rxlReset();rxlSeen=rxlLog().total();rxlFirstShown=0;rxlOptionFocus=0;}
// Any leave (Back, Home, lock): the detail copy goes and the list goes live again for the next visit.
static void rxLogClose(){rxlReset();}
static void rxLogCompose(mm::State &v){
 auto &log=rxlLog();auto &x=v.rxl;x=mm::RxLogView();
 int n=rxlCount(),sel=rxlSelIndex(n),first=rxlFirst(sel,n);rxlFirstShown=first;
 x.ready=log.ready();x.paused=rxlPaused;x.stored=log.count();x.shown=n;x.first=first;x.fresh=rxlPaused?matter::rxNewCount(log,rxlFilter,rxlCut):0;x.filterName=matter::rxFilterName(rxlFilter);x.filtered=rxlFilter!=matter::RxAll;
 for(int i=0;i<mm::RxLogRows;i++){auto p=matter::rxViewAt(log,rxlFilter,rxlCut,first+i);if(!p)break;auto r=matter::rxRow(*p,rxlUptime);mm::RxLogRow row;row.time=r.time;row.type=r.type;row.route=r.route;row.hops=r.hops;row.rssi=r.rssi;row.snr=r.snr;row.hash=r.hash;row.length=r.length;row.selected=first+i==sel;x.rows.push_back(row);}
 x.options=rxlOptions;x.clearArmed=rxlClearArmed;x.uptime=rxlUptime;x.autoPause=rxlAutoPause;x.optionFocus=rxlOptionFocus;x.defaultFilterName=matter::rxFilterName(rxlDefaultFilter);
 if(rxlDetail){x.detail=true;x.detailTitle=rxlDetailTitle;x.detailLines=rxlDetailLines;x.detailScroll=rxlDetailScroll;}
 rxlSeen=log.total();rxlPaintedAt=millis();
}
static bool rxLogBack(){if(rxlDetail){rxlCloseDetail();render();return true;}if(rxlOptions){rxlOptions=rxlClearArmed=false;render();return true;}return false;}
static void rxlDetailMove(int delta){rxlDetailScroll=constrain(rxlDetailScroll+delta,0,std::max<int>(0,int(rxlDetailLines.size())-mm::RxDetailRows));}
// Keys: W/S (K/J) move, ENTER detail, H/L (D) type filter, SPACE (P) pause, M options, E export, A / Backspace back.
static bool rxLogKey(char c){
 notice="";
 if(c=='\b'||c=='a'){back();return true;}
 if(rxlDetail){if(c=='w'||c=='k')rxlDetailMove(-1);else if(c=='s'||c=='j')rxlDetailMove(1);else if(c=='\n')rxlCloseDetail();render();return true;}
 if(rxlOptions){
  if(c=='w'||c=='k'){rxlOptionFocus=(rxlOptionFocus+mm::RxOptionCount-1)%mm::RxOptionCount;rxlClearArmed=false;}
  else if(c=='s'||c=='j'){rxlOptionFocus=(rxlOptionFocus+1)%mm::RxOptionCount;rxlClearArmed=false;}
  else if(c=='h'){rxlOptionApply(rxlOptionFocus,-1);return true;}
  else if(c=='l'||c=='d'||c=='\n'||c==' '){rxlOptionApply(rxlOptionFocus,1);return true;}
  render();return true;}
 if(c=='w'||c=='k')rxlMove(-1);else if(c=='s'||c=='j')rxlMove(1);
 else if(c=='\n')rxlOpenDetail();
 else if(c==' '||c=='p')rxlSetPaused(!rxlPaused);
 else if(c=='h')rxlStepFilter(-1);else if(c=='l'||c=='d')rxlStepFilter(1);
 else if(c=='m'){rxlOptions=true;rxlOptionFocus=0;rxlClearArmed=false;}
 else if(c=='e')rxlExport();
 render();return true;
}
static bool rxLogTap(int x,int y){
 if(mm::BackButton.contains(x,y))return false;// main's Back: closes the popup or leaves
 notice="";
 if(rxlDetail){
  if(mm::rxDetailButton(0).contains(x,y))rxlDetailMove(-(mm::RxDetailRows-2));else if(mm::rxDetailButton(1).contains(x,y))rxlDetailMove(mm::RxDetailRows-2);else if(mm::rxDetailButton(2).contains(x,y))rxlCloseDetail();
  render();return true;}
 if(rxlOptions){
  if(mm::RxOptionsPopup.contains(x,y)){for(int i=0;i<mm::RxOptionCount;i++)if(mm::rxOptionRect(i).contains(x,y)){rxlOptionApply(i,1);return true;}return true;}
  rxlOptions=rxlClearArmed=false;render();return true;}
 if(mm::ComposeButton.contains(x,y)){rxlOptions=true;rxlOptionFocus=0;rxlClearArmed=false;render();return true;}
 if(mm::RxFilterPrev.contains(x,y)){rxlStepFilter(-1);render();return true;}
 if(mm::RxFilterNext.contains(x,y)||mm::RxFilterLabel.contains(x,y)){rxlStepFilter(1);render();return true;}
 if(mm::RxPauseButton.contains(x,y)){rxlSetPaused(!rxlPaused);render();return true;}
 for(int i=0;i<mm::RxLogRows;i++)if(mm::rxRowRect(i).contains(x,y)){if(matter::rxViewAt(rxlLog(),rxlFilter,rxlCut,rxlFirstShown+i)){rxlSelect(rxlFirstShown+i);rxlOpenDetail();render();}return true;}
 return false;
}
// Swipe up (dy<0) shows older packets: a page of rows; in the detail a page of lines.
static void rxLogScroll(int dy){if(rxlDetail)rxlDetailMove(dy<0?mm::RxDetailRows-2:-(mm::RxDetailRows-2));else if(!rxlOptions)rxlMove(dy<0?mm::RxLogRows:-mm::RxLogRows);}
// New packets repaint the open list at most every 1.5 s (e-paper); closed, detail or options open: nothing to do.
static void rxLogTick(){
 if(screen!=Screen::Section||section!=mm::RxLogSection||rxlDetail||rxlOptions)return;
 if(rxlLog().total()==rxlSeen||uint32_t(millis()-rxlPaintedAt)<1500)return;
 markRadioDirty();
}
