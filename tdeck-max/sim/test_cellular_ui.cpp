#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
using namespace mm;
int main(){
 auto inside=[](Rect r){return r.x>=Margin&&r.x+r.w<=W-Margin&&r.y>=Top&&r.y+r.h<=Bottom;};
 auto apart=[](Rect a,Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 assert(CellularSwitch.y==SettingsSwitch.y&&CellularSwitch.h==SettingsSwitch.h);
 std::vector<Rect> targets={CellularSwitch,CellularStatus,CellularUp,CellularDown};for(int i=0;i<CellVisible;i++)targets.push_back(cellularRow(i));
 for(size_t i=0;i<targets.size();i++){assert(inside(targets[i]));for(size_t j=i+1;j<targets.size();j++)assert(apart(targets[i],targets[j]));}
 assert(CellularLabelY>=CellularStatus.y+CellularStatus.h&&CellularLabelY+13<=cellularRow(0).y);
 // Page buttons step by one window and the windows cover every row.
 {int seen=CellVisible,scroll=0;while(scroll<cellMaxScroll()){int next=cellPageScroll(scroll,true);assert(next>scroll&&next<=seen);scroll=next;seen=scroll+CellVisible;}assert(seen==CellRows);}
 GFXcanvas1 canvas(W,H);Renderer renderer(canvas);
 auto shot=[&](const State &s){renderer.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+W*H/8);};
 auto base=[&](){State s;s.page=Page::Cellular;s.cell.modem=true;s.cell.phase=device::ModemPhase::Registered;s.cell.model="A7682E";s.cell.op="Operator";s.cell.csq=20;s.cell.bars=3;s.cell.rat=3;s.cell.simReady=true;s.cell.ip="10.1.2.3";s.cell.data=device::DataPhase::Online;
  for(int r=0;r<CellRows;r++)s.cell.value[r]=std::string(cellRowLabel(r))+" value";s.cell.mqttState="Connected";s.cell.mqttCounts="12 published / 0 dropped / 3 queued";return s;};
 // Long strings and every phase/mode render without crashing.
 {State s=base();std::string l(60,'W');s.cell.op=l;s.cell.ip=l;s.cell.message=l;s.cell.model=l;s.cell.input=l;for(auto &v:s.cell.value)v=l;s.cell.mqttState=l;s.cell.mqttCounts=l;
  for(int p=0;p<=int(device::ModemPhase::Failed);p++)for(int d=0;d<=int(device::DataPhase::Failed);d++)for(int m=0;m<device::NetModeCount;m++){s.cell.phase=device::ModemPhase(p);s.cell.data=device::DataPhase(d);s.cell.value[CellNetwork]=device::netModeName(device::NetMode(m));
   for(int sc=0;sc<=cellMaxScroll();sc+=cellMaxScroll()){s.cell.scroll=sc;for(int f=0;f<CellFocusCount;f+=5){s.cell.focus=f;shot(s);}}
   for(int e=0;e<CellRows;e++){s.cell.editing=e;shot(s);}s.cell.editing=-1;}}
 // The rendering changes with the state.
 {State a=base(),b=base();auto ref=shot(a);
  b.cell.modem=false;b.cell.phase=device::ModemPhase::Off;assert(shot(b)!=ref);
  b=base();b.cell.csq=0;b.cell.bars=0;assert(shot(b)!=ref);b=base();b.cell.csq=31;b.cell.bars=4;assert(shot(b)!=ref);
  b=base();b.cell.csq=99;assert(shot(b)!=ref);b=base();b.cell.message="SIM failure";assert(shot(b)!=ref);b=base();b.cell.rat=1;assert(shot(b)!=ref);b=base();b.cell.data=device::DataPhase::Failed;assert(shot(b)!=ref);
  b=base();b.cell.scroll=cellMaxScroll();assert(shot(b)!=ref);b=base();b.cell.scroll=CellVisible;assert(shot(b)!=ref);
  b=base();b.cell.editing=CellPin;b.cell.input="12";auto e1=shot(b);assert(e1!=ref);b.cell.input="123";assert(shot(b)!=e1);
  b=base();b.cell.value[CellApn]="other";assert(shot(b)!=ref);
  // Each switch row shows its state (rows visible at the scroll that holds them).
  for(int row:{CellRoaming,CellMqtt,CellTls,CellDownlink,CellDlSend,CellDlPublish}){State t=base();t.cell.scroll=std::min(row,cellMaxScroll());auto off=shot(t);t.cell.on[row]=true;assert(shot(t)!=off);}
  // The generate row shows the new token (revealed) in place of its hint, and the token row shows its masked value.
  {State t=base();t.cell.scroll=std::min(int(CellDlToken),cellMaxScroll());auto hint=shot(t);t.cell.on[CellDlGenerate]=true;t.cell.value[CellDlGenerate]="ABCDEFGHJKLMNPQRSTUVWXYZ";assert(shot(t)!=hint);t=base();t.cell.scroll=std::min(int(CellDlToken),cellMaxScroll());t.cell.value[CellDlToken]="**********";assert(shot(t)!=hint);}
  {State t=base();t.cell.scroll=cellMaxScroll();auto last=shot(t);t.cell.mqttState="Failed";assert(shot(t)!=last);t=base();t.cell.scroll=cellMaxScroll();t.cell.mqttCounts="1 published";assert(shot(t)!=last);}
  b=base();b.cell.focus=1;auto f1=shot(b);b.cell.focus=2;assert(shot(b)!=f1&&f1!=ref);
 }
 // The focused row is visible and drawn as an inverted card at every focus position, from any scroll.
 for(int start:{0,cellMaxScroll()})for(int f=0;f<CellFocusCount;f++){State s=base();s.cell.focus=f;s.cell.scroll=cellFollow(start,f);renderer.draw(s);int row=cellFocusRow(f);
  if(row<0){assert(canvas.getPixel(CellularSwitch.x+1,CellularSwitch.y+CellularSwitch.h/2));continue;}
  assert(row>=s.cell.scroll&&row<s.cell.scroll+CellVisible);Rect r=cellularRow(row-s.cell.scroll);assert(canvas.getPixel(r.x+2,r.y+r.h/2));
  for(int o=0;o<CellVisible;o++)if(o!=row-s.cell.scroll&&s.cell.scroll+o!=CellMqttInfo){Rect q=cellularRow(o);assert(!canvas.getPixel(q.x+2,q.y+q.h/2));}}
 {State s=base();s.cell.focus=1;renderer.draw(s);assert(!canvas.getPixel(CellularSwitch.x+1,CellularSwitch.y+CellularSwitch.h/2));}
 std::cout<<"PASS: cellular page touch targets inside and apart, switch card shared, renders every phase/mode with long strings, state changes repaint, focused row visible at every position\n";
}
