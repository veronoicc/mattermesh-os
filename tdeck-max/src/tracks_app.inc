// GPS tracks (services/tracks): the Tracks app (section 49) and Settings > GPS tracks (section 50). Included by main.cpp.
// Keys: W/K up, S/J down, D/L or ENTER activate (D next on a < value > row, H previous), A back. Touch: the cards themselves.
static int trkView=0,trkFocus=0,trkScroll=0;// view: 0 list, 1 detail, 2 delete confirm
static std::vector<device::TrackInfo> trkList;static std::string trkSelected;
static std::string trkLocal(uint32_t t){if(!t)return "No clock";time_t tt=t;struct tm tm;localtime_r(&tt,&tm);char b[24];strftime(b,sizeof b,"%a %d %b %H:%M",&tm);return b;}
static std::string trkMeta(const mm_track::Summary &s){return mm_track::distanceLabel(s.distanceM)+"  "+(s.durationS?mm_track::durationLabel(s.durationS):std::string("--"))+"  +"+std::to_string(s.gainM)+" m";}
static void trkRefresh(){trkList=device::tracksList(64);int n=int(trkList.size());trkFocus=std::min(trkFocus,n);trkScroll=constrain(trkScroll,0,std::max(0,n-mm::TrackRows));}
void tracksOpen(int id){trkView=0;trkFocus=0;trkScroll=0;trkSelected.clear();device::tracksDeselect();if(id==mm::TracksAppSection)trkRefresh();}
// Leaving the app: the selection goes, except into the Maps app ("View on map" draws it there).
void tracksLeft(int next){if(next!=9){trkSelected.clear();device::tracksDeselect();}}
void tracksCompose(mm::State &v){
 auto st=device::tracksState();auto cfg=device::tracksSettings();
 v.status.rec=st.recording;v.trkRecording=st.recording;v.trkSd=st.sd;v.trkAuto=cfg.autoStart;v.trkMap=cfg.showOnMap;v.trkKeepGps=cfg.keepGps;v.trkInterval=cfg.intervalS;v.trkDistance=cfg.minDistM;
 if(deviceLocked||screen!=Screen::Section||(section!=mm::TracksAppSection&&section!=mm::TracksSection))return;
 v.trkFocus=trkFocus;v.trkView=trkView;v.trkScroll=trkScroll;
 if(section==mm::TracksSection){v.trkStatus=!st.sd?"No SD card":st.recording?std::to_string(st.points)+" points / "+mm_track::distanceLabel(st.distanceM):"Off";if(st.recording&&!st.error.empty())v.trkStatus=st.error;if(notice.length())v.trkStatus=notice.c_str();return;}
 auto gps=device::locationState();
 v.trkStatus=!st.sd?"No SD card":st.recording?(!st.error.empty()?st.error:st.points?mm_track::distanceLabel(st.distanceM)+" / "+mm_track::durationLabel(st.durationS):std::string(gps.fix?"Waiting for a point":"Waiting for GPS fix")):!gps.available?"GPS unavailable":!gps.enabled?"GPS is off":gps.fix?"GPS fix, ready":"Waiting for GPS fix";
 if(trkView==0){for(auto &t:trkList){mm::State::TrackRow row;row.when=trkLocal(t.startT);row.meta=trkMeta(t.sum);row.live=t.live;v.trkRows.push_back(row);}v.trkTotal=int(trkList.size());return;}
 // Detail: the track is read once (statistics and a thinned line), again after the Maps app dropped it.
 auto sel=device::tracksSelected();if(!sel||sel->name!=trkSelected){device::tracksSelect(trkSelected);sel=device::tracksSelected();}
 if(!sel){trkView=0;v.trkView=0;return;}
 const auto &s=sel->stats;v.trkLive=sel->live;v.trkTitle=s.startT?trkLocal(s.startT):sel->name;
 const bool timed=s.durationS()>0;
 v.trkStats={{"DISTANCE",mm_track::distanceLabel(s.distanceM)},{"DURATION",timed?mm_track::durationLabel(s.durationS()):"--"},{"ELEV GAIN","+"+std::to_string(s.gainM)+" m"},{"MAX SPEED",timed?mm_track::speedLabel(s.maxSpeedMps):"--"},{"POINTS",std::to_string(s.points)},{"AVG SPEED",timed?mm_track::speedLabel(s.distanceM/s.durationS()):"--"}};
 for(auto &p:mm_track::shapeFit(sel->line,mm::TracksShape.w,mm::TracksShape.h,8))v.trkShape.push_back({p.x,p.y});
}
static void trkToggle(){if(device::tracksState().recording){device::tracksStop();notice="";}else notice=device::tracksStart()?"":"No SD card";trkRefresh();render();}
static void trkOpenTrack(int index){if(index<0||index>=int(trkList.size()))return;trkSelected=trkList[index].name;if(!device::tracksSelect(trkSelected)){trkSelected.clear();notice="Track unreadable";render();return;}trkView=1;trkFocus=0;notice="";render();}
// View on map: fit the track into the Maps app, which draws the selected track in its own style.
static void trkViewOnMap(){
 auto &line=device::tracksMapLine();if(line.empty()){notice="No route yet";render();return;}
 std::vector<double> lat,lon;for(auto &p:line){lat.push_back(mm_track::deg(p.lat));lon.push_back(mm_track::deg(p.lon));}
 double clat=0,clon=0;int zoom=device::mapFitZoom(lat.data(),lon.data(),int(lat.size()),mm::MapPageArea.w,mm::MapPageArea.h,40,device::MapMaxZoom,clat,clon);
 openSection(9,false);mapView.lat=clat;mapView.lon=clon;mapView.zoom=constrain(zoom,device::MapMinZoom,device::MapMaxZoom);mapView.follow=false;mapView.ready=true;device::mapTilesForget();render();}
