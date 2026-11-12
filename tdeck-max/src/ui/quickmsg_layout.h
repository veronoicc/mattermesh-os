#pragma once
#include <string>
#include <vector>
#include "theme.h"
namespace mm {
// Settings > Quick messages (QuickMessagesSection): the Quick key and Mode < value > rows, QuickRows preset rows (scrolling), a hint /
// notice line, the actions of the focused preset (EDIT UP DOWN PIN DEL) and + ADD / RESET. Focus: 0 key, 1 mode, 2+i preset i, then ADD, RESET.
constexpr int QuickRows=4,QuickActionCount=5,QuickPickRows=8,QuickPresetMax=8;
constexpr Rect QuickKeyRow={Margin,Top+2,W-2*Margin,30},QuickModeRow={Margin,Top+34,W-2*Margin,30};
inline Rect quickRowRect(int i){return {Margin,Top+70+i*31,W-2*Margin,29};}
constexpr int QuickCountY=Top+195;
inline Rect quickActionRect(int i){int w=(W-2*Margin-4*4)/5;return {Margin+i*(w+4),Top+210,w,26};}
constexpr Rect QuickAdd={Margin,Top+240,(W-2*Margin-6)/2,22},QuickReset={W/2+3,Top+240,(W-2*Margin-6)/2,22};
// Delete / reset ask first (the Tracks dialog): the buttons under the text.
constexpr Rect QuickConfirmYes=ChatClearYes,QuickConfirmNo=ChatClearNo;
// The picker over a chat (overlayPanel): a title, one row per preset, a notice line, CANCEL.
inline Rect quickPickRect(int i){return {Margin+8,Top+30+i*23,W-2*Margin-16,21};}
constexpr int QuickPickNoticeY=Top+216;
constexpr Rect QuickPickCancel={Margin+8,Top+232,W-2*Margin-16,22};
struct QuickView {
 struct Row {std::string text;bool pin=false;Row(std::string t=std::string(),bool p=false):text(std::move(t)),pin(p){}};
 std::vector<Row> rows;// the presets; a preset being added is the last row, empty
 int focus=0,scroll=0,editing=-1,confirm=0,confirmFocus=0;// confirm: 1 delete the focused preset, 2 reset to the defaults
 std::string input,keyLabel="Off",modeLabel="Send now",summary;
 bool picker=false;int pickFocus=0;std::string pickNotice;
};
}
