#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
int main(){
 using namespace mm;
 auto inside=[](Rect r){return r.x>=Margin&&r.x+r.w<=W-Margin&&r.y>=Top&&r.y+r.h<=Bottom;};
 auto apart=[](Rect a,Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 auto disjoint=[&](const std::vector<Rect> &page){for(size_t i=0;i<page.size();i++){assert(inside(page[i]));for(size_t j=i+1;j<page.size();j++)assert(apart(page[i],page[j]));}};
 // Touch targets: inside the content area, none overlapping, texts clear of the targets.
 {std::vector<Rect> list={SchedHistoryCard};for(int i=0;i<SchedRows;i++)list.push_back(schedRowRect(i));disjoint(list);assert(schedRowRect(SchedRows-1).y+schedRowRect(SchedRows-1).h<=SchedCountY&&SchedCountY+10<=Bottom);}
 {std::vector<Rect> hist;for(int i=0;i<SchedHistRows;i++)hist.push_back(schedHistRect(i));disjoint(hist);assert(schedHistRect(SchedHistRows-1).y+schedHistRect(SchedHistRows-1).h<=SchedCountY);}
 {std::vector<Rect> item={SchedItemWhen,SchedItemTo,SchedItemText};for(int i=0;i<SchedActionCount;i++)item.push_back(schedActionRect(i));disjoint(item);assert(SchedItemStateY>=SchedItemText.y+SchedItemText.h&&SchedItemStateY+13<=schedActionRect(0).y);}
 disjoint({SchedDeleteYes,SchedDeleteNo});
 {std::vector<Rect> form={SchedFormText,SchedFormCond,SchedFormMinus,SchedFormPlus,SchedFormSave,SchedFormCancel};for(int i=0;i<5;i++)form.push_back(schedFieldRect(i));disjoint(form);
  assert(SchedFormToY>=SchedFormText.y+SchedFormText.h&&SchedFormToY+13<=SchedFormCond.y&&SchedFormLabelY>=SchedFormCond.y+SchedFormCond.h&&schedFieldRect(0).y>=SchedFormLabelY+14);
  assert(SchedFormSummaryY>=SchedFormPlus.y+SchedFormPlus.h&&SchedFormNoticeY>=SchedFormSummaryY+13&&SchedFormNoticeY+13<=SchedFormSave.y);}
 GFXcanvas1 canvas(W,H);Renderer r(canvas);
 auto shot=[&](const State &s){r.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+(W+7)/8*H);};
 auto differs=[&](const State &a,const State &b){return shot(a)!=shot(b);};
 // ---- list
 State s;s.page=Page::Scheduled;auto empty=shot(s);
 State noClock=s;noClock.sched.clock=false;assert(differs(s,noClock));
 State rows=s;for(int i=0;i<12;i++){ScheduledView::Row row;row.when="Tue 11 Nov 15:"+std::to_string(10+i);row.meta="Alice: on my way, see you soon";row.retry=i==0;rows.sched.rows.push_back(row);}rows.sched.total=12;
 assert(differs(s,rows));
 State scrolled=rows;scrolled.sched.scroll=5;assert(differs(rows,scrolled));
 State focus1=rows;focus1.sched.focus=1;State focus2=rows;focus2.sched.focus=2;assert(differs(rows,focus1)&&differs(focus1,focus2));
 State retry=rows;retry.sched.rows[1].retry=true;assert(differs(rows,retry));
 // ---- one message and the delete question
 State d=s;d.sched.view=1;d.sched.itemWhen="Tue 11 Nov 15:30";d.sched.itemTo="Alice (direct)";d.sched.itemText="Running late, start without me.";d.sched.itemState="Waiting";
 assert(differs(s,d));
 State act=d;act.sched.focus=1;assert(differs(d,act));State act2=d;act2.sched.focus=2;assert(differs(act,act2));
 State note=d;note.notice="Sending as soon as the radio is free";assert(differs(d,note));
 State long1=d;long1.sched.itemText=std::string(150,'x');assert(differs(d,long1));
 State confirm=d;confirm.sched.view=4;assert(differs(d,confirm));State confirm1=confirm;confirm1.sched.focus=1;assert(differs(confirm,confirm1));
 // ---- form
 State f=s;f.sched.view=2;f.sched.text="Meet at the lake";f.sched.target="Public";f.sched.condition=0;f.sched.date[0]=11;f.sched.date[1]=11;f.sched.date[2]=2026;f.sched.date[3]=15;f.sched.date[4]=30;f.sched.summary="Tue 11 Nov 15:30  (in 15 min)";
 assert(differs(d,f));
 for(int focus=0;focus<SchedFocusCount;focus++){State t=f;t.sched.focus=focus;if(focus>0)assert(differs(f,t)||focus==0);}
 {State a=f,b=f;a.sched.focus=1;b.sched.focus=2;assert(differs(a,b));b.sched.focus=7;assert(differs(a,b));b.sched.focus=8;assert(differs(a,b));}
 State t1=f;t1.sched.text="Another text";assert(differs(f,t1));State typing=f;typing.sched.typing=true;assert(differs(f,typing));
 for(int c=1;c<4;c++){State t=f;t.sched.condition=c;assert(differs(f,t));}
 {State a=f,b=f;a.sched.condition=1;b.sched.condition=2;assert(differs(a,b));}
 State dt=f;dt.sched.date[3]=16;assert(differs(f,dt));State ed=f;ed.sched.editing=true;assert(differs(f,ed));State fn=f;fn.notice="Pick a time in the future";assert(differs(f,fn));
 // ---- history
 State h=s;h.sched.view=3;assert(differs(s,h));
 State hs=h;{ScheduledView::Past p;p.head="Sent 11 Nov 15:30  Alice";p.meta="Running late";p.sent=true;hs.sched.past.push_back(p);ScheduledView::Past q;q.head="Failed 11 Nov 15:31  Public";q.meta="Radio offline: hello";q.sent=false;hs.sched.past.push_back(q);}
 assert(differs(h,hs));State hf=hs;hf.sched.focus=1;assert(differs(hs,hf));
 State hq=hs;hq.sched.past[0].sent=false;assert(differs(hs,hq));
 State hm=hs;for(int i=0;i<8;i++)hm.sched.past.push_back(hs.sched.past[0]);State hms=hm;hms.sched.scroll=2;assert(differs(hm,hms));
 std::cout<<"PASS: Scheduled UI: touch targets inside the content area and apart (list, history, message, delete, form), every page redraws with its state\n";
}
