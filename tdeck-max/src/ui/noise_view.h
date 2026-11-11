#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "../services/noise_stats.h"
#include "noise_layout.h"
namespace mm {
constexpr int NoiseLive=0,NoisePaused=1,NoiseOffline=2,NoiseSuspended=3,NoiseBusy=4;// status of the sampler
// Noise Floor app: everything the page draws (tenths of dBm unless noted); filled by noiseCompose.
struct NoiseView {
 std::vector<noise::Column> cols;noise::Scale scale;
 bool hasCurrent=false,warn=false,hasPacket=false,warnOn=true,options=false;
 int status=NoiseLive,valid=0,current=0,min=0,max=0,avg=0,floorDbm=0,excess=0,rate=1,window=0,threshold=10,focus=0,optionFocus=0,windowSecs=60,rateSecs=5,stride=1;
 int packetRssi=0,packetSnr=0;uint32_t packetAgeSecs=0;std::vector<uint32_t> marks;// packets heard inside the window: age in ms, newest first
 std::string toast;
};
}
