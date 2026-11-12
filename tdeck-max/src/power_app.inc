// Settings > Power / timeouts (20), Battery (5), Power profile (57) and the sleep glue of the main loop (light sleep between iterations, deep sleep).
// Included by main.cpp before the Motion app. Pure logic: services/sleep_policy.h, battery_math.h, power_profile.h; hardware: services/sleep.cpp, power_stats.cpp.
#include <cstdio>
#include "services/power_profile.h"
#include "services/power_stats.h"
#include "services/prefs.h"
static int batteryTab=0,profileFocus=1,profilePreview=1,powerChipField=0,powerConfirmFocus=1;static bool powerConfirm=false,profileLoaded=false;static device::Profile profileSaved=device::Profile::Normal;
static bool powerPage(){return !deviceLocked&&screen==Screen::Section&&(section==20||section==5||section==mm::PowerProfileSection);}
// What the settings amount to right now, read from the services the profile writes to.
static device::ProfileSettings profileNow(){
 device::ProfileSettings p;auto m=device::motionState();auto pw=device::powerState();auto sl=device::sleepSettings();
 p.gps=device::locationState().userEnabled;p.modem=device::cellularSettings().modem;p.motion=m.enabled;p.raise=m.raise;p.wifi=device::wifiOn();p.ble=device::bleOn();
 p.displayS=pw.displaySeconds;p.keyboardS=pw.keyboardSeconds;p.sleep=sl.enabled;p.sleepDelayS=sl.delayS;p.stayAwakeUsb=sl.stayAwakeUsb;return p;}
static device::Profile profileResolved(){if(!profileLoaded){profileLoaded=true;profileSaved=device::profileFromByte(uint8_t(device::prefGetInt("mmprofile","p",int(device::Profile::Normal))));}return device::resolveProfile(profileSaved,profileNow());}
// Writes a preset through the settings' own setters (the same ones their pages use); the profile is remembered when it was stored, and shows as Custom as soon as any of them differs.
static bool profileApply(device::Profile p){
 if(!device::profileHasPreset(p))return false;const auto &t=device::profilePreset(p);int failed=0;
 if(device::locationState().userEnabled!=t.gps&&!device::locationEnabled(t.gps))failed++;
 {auto c=device::cellularSettingsCopy();if(c.modem!=t.modem){c.modem=t.modem;if(!device::cellularApply(c))failed++;}}
 {auto m=device::motionState();if(m.enabled!=t.motion&&!device::motionEnable(t.motion))failed++;if(m.raise!=t.raise&&!device::motionRaiseToWake(t.raise))failed++;}
 if(device::wifiOn()!=t.wifi)device::wifiEnabled(t.wifi);if(device::bleOn()!=t.ble)device::bleEnabled(t.ble);
 {auto pw=device::powerState();if(pw.displaySeconds!=t.displayS&&!device::timeout(false,t.displayS))failed++;if(pw.keyboardSeconds!=t.keyboardS&&!device::timeout(true,t.keyboardS))failed++;}
 {auto sl=device::sleepSettings();if(sl.enabled!=t.sleep&&!device::sleepEnable(t.sleep))failed++;if(t.sleep&&sl.delayS!=t.sleepDelayS&&!device::sleepDelay(t.sleepDelayS))failed++;if(sl.stayAwakeUsb!=t.stayAwakeUsb&&!device::sleepStayAwakeUsb(t.stayAwakeUsb))failed++;}
 profileSaved=p;profileLoaded=true;profilePreview=int(p);if(!device::prefPutInt("mmprofile","p",int(p)))failed++;
 if(failed)notice=String(device::profileName(p))+": "+failed+" setting(s) not applied";else notice=String(device::profileName(p))+" profile applied";return failed==0;}
void powerOpen(int id){
 batteryTab=0;powerConfirm=false;powerConfirmFocus=1;powerField=0;powerChipField=0;
 if(id==mm::PowerProfileSection){profilePreview=int(profileResolved());profileFocus=profilePreview;}}