static void trkExport(){std::string out;bool ok=device::tracksExportGpx(trkSelected,out);if(ok)out=out.substr(out.rfind('/')+1);notice=ok?("Saved "+out).c_str():out.c_str();render();}
static void trkDelete(){if(device::tracksRemove(trkSelected)){trkSelected.clear();trkView=0;notice="";trkRefresh();}else{trkView=1;notice="Delete failed";}render();}
static void trkAction(int i){if(i==0)trkViewOnMap();else if(i==1)trkExport();else if(device::tracksState().recording&&trkList.size()&&device::tracksSelected()&&device::tracksSelected()->live)trkToggle();else{trkView=2;trkFocus=1;notice="";render();}}
static void trkSetting(int i,int dir){
 auto s=device::tracksSettings();
 if(i==0)s.autoStart=!s.autoStart;else if(i==1)s.showOnMap=!s.showOnMap;else if(i==2)s.keepGps=!s.keepGps;else if(i==3)s.intervalS=mm_track::stepChoice(mm_track::Intervals,s.intervalS,dir);else s.minDistM=mm_track::stepChoice(mm_track::Distances,s.minDistM,dir);
 notice=device::tracksSetSettings(s)?"":"Save failed";render();}
static void trkMove(int d){int n=int(trkList.size());trkFocus=constrain(trkFocus+d,0,n);if(trkFocus>0){int idx=trkFocus-1;if(idx<trkScroll)trkScroll=idx;else if(idx>=trkScroll+mm::TrackRows)trkScroll=idx-mm::TrackRows+1;}else trkScroll=0;render();}
bool tracksBack(){
 if(deviceLocked||screen!=Screen::Section||section!=mm::TracksAppSection)return false;
 if(trkView==2){trkView=1;trkFocus=2;notice="";render();return true;}
 if(trkView==1){int index=0;for(int i=0;i<int(trkList.size());i++)if(trkList[i].name==trkSelected)index=i;trkView=0;trkFocus=trkList.empty()?0:index+1;trkSelected.clear();device::tracksDeselect();notice="";trkRefresh();trkMove(0);return true;}
 return false;}
bool tracksKey(char c){
 if(deviceLocked||screen!=Screen::Section||(section!=mm::TracksAppSection&&section!=mm::TracksSection))return false;
 const bool down=c=='s'||c=='j',up=c=='w'||c=='k',next=c=='d'||c=='l',prev=c=='h',ok=c=='\n'||c==' ';
 if(!(down||up||next||prev||ok))return false;
 if(section==mm::TracksSection){
  if(down||up){trkFocus=constrain(trkFocus+(down?1:-1),0,mm::TracksSettingCount);render();return true;}
  if(trkFocus==0){trkToggle();return true;}
  int row=trkFocus-1;if(row<3)trkSetting(row,1);else trkSetting(row,prev?-1:1);return true;}
 if(trkView==0){
  if(down||up){trkMove(down?1:-1);return true;}
  if(trkFocus==0)trkToggle();else trkOpenTrack(trkFocus-1);return true;}
 if(trkView==2){if(ok&&trkFocus==0)trkDelete();else if(ok)tracksBack();else{trkFocus=1-trkFocus;render();}return true;}
 if(ok){trkAction(trkFocus);return true;}
 trkFocus=constrain(trkFocus+((down||next)?1:-1),0,mm::TrackActionCount-1);render();return true;}
