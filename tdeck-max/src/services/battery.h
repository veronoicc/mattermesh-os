#pragma once
#include "policy.h"
namespace device {
struct BatteryState {int percent=-1,voltage=-1,soc=-1;bool gauge=false,charger=false,full=false,charging=false,estimated=false;};
int batteryFullMillivolts();bool batteryCalibrateFull();
enum class PowerSource {Unknown,Battery,USB};PowerSource batteryPowerSource();bool batteryShutdown();
void batteryLoop();BatteryState batteryState();
// BQ27220 current (mA, negative = discharging), its running average, and the gauge's own time to empty / full in minutes (-1: it cannot tell). Read every 10 s.
struct BatteryGauge {bool valid=false;int currentMa=0,avgMa=0,tteMin=-1,ttfMin=-1;};void batteryGaugeLoop();BatteryGauge batteryGauge();
}
