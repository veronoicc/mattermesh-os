#include "power_stats.h"
#include <algorithm>
#include <time.h>
#include "battery.h"
#include "clock_limits.h"
#include "connectivity.h"
#include "location.h"
#include "modem.h"
#include "power.h"
#include "sd_replace.h"
#include "sleep.h"
#include "../mesh/service.h"
namespace device {
constexpr const char *FILE_PATH="/mattermesh/battery.bin",*TMP_PATH="/mattermesh/battery.tmp",*BAK_PATH="/mattermesh/battery.bak";
constexpr size_t FILE_RECORDS_MAX=2*BatteryHistory::Max;// the file is rewritten from the ring once it holds this many records
constexpr uint32_t RETRY_MS=30000;// no clock or no battery reading yet: look again after this
static BatteryHistory history;static fs::FS *card=nullptr;static bool stored=false;static size_t fileRecords=0;
struct Counters {uint64_t screenMs=0,gpsMs=0,modemMs=0,radioMs=0,sleepMs=0,txMs=0;};
static Counters total,base;static uint32_t lastTick=0,intervalAt=0,retryAt=0;
static uint16_t clampS(uint64_t ms){uint64_t s=ms/1000;return uint16_t(s>65535?65535:s);}
static bool writeAll(){if(!card)return false;File f=card->open(TMP_PATH,FILE_WRITE);if(!f)return false;bool ok=true;uint8_t b[BATTERY_SAMPLE_BYTES];for(auto &s:history.samples()){packSample(s,b);if(f.write(b,sizeof(b))!=sizeof(b)){ok=false;break;}}f.close();if(!ok){card->remove(TMP_PATH);return false;}if(!sdReplace(*card,TMP_PATH,FILE_PATH,BAK_PATH))return false;fileRecords=history.samples().size();return true;}
static void append(const BatterySample &s){if(!card||!stored)return;if(fileRecords>=FILE_RECORDS_MAX){writeAll();return;}File f=card->open(FILE_PATH,FILE_APPEND);if(!f){stored=false;return;}uint8_t b[BATTERY_SAMPLE_BYTES];packSample(s,b);bool ok=f.write(b,sizeof(b))==sizeof(b);f.close();if(ok)fileRecords++;else stored=false;}
void powerStatsBegin(fs::FS &fs,bool sdReady){
 history.reserve();lastTick=intervalAt=millis();if(!sdReady)return;card=&fs;stored=true;// reserve: the 7 KB ring is one allocation, which the heap puts in PSRAM
 File f=fs.open(sdSavedPath(fs,FILE_PATH,BAK_PATH));size_t n=0;uint8_t b[BATTERY_SAMPLE_BYTES];BatterySample s;
 if(f){while(f.read(b,sizeof(b))==sizeof(b)){n++;if(unpackSample(b,s))history.add(s);}f.close();}
 fileRecords=n;if(n>FILE_RECORDS_MAX||(n&&history.samples().size()<n))writeAll();// drops what fell out of the 24 h window
}
std::vector<BatterySample> batteryHistory(){return history.samples();}
bool powerStatsStored(){return stored;}
static Counters delta(){Counters d;d.screenMs=total.screenMs-base.screenMs;d.gpsMs=total.gpsMs-base.gpsMs;d.modemMs=total.modemMs-base.modemMs;d.radioMs=total.radioMs-base.radioMs;d.sleepMs=total.sleepMs-base.sleepMs;d.txMs=total.txMs-base.txMs;return d;}
OnTime powerOnTime(){
 OnTime o;time_t clock=time(nullptr);if(clock>=MM_EPOCH_MIN)o=sumOnTime(history.samples(),uint32_t(clock),BATTERY_WINDOW_S);
 Counters d=delta();o.windowS+=uint32_t((millis()-intervalAt)/1000);o.screenS+=uint32_t(d.screenMs/1000);o.gpsS+=uint32_t(d.gpsMs/1000);o.modemS+=uint32_t(d.modemMs/1000);o.radioS+=uint32_t(d.radioMs/1000);o.sleepS+=uint32_t(d.sleepMs/1000);o.txMs+=uint32_t(d.txMs);return o;}
void powerStatsLoop(){
 uint32_t t=millis(),dt=t-lastTick;lastTick=t;
 if(powerState().displayPWM>0)total.screenMs+=dt;if(gpsActive())total.gpsMs+=dt;if(modemEnabled())total.modemMs+=dt;if(wifiOn()||bleOn())total.radioMs+=dt;
 total.sleepMs=sleepTotalMs();total.txMs=matter::txAirtimeMs();// sleep and TX airtime are counted where they happen
 uint32_t since=t-intervalAt;
 if(since<BATTERY_SAMPLE_S*1000u){sleepWakeIn("power-stats",BATTERY_SAMPLE_S*1000u-since);return;}
 if(retryAt&&int32_t(t-retryAt)<0){sleepWakeIn("power-stats",retryAt-t);return;}
 time_t clock=time(nullptr);auto b=batteryState();auto g=batteryGauge();
 if(clock<MM_EPOCH_MIN||b.percent<0){retryAt=t+RETRY_MS;return;}// the interval keeps growing meanwhile
 retryAt=0;Counters d=delta();BatterySample s;s.t=uint32_t(clock);s.soc=uint8_t(b.percent);s.flags=uint8_t((b.charging?SampleCharging:0)|(batteryPowerSource()==PowerSource::USB?SampleUsb:0));s.mv=uint16_t(b.voltage<0?0:b.voltage);s.ma=int16_t(g.valid?g.avgMa:0);
 s.spanS=clampS(since);s.screenS=clampS(d.screenMs);s.gpsS=clampS(d.gpsMs);s.modemS=clampS(d.modemMs);s.radioS=clampS(d.radioMs);s.sleepS=clampS(d.sleepMs);s.txDs=uint16_t(std::min<uint64_t>(d.txMs/100,65535));
 if(history.add(s))append(s);
 base=total;intervalAt=t;
}
}
