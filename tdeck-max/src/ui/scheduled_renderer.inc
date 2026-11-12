// Scheduled app (section 63): the list, one message, the Send later / edit form, the delete question and the history.
// Included by renderer.cpp. Layout: ui/scheduled_layout.h, state: scheduled_app.inc.
void Renderer::scheduledPage(const State &s){
 const ScheduledView &v=s.sched;
 shell(s,v.view==3?"History":v.view==2?(v.editing?"Edit message":"Send later"):v.view?"Scheduled message":"Scheduled",true,Icon::Menu);g.fillRect(ChatMenu.x,ChatMenu.y,ChatMenu.w,ChatMenu.h-1,BG);
 // A rounded pill in the ink of its card.
 auto pill=[&](Rect r,const std::string &text,bool on,uint16_t ink){uint16_t paper=ink==FG?BG:FG;if(on)g.fillRoundRect(r.x,r.y,r.w,r.h,r.h/2,ink);else g.drawRoundRect(r.x,r.y,r.w,r.h,r.h/2,ink);c.centered(r.x,r.y+(r.h-13)/2,r.w,text,Font::Bold,on?paper:ink);};
 // Empty state: a title and the wrapped hint under it.
 auto empty=[&](int y,const std::string &title,const std::string &hint){c.centered(Margin,y,W-2*Margin,title,Font::ChatTitle);int ty=y+30;for(auto &line:c.lines(hint,W-2*Margin-24,Font::ChatMeta)){c.centered(Margin+12,ty,W-2*Margin-24,line,Font::ChatMeta);ty+=14;}};
 auto count=[&](int first,int shown,int total){if(total>shown)c.text(Margin+2,SchedCountY,std::to_string(first+1)+"-"+std::to_string(std::min(total,first+shown))+" of "+std::to_string(total),Font::ChatMeta,FG,W-2*Margin-4);};
 if(v.view==0){
  {Rect r=SchedHistoryCard;bool focus=v.focus==0;uint16_t ink=focus?BG:FG;c.card(r,focus);c.icon(Icon::Clock,r.x+8,r.y+10,24,ink);c.text(r.x+40,r.y+5,"History",Font::Bold,ink,r.w-70);c.text(r.x+40,r.y+23,"Last sent and failed messages",Font::ChatMeta,ink,r.w-70);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12,ink);}
  const int n=int(v.rows.size());
  for(int i=0;i<SchedRows;i++){int idx=v.scroll+i;if(idx>=n)break;const auto &row=v.rows[idx];Rect r=schedRowRect(i);bool focus=v.focus==idx+1;uint16_t ink=focus?BG:FG;c.card(r,focus);
   c.text(r.x+8,r.y+3,row.when,Font::Bold,ink,r.w-(row.retry?84:30));c.text(r.x+8,r.y+19,row.meta,Font::ChatMeta,ink,r.w-30);
   if(row.retry)pill({r.x+r.w-70,r.y+9,50,18},"RETRY",false,ink);c.icon(Icon::ChevronRight,r.x+r.w-16,r.y+(r.h-12)/2,12,ink);}
  if(!n)empty(SchedListY+34,"Nothing scheduled",v.clock?"Open a chat, tap its menu and choose Send later.":"Open a chat, tap its menu and choose Send later. The clock is not set: only the event conditions can fire.");
  else count(std::min(v.scroll,std::max(0,n-SchedRows)),SchedRows,v.total>n?v.total:n);
  return;}
 if(v.view==3){
  const int n=int(v.past.size());
  for(int i=0;i<SchedHistRows;i++){int idx=v.scroll+i;if(idx>=n)break;const auto &p=v.past[idx];Rect r=schedHistRect(i);bool focus=v.focus==idx;uint16_t ink=focus?BG:FG;c.card(r,focus);
   c.text(r.x+8,r.y+3,p.head,Font::Bold,ink,r.w-(p.sent?16:72));c.text(r.x+8,r.y+19,p.meta,Font::ChatMeta,ink,r.w-16);if(!p.sent)pill({r.x+r.w-60,r.y+9,52,18},"FAILED",true,ink);}
  if(!n)empty(Top+60,"No history yet","Sent and failed scheduled messages show here, the last eight.");
  else count(std::min(v.scroll,std::max(0,n-SchedHistRows)),SchedHistRows,n);
  return;}
 if(v.view==2){
  c.sectionLabel(Top+0,"MESSAGE",std::to_string(v.text.size())+" bytes");
  {Rect r=SchedFormText;bool focus=v.focus==0;c.card(r,false);if(focus)g.drawRoundRect(r.x+2,r.y+2,r.w-4,r.h-4,Radius,FG);
   auto lines=c.lines(v.typing?v.text+"_":v.text,r.w-16,Font::ChatBody);int first=std::max(0,int(lines.size())-3);for(int i=first;i<int(lines.size());i++)c.text(r.x+8,r.y+3+(i-first)*15,lines[i],Font::ChatBody,FG,r.w-16);}
  c.text(Margin+2,SchedFormToY,"To: "+v.target,Font::ChatMeta,FG,W-2*Margin-4);
  {Rect r=SchedFormCond;bool focus=v.focus==1;uint16_t ink=focus?BG:FG;c.card(r,focus);c.text(r.x+8,r.y+8,"WHEN",Font::ChatMeta,ink,40);
   std::string name=mm_sched::conditionName(mm_sched::Condition(v.condition));c.text(r.x+52,r.y+(r.h-13)/2,name,Font::Bold,ink,r.w-120);c.icon(Icon::Back,r.x+r.w-44,r.y+(r.h-12)/2,12,ink);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12,ink);}
  if(v.condition==int(mm_sched::Condition::AtTime)){
   c.sectionLabel(SchedFormLabelY,"SEND AT","DD MM YYYY  HH MM");
   for(int i=0;i<5;i++){char t[8];snprintf(t,sizeof t,i==2?"%04d":"%02d",v.date[i]);c.button(schedFieldRect(i),t,v.focus==2+i);}
   c.button(SchedFormMinus,"-");c.button(SchedFormPlus,"+");
   c.text(Margin+2,SchedFormSummaryY,v.summary,Font::ChatMeta,FG,W-2*Margin-4);}
  else{
   c.sectionLabel(SchedFormLabelY,"SENDS ONCE");
   const char *hint[]={"","The next time a repeater is heard after you schedule it.","When the radio has been quiet for a few seconds.","The next time this contact is heard after you schedule it."};
   int ty=SchedFormLabelY+20;for(auto &line:c.lines(hint[std::max(0,std::min(3,v.condition))],W-2*Margin-4,Font::ChatMeta)){c.text(Margin+2,ty,line,Font::ChatMeta,FG,W-2*Margin-4);ty+=14;}}
  if(!s.notice.empty())c.text(Margin+2,SchedFormNoticeY,s.notice,Font::ChatMeta,FG,W-2*Margin-4);
  c.button(SchedFormSave,v.editing?"SAVE":"SCHEDULE",v.focus==SchedFocusSave);c.button(SchedFormCancel,"CANCEL",v.focus==SchedFocusCancel);
  return;}
 // One message (view 1), with the delete question over it (view 4).
 {Rect r=SchedItemWhen;c.card(r);c.text(r.x+8,r.y+3,"WHEN",Font::ChatMeta,FG,r.w-16);c.text(r.x+8,r.y+19,v.itemWhen,Font::Bold,FG,r.w-16);}
 {Rect r=SchedItemTo;c.card(r);c.text(r.x+8,r.y+3,"TO",Font::ChatMeta,FG,r.w-16);c.text(r.x+8,r.y+19,v.itemTo,Font::Bold,FG,r.w-16);}
 {Rect r=SchedItemText;c.card(r);int ty=r.y+3;auto lines=c.lines(v.itemText,r.w-16,Font::ChatBody);for(int i=0;i<std::min(6,int(lines.size()));i++){c.text(r.x+8,ty,lines[i],Font::ChatBody,FG,r.w-16);ty+=15;}}
 {const std::string &line=s.notice.empty()?v.itemState:s.notice;if(!line.empty())c.text(Margin+2,SchedItemStateY,line,Font::ChatMeta,FG,W-2*Margin-4);}
 const char *labels[]={"EDIT","SEND NOW","DELETE"};for(int i=0;i<SchedActionCount;i++)c.button(schedActionRect(i),labels[i],v.view==1&&v.focus==i);
 if(v.view==4){Rect panel=overlayPanel();c.text(Margin+8,Top+15,"Delete this message?",Font::ChatTitle,FG,panel.w-16);int ty=Top+56;auto lines=c.lines(v.itemText,panel.w-16,Font::ChatMeta);for(int i=0;i<std::min(4,int(lines.size()));i++){c.text(Margin+8,ty,lines[i],Font::ChatMeta,FG,panel.w-16);ty+=14;}
  c.button(SchedDeleteYes,"DELETE",v.focus==0);c.button(SchedDeleteNo,"CANCEL",v.focus==1);}
}