// A short text for seconds: "42 s", "3 min", "1 h 20 min".
static std::string spanText(uint32_t seconds){return seconds<60?std::to_string(seconds)+" s":device::durationText(int(seconds/60));}
void powerCompose(mm::State &v){
 const bool settings=!deviceLocked&&screen==Screen::Section&&section==19;
 if(!settings&&!powerPage())return;
 mm::PowerView &w=v.pw;auto sl=device::sleepSettings();w.sleepOn=sl.enabled;w.sleepDelayS=sl.delayS;w.stayUsb=sl.stayAwakeUsb;
 if(settings){w.profile=device::profileName(profileResolved());return;}// the Settings row shows which profile is active
 if(section==20){
  w.field=powerChipField;w.sleepPercent=device::sleepStats().lastHourPercent;w.confirm=powerConfirm;w.confirmFocus=powerConfirmFocus;
  device::SleepInputs in;in.stayAwakeUsb=sl.stayAwakeUsb;device::sleepEnvironment(in);w.sleepHint=sl.enabled?device::sleepBlockText(device::sleepBlockers(in)):"";// what keeps it awake right now
  return;}
 if(section==mm::PowerProfileSection){
  w.active=int(profileResolved());w.preview=profilePreview;w.focus=profileFocus;w.profile=device::profileName(device::Profile(w.active));
  w.rows=device::profileRows(profilePreview==int(device::Profile::Custom)?profileNow():device::profilePreset(device::Profile(profilePreview)));
  w.applicable=profilePreview!=w.active&&profilePreview!=int(device::Profile::Custom);return;}
 // Battery
 w.tab=batteryTab;time_t clock=time(nullptr);const bool timed=clock>=MM_EPOCH_MIN;auto history=device::batteryHistory();
 w.graph=device::batteryGraph(timed?history:std::vector<device::BatterySample>(),timed?uint32_t(clock):0,mm::BatteryColumns);w.stored=device::powerStatsStored();
 auto b=device::batteryState();auto g=device::batteryGauge();int ma=0;const bool haveAvg=timed&&device::averageCurrent(history,uint32_t(clock),3600,ma);if(!haveAvg&&g.valid){ma=g.avgMa;}
 const bool known=haveAvg||g.valid;w.avgValue=known?std::to_string(ma)+" mA":"--";w.nowValue=g.valid?std::to_string(g.currentMa)+" mA":"--";w.source=b.estimated?"Voltage est.":"BQ27220 gauge";
 const bool charging=b.charging||v.status.charging;int soc=b.soc>=0?b.soc:v.status.battery;
 if(b.full){w.timeLabel="TIME TO FULL";w.timeValue="Full";}
 else if(charging){w.timeLabel="TIME TO FULL";int m=g.ttfMin>=0?g.ttfMin:known?device::timeToFullMinutes(soc,ma):-1;w.timeValue=device::durationText(m);}
 else{w.timeLabel="TIME TO EMPTY";int m=g.tteMin>=0?g.tteMin:known?device::timeToEmptyMinutes(soc,ma):-1;w.timeValue=device::durationText(m);}
 auto o=device::powerOnTime();auto d=device::estimateDrain(o);w.usage.clear();
 for(int k=0;k<d.count;k++){char mah[16];snprintf(mah,sizeof(mah),"%.1f mAh",d.item[k].mah);mm::UsageRow r;r.name=d.item[k].name;r.detail=spanText(d.item[k].seconds)+"  "+mah;r.percent=device::sharePercent(d.item[k].mah,d.totalMah);w.usage.push_back(r);}
 char text[64];snprintf(text,sizeof(text),"Model avg %.0f mA, %.0f mAh in %s",device::averageModelMa(o),d.totalMah,device::durationText(int(o.windowS/60)).c_str());w.usageModel=text;
 w.usageGauge=g.valid?"Gauge now "+std::to_string(g.currentMa)+" mA, avg "+std::to_string(g.avgMa)+" mA":"Gauge not readable";
}
// ---- Power / timeouts
static void powerDeepSleepAsk(){
 if(matter::repeatEnabled()){notice="Turn Repeat off first (LoRa > Device)";render();return;}// a repeater that stops listening silently stops repeating
 if(device::modemEnabled()){notice="Turn the modem off first (Cellular)";render();return;}
 powerConfirm=true;powerConfirmFocus=1;notice="";render();}
