// Self-test app (section 60), part of the OS. Included by renderer.cpp; the layout (selftest_layout.h) arrives through selftest_view.h (model.h).
void Renderer::selfTestApp(const State &s){
 const SelfTestView &t=s.selftest;
 shell(s,"Self-test",true,Icon::Menu);
 // Totals card: counts big, progress / export result / hint below.
 c.card(SelfTestSummary,false,8);
 {std::string counts=std::to_string(t.pass)+" PASS  "+std::to_string(t.fail)+" FAIL  "+std::to_string(t.skip)+" SKIP";c.text(SelfTestSummary.x+8,SelfTestSummary.y+3,counts,Font::Bold,FG,SelfTestSummary.w-16);
  c.text(SelfTestSummary.x+8,SelfTestSummary.y+20,t.toast.empty()?t.line:t.toast,Font::ChatMeta,FG,SelfTestSummary.w-16);}
 // Check list: status glyph, name, result word, detail; a bar shows the scroll position.
 const int count=int(t.rows.size());
 for(int i=0;i<SelfTestRows&&t.first+i<count;i++){const SelfTestRow &row=t.rows[t.first+i];Rect r=selfTestRow(i);bool focused=t.button<0&&t.focus==t.first+i;auto ink=focused?BG:FG;c.card(r,focused,4);
  int gx=r.x+7,gy=r.y+(r.h-14)/2;auto st=static_cast<selftest::Status>(row.status);
  if(st==selftest::Status::Pass){g.drawLine(gx,gy+7,gx+4,gy+11,ink);g.drawLine(gx+1,gy+7,gx+5,gy+11,ink);g.drawLine(gx+5,gy+11,gx+13,gy+2,ink);g.drawLine(gx+4,gy+11,gx+12,gy+2,ink);}
  else if(st==selftest::Status::Fail){g.drawLine(gx+1,gy+1,gx+12,gy+12,ink);g.drawLine(gx+2,gy+1,gx+13,gy+12,ink);g.drawLine(gx+12,gy+1,gx+1,gy+12,ink);g.drawLine(gx+13,gy+1,gx+2,gy+12,ink);}
  else if(st==selftest::Status::Skip){g.fillRect(gx+2,gy+6,10,3,ink);}
  else if(st==selftest::Status::Running){for(int d=0;d<3;d++)g.fillRect(gx+1+d*5,gy+6,3,3,ink);}
  else if(st==selftest::Status::Input)c.text(gx+3,gy,"?",Font::Bold,ink);
  else g.drawCircle(gx+7,gy+7,5,ink);
  const char *word=st==selftest::Status::Pass?"PASS":st==selftest::Status::Fail?"FAIL":st==selftest::Status::Skip?"SKIP":st==selftest::Status::Running?"RUNNING":st==selftest::Status::Input?"WAITING":"";
  int ww=c.width(word,Font::ChatMeta);c.text(r.x+r.w-6-ww,r.y+3,word,Font::ChatMeta,ink);
  c.text(r.x+27,r.y+3,row.label,Font::ChatMeta,ink,r.w-27-ww-14);
  if(!row.detail.empty())c.text(r.x+27,r.y+17,row.detail,Font::Tiny,ink,r.w-33);}
 if(count>SelfTestRows){const Rect b=SelfTestScrollBar;int h=std::max(8,b.h*SelfTestRows/count),y=b.y+(b.h-h)*t.first/std::max(1,count-SelfTestRows);g.drawFastVLine(b.x+b.w/2,b.y,b.h,FG);g.fillRect(b.x,y,b.w,h,FG);}
 // Buttons: RUN ALL turns into STOP while a run is going.
 c.button(selfTestButton(0),t.busy?"STOP":"RUN ALL",t.button==0);c.button(selfTestButton(1),"RUN ONE",t.button==1);c.button(selfTestButton(2),"EXPORT",t.button==2);
 // An interactive check waits for the user: the card covers the list (the touch check shows the square to tap).
 if(t.prompt){g.fillRect(SelfTestPrompt.x,SelfTestPrompt.y,SelfTestPrompt.w,SelfTestPrompt.h,BG);c.card(SelfTestPrompt,false,8);
  c.centered(SelfTestPrompt.x,SelfTestPrompt.y+8,SelfTestPrompt.w,t.promptTitle,Font::Bold);c.centered(SelfTestPrompt.x+6,SelfTestPrompt.y+30,SelfTestPrompt.w-12,t.promptText,Font::ChatMeta);
  if(t.prompt==2){const Rect q=SelfTestTarget;g.drawRoundRect(q.x,q.y,q.w,q.h,8,FG);g.drawRoundRect(q.x+1,q.y+1,q.w-2,q.h-2,7,FG);g.drawFastHLine(q.x+8,q.y+q.h/2,q.w-16,FG);g.drawFastVLine(q.x+q.w/2,q.y+8,q.h-16,FG);g.drawCircle(q.x+q.w/2,q.y+q.h/2,14,FG);}
  else c.centered(SelfTestPrompt.x,SelfTestPrompt.y+80,SelfTestPrompt.w,"[ any key ]",Font::ChatTitle);
  c.centered(SelfTestPrompt.x,SelfTestPrompt.y+SelfTestPrompt.h-20,SelfTestPrompt.w,"Skipped in "+std::to_string(t.promptSecs)+" s",Font::ChatMeta);}
 if(!t.options)return;
 // Options sheet: which checks RUN ALL includes.
 g.fillRect(SelfTestPopup.x,SelfTestPopup.y,SelfTestPopup.w,SelfTestPopup.h,BG);c.card(SelfTestPopup);c.text(SelfTestPopup.x+9,SelfTestPopup.y+5,"Self-test options",Font::ChatMeta,FG,SelfTestPopup.w-18);
 auto item=[&](int i,const std::string &label,const std::string &detail,bool on){Rect r=selfTestOptionRow(i);bool focused=t.optionFocus==i;auto ink=focused?BG:FG;if(focused)g.fillRoundRect(r.x,r.y,r.w,r.h,Radius,FG);int pw=40,px=r.x+r.w-pw-6,py=r.y+(r.h-22)/2,textW=r.w-pw-20;
  if(on)g.fillRoundRect(px,py,pw,22,11,ink);else g.drawRoundRect(px,py,pw,22,11,ink);c.centered(px,py+5,pw,on?"ON":"OFF",Font::Bold,on?(focused?FG:BG):ink);
  c.text(r.x+6,r.y+3,label,Font::ChatBody,ink,textW);c.text(r.x+6,r.y+20,detail,Font::ChatMeta,ink,textW);};
 item(0,"Interactive checks",t.interactive?"keyboard and touch ask for input":"keyboard and touch skipped",t.interactive);item(1,"Sound / vibration checks",t.output?"speaker, mic and motor are used":"quiet: no sound, no buzz",t.output);
}
