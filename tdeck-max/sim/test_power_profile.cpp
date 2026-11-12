#include <cassert>
#include <iostream>
#include <set>
#include "services/power_profile.h"
#include "services/policy.h"
#include "services/sleep_policy.h"
int main(){
 using namespace device;
 // Three presets, all made of values the underlying settings accept.
 for(auto p:{Profile::Field,Profile::Normal,Profile::Max}){auto &s=profilePreset(p);assert(profileHasPreset(p)&&validTimeout(s.displayS)&&validTimeout(s.keyboardS)&&validSleepDelay(s.sleepDelayS));}
 assert(!profileHasPreset(Profile::Custom));
 // Field saves the most, Max keeps everything on and never sleeps.
 {auto &f=profilePreset(Profile::Field);assert(!f.gps&&!f.modem&&!f.motion&&!f.raise&&!f.wifi&&!f.ble&&f.sleep&&f.displayS==15&&f.keyboardS==15&&f.sleepDelayS==10&&!f.stayAwakeUsb);
  auto &n=profilePreset(Profile::Normal);assert(n.gps&&!n.modem&&n.motion&&n.raise&&!n.wifi&&!n.ble&&n.sleep&&n.displayS==30&&n.sleepDelayS==60&&n.stayAwakeUsb);
  auto &m=profilePreset(Profile::Max);assert(m.gps&&m.modem&&m.motion&&m.raise&&m.wifi&&m.ble&&!m.sleep&&m.displayS==120&&m.stayAwakeUsb);
  assert(!(f==n)&&!(n==m)&&!(f==m)&&f==profilePreset(Profile::Field));}
 // The profile shown follows the settings: still the saved preset, or Custom after any single change.
 for(auto p:{Profile::Field,Profile::Normal,Profile::Max}){
  ProfileSettings now=profilePreset(p);assert(resolveProfile(p,now)==p);
  const int fields=10;for(int f=0;f<fields;f++){ProfileSettings c=now;
   switch(f){case 0:c.gps=!c.gps;break;case 1:c.modem=!c.modem;break;case 2:c.motion=!c.motion;break;case 3:c.raise=!c.raise;break;case 4:c.wifi=!c.wifi;break;case 5:c.ble=!c.ble;break;case 6:c.displayS=c.displayS==15?30:15;break;case 7:c.keyboardS=c.keyboardS==15?30:15;break;case 8:c.sleep=!c.sleep;break;default:c.stayAwakeUsb=!c.stayAwakeUsb;}
   assert(resolveProfile(p,c)!=p);}// any single change leaves the saved profile
  {ProfileSettings c=now;c.sleepDelayS=c.sleepDelayS==10?30:10;assert(resolveProfile(p,c)==(now.sleep?Profile::Custom:p));}}
 // Custom stays Custom while the settings match no preset; matching one by hand names it.
 {ProfileSettings odd=profilePreset(Profile::Normal);odd.displayS=300;assert(resolveProfile(Profile::Custom,odd)==Profile::Custom&&resolveProfile(Profile::Field,odd)==Profile::Custom);
  assert(resolveProfile(Profile::Custom,profilePreset(Profile::Max))==Profile::Max&&resolveProfile(Profile::Normal,profilePreset(Profile::Field))==Profile::Field);}
 // The sleep delay does not matter while sleep is off (Max): changing it keeps the profile.
 {ProfileSettings m=profilePreset(Profile::Max);m.sleepDelayS=10;assert(resolveProfile(Profile::Max,m)==Profile::Max);}
 // Storage form: a byte, bad values become Normal.
 assert(profileFromByte(0)==Profile::Field&&profileFromByte(3)==Profile::Custom&&profileFromByte(4)==Profile::Normal&&profileFromByte(255)==Profile::Normal);
 assert(std::string(profileName(Profile::Field))=="Field"&&std::string(profileName(Profile::Custom))=="Custom"&&std::string(profileTagline(Profile::Max))!="");
 // Rows the page lists for a profile: one per setting, distinct labels, readable values.
 {auto rows=profileRows(profilePreset(Profile::Field));assert(int(rows.size())==PROFILE_ROWS);std::set<std::string> labels;for(auto &r:rows){assert(!r.value.empty());labels.insert(r.label);}assert(int(labels.size())==PROFILE_ROWS);
  assert(rows[0].label=="GPS"&&rows[0].value=="Off");
  auto max=profileRows(profilePreset(Profile::Max));assert(max[0].value=="On"&&max[6].value=="2 min"&&max[8].value=="Off"&&max[9].value=="On");
  auto field=profileRows(profilePreset(Profile::Field));assert(field[6].value=="15 s"&&field[8].value=="On, after 10 s"&&field[9].value=="Off");
  auto normal=profileRows(profilePreset(Profile::Normal));assert(normal[8].value=="On, after 1 min");
  ProfileSettings five=profilePreset(Profile::Normal);five.sleepDelayS=300;five.displayS=0;auto r5=profileRows(five);assert(r5[8].value=="On, after 5 min"&&r5[6].value=="Never");}
 std::cout<<"PASS: power profile: preset table, Custom detection, storage byte, rows\n";
}
