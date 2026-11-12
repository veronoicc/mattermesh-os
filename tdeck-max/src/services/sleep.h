#pragma once
#include <Arduino.h>
#include "sleep_policy.h"
// Automatic ESP32-S3 light sleep while the screen is off / locked and the device is idle, and the explicit deep sleep.
// The decision is sleep_policy.h; this file owns the settings (NVS "mmsleep"), the named holds of other services and the hardware.
namespace device {
struct SleepSettings {bool enabled=true;uint32_t delayS=60;bool stayAwakeUsb=true;};
struct SleepStats {uint32_t naps=0,lastBlockers=0,lastWake=0;int lastHourPercent=0;uint64_t sleptMs=0;};// lastWake: WakeSource bits of the latest nap
void sleepBegin();// loads the settings
SleepSettings sleepSettings();
bool sleepEnable(bool on);bool sleepDelay(uint32_t seconds);bool sleepStayAwakeUsb(bool on);// Settings > Power / timeouts; false if NVS refused or the value is not offered
// Other services: stay awake for `ms` (0 = until sleepRelease) for `reason`, or ask to be woken in `ms` for periodic work (GPS track sampling,
// MQTT keep-alive, scheduled messages, battery history). Any task; the same reason again replaces the earlier request. false: all 8 slots are taken.
bool sleepHold(const char *reason,uint32_t ms=0);void sleepRelease(const char *reason);bool sleepWakeIn(const char *reason,uint32_t ms);
// Input that restarts the idle clock (a key, a received message, lock / unlock, a wake from the keyboard, touch or modem line).
void sleepNoteActivity();uint32_t sleepQuietMs();
// Fills what the hardware and the services say (USB host / charger, Wi-Fi, BLE, audio, update, GPS, modem session): the Power page and sleepTry share it.
void sleepEnvironment(SleepInputs &in);
// Main loop, once per iteration when it has nothing to do: sleeps one nap when the policy allows. The caller fills screenOff, quietMs, radioBusy,
// uiBusy, web, sdBusy (what only it knows); the rest is filled in here. wakeAtMs: an absolute millis() deadline of the caller's own (0: none).
// touchOn: the touch controller runs. Returns true after a nap (millis() has moved on); false: stayed awake.
bool sleepTry(SleepInputs in,bool touchOn,uint32_t wakeAtMs=0);
SleepStats sleepStats();uint64_t sleepTotalMs();
// A host reads the USB serial port (never sleep: the USB peripheral does not survive light sleep).
bool sleepUsbHost();
// Deep sleep: lights off, the keyboard INT (GPIO15, RTC pad) is the only wake source; the chip restarts when it fires. Never returns.
// The caller powers the radio / GPS / IMU rails down first (they are XL9555 outputs that survive it).
[[noreturn]] void sleepDeep();
}
