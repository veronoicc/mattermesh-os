#include <string>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include "services/motion_policy.h"
using namespace device;
// Accelerometer sample feeder at 25 Hz (40 ms): pose = tilt of the screen normal from vertical about the device x axis.
struct Rig {RaiseDetector d;uint32_t t=1000;int fired=0;uint32_t firedAt=0;
 void sample(float tiltDeg,float swayDeg,float bump){float a=tiltDeg*float(M_PI)/180,s=swayDeg*float(M_PI)/180;float m=GRAVITY+bump;
  float ux=sinf(s)*cosf(a),uy=sinf(a)*cosf(s),uz=cosf(a)*cosf(s);if(d.feed(t,ux*m,uy*m,uz*m)){fired++;firedAt=t;}t+=40;}
 void still(float tilt,int ms){for(int i=0;i<ms/40;i++)sample(tilt,0,0.04f*sinf(float(i)));}
 // Smooth move between two tilts with a magnitude bump in the middle (the lift).
 void move(float from,float to,int ms,float peak){int n=ms/40;for(int i=1;i<=n;i++){float f=float(i)/n;float e=f*f*(3-2*f);sample(from+(to-from)*e,0,peak*sinf(float(M_PI)*f));}}
 // Walking: constant carry pose, step shocks at ~2 Hz on the gravity axis and a leg swing of the pose.
 void walk(float tilt,int ms,float shock,float swing){for(int i=0;i<ms/40;i++){float ph=2*float(M_PI)*2.0f*(i*0.04f);sample(tilt+swing*sinf(ph*0.5f),swing*sinf(ph),shock*sinf(ph));}}
};
int main(){
 // Angles: wrap, shortest difference, smoothing across north.
 assert(wrapDegrees(370)==10&&wrapDegrees(-10)==350&&wrapDegrees(720)==0&&wrapDegrees(0)==0&&wrapDegrees(359.9f)<360);
 assert(fabsf(angleDelta(350,10)-20)<1e-3&&fabsf(angleDelta(10,350)+20)<1e-3&&fabsf(angleDelta(0,180)-180)<1e-3);
 {float h=350;for(int i=0;i<60;i++)h=smoothAngle(h,10,0.3f);assert(fabsf(angleDelta(h,10))<0.1f);float p=350;p=smoothAngle(p,10,0.5f);assert(fabsf(angleDelta(p,0))<1e-3);}// goes through north, not 180 degrees round
 assert(std::string(compassName(0))=="N"&&std::string(compassName(44))=="NE"&&std::string(compassName(359))=="N"&&std::string(compassName(182))=="S"&&std::string(compassName(-90))=="W");
 // Quaternion heading: identity faces north flat; 90 deg clockwise (about -z) is east, counter-clockwise west; upright reports where the back faces.
 assert(fabsf(quatHeadingDegrees(0,0,0,1))<1e-3);
 assert(fabsf(quatHeadingDegrees(0,0,-0.70710678f,0.70710678f)-90)<1e-2);
 assert(fabsf(angleDelta(quatHeadingDegrees(0,0,0.70710678f,0.70710678f),270))<1e-2);
 assert(fabsf(angleDelta(quatHeadingDegrees(0.70710678f,0,0,0.70710678f),0))<1e-2);
 assert(fabsf(angleDelta(quatHeadingDegrees(0.5f,-0.5f,-0.5f,0.5f),90))<1e-2);// upright, then turned 90 deg clockwise
 // Sensitivity levels order: higher is easier (smaller tilt, lower lift, shorter hold); bad values read as medium.
 assert(motionSensitivity(0).tiltDeg>motionSensitivity(1).tiltDeg&&motionSensitivity(1).tiltDeg>motionSensitivity(2).tiltDeg);
 assert(motionSensitivity(0).excursion>motionSensitivity(2).excursion&&motionSensitivity(0).holdMs>motionSensitivity(2).holdMs);
 assert(motionSensitivity(-1).tiltDeg==motionSensitivity(1).tiltDeg&&motionSensitivity(9).tiltDeg==motionSensitivity(1).tiltDeg);
 // Table: flat and still, a knock that does not move it: never fires.
 {Rig r;r.still(0,3000);for(int k=0;k<5;k++){r.sample(0,0,3.5f);r.still(0,1500);}assert(r.fired==0);}
 // Raise from the table to a reading angle: exactly once, after the hold time.
 {Rig r;r.d.setSensitivity(1);r.still(0,3000);uint32_t startMove=r.t;r.move(0,50,800,3.5f);uint32_t settled=r.t;r.still(50,1500);assert(r.fired==1);assert(r.firedAt>startMove+400&&r.firedAt<=settled+motionSensitivity(1).holdMs+200);}
 // Raise out of a pocket (upright) to the reading angle.
 {Rig r;r.still(90,3000);r.move(90,45,900,3.0f);r.still(45,1500);assert(r.fired==1);}
 // Walking with the device carried upright, then stopping in the same pose: never fires.
 {Rig r;r.still(90,2000);r.walk(90,30000,4.5f,14);r.still(90,2000);assert(r.fired==0);}
 // Walking with the device held at a reading angle (hand swing): never fires.
 {Rig r;r.still(45,2000);r.walk(45,20000,3.5f,10);r.still(45,2000);assert(r.fired==0);}
 // Putting it down on the table and into a pocket are not raises.
 {Rig r;r.still(45,2000);r.move(45,0,800,3.0f);r.still(0,2000);assert(r.fired==0);r.still(45,0);r.move(0,90,800,3.0f);r.still(90,2000);assert(r.fired==0);}
 // A short pass through the reading pose that keeps moving is not held.
 {Rig r;r.still(0,2000);r.move(0,50,600,3.5f);r.still(50,200);r.move(50,60,400,3.5f);assert(r.fired==0);r.still(60,1500);assert(r.fired==1);}
 // Cooldown: a second raise inside it is ignored, one after it fires.
 {Rig r;r.still(0,2000);r.move(0,50,600,3.5f);r.still(50,800);assert(r.fired==1);r.move(50,0,600,3.5f);r.still(0,600);r.move(0,50,600,3.5f);r.still(50,600);assert(r.fired==1);
  r.still(50,4000);r.move(50,0,600,3.5f);r.still(0,1000);r.move(0,50,600,3.5f);r.still(50,1500);assert(r.fired==2);}
 // Sensitivity: a gentle small raise fires on High and not on Low; a firm raise fires on every level.
 for(int level=0;level<3;level++){Rig gentle;gentle.d.setSensitivity(level);gentle.still(0,2000);gentle.move(0,32,900,1.6f);gentle.still(32,1500);assert(gentle.fired==(level==2?1:0));
  Rig firm;firm.d.setSensitivity(level);firm.still(0,2000);firm.move(0,60,700,4.0f);firm.still(60,1500);assert(firm.fired==1);}
 // reset() forgets the rest pose: no fire from stale data after a pause in sampling.
 {Rig r;r.still(0,2000);r.d.reset();r.t+=60000;r.still(50,2000);assert(r.fired==0);}
 // Heading tracker: unknown until GPS course anchors the gyro frame; follows turns; rate-limited re-anchor; expires.
 {HeadingTracker h;h.sample(100,1000);assert(!h.valid(1000));
  h.course(90,2.0f,true,1100);assert(!h.valid(1100));// too slow: course is noise
  h.course(90,6.0f,false,1200);assert(!h.valid(1200));// no valid course
  h.course(90,6.0f,true,1300);assert(h.valid(1300)&&fabsf(angleDelta(h.current(),90))<1e-2);
  for(uint32_t t=1340;t<3000;t+=40)h.sample(160,t);assert(fabsf(angleDelta(h.current(),150))<2);// device turned 60 deg: heading follows after smoothing
  float before=h.current();h.course(300,6.0f,true,1350);assert(h.current()==before);// inside the 1 s rate limit
  h.course(150,6.0f,true,3500);for(uint32_t t=3500;t<5000;t+=40)h.sample(160,t);assert(fabsf(angleDelta(h.current(),150))<2);
  assert(h.valid(3500+HEADING_VALID_MS)&&!h.valid(3500+HEADING_VALID_MS+1));}
 // Anchor across north: course 5, raw 350 -> offset 15; a raw 10 reads 25, never 385 or -335.
 {HeadingTracker h;h.sample(350,0);h.course(5,5.0f,true,10);for(uint32_t t=40;t<2000;t+=40)h.sample(10,t);assert(fabsf(angleDelta(h.current(),25))<1&&h.current()>=0&&h.current()<360);}
 // Relative bearing for the radar: node at north-east, device facing east, is ahead-left; wraps into 0..359.
 assert(relativeBearing(45,90)==315&&relativeBearing(10,350)==20&&relativeBearing(0,0)==0);
 std::puts("PASS: raise gesture (table, raise, pocket, walking, put down, hold, cooldown, sensitivity), heading math (wrap, smoothing, quaternion, GPS anchor, rate limit, expiry), relative bearing");
}
