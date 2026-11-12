#pragma once
#include <string>
#include <vector>
#include "theme.h"
#include "services/scheduler.h"
// Scheduled app (ScheduledSection): list of waiting messages under a History card; one message (WHEN, TO, text, EDIT /
// SEND NOW / DELETE); the form (also opened by Chat options > Send later); the delete question; the History of the last
// sent and failed ones. Everything the renderer needs is in ScheduledView; scheduled_app.inc (schedCompose) fills it.
namespace mm {
constexpr int SchedRowH=38,SchedRows=5,SchedListY=Top+54,SchedCountY=Bottom-13;
constexpr Rect SchedHistoryCard={Margin,Top+4,W-2*Margin,44};
inline Rect schedRowRect(int i){return {Margin,SchedListY+i*SchedRowH,W-2*Margin,SchedRowH-2};}
// History: the same rows, SchedHistRows of them, newest first.
constexpr int SchedHistRows=6;
inline Rect schedHistRect(int i){return {Margin,Top+4+i*SchedRowH,W-2*Margin,SchedRowH-2};}
// One message.
constexpr Rect SchedItemWhen={Margin,Top+4,W-2*Margin,40},SchedItemTo={Margin,Top+48,W-2*Margin,40},SchedItemText={Margin,Top+92,W-2*Margin,96};
constexpr int SchedItemStateY=Top+196,SchedActionCount=3;
inline Rect schedActionRect(int i){int w=(W-2*Margin-2*6)/3;return {Margin+i*(w+6),Bottom-30,w,30};}
constexpr Rect SchedDeleteYes=ChatClearYes,SchedDeleteNo=ChatClearNo;
// Form: the text, the condition (< value >), for At time the five date / time fields with - / +, then SCHEDULE / CANCEL.
// Focus: 0 text, 1 condition, 2..6 the fields, 7 SCHEDULE, 8 CANCEL.
constexpr int SchedFocusCount=9,SchedFocusSave=7,SchedFocusCancel=8,SchedFormToY=Top+70,SchedFormLabelY=Top+122,SchedFormSummaryY=Top+202,SchedFormNoticeY=Top+216;
constexpr Rect SchedFormText={Margin,Top+14,W-2*Margin,52},SchedFormCond={Margin,Top+86,W-2*Margin,30};
inline Rect schedFieldRect(int i){constexpr int w[5]={38,38,56,38,38};int x=Margin;for(int k=0;k<i;k++)x+=w[k]+4;return {x,Top+138,w[i],30};}
constexpr Rect SchedFormMinus={Margin,Top+172,(W-2*Margin-6)/2,26},SchedFormPlus={W/2+3,Top+172,(W-2*Margin-6)/2,26};
constexpr Rect SchedFormSave={Margin,Bottom-30,(W-2*Margin-6)/2,30},SchedFormCancel={W/2+3,Bottom-30,(W-2*Margin-6)/2,30};
struct ScheduledView {
 struct Row {std::string when,meta;bool retry=false;};// list: "Tue 14:30" or the condition, "Alice: text"; retry: failed once, trying again
 struct Past {std::string head,meta;bool sent=true;};// history: "Sent 14:30  Alice" / "Failed ...", text or reason
 int view=0;// 0 list, 1 message, 2 form, 3 history, 4 delete question
 int focus=0,scroll=0,total=0;// list: 0 History card, 1.. rows; message 0..2 actions; history 0.. rows; delete 0 DELETE 1 CANCEL
 bool clock=true;// the wall clock is set
 std::vector<Row> rows;std::vector<Past> past;
 std::string itemWhen,itemTo,itemText,itemState;// message view
 // Form
 bool editing=false,typing=false,direct=false;int condition=0;int date[5]={1,1,2026,12,0};// editing: an existing message (SAVE), typing: the text field takes keys
 std::string text,target,summary;
};
}
