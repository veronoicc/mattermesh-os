#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "rx_log_layout.h"
namespace mm {
// RX Log app state for the renderer, filled by rxLogCompose (rx_log_app.inc): the visible rows (newest first), the
// toolbar, the options sheet and the packet detail.
struct RxLogRow {std::string time,type,route,hops,rssi,snr,hash,length;bool selected=false;};
struct RxLogView {
 std::vector<RxLogRow> rows;int first=0,shown=0,stored=0,fresh=0;bool ready=true,paused=false;std::string filterName="All";bool filtered=false;
 bool options=false,clearArmed=false,uptime=false,autoPause=true;int optionFocus=0;std::string defaultFilterName="All";
 bool detail=false;std::string detailTitle;std::vector<std::string> detailLines;int detailScroll=0;
};
}