// Lights off, radio asleep, rails off, the e-paper keeps "DEEP SLEEP" as its image, then the keyboard INT is the only way back (the chip restarts).
static void powerDeepSleep(){
 powerConfirm=false;device::audioCancel();stopVibration();if(!matter::radioSuspend(true)){notice="Radio would not stop";render();return;}
 device::powerLocked(true);setTouchEnabled(false);epd.epd2.setBusyCallback(nullptr);
 frame.fillScreen(mm::BG);{mm::Components c(frame);c.centered(8,128,224,"DEEP SLEEP",mm::Font::ChatTitle);c.centered(8,160,224,"Press a key to wake",mm::Font::ChatMeta);c.centered(8,176,224,"The device restarts",mm::Font::ChatMeta);}
 {mm::RefreshPlan plan;plan.changed=true;plan.full=true;mm::paintFramebuffer(epd.epd2,frame.getBuffer(),plan);refreshTracker.commit(frame.getBuffer(),plan);}
 if(expanderReady){for(int pin:{BOARD_XL9555_01_LORA_EN,BOARD_XL9555_02_GPS_EN,BOARD_XL9555_03_1V8_EN,BOARD_XL9555_05_MOTOR_EN,BOARD_XL9555_06_AMPLIFIER}){expander.pinMode(pin,OUTPUT);expander.digitalWrite(pin,LOW);}}
 {uint32_t until=millis()+1000;while(keyboardReady&&digitalRead(BOARD_KEYBOARD_INT)==LOW&&int32_t(until-millis())>0){keyboard.flush();delay(10);}}// the line must be high, or the first nap would be over at once
 Serial.println("DEEP_SLEEP");delay(50);device::sleepDeep();}
static void powerActivate(int row,int dir){
 auto p=device::powerState();auto sl=device::sleepSettings();
 if(row<2){powerChipField=row;uint32_t current=row?p.keyboardSeconds:p.displaySeconds;int i=0;while(i<6&&device::TIMEOUT_VALUES[i]!=current)i++;i=(i+6+(dir<0?-1:1))%6;notice=device::timeout(row!=0,device::TIMEOUT_VALUES[i])?"Timeout saved":"Save failed";}
 else if(row==2)notice=device::sleepEnable(!sl.enabled)?(sl.enabled?"Sleep off":"Sleep on"):"Save failed";
 else if(row==3)notice=device::sleepDelay(device::nextSleepDelay(sl.delayS,dir))?"Sleep delay saved":"Save failed";
 else if(row==4)notice=device::sleepStayAwakeUsb(!sl.stayAwakeUsb)?(sl.stayAwakeUsb?"Sleeps on USB too":"Awake on USB"):"Save failed";
 else{powerDeepSleepAsk();return;}
 render();}
