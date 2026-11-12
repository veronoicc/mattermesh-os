// Settings > Quick messages (61) and the picker over a chat. Included by renderer.cpp.
void Renderer::quickPage(const State &s){
 const QuickView &v=s.quick;const int n=int(v.rows.size());
 shell(s,"Quick messages");g.fillRect(ChatMenu.x,ChatMenu.y,ChatMenu.w,ChatMenu.h-1,BG);
 // < value >: a tap on "<" steps back, anywhere else forward (stepRowTap).
 auto stepRow=[&](Rect r,const char *label,const std::string &value,bool focus){uint16_t ink=focus?BG:FG;c.card(r,focus,6);int y=r.y+(r.h-13)/2,valueX=r.x+r.w-60-c.width(value,Font::Bold);
  c.text(r.x+8,y,label,Font::Bold,ink,valueX-r.x-12);c.text(valueX,y,value,Font::Bold,ink);c.icon(Icon::Back,r.x+r.w-44,r.y+(r.h-12)/2,12,ink);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12,ink);};
 stepRow(QuickKeyRow,"Quick key",v.keyLabel,v.focus==0);stepRow(QuickModeRow,"Mode",v.modeLabel,v.focus==1);
 for(int i=0;i<QuickRows;i++){int idx=v.scroll+i;if(idx>=n)break;const auto &row=v.rows[idx];Rect r=quickRowRect(i);bool edit=v.editing==idx,focus=v.focus==2+idx||edit;uint16_t ink=focus?BG:FG;c.card(r,focus,6);
  c.text(r.x+8,r.y+(r.h-11)/2,std::to_string(idx+1),Font::ChatMeta,ink,12);int textX=r.x+22,room=r.w-22-(row.pin||edit?26:10);
  c.text(textX,r.y+(r.h-13)/2,edit?c.inputTail(v.input+"_",room,Font::Bold):row.text,Font::Bold,ink,room);
  if(row.pin)c.icon(Icon::GPS,r.x+r.w-20,r.y+(r.h-12)/2,12,ink);}
 if(!n)c.centered(Margin,quickRowRect(1).y,W-2*Margin,"No messages yet",Font::ChatTitle);
 {std::string hint=!s.notice.empty()?s.notice:v.editing>=0?"ENTER saves / BACK cancels / "+std::to_string(v.input.size())+"/60":"ENTER edit  D pin  X del";
  std::string range=n>QuickRows&&v.editing<0?std::to_string(v.scroll+1)+"-"+std::to_string(std::min(n,v.scroll+QuickRows))+" of "+std::to_string(n):"";int rw=range.empty()?0:c.width(range,Font::ChatMeta)+6;
  c.text(Margin+2,QuickCountY,hint,Font::ChatMeta,FG,W-2*Margin-4-rw);if(rw)c.text(W-Margin-2-rw+6,QuickCountY,range,Font::ChatMeta);}
 const int p=v.focus-2;const bool onRow=p>=0&&p<n;const char *labels[]={v.editing>=0?"SAVE":"EDIT","UP","DOWN","PIN","DEL"};
 for(int i=0;i<QuickActionCount;i++)c.button(quickActionRect(i),labels[i],onRow&&((i==0&&v.editing>=0)||(i==3&&v.rows[p].pin)));
 c.button(QuickAdd,"+ ADD",v.focus==2+n);c.button(QuickReset,"RESET",v.focus==3+n);
 if(v.confirm){Rect panel=overlayPanel();const bool reset=v.confirm==2;
  c.text(Margin+8,Top+15,reset?"Reset to defaults?":"Delete this message?",Font::ChatTitle,FG,panel.w-16);
  if(reset)c.text(Margin+8,Top+56,"Your own messages are replaced.",Font::ChatMeta,FG,panel.w-16);else if(p>=0&&p<n)c.text(Margin+8,Top+56,v.rows[p].text,Font::Bold,FG,panel.w-16);
  c.button(QuickConfirmYes,reset?"RESET":"DELETE",v.confirmFocus==0);c.button(QuickConfirmNo,"CANCEL",v.confirmFocus==1);}
}
// The picker: every preset in a row (digit 1-8), a pin on those that add the position, the mode on the title line.
void Renderer::quickPicker(const State &s){
 const QuickView &v=s.quick;const int n=std::min(QuickPickRows,int(v.rows.size()));Rect panel=overlayPanel();
 c.text(Margin+8,Top+12,"Quick messages",Font::ChatTitle,FG,panel.w-16);
 for(int i=0;i<n;i++){Rect r=quickPickRect(i);bool focus=v.pickFocus==i;uint16_t ink=focus?BG:FG;c.card(r,focus,5);
  c.text(r.x+7,r.y+5,std::to_string(i+1),Font::ChatMeta,ink,10);c.text(r.x+22,r.y+4,v.rows[i].text,Font::Bold,ink,r.w-22-(v.rows[i].pin?26:8));
  if(v.rows[i].pin)c.icon(Icon::GPS,r.x+r.w-20,r.y+(r.h-12)/2,12,ink);}
 if(!n){c.centered(Margin+8,Top+80,panel.w-16,"No messages yet",Font::ChatTitle,FG);int y=Top+110;for(auto &line:c.lines("Add some in Settings > Quick messages.",panel.w-32,Font::ChatMeta)){c.centered(Margin+16,y,panel.w-32,line,Font::ChatMeta);y+=14;}}
 c.text(Margin+10,QuickPickNoticeY,v.pickNotice.empty()?v.modeLabel+": tap, 1-8 or ENTER":v.pickNotice,Font::ChatMeta,FG,panel.w-20);
 c.button(QuickPickCancel,"CANCEL");
}
