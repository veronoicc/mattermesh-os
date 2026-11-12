#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/selftest_layout.h"
int main(){
 auto inside=[](mm::Rect r){return r.x>=mm::Margin&&r.x+r.w<=mm::W-mm::Margin&&r.y>=mm::Top&&r.y+r.h<=mm::Bottom;};
 auto apart=[](mm::Rect a,mm::Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 auto within=[](mm::Rect a,mm::Rect b){return a.x>=b.x&&a.y>=b.y&&a.x+a.w<=b.x+b.w&&a.y+a.h<=b.y+b.h;};
 // Page targets: the visible rows and the three buttons are inside the content area and never overlap; the totals card does not touch them.
 std::vector<mm::Rect> page;for(int i=0;i<mm::SelfTestRows;i++)page.push_back(mm::selfTestRow(i));for(int i=0;i<mm::SelfTestButtons;i++)page.push_back(mm::selfTestButton(i));
 for(size_t i=0;i<page.size();i++){assert(inside(page[i]));for(size_t j=i+1;j<page.size();j++)assert(apart(page[i],page[j]));}
 assert(inside(mm::SelfTestSummary));for(auto t:page)assert(apart(mm::SelfTestSummary,t));
 assert(inside(mm::SelfTestScrollBar));for(auto t:page)assert(apart(mm::SelfTestScrollBar,t)||t.y>=mm::SelfTestScrollBar.y+mm::SelfTestScrollBar.h);
 // The prompt card covers exactly the list, the touch target sits inside it clear of the buttons.
 assert(inside(mm::SelfTestPrompt)&&within(mm::SelfTestTarget,mm::SelfTestPrompt)&&mm::SelfTestTarget.y>=mm::SelfTestPrompt.y+48&&mm::SelfTestTarget.y+mm::SelfTestTarget.h<=mm::SelfTestPrompt.y+mm::SelfTestPrompt.h-24);
 for(int i=0;i<mm::SelfTestButtons;i++)assert(apart(mm::SelfTestPrompt,mm::selfTestButton(i)));
 // Options sheet: inside the content area, rows apart and inside the panel.
 assert(inside(mm::SelfTestPopup));
 for(int i=0;i<mm::SelfTestOptionRows;i++){auto r=mm::selfTestOptionRow(i);assert(inside(r)&&within(r,mm::SelfTestPopup));for(int j=i+1;j<mm::SelfTestOptionRows;j++)assert(apart(r,mm::selfTestOptionRow(j)));}
 GFXcanvas1 canvas(mm::W,mm::H);mm::Renderer renderer(canvas);mm::State s;s.page=mm::Page::SelfTest;
 auto shot=[&](){renderer.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+(mm::W*mm::H+7)/8);};
 auto &t=s.selftest;auto empty=shot();
 // A full list: every status glyph, long labels and details, more rows than fit.
 const selftest::Status all[]={selftest::Status::Pass,selftest::Status::Fail,selftest::Status::Skip,selftest::Status::Running,selftest::Status::Input,selftest::Status::Idle};
 for(int i=0;i<20;i++)t.rows.push_back({"Check number "+std::to_string(i)+" with a long label that must be cut","detail text of the check "+std::to_string(i)+" that is also rather long for the row",int(all[i%6]),uint32_t(i*7)});
 t.pass=4;t.fail=3;t.skip=3;t.notRun=10;t.line="Ready: 20 checks";
 auto base=shot();assert(base!=empty);
 // The canvas follows the state: counts, progress line, toast, scroll, row focus, each button focus, STOP while running.
 t.pass=5;assert(shot()!=base);t.pass=4;
 t.line="Running 4/20: SX1262";assert(shot()!=base);t.line="Ready: 20 checks";
 t.toast="Saved selftest-20261112-093005.txt";assert(shot()!=base);t.toast="";
 t.first=7;assert(shot()!=base);t.first=0;
 t.focus=2;auto focused=shot();assert(focused!=base);t.focus=0;
 for(int b=0;b<mm::SelfTestButtons;b++){t.button=b;assert(shot()!=base);}t.button=-1;
 t.busy=true;assert(shot()!=base);t.busy=false;
 for(int st=0;st<6;st++){t.rows[0].status=st;shot();}t.rows[0].status=int(selftest::Status::Pass);
 t.rows[0].status=int(selftest::Status::Fail);auto failed=shot();assert(failed!=base);t.rows[0].status=int(selftest::Status::Pass);
 // Interactive prompts: the key prompt and the touch prompt differ from each other and from the list.
 t.prompt=1;t.promptTitle="Keyboard test";t.promptText="Press any key on the keyboard";t.promptSecs=12;auto key=shot();assert(key!=base);
 t.prompt=2;t.promptTitle="Touch test";t.promptText="Tap the square";auto touch=shot();assert(touch!=key&&touch!=base);
 t.promptSecs=11;assert(shot()!=touch);t.prompt=0;
 // Options sheet: every focus row, both switch states.
 t.options=true;auto opt=shot();assert(opt!=base);
 t.optionFocus=1;assert(shot()!=opt);t.optionFocus=0;t.interactive=false;assert(shot()!=opt);t.interactive=true;t.output=false;assert(shot()!=opt);t.output=true;t.options=false;
 // Extremes: no rows, one row, exactly a full page.
 t.rows.clear();shot();t.rows.push_back({"Only","one",int(selftest::Status::Pass),1});shot();while(t.rows.size()<size_t(mm::SelfTestRows))t.rows.push_back({"Row","",int(selftest::Status::Skip),0});shot();
 std::cout<<"PASS: self-test UI (targets inside and apart, prompts, options sheet, every status/focus/button renders, canvas follows the state)\n";
}