// ---- keys, taps, back
bool powerKey(char c){
 if(!powerPage())return false;
 const bool next=c=='j'||c=='s'||c=='l'||c=='d',prev=c=='k'||c=='w'||c=='h',ok=c=='\n'||c==' ';
 if(!(next||prev||ok))return false;
 if(section==5){if(next||prev||ok){batteryTab=1-batteryTab;render();}return true;}
 if(section==mm::PowerProfileSection){
  if(next||prev){profileFocus=constrain(profileFocus+(next?1:-1),0,mm::ProfileApplyFocus);if(profileFocus<mm::ProfileCount)profilePreview=profileFocus;notice="";render();return true;}
  if(profileFocus<mm::ProfileCount){profileFocus=mm::ProfileApplyFocus;render();return true;}
  if(profilePreview!=int(profileResolved())&&profilePreview!=int(device::Profile::Custom))profileApply(device::Profile(profilePreview));else notice=profilePreview==int(device::Profile::Custom)?"Change a setting to get Custom":"Already active";
  render();return true;}
 // Power / timeouts
 if(powerConfirm){if(c=='\n'||c==' '){if(powerConfirmFocus==0)powerDeepSleep();else{powerConfirm=false;render();}}else{powerConfirmFocus=1-powerConfirmFocus;render();}return true;}
 if(c=='j'||c=='s'||c=='k'||c=='w'){powerField=constrain(powerField+((c=='j'||c=='s')?1:-1),0,mm::PowerRows-1);if(powerField<2)powerChipField=powerField;notice="";render();return true;}
 powerActivate(powerField,c=='h'?-1:1);return true;}
bool powerTap(int x,int y){
 if(!powerPage())return false;
 if(section==5){for(int i=0;i<mm::BatteryTabCount;i++)if(mm::batteryTabRect(i).contains(x,y)){batteryTab=i;render();}return true;}
 if(section==mm::PowerProfileSection){
  for(int i=0;i<mm::ProfileCount;i++)if(mm::profileCardRect(i).contains(x,y)){profileFocus=profilePreview=i;notice="";render();return true;}
  if(mm::ProfileApply.contains(x,y)){profileFocus=mm::ProfileApplyFocus;if(profilePreview!=int(profileResolved())&&profilePreview!=int(device::Profile::Custom))profileApply(device::Profile(profilePreview));else notice=profilePreview==int(device::Profile::Custom)?"Change a setting to get Custom":"Already active";render();}
  return true;}
 if(powerConfirm){if(mm::ChatClearYes.contains(x,y))powerDeepSleep();else if(mm::ChatClearNo.contains(x,y)){powerConfirm=false;render();}return true;}
 for(int i=0;i<mm::PowerRows;i++)if(mm::powerRowRect(i).contains(x,y)){powerField=i;if(i<2)powerChipField=i;powerActivate(i,i==3?mm::stepRowTap(mm::powerRowRect(i),x):1);return true;}
 for(int i=0;i<6;i++)if(mm::timeoutOptionRect(i).contains(x,y)){notice=device::timeout(powerChipField!=0,device::TIMEOUT_VALUES[i])?"Timeout saved":"Save failed";render();return true;}
 return true;}
bool powerBack(){if(powerPage()&&section==20&&powerConfirm){powerConfirm=false;render();return true;}return false;}
// ---- sleep between main loop iterations: the policy (sleep_policy.h) decides; what only the main loop knows is filled in here
static bool powerSleepStep(){
 auto p=device::powerState();if(p.displayPWM||p.keyboardPWM)return false;// lights on: awake (also while a notification flashes the keyboard)
 device::SleepInputs in;in.screenOff=true;uint32_t quiet=device::sleepQuietMs();if(!deviceLocked){uint32_t idle=uint32_t(millis()-p.lastActivity);if(idle<quiet)quiet=idle;}in.quietMs=quiet;
 in.radioBusy=matter::busy();
 in.uiBusy=dirty||radioDirty||lockScreenDirty||quickReply.active||touchPipeline.contactActive()||deleteHeld||pendingKeys.waiting()||keyboardQueue.size()>0||dictationChat>=0||unlockTaps.active()||unlockCountdownAt||(lockHintUntil&&int32_t(lockHintUntil-millis())>0)||hapticActive||screen==Screen::Welcome||welcomeWifi||uint32_t(millis()-lastPhysicalPaint)<1500;
 in.web=webActive()||radioLinkActive();in.sdBusy=device::networkJobActive();
 return device::sleepTry(in,!deviceLocked||quickReply.active,millis()+device::msToNextMinute(uint32_t(time(nullptr))));}// the clock on the e-paper moves once a minute
