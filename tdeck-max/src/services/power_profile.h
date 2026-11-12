#pragma once
#include <stdint.h>
#include <string>
#include <vector>
// Power profiles: what each one sets, and which one the current settings amount to. Pure; applying them is main's job
// (power_app.inc) through the existing setters. No e-paper refresh row: the refresh policy has no user setting.
namespace device {
enum class Profile:uint8_t {Field,Normal,Max,Custom};
struct ProfileSettings {
 bool gps=false,modem=false,motion=false,raise=false,wifi=false,ble=false,sleep=true,stayAwakeUsb=true;
 uint32_t displayS=30,keyboardS=30,sleepDelayS=60;// lights timeouts (policy.h TIMEOUT_VALUES), Sleep delay (sleep_policy.h)
 ProfileSettings()=default;// the firmware builds as C++11: no aggregate initialisation with default member initialisers
 ProfileSettings(bool g,bool m,bool mo,bool r,bool w,bool b,bool s,bool u,uint32_t d,uint32_t k,uint32_t delay):gps(g),modem(m),motion(mo),raise(r),wifi(w),ble(b),sleep(s),stayAwakeUsb(u),displayS(d),keyboardS(k),sleepDelayS(delay){}
 // The Sleep delay is not part of a profile that does not sleep.
 bool operator==(const ProfileSettings &o)const{return gps==o.gps&&modem==o.modem&&motion==o.motion&&raise==o.raise&&wifi==o.wifi&&ble==o.ble&&sleep==o.sleep&&stayAwakeUsb==o.stayAwakeUsb&&displayS==o.displayS&&keyboardS==o.keyboardS&&(!sleep||sleepDelayS==o.sleepDelayS);}
};
inline bool profileHasPreset(Profile p){return p!=Profile::Custom;}
//                                   gps   modem motion raise wifi  ble   sleep stayUsb disp kbd delay
inline const ProfileSettings &profilePreset(Profile p){
 static const ProfileSettings field(false,false,false,false,false,false,true,false,15,15,10),normal(true,false,true,true,false,false,true,true,30,30,60),max(true,true,true,true,true,true,false,true,120,120,300);
 return p==Profile::Field?field:p==Profile::Max?max:normal;}
// The profile the settings amount to: the saved one while they still match it, else a preset they match, else Custom.
inline Profile resolveProfile(Profile saved,const ProfileSettings &now){
 if(profileHasPreset(saved)&&now==profilePreset(saved))return saved;
 for(auto p:{Profile::Field,Profile::Normal,Profile::Max})if(now==profilePreset(p))return p;
 return Profile::Custom;}
inline Profile profileFromByte(uint8_t b){return b<=3?Profile(b):Profile::Normal;}
inline const char *profileName(Profile p){return p==Profile::Field?"Field":p==Profile::Normal?"Normal":p==Profile::Max?"Max":"Custom";}
inline const char *profileTagline(Profile p){return p==Profile::Field?"Max battery":p==Profile::Normal?"Balanced":p==Profile::Max?"All on":"Your own";}
inline std::string timeoutText(uint32_t s){return s==0?"Never":s>=60?std::to_string(s/60)+" min":std::to_string(s)+" s";}
struct ProfileRow {std::string label,value;};
constexpr int PROFILE_ROWS=10;
inline std::vector<ProfileRow> profileRows(const ProfileSettings &s){
 auto onoff=[](bool b){return std::string(b?"On":"Off");};
 return {{"GPS",onoff(s.gps)},{"Modem",onoff(s.modem)},{"Motion sensor",onoff(s.motion)},{"Raise to wake",onoff(s.raise)},{"Wi-Fi",onoff(s.wifi)},{"Bluetooth",onoff(s.ble)},
  {"Display timeout",timeoutText(s.displayS)},{"Keyboard timeout",timeoutText(s.keyboardS)},{"Sleep when idle",s.sleep?"On, after "+timeoutText(s.sleepDelayS):"Off"},{"Stay awake on USB",onoff(s.stayAwakeUsb)}};}
}
