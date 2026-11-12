#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/navigation_catalog.h"
int main(){
 using namespace mm;
 // The shared contract: the Settings row opens the page this slice draws.
 int row=-1;for(int i=0;i<SettingsCount;i++)if(SettingsItems[i].section==QuickMessagesSection)row=i;assert(row>=0);
 auto inside=[](Rect r){return r.x>=Margin&&r.x+r.w<=W-Margin&&r.y>=Top&&r.y+r.h<=Bottom;};
 auto apart=[](Rect a,Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 auto disjoint=[&](const std::vector<Rect> &page){for(size_t i=0;i<page.size();i++){assert(inside(page[i]));for(size_t j=i+1;j<page.size();j++)assert(apart(page[i],page[j]));}};
 // Touch targets: inside the content area, none overlapping, the hint line clear of them.
 {std::vector<Rect> page={QuickKeyRow,QuickModeRow,QuickAdd,QuickReset};for(int i=0;i<QuickRows;i++)page.push_back(quickRowRect(i));for(int i=0;i<QuickActionCount;i++)page.push_back(quickActionRect(i));disjoint(page);
  assert(quickRowRect(QuickRows-1).y+quickRowRect(QuickRows-1).h<=QuickCountY&&QuickCountY+13<=quickActionRect(0).y&&quickActionRect(0).y+quickActionRect(0).h<=QuickAdd.y);
  assert(QuickRows*2>=QuickPresetMax);}// two pages hold all eight
 disjoint({QuickConfirmYes,QuickConfirmNo});
 {std::vector<Rect> pick={QuickPickCancel};for(int i=0;i<QuickPickRows;i++)pick.push_back(quickPickRect(i));disjoint(pick);
  Rect last=quickPickRect(QuickPickRows-1);assert(last.y+last.h<=QuickPickNoticeY&&QuickPickNoticeY+13<=QuickPickCancel.y&&QuickPickCancel.y+QuickPickCancel.h<=Bottom-6);assert(QuickPickRows==QuickPresetMax);}
 GFXcanvas1 canvas(W,H);Renderer r(canvas);
 auto shot=[&](const State &s){r.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+(W+7)/8*H);};
 auto differs=[&](const State &a,const State &b){return shot(a)!=shot(b);};
 auto rows=[](int n){std::vector<QuickView::Row> v;const char *t[]={"OK","On my way","At camp","Need help","Call me","Where are you?","Back soon","Wait for me"};for(int i=0;i<n;i++)v.push_back({t[i],i==3});return v;};
 // ---- Settings page
 State s;s.page=Page::QuickMessages;s.quick.rows=rows(6);
 State empty=s;empty.quick.rows.clear();assert(differs(s,empty));
 State f=s;f.quick.focus=3;assert(differs(s,f));
 State fk=s;fk.quick.focus=1;assert(differs(s,fk));State fa=s;fa.quick.focus=2+6;assert(differs(s,fa));State fr=s;fr.quick.focus=3+6;assert(differs(fa,fr));
 State key=s;key.quick.keyLabel="Alt+Q";assert(differs(s,key));State mode=s;mode.quick.modeLabel="Insert";assert(differs(s,mode));
 State pin=s;pin.quick.rows[0].pin=true;assert(differs(s,pin));
 State text=s;text.quick.rows[1].text="On my way, 10 min";assert(differs(s,text));
 State scrolled=s;scrolled.quick.scroll=2;assert(differs(s,scrolled));
 State ed=s;ed.quick.focus=3;ed.quick.editing=1;ed.quick.input="Typing";assert(differs(f,ed));State ed2=ed;ed2.quick.input="Typing more";assert(differs(ed,ed2));
 State add=s;add.quick.rows.push_back({"",false});add.quick.editing=6;add.quick.focus=8;add.quick.input="N";assert(differs(s,add));
 State note=s;note.notice="Save failed";assert(differs(s,note));
 State del=s;del.quick.focus=3;State conf=del;conf.quick.confirm=1;assert(differs(del,conf));State rst=del;rst.quick.confirm=2;assert(differs(conf,rst));State cf=conf;cf.quick.confirmFocus=1;assert(differs(conf,cf));
 // ---- the Settings list row detail
 {State l;l.page=Page::Settings;l.settingsFocus=row;l.settingsOffset=row;State sm=l;sm.quick.summary="6 messages / Alt+Q";assert(differs(l,sm));}
 // ---- the picker over a chat
 {State c;c.page=Page::Chat;c.chatName="Public";c.quick.rows=rows(6);State p=c;p.quick.picker=true;assert(differs(c,p));
  State p1=p;p1.quick.pickFocus=2;assert(differs(p,p1));State pn=p;pn.quick.pickNotice="No GPS fix";assert(differs(p,pn));State pm=p;pm.quick.modeLabel="Insert";assert(differs(p,pm));
  State pp=p;pp.quick.rows[0].pin=true;assert(differs(p,pp));State pe=p;pe.quick.rows.clear();assert(differs(p,pe));State pf=p;pf.quick.rows=rows(8);assert(differs(p,pf));}
 std::cout<<"PASS: quick messages UI: touch targets inside the content area and apart (settings, confirm, picker), every state redraws\n";
}