// ---- USB commands: sleep status, battery status, profile [field|normal|max|status], deep sleep
bool powerSerial(const String &s){
 if(s=="sleep status"){auto c=device::sleepSettings();auto t=device::sleepStats();device::SleepInputs in;in.stayAwakeUsb=c.stayAwakeUsb;device::sleepEnvironment(in);
  Serial.printf("SLEEP_STATUS enabled=%d delay=%lu usb_awake=%d naps=%lu slept_ms=%lu hour_percent=%d blockers=0x%lx env=%s last_wake=0x%lx quiet_ms=%lu\n",c.enabled,(unsigned long)c.delayS,c.stayAwakeUsb,(unsigned long)t.naps,(unsigned long)t.sleptMs,t.lastHourPercent,(unsigned long)t.lastBlockers,device::sleepBlockText(device::sleepBlockers(in)).c_str(),(unsigned long)t.lastWake,(unsigned long)device::sleepQuietMs());return true;}
 if(s.startsWith("sleep set ")){int a=s.indexOf(' ',10);String key=a<0?s.substring(10):s.substring(10,a);int value=a<0?0:s.substring(a+1).toInt();bool ok=key=="on"?device::sleepEnable(value!=0):key=="delay"?device::sleepDelay(value):key=="usb"?device::sleepStayAwakeUsb(value!=0):false;Serial.printf("SLEEP_SET ok=%d\n",ok);return true;}
 if(s=="battery status"){auto b=device::batteryState();auto g=device::batteryGauge();auto h=device::batteryHistory();auto o=device::powerOnTime();Serial.printf("BATTERY_STATUS percent=%d mv=%d gauge=%d current_ma=%d avg_ma=%d tte_min=%d ttf_min=%d samples=%u stored=%d window_s=%lu screen_s=%lu gps_s=%lu modem_s=%lu radio_s=%lu sleep_s=%lu tx_ms=%lu\n",b.percent,b.voltage,g.valid,g.currentMa,g.avgMa,g.tteMin,g.ttfMin,unsigned(h.size()),device::powerStatsStored(),(unsigned long)o.windowS,(unsigned long)o.screenS,(unsigned long)o.gpsS,(unsigned long)o.modemS,(unsigned long)o.radioS,(unsigned long)o.sleepS,(unsigned long)o.txMs);return true;}
 if(s=="profile"||s=="profile status"){auto p=profileResolved();auto n=profileNow();Serial.printf("PROFILE name=%s saved=%s gps=%d modem=%d motion=%d raise=%d wifi=%d ble=%d display=%lu keyboard=%lu sleep=%d delay=%lu usb=%d\n",device::profileName(p),device::profileName(profileSaved),n.gps,n.modem,n.motion,n.raise,n.wifi,n.ble,(unsigned long)n.displayS,(unsigned long)n.keyboardS,n.sleep,(unsigned long)n.sleepDelayS,n.stayAwakeUsb);return true;}
 if(s.startsWith("profile apply ")){String name=s.substring(14);device::Profile p=name=="field"?device::Profile::Field:name=="max"?device::Profile::Max:device::Profile::Normal;bool ok=(name=="field"||name=="normal"||name=="max")&&profileApply(p);Serial.printf("PROFILE_APPLY ok=%d name=%s\n",ok,device::profileName(profileResolved()));render();return true;}
 if(s=="deep sleep"){powerDeepSleepAsk();Serial.printf("DEEP_SLEEP_ASK confirm=%d notice=%s\n",powerConfirm,notice.c_str());return true;}
 if(s=="deep sleep now"){powerDeepSleep();return true;}
 return false;}
