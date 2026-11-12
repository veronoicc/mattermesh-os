#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/navigation_catalog.h"
int main(){
 using namespace mm;
 auto inside=[](Rect r){return r.x>=Margin&&r.x+r.w<=W-Margin&&r.y>=Top&&r.y+r.h<=Bottom;};
 auto apart=[](Rect a,Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 auto disjoint=[&](const std::vector<Rect> &page){for(size_t i=0;i<page.size();i++){assert(inside(page[i]));for(size_t j=i+1;j<page.size();j++)assert(apart(page[i],page[j]));}};
 // ---- touch targets: inside the content area, none overlapping, texts clear of them
 {std::vector<Rect> list={NotesNew,NotesOptions};for(int i=0;i<NoteRows;i++)list.push_back(noteRowRect(i));disjoint(list);assert(noteRowRect(NoteRows-1).y+noteRowRect(NoteRows-1).h<=NotesCountY&&NotesCountY+10<=Bottom);}
 {std::vector<Rect> view={NoteTitle,NoteBody};for(int i=0;i<NoteActionCount;i++)view.push_back(noteActionRect(i));disjoint(view);
  for(int i=0;i<NoteViewRows;i++){Rect r=noteBodyRow(i);assert(r.x>=NoteBody.x&&r.x+r.w<=NoteBody.x+NoteBody.w&&r.y>=NoteBody.y&&r.y+r.h<=NoteBody.y+NoteBody.h);if(i)assert(apart(r,noteBodyRow(i-1)));}}
 disjoint({NoteEditBox,NoteEditDone,NoteEditSide});assert(NoteEditCounterY>=NoteEditBox.y+NoteEditBox.h&&NoteEditCounterY+13<=NoteEditDone.y);
 disjoint({NoteRenameBox,NoteEditDone,NoteEditSide});assert(NoteRenameLabelY+13<=NoteRenameBox.y);
 {std::vector<Rect> opts;for(int i=0;i<NoteOptionCount;i++)opts.push_back(noteOptionRect(i));disjoint(opts);assert(NoteOptionsLabelY+13<=noteOptionRect(0).y);}
 disjoint({NotesDeleteYes,NotesDeleteNo});
 // ---- focus moves
 assert(noteListMove(0,-1,3)==0&&noteListMove(0,1,3)==1&&noteListMove(4,1,3)==4&&noteListMove(2,1,0)==1);// New, Options, then the rows
 assert(noteViewStep(0,-1,2)==0&&noteViewStep(1,1,2)==2&&noteViewStep(7,1,2)==7&&noteViewStep(0,1,0)==1);// two items, six actions
 assert(noteViewAction(0,1,2)==2&&noteViewAction(2,1,2)==3&&noteViewAction(7,1,2)==7&&noteViewAction(3,-1,2)==2&&noteViewAction(2,-1,2)==2&&noteViewAction(0,-1,2)==0);
 // ---- every page redraws with its state
 GFXcanvas1 canvas(W,H);Renderer r(canvas);
 auto shot=[&](const State &s){r.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+(W+7)/8*H);};
 auto differs=[&](const State &a,const State &b){return shot(a)!=shot(b);};
 // list
 State s;s.page=Page::Notes;auto empty=shot(s);
 State nosd=s;nosd.notes.sd=false;assert(differs(s,nosd));
 State rows=s;for(int i=0;i<12;i++){NoteRow n;n.title="Note "+std::to_string(i);n.preview="first line of the note";n.date="11 Nov 15:"+std::to_string(10+i);if(i%3==0)n.progress="3/7";n.pinned=i==1;rows.notes.rows.push_back(n);}rows.notes.total=12;
 assert(differs(s,rows));
 State scrolled=rows;scrolled.notes.scroll=5;assert(differs(rows,scrolled));
 State f1=rows;f1.notes.focus=1;State f2=rows;f2.notes.focus=2;State f0=rows;assert(differs(f0,f1)&&differs(f1,f2)&&differs(f0,f2));
 State pinned=rows;pinned.notes.rows[2].pinned=true;assert(differs(rows,pinned));
 State typeCl=s;typeCl.notes.newType=1;assert(differs(s,typeCl));
 // note view
 State v=s;v.notes.mode=NotesMode::View;v.notes.title="Groceries";v.notes.meta="2/4 done  11 Nov 15:30";v.notes.lines=mm_notes::parseLines("Groceries\n- [ ] milk\n- [x] eggs\nbring bags and a very long line that has to wrap over several rows of the screen\n- [ ] bread\n- [x] butter");
 auto base=shot(v);assert(base!=empty);
 State chk=v;chk.notes.lines=mm_notes::parseLines("Groceries\n- [x] milk\n- [x] eggs\nbring bags and a very long line that has to wrap over several rows of the screen\n- [ ] bread\n- [x] butter");assert(differs(v,chk));
 State hide=v;hide.notes.showChecked=false;assert(differs(v,hide));
 State fl=v;fl.notes.focusLine=1;State fl2=v;fl2.notes.focusLine=4;assert(differs(v,fl)&&differs(fl,fl2));
 State fa=v;fa.notes.focusAction=0;State fa2=v;fa2.notes.focusAction=5;assert(differs(v,fa)&&differs(fa,fa2));
 State pin=v;pin.notes.pinned=true;assert(differs(v,pin));
 State sc=v;{std::string many="Long";for(int i=0;i<20;i++)many+="\n- [ ] item "+std::to_string(i);sc.notes.lines=mm_notes::parseLines(many);}State sc2=sc;sc2.notes.viewScroll=2;assert(differs(sc,sc2));
 State nt=v;nt.notice="Pinned to Home";assert(differs(v,nt));
 State plain=v;plain.notes.lines=mm_notes::parseLines("Idea\nsome text");plain.notes.title="Idea";assert(differs(v,plain));
 State noted=v;noted.notes.lines=mm_notes::parseLines("Empty");assert(differs(v,noted));
 State clr=v;clr.notes.checked=2;assert(differs(v,clr));// CLEAR is dotted when nothing is checked
 State confirm=v;confirm.notes.mode=NotesMode::Confirm;assert(differs(v,confirm));
 // editor and rename
 State e=s;e.notes.mode=NotesMode::Edit;auto edit0=shot(e);assert(edit0!=empty);
 State e1=e;e1.notes.text="Groceries\n- [ ] milk\n- [ ] ";assert(differs(e,e1));
 State e2=e1;e2.notes.text+="x";assert(differs(e1,e2));
 State e3=e1;e3.notes.editFocus=0;State e4=e1;e4.notes.editFocus=1;assert(differs(e1,e3)&&differs(e3,e4));
 State e5=e1;for(int i=0;i<30;i++)e5.notes.text+="\nline "+std::to_string(i);assert(differs(e1,e5));
 State rn=s;rn.notes.mode=NotesMode::Rename;rn.notes.heading="Rename";assert(differs(e,rn));
 State rn2=rn;rn2.notes.text="Groceries";assert(differs(rn,rn2));State rn3=rn;rn3.notes.heading="New checklist";assert(differs(rn,rn3));
 // options
 State o=s;o.notes.mode=NotesMode::Options;auto opt0=shot(o);assert(opt0!=empty);
 State o1=o;o1.notes.sort=1;State o2=o;o2.notes.showChecked=false;o2.notes.newType=0;State o3=o;o3.notes.newType=1;State o4=o;o4.notes.optFocus=2;
 for(auto *t:{&o1,&o3,&o4})assert(differs(o,*t));
 {State a=o,b=o;b.notes.showChecked=false;a.notes.showChecked=true;assert(differs(a,b));}
 std::cout<<"PASS: Notes UI: touch targets inside the content area and apart (list, view, editor, rename, options, delete), focus moves, every page redraws with its state\n";
}
