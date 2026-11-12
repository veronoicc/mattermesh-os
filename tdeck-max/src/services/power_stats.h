#pragma once
#include <Arduino.h>
#include <FS.h>
#include <vector>
#include "battery_math.h"
// On-time accounting (screen, GPS, modem, Wi-Fi / BLE, sleep, LoRa TX) and the 24 h battery history behind Settings > Battery.
// Every 5 minutes the main loop stores one BatterySample (gauge readings + the on-times of that interval) in a PSRAM-backed ring
// and appends it to /mattermesh/battery.bin on the SD card, which powerStatsBegin() reads back after a reboot.
namespace device {
void powerStatsBegin(fs::FS &fs,bool sdReady);
void powerStatsLoop();// main loop, every iteration: counts the elapsed time against what is on; samples when due
std::vector<BatterySample> batteryHistory();// the last 24 h, oldest first
OnTime powerOnTime();// the stored 24 h plus the interval still running
bool powerStatsStored();// the SD card holds the history (false: kept in RAM only)
}
