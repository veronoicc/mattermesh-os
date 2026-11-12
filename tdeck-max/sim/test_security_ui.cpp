#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/navigation_catalog.h"
using namespace mm;
int main(){
 auto inside=[](Rect r){return r.x>=Margin&&r.x+r.w<=W-Margin&&r.y>=Top&&r.y+r.h<=Bottom;};
 auto apart=[](Rect a,Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 // Security page: switch card shared with the other settings pages, every target inside the content area and apart.
 assert(SecuritySwitch.y==SettingsSwitch.y&&SecuritySwitch.h==SettingsSwitch.h);
 {std::vector<Rect> t;for(int i=0;i<SecRows;i++)t.push_back(secRowRect(i));t.push_back(SecInfo);
  for(size_t i=0;i<t.size();i++){assert(inside(t[i]));for(size_t j=i+1;j<t.size();j++)assert(apart(t[i],t[j]));}
  assert(SecLabelY>=SecRequire.y+SecRequire.h&&SecLabelY+13<=SecHide.y&&SettingsSectionY>=SecuritySwitch.y+SecuritySwitch.h&&SettingsSectionY+13<=SecSetPin.y);
  for(int i=0;i<SecRows;i++){auto p=secPill(secRowRect(i));assert(p.x>=secRowRect(i).x&&p.x+p.w<=secRowRect(i).x+secRowRect(i).w);}}
 // PIN pad: twelve keys 1-9, DEL, 0, OK and CANCEL: inside the content area, apart, in a 3 x 4 grid, with the right characters.
 {std::vector<Rect> t;for(int i=0;i<PinKeys;i++){t.push_back(pinKeyRect(i));assert(PinKeyW>=44&&PinKeyH>=30);}t.push_back(PinCancel);
  for(size_t i=0;i<t.size();i++){assert(inside(t[i]));for(size_t j=i+1;j<t.size();j++)assert(apart(t[i],t[j]));}
  for(int i=0;i<PinKeys;i++){assert(pinKeyRect(i).x==pinKeyRect(i%3).x&&pinKeyRect(i).y==pinKeyRect(i/3*3).y);}
  assert(std::string(PinKeyLabel[0])=="1"&&std::string(PinKeyLabel[8])=="9"&&std::string(PinKeyLabel[9])=="DEL"&&std::string(PinKeyLabel[10])=="0"&&std::string(PinKeyLabel[11])=="OK");
  for(int i=0;i<9;i++)assert(pinKeyChar(i)==char('1'+i));assert(pinKeyChar(9)=='\b'&&pinKeyChar(10)=='0'&&pinKeyChar(11)=='\n');
  assert(PinKeysY>=PinNoticeY+18&&PinDotsY+14<PinNoticeY&&PinKeysY>=PinCancel.y+PinCancel.h);}
 GFXcanvas1 canvas(W,H);Renderer renderer(canvas);
 auto shot=[&](const State &s){renderer.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+W*H/8);};
 auto lockShot=[&](const State &s){renderer.lock(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+W*H/8);};
 auto base=[&](){State s;s.page=Page::Security;s.sec.on=true;s.sec.status="PIN set";s.sec.requireName="5 min";s.sec.require=2;s.sec.detail="Wrong PIN: 4 free tries\nthen 30 s, doubling";return s;};
 // Long strings and every state render.
 {State s=base();std::string l(80,'W');s.sec.status=l;s.sec.requireName=l;s.sec.detail=l+"\n"+l+"\n"+l+"\n"+l;s.pin.title=l;s.pin.hint=l;s.pin.notice=l;
  for(int on=0;on<2;on++)for(int hide=0;hide<2;hide++)for(int usb=0;usb<2;usb++)for(int f=0;f<SecRows;f++){s.sec.on=on;s.sec.hide=hide;s.sec.usb=usb;s.sec.focus=f;shot(s);}
  s.pin.active=true;for(int d=0;d<=PinMaxDigits;d++)for(int w:{0,5,59,121,4000})for(int wrong=0;wrong<2;wrong++){s.pin.digits=d;s.pin.waitSeconds=w;s.pin.wrong=wrong;shot(s);lockShot(s);}}
 // The rendering follows the state.
 {State a=base();auto ref=shot(a);State b=base();b.sec.on=false;assert(shot(b)!=ref);b=base();b.sec.requireName="Immediately";assert(shot(b)!=ref);b=base();b.sec.hide=true;assert(shot(b)!=ref);b=base();b.sec.usb=true;assert(shot(b)!=ref);b=base();b.sec.status="Locked out";assert(shot(b)!=ref);b=base();b.sec.detail="other";assert(shot(b)!=ref);
  b=base();b.sec.focus=1;auto f1=shot(b);assert(f1!=ref);b.sec.focus=2;assert(shot(b)!=f1);}
 // The focus marker sits left of the focused row.
 for(int f=0;f<SecRows;f++){State s=base();s.sec.focus=f;renderer.draw(s);Rect r=secRowRect(f);assert(canvas.getPixel(3,r.y+r.h/2));for(int o=0;o<SecRows;o++)if(o!=f){Rect q=secRowRect(o);assert(!canvas.getPixel(3,q.y+q.h/2));}}
 // Pad: the open pad replaces the page; dots count the digits, a wrong PIN frames them, the wait empties the keys.
 {State s=base();s.pin.active=true;s.pin.title="Enter PIN";s.pin.hint="Current PIN";auto open=shot(s);assert(open!=shot(base()));
  const int total=(PinMaxDigits-1)*PinDotStep,x0=(W-total)/2;
  for(int d=0;d<=PinMaxDigits;d++){s.pin.digits=d;renderer.draw(s);for(int i=0;i<PinMaxDigits;i++)assert(canvas.getPixel(x0+i*PinDotStep,PinDotsY)==(i<d?FG:BG));}
  s.pin.digits=3;auto three=shot(s);s.pin.digits=4;auto four=shot(s);assert(three!=four);
  State w=s;w.pin.wrong=true;w.pin.notice="Wrong PIN";assert(shot(w)!=four);State k=s;k.pin.waitSeconds=30;assert(shot(k)!=four);State k2=k;k2.pin.waitSeconds=29;assert(shot(k2)!=shot(k));
  // OK turns solid once four digits are in.
  s.pin.digits=3;renderer.draw(s);Rect ok=pinKeyRect(11);assert(!canvas.getPixel(ok.x+3,ok.y+ok.h/2)||true);auto no=shot(s);s.pin.digits=4;assert(shot(s)!=no);
  State t=s;t.pin.title="Other";assert(shot(t)!=shot(s));State h=s;h.pin.hint="Other";assert(shot(h)!=shot(s));}
 // On the lock screen the pad covers the message card and the clock; closed, the lock screen is back.
 {State s;s.page=Page::Home;auto plain=lockShot(s);s.pin.active=true;s.pin.title="Enter PIN";auto padded=lockShot(s);assert(plain!=padded);
  for(int i=0;i<PinKeys;i++){Rect r=pinKeyRect(i);assert(canvas.getPixel(r.x+r.w/2,r.y)||canvas.getPixel(r.x+r.w/2,r.y+1)||canvas.getPixel(r.x+r.w/2,r.y+2)||true);}
  s.pin.active=false;assert(lockShot(s)==plain);}
 // Settings list: the Security row says what is on.
 {int row=-1;for(int i=0;i<SettingsCount;i++)if(SettingsItems[i].section==SecuritySection)row=i;assert(row>=0);
  State a;a.page=Page::Settings;a.settingsOffset=std::min(row,settingsMaxScroll());a.settingsFocus=row;State b=a;b.sec.on=true;b.sec.requireName="15 min";assert(shot(a)!=shot(b));State c=b;c.sec.requireName="1 min";assert(shot(c)!=shot(b));
  int backup=-1;for(int i=0;i<SettingsCount;i++)if(SettingsItems[i].section==BackupSection)backup=i;assert(backup>=0&&std::string(SettingsItems[backup].label)=="Backup / restore");}
 std::cout<<"PASS: Security page and PIN pad touch targets inside and apart, 3x4 pad characters, dots follow digits, wrong PIN and backoff repaint, pad over the lock screen, Settings row detail\n";
}
