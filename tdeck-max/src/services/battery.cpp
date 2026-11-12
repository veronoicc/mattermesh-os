#include "battery.h"
#include "battery_math.h"
#include <Arduino.h>
#include <Wire.h>
#include <TDeckMaxBoard.h>
#include <PowersSY6970.tpp>
#include <Preferences.h>
namespace device {
static BatteryState state;static uint32_t last=0;static bool initialized=false;
static Preferences calibration;static int fullMillivolts=BATTERY_FULL_MV;
// Read existing charger driver only. Never change charging/voltage/current policy.
static PowersSY6970 charger(Wire,BOARD_I2C_SDA,BOARD_I2C_SCL,BOARD_I2C_ADDR_SY6970);
static bool read(uint8_t address,uint8_t reg,uint8_t *bytes,uint8_t n){Wire.beginTransmission(address);Wire.write(reg);if(Wire.endTransmission(false)!=0||Wire.requestFrom(address,n)!=n)return false;for(int i=0;i<n;i++)bytes[i]=Wire.read();return true;}
void batteryLoop(){if(!initialized){calibration.begin("mmbattery",false);int saved=calibration.getInt("full_mv",BATTERY_FULL_MV);if(validFullCalibration(saved))fullMillivolts=saved;}if(initialized&&millis()-last<10000)return;initialized=true;last=millis();uint8_t b[2];state=BatteryState();if(read(BOARD_I2C_ADDR_BQ27220,0x2c,b,2)){int soc=b[0]|(b[1]<<8);if(soc<=100){state.soc=soc;state.gauge=true;}}if(read(BOARD_I2C_ADDR_BQ27220,0x08,b,2)){int v=b[0]|(b[1]<<8);if(v>=2500&&v<=5000)state.voltage=v;}if(read(BOARD_I2C_ADDR_SY6970,0x0b,b,1)){state.charger=true;auto status=charger.chargeStatus();state.full=status==PowersSY6970::CHARGE_STATE_DONE;state.charging=status==PowersSY6970::CHARGE_STATE_PRE_CHARGE||status==PowersSY6970::CHARGE_STATE_FAST_CHARGE;}if(state.voltage<0&&read(BOARD_I2C_ADDR_SY6970,0x0e,b,1)){int v=POWERS_SY6970_VBAT_MASK_VAL(b[0])*POWERS_SY6970_VBAT_VOL_STEP+POWERS_SY6970_VBAT_BASE_VAL;if(v>=2500&&v<=5000)state.voltage=v;}state.percent=batteryPercent(state.soc,state.gauge,state.voltage,state.full,fullMillivolts);state.estimated=state.percent>=0&&!state.full;}
PowerSource batteryPowerSource(){uint8_t value;if(!read(BOARD_I2C_ADDR_SY6970,0x0b,&value,1))return PowerSource::Unknown;return ((value>>5)&7)?PowerSource::USB:PowerSource::Battery;}
bool batteryShutdown(){if(batteryPowerSource()!=PowerSource::Battery)return false;charger.shutdown();return true;}
int batteryFullMillivolts(){return fullMillivolts;}
bool batteryCalibrateFull(){
 uint8_t status;if(!read(BOARD_I2C_ADDR_SY6970,0x0b,&status,1)||((status>>3)&3)!=PowersSY6970::CHARGE_STATE_DONE)return false;
 int sum=0;uint8_t bytes[2];for(int i=0;i<5;i++){if(!read(BOARD_I2C_ADDR_BQ27220,0x08,bytes,2))return false;int mv=bytes[0]|(bytes[1]<<8);if(!validFullCalibration(mv))return false;sum+=mv;delay(10);}
 int measured=(sum+2)/5;if(calibration.putInt("full_mv",measured)!=sizeof(int32_t))return false;fullMillivolts=measured;state.voltage=measured;state.full=true;state.percent=100;state.estimated=false;return true;
}
BatteryState batteryState(){return state;}
static BatteryGauge gauge;static uint32_t gaugeAt=0;
void batteryGaugeLoop(){if(gaugeAt&&millis()-gaugeAt<10000)return;gaugeAt=millis()|1;BatteryGauge g;uint8_t b[2];
 if(read(BOARD_I2C_ADDR_BQ27220,0x0c,b,2)){g.currentMa=int16_t(b[0]|(b[1]<<8));g.valid=true;}
 if(read(BOARD_I2C_ADDR_BQ27220,0x14,b,2))g.avgMa=int16_t(b[0]|(b[1]<<8));
 if(read(BOARD_I2C_ADDR_BQ27220,0x16,b,2))g.tteMin=gaugeMinutes(uint16_t(b[0]|(b[1]<<8)));
 if(read(BOARD_I2C_ADDR_BQ27220,0x18,b,2))g.ttfMin=gaugeMinutes(uint16_t(b[0]|(b[1]<<8)));
 gauge=g;}
BatteryGauge batteryGauge(){return gauge;}
}
