#include <cassert>
#include <iostream>
#include "ui/renderer.h"
#include "ui/repeat_toggle.h"
static int ink(GFXcanvas1 &canvas,mm::Rect rect){int n=0;for(int y=rect.y;y<rect.y+rect.h;y++)for(int x=rect.x;x<rect.x+rect.w;x++)n+=canvas.getPixel(x,y);return n;}
int main(){
 GFXcanvas1 canvas(mm::W,mm::H);mm::Components c(canvas);mm::Renderer r(canvas);mm::State s;
 s.page=mm::Page::Radio;s.radioDevicePage=true;s.radioName="MatterMesh_Ridge";
 // The row and its hint sit inside the panel, clear of the path buttons above and APPLY below.
 assert(mm::LoRaRepeat.y>=mm::loraBytesRect(1).y+mm::loraBytesRect(1).h&&mm::LoRaRepeat.y+mm::LoRaRepeat.h<mm::LoRaDeviceApply.y&&mm::LoRaRepeat.x>=mm::Margin&&mm::LoRaRepeat.x+mm::LoRaRepeat.w<=mm::W-mm::Margin);
 assert(c.width(mm::LoRaRepeatHint,mm::Font::ChatMeta)<=mm::LoRaContentW);
 // Visible, and the ON/OFF pill changes the picture.
 r.draw(s);int off=ink(canvas,mm::LoRaRepeat);assert(off>0);
 s.radioRepeat=true;r.draw(s);assert(ink(canvas,mm::LoRaRepeat)>off);
 // Keyboard focus on the row draws a frame around it.
 s.radioRepeat=false;r.draw(s);int plain=ink(canvas,{mm::LoRaRepeat.x-3,mm::LoRaRepeat.y-3,mm::LoRaRepeat.w+6,mm::LoRaRepeat.h+6});s.radioDeviceFocus=mm::LoRaRepeatFocus;r.draw(s);assert(ink(canvas,{mm::LoRaRepeat.x-3,mm::LoRaRepeat.y-3,mm::LoRaRepeat.w+6,mm::LoRaRepeat.h+6})>plain);
 // Activation calls the setter with the opposite state and reports it.
 bool on=false;int calls=0,want=-1;std::string notice;
 auto set=[&](bool v,std::string &){calls++;want=v;on=v;return true;};
 mm::repeatToggle(on,set,notice);assert(calls==1&&want==1&&on&&notice=="Repeat on");
 mm::repeatToggle(on,set,notice);assert(calls==2&&want==0&&!on&&notice=="Repeat off");
 // A refusal shows the service's reason and leaves the switch off.
 auto refuse=[&](bool,std::string &reason){calls++;reason="Repeat needs an allowed frequency";return false;};
 mm::repeatToggle(on,refuse,notice);assert(calls==3&&!on&&notice=="Repeat needs an allowed frequency");
 s.radioRepeat=on;s.notice=notice;r.draw(s);assert(ink(canvas,mm::LoRaRepeat)>0&&!s.radioRepeat);
 std::cout<<"PASS: repeat toggle row drawn with ON/OFF and focus, setter called with the opposite state, refusal reason shown and switch stays off\n";
}
