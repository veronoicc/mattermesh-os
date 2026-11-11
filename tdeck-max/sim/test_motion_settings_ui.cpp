#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/navigation_catalog.h"
int main(){
 // The Settings list row points at the page and the page is reachable from the list.
 int row=-1;for(int i=0;i<mm::SettingsCount;i++)if(mm::SettingsItems[i].section==mm::MotionSection)row=i;assert(row>=0);
 // Touch targets: inside the content area and the margins, no two overlapping, shared switch card on top.
 auto inside=[](mm::Rect r){return r.x>=mm::Margin&&r.x+r.w<=mm::W-mm::Margin&&r.y>=mm::Top&&r.y+r.h<=mm::Bottom;};
 auto apart=[](mm::Rect a,mm::Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 assert(mm::MotionToggle.y==mm::SettingsSwitch.y&&mm::MotionToggle.h==mm::SettingsSwitch.h);
 std::vector<mm::Rect> targets;for(int i=0;i<mm::MotionRows;i++)targets.push_back(mm::motionRowRect(i));targets.push_back(mm::MotionHeadingCard);
 for(size_t i=0;i<targets.size();i++){assert(inside(targets[i]));for(size_t j=i+1;j<targets.size();j++)assert(apart(targets[i],targets[j]));}
 // Pills sit inside their rows; the group labels (12 px) fit between the cards above and the rows below.
 for(auto r:{mm::MotionRaise,mm::MotionShare}){auto p=mm::motionPill(r);assert(p.x>=r.x&&p.x+p.w<=r.x+r.w&&p.y>=r.y&&p.y+p.h<=r.y+r.h);}
 assert(mm::Top+49>=mm::SettingsSwitch.y+mm::SettingsSwitch.h&&mm::Top+49+12<=mm::MotionHeadingCard.y);
 assert(mm::MotionRaise.y-15>=mm::MotionHeadingCard.y+mm::MotionHeadingCard.h&&mm::MotionOrientation.y-15>=mm::MotionSensitivityRow.y+mm::MotionSensitivityRow.h);
 // Step rows: the "<" arrow steps back, anywhere else forward (the same convention as the vibration rows).
 assert(mm::stepRowTap(mm::MotionSensitivityRow,mm::MotionSensitivityRow.x+mm::MotionSensitivityRow.w-44)==-1&&mm::stepRowTap(mm::MotionSensitivityRow,mm::MotionSensitivityRow.x+20)==1);
 GFXcanvas1 canvas(mm::W,mm::H);mm::Renderer renderer(canvas);
 auto shot=[&](const mm::State &s){renderer.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+mm::W*mm::H/8);};
 mm::State s;s.page=mm::Page::Motion;s.motionEnabled=true;s.motionPhase=3;auto base=shot(s);
 // Every state the page can show draws something different from the previous one.
 auto differs=[&](mm::State t){auto now=shot(t);bool d=now!=base;base=now;return d;};
 {auto t=s;t.motionEnabled=false;t.motionPhase=0;assert(differs(t));}
 {auto t=s;t.motionPhase=2;t.motionLoad=40;assert(differs(t));}
 {auto t=s;t.motionPhase=2;t.motionLoad=80;assert(differs(t));}
 {auto t=s;t.motionPhase=4;t.motionError="no answer at 0x28";assert(differs(t));}
 {auto t=s;t.motionPhase=5;t.motionError="firmware upload failed";assert(differs(t));}
 {auto t=s;t.motionHeadingValid=true;t.motionHeading=128;t.motionAnchorAge=125000;assert(differs(t));}
 {auto t=s;t.motionHeadingValid=true;t.motionHeading=300;t.motionAnchorAge=125000;assert(differs(t));}
 {auto t=s;t.motionRaise=true;assert(differs(t));}
 {auto t=s;t.motionRaise=true;t.motionSensitivity=0;assert(differs(t));}
 {auto t=s;t.motionRaise=true;t.motionSensitivity=2;assert(differs(t));}
 {auto t=s;t.motionHeadingUp=true;assert(differs(t));}
 {auto t=s;t.motionTelemetry=true;assert(differs(t));}
 for(int focus=0;focus<mm::MotionRows;focus++){auto t=s;t.motionFocus=focus;assert(differs(t));}
 // Settings list shows the row with a detail that follows the state, and the map and radar draw the heading.
 {mm::State list;list.page=mm::Page::Settings;list.settingsFocus=row;list.settingsOffset=std::min(row,mm::settingsMaxScroll());assert(mm::settingsVisible(row,list.settingsOffset));base=shot(list);
  auto t=list;t.motionHeadingValid=true;t.motionHeading=90;assert(differs(t));t=list;t.motionEnabled=false;assert(differs(t));t=list;t.motionPhase=4;assert(differs(t));}
 {mm::State map;map.page=mm::Page::Maps;map.mapFull=true;map.mapSelfX=120;map.mapSelfY=160;base=shot(map);
  auto t=map;t.mapHeading=0;assert(differs(t));t.mapHeading=90;assert(differs(t));t.mapHeading=270;assert(differs(t));}
 {mm::State home;home.page=mm::Page::Home;home.homeMain=mm::MainRadar;home.widget.position=true;home.main.here=true;home.main.nearby.push_back({"A","300 m NE",300,45,true});base=shot(home);
  auto t=home;t.main.northAngle=90;assert(differs(t));}
 std::cout<<"PASS: Motion / compass page - targets inside the content area and disjoint, step rows, every state redraws, list detail, map heading arrow, heading-up radar\n";
}
