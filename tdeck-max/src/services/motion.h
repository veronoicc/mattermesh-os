#pragma once
#include <Arduino.h>
#include <ExtensionIOXL9555.hpp>
namespace device {
// BHI260AP motion sensor: raise to wake, compass heading (gyro frame anchored to GPS course) and accelerometer telemetry.
enum class MotionPhase : uint8_t {Off,Powering,Loading,Ready,Absent,Failed};
struct MotionState {bool enabled=true,raise=false,headingUp=false,telemetry=false;int sensitivity=1;MotionPhase phase=MotionPhase::Off;int loadPercent=0;bool headingValid=false,anchored=false,accelFresh=false;float heading=0,ax=0,ay=0,az=0;uint32_t anchorAgeMs=UINT32_MAX,raises=0,revision=0;const char *error="";};
void motionBegin(ExtensionIOXL9555 &io,bool ready);void motionLoop();MotionState motionState();
bool motionEnable(bool on);bool motionRaiseToWake(bool on);bool motionSetSensitivity(int level);bool motionHeadingUp(bool on);bool motionShareTelemetry(bool on);// saved to NVS (mmmotion)
bool motionRaiseTake();// true once per detected raise gesture
bool motionHeading(float &degrees);// fresh, anchored heading: where the device faces
bool motionTelemetry(float accelG[3],float &heading);// accelerometer in g for CayenneLPP when sharing is on; heading NAN when unknown
}
