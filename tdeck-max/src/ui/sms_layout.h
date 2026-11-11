#pragma once
#include <string>
#include <vector>
#include "theme.h"
// SMS app (section 52): inbox, message view, compose. The view is filled by sms_app.inc, drawn by ui/sms_renderer.inc.
namespace mm {
enum class SmsMode:uint8_t {Inbox,Read,Compose,Confirm};// Confirm: "delete this message?" over the message view
struct SmsRow {std::string number,time,preview;bool unread=false;};
struct SmsView {
 SmsMode mode=SmsMode::Inbox;
 // Inbox: the rows of the page on screen (start = index of the first one in the whole list, newest first), the focused row.
 std::vector<SmsRow> rows;int start=0,focus=0,total=0,unread=0;
 std::string status,hint;// why there is nothing to read ("" when the modem is registered)
 // Message view: sender, time, text; scroll = first text line shown; readFocus 0 REPLY, 1 DELETE, 2 BACK.
 std::string number,time,text;int scroll=0,readFocus=0;
 // Compose: field 0 number, 1 text, 2 SEND, 3 CANCEL; counter "units/limit"; result = why the last send failed.
 std::string to,body,counter,result;int field=0;bool canSend=false,sending=false,numberBad=false,over=false;
};
constexpr int SmsRows=5,SmsRowH=44,SmsListY=Top+4,SmsPagerY=SmsListY+SmsRows*SmsRowH+4,SmsBodyLineH=15,SmsTextMaxLines=6,SmsReadLines=10;
inline Rect smsRow(int i){return {Margin,SmsListY+i*SmsRowH,W-2*Margin,SmsRowH-4};}
constexpr Rect SmsPrev={W-Margin-2*34-Gap,SmsPagerY,34,28},SmsNext={W-Margin-34,SmsPagerY,34,28};
// Message view: sender card, the text card (upper half scrolls up, lower half down), REPLY / DELETE / BACK, the delete dialog.
constexpr Rect SmsReadHead={Margin,Top+4,W-2*Margin,40},SmsReadBody={Margin,Top+50,W-2*Margin,SmsReadLines*SmsBodyLineH+20};
constexpr Rect SmsReadUp={Margin,SmsReadBody.y,W-2*Margin,SmsReadBody.h/2},SmsReadDown={Margin,SmsReadBody.y+SmsReadBody.h/2,W-2*Margin,SmsReadBody.h/2};
inline Rect smsReadButton(int i){int w=(W-2*Margin-2*Gap)/3;return {Margin+i*(w+Gap),Bottom-36,w,30};}
constexpr Rect SmsConfirmCard={Margin,Top+40,W-2*Margin,Top+173+30+10-(Top+40)},SmsConfirmYes=ChatClearYes,SmsConfirmNo=ChatClearNo;
// Compose: number field, text field (grows with its wrapped lines; smsTextRect(SmsTextMaxLines) is the touch area), counter, result, buttons.
constexpr Rect SmsNumber={Margin,Top+18,W-2*Margin,30};
inline Rect smsTextRect(int lines){lines=lines<1?1:lines>SmsTextMaxLines?SmsTextMaxLines:lines;return {Margin,Top+72,W-2*Margin,18+lines*SmsBodyLineH};}
constexpr int SmsCounterY=Top+72+18+SmsTextMaxLines*SmsBodyLineH+6,SmsResultY=SmsCounterY+16;
constexpr Rect SmsSend={Margin,Bottom-38,(W-2*Margin-Gap)/2,32},SmsCancel={Margin+(W-2*Margin-Gap)/2+Gap,Bottom-38,(W-2*Margin-Gap)/2,32};
}
#include "sms_page.h"
