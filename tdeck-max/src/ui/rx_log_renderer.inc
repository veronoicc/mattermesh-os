// RX Log app (RxLogSection), part of the OS. Included by renderer.cpp.
#include "rx_log_layout.h"
void Renderer::rxLogApp(const State &s){
 const RxLogView &v=s.rxl;
 shell(s,"RX Log",true,Icon::Menu);
 // Bottom hint line: the keys of the open page, or the last result (export, clear).
 auto hint=[&](const std::string &keys){g.fillRect(0,FooterY,W,FooterH,BG);g.drawFastHLine(0,FooterY,W,FG);c.text(Margin,FooterY+4,s.notice.empty()?keys:s.notice,Font::Small,FG,W-2*Margin);};
 if(v.detail){
  g.fillRect(0,Top,W,H-Top,BG);c.card(RxDetailPanel);
  c.text(RxDetailPanel.x+8,RxDetailPanel.y+6,v.detailTitle,Font::ChatTitle,FG,RxDetailPanel.w-16);
  int total=int(v.detailLines.size()),most=std::max(0,total-RxDetailRows),first=std::max(0,std::min(v.detailScroll,most));
  for(int i=0;i<RxDetailRows&&first+i<total;i++)c.text(RxDetailPanel.x+8,RxDetailTextY+i*RxDetailLineH,v.detailLines[first+i],Font::ChatMeta,FG,RxDetailPanel.w-22);
  if(most){int track=RxDetailRows*RxDetailLineH,thumb=std::max(10,track*RxDetailRows/total);g.fillRect(RxDetailPanel.x+RxDetailPanel.w-5,RxDetailTextY+(track-thumb)*first/most,3,thumb,FG);}
  c.button(rxDetailButton(0),"UP",first>0);c.button(rxDetailButton(1),"DOWN",first<most);c.button(rxDetailButton(2),"CLOSE");
  hint("W/S scroll  A close");return;}
 // Toolbar: type filter < All > and PAUSE / RESUME.
 c.button(RxFilterPrev,"<");c.card(RxFilterLabel);c.centered(RxFilterLabel.x,RxFilterLabel.y+(RxFilterLabel.h-13)/2,RxFilterLabel.w,v.filterName,Font::Bold);c.button(RxFilterNext,">");
 c.button(RxPauseButton,v.paused?"RESUME":"PAUSE",v.paused);
 // Status line: what the list holds, LIVE or how many packets came in while it is paused.
 c.text(Margin+2,RxStatusY,v.ready?(v.filtered?std::to_string(v.shown)+" of "+std::to_string(v.stored):std::to_string(v.stored)+" packets"):"RX log not ready",Font::ChatMeta,FG,W/2);
 {std::string right=v.paused?"PAUSED +"+std::to_string(v.fresh):"LIVE";c.text(W-Margin-2-c.width(right,Font::ChatMeta),RxStatusY,right,Font::ChatMeta);}
 for(int i=0;i<RxLogRows&&i<int(v.rows.size());i++){
  Rect r=rxRowRect(i);const RxLogRow &row=v.rows[i];if(row.selected)g.fillRoundRect(r.x,r.y,r.w,r.h,5,FG);else g.drawFastHLine(r.x+4,r.y+r.h,r.w-8,FG);auto ink=row.selected?BG:FG;
  int right=r.x+r.w-4;std::string route=row.route+" "+row.hops;
  c.text(r.x+4,r.y+1,row.time,Font::ChatMeta,ink,RxColType-6);c.text(r.x+4+RxColType,r.y+1,row.type,Font::Bold,ink,right-c.width(route,Font::ChatMeta)-(r.x+4+RxColType)-4);c.text(right-c.width(route,Font::ChatMeta),r.y+1,route,Font::ChatMeta,ink);
  c.text(r.x+4,r.y+14,row.rssi,Font::ChatMeta,ink,RxColSnr-4);c.text(r.x+4+RxColSnr,r.y+14,row.snr,Font::ChatMeta,ink,RxColHash-RxColSnr-4);c.text(r.x+4+RxColHash,r.y+14,row.hash,Font::ChatMeta,ink);c.text(right-c.width(row.length,Font::ChatMeta),r.y+14,row.length,Font::ChatMeta,ink);}
 if(v.rows.empty()){c.centered(Margin,RxListY+50,W-2*Margin,!v.ready?"RX log unavailable":v.filtered?"No packet of this type":"No packets yet",Font::Bold);c.centered(Margin,RxListY+70,W-2*Margin,!v.ready?"Radio offline / check LoRa":"Waiting for the radio",Font::ChatMeta);}
 if(v.shown>RxLogRows){int track=RxLogRows*RxLogRowH-2,thumb=std::max(12,track*RxLogRows/v.shown),most=v.shown-RxLogRows;g.fillRect(W-4,RxListY+std::min(v.first,most)*(track-thumb)/most,3,thumb,FG);}
 hint(v.options?"W/S move  H/L/ENTER change  A back":"W/S ENT  H/L type  SPC pause  M menu");
 if(v.options){
  g.fillRect(RxOptionsPopup.x,RxOptionsPopup.y,RxOptionsPopup.w,RxOptionsPopup.h,BG);c.card(RxOptionsPopup);
  c.text(RxOptionsPopup.x+9,RxOptionsPopup.y+5,"RX log options",Font::ChatMeta,FG,RxOptionsPopup.w-18);
  // label + detail line, with a value pill (value: "" none); the focused row is inverted.
  auto item=[&](int i,const std::string &label,const std::string &detail,const std::string &pill,bool on){
   Rect r=rxOptionRect(i);bool focused=v.optionFocus==i;auto ink=focused?BG:FG;if(focused)g.fillRoundRect(r.x,r.y,r.w,r.h,Radius,FG);else g.drawRoundRect(r.x,r.y,r.w,r.h,Radius,FG);
   int textW=r.w-16;if(!pill.empty()){int pw=std::max(44,c.width(pill,Font::Bold)+16),px=r.x+r.w-pw-6,py=r.y+(r.h-22)/2;textW-=pw+4;if(on)g.fillRoundRect(px,py,pw,22,11,ink);else g.drawRoundRect(px,py,pw,22,11,ink);c.centered(px,py+5,pw,pill,Font::Bold,on?(focused?FG:BG):ink);}
   c.text(r.x+6,r.y+2,label,Font::ChatBody,ink,textW);c.text(r.x+6,r.y+18,detail,Font::ChatMeta,ink,textW);};
  item(0,"Default type","Filter at app start",v.defaultFilterName,false);
  item(1,"Time format",v.uptime?"Since boot":"Local clock",v.uptime?"UPTIME":"CLOCK",false);
  item(2,"Auto-pause","Freeze the list in detail",v.autoPause?"ON":"OFF",v.autoPause);
  item(3,"Export CSV","To the SD root (Files)","",false);
  item(4,v.clearArmed?"Tap again to clear":"Clear log",std::to_string(v.stored)+" packets in memory","",false);
  item(5,"Done","","",false);
 }
}
