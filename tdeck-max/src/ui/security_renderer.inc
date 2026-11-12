// Settings > Security (58) and the PIN pad (lock screen and Security page). Included by renderer.cpp.
#include "security_layout.h"
#include "services/pin_policy.h"
// The pad: title with CANCEL, dots for the typed digits, a notice line (wrong PIN, tries left, the backoff countdown), 3 x 4 keys.
// A wrong PIN frames the dots and inverts the notice; while the backoff runs the keys are drawn empty.
void Renderer::pinPad(const State &s){
 const PinPadView &p=s.pin;const Rect panel=PinPanel;
 g.fillRect(0,StatusH,W,H-StatusH,BG);c.card(panel);
 c.text(panel.x+8,PinTitleY,p.title,Font::ChatTitle,FG,PinCancel.x-panel.x-12);c.button(PinCancel,"CANCEL");
 c.text(panel.x+8,PinHintY,p.hint,Font::ChatMeta,FG,panel.w-16);
 const int total=(PinMaxDigits-1)*PinDotStep,x0=(W-total)/2,shake=p.wrong?5:0;
 for(int i=0;i<PinMaxDigits;i++){int x=x0+i*PinDotStep+shake;if(i<p.digits)g.fillCircle(x,PinDotsY,PinDotR,FG);else g.drawCircle(x,PinDotsY,PinDotR,FG);}
 if(p.wrong){g.drawRoundRect(x0-14+shake,PinDotsY-13,total+28,26,Radius,FG);g.drawRoundRect(x0-15+shake,PinDotsY-14,total+30,28,Radius,FG);}
 const bool waiting=p.waitSeconds>0;const std::string note=waiting?"Try again in "+(p.waitSeconds>=120?std::to_string((p.waitSeconds+59)/60)+" min":std::to_string(p.waitSeconds)+" s"):p.notice;
 if(!note.empty()){Rect n={panel.x+8,PinNoticeY,panel.w-16,18};const bool hot=waiting||p.wrong;if(hot)g.fillRoundRect(n.x,n.y,n.w,n.h,Radius,FG);c.centered(n.x,n.y+3,n.w,note,Font::ChatMeta,hot?BG:FG);}
 for(int i=0;i<PinKeys;i++){Rect r=pinKeyRect(i);if(waiting)c.card(r);else c.button(r,PinKeyLabel[i],i==11&&p.digits>=int(device::PinMin));}
}
void Renderer::securityPage(const State &s){
 const SecurityView &v=s.sec;shell(s,"Security");g.fillRect(W-32,StatusH,32,HeaderH-1,BG);
 if(s.pin.active){pinPad(s);return;}
 auto pill=[&](Rect row,const char *label,bool on){c.card(row);c.text(row.x+7,row.y+(row.h-15)/2,label,Font::ChatBody,FG,row.w-80);auto p=secPill(row);if(on)g.fillRoundRect(p.x,p.y,p.w,p.h,11,FG);else g.drawRoundRect(p.x,p.y,p.w,p.h,11,FG);c.centered(p.x,p.y+5,p.w,on?"ON":"OFF",Font::Bold,on?BG:FG);};
 c.toggleRow(SecuritySwitch,Icon::Lock,"Lock PIN",v.status,v.on);
 c.sectionLabel(SettingsSectionY,"PIN");
 {Rect r=SecSetPin;c.card(r);c.text(r.x+7,r.y+(r.h-15)/2,v.on?"Change / remove PIN":"Set a PIN",Font::ChatBody,FG,r.w-40);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12);}
 {Rect r=SecRequire;c.card(r);c.text(r.x+7,r.y+(r.h-15)/2,"Ask after",Font::ChatBody,FG,80);int right=r.x+r.w-56;c.text(right-c.width(v.requireName,Font::Bold),r.y+(r.h-13)/2,v.requireName,Font::Bold);c.icon(Icon::Back,r.x+r.w-44,r.y+(r.h-12)/2,12);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12);}
 c.sectionLabel(SecLabelY,"LOCK SCREEN");
 pill(SecHide,"Hide previews",v.hide);pill(SecUsb,"Block USB data",v.usb);
 {Rect r=SecInfo;c.card(r,false,6);int y=r.y+4;size_t at=0;const std::string &d=v.detail;while(at<=d.size()&&y+13<=r.y+r.h){size_t end=d.find('\n',at);if(end==std::string::npos)end=d.size();c.text(r.x+8,y,d.substr(at,end-at),Font::ChatMeta,FG,r.w-16);y+=13;at=end+1;}}
 {Rect r=secRowRect(std::max(0,std::min(SecRows-1,v.focus)));g.fillRect(2,r.y+4,3,r.h-8,FG);}
}
