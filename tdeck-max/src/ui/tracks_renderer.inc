// GPS tracks, part of the OS: the Tracks app (section 49) and Settings > GPS tracks (section 50). Included by renderer.cpp.
void Renderer::tracksPages(const State &s){
 const bool settingsPage=s.page==Page::TracksSettings;
 shell(s,settingsPage?"GPS tracks":s.trkView?"Track":"Tracks",true,Icon::Menu);g.fillRect(ChatMenu.x,ChatMenu.y,ChatMenu.w,ChatMenu.h-1,BG);
 // A rounded pill (ON / OFF, START / STOP, REC) in the ink of its card; on = filled.
 auto pill=[&](Rect r,const std::string &text,bool on,uint16_t ink){uint16_t paper=ink==FG?BG:FG;if(on)g.fillRoundRect(r.x,r.y,r.w,r.h,r.h/2,ink);else g.drawRoundRect(r.x,r.y,r.w,r.h,r.h/2,ink);c.centered(r.x,r.y+(r.h-13)/2,r.w,text,Font::Bold,on?paper:ink);};
 if(settingsPage){
  c.toggleRow(SettingsSwitch,Icon::GPS,"Recording",s.trkStatus,s.trkRecording);
  if(s.trkFocus==0)g.drawRoundRect(SettingsSwitch.x+2,SettingsSwitch.y+2,SettingsSwitch.w-4,SettingsSwitch.h-4,Radius,FG);
  c.sectionLabel(SettingsSectionY,"TRACKING");c.sectionLabel(TracksSamplingLabelY,"SAMPLING","BOTH MUST BE MET");
  const char *titles[]={"Auto-start","Show on map","Keep GPS on","Interval","Min distance"},*hints[]={"When GPS has a fix","Draw the track in Maps","While recording","Time between points","Distance apart"};
  const bool values[]={s.trkAuto,s.trkMap,s.trkKeepGps};
  for(int i=0;i<TracksSettingCount;i++){
   Rect r=tracksSettingRect(i);bool focus=s.trkFocus==i+1;uint16_t ink=focus?BG:FG;c.card(r,focus);
   if(i<3){c.text(r.x+8,r.y+2,titles[i],Font::Bold,ink,r.w-70);c.text(r.x+8,r.y+17,hints[i],Font::ChatMeta,ink,r.w-70);pill({r.x+r.w-50,r.y+6,44,20},values[i]?"ON":"OFF",values[i],ink);continue;}
   // < value >: a tap on "<" steps back, anywhere else forward (stepRowTap).
   std::string value=i==3?std::to_string(s.trkInterval)+" s":std::to_string(s.trkDistance)+" m";int valueX=r.x+r.w-56-c.width(value,Font::Bold),room=valueX-(r.x+8)-4;
   c.text(r.x+8,r.y+2,titles[i],Font::Bold,ink,room);c.text(r.x+8,r.y+17,hints[i],Font::ChatMeta,ink,room);c.text(valueX,r.y+(r.h-13)/2,value,Font::Bold,ink);
   c.icon(Icon::Back,r.x+r.w-44,r.y+(r.h-12)/2,12,ink);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12,ink);}
  return;}
 if(!s.trkView){
  // Record card, then the tracks newest first.
  {Rect r=TracksRecord;bool focus=s.trkFocus==0;uint16_t ink=focus?BG:FG;c.card(r,focus);c.icon(Icon::GPS,r.x+8,r.y+10,24,ink);
   c.text(r.x+40,r.y+5,s.trkRecording?"Recording":"Record a track",Font::Bold,ink,r.w-108);c.text(r.x+40,r.y+23,s.trkStatus,Font::ChatMeta,ink,r.w-108);
   pill({r.x+r.w-62,r.y+10,54,24},s.trkRecording?"STOP":"START",s.trkRecording,ink);}
  int n=int(s.trkRows.size());
  for(int i=0;i<TrackRows;i++){int idx=s.trkScroll+i;if(idx>=n)break;const auto &row=s.trkRows[idx];Rect r=trackRowRect(i);bool focus=s.trkFocus==idx+1;uint16_t ink=focus?BG:FG;c.card(r,focus);
   c.text(r.x+8,r.y+3,row.when,Font::Bold,ink,r.w-(row.live?72:30));c.text(r.x+8,r.y+19,row.meta,Font::ChatMeta,ink,r.w-30);
   if(row.live)pill({r.x+r.w-56,r.y+8,36,18},"REC",true,ink);c.icon(Icon::ChevronRight,r.x+r.w-16,r.y+(r.h-12)/2,12,ink);}
  if(!n){const int y=TrackListY+34;c.centered(Margin,y,W-2*Margin,s.trkSd?"No tracks yet":"No SD card",Font::ChatTitle);
   int ty=y+30;for(auto &line:c.lines(s.trkSd?"Record one above, or turn on Auto-start in Settings > GPS tracks.":"Tracks are stored on the SD card.",W-2*Margin-24,Font::ChatMeta)){c.centered(Margin+12,ty,W-2*Margin-24,line,Font::ChatMeta);ty+=14;}}
  else if(n>TrackRows){int first=std::min(s.trkScroll,n-TrackRows);c.text(Margin+2,TracksCountY,std::to_string(first+1)+"-"+std::to_string(first+TrackRows)+" of "+std::to_string(s.trkTotal>n?s.trkTotal:n),Font::ChatMeta,FG,W-2*Margin-4);}
  return;}
 // One track: title, statistics, a sketch of the route, MAP / GPX / DELETE.
 {Rect t=TracksTitle;c.card(t);c.text(t.x+8,t.y+9,s.trkTitle,Font::Bold,FG,t.w-(s.trkLive?72:16));if(s.trkLive)pill({t.x+t.w-52,t.y+7,44,18},"REC",true,FG);}
 for(int i=0;i<TrackStatCount&&i<int(s.trkStats.size());i++){Rect r=trackStatRect(i);c.card(r);c.text(r.x+7,r.y+3,s.trkStats[i].first,Font::ChatMeta,FG,r.w-14);c.text(r.x+7,r.y+19,s.trkStats[i].second,Font::Bold,FG,r.w-14);}
 {Rect r=TracksShape;c.card(r);
  if(s.trkShape.size()>=2){for(size_t i=1;i<s.trkShape.size();i++){int x0=r.x+s.trkShape[i-1].first,y0=r.y+s.trkShape[i-1].second,x1=r.x+s.trkShape[i].first,y1=r.y+s.trkShape[i].second;g.drawLine(x0,y0,x1,y1,FG);g.drawLine(x0,y0+1,x1,y1+1,FG);}
   g.drawCircle(r.x+s.trkShape.front().first,r.y+s.trkShape.front().second,3,FG);g.fillCircle(r.x+s.trkShape.back().first,r.y+s.trkShape.back().second,3,FG);}// start: ring, end: dot
  else c.centered(r.x,r.y+(r.h-13)/2,r.w,"No route yet",Font::ChatMeta);}
 if(!s.notice.empty())c.text(Margin+2,TracksNoticeY,s.notice,Font::ChatMeta,FG,W-2*Margin-4);
 const char *labels[]={"MAP","GPX",s.trkLive?"STOP":"DELETE"};for(int i=0;i<TrackActionCount;i++)c.button(trackActionRect(i),labels[i],s.trkView==1&&s.trkFocus==i);
 if(s.trkView==2){Rect panel=overlayPanel();c.text(Margin+8,Top+15,"Delete this track?",Font::ChatTitle,FG,panel.w-16);c.text(Margin+8,Top+56,s.trkTitle,Font::Bold,FG,panel.w-16);c.text(Margin+8,Top+76,"An exported GPX file is kept.",Font::ChatMeta,FG,panel.w-16);
  c.button(TracksDeleteYes,"DELETE",s.trkFocus==0);c.button(TracksDeleteNo,"CANCEL",s.trkFocus==1);}
}
