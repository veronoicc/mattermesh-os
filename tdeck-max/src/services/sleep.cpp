#include "sleep.h"
#include <atomic>
#include <Preferences.h>
#include <TDeckMaxBoard.h>
#include "audio.h"
#include "battery.h"
#include "connectivity.h"
#include "location.h"
#include "modem.h"
#include "motion.h"
#include "ota.h"
#ifndef MM_EMULATOR
#include <HWCDC.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#endif
namespace device {
static Preferences prefs;static SleepSettings cfg;static SleepHolds holds;static SleepStats stats;static SleepWindow window;static std::atomic_flag lock=ATOMIC_FLAG_INIT;
static volatile uint32_t activityAt=0;
struct Guard {Guard(){while(lock.test_and_set(std::memory_order_acquire)){}}~Guard(){lock.clear(std::memory_order_release);}};
constexpr uint32_t POWER_SOURCE_EVERY_MS=3000;// the charger is read over I2C at most this often
void sleepBegin(){
#ifndef MM_EMULATOR
 gpio_deep_sleep_hold_dis();for(int pin:{BOARD_LORA_CS,BOARD_LORA_RST,BOARD_SD_CS,BOARD_EPD_CS,BOARD_EPD_DC,BOARD_EPD_RST,BOARD_EPD_BL,BOARD_KEYBOARD_LED})gpio_hold_dis(gpio_num_t(pin));// pads held for a deep sleep are free again after the restart it ends with
#endif
 prefs.begin("mmsleep",false);cfg.enabled=prefs.getBool("on",true);cfg.delayS=prefs.getUInt("delay",60);if(!validSleepDelay(cfg.delayS))cfg.delayS=60;cfg.stayAwakeUsb=prefs.getBool("usb",true);activityAt=millis();}
SleepSettings sleepSettings(){Guard g;return cfg;}
bool sleepEnable(bool on){if(prefs.putBool("on",on)!=1)return false;Guard g;cfg.enabled=on;return true;}
bool sleepDelay(uint32_t seconds){if(!validSleepDelay(seconds)||prefs.putUInt("delay",seconds)!=sizeof(uint32_t))return false;Guard g;cfg.delayS=seconds;return true;}
bool sleepStayAwakeUsb(bool on){if(prefs.putBool("usb",on)!=1)return false;Guard g;cfg.stayAwakeUsb=on;return true;}
bool sleepHold(const char *reason,uint32_t ms){Guard g;return holds.hold(reason,millis(),ms);}
void sleepRelease(const char *reason){Guard g;holds.release(reason);}
bool sleepWakeIn(const char *reason,uint32_t ms){Guard g;return holds.wake(reason,millis(),ms);}
void sleepNoteActivity(){activityAt=millis();}
uint32_t sleepQuietMs(){return uint32_t(millis()-activityAt);}
SleepStats sleepStats(){Guard g;SleepStats s=stats;s.lastHourPercent=window.percent(millis());return s;}
uint64_t sleepTotalMs(){Guard g;return stats.sleptMs;}
bool sleepUsbHost(){
#ifdef MM_EMULATOR
 return true;// the emulator's serial port is a PC pipe
#else
 return HWCDC::isPlugged();// the host's 1 ms SOF packets; a charger or power bank sends none
#endif
}
void sleepEnvironment(SleepInputs &in){
 in.usbSerial=sleepUsbHost();
 static uint32_t powerAt=0;static bool usb=false;uint32_t now=millis();if(!powerAt||uint32_t(now-powerAt)>=POWER_SOURCE_EVERY_MS){powerAt=now|1;usb=batteryPowerSource()==PowerSource::USB;}in.usbPower=usb;
 in.wifi=wifiOn();in.ble=bleOn();in.audio=audioBusy();in.gps=gpsActive();
 in.modemSession=false;if(modemEnabled()){auto m=modemStatus();in.modemSession=m.phase!=ModemPhase::Registered||m.data!=DataPhase::Idle;}// a registered modem with no data session wakes us by its RI line
 auto o=otaState();in.ota=o.phase==OtaPhase::Checking||o.phase==OtaPhase::Downloading||o.phase==OtaPhase::Verifying||o.phase==OtaPhase::Restarting;
}
#ifndef MM_EMULATOR
struct WakePin {int gpio;gpio_int_type_t level;uint32_t source;};
// SX1262 DIO1 is active high; the TCA8418 keyboard, CST328 touch, A7682E RI and the boot button are active low. All are plain digital pads:
// light sleep wakes on them with gpio_wakeup_enable() (level triggered), no RTC pad / ext1 needed. Deep sleep is where the RTC pad matters.
static const WakePin WAKE_PINS[]={{BOARD_LORA_INT,GPIO_INTR_HIGH_LEVEL,WakeRadio},{BOARD_KEYBOARD_INT,GPIO_INTR_LOW_LEVEL,WakeKeyboard},{BOARD_TOUCH_INT,GPIO_INTR_LOW_LEVEL,WakeTouch},{BOARD_A7682E_RI,GPIO_INTR_LOW_LEVEL,WakeModem},{BOARD_BOOT_PIN,GPIO_INTR_LOW_LEVEL,WakeBoot}};
static bool pinActive(const WakePin &p){return digitalRead(p.gpio)==(p.level==GPIO_INTR_HIGH_LEVEL?HIGH:LOW);}
// Output pads that keep their level through a nap: chip selects and resets of the shared SPI bus (the backlights are LEDC outputs, off while asleep).
static const int HOLD_PINS[]={BOARD_LORA_CS,BOARD_LORA_RST,BOARD_SD_CS,BOARD_EPD_CS,BOARD_EPD_DC,BOARD_EPD_RST};
static uint32_t activePins(uint32_t mask){uint32_t a=0;for(auto &p:WAKE_PINS)if((mask&p.source)&&pinActive(p))a|=p.source;return a;}
static uint32_t nap(uint32_t ms,uint32_t mask){
 esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
 if(mask&WakeRadio)gpio_intr_disable(gpio_num_t(BOARD_LORA_INT));// RadioLib's rising-edge handler must not see the wake level
 for(auto &p:WAKE_PINS)if(mask&p.source)gpio_wakeup_enable(gpio_num_t(p.gpio),p.level);// the line was checked inactive just before (activePins), so a pending interrupt never makes a nap of zero length
 esp_sleep_enable_gpio_wakeup();esp_sleep_enable_timer_wakeup(uint64_t(ms)*1000u);
 for(int pin:HOLD_PINS)gpio_hold_en(gpio_num_t(pin));
 esp_light_sleep_start();// no Serial.flush(): nothing sleeps while a USB host listens, so the output would be dropped anyway
 for(int pin:HOLD_PINS)gpio_hold_dis(gpio_num_t(pin));
 uint32_t woke=0;if(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_TIMER)woke|=WakeTimer;
 for(auto &p:WAKE_PINS)if(mask&p.source){if(pinActive(p))woke|=p.source;gpio_wakeup_disable(gpio_num_t(p.gpio));}// gpio_wakeup_disable also resets the pad's interrupt type
 // RadioLib armed DIO1 for the rising edge, which the level wake replaced: if the line is still high (a packet waits) hand the driver one level interrupt so its
 // "packet ready" flag is set, then back to the edge.
 if(mask&WakeRadio){if(digitalRead(BOARD_LORA_INT)==HIGH){gpio_set_intr_type(gpio_num_t(BOARD_LORA_INT),GPIO_INTR_HIGH_LEVEL);gpio_intr_enable(gpio_num_t(BOARD_LORA_INT));delayMicroseconds(30);}gpio_intr_disable(gpio_num_t(BOARD_LORA_INT));gpio_set_intr_type(gpio_num_t(BOARD_LORA_INT),GPIO_INTR_POSEDGE);gpio_intr_enable(gpio_num_t(BOARD_LORA_INT));}
 return woke;}
#endif
bool sleepTry(SleepInputs in,bool touchOn,uint32_t wakeAtMs){
 SleepSettings s=sleepSettings();uint32_t now=millis();
 in.enabled=s.enabled;in.delayMs=s.delayS*1000u;in.stayAwakeUsb=s.stayAwakeUsb;
 if(!in.enabled||!in.screenOff||in.quietMs<in.delayMs||in.uiBusy||in.radioBusy||in.web||in.sdBusy){Guard g;stats.lastBlockers=sleepBlockers(in);return false;}// the cheap checks first
 sleepEnvironment(in);
 uint32_t deadlines[SleepHolds::Slots+1];int n;{Guard g;in.holds=uint32_t(holds.count(now));n=holds.deadlines(now,deadlines);}
 if(wakeAtMs)deadlines[n++]=wakeAtMs;
 uint32_t mask=sleepWakeMask(modemEnabled(),touchOn);
#ifdef MM_EMULATOR
 {Guard g;stats.lastBlockers=sleepBlockers(in);}return false;// the PC cannot light-sleep: the policy still runs, so the Power page reports the same blockers
#else
 in.pinsActive=activePins(mask&~WakeTimer);
 uint32_t b=sleepBlockers(in);{Guard g;stats.lastBlockers=b;}if(b)return false;
 auto m=motionState();uint32_t ms=sleepNapMs(now,deadlines,n,sleepCapMs(touchOn,m.enabled&&m.raise&&m.phase==MotionPhase::Ready));
 uint64_t t0=esp_timer_get_time();uint32_t woke=nap(ms,mask);uint32_t slept=uint32_t((esp_timer_get_time()-t0)/1000);// esp_timer keeps counting through light sleep
 if(woke&(WakeKeyboard|WakeTouch|WakeBoot|WakeModem))sleepNoteActivity();
 {Guard g;stats.naps++;stats.lastWake=woke;stats.sleptMs+=slept;window.add(millis(),slept);}
 return true;
#endif
}
void sleepDeep(){
#ifdef MM_EMULATOR
 Serial.println("DEEP_SLEEP");Serial.flush();exit(0);
#else
 pinMode(BOARD_EPD_BL,OUTPUT);digitalWrite(BOARD_EPD_BL,LOW);pinMode(BOARD_KEYBOARD_LED,OUTPUT);digitalWrite(BOARD_KEYBOARD_LED,LOW);
 for(int pin:HOLD_PINS)gpio_hold_en(gpio_num_t(pin));gpio_hold_en(gpio_num_t(BOARD_EPD_BL));gpio_hold_en(gpio_num_t(BOARD_KEYBOARD_LED));gpio_deep_sleep_hold_en();// chip selects and backlights stay as they are
 rtc_gpio_pullup_en(gpio_num_t(BOARD_KEYBOARD_INT));rtc_gpio_pulldown_dis(gpio_num_t(BOARD_KEYBOARD_INT));// the TCA8418 INT is open drain, low while a key waits (the caller drained the FIFO, so it is high now)
 esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);esp_sleep_enable_ext0_wakeup(gpio_num_t(BOARD_KEYBOARD_INT),0);
 esp_deep_sleep_start();for(;;){}
#endif
}
}
