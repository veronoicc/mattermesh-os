#include <cassert>
#include <iostream>
#include "services/sleep_policy.h"
int main(){
 using namespace device;
 // A fully idle, screen-off device with sleep enabled may sleep.
 SleepInputs i;i.enabled=true;i.screenOff=true;i.quietMs=40000;i.delayMs=30000;i.stayAwakeUsb=true;
 assert(sleepBlockers(i)==0&&sleepAllowed(i));
 // Each blocker alone stops it and is reported by its own bit.
 auto only=[&](auto set,uint32_t bit){SleepInputs j=i;set(j);assert(sleepBlockers(j)==bit&&!sleepAllowed(j));};
 only([](SleepInputs &j){j.enabled=false;},SleepOff);
 only([](SleepInputs &j){j.screenOff=false;},SleepScreenOn);
 only([](SleepInputs &j){j.quietMs=29999;},SleepNotIdle);
 only([](SleepInputs &j){j.usbSerial=true;},SleepUsbSerial);
 only([](SleepInputs &j){j.usbPower=true;},SleepUsbPower);
 only([](SleepInputs &j){j.wifi=true;},SleepWifi);
 only([](SleepInputs &j){j.ble=true;},SleepBle);
 only([](SleepInputs &j){j.web=true;},SleepWeb);
 only([](SleepInputs &j){j.audio=true;},SleepAudio);
 only([](SleepInputs &j){j.ota=true;},SleepOta);
 only([](SleepInputs &j){j.sdBusy=true;},SleepSd);
 only([](SleepInputs &j){j.gps=true;},SleepGps);
 only([](SleepInputs &j){j.modemSession=true;},SleepModem);
 only([](SleepInputs &j){j.radioBusy=true;},SleepRadio);
 only([](SleepInputs &j){j.uiBusy=true;},SleepUi);
 only([](SleepInputs &j){j.holds=2;},SleepHeld);
 only([](SleepInputs &j){j.pinsActive=WakeKeyboard;},SleepPinActive);
 // USB power only blocks when "Stay awake on USB" is on; the serial link blocks regardless.
 {SleepInputs j=i;j.usbPower=true;j.stayAwakeUsb=false;assert(sleepAllowed(j));j.usbSerial=true;assert(sleepBlockers(j)==SleepUsbSerial);}
 // Exactly at the delay it sleeps; several blockers add up.
 {SleepInputs j=i;j.quietMs=30000;assert(sleepAllowed(j));j.wifi=j.gps=true;assert(sleepBlockers(j)==(SleepWifi|SleepGps));}
 // Delay choices: 10 s, 30 s, 1 min, 5 min; anything else is rejected.
 assert(validSleepDelay(10)&&validSleepDelay(30)&&validSleepDelay(60)&&validSleepDelay(300)&&!validSleepDelay(0)&&!validSleepDelay(45));
 assert(nextSleepDelay(10,1)==30&&nextSleepDelay(300,1)==10&&nextSleepDelay(10,-1)==300&&nextSleepDelay(60,-1)==30&&nextSleepDelay(7,1)==30);
 // Wake sources: radio DIO1, keyboard and boot always; touch only with a live touch controller; modem RI only with the modem on.
 assert(sleepWakeMask(false,false)==(WakeRadio|WakeKeyboard|WakeBoot|WakeTimer));
 assert(sleepWakeMask(true,false)==(WakeRadio|WakeKeyboard|WakeBoot|WakeTimer|WakeModem));
 assert(sleepWakeMask(false,true)==(WakeRadio|WakeKeyboard|WakeBoot|WakeTimer|WakeTouch));
 // Next wake: earliest deadline, never below the minimum nap nor above the housekeeping cap; no deadline = the cap.
 {uint32_t now=1000,d[]={now+5000,now+800,now+90000};
  assert(sleepNapMs(now,d,3,60000)==800);
  assert(sleepNapMs(now,d,0,60000)==60000);
  uint32_t far[]={now+500000};assert(sleepNapMs(now,far,1,60000)==60000);
  uint32_t past[]={now-5};assert(sleepNapMs(now,past,1,60000)==SLEEP_MIN_NAP_MS);
  uint32_t soon[]={now+3};assert(sleepNapMs(now,soon,1,60000)==SLEEP_MIN_NAP_MS);}
 // Wrap-around of the 32-bit millisecond counter.
 {uint32_t now=0xFFFFFF00u,d[]={now+0x300u};assert(sleepNapMs(now,d,1,60000)==0x300u);}
 // Milliseconds to the next minute boundary (lock screen clock).
 assert(msToNextMinute(0)==60000&&msToNextMinute(59)==1000&&msToNextMinute(60)==60000&&msToNextMinute(125)==55000);
 // Longest nap: housekeeping, shorter while the touch controller runs (its INT is a short pulse) or the IMU is polled for raise to wake.
 assert(sleepCapMs(false,false)==SLEEP_HOUSEKEEPING_MS&&sleepCapMs(true,false)==SLEEP_TOUCH_POLL_MS&&sleepCapMs(false,true)==SLEEP_MOTION_POLL_MS&&sleepCapMs(true,true)==SLEEP_TOUCH_POLL_MS&&SLEEP_TOUCH_POLL_MS<SLEEP_MOTION_POLL_MS&&SLEEP_MOTION_POLL_MS<SLEEP_HOUSEKEEPING_MS);
 // What the Power page tells the user keeps the device awake: only the lasting causes, in a fixed order.
 assert(sleepBlockText(0)==""&&sleepBlockText(SleepWifi)=="Wi-Fi"&&sleepBlockText(SleepBle|SleepWifi)=="Wi-Fi, Bluetooth"&&sleepBlockText(SleepUsbSerial|SleepGps|SleepModem)=="USB serial, GPS, modem");
 assert(sleepBlockText(SleepScreenOn|SleepNotIdle|SleepUi|SleepRadio|SleepHeld|SleepSd|SleepOff)==""&&(SleepEnvMask&SleepScreenOn)==0&&(SleepEnvMask&SleepWifi)!=0);
 // Last-hour window: empty = 0 %, half asleep = 50 %, old minutes fall out.
 {SleepWindow w;assert(w.percent(0)==0);
  w.add(60000,30000);w.add(120000,30000);assert(w.percent(120000)==50);// 60 s asleep of the 120 s lived
  SleepWindow k;for(uint32_t m=1;m<=60;m++)k.add(m*60000u,60000);assert(k.percent(3600000)==100);
  k.add(3660000u,0);assert(k.percent(3660000)==98);// the oldest minute fell out and the new minute was awake
  SleepWindow z;z.add(10000,5000);assert(z.percent(10000)==50);}
 // Named holds and wake requests: a hold blocks while its time lasts, requests expire and the earliest wins.
 {SleepHolds h;assert(!h.held(0)&&h.count(0)==0);
  h.hold("tracks",0,5000);assert(h.held(4999)&&h.count(4999)==1&&!h.held(5000));
  h.hold("tracks",1000,5000);assert(h.count(1000)==1);// same reason: extended, not duplicated
  assert(h.held(5999)&&!h.held(6000));
  h.hold("audio",0,0);assert(h.held(1000000));h.release("audio");assert(!h.held(1000000));
  h.wake("mqtt",1000,9000);h.wake("scheduled",1000,4000);uint32_t out[SleepHolds::Slots];assert(h.deadlines(1000,out)==2);
  assert(sleepNapMs(1000,out,2,60000)==4000);assert(h.deadlines(20000,out)==0);
  SleepHolds g;const char *names[8]={"a","b","c","d","e","f","g","h"};for(int n=0;n<8;n++)assert(g.hold(names[n],0,0));assert(g.count(1)==SleepHolds::Slots&&!g.hold("overflow",0,0)&&g.count(1)==SleepHolds::Slots);}
 std::cout<<"PASS: sleep policy: blockers, wake sources, nap length, last-hour sleep window, named holds\n";
}
