#pragma once
#include <string>
#include "theme.h"
#include "cellular_page.h"
// Settings > Cellular (51): the Modem switch and the status card stay on top; below them CellVisible rows scroll
// (page buttons on the label line), the group label above them names the first visible row's group.
namespace mm {
constexpr Rect CellularSwitch=SettingsSwitch,CellularStatus={Margin,Top+50,W-2*Margin,58};
constexpr int CellularLabelY=Top+112;
constexpr Rect CellularUp={W-Margin-72,Top+110,34,20},CellularDown={W-Margin-34,Top+110,34,20};
inline Rect cellularRow(int slot){return {Margin,Top+132+slot*32,W-2*Margin,28};}
// Everything the renderer needs; cellularCompose (cellular_app.inc) fills it.
struct CellularView {
 bool modem=false,simReady=false,roaming=false;
 device::ModemPhase phase=device::ModemPhase::Off;device::DataPhase data=device::DataPhase::Idle;
 std::string model,op,ip,message;int csq=99,bars=0,rat=0;
 int focus=0,scroll=0,editing=-1;std::string input;// editing: the row being typed in (-1 none)
 std::string value[CellRows];bool on[CellRows]={};// display text of each row and its switch state
 std::string mqttState,mqttCounts;// the MQTT status line (two lines)
};
}
