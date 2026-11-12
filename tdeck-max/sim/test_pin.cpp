#include <cassert>
#include <iostream>
#include "services/pin_policy.h"
using namespace device;
static const uint8_t saltA[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},saltB[16]={9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9};
int main(){
 // 4 to 8 digits, nothing else.
 assert(pinValid("1234")&&pinValid("12345678")&&!pinValid("123")&&!pinValid("123456789")&&!pinValid("12a4")&&!pinValid("")&&!pinValid("12 4"));
 // Hash: the right PIN verifies, a wrong one and a changed salt / iteration count do not; the PIN itself is not in the record.
 PinRecord r=pinMake("2468",saltA,50);
 assert(pinVerify(r,"2468")&&!pinVerify(r,"2469")&&!pinVerify(r,"246")&&!pinVerify(r,"24680")&&!pinVerify(r,""));
 {PinRecord o=pinMake("2468",saltB,50);assert(memcmp(o.hash,r.hash,32)!=0);PinRecord s=r;memcpy(s.salt,saltB,16);assert(!pinVerify(s,"2468"));PinRecord it=r;it.iterations=51;assert(!pinVerify(it,"2468"));PinRecord z=r;z.iterations=0;assert(!pinVerify(z,"2468"));}
 assert(pinMake("2468",saltA,50).iterations==50&&memcmp(pinMake("2468",saltA,50).hash,r.hash,32)==0);
 {std::string raw(reinterpret_cast<const char*>(r.hash),32);assert(raw.find("2468")==std::string::npos);}
 assert(PinIterations>=5000);// the shipped cost
 // Keys: digits as typed, and the letters under them on the T-Deck keyboard (the SYM layer).
 for(char d='0';d<='9';d++)assert(pinKeyDigit(d)==d);
 assert(pinKeyDigit('w')=='1'&&pinKeyDigit('e')=='2'&&pinKeyDigit('r')=='3'&&pinKeyDigit('s')=='4'&&pinKeyDigit('d')=='5'&&pinKeyDigit('f')=='6'&&pinKeyDigit('z')=='7'&&pinKeyDigit('x')=='8'&&pinKeyDigit('c')=='9'&&pinKeyDigit('$')=='0'&&pinKeyDigit('q')==0&&pinKeyDigit('\n')==0);
 // Entry: digits only, at most 8, erase.
 {PinEntry e;assert(!e.add('x')&&e.digits.empty());for(char c:std::string("12345678"))assert(e.add(c));assert(!e.add('9')&&e.digits=="12345678");assert(e.erase()&&e.digits=="1234567");e.clear();assert(e.digits.empty()&&!e.erase());}
 // Backoff: four free tries, then 30 s doubling per wrong PIN, capped at an hour.
 assert(pinLockoutMs(0)==0&&pinLockoutMs(4)==0&&pinLockoutMs(5)==30000&&pinLockoutMs(6)==60000&&pinLockoutMs(7)==120000&&pinLockoutMs(8)==240000&&pinLockoutMs(12)==PinLockMaxMs&&pinLockoutMs(40)==PinLockMaxMs&&pinLockoutMs(0xffffffffu)==PinLockMaxMs);
 assert(pinTriesLeft(0)==5&&pinTriesLeft(4)==1&&pinTriesLeft(5)==0&&pinTriesLeft(9)==0);
 {PinGuard g;uint32_t t=1000;for(int i=1;i<=4;i++){assert(g.allowed(t));assert(g.fail(t)==0);assert(g.fails==uint32_t(i));}
  assert(g.allowed(t)&&g.waitMs(t)==0);assert(g.fail(t)==30000);assert(!g.allowed(t)&&g.waitMs(t)==30000&&g.waitMs(t+10000)==20000&&!g.allowed(t+29999)&&g.allowed(t+30000)&&g.waitMs(t+30000)==0);
  t+=30000;assert(g.fail(t)==60000&&g.waitMs(t)==60000);t+=60000;assert(g.allowed(t));assert(g.fail(t)==120000);
  g.success();assert(g.fails==0&&g.allowed(t)&&g.waitMs(t)==0);for(int i=0;i<4;i++)g.fail(t);assert(g.allowed(t));}
 // The clock wraps (millis after 49 days): a wait that crosses the wrap still counts down.
 {PinGuard g;for(int i=0;i<4;i++)g.fail(0);uint32_t t=0xffffff00u;g.fail(t);assert(g.waitMs(t)==30000&&g.waitMs(t+20000)==10000&&g.allowed(t+30000));}
 // Reboot: the failure count comes back from NVS and the penalty starts again from boot, so a power cycle never skips it.
 {PinGuard g;g.restore(0,500);assert(g.allowed(500));g.restore(4,500);assert(g.allowed(500)&&g.fails==4);g.restore(5,500);assert(!g.allowed(500)&&g.waitMs(500)==30000);g.restore(7,500);assert(g.waitMs(500)==120000&&g.allowed(500+120000));}
 // Require PIN after: immediately, 1, 5, 15 minutes; the boot always asks.
 assert(PinRequireCount==4&&pinRequireMs(0)==0&&pinRequireMs(1)==60000&&pinRequireMs(2)==300000&&pinRequireMs(3)==900000&&pinRequireMs(-1)==0&&pinRequireMs(9)==900000);
 assert(std::string(pinRequireName(0))=="Immediately"&&std::string(pinRequireName(1))=="1 min"&&std::string(pinRequireName(3))=="15 min");
 assert(pinDue(0,0,false)&&pinDue(0,5,false));
 assert(!pinDue(1,59999,false)&&pinDue(1,60000,false)&&!pinDue(2,299999,false)&&pinDue(2,300000,false)&&!pinDue(3,899999,false)&&pinDue(3,900000,false));
 assert(pinDue(3,0,true)&&pinDue(1,1,true));
 // Set / change / remove wizard.
 {PinSetup s;assert(s.stage==PinSetup::Idle);s.begin(PinSetup::Set);assert(s.stage==PinSetup::New);
  assert(s.submit("12")==PinSetup::Short&&s.stage==PinSetup::New);assert(s.submit("1234")==PinSetup::Pending&&s.stage==PinSetup::Confirm);
  assert(s.submit("1235")==PinSetup::Mismatch&&s.stage==PinSetup::New);assert(s.submit("4321")==PinSetup::Pending);assert(s.submit("4321")==PinSetup::Save&&s.pin=="4321"&&s.stage==PinSetup::Idle);}
 {PinSetup s;s.begin(PinSetup::Change);assert(s.stage==PinSetup::Old);assert(s.submit("1111",false)==PinSetup::Wrong&&s.stage==PinSetup::Old);assert(s.submit("1111",true)==PinSetup::Pending&&s.stage==PinSetup::New);
  assert(s.submit("55555")==PinSetup::Pending);assert(s.submit("55555")==PinSetup::Save&&s.pin=="55555");}
 {PinSetup s;s.begin(PinSetup::Remove);assert(s.stage==PinSetup::Old);assert(s.submit("1",false)==PinSetup::Wrong);assert(s.submit("2222",true)==PinSetup::Removed&&s.stage==PinSetup::Idle);}
 {PinSetup s;s.begin(PinSetup::Change);s.cancel();assert(s.stage==PinSetup::Idle&&s.mode==PinSetup::None);}
 std::cout<<"PASS: PIN rules, salted hash verify, SYM-layer digits, backoff with persistence and clock wrap, require-after timing, set/change/remove wizard\n";
}
