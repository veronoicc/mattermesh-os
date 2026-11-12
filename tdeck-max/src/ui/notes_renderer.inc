// Notes app (56): the list, a note (checklist items with tappable checkboxes), the editor, rename, options, delete dialog.
// Included by renderer.cpp.
void Renderer::notesPage(const State &s){
 const NotesView &v=s.notes;
 shell(s,v.mode==NotesMode::List?"Notes":v.mode==NotesMode::Options?"Notes options":v.mode==NotesMode::Edit?"Edit note":"Note",true,Icon::Menu);g.fillRect(ChatMenu.x,ChatMenu.y,ChatMenu.w,ChatMenu.h-1,BG);
 // A rounded pill (ON / OFF, HOME) in the ink of its card; on = filled.
 auto pill=[&](Rect r,const std::string &text,bool on,uint16_t ink){uint16_t paper=ink==FG?BG:FG;if(on)g.fillRoundRect(r.x,r.y,r.w,r.h,r.h/2,ink);else g.drawRoundRect(r.x,r.y,r.w,r.h,r.h/2,ink);c.centered(r.x,r.y+(r.h-13)/2,r.w,text,Font::Bold,on?paper:ink);};
 auto ring=[&](Rect r){g.drawRoundRect(r.x+1,r.y+1,r.w-2,r.h-2,Radius,FG);};
 if(v.mode==NotesMode::Options){
  c.sectionLabel(NoteOptionsLabelY,"LIST");
  const char *titles[]={"Sort by","Show checked items","New note is a"},*hints[]={"Order of the list","Ticked items in a note","Used by New note"};
  const std::string values[]={v.sort?"Title":"Modified","",v.newType?"Checklist":"Text"};
  for(int i=0;i<NoteOptionCount;i++){
   Rect r=noteOptionRect(i);bool focus=v.optFocus==i;uint16_t ink=focus?BG:FG;c.card(r,focus);
   if(i==1){c.text(r.x+8,r.y+2,titles[i],Font::Bold,ink,r.w-70);c.text(r.x+8,r.y+17,hints[i],Font::ChatMeta,ink,r.w-70);pill({r.x+r.w-50,r.y+7,44,20},v.showChecked?"ON":"OFF",v.showChecked,ink);continue;}
   // < value >: a tap on "<" steps back, anywhere else forward (stepRowTap).
   int valueX=r.x+r.w-56-c.width(values[i],Font::Bold),room=valueX-(r.x+8)-4;
   c.text(r.x+8,r.y+2,titles[i],Font::Bold,ink,room);c.text(r.x+8,r.y+17,hints[i],Font::ChatMeta,ink,room);c.text(valueX,r.y+(r.h-13)/2,values[i],Font::Bold,ink);
   c.icon(Icon::Back,r.x+r.w-44,r.y+(r.h-12)/2,12,ink);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12,ink);}
  return;}
 if(v.mode==NotesMode::Edit){
  c.card(NoteEditBox,false,8);ring(NoteEditBox);
  auto rows=c.lines(v.text+"_",NoteEditBox.w-20,Font::ChatBody);int fit=(NoteEditBox.h-16)/15,first=std::max(0,int(rows.size())-fit);
  for(int k=first;k<int(rows.size());k++)c.text(NoteEditBox.x+10,NoteEditBox.y+8+(k-first)*15,rows[k],Font::ChatBody,FG,NoteEditBox.w-20);
  c.text(Margin+2,NoteEditCounterY,std::to_string(v.text.size())+" / "+std::to_string(mm_notes::MaxBytes),Font::ChatMeta,FG,W/2-Margin);
  {std::string hint=s.notice.empty()?"BACK saves":s.notice;c.text(W-Margin-2-c.width(hint,Font::ChatMeta),NoteEditCounterY,hint,Font::ChatMeta,FG,W/2);}
  c.button(NoteEditDone,"DONE",v.editFocus==0);c.button(NoteEditSide,"ADD ITEM",v.editFocus==1);
  return;}
 if(v.mode==NotesMode::Rename){
  c.text(Margin+2,NoteRenameLabelY,v.heading,Font::ChatMeta,FG,W-2*Margin);
  c.card(NoteRenameBox,false,6);ring(NoteRenameBox);auto tail=c.inputTail(v.text,NoteRenameBox.w-24,Font::ChatBody);
  c.text(NoteRenameBox.x+8,NoteRenameBox.y+8,tail,Font::ChatBody,FG,NoteRenameBox.w-16);g.drawFastVLine(NoteRenameBox.x+8+c.width(tail,Font::ChatBody)+1,NoteRenameBox.y+7,16,FG);
  c.text(Margin+2,NoteRenameBox.y+NoteRenameBox.h+8,s.notice.empty()?"ENTER saves":s.notice,Font::ChatMeta,FG,W-2*Margin);
  c.button(NoteEditDone,"SAVE",v.editFocus==0);c.button(NoteEditSide,"CANCEL",v.editFocus==1);
  return;}
 if(v.mode==NotesMode::List){
  {Rect r=NotesNew;bool on=v.focus==0;uint16_t ink=on?BG:FG;c.card(r,on);c.icon(Icon::Notes,r.x+8,r.y+10,24,ink);
   c.text(r.x+40,r.y+5,"New note",Font::Bold,ink,r.w-48);c.text(r.x+40,r.y+23,v.newType?"Checklist":"Text note",Font::ChatMeta,ink,r.w-48);}
  c.iconButton(NotesOptions,Icon::Settings,v.focus==1);if(v.focus!=1)c.card(NotesOptions);
  int n=int(v.rows.size());
  for(int i=0;i<NoteRows;i++){int idx=v.scroll+i;if(idx>=n)break;const NoteRow &row=v.rows[idx];Rect r=noteRowRect(i);bool on=v.focus==idx+2;uint16_t ink=on?BG:FG;c.card(r,on);
   int dateW=c.width(row.date,Font::ChatMeta),dateX=r.x+r.w-8-dateW-(row.pinned?10:0);
   c.text(r.x+8,r.y+3,row.title,Font::Bold,ink,dateX-r.x-14);c.text(dateX,r.y+5,row.date,Font::ChatMeta,ink);if(row.pinned)g.fillCircle(r.x+r.w-12,r.y+12,3,ink);
   int progW=row.progress.empty()?0:c.width(row.progress,Font::Bold)+8;
   c.text(r.x+8,r.y+19,row.preview,Font::ChatMeta,ink,r.w-16-progW);if(progW)c.text(r.x+r.w-8-progW+8,r.y+19,row.progress,Font::Bold,ink);}
  if(!n){const int y=NoteListY+34;c.centered(Margin,y,W-2*Margin,v.sd?"No notes yet":"No SD card",Font::ChatTitle);
   int ty=y+30;for(auto &line:c.lines(v.sd?"Tap New note above. Checklist lines start with - [ ] and tick on a tap.":"Notes are stored on the SD card.",W-2*Margin-24,Font::ChatMeta)){c.centered(Margin+12,ty,W-2*Margin-24,line,Font::ChatMeta);ty+=14;}}
  std::string count=!s.notice.empty()?s.notice:n>NoteRows?std::to_string(std::min(v.scroll,n-NoteRows)+1)+"-"+std::to_string(std::min(v.scroll,n-NoteRows)+NoteRows)+" of "+std::to_string(v.total>n?v.total:n):n?std::to_string(n)+(n==1?" note":" notes"):"";
  if(!count.empty())c.text(Margin+2,NotesCountY,count,Font::ChatMeta,FG,W-2*Margin-4);
  return;}
 // One note (also under the delete dialog).
 {Rect t=NoteTitle;c.card(t);c.text(t.x+8,t.y+3,v.title,Font::Bold,FG,t.w-(v.pinned?64:16));c.text(t.x+8,t.y+19,s.notice.empty()?v.meta:s.notice,Font::ChatMeta,FG,t.w-16);if(v.pinned)pill({t.x+t.w-54,t.y+9,46,18},"HOME",true,FG);}
 {Rect b=NoteBody;c.card(b,false,6);
  auto rows=mm_notes::visualRows(v.lines,v.showChecked,[&](const std::string &text,int w){return c.lines(text,w,Font::ChatBody);},NoteTextW,NoteCheckW);
  int first=mm_notes::clampScroll(v.viewScroll,int(rows.size()),NoteViewRows);
  if(rows.empty()){c.centered(b.x,b.y+54,b.w,"Nothing here yet",Font::ChatBody);c.centered(b.x,b.y+76,b.w,"EDIT writes, ADD starts a checklist item",Font::ChatMeta);}
  for(int i=0;i<NoteViewRows&&first+i<int(rows.size());i++){
   const mm_notes::VisualRow &row=rows[first+i];Rect rr=noteBodyRow(i);bool on=row.line==v.focusLine,item=row.kind!=mm_notes::Kind::Text;uint16_t ink=on?BG:FG;
   if(on)g.fillRoundRect(rr.x,rr.y,rr.w-14,rr.h,3,FG);
   if(item&&row.first){int bx=rr.x+3,by=rr.y+2;g.drawRect(bx,by,12,12,ink);if(row.kind==mm_notes::Kind::Done){g.drawLine(bx+2,by+6,bx+5,by+9,ink);g.drawLine(bx+2,by+7,bx+5,by+10,ink);g.drawLine(bx+5,by+9,bx+10,by+3,ink);g.drawLine(bx+5,by+10,bx+10,by+4,ink);}}
   int tx=rr.x+(item?NoteCheckW:4);c.text(tx,rr.y+1,row.text,Font::ChatBody,ink,rr.x+rr.w-14-tx);
   if(row.kind==mm_notes::Kind::Done&&!row.text.empty())g.drawFastHLine(tx,rr.y+8,std::min(c.width(row.text,Font::ChatBody),rr.x+rr.w-14-tx),ink);}// a checked item is struck through
  if(first>0)g.fillTriangle(b.x+b.w-12,b.y+4,b.x+b.w-6,b.y+10,b.x+b.w-18,b.y+10,FG);
  if(first+NoteViewRows<int(rows.size()))g.fillTriangle(b.x+b.w-12,b.y+b.h-4,b.x+b.w-6,b.y+b.h-10,b.x+b.w-18,b.y+b.h-10,FG);}
 const char *labels[]={"EDIT","ADD","CLEAR","RENAME",v.pinned?"UNPIN":"PIN","DELETE"};
 for(int i=0;i<NoteActionCount;i++){Rect r=noteActionRect(i);c.button(r,labels[i],v.focusAction==i);
  if(i==NoteClear&&!v.checked&&v.focusAction!=i)for(int x=r.x+4;x<r.x+r.w-4;x+=2){g.drawPixel(x,r.y,BG);g.drawPixel(x,r.y+r.h-1,BG);}}// nothing to clear: a dotted outline
 if(v.mode==NotesMode::Confirm){Rect panel=overlayPanel();c.text(Margin+8,Top+15,"Delete this note?",Font::ChatTitle,FG,panel.w-16);c.text(Margin+8,Top+56,v.title,Font::Bold,FG,panel.w-16);c.text(Margin+8,Top+76,"It is removed from the SD card.",Font::ChatMeta,FG,panel.w-16);
  c.button(NotesDeleteYes,"DELETE",v.editFocus==0);c.button(NotesDeleteNo,"CANCEL",v.editFocus!=0);}
}
