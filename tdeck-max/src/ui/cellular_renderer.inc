// Settings > Cellular (51): Modem switch, status card, then the scrolling rows (SIM, APN, network, MQTT uplink).
// Included by renderer.cpp.
#include "cellular_layout.h"
void Renderer::cellularPage(const State &s){
 const CellularView &v=s.cell;
 shell(s,"Cellular");g.fillRect(ChatMenu.x,ChatMenu.y,ChatMenu.w,ChatMenu.h-1,BG);
 c.toggleRow(CellularSwitch,Icon::Signal,"Modem",device::modemPhaseText(v.phase),v.modem);
 if(v.focus==0)for(int i=1;i<=2;i++)g.drawRoundRect(CellularSwitch.x+i,CellularSwitch.y+i,CellularSwitch.w-2*i,CellularSwitch.h-2*i,Radius,FG);// focus ring inside the card
 {Rect r=CellularStatus;c.card(r,false,8);const int x=r.x+8,w=r.w-16,y=r.y+3;
  auto right=[&](int yy,const std::string &t,Font f){c.text(r.x+r.w-8-c.width(t,f),yy,t,f);return c.width(t,f);};
  int simW=right(y,"SIM "+cellSimText(v.phase,v.simReady),Font::ChatMeta);c.text(x,y,v.model.empty()?(v.modem?"Modem":"Modem off"):v.model,Font::Bold,FG,w-simW-8);
  int ratW=right(y+13,cellRatText(v.rat),Font::ChatMeta);c.text(x,y+13,v.op.empty()?"No operator":v.op,Font::Bold,FG,w-ratW-8);
  int bars=std::max(0,std::min(4,v.bars));for(int i=0;i<4;i++){int h=3+3*i,bx=x+i*5;if(i<bars)g.fillRect(bx,y+38-h,4,h,FG);else g.drawRect(bx,y+38-h,4,h,FG);}
  c.text(x+26,y+26,cellDbm(v.csq),Font::ChatMeta,FG,70);right(y+26,std::string("Data ")+device::dataPhaseText(v.data),Font::ChatMeta);
  c.text(x,y+39,v.message.empty()?v.ip:v.message,Font::ChatMeta,FG,w);}
 c.sectionLabel(CellularLabelY,cellGroup(v.scroll),"");
 for(int i=0;i<2;i++){Rect r=i?CellularDown:CellularUp;bool can=i?v.scroll<cellMaxScroll():v.scroll>0;c.card(r,false,5);int cx=r.x+r.w/2,cy=r.y+r.h/2,d=i?-1:1;
  if(can)g.fillTriangle(cx-5,cy+3*d,cx+5,cy+3*d,cx,cy-3*d,FG);else g.drawTriangle(cx-5,cy+3*d,cx+5,cy+3*d,cx,cy-3*d,FG);}
 for(int slot=0;slot<CellVisible;slot++){int row=v.scroll+slot;if(row>=CellRows)break;Rect r=cellularRow(slot);CellKind kind=cellKind(row);
  if(kind==CellInfo){c.card(r,false,6);c.text(r.x+8,r.y+1,v.mqttState,Font::Bold,FG,r.w-16);c.text(r.x+8,r.y+14,v.mqttCounts,Font::ChatMeta,FG,r.w-16);continue;}
  bool edit=v.editing==row,focus=v.focus==row+1||edit;c.card(r,focus,6);auto ink=focus?BG:FG;auto paper=focus?FG:BG;
  if(kind==CellStep){c.text(r.x+8,r.y+7,v.value[row],Font::Bold,ink,r.w-60);c.icon(Icon::Back,r.x+r.w-44,r.y+(r.h-12)/2,12,ink);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12,ink);continue;}
  if(kind==CellAction&&v.on[row]){c.text(r.x+8,r.y+1,"New token (shown once)",Font::ChatMeta,ink,r.w-16);c.text(r.x+8,r.y+14,v.value[row],Font::Bold,ink,r.w-16);continue;}// the generated token, visible until the page moves on
  c.text(r.x+8,r.y+8,cellRowLabel(row),Font::ChatMeta,ink,72);
  if(kind==CellToggle){bool on=v.on[row];int pw=44,px=r.x+r.w-pw-6,py=r.y+4;
   if(on)g.fillRoundRect(px,py,pw,20,10,ink);else g.drawRoundRect(px,py,pw,20,10,ink);c.centered(px,py+4,pw,on?"ON":"OFF",Font::Bold,on?paper:ink);continue;}
  int vx=r.x+80,vw=r.w-88;c.text(vx,r.y+7,edit?c.inputTail(v.input+"_",vw,Font::Bold):v.value[row],Font::Bold,ink,vw);}
}
