#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
// Pure decisions for automatic ESP32-S3 light sleep (services/sleep.cpp does the hardware part). No Arduino.
namespace device {
// Why the device may not sleep right now (bit set); 0 = sleep is allowed.
enum SleepBlock:uint32_t {SleepOff=1,SleepScreenOn=2,SleepNotIdle=4,SleepUsbSerial=8,SleepUsbPower=16,SleepWifi=32,SleepBle=64,SleepWeb=128,SleepAudio=256,SleepOta=512,SleepSd=1024,SleepGps=2048,SleepModem=4096,SleepRadio=8192,SleepUi=16384,SleepHeld=32768,SleepPinActive=65536};
// What can wake it (bit set).
enum WakeSource:uint32_t {WakeRadio=1,WakeKeyboard=2,WakeTouch=4,WakeModem=8,WakeBoot=16,WakeTimer=32};
struct SleepInputs {
 bool enabled=false;// Settings > Power: Sleep when idle
 bool screenOff=false;// locked, or the lights timed out
 uint32_t quietMs=0,delayMs=30000;// time since the last key / touch / message, and the chosen Sleep delay
 bool usbSerial=false,usbPower=false,stayAwakeUsb=true;// a host reads the serial port; charger sees USB; Settings > Power: Stay awake on USB
 bool wifi=false,ble=false,web=false;// Wi-Fi, BLE (companion link) or MatterMesh Web needs the CPU
 bool audio=false,ota=false,sdBusy=false;// tone / dictation, firmware update, SD card write
 bool gps=false;// the receiver streams NMEA into a UART that only the awake CPU drains
 bool modemSession=false;// the modem is starting, searching or holds a data session (a registered idle modem wakes us by RI)
 bool radioBusy=false,uiBusy=false;// mesh TX / queued packets / packet being received; a repaint or touch gesture in flight
 uint32_t holds=0;// named sleepHold() reasons currently active
 uint32_t pinsActive=0;// WakeSource bits whose line is already at its active level (an interrupt is pending: serve it first)
};
inline uint32_t sleepBlockers(const SleepInputs &i){
 uint32_t b=0;
 if(!i.enabled)b|=SleepOff;if(!i.screenOff)b|=SleepScreenOn;if(i.quietMs<i.delayMs)b|=SleepNotIdle;
 if(i.usbSerial)b|=SleepUsbSerial;if(i.usbPower&&i.stayAwakeUsb)b|=SleepUsbPower;
 if(i.wifi)b|=SleepWifi;if(i.ble)b|=SleepBle;if(i.web)b|=SleepWeb;if(i.audio)b|=SleepAudio;if(i.ota)b|=SleepOta;if(i.sdBusy)b|=SleepSd;
 if(i.gps)b|=SleepGps;if(i.modemSession)b|=SleepModem;if(i.radioBusy)b|=SleepRadio;if(i.uiBusy)b|=SleepUi;if(i.holds)b|=SleepHeld;if(i.pinsActive)b|=SleepPinActive;
 return b;}
inline bool sleepAllowed(const SleepInputs &i){return sleepBlockers(i)==0;}
// Sleep delay choices (seconds) of Settings > Power.
constexpr uint32_t SLEEP_DELAYS[]={10,30,60,300};
inline bool validSleepDelay(uint32_t s){for(auto v:SLEEP_DELAYS)if(v==s)return true;return false;}
// The choice after / before `seconds` (wraps); an unknown value steps to 30 s.
inline uint32_t nextSleepDelay(uint32_t seconds,int dir){int n=int(sizeof(SLEEP_DELAYS)/sizeof(SLEEP_DELAYS[0])),at=-1;for(int k=0;k<n;k++)if(SLEEP_DELAYS[k]==seconds)at=k;if(at<0)return 30;return SLEEP_DELAYS[(at+n+(dir<0?-1:1))%n];}
// SX1262 DIO1 and the boot button always; the keyboard INT; the touch INT only while the touch controller runs (it is held in reset on the lock screen); modem RI only with the modem on; the timer.
inline uint32_t sleepWakeMask(bool modemOn,bool touchOn){return WakeRadio|WakeKeyboard|WakeBoot|WakeTimer|(modemOn?WakeModem:0)|(touchOn?WakeTouch:0);}
// A nap: until the earliest absolute deadline (millis, wrap-safe), at least SLEEP_MIN_NAP_MS (a late deadline wakes at once) and at most `capMs` (housekeeping).
constexpr uint32_t SLEEP_MIN_NAP_MS=20;
inline uint32_t sleepNapMs(uint32_t now,const uint32_t *deadlines,int n,uint32_t capMs){
 uint32_t nap=capMs;for(int k=0;k<n;k++){int32_t d=int32_t(deadlines[k]-now);uint32_t ms=d<=0?0:uint32_t(d);if(ms<nap)nap=ms;}
 return nap<SLEEP_MIN_NAP_MS?SLEEP_MIN_NAP_MS:nap;}
// Milliseconds until the clock shows the next minute (epoch seconds in): the lock screen redraws then.
inline uint32_t msToNextMinute(uint32_t epochSeconds){return (60-epochSeconds%60)*1000u;}
// Longest nap: housekeeping (DM ack timeouts, battery, USB plug-in are noticed this late); shorter while the touch controller runs
// (the CST328 INT is a short pulse) or the IMU is polled for raise to wake (its samples are batched in the sensor's FIFO).
constexpr uint32_t SLEEP_HOUSEKEEPING_MS=10000,SLEEP_TOUCH_POLL_MS=250,SLEEP_MOTION_POLL_MS=500;
inline uint32_t sleepCapMs(bool touchOn,bool motionRaise){uint32_t c=SLEEP_HOUSEKEEPING_MS;if(motionRaise)c=SLEEP_MOTION_POLL_MS;if(touchOn)c=SLEEP_TOUCH_POLL_MS;return c;}
// The lasting causes (not the momentary ones) the Power page names when the device cannot sleep.
constexpr uint32_t SleepEnvMask=SleepUsbSerial|SleepUsbPower|SleepWifi|SleepBle|SleepAudio|SleepOta|SleepGps|SleepModem;
inline std::string sleepBlockText(uint32_t mask){struct N {uint32_t bit;const char *name;};static const N names[]={{SleepUsbSerial,"USB serial"},{SleepUsbPower,"USB power"},{SleepWifi,"Wi-Fi"},{SleepBle,"Bluetooth"},{SleepAudio,"audio"},{SleepOta,"update"},{SleepGps,"GPS"},{SleepModem,"modem"}};
 std::string out;for(auto &n:names)if(mask&SleepEnvMask&n.bit){if(!out.empty())out+=", ";out+=n.name;}return out;}
// Share of the last 60 minutes spent asleep, in one-minute slots.
class SleepWindow {
 uint32_t minute[60]={},slept[60]={};bool used[60]={};
public:
 void add(uint32_t nowMs,uint32_t sleptMs){uint32_t m=nowMs/60000u;int k=int(m%60);if(!used[k]||minute[k]!=m){used[k]=true;minute[k]=m;slept[k]=0;}slept[k]+=sleptMs;}
 int percent(uint32_t nowMs)const{uint32_t m=nowMs/60000u;uint64_t sum=0;for(int k=0;k<60;k++)if(used[k]&&minute[k]<=m&&m-minute[k]<60)sum+=slept[k];
  uint32_t span=nowMs<3600000u?nowMs:3600000u;if(!span)return 0;uint64_t p=sum*100/span;return p>100?100:int(p);}
};
// Reasons other services give for staying awake (hold) or for being woken at a time (wake request): a small fixed table, matched by name.
class SleepHolds {
public:
 static constexpr int Slots=8;
private:
 struct Slot {char name[16]={};uint32_t until=0;bool forever=false,wake=false,used=false;};
 Slot slot[Slots];
 static bool live(const Slot &s,uint32_t now){return s.used&&(s.forever||int32_t(s.until-now)>0);}
 Slot *find(const char *name,bool wake,bool create,uint32_t now){Slot *free=nullptr;for(auto &s:slot){if(s.used&&s.wake==wake&&!strncmp(s.name,name,sizeof(s.name)-1))return &s;if(!free&&!live(s,now))free=&s;}if(!create||!free)return nullptr;*free=Slot();free->used=true;free->wake=wake;strncpy(free->name,name,sizeof(free->name)-1);return free;}
public:
 // ms==0: until release(). Again with the same name: extended, not duplicated. false: no free slot.
 bool hold(const char *name,uint32_t now,uint32_t ms){Slot *s=find(name,false,true,now);if(!s)return false;s->forever=ms==0;s->until=now+ms;return true;}
 void release(const char *name){for(auto &s:slot)if(s.used&&!s.wake&&!strncmp(s.name,name,sizeof(s.name)-1))s=Slot();}
 // Ask to be woken `inMs` from now (a periodic job). Replaces the earlier request of the same name.
 bool wake(const char *name,uint32_t now,uint32_t inMs){Slot *s=find(name,true,true,now);if(!s)return false;s->forever=false;s->until=now+inMs;return true;}
 int count(uint32_t now)const{int n=0;for(auto &s:slot)if(!s.wake&&live(s,now))n++;return n;}
 bool held(uint32_t now)const{return count(now)>0;}
 // Absolute deadlines of the live wake requests; returns how many.
 int deadlines(uint32_t now,uint32_t *out)const{int n=0;for(auto &s:slot)if(s.wake&&live(s,now))out[n++]=s.until;return n;}
 // The first live hold's name, for diagnostics.
 const char *firstHold(uint32_t now)const{for(auto &s:slot)if(!s.wake&&live(s,now))return s.name;return "";}
};
}
