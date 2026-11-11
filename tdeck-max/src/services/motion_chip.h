#pragma once
// BHI260AP (I2C 0x28, no interrupt line used) on the shared bus, through LilyGO's SensorLib driver and Bosch's BHY2 API.
// SensorLib's own init uploads the 100 KB firmware in one call (about 10 s at 100 kHz); this class does the same steps but uploads
// in time-boxed slices (load) so the main loop keeps running. Only the final boot step runs in one go.
#include <Wire.h>
#include <SensorBHI260AP.hpp>
namespace device {
class MotionChip : SensorBHI260AP {
 public:
  using AccelSink=void(*)(float x,float y,float z,uint32_t ms);// m/s2
  using RotationSink=void(*)(float x,float y,float z,float w,uint32_t ms);// game rotation vector
  enum Sensor {Accel=SENSOR_ID_ACC_PASS,Rotation=SENSOR_ID_GAMERV};
  static inline AccelSink accelSink=nullptr;static inline RotationSink rotationSink=nullptr;
  MotionChip(){setPins(SENSOR_PIN_NONE,SENSOR_PIN_NONE);}// polled: the chip's interrupt output is not read
  // Answers on the bus with the right product id; leaves it in reset, ready for load().
  bool probe(TwoWire &wire,int sda,int scl){
   stop();__handler.u.i2c_dev.scl=scl;__handler.u.i2c_dev.sda=sda;__handler.u.i2c_dev.addr=BHI260AP_SLAVE_ADDRESS_L;__handler.u.i2c_dev.wire=&wire;__handler.intf=SENSORLIB_I2C_INTERFACE;
   wire.beginTransmission(BHI260AP_SLAVE_ADDRESS_L);if(wire.endTransmission()!=0){failure="no answer at 0x28";return false;}
   bhy2=(struct bhy2_dev*)malloc(sizeof(struct bhy2_dev));if(!bhy2){failure="no memory";return false;}
   __max_rw_length=64;__boot_from_flash=false;__write_flash=false;pos=0;
   __error_code=bhy2_init(BHY2_I2C_INTERFACE,SensorInterfaces::bhy2_i2c_read,SensorInterfaces::bhy2_i2c_write,SensorInterfaces::bhy2_delay_us,__max_rw_length,&__handler,bhy2);
   if(__error_code==BHY2_OK)__error_code=bhy2_soft_reset(bhy2);
   uint8_t product=0;if(__error_code==BHY2_OK)__error_code=bhy2_get_product_id(&product,bhy2);
   if(__error_code!=BHY2_OK||product!=BHY2_PRODUCT_ID){failure=__error_code!=BHY2_OK?"reset failed":"wrong product id";stop();return false;}
   return true;}
  // Uploads firmware for at most budgetMs; 0..99 progress, 100 once booted with the sensor list read, -1 failed (see error()).
  int load(uint32_t budgetMs){
   uint32_t started=millis();const uint32_t total=sizeof(bhy2_firmware_image);
   while(pos<total&&uint32_t(millis()-started)<budgetMs){uint32_t n=total-pos<CHUNK?total-pos:CHUNK;
    __error_code=bhy2_upload_firmware_to_ram_partly(bhy2_firmware_image+pos,total,pos,n,bhy2);if(__error_code!=BHY2_OK){failure="firmware upload failed";return -1;}pos+=n;}
   if(pos<total)return int(uint64_t(pos)*100/total);
   uint8_t sensorError=0;bhy2_get_error_value(&sensorError,bhy2);if(sensorError){failure="firmware rejected";return -1;}
   __error_code=bhy2_boot_from_ram(bhy2);if(__error_code!=BHY2_OK){failure="firmware boot failed";return -1;}
   bhy2_get_error_value(&sensorError,bhy2);if(sensorError){failure="firmware error after boot";return -1;}
   if(!getKernelVersion()){failure="no kernel version";return -1;}
   bhy2_register_fifo_parse_callback(BHY2_SYS_ID_META_EVENT,BoschParse::parseMetaEvent,(void*)&__accuracy,bhy2);bhy2_register_fifo_parse_callback(BHY2_SYS_ID_META_EVENT_WU,BoschParse::parseMetaEvent,(void*)&__accuracy,bhy2);
   __pro_buf=(uint8_t*)ps_malloc(__pro_buf_size);if(!__pro_buf){failure="no memory";return -1;}
   __error_code=bhy2_get_and_process_fifo(__pro_buf,__pro_buf_size,bhy2);if(__error_code!=BHY2_OK){failure="fifo read failed";return -1;}
   bhy2_update_virtual_sensor_list(bhy2);bhy2_get_virt_sensor_list(bhy2);
   for(uint8_t i=0;i<BHY2_SENSOR_ID_MAX;i++)if(bhy2_is_sensor_available(i,bhy2))bhy2_register_fifo_parse_callback(i,BoschParse::parseData,NULL,bhy2);
   onResultEvent(SENSOR_ID_ACC_PASS,onAccel);onResultEvent(SENSOR_ID_GAMERV,onRotation);registered=true;
   return 100;}
  bool available(Sensor id){return bhy2&&bhy2_is_sensor_available(id,bhy2);}
  // Hz, 0 stops the sensor.
  bool configure(Sensor id,float hz){return bhy2&&SensorBHI260AP::configure(id,hz,0);}
  void poll(){update();}
  void stop(){if(registered){removeResultEvent(SENSOR_ID_ACC_PASS,onAccel);removeResultEvent(SENSOR_ID_GAMERV,onRotation);registered=false;}deinit();}
  const char *error()const{return failure;}
 private:
  static constexpr uint32_t CHUNK=56;// words of 4: first frame = header + 56 bytes still fits the 64 byte I2C write
  static_assert(sizeof(bhy2_firmware_image)%4==0,"firmware is whole words");
  static void onAccel(uint8_t id,uint8_t *data,uint32_t,uint64_t *stamp){bhy2_data_xyz v;bhy2_parse_xyz(data,&v);float k=get_sensor_default_scaling(id)*9.80665f;if(accelSink)accelSink(v.x*k,v.y*k,v.z*k,uint32_t(*stamp/64));}// 64 kHz ticks
  static void onRotation(uint8_t id,uint8_t *data,uint32_t,uint64_t *stamp){bhy2_data_quaternion q;bhy2_parse_quaternion(data,&q);float k=get_sensor_default_scaling(id);if(rotationSink)rotationSink(q.x*k,q.y*k,q.z*k,q.w*k,uint32_t(*stamp/64));}
  uint32_t pos=0;bool registered=false;const char *failure="";
};
}
