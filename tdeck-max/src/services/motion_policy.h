#pragma once
#include <stdint.h>
#include <math.h>
namespace device {
constexpr float GRAVITY=9.80665f;
// Angles in degrees, compass style: 0 north, clockwise.
inline float wrapDegrees(float d){d=fmodf(d,360.0f);if(d<0)d+=360.0f;return d>=360.0f?0.0f:d;}
// Shortest signed turn from one angle to the other, (-180, 180].
inline float angleDelta(float from,float to){float d=wrapDegrees(to-from);return d>180.0f?d-360.0f:d;}
inline float smoothAngle(float current,float target,float alpha){return wrapDegrees(current+alpha*angleDelta(current,target));}
inline const char *compassName(float degrees){static const char *names[]={"N","NE","E","SE","S","SW","W","NW"};return names[int((wrapDegrees(degrees)+22.5f)/45.0f)%8];}
// A sample stamped `at` (0 = none yet) is fresh within windowMs of now; a stamp after now (set during a poll that follows reading the clock) counts as fresh.
inline bool sampleFresh(uint32_t now,uint32_t at,uint32_t windowMs){return at&&int32_t(now-at)<int32_t(windowMs);}
// Bearing of a point relative to where the device faces (0 ahead, 90 to the right), whole degrees 0..359.
inline int relativeBearing(int bearing,int heading){return int(wrapDegrees(float(bearing-heading)));}
// Heading of the device from its rotation quaternion (game rotation vector: x,y,z,w, device to world, z up): the top edge while
// the device is held flatter than 45 degrees, otherwise the direction the back faces (held up in front of you).
inline float quatHeadingDegrees(float x,float y,float z,float w){
 float r01=2*(x*y-z*w),r11=1-2*(x*x+z*z),r02=2*(x*z+y*w),r12=2*(y*z-x*w),r22=1-2*(x*x+y*y);bool flat=fabsf(r22)>=0.7071f;
 return wrapDegrees(atan2f(flat?r01:-r02,flat?r11:-r12)*57.29578f);}

// Raise to wake. level 0 Low, 1 Medium, 2 High: how far the device must tilt, how firm the lift is and how long it is then held.
struct MotionSensitivity {float tiltDeg,excursion;uint32_t holdMs;};
inline MotionSensitivity motionSensitivity(int level){switch(level){case 0:return {50.0f,3.0f,500};case 2:return {25.0f,1.2f,250};default:return {38.0f,2.0f,350};}}
inline const char *motionSensitivityName(int level){return level==0?"Low":level==2?"High":"Medium";}
// Takes accelerometer samples (m/s2, device frame, any mounting: only the angle between poses counts) and answers true once when
// a pick-up has just ended: the device was still, moved for 0.15..3 s with a firm lift, tilted far enough, and is now still at a
// reading angle (screen roughly facing up or down, not flat on a table) for the hold time. Walking never settles; a knock changes no pose.
class RaiseDetector {
 public:
  RaiseDetector(int l=1):level(l){}
  void setSensitivity(int l){level=l;}
  void reset(){*this=RaiseDetector(level);}
  bool feed(uint32_t now,float ax,float ay,float az){
   float m=sqrtf(ax*ax+ay*ay+az*az);if(m<1.0f){seen=false;return false;}// free fall / garbage: start over
   float ux=ax/m,uy=ay/m,uz=az/m,excursion=fabsf(m-GRAVITY),step=0;uint32_t dt=seen?uint32_t(now-last):0;
   if(seen){float dot=ux*px+uy*py+uz*pz;dot=dot>1?1:dot<-1?-1:dot;if(dt<10)dt=10;step=acosf(dot)*57.29578f*1000.0f/float(dt);}
   px=ux;py=uy;pz=uz;last=now;
   if(!seen){seen=true;stableSince=now;return false;}
   if(dt>1000){stableSince=0;haveRest=false;moved=false;evaluated=false;}// sampling paused: the rest pose is stale
   bool still=excursion<STILL_BAND&&step<STILL_RATE_DEG_S;
   if(!still){if(stableSince){stableSince=0;evaluated=false;if(!moved){moved=true;motionSince=now;peak=0;}}if(excursion>peak)peak=excursion;return false;}
   if(!stableSince)stableSince=now;
   auto s=motionSensitivity(level);if(evaluated||uint32_t(now-stableSince)<s.holdMs)return false;
   evaluated=true;bool fire=false;
   if(!haveRest)haveRest=true;
   else{float dot=ux*rx+uy*ry+uz*rz;dot=dot>1?1:dot<-1?-1:dot;float tilt=acosf(dot)*57.29578f,flat=fabsf(uz);uint32_t span=uint32_t(stableSince-motionSince);
    fire=moved&&peak>=s.excursion&&span>=MIN_MOVE_MS&&span<=MAX_MOVE_MS&&tilt>=s.tiltDeg&&flat>=VIEW_FLAT_MIN&&flat<=VIEW_FLAT_MAX&&!(cooling&&int32_t(now-cooldownUntil)<0);}
   rx=ux;ry=uy;rz=uz;moved=false;peak=0;if(fire){cooling=true;cooldownUntil=now+COOLDOWN_MS;}return fire;}
 private:
  static constexpr float STILL_BAND=1.0f,STILL_RATE_DEG_S=90.0f,VIEW_FLAT_MIN=0.35f,VIEW_FLAT_MAX=0.985f;
  static constexpr uint32_t MIN_MOVE_MS=150,MAX_MOVE_MS=3000,COOLDOWN_MS=3000;
  int level=1;bool seen=false,haveRest=false,moved=false,evaluated=false,cooling=false;float px=0,py=0,pz=1,rx=0,ry=0,rz=1,peak=0;uint32_t last=0,stableSince=0,motionSince=0,cooldownUntil=0;
};

// The BHI260AP has no magnetometer on this board: the game rotation vector gives the turning, GPS course over ground (while moving)
// anchors it to the compass. Re-anchoring moves the offset a fraction of the way each second so GPS jitter does not shake the arrow.
constexpr uint32_t HEADING_VALID_MS=15*60*1000,HEADING_ANCHOR_INTERVAL_MS=1000;
constexpr float HEADING_MIN_SPEED_KMH=4.0f,HEADING_ANCHOR_GAIN=0.25f,HEADING_SMOOTHING=0.25f;
class HeadingTracker {
 public:
  void sample(float rawHeading,uint32_t now){(void)now;raw=rawHeading;rawKnown=true;if(anchored){float t=wrapDegrees(raw+offset);out=haveOut?smoothAngle(out,t,HEADING_SMOOTHING):t;haveOut=true;}}
  void course(float courseDeg,float speedKmh,bool courseValid,uint32_t now){
   if(!courseValid||speedKmh<HEADING_MIN_SPEED_KMH||!rawKnown)return;
   if(anchored&&uint32_t(now-anchorAt)<HEADING_ANCHOR_INTERVAL_MS)return;
   float target=wrapDegrees(courseDeg-raw);if(!anchored){offset=target;anchored=true;}else offset=wrapDegrees(offset+HEADING_ANCHOR_GAIN*angleDelta(offset,target));
   anchorAt=now;out=wrapDegrees(raw+offset);haveOut=true;}
  bool valid(uint32_t now)const{return anchored&&haveOut&&uint32_t(now-anchorAt)<=HEADING_VALID_MS;}
  bool anchoredEver()const{return anchored;}
  uint32_t anchorAge(uint32_t now)const{return anchored?uint32_t(now-anchorAt):UINT32_MAX;}
  float current()const{return out;}
  void reset(){*this=HeadingTracker();}
 private:
  bool rawKnown=false,anchored=false,haveOut=false;float raw=0,offset=0,out=0;uint32_t anchorAt=0;
};
}
