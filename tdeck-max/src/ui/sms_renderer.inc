// SMS app (52): inbox, message view with its delete dialog, compose.
// Included by renderer.cpp.
// sms_page.h (paging, previews) comes with sms_layout.h, which model.h includes at file scope: this file sits inside namespace mm.
// A field with the keyboard focus gets a second outline inside the card.
static void smsFocusRing(Adafruit_GFX &g,Rect r,bool on){if(on)g.drawRoundRect(r.x+1,r.y+1,r.w-2,r.h-2,5,FG);}
void Renderer::smsPage(const State &s){
 const SmsView &v=s.sms;
 auto noHeaderButton=[&]{g.fillRect(W-32,StatusH,32,HeaderH-1,BG);};
 if(v.mode==SmsMode::Inbox){
  shell(s,"SMS",true,Icon::Plus);
  for(int i=0;i<SmsRows&&i<int(v.rows.size());i++){
   const SmsRow &m=v.rows[i];Rect r=smsRow(i);bool on=v.start+i==v.focus;auto ink=on?BG:FG;c.card(r,on,5);
   int timeW=c.width(m.time,Font::ChatMeta),timeX=r.x+r.w-8-timeW-(m.unread?10:0);
   c.text(r.x+8,r.y+4,m.number.empty()?"Unknown sender":m.number,m.unread?Font::Bold:Font::ChatBody,ink,timeX-r.x-14);
   c.text(timeX,r.y+6,m.time,Font::ChatMeta,ink);if(m.unread)g.fillCircle(r.x+r.w-12,r.y+13,3,ink);
   c.text(r.x+8,r.y+23,m.preview,Font::ChatMeta,ink,r.w-16);}
  if(v.rows.empty()){
   if(v.status.empty())c.centered(Margin,SmsListY+60,W-2*Margin,"No messages",Font::ChatBody);
   else{c.centered(Margin,SmsListY+44,W-2*Margin,v.status,Font::Bold);auto hint=c.lines(v.hint,W-2*Margin-24,Font::ChatMeta);for(unsigned i=0;i<hint.size()&&i<3;i++)c.centered(Margin,SmsListY+66+i*15,W-2*Margin,hint[i],Font::ChatMeta);}}
  std::string left=!s.notice.empty()?s.notice:!v.status.empty()?v.status:smsRange(v.start,SmsRows,v.total)+(v.unread?"  "+std::to_string(v.unread)+" new":"");
  c.text(Margin,SmsPagerY+8,left,Font::ChatMeta,FG,SmsPrev.x-Margin-6);
  if(v.start>0)c.button(SmsPrev,"<",false);if(v.start+SmsRows<v.total)c.button(SmsNext,">",false);
  return;}
 const bool dialog=v.mode==SmsMode::Confirm;
 if(v.mode==SmsMode::Read||dialog){
  shell(s,"Message");noHeaderButton();
  if(!dialog){{Rect r=SmsReadHead;c.card(r,false,6);c.text(r.x+8,r.y+4,v.number.empty()?"Unknown sender":v.number,Font::ChatTitle,FG,r.w-16);c.text(r.x+8,r.y+24,v.time,Font::ChatMeta,FG,r.w-16);}
  {Rect r=SmsReadBody;c.card(r,false,6);auto rows=c.lines(v.text,r.w-16,Font::ChatBody);int first=smsScroll(v.scroll,0,int(rows.size()),SmsReadLines);
   for(int i=0;i<SmsReadLines&&first+i<int(rows.size());i++)c.text(r.x+8,r.y+10+i*SmsBodyLineH,rows[first+i],Font::ChatBody,FG,r.w-16);
   if(first>0)g.fillTriangle(r.x+r.w-12,r.y+4,r.x+r.w-6,r.y+10,r.x+r.w-18,r.y+10,FG);
   if(first+SmsReadLines<int(rows.size()))g.fillTriangle(r.x+r.w-12,r.y+r.h-4,r.x+r.w-6,r.y+r.h-10,r.x+r.w-18,r.y+r.h-10,FG);}
  const char *labels[]={"REPLY","DELETE","BACK"};for(int i=0;i<3;i++)c.button(smsReadButton(i),labels[i],v.readFocus==i);}
  if(dialog){
   Rect r=SmsConfirmCard;c.card(r,false,8);g.drawRoundRect(r.x+1,r.y+1,r.w-2,r.h-2,7,FG);
   c.centered(r.x,r.y+10,r.w,"Delete this message?",Font::ChatTitle);c.centered(r.x,r.y+36,r.w,v.number.empty()?"Unknown sender":v.number,Font::Bold);
   auto rows=c.lines(v.text,r.w-24,Font::ChatMeta);for(unsigned i=0;i<rows.size()&&i<3;i++)c.centered(r.x,r.y+58+i*14,r.w,i==2&&rows.size()>3?rows[i]+"...":rows[i],Font::ChatMeta);
   c.text(r.x+10,r.y+112,"It is removed from the SIM card.",Font::ChatMeta,FG,r.w-20);
   c.button(SmsConfirmYes,"DELETE",true);c.button(SmsConfirmNo,"CANCEL",false);}
  return;}
 shell(s,"New SMS");noHeaderButton();
 c.text(Margin,Top+2,"TO",Font::ChatMeta);if(v.numberBad&&!v.to.empty()){std::string bad="not a valid number";c.text(W-Margin-c.width(bad,Font::ChatMeta),Top+2,bad,Font::ChatMeta);}
 {Rect r=SmsNumber;bool on=v.field==0;c.card(r,false,6);smsFocusRing(g,r,on);
  if(v.to.empty())c.text(r.x+8,r.y+8,"Phone number",Font::ChatBody,FG,r.w-16);else c.text(r.x+8,r.y+8,v.to,Font::ChatBody,FG,r.w-16);
  if(on)g.drawFastVLine(r.x+8+(v.to.empty()?0:c.rawWidth(v.to,Font::ChatBody))+1,r.y+7,16,FG);}
 c.text(Margin,Top+56,"MESSAGE",Font::ChatMeta);
 {const int width=W-2*Margin-16;std::vector<std::string> rows=v.body.empty()?std::vector<std::string>():c.lines(v.body,width-3,Font::ChatBody);if(rows.empty())rows.push_back("");
  int shown=std::min(int(rows.size()),SmsTextMaxLines),skip=int(rows.size())-shown;Rect r=smsTextRect(shown);bool on=v.field==1;c.card(r,false,6);smsFocusRing(g,r,on);
  if(v.body.empty())c.text(r.x+8,r.y+9,"Write message...",Font::ChatBody,FG,width);
  else for(int i=0;i<shown;i++)c.text(r.x+8,r.y+9+i*SmsBodyLineH,rows[skip+i],Font::ChatBody,FG,width);
  if(on){int x=r.x+8+(v.body.empty()?0:c.rawWidth(rows.back(),Font::ChatBody)+(v.body.back()==' '?c.rawWidth(" ",Font::ChatBody):0))+1;g.drawFastVLine(x,r.y+8+(shown-1)*SmsBodyLineH,15,FG);}}
 c.text(Margin,SmsCounterY,v.counter+(v.over?"  too long":""),v.over?Font::Bold:Font::ChatMeta,FG,W-2*Margin);
 {std::string line=v.sending?"Sending...":v.result;if(!line.empty())c.text(Margin,SmsResultY,line,Font::ChatMeta,FG,W-2*Margin);}
 {Rect r=SmsSend;bool on=v.field==2;
  if(v.canSend)c.button(r,"SEND",on);
  else{// disabled: a dotted outline
   for(int x=r.x+3;x<r.x+r.w-3;x+=2){g.drawPixel(x,r.y,FG);g.drawPixel(x,r.y+r.h-1,FG);}for(int y=r.y+3;y<r.y+r.h-3;y+=2){g.drawPixel(r.x,y,FG);g.drawPixel(r.x+r.w-1,y,FG);}
   smsFocusRing(g,r,on);c.centered(r.x,r.y+(r.h-13)/2,r.w,v.sending?"SENDING":"SEND",Font::Bold);}}
 c.button(SmsCancel,"CANCEL",v.field==3);
}