bool tracksTap(int x,int y){
 if(deviceLocked||screen!=Screen::Section||(section!=mm::TracksAppSection&&section!=mm::TracksSection)||mm::BackButton.contains(x,y))return false;
 if(section==mm::TracksSection){
  if(mm::SettingsSwitch.contains(x,y)){trkFocus=0;trkToggle();return true;}
  for(int i=0;i<mm::TracksSettingCount;i++)if(mm::tracksSettingRect(i).contains(x,y)){trkFocus=i+1;trkSetting(i,i<3?1:mm::stepRowTap(mm::tracksSettingRect(i),x));return true;}
  return true;}
 if(trkView==0){
  if(mm::TracksRecord.contains(x,y)){trkFocus=0;trkToggle();return true;}
  for(int i=0;i<mm::TrackRows;i++)if(mm::trackRowRect(i).contains(x,y)){trkFocus=trkScroll+i+1;trkOpenTrack(trkScroll+i);return true;}
  return true;}
 if(trkView==2){if(mm::TracksDeleteYes.contains(x,y))trkDelete();else if(mm::TracksDeleteNo.contains(x,y))tracksBack();return true;}
 for(int i=0;i<mm::TrackActionCount;i++)if(mm::trackActionRect(i).contains(x,y)){trkFocus=i;trkAction(i);return true;}
 return true;}
// A swipe pages the list.
bool tracksScroll(int dy){
 if(deviceLocked||screen!=Screen::Section||section!=mm::TracksAppSection||trkView!=0)return false;
 trkScroll=constrain(trkScroll+(dy<0?mm::TrackRows:-mm::TrackRows),0,std::max(0,int(trkList.size())-mm::TrackRows));render();return true;}
// While recording on a Tracks page: repaint when points were added, at most every 15 s (e-ink).
void tracksUiLoop(){
 static uint32_t at=0,shown=0;if(deviceLocked||screen!=Screen::Section||(section!=mm::TracksAppSection&&section!=mm::TracksSection))return;
 auto st=device::tracksState();if(!st.recording){shown=0;return;}if(st.points==shown||millis()-at<15000)return;shown=st.points;at=millis();
 if(section==mm::TracksAppSection&&trkView==0)trkRefresh();render();}
// Serial (tests): "tracks status|start|stop|list|export <stem>|delete <stem>".
bool tracksSerial(const String &s){
 if(!s.startsWith("tracks"))return false;auto st=device::tracksState();
 if(s=="tracks start"){Serial.printf("TRACKS start=%d\n",device::tracksStart());}
 else if(s=="tracks stop"){device::tracksStop();Serial.println("TRACKS stopped");}
 else if(s=="tracks list"){for(auto &t:device::tracksList(64))Serial.printf("TRACK %s start=%lu points=%lu distance_m=%lu duration_s=%lu gain_m=%u closed=%d live=%d\n",t.name.c_str(),(unsigned long)t.startT,(unsigned long)t.sum.points,(unsigned long)t.sum.distanceM,(unsigned long)t.sum.durationS,t.sum.gainM,t.closed,t.live);Serial.println("TRACKS_END");}
 else if(s.startsWith("tracks export ")){std::string out;bool ok=device::tracksExportGpx(s.substring(14).c_str(),out);Serial.printf("TRACKS export=%d %s\n",ok,out.c_str());}
 else if(s.startsWith("tracks delete ")){Serial.printf("TRACKS delete=%d\n",device::tracksRemove(s.substring(14).c_str()));}
 else if(s.startsWith("tracks cfg ")){int interval=0,distance=0;auto cfg=device::tracksSettings();bool ok=sscanf(s.c_str(),"tracks cfg %d %d",&interval,&distance)==2;if(ok){cfg.intervalS=uint16_t(interval);cfg.minDistM=uint16_t(distance);ok=device::tracksSetSettings(cfg);}cfg=device::tracksSettings();Serial.printf("TRACKS cfg=%d interval=%u distance=%u auto=%d map=%d keep_gps=%d\n",ok,cfg.intervalS,cfg.minDistM,cfg.autoStart,cfg.showOnMap,cfg.keepGps);}
 else Serial.printf("TRACKS recording=%d sd=%d file=%d points=%lu distance_m=%.0f name=%s error=%s\n",st.recording,st.sd,st.fileOpen,(unsigned long)st.points,st.distanceM,st.name.c_str(),st.error.c_str());
 return true;}
