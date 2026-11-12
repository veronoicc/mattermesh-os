#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
#include "mm_crypto.h"
// Lock-screen PIN: 4 to 8 digits kept as a salted PBKDF2-SHA256 hash, the wrong-PIN backoff and the "require after" timing.
// Pure logic, no hardware: main.cpp (security_app.inc) stores the record in NVS "mmsec" and drives the pad.
// ponytail: 5000 iterations is about 0.25 s on the ESP32-S3; raise PinIterations (stored per record) if verifying may take longer.
namespace device {
constexpr size_t PinMin=4,PinMax=8;constexpr uint32_t PinIterations=5000;
inline bool pinValid(const std::string &pin){if(pin.size()<PinMin||pin.size()>PinMax)return false;for(char c:pin)if(c<'0'||c>'9')return false;return true;}
struct PinRecord {uint8_t salt[16]={};uint8_t hash[32]={};uint32_t iterations=0;};
inline PinRecord pinMake(const std::string &pin,const uint8_t salt[16],uint32_t iterations=PinIterations){PinRecord r;memcpy(r.salt,salt,16);r.iterations=iterations;mmcrypto::pbkdf2Sha256(reinterpret_cast<const uint8_t*>(pin.data()),pin.size(),r.salt,16,iterations,r.hash,32);return r;}
// A record with no iterations (damaged NVS) or an invalid PIN never verifies.
inline bool pinVerify(const PinRecord &r,const std::string &pin){if(!r.iterations||!pinValid(pin))return false;uint8_t h[32];mmcrypto::pbkdf2Sha256(reinterpret_cast<const uint8_t*>(pin.data()),pin.size(),r.salt,16,r.iterations,h,32);return mmcrypto::equal(h,r.hash,32);}
// A typed character as a PIN digit: digits themselves, or the letters of the keyboard's SYM layer (w e r / s d f / z x c / $).
inline char pinKeyDigit(char c){if(c>='0'&&c<='9')return c;switch(c){case 'w':return '1';case 'e':return '2';case 'r':return '3';case 's':return '4';case 'd':return '5';case 'f':return '6';case 'z':return '7';case 'x':return '8';case 'c':return '9';case '$':return '0';}return 0;}
struct PinEntry {
 std::string digits;
 bool add(char c){if(c<'0'||c>'9'||digits.size()>=PinMax)return false;digits+=c;return true;}
 bool erase(){if(digits.empty())return false;digits.pop_back();return true;}
 void clear(){digits.clear();}
};
// Wrong PINs: the first four are free, from the fifth the wait is 30 s and doubles with every further wrong PIN, one hour at most.
constexpr uint32_t PinFreeTries=4,PinLockBaseMs=30000,PinLockMaxMs=3600000;
inline uint32_t pinLockoutMs(uint32_t fails){if(fails<=PinFreeTries)return 0;uint32_t shift=fails-PinFreeTries-1;if(shift>7)return PinLockMaxMs;uint32_t ms=PinLockBaseMs<<shift;return ms>PinLockMaxMs?PinLockMaxMs:ms;}
inline uint32_t pinTriesLeft(uint32_t fails){return fails>=PinFreeTries+1?0:PinFreeTries+1-fails;}
// fails is persisted by the caller before the PIN is checked. The wait is on the millis clock (wrap safe); after a restart it begins again from boot.
struct PinGuard {
 uint32_t fails=0,until=0;bool waiting=false;
 void restore(uint32_t persisted,uint32_t now){fails=persisted;uint32_t ms=pinLockoutMs(fails);waiting=ms>0;until=now+ms;}
 uint32_t waitMs(uint32_t now)const{return waiting&&int32_t(until-now)>0?until-now:0;}
 bool allowed(uint32_t now)const{return waitMs(now)==0;}
 uint32_t fail(uint32_t now){if(fails<0xffffffffu)fails++;uint32_t ms=pinLockoutMs(fails);waiting=ms>0;until=now+ms;return ms;}
 void success(){fails=0;waiting=false;until=0;}
};
// Require PIN after the screen has been locked for: Immediately, 1, 5 or 15 minutes. After a restart the PIN is always asked.
constexpr int PinRequireCount=4;
inline uint32_t pinRequireMs(int mode){static const uint32_t ms[PinRequireCount]={0,60000,300000,900000};return ms[mode<0?0:mode>=PinRequireCount?PinRequireCount-1:mode];}
inline const char *pinRequireName(int mode){static const char *n[PinRequireCount]={"Immediately","1 min","5 min","15 min"};return n[mode<0?0:mode>=PinRequireCount?PinRequireCount-1:mode];}
inline bool pinDue(int mode,uint32_t lockedMs,bool afterBoot){return afterBoot||lockedMs>=pinRequireMs(mode);}
// Set / change / remove: remove and change ask for the current PIN first; a new PIN is typed twice. submit() gets the digits of the
// current step (oldCorrect: the caller's check of the current PIN) and says what happened; Save has the new PIN in pin.
struct PinSetup {
 enum Mode {None,Set,Change,Remove};enum Stage {Idle,Old,New,Confirm};enum Result {Pending,Wrong,Short,Mismatch,Save,Removed};
 Mode mode=None;Stage stage=Idle;std::string first,pin;
 void begin(Mode m){mode=m;stage=m==Set?New:Old;first.clear();pin.clear();}
 void cancel(){mode=None;stage=Idle;first.clear();pin.clear();}
 Result submit(const std::string &digits,bool oldCorrect=false){
  if(stage==Old){if(!oldCorrect)return Wrong;if(mode==Remove){cancel();return Removed;}stage=New;return Pending;}
  if(stage==New){if(!pinValid(digits))return Short;first=digits;stage=Confirm;return Pending;}
  if(stage==Confirm){if(digits==first){pin=digits;stage=Idle;first.clear();return Save;}stage=New;first.clear();return Mismatch;}
  return Pending;
 }
};
}
