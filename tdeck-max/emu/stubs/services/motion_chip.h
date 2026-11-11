#pragma once
// Emulator stand-in for services/motion_chip.h: the BHI260AP is not modelled on the emulated I2C bus (see emu/README.md), so the
// chip never answers and the Motion page reports "Not detected" exactly as a board without the sensor would.
#include <Wire.h>
namespace device {
class MotionChip {
 public:
  using AccelSink=void(*)(float,float,float,uint32_t);using RotationSink=void(*)(float,float,float,float,uint32_t);
  enum Sensor {Accel=1,Rotation=37};
  static inline AccelSink accelSink=nullptr;static inline RotationSink rotationSink=nullptr;
  bool probe(TwoWire &,int,int){return false;}
  int load(uint32_t){return -1;}
  bool available(Sensor){return false;}
  bool configure(Sensor,float){return false;}
  void poll(){}
  void stop(){}
  const char *error()const{return "not emulated";}
};
}
