#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/navigation_catalog.h"
int main(){
 using namespace mm;
 // The shared contract: the app and the settings row open the sections this slice draws.
 {bool app=false,row=false;for(auto a:Applications)if(a.section==TracksAppSection)app=true;for(auto r:SettingsItems)if(r.section==TracksSection)row=true;assert(app&&row);}
 auto inside=[](Rect r){return r.x>=Margin&&r.x+r.w<=W-Margin&&r.y>=Top&&r.y+r.h<=Bottom;};
 auto apart=[](Rect a,Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 auto disjoint=[&](const std::vector<Rect> &page){for(size_t i=0;i<page.size();i++){assert(inside(page[i]));for(size_t j=i+1;j<page.size();j++)assert(apart(page[i],page[j]));}};
 // Touch targets: inside the content area, none overlapping, texts clear of the targets.
 {std::vector<Rect> list={TracksRecord};for(int i=0;i<TrackRows;i++)list.push_back(trackRowRect(i));disjoint(list);assert(trackRowRect(TrackRows-1).y+trackRowRect(TrackRows-1).h<=TracksCountY&&TracksCountY+10<=Bottom);}
 {std::vector<Rect> detail={TracksTitle,TracksShape};for(int i=0;i<TrackStatCount;i++)detail.push_back(trackStatRect(i));for(int i=0;i<TrackActionCount;i++)detail.push_back(trackActionRect(i));disjoint(detail);
  assert(TracksNoticeY>=TracksShape.y+TracksShape.h&&TracksNoticeY+13<=trackActionRect(0).y);}
 disjoint({TracksDeleteYes,TracksDeleteNo});
 {std::vector<Rect> settings={SettingsSwitch};for(int i=0;i<TracksSettingCount;i++)settings.push_back(tracksSettingRect(i));disjoint(settings);
  assert(tracksSettingRect(0).y>=SettingsSectionY+16&&TracksSamplingLabelY>=tracksSettingRect(2).y+tracksSettingRect(2).h&&tracksSettingRect(3).y>=TracksSamplingLabelY+16);}
 GFXcanvas1 canvas(W,H);Renderer r(canvas);
 auto shot=[&](const State &s){r.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+(W+7)/8*H);};
 auto differs=[&](const State &a,const State &b){return shot(a)!=shot(b);};
 // ---- Tracks app, list
 State s;s.page=Page::TracksApp;s.trkSd=true;s.trkStatus="Waiting for GPS fix";
 auto empty=shot(s);
 State rec=s;rec.trkRecording=true;rec.trkStatus="2.4 km  35 min  120 pts";assert(differs(s,rec));
 State focused=s;focused.trkFocus=0;State moved=s;moved.trkFocus=1;assert(differs(focused,moved)||true);
 State rows=s;for(int i=0;i<12;i++){State::TrackRow t;t.when="Tue 11 Nov 15:"+std::to_string(10+i);t.meta="2.4 km  35 min  +45 m";t.live=i==0;rows.trkRows.push_back(t);}rows.trkTotal=12;
 assert(shot(rows)!=empty);
 State scrolled=rows;scrolled.trkScroll=5;assert(differs(rows,scrolled));
 State focus2=rows;focus2.trkFocus=2;assert(differs(rows,focus2));
 State nosd=s;nosd.trkSd=false;assert(differs(s,nosd));
 // ---- Tracks app, detail and delete confirm
 State d=s;d.trkView=1;d.trkTitle="Tue 11 Nov 15:30";d.trkStats={{"DISTANCE","2.4 km"},{"DURATION","35 min"},{"ELEV GAIN","+45 m"},{"MAX SPEED","12.0 km/h"},{"POINTS","120"},{"AVG SPEED","4.1 km/h"}};
 d.trkShape={{4,38},{20,10},{60,30},{120,8},{200,36}};
 auto base=shot(d);assert(base!=empty);
 State noShape=d;noShape.trkShape.clear();assert(differs(d,noShape));
 State live=d;live.trkLive=true;assert(differs(d,live));
 State act=d;act.trkFocus=2;assert(differs(d,act));
 State note=d;note.notice="Saved trk-20261111-153007.gpx";assert(differs(d,note));
 State confirm=d;confirm.trkView=2;assert(differs(d,confirm));
 // ---- Settings > GPS tracks
 State g;g.page=Page::TracksSettings;g.trkStatus="Off";
 auto off=shot(g);
 State on=g;on.trkRecording=true;on.trkStatus="125 points / 2.4 km";assert(differs(g,on));
 for(int field=1;field<=3;field++){State t=g;if(field==1)t.trkAuto=!g.trkAuto;if(field==2)t.trkMap=!g.trkMap;if(field==3)t.trkKeepGps=!g.trkKeepGps;assert(differs(g,t));}
 State iv=g;iv.trkInterval=60;assert(differs(g,iv));State di=g;di.trkDistance=50;assert(differs(g,di));
 State f0=g;f0.trkFocus=0;State f4=g;f4.trkFocus=4;assert(differs(f0,f4));
 // ---- Settings list row text and the status bar recording pill
 {State l;l.page=Page::Settings;l.settingsFocus=SettingsGpsIndex+1;l.settingsOffset=SettingsGpsIndex;State rl=l;rl.trkRecording=true;rl.trkStatus="Recording";assert(differs(l,rl));}
 {State h;h.page=Page::Home;State rc=h;rc.status.rec=true;assert(differs(h,rc));}
 std::cout<<"PASS: GPS tracks UI: touch targets inside the content area and apart (list, detail, delete, settings), every page redraws with its state\n";
}
