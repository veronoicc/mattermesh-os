#pragma once
#include <string>
#include "theme.h"
// Settings > Security (58) and the PIN pad, which is the same panel on the lock screen and inside the Security page.
// Security page: the Lock PIN switch card on top, then compact rows (action row, "< value >" row, ON/OFF pills), a status card.
namespace mm {
constexpr Rect SecuritySwitch=SettingsSwitch,SecSetPin={Margin,Top+70,W-2*Margin,28},SecRequire={Margin,Top+101,W-2*Margin,28},SecHide={Margin,Top+153,W-2*Margin,28},SecUsb={Margin,Top+184,W-2*Margin,28},SecInfo={Margin,Top+216,W-2*Margin,46};
constexpr int SecRows=5,SecLabelY=Top+137;// keyboard focus: 0 PIN switch, 1 set / change, 2 require after, 3 hide previews, 4 block USB
constexpr Rect secRowRect(int row){return row==0?SecuritySwitch:row==1?SecSetPin:row==2?SecRequire:row==3?SecHide:SecUsb;}
constexpr Rect secPill(Rect r){return {r.x+r.w-62,r.y+3,54,22};}
// PIN pad: title and Cancel on top, one dot per typed digit, a notice line, then the 3 x 4 keys (1-9, DEL 0 OK).
constexpr Rect PinPanel={Margin,Top+2,W-2*Margin,Bottom-Top-4},PinCancel={W-Margin-72,Top+8,64,24};
constexpr int PinKeys=12,PinTitleY=Top+10,PinHintY=Top+34,PinDotsY=Top+66,PinNoticeY=Top+84,PinKeyW=68,PinKeyH=34,PinKeyGap=6,PinKeysY=Top+104,PinDotR=5,PinDotStep=22,PinMaxDigits=8;
constexpr Rect pinKeyRect(int i){return {(W-(3*PinKeyW+2*PinKeyGap))/2+(i%3)*(PinKeyW+PinKeyGap),PinKeysY+(i/3)*(PinKeyH+PinKeyGap),PinKeyW,PinKeyH};}
constexpr const char *PinKeyLabel[PinKeys]={"1","2","3","4","5","6","7","8","9","DEL","0","OK"};
// What a key sends: a digit, '\b' for DEL, '\n' for OK.
constexpr char pinKeyChar(int i){return i<9?char('1'+i):i==9?'\b':i==10?'0':'\n';}
struct PinPadView {
 bool active=false,wrong=false;int digits=0,waitSeconds=0;// waitSeconds > 0: the backoff is running and the keys wait
 std::string title,hint,notice;
};
struct SecurityView {bool on=false,hide=false,usb=false;int require=0,focus=0;std::string requireName="Immediately",status,detail;};
}
