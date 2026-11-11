#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/rpt_layout.h"
// RPT Scanner pages: every touch target inside the content area and apart from the others (also with the options sheet,
// the DM PATH picker, the password page and the console open); every page draws with no data and with full data, and
// the canvas changes with the state.
using namespace mm;
static std::vector<uint8_t> shot(const GFXcanvas1 &g){return std::vector<uint8_t>(g.getBuffer(),g.getBuffer()+W*H/8);}
int main(){
 auto inside=[](Rect r){return r.x>=Margin&&r.x+r.w<=W-Margin&&r.y>=Top&&r.y+r.h<=Bottom;};
 auto apart=[](Rect a,Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 std::vector<std::pair<const char*,std::vector<Rect>>> pages;
 {std::vector<Rect> list={RptScan};for(int i=0;i<RptTabCount;i++)list.push_back(rptTabRect(i));for(int i=0;i<RptRows;i++)list.push_back(rptRowRect(i));pages.push_back({"list",list});}
 {std::vector<Rect> menu;for(int i=0;i<RptMenuRows;i++)menu.push_back(rptMenuRow(i));pages.push_back({"options",menu});assert(inside(RptMenuPopup));for(int i=0;i<RptMenuRows;i++){auto r=rptMenuRow(i);assert(r.x>=RptMenuPopup.x&&r.x+r.w<=RptMenuPopup.x+RptMenuPopup.w&&r.y>=RptMenuPopup.y&&r.y+r.h<=RptMenuPopup.y+RptMenuPopup.h);}}
 {std::vector<Rect> detail;for(int i=0;i<RptActionCount;i++)detail.push_back(rptAction(i));pages.push_back({"repeater",detail});assert(inside(RptInfo)&&inside(RptAnswer)&&apart(RptInfo,RptDm)&&apart(RptDm,RptAnswer)&&RptAnswer.h>=7*RptLineH+8);}
 {std::vector<Rect> pick;for(int i=0;i<RptPickRows;i++)pick.push_back(rptPickRow(i));for(int i=0;i<3;i++)pick.push_back(rptPickButton(i));pages.push_back({"picker",pick});}
 pages.push_back({"password",{RptShow,RptCancel,RptGo,RptField}});
 {std::vector<Rect> console;for(int i=0;i<RptQuickCount;i++)console.push_back(rptQuick(i));console.push_back(RptOut);pages.push_back({"console",console});assert(inside(RptBar)&&RptInputY>=RptOut.y+RptOut.h&&RptInputY+RptLineH<=RptBar.y&&RptOut.h/RptLineH>=12);}
 for(auto &page:pages)for(size_t i=0;i<page.second.size();i++){assert(inside(page.second[i]));for(size_t j=i+1;j<page.second.size();j++)assert(apart(page.second[i],page.second[j]));}
 // The list rows end above the bottom margin and the header menu button is the Compose button of every page header.
 assert(rptRowRect(RptRows-1).y+rptRowRect(RptRows-1).h<=Bottom&&RptListY>=RptScan.y+RptScan.h);assert(RptMenu.x==ComposeButton.x&&RptMenu.y==ComposeButton.y);
 assert(RptState.x>=RptScan.x+RptScan.w&&RptState.x+RptState.w<=W-Margin);
 GFXcanvas1 canvas(W,H);Renderer renderer(canvas);State s;s.page=Page::RptScanner;
 auto draw=[&](){renderer.draw(s);return shot(canvas);};
 // Empty state of every page.
 for(int mode=0;mode<4;mode++){s.rpt=RptView();s.rpt.mode=mode;draw();}
 s.rpt=RptView();s.rpt.radioOn=false;auto off=draw();s.rpt.radioOn=true;auto empty=draw();assert(off!=empty);
 // Full list: a row window, a long name, no distance, and a scan running.
 s.rpt=RptView();s.rpt.total=12;
 for(int i=0;i<RptRows;i++){RptRowView row;row.name=i==1?"A repeater with a very long name that must be cut":"Hill "+std::to_string(i);row.signal="SNR 8.5 / -92 dBm";row.age="direct / 3m";row.distance=i==2?"":"2.4 km NE";row.live=i%2==0;s.rpt.rows.push_back(row);}
 s.rpt.status="12 repeaters / 3 stale hidden";auto list=draw();assert(list!=empty);
 s.rpt.focus=2;auto focused=draw();assert(focused!=list);
 s.rpt.sort=2;auto sorted=draw();assert(sorted!=focused);
 s.rpt.scanning=true;s.rpt.status="Scanning 8s / 3 found";auto scanning=draw();assert(scanning!=sorted);
 s.rpt.first=7;s.rpt.focus=9;auto scrolled=draw();assert(scrolled!=scanning);
 // Options sheet: each option changes the picture.
 s.rpt.menu=true;auto menu=draw();assert(menu!=scrolled);
 for(int f=0;f<RptMenuRows;f++){s.rpt.menuFocus=f;draw();}
 s.rpt.menuFocus=0;s.rpt.defaultSort=3;auto sort=draw();assert(sort!=menu);s.rpt.hideHours=6;auto hide=draw();assert(hide!=sort);s.rpt.listenSec=20;auto listen=draw();assert(listen!=hide);s.rpt.showRssi=false;auto rssi=draw();assert(rssi!=listen);
 // Repeater page, with the answers each action gives.
 s.rpt=RptView();s.rpt.mode=1;s.rpt.name="Hilltop";s.rpt.key=std::string(64,'a');s.rpt.position="46.12345, 7.12345 / 2.4 km NE";s.rpt.advert="Heard 3m ago / direct";s.rpt.signal="SNR 6.5 in / 4.0 out / RSSI -97";s.rpt.path="Path: 2 hops a1b2>c3d4";s.rpt.answer={"PING tests the direct link"};
 auto page=draw();s.rpt.actionFocus=1;auto focus=draw();assert(focus!=page);s.rpt.loggedIn=true;auto console=draw();assert(console!=focus);s.rpt.loggedIn=false;
 s.rpt.answerState=1;s.rpt.answer={"Status requested"};auto waiting=draw();assert(waiting!=focus);
 s.rpt.answerState=2;s.rpt.answer={"Up 3d 4h 05m / 4012 mV","Noise -112 dBm / RSSI -94 / SNR 8.5","RX 1234 / TX 567 / queue 3","Air TX 321s / RX 654s","Flood TX 100 / RX 300","Direct TX 200 / RX 400","Err 2 / RX err 9 / dups 5+6","an eighth line that has no room"};auto status=draw();assert(status!=waiting);
 s.rpt.key=std::string(16,'b');s.rpt.contact=false;auto prefix=draw();assert(prefix!=status);
 s.rpt.picker=true;auto none=draw();s.rpt.pickRows={"Alice","Bob","Carol","Dave","Eve"};s.rpt.pickTotal=12;auto pick=draw();assert(pick!=none);s.rpt.pickFocus=2;auto picked=draw();assert(picked!=pick);
 // Password page: the typed characters are masked or shown, the state changes the buttons.
 s.rpt=RptView();s.rpt.mode=2;s.rpt.name="Hilltop";auto pw=draw();s.rpt.input="secret";auto typed=draw();assert(typed!=pw);s.rpt.masked=false;auto shown=draw();assert(shown!=typed);s.rpt.waiting=true;auto login=draw();assert(login!=shown);s.rpt.note="No reply: wrong password or out of range";assert(draw()!=login);
 // Console: lines, scrolling, input and the waiting mark.
 s.rpt=RptView();s.rpt.mode=3;auto blank=draw();for(int i=0;i<40;i++)s.rpt.console.push_back(i%2?"< reply line "+std::to_string(i):"> command "+std::to_string(i));auto lines=draw();assert(lines!=blank);
 s.rpt.consoleScroll=10;auto scrolledUp=draw();assert(scrolledUp!=lines);s.rpt.input="get name";auto input=draw();assert(input!=scrolledUp);s.rpt.waiting=true;assert(draw()!=input);
 s.rpt.input=std::string(150,'x');draw();
 std::cout<<"PASS: RPT Scanner touch targets inside the content area and apart (list, options, repeater, picker, password, console), every page and popup draws empty and full, the canvas follows the state\n";
}
