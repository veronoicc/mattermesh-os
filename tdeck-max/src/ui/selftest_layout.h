#pragma once
#include "theme.h"
// Self-test app (section 60): summary card, six check rows (scrollable), RUN ALL / RUN ONE / EXPORT buttons.
namespace mm {
constexpr Rect SelfTestSummary={Margin,Top+3,W-2*Margin,36};
constexpr int SelfTestRows=6,SelfTestRowH=31,SelfTestListY=Top+42,SelfTestButtons=3;
inline Rect selfTestRow(int i){return {Margin,SelfTestListY+i*SelfTestRowH,W-2*Margin-8,SelfTestRowH-2};}// 8 px right of the rows: the scroll bar
constexpr Rect SelfTestScrollBar={W-Margin-4,SelfTestListY,4,SelfTestRows*SelfTestRowH-2};
// Focus order after the rows: 0 RUN ALL (STOP while running), 1 RUN ONE (the focused row), 2 EXPORT.
inline Rect selfTestButton(int i){return {Margin+i*((W-2*Margin-12)/3+6),Top+234,(W-2*Margin-12)/3,26};}
// Interactive checks cover the list: a card with the instruction and, for the touch check, the square to tap.
constexpr Rect SelfTestPrompt={Margin,SelfTestListY,W-2*Margin,SelfTestRows*SelfTestRowH-2},SelfTestTarget={(W-72)/2,SelfTestListY+76,72,72};
// Options sheet: interactive checks, sound / vibration checks.
constexpr int SelfTestOptionRows=2,SelfTestOptionRowH=40,SelfTestOptionHeadH=22;
constexpr Rect SelfTestPopup={10,Top+6,W-20,SelfTestOptionHeadH+SelfTestOptionRows*SelfTestOptionRowH+2};
inline Rect selfTestOptionRow(int i){return {SelfTestPopup.x+3,SelfTestPopup.y+SelfTestOptionHeadH+i*SelfTestOptionRowH,SelfTestPopup.w-6,SelfTestOptionRowH-2};}
}
