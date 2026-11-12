#pragma once
#include <string>
#include <vector>
#include "theme.h"
#include "services/battery_math.h"
#include "services/power_profile.h"
// Power pages: Settings > Power / timeouts (20), Battery (5) and Power profile (57). The view is filled by power_app.inc, drawn by ui/power_renderer.inc.
namespace mm {
// ---- Power / timeouts, focus order: display timeout 0, keyboard timeout 1, Sleep when idle 2, Sleep delay 3, Stay awake on USB 4, Deep sleep now 5.
constexpr int PowerRows=6;
inline Rect powerRowRect(int i){return i<2?Rect{Margin,Top+15+i*34,W-2*Margin,32}:Rect{Margin,Top+141+(i-2)*30,W-2*Margin,28};}
inline Rect timeoutOptionRect(int i){return {Margin+i*38,Top+98,34,24};}
constexpr int PowerLightsLabelY=Top+1,PowerChipsLabelY=Top+84,PowerSleepLabelY=Top+126;
// ---- Battery: GRAPH / USAGE tabs; graph view: header card, 24 h graph, four stat cards; usage view: one row per subsystem and two lines under them.
constexpr int BatteryTabCount=2;
inline Rect batteryTabRect(int i){int w=(W-2*Margin-6)/2;return {Margin+i*(w+6),Top+3,w,24};}
constexpr Rect BatteryHeader={Margin,Top+31,W-2*Margin,44},BatteryGraph={Margin,Top+80,W-2*Margin,92};
constexpr int BatteryStatCount=4;
inline Rect batteryStatRect(int i){return settingsHalf(i%2,Top+178+(i/2)*42,38);}
constexpr int BatteryColumns=48,BatteryUsageRowCount=7,BatteryUsageLabelY=Top+31,BatteryTotalY=Top+232;
inline Rect batteryUsageRect(int i){return {Margin,Top+47+i*26,W-2*Margin,24};}
// ---- Power profile: four profile cards (2 x 2), what the chosen one sets, APPLY. Focus 0..3 cards, 4 APPLY.
constexpr int ProfileCount=4,ProfileApplyFocus=4,ProfileLabelY=Top+84;
inline Rect profileCardRect(int i){int w=(W-2*Margin-6)/2;return {Margin+(i%2)*(w+6),Top+4+(i/2)*38,w,34};}
constexpr Rect ProfileList={Margin,Top+98,W-2*Margin,136},ProfileApply={Margin,Top+238,W-2*Margin,24};
struct UsageRow {std::string name,detail;int percent=0;};
struct PowerView {
 // Power / timeouts
 int field=0;// 0 display, 1 keyboard: the timeouts the chips set
 bool sleepOn=true,stayUsb=true,confirm=false;int confirmFocus=1;uint32_t sleepDelayS=60;int sleepPercent=0;std::string sleepHint;// sleepHint: what keeps it awake now ("" = nothing)
 // Battery
 int tab=0;std::vector<device::GraphColumn> graph;std::string timeLabel="TIME TO EMPTY",timeValue="--",avgValue="--",nowValue="--",source;bool stored=false;
 std::vector<UsageRow> usage;std::string usageModel,usageGauge;
 // Power profile
 int active=1,preview=1,focus=0;bool applicable=false;std::vector<device::ProfileRow> rows;std::string profile;
};
}
