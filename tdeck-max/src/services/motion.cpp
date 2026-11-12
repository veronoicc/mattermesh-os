#include "motion.h"
#include "motion_policy.h"
#include "prefs.h"
#include "location.h"
#include "services/motion_chip.h"
#include <TDeckMaxBoard.h>
namespace device {
constexpr uint32_t POWER_SETTLE_MS=40,PROBE_RETRY_MS=100,LOAD_SLICE_MS=12,POLL_MS=40,FRESH_MS=2000;constexpr int PROBE_TRIES=3;constexpr float SAMPLE_HZ=25.0f;
static MotionChip chip;static MotionState state;static RaiseDetector raise;static HeadingTracker heading;static ExtensionIOXL9555 *io=nullptr;
static bool raisePending=false;static int probes=0;static uint32_t phaseAt=0,pollAt=0,accelAt=0,rotationAt=0;static float shownHeading=-1000;
static void power(bool on){if(!io)return;io->pinMode(BOARD_XL9555_03_1V8_EN,OUTPUT);io->digitalWrite(BOARD_XL9555_03_1V8_EN,on?HIGH:LOW);}
static void phase(MotionPhase p,const char *error=""){state.phase=p;state.error=error;state.loadPercent=0;phaseAt=millis();state.revision++;}
static void shutdown(){chip.stop();power(false);raise.reset();heading.reset();raisePending=false;state.accelFresh=state.headingValid=state.anchored=false;accelAt=rotationAt=0;phase(MotionPhase::Off);}
static void onAccel(float x,float y,float z,uint32_t ms){state.ax=x;state.ay=y;state.az=z;accelAt=millis();if(state.raise&&raise.feed(ms,x,y,z)){raisePending=true;state.raises++;}}
static void onRotation(float x,float y,float z,float w,uint32_t){heading.sample(quatHeadingDegrees(x,y,z,w),millis());rotationAt=millis();}
void motionBegin(ExtensionIOXL9555 &expander,bool ready){
 state.enabled=prefGetInt("mmmotion","on",1)!=0;state.raise=prefGetInt("mmmotion","raise",0)!=0;state.sensitivity=constrain(prefGetInt("mmmotion","sens",1),0,2);state.headingUp=prefGetInt("mmmotion","hup",0)!=0;state.telemetry=prefGetInt("mmmotion","telem",0)!=0;
 raise.setSensitivity(state.sensitivity);MotionChip::accelSink=onAccel;MotionChip::rotationSink=onRotation;
 if(ready)io=&expander;if(!io)phase(MotionPhase::Absent,"no IO expander");else if(state.enabled){power(true);phase(MotionPhase::Powering);}else power(false);
}
void motionLoop(){
 uint32_t now=millis();
 if(!state.enabled){if(state.phase!=MotionPhase::Off)shutdown();return;}
 switch(state.phase){
  case MotionPhase::Off:if(io){power(true);probes=0;phase(MotionPhase::Powering);}return;
  case MotionPhase::Powering:{if(uint32_t(now-phaseAt)<(probes?PROBE_RETRY_MS:POWER_SETTLE_MS))return;
   if(chip.probe(Wire,BOARD_I2C_SDA,BOARD_I2C_SCL)){phase(MotionPhase::Loading);return;}
   if(++probes>=PROBE_TRIES){const char *why=chip.error();power(false);phase(MotionPhase::Absent,why);return;}phaseAt=now;return;}
  case MotionPhase::Loading:{int p=chip.load(LOAD_SLICE_MS);
   if(p<0){const char *why=chip.error();chip.stop();power(false);phase(MotionPhase::Failed,why);return;}
   if(p<100){if(p/5!=state.loadPercent/5)state.revision++;state.loadPercent=p;return;}
   bool ok=chip.configure(MotionChip::Accel,SAMPLE_HZ)&&chip.configure(MotionChip::Rotation,SAMPLE_HZ);
   if(!ok){chip.stop();power(false);phase(MotionPhase::Failed,"sensors missing in firmware");return;}
   phase(MotionPhase::Ready);state.loadPercent=100;pollAt=now;return;}
  case MotionPhase::Ready:{
   if(uint32_t(now-pollAt)<POLL_MS)return;pollAt=now;chip.poll();
   auto loc=locationState();heading.course(loc.courseDeg,loc.speedKmh,loc.courseValid&&loc.fix&&loc.fixAgeMs<3000,now);
   state.accelFresh=sampleFresh(now,accelAt,FRESH_MS);state.anchored=heading.anchoredEver();state.anchorAgeMs=heading.anchorAge(now);
   bool valid=heading.valid(now)&&sampleFresh(now,rotationAt,FRESH_MS);state.headingValid=valid;state.heading=valid?heading.current():0;
   if(valid!=(shownHeading>=0)||(valid&&fabsf(angleDelta(shownHeading,state.heading))>=5)){shownHeading=valid?state.heading:-1000;state.revision++;}
   return;}
  default:return;}
}
MotionState motionState(){return state;}
static bool saveInt(const char *key,int value){return prefPutInt("mmmotion",key,value);}
bool motionEnable(bool on){if(!saveInt("on",on))return false;state.enabled=on;if(!on)shutdown();state.revision++;return true;}
bool motionRaiseToWake(bool on){if(!saveInt("raise",on))return false;state.raise=on;raise.reset();raisePending=false;state.revision++;return true;}
bool motionSetSensitivity(int level){level=constrain(level,0,2);if(!saveInt("sens",level))return false;state.sensitivity=level;raise.setSensitivity(level);state.revision++;return true;}
bool motionHeadingUp(bool on){if(!saveInt("hup",on))return false;state.headingUp=on;state.revision++;return true;}
bool motionShareTelemetry(bool on){if(!saveInt("telem",on))return false;state.telemetry=on;state.revision++;return true;}
bool motionRaiseTake(){bool r=raisePending;raisePending=false;return r;}
bool motionHeading(float &degrees){if(state.phase!=MotionPhase::Ready||!state.headingValid)return false;degrees=state.heading;return true;}
bool motionTelemetry(float accelG[3],float &headingDeg){
 if(!state.telemetry||state.phase!=MotionPhase::Ready||!state.accelFresh)return false;
 accelG[0]=state.ax/GRAVITY;accelG[1]=state.ay/GRAVITY;accelG[2]=state.az/GRAVITY;headingDeg=state.headingValid?state.heading:NAN;return true;}
}
