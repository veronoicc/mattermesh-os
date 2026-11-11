// RPT Scanner (section 53), part of the OS. Included by renderer.cpp.
#include "rpt_layout.h"
void Renderer::rptApp(const State &s){
 const RptView &v=s.rpt;
 const char *title=v.mode==0?"RPT Scanner":v.mode==2?"Login":v.mode==3?"Console":nullptr;
 shell(s,title?title:(v.name.empty()?"Repeater":v.name),true,Icon::Menu);if(v.mode!=0)g.fillRect(ComposeButton.x,StatusH,ComposeButton.w,HeaderH-1,BG);
 // A line list wrapped to a width, drawn from y; stops at the bottom of the box.
 auto lines=[&](const std::vector<std::string> &list,Rect box,int y){for(auto &line:list)for(auto &part:c.lines(line,box.w-16,Font::ChatMeta)){if(y+RptLineH>box.y+box.h)return;c.text(box.x+8,y,part,Font::ChatMeta,FG,box.w-16);y+=RptLineH;}};
 if(v.mode==0){
  const char *tabs[]={"SNR","HEARD","DIST","NAME"};
  for(int i=0;i<RptTabCount;i++){Rect r=rptTabRect(i);bool on=v.sort==i;c.card(r,on,4);c.centered(r.x,r.y+(r.h-13)/2,r.w,tabs[i],Font::Bold,on?BG:FG);}
  c.button(RptScan,v.scanning?"...":"SCAN",false);
  auto state=c.lines(v.status,RptState.w,Font::ChatMeta);for(int i=0;i<2&&i<int(state.size());i++)c.text(RptState.x,RptState.y+(state.size()>1?1:8)+i*RptLineH,state[i],Font::ChatMeta,FG,RptState.w);
  for(int i=0;i<RptRows&&i<int(v.rows.size());i++){
   Rect r=rptRowRect(i);auto &n=v.rows[i];bool on=v.first+i==v.focus;
   if(on)g.fillRoundRect(r.x,r.y,r.w,r.h,5,FG);else g.drawFastHLine(r.x+24,r.y+r.h,r.w-24,FG);auto ink=on?BG:FG;
   c.icon(Icon::Radio,r.x+4,r.y+(r.h-16)/2,16,ink);
   int right=r.x+r.w-5,dw=n.distance.empty()?0:c.width(n.distance,Font::ChatMeta),aw=n.age.empty()?0:c.width(n.age,Font::ChatMeta);
   if(dw)c.text(right-dw,r.y+5,n.distance,Font::ChatMeta,ink);
   c.text(r.x+25,r.y+3,n.name,Font::ChatBody,ink,right-r.x-25-(dw?dw+6:0));
   if(aw)c.text(right-aw,r.y+20,n.age,Font::ChatMeta,ink);
   c.text(r.x+25,r.y+20,n.signal,Font::ChatMeta,ink,right-r.x-25-(aw?aw+6:0));}
  if(v.rows.empty()){c.centered(Margin,RptListY+50,W-2*Margin,v.radioOn?"No repeaters heard yet":"Radio off",Font::Bold);c.centered(Margin,RptListY+70,W-2*Margin,v.radioOn?"SCAN, or wait for adverts":"Companion link in use",Font::ChatMeta);}
  if(v.total>RptRows){int track=RptRows*RptRowH-3,thumb=std::max(12,track*RptRows/v.total),max=v.total-RptRows;g.fillRect(W-4,RptListY+std::min(v.first,max)*(track-thumb)/max,3,thumb,FG);}
  if(v.menu){
   g.fillRect(RptMenuPopup.x,RptMenuPopup.y,RptMenuPopup.w,RptMenuPopup.h,BG);c.card(RptMenuPopup);c.text(RptMenuPopup.x+9,RptMenuPopup.y+5,"RPT Scanner options",Font::ChatMeta,FG,RptMenuPopup.w-18);
   const char *sorts[]={"SNR","HEARD","DIST","NAME"},*hides[]={"OFF","1 h","6 h","24 h"};
   auto item=[&](int i,const char *label,const char *detail,const std::string &value,int on){Rect r=rptMenuRow(i);bool focused=v.menuFocus==i;auto ink=focused?BG:FG;if(focused)g.fillRoundRect(r.x,r.y,r.w,r.h,Radius,FG);
    int pw=52,px=r.x+r.w-pw-6,py=r.y+(r.h-22)/2;if(on>0)g.fillRoundRect(px,py,pw,22,11,ink);else g.drawRoundRect(px,py,pw,22,11,ink);c.centered(px,py+5,pw,value,Font::Bold,on>0?(focused?FG:BG):ink);
    c.text(r.x+6,r.y+3,label,Font::ChatBody,ink,r.w-pw-18);c.text(r.x+6,r.y+20,detail,Font::ChatMeta,ink,r.w-pw-18);};
   item(0,"Default sort","Order on opening",sorts[std::max(0,std::min(3,v.defaultSort))],0);
   item(1,"Hide stale","Not heard for over",hides[v.hideHours==1?1:v.hideHours==6?2:v.hideHours==24?3:0],0);
   item(2,"Scan window","Listen time after SCAN",std::to_string(v.listenSec)+" s",0);
   item(3,"Show RSSI","Beside the SNR in rows",v.showRssi?"ON":"OFF",v.showRssi?1:0);}
  return;}
 if(v.mode==1){
  c.card(RptInfo,false,8);int y=RptInfo.y+5;
  std::vector<std::string> key;if(v.key.size()>32){key.push_back(v.key.substr(0,32));key.push_back(v.key.substr(32));}else key.push_back(v.key+(v.key.size()<64?" (prefix)":""));
  for(auto &k:key){c.text(RptInfo.x+8,y,k,Font::Small,FG,RptInfo.w-16);y+=c.lineHeight(Font::Small);}
  for(auto *t:{&v.position,&v.advert,&v.signal,&v.path}){if(y+RptLineH>RptInfo.y+RptInfo.h)break;c.text(RptInfo.x+8,y,*t,Font::ChatMeta,FG,RptInfo.w-16);y+=RptLineH;}
  const char *labels[]={"PING","STATUS",v.loggedIn?"CONSOLE":"LOGIN","DM PATH"};
  for(int i=0;i<RptActionCount;i++)c.button(rptAction(i),labels[i],v.actionFocus==i);
  c.card(RptAnswer,false,8);if(v.answerState==1)c.text(RptAnswer.x+RptAnswer.w-26,RptAnswer.y+4,"...",Font::Bold,FG);
  lines(v.answer,RptAnswer,RptAnswer.y+4);
  if(v.picker){
   Rect panel=overlayPanel();c.text(panel.x+8,Top+10,"Set DM path via "+v.name,Font::ChatBody,FG,panel.w-16);
   for(int i=0;i<RptPickRows&&i<int(v.pickRows.size());i++)c.button(rptPickRow(i),v.pickRows[i],v.pickFirst+i==v.pickFocus);
   if(v.pickRows.empty())c.centered(Margin,Top+80,W-2*Margin,"No contacts yet",Font::Bold);
   const char *pick[]={"PREV","NEXT","CANCEL"};for(int i=0;i<3;i++)c.button(rptPickButton(i),pick[i],false);
   if(v.pickTotal>RptPickRows)c.text(Margin+8,Top+238,std::to_string(v.pickFirst+1)+"-"+std::to_string(std::min(v.pickTotal,v.pickFirst+RptPickRows))+" of "+std::to_string(v.pickTotal),Font::ChatMeta,FG,panel.w-16);}
  return;}
 if(v.mode==2){
  c.text(Margin,Top+6,"Admin password for",Font::ChatMeta,FG,W-2*Margin);c.text(Margin,Top+20,v.name,Font::Bold,FG,W-2*Margin);
  c.card(RptField,false,6);std::string shown=v.masked?std::string(v.input.size(),'*'):v.input,tail=c.inputTail(shown,RptField.w-24,Font::ChatBody);
  c.text(RptField.x+8,RptField.y+9,tail,Font::ChatBody,FG,RptField.w-24);g.fillRect(RptField.x+9+c.width(tail,Font::ChatBody),RptField.y+8,2,17,FG);
  c.button(RptShow,v.masked?"SHOW":"HIDE",false);c.button(RptCancel,"CANCEL",false);c.button(RptGo,v.waiting?"...":"LOGIN",true);
  auto note=c.lines(v.note.empty()?std::string("A wrong password gets no reply."):v.note,W-2*Margin,Font::ChatMeta);for(int i=0;i<4&&i<int(note.size());i++)c.text(Margin,RptLoginNoteY+i*RptLineH,note[i],Font::ChatMeta,FG,W-2*Margin);
  return;}
 // Console: the newest lines above the input line, a bar of quick commands below.
 std::vector<std::string> wrapped;for(auto &line:v.console)for(auto &part:c.lines(line,RptOut.w-6,Font::ChatMeta))wrapped.push_back(part);
 int visible=RptOut.h/RptLineH,maxScroll=std::max(0,int(wrapped.size())-visible),scroll=std::min(v.consoleScroll,maxScroll),first=std::max(0,int(wrapped.size())-visible-scroll);
 for(int i=0;i<visible&&first+i<int(wrapped.size());i++)c.text(RptOut.x,RptOut.y+i*RptLineH,wrapped[first+i],Font::ChatMeta,FG,RptOut.w-6);
 if(maxScroll){int height=visible*RptLineH,thumb=std::max(10,height*visible/int(wrapped.size()));g.drawFastVLine(W-3,RptOut.y,height,FG);g.fillRect(W-4,RptOut.y+(height-thumb)*(maxScroll-scroll)/maxScroll,3,thumb,FG);}
 {std::string before="> "+v.input;int room=RptOut.w-(v.waiting?22:6);std::string head=c.width(before,Font::ChatMeta)>room?c.inputTail(before,room,Font::ChatMeta):before;
  c.text(RptOut.x,RptInputY,head,Font::ChatMeta,FG,room);g.fillRect(RptOut.x+c.width(head,Font::ChatMeta)+1,RptInputY,2,RptLineH-1,FG);if(v.waiting)c.text(RptOut.x+RptOut.w-18,RptInputY,"...",Font::Bold,FG);}
 c.card(RptBar,false,8);const char *quick[]={"ver","name","nbrs","clock","advert"};
 for(int i=0;i<RptQuickCount;i++){Rect r=rptQuick(i);c.card(r,false,4);c.centered(r.x,r.y+(r.h-13)/2,r.w,quick[i],Font::ChatMeta);}
}
