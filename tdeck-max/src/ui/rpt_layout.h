#pragma once
#include "theme.h"
// RPT Scanner (section 53): the repeater list (sort tabs, SCAN, five rows), its options sheet, the repeater page with
// PING / STATUS / LOGIN, the password page, the admin console and the contact picker of DM PATH.
namespace mm {
// List: four sort tabs, the SCAN button with the state line beside it, then the rows. The header menu button opens the options.
constexpr int RptTabCount=4,RptTabGap=4,RptTabW=(W-2*Margin-(RptTabCount-1)*RptTabGap)/RptTabCount;
constexpr int RptRows=5,RptRowH=38,RptListY=Top+64;
constexpr Rect RptMenu=ComposeButton,RptScan={Margin,Top+32,76,28},RptState={Margin+82,Top+32,W-2*Margin-82,28};
constexpr Rect rptTabRect(int i){return {Margin+i*(RptTabW+RptTabGap),Top+3,RptTabW,24};}
constexpr Rect rptRowRect(int i){return {Margin,RptListY+i*RptRowH,W-2*Margin,RptRowH-2};}
// Options sheet: sort, hide stale, scan window, show RSSI.
constexpr int RptMenuRows=4,RptMenuRowH=40,RptMenuHeadH=22;
constexpr Rect RptMenuPopup={10,Top+6,W-20,RptMenuHeadH+RptMenuRows*RptMenuRowH+2};
constexpr Rect rptMenuRow(int i){return {RptMenuPopup.x+3,RptMenuPopup.y+RptMenuHeadH+i*RptMenuRowH,RptMenuPopup.w-6,RptMenuRowH-2};}
// Repeater page: facts card, three actions, DM PATH, the answer card.
constexpr int RptActionCount=4;
constexpr Rect RptInfo={Margin,Top+2,W-2*Margin,88},RptDm={Margin,Top+128,W-2*Margin,26},RptAnswer={Margin,Top+160,W-2*Margin,Bottom-(Top+160)};
constexpr Rect rptAction(int i){return i==3?RptDm:Rect{Margin+i*((W-2*Margin-2*Gap)/3+Gap),Top+96,(W-2*Margin-2*Gap)/3,28};}
// Password page: the field, then SHOW / CANCEL / LOGIN.
constexpr Rect RptField={Margin,Top+44,W-2*Margin,34},RptShow={Margin,Top+84,64,28},RptCancel={Margin+70,Top+84,72,28},RptGo={Margin+148,Top+84,W-2*Margin-148,28};
constexpr int RptLoginNoteY=Top+122;
// Console: output lines, the input line, a bar of five quick commands.
constexpr int RptLineH=13,RptQuickCount=5,RptQuickGap=4;
constexpr Rect RptBar={Margin,Bottom-34,W-2*Margin,34};
constexpr int RptInputY=Bottom-34-RptLineH-4;
constexpr Rect RptOut={Margin,Top+4,W-2*Margin,RptInputY-(Top+4)-2};
constexpr Rect rptQuick(int i){return {RptBar.x+4+i*((RptBar.w-8-(RptQuickCount-1)*RptQuickGap)/RptQuickCount+RptQuickGap),RptBar.y+3,(RptBar.w-8-(RptQuickCount-1)*RptQuickGap)/RptQuickCount,RptBar.h-6};}
// DM PATH contact picker: five rows, PREV / NEXT / CANCEL.
constexpr int RptPickRows=5;
constexpr Rect rptPickRow(int i){return {Margin+8,Top+34+i*32,W-2*Margin-16,28};}
constexpr Rect rptPickButton(int i){return {Margin+8+i*((W-2*Margin-16-2*6)/3+6),Top+200,(W-2*Margin-16-2*6)/3,30};}
}
