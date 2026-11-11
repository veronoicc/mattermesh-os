// Emulated SIMCom A7682E AT modem on Serial1. It powers up like the board: the XL9555 6609_EN pin (P00) supplies it, a
// PWRKEY_EN (P10) press of about a second starts it, and it answers AT after 3 s ("RDY" first). SIM ready after 4 s, LTE
// registration after 6 s on "Emu Mobile" with CSQ 21, a data bearer (+NETOPEN) that always opens. No TCP/IP sockets
// leave the PC: AT+CIPOPEN reports "+CIPOPEN: n,11" (DNS failure), so Wi-Fi remains the only network of the emulator.
// Text messages: UCS2 text mode like the device (hex UTF-16BE numbers and bodies); AT+CMGS sends are logged as EMU_SMS_SENT.
#include "modem_a76xx.h"
#ifdef MM_BOARD_MAX
#include "max_devices.h"
#endif
#include "services/modem_at.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <emu_hal.h>
#include <cstdio>
#include <mutex>
#include <vector>
namespace modememu {
namespace {
constexpr uint32_t KeyHoldMs=900,BootMs=3000,SimMs=4000,NetMs=6000;
struct Sms {int index;bool unread;std::string number,text,scts;};
bool present=true;std::mutex mutex;bool on=false,echo=true,keyHeld=false,prompt=false;uint32_t keyAt=0,bootAt=0;std::string line,pendingNumber;std::vector<Sms> store;int nextIndex=1;bool netOpen=false;
void out(const std::string &s){Serial1.feed(reinterpret_cast<const uint8_t*>(s.data()),s.size());}
void ok(const std::string &body=""){out(body.empty()?"\r\nOK\r\n":"\r\n"+body+"\r\n\r\nOK\r\n");}
bool powered(){
#ifdef MM_BOARD_MAX
 return maxdev::ioLevel(0);
#else
 return true;
#endif
}
bool key(){
#ifdef MM_BOARD_MAX
 return maxdev::ioLevel(8);
#else
 return false;
#endif
}
uint32_t up(){return millis()-bootAt;}
std::string scts(){time_t t=time(nullptr);tm u;gmtime_r(&t,&u);char b[32];snprintf(b,sizeof b,"%02d/%02d/%02d,%02d:%02d:%02d+00",u.tm_year%100,u.tm_mon+1,u.tm_mday,u.tm_hour,u.tm_min,u.tm_sec);return b;}
void add(const std::string &number,const std::string &text){Sms m{nextIndex++,true,number,text,scts()};store.push_back(m);out("\r\n+CMTI: \"SM\","+std::to_string(m.index)+"\r\n");}
void command(const std::string &c){
 if(c.empty())return;
 if(echo&&c!="ATE0")out(c+"\r");
 if(c=="AT"||c=="ATE0"){if(c=="ATE0")echo=false;ok();}
 else if(c=="ATI")ok("Manufacturer: SIMCOM INCORPORATED\r\nModel: A7682E\r\nRevision: A7682E-EMU_V1.0\r\nIMEI: 860000000000000");
 else if(c=="AT+CPIN?"){if(up()<SimMs)out("\r\n+CME ERROR: 14\r\n");else ok("+CPIN: READY");}
 else if(c=="AT+CEREG?")ok(up()<NetMs?"+CEREG: 0,2":"+CEREG: 0,1,\"0001\",\"00000001\",7");
 else if(c=="AT+CREG?")ok("+CREG: 0,0");
 else if(c=="AT+CSQ")ok(up()<NetMs?"+CSQ: 99,99":"+CSQ: 21,99");
 else if(c=="AT+COPS?")ok(up()<NetMs?"+COPS: 0":"+COPS: 0,0,\"Emu Mobile\",7");
 else if(c=="AT+NETOPEN"){if(netOpen)out("\r\n+IP ERROR: Network is already opened\r\n\r\nERROR\r\n");else{netOpen=true;out("\r\nOK\r\n\r\n+NETOPEN: 0\r\n");}}
 else if(c=="AT+NETCLOSE"){netOpen=false;out("\r\nOK\r\n\r\n+NETCLOSE: 0\r\n");}
 else if(c=="AT+IPADDR")ok("+IPADDR: 10.64.0.2");
 else if(c.rfind("AT+CIPOPEN=",0)==0){int link=atoi(c.c_str()+11);out("\r\nOK\r\n\r\n+CIPOPEN: "+std::to_string(link)+",11\r\n");}
 else if(c.rfind("AT+CIPCLOSE=",0)==0){int link=atoi(c.c_str()+12);out("\r\nOK\r\n\r\n+CIPCLOSE: "+std::to_string(link)+",0\r\n");}
 else if(c=="AT+CMGL=\"ALL\""){std::string body;for(auto &m:store)body+="+CMGL: "+std::to_string(m.index)+(m.unread?",\"REC UNREAD\",\"":",\"REC READ\",\"")+modem_at::ucs2Hex(m.number)+"\",\"\",\""+m.scts+"\",145,"+std::to_string(m.text.size())+"\r\n"+modem_at::ucs2Hex(m.text)+"\r\n";for(auto &m:store)m.unread=false;out("\r\n"+body+"\r\nOK\r\n");}
 else if(c.rfind("AT+CMGD=",0)==0){int i=atoi(c.c_str()+8);for(size_t k=0;k<store.size();k++)if(store[k].index==i){store.erase(store.begin()+long(k));break;}ok();}
 else if(c.rfind("AT+CMGS=\"",0)==0){std::string hex=c.substr(9,c.size()-10),n;if(!modem_at::utf8FromUcs2Hex(hex,n))n=hex;pendingNumber=n;prompt=true;out("\r\n> ");}
 else if(c=="AT+CPOF"){on=false;ok();}
 else ok();}
void onWrite(const uint8_t *d,size_t n){
 std::lock_guard<std::mutex> g(mutex);
 if(!present||!powered()||!on||up()<BootMs)return;
 for(size_t i=0;i<n;i++){
  if(prompt){line+=char(d[i]);if(d[i]==0x1A){std::string hex=line.substr(0,line.size()-1),text;if(!modem_at::utf8FromUcs2Hex(hex,text))text=hex;printf("EMU_SMS_SENT number=%s text=%s\n",pendingNumber.c_str(),text.c_str());fflush(stdout);prompt=false;line.clear();out("\r\n+CMGS: 1\r\n\r\nOK\r\n");}continue;}
  if(d[i]=='\r'||d[i]=='\n'){std::string c=line;line.clear();command(c);}else line+=char(d[i]);}
}
void tick(){
 std::lock_guard<std::mutex> g(mutex);if(!present)return;
 bool p=powered(),k=key();uint32_t now=millis();
 if(!p){on=false;netOpen=false;keyHeld=false;return;}
 if(k&&!keyHeld){keyHeld=true;keyAt=now;}
 if(!k&&keyHeld){keyHeld=false;if(!on&&now-keyAt>=KeyHoldMs){on=true;bootAt=now;echo=true;line.clear();prompt=false;out("\r\nRDY\r\n");}}
}
struct Planned {unsigned at;std::string text;bool done;};std::vector<Planned> planned;
void plannedTick(){std::lock_guard<std::mutex> g(mutex);if(!present||!on||up()<NetMs)return;uint32_t now=emu::millis32();for(auto &p:planned)if(!p.done&&now>=p.at){p.done=true;add("+39000111222",p.text);}}
}
void configure(bool on_){present=on_;}
void attach(){Serial1.emuOnWrite(onWrite);emu::addPumpHook(tick);emu::addPumpHook(plannedTick);}
void injectSms(const std::string &number,const std::string &text){std::lock_guard<std::mutex> g(mutex);if(present&&on)add(number,text);}
void scheduleSms(unsigned afterMs,const std::string &text){std::lock_guard<std::mutex> g(mutex);planned.push_back({afterMs,text,false});}
}
