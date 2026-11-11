#pragma once
#include "theme.h"
// RX Log app (section RxLogSection): filter + pause toolbar, status line, two-line packet rows, options sheet, packet detail.
namespace mm {
constexpr int RxLogRows=7,RxLogRowH=28,RxListY=Top+50;
constexpr Rect RxFilterPrev={Margin,Top+4,28,26},RxFilterLabel={Margin+30,Top+4,88,26},RxFilterNext={Margin+120,Top+4,28,26},RxPauseButton={Margin+154,Top+4,W-2*Margin-154,26};
constexpr int RxStatusY=Top+34;
inline Rect rxRowRect(int i){return {Margin,RxListY+i*RxLogRowH,W-2*Margin,RxLogRowH-2};}
// Row columns (x from the row's left edge): time, type | RSSI, SNR, hash; route + hops and the length sit at the right edge.
constexpr int RxColType=66,RxColSnr=58,RxColHash=114;
// Options sheet: default type, time format, auto-pause, export, clear, done.
constexpr int RxOptionCount=6,RxOptionRowH=36,RxOptionHeadH=22;
constexpr Rect RxOptionsPopup={Margin,Top+2,W-2*Margin,RxOptionHeadH+RxOptionCount*RxOptionRowH+2};
inline Rect rxOptionRect(int i){return {RxOptionsPopup.x+3,RxOptionsPopup.y+RxOptionHeadH+i*RxOptionRowH,RxOptionsPopup.w-6,RxOptionRowH-2};}
// Detail: one packet's lines in a panel, UP / DOWN / CLOSE under it.
constexpr int RxDetailRows=14,RxDetailLineH=13;
constexpr Rect RxDetailPanel={Margin,Top+4,W-2*Margin,Bottom-Top-8};
constexpr int RxDetailTextY=RxDetailPanel.y+28;
inline Rect rxDetailButton(int i){int w=(RxDetailPanel.w-16-12)/3;return {RxDetailPanel.x+8+i*(w+6),RxDetailPanel.y+RxDetailPanel.h-34,w,28};}
}
