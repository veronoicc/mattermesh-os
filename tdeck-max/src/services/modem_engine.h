#pragma once
#include <stdint.h>
#include <string>
#include <memory>
#include <vector>
#include <deque>
#include <functional>
#include "services/modem_at.h"
#include "services/modem_status.h"
#include "services/cellular_settings.h"
// Non-blocking AT engine for the SIMCom A7682E / A7670G: tick() reads what arrived, completes the running command and starts
// the next one, so a caller (the modem task, a host test) never waits inside it. Sequence: rail on, AT probe, PWRKEY pulse
// when silent, sync, init, SIM / PIN, registration, data bearer (AT+NETOPEN), SMS. Pure: the UART and the board pins are the Io.
namespace modem {
constexpr int Links=10;// AT+CIPOPEN link numbers 0..9
struct Io {
 virtual ~Io(){}
 virtual int read()=0;// next UART byte, -1 when none
 virtual void write(const std::string &bytes)=0;
 virtual uint32_t ms()=0;
 virtual void rail(bool on)=0;// 6609_EN: modem supply
 virtual void pwrkey(bool pressed)=0;// PWRKEY_EN: true presses the key
 virtual bool pinsSettled(){return true;}// the requested rail / PWRKEY levels are on the pins
};
// One AT command. after: an unsolicited line that ends the command after its OK (+NETOPEN:, +CIPOPEN:, +CIPSEND:, +CIPCLOSE:).
// prompt: send payload at the '>' prompt. rawPrefix: a reply header whose 3rd field is a byte count followed by that many raw bytes.
struct Cmd {std::string text,payload;uint32_t timeoutMs=3000,afterMs=0;const char *after=nullptr,*rawPrefix=nullptr;bool prompt=false;std::function<void(bool,const std::string&)> done;};
class Engine {
public:
 explicit Engine(Io &io):io(io){}
 std::function<void(const std::vector<modem_at::ParsedSms>&)> onSmsList;// whole inbox, after every AT+CMGL
 std::function<void()> onSmsAlert;// +CMTI or a failed send: the cache is stale
 void configure(const device::CellularSettings &c){bool pin=c.pin!=cfg.pin;bool bearer=c.apn!=cfg.apn||c.apnUser!=cfg.apnUser||c.apnPass!=cfg.apnPass||c.roaming!=cfg.roaming;cfg=c;if(pin)pinTried=false;if(bearer)dataRestart=true;}
 void setWanted(bool on){want=on;}
 void setDataWanted(bool on){dataWanted=on;}
 bool textUcs2()const{return ucs2;}// the module returns numbers and texts as UCS2 hex
 void refreshSms(){smsDue=true;}
 void submit(const Cmd &c){queue.push_back(c);}
 const device::ModemStatus &status()const{return st;}
 bool linkOpen(int l)const{return l>=0&&l<Links&&open[l];}
 bool linkData(int l)const{return l>=0&&l<Links&&ready[l];}
 void linkOpened(int l){if(l>=0&&l<Links){open[l]=true;ready[l]=false;}}
 void linkClosed(int l){if(l>=0&&l<Links){open[l]=false;}}
 void linkDrained(int l){if(l>=0&&l<Links)ready[l]=false;}
 bool bearerUp()const{return st.data==device::DataPhase::Online;}
 size_t pending()const{return queue.size()+(cur?1:0);}
 void tick(){uint32_t now=io.ms();pump(now);if(cur&&int32_t(now-deadline)>=0){finish(false,"timeout");}if(!cur)advance(now);}
private:
 enum class P:uint8_t {Off,RailWait,Probe,Pulse,Sync,Init,Sim,Net,Stopping};
 Io &io;device::CellularSettings cfg;device::ModemStatus st;P phase=P::Off;
 bool want=false,dataWanted=false,dataRestart=false,pinTried=false,smsDue=false,pulseHeld=false,roamingOk=true,netStep=false,dataBusy=false;
 uint32_t until=0,syncStart=0,nextPoll=0,nextOps=0,nextData=0,nextSms=0,smsPoll=0,rawLeft=0;int probes=0,simTries=0,stat4=0,stat2=0,act=-1;bool sawOk=false;
 std::deque<Cmd> queue;std::unique_ptr<Cmd> cur;uint32_t deadline=0;bool promptSent=false,awaitingAfter=false;std::string reply,line,raw;
 bool open[Links]={},ready[Links]={};
 void touch(){st.revision++;}
 void phaseText(device::ModemPhase p,const char *msg=""){if(st.phase!=p||st.message!=msg){st.phase=p;st.message=msg;touch();}}
 void send(Cmd c){queue.push_back(std::move(c));}
 void at(const char *text,uint32_t timeout,std::function<void(bool,const std::string&)> done=nullptr){Cmd c;c.text=text;c.timeoutMs=timeout;c.done=std::move(done);queue.push_back(std::move(c));}
 // ---- bytes to lines ----
 void pump(uint32_t now){int budget=4096;int b;while(budget--&&(b=io.read())>=0){
   if(rawLeft){raw+=char(b);if(--rawLeft==0)reply+=raw,raw.clear();continue;}
   char ch=char(b);
   if(ch=='\n'){onLine(modem_at::trim(line),now);line.clear();continue;}
   if(ch=='\r')continue;
   if(line.size()<4096)line+=ch;
   if(cur&&cur->prompt&&!promptSent&&(line==">"||line=="> ")){promptSent=true;io.write(cur->payload);line.clear();}}}
 void urc(const std::string &l){std::string mem;int n,mr;
  if(modem_at::parseCmti(l,mem,n)){smsDue=true;if(onSmsAlert)onSmsAlert();}
  else if(modem_at::startsWith(l,"+CIPRXGET: 1,")){int link;if(modem_at::toInt(l.substr(13),link)&&link>=0&&link<Links)ready[link]=true;}
  else if(modem_at::startsWith(l,"+IPCLOSE:")){int link;if(modem_at::toInt(modem_at::splitFields(l.substr(9))[0],link)&&link>=0&&link<Links)open[link]=false;}
  else if(modem_at::startsWith(l,"+CIPEVENT:")){for(int i=0;i<Links;i++)open[i]=false;if(st.data==device::DataPhase::Online){st.data=device::DataPhase::Failed;st.ip.clear();st.message="Data connection lost";touch();nextData=io.ms()+10000;netStep=true;}}
  else if(modem_at::startsWith(l,"+CPIN: NOT READY")){if(phase==P::Net){phase=P::Sim;simTries=0;st.simReady=false;st.data=device::DataPhase::Idle;touch();}}
  (void)mr;}
 void onLine(const std::string &l,uint32_t now){(void)now;if(l.empty())return;
  if(cur&&l==cur->text)return;// echo
  bool ours=false;
  if(cur){
   if(awaitingAfter){if(cur->after&&modem_at::startsWith(l,cur->after)){reply+=l+"\n";finish(true,reply);return;}
    if(l=="ERROR"||modem_at::startsWith(l,"+CME ERROR")||modem_at::startsWith(l,"+IP ERROR")){reply+=l+"\n";finish(false,reply);return;}}
   else if(l=="OK"){if(cur->after){awaitingAfter=true;deadline=io.ms()+cur->afterMs;return;}finish(true,reply);return;}
   else if(l=="ERROR"||modem_at::startsWith(l,"+CME ERROR")||modem_at::startsWith(l,"+CMS ERROR")||modem_at::startsWith(l,"+IP ERROR")||modem_at::startsWith(l,"+CIPERROR")){reply+=l+"\n";finish(false,reply);return;}
   if(cur->rawPrefix&&modem_at::startsWith(l,cur->rawPrefix)){auto f=modem_at::splitFields(l);int len;if(f.size()>=3&&modem_at::toInt(f[2],len)&&len>0){reply+=l+"\n";rawLeft=uint32_t(len);return;}}
   if(!modem_at::startsWith(l,"+CMTI")&&!modem_at::startsWith(l,"+CIPRXGET: 1,")&&!modem_at::startsWith(l,"+IPCLOSE")&&!modem_at::startsWith(l,"+CIPEVENT")&&!modem_at::startsWith(l,"+CPIN: NOT READY")){if(reply.size()<24576)reply+=l+"\n";ours=true;}}
  if(!ours)urc(l);}
 void finish(bool ok,const std::string &text){if(!cur)return;std::unique_ptr<Cmd> c=std::move(cur);std::string r=text;reply.clear();raw.clear();rawLeft=0;promptSent=awaitingAfter=false;if(c->done)c->done(ok,r);}
 void start(Cmd c,uint32_t now){cur.reset(new Cmd(std::move(c)));reply.clear();raw.clear();rawLeft=0;promptSent=awaitingAfter=false;line.clear();deadline=now+cur->timeoutMs;io.write(cur->text+"\r");}
 // ---- the sequence ----
 void advance(uint32_t now){
  if(!queue.empty()){Cmd c=std::move(queue.front());queue.pop_front();start(std::move(c),now);return;}
  if(!want&&phase!=P::Off&&phase!=P::Stopping){beginStop(now);return;}
  switch(phase){
   case P::Off:if(want&&int32_t(now-nextPoll)>=0){io.rail(true);phase=P::RailWait;until=now+1500;phaseText(device::ModemPhase::Powering);st.data=device::DataPhase::Idle;st.simReady=false;st.operatorName.clear();st.ip.clear();st.csq=99;st.bars=0;st.rat=0;st.roaming=false;st.model.clear();for(int i=0;i<Links;i++)open[i]=ready[i]=false;}break;
   case P::RailWait:if(int32_t(now-until)>=0&&io.pinsSettled()){phase=P::Probe;probes=0;probe();}break;
   case P::Probe:break;// the probe commands run through the queue
   case P::Pulse:if(!pulseHeld){io.pwrkey(true);if(io.pinsSettled()){pulseHeld=true;until=now+1200;}}else if(int32_t(now-until)>=0){io.pwrkey(false);if(io.pinsSettled()){phase=P::Sync;syncStart=now;nextPoll=now+2000;}}break;
   case P::Sync:if(int32_t(now-syncStart)>35000){fail("Modem not responding");break;}if(int32_t(now-nextPoll)>=0){nextPoll=now+1000;at("AT",800,[this](bool ok,const std::string&){if(ok&&phase==P::Sync){phase=P::Init;initSequence();}});}break;
   case P::Init:break;
   case P::Sim:simCheck(now);break;
   case P::Net:net(now);break;
   case P::Stopping:if(int32_t(now-until)>=0){io.rail(false);if(io.pinsSettled()){phase=P::Off;nextPoll=now+1000;phaseText(device::ModemPhase::Off);st.data=device::DataPhase::Idle;st.simReady=false;st.ip.clear();st.operatorName.clear();st.bars=0;st.csq=99;touch();}}break;}}
 void probe(){at("AT",1000,[this](bool ok,const std::string&){if(phase!=P::Probe)return;if(ok){phase=P::Init;initSequence();return;}if(++probes<2){probe();return;}phase=P::Pulse;pulseHeld=false;});}
 void fail(const char *msg){phaseText(device::ModemPhase::Failed,msg);phase=P::Off;nextPoll=io.ms()+30000;io.pwrkey(false);}
 void beginStop(uint32_t now){queue.clear();phase=P::Stopping;st.data=device::DataPhase::Idle;st.ip.clear();until=now+2500;Cmd c;c.text="AT+CPOF";c.timeoutMs=3000;queue.push_back(c);}
 void initSequence(){phaseText(device::ModemPhase::Starting);pinTried=false;
  at("ATE0",1000);at("AT+CMEE=1",1000);
  at("ATI",2000,[this](bool ok,const std::string &r){if(ok){std::string m=modem_at::modelLabel(r);if(!m.empty()&&m!=st.model){st.model=m;touch();}}});
  at("AT+CSCS=\"UCS2\"",2000,[this](bool ok,const std::string&){ucs2=ok;});
  at("AT+CMGF=1",2000);at("AT+CSDH=1",2000);at("AT+CNMI=2,1,0,0,0",2000);at("AT+COPS=3,0",2000);at("AT+CTZU=1",2000);
  at("AT+CPIN?",2000,[this](bool,const std::string&){phase=P::Sim;simTries=0;nextOps=0;});}
 bool ucs2=false;
 void simCheck(uint32_t now){if(int32_t(now-nextOps)<0||pending())return;nextOps=now+1000;
  at("AT+CPIN?",3000,[this](bool ok,const std::string &r){if(phase!=P::Sim)return;
   auto cp=modem_at::Cpin::Unknown;int code=0;bool cme=false;for(auto &l:modem_at::splitLines(r)){auto c=modem_at::parseCpin(l);if(c!=modem_at::Cpin::Unknown)cp=c;int k;if(modem_at::parseCmeError(l,k)){cme=true;code=k;}}
   if(ok&&cp==modem_at::Cpin::Ready){st.simReady=true;phase=P::Net;stat4=stat2=0;nextPoll=0;simTries=0;smsDue=true;phaseText(device::ModemPhase::Searching);return;}
   if(ok&&cp==modem_at::Cpin::Pin){if(!cfg.pin.empty()&&device::validSimPin(cfg.pin)&&!pinTried){pinTried=true;Cmd c;c.text="AT+CPIN=\""+cfg.pin+"\"";c.timeoutMs=8000;c.done=[this](bool ok2,const std::string&){if(!ok2){phaseText(device::ModemPhase::PinNeeded,"Wrong SIM PIN");}nextOps=io.ms()+3000;};queue.push_back(c);return;}
    phaseText(device::ModemPhase::PinNeeded,pinTried?"Wrong SIM PIN":"");nextOps=io.ms()+4000;return;}
   if(ok&&(cp==modem_at::Cpin::Puk)){phaseText(device::ModemPhase::PukNeeded);nextOps=io.ms()+10000;return;}
   if(cme&&modem_at::simMissing(code)){phaseText(device::ModemPhase::NoSim);nextOps=io.ms()+5000;return;}
   if(++simTries>40){fail("SIM not ready");return;}nextOps=io.ms()+1500;});}
 void net(uint32_t now){
  if(!st.simReady){phase=P::Sim;return;}
  if(int32_t(now-nextPoll)>=0&&!pending()){nextPoll=now+(st.phase==device::ModemPhase::Registered?15000:3000);pollNet();return;}
  if(smsDue&&!pending()&&int32_t(now-nextSms)>=0){smsDue=false;nextSms=now+1000;listSms();return;}
  if(st.phase==device::ModemPhase::Registered)dataStep(now);
  if(st.phase==device::ModemPhase::Registered&&int32_t(now-smsPoll)>=300000){smsPoll=now;smsDue=true;}}
 void pollNet(){
  at("AT+CEREG?",2000,[this](bool ok,const std::string &r){stat4=0;act=-1;if(!ok)return;for(auto &l:modem_at::splitLines(r)){int s,a;if(modem_at::parseReg(l,"+CEREG",s,a)){stat4=s;if(modem_at::regOk(s))act=a;}}});
  at("AT+CREG?",2000,[this](bool ok,const std::string &r){stat2=0;int a2=-1;if(ok)for(auto &l:modem_at::splitLines(r)){int s,a;if(modem_at::parseReg(l,"+CREG",s,a)){stat2=s;if(modem_at::regOk(s))a2=a;}}
   bool okNet=modem_at::regOk(stat4)||modem_at::regOk(stat2);int stat=modem_at::regOk(stat4)?stat4:stat2;if(!okNet&&modem_at::regDenied(stat4))stat=stat4;
   if(okNet){if(st.phase!=device::ModemPhase::Registered||st.roaming!=modem_at::regRoaming(stat)){st.roaming=modem_at::regRoaming(stat);phaseText(device::ModemPhase::Registered);st.operatorName.clear();}
    int rat=modem_at::ratClass(modem_at::regOk(stat4)?act:a2);if(rat&&rat!=st.rat){st.rat=rat;touch();}}
   else{if(st.phase==device::ModemPhase::Registered){dataDown();}phaseText(modem_at::regDenied(stat4)||modem_at::regDenied(stat2)?device::ModemPhase::Denied:device::ModemPhase::Searching);if(st.csq!=99||st.bars){}}});
  at("AT+CSQ",2000,[this](bool ok,const std::string &r){if(!ok)return;for(auto &l:modem_at::splitLines(r)){int a,b;if(modem_at::parseCsq(l,a,b)){int bars=modem_at::csqBars(a);if(a!=st.csq||bars!=st.bars){st.csq=a;st.bars=bars;touch();}}}});
  if(st.operatorName.empty())at("AT+COPS?",3000,[this](bool ok,const std::string &r){if(!ok)return;for(auto &l:modem_at::splitLines(r)){std::string o;int a;if(modem_at::parseCops(l,o,a)&&o!=st.operatorName){st.operatorName=o;touch();}}});}
 void dataDown(){if(st.data!=device::DataPhase::Idle){st.data=device::DataPhase::Idle;st.ip.clear();touch();}for(int i=0;i<Links;i++)open[i]=ready[i]=false;}
 void dataStep(uint32_t now){
  roamingOk=!st.roaming||cfg.roaming;
  bool wanted=dataWanted&&roamingOk;
  if(dataBusy||pending())return;
  if(st.data==device::DataPhase::Online&&(!wanted||dataRestart)){dataBusy=true;Cmd c;c.text="AT+NETCLOSE";c.timeoutMs=10000;c.after="+NETCLOSE:";c.afterMs=60000;c.done=[this](bool,const std::string&){dataBusy=false;dataRestart=false;dataDown();};queue.push_back(c);return;}
  if(st.data==device::DataPhase::Failed&&int32_t(now-nextData)<0)return;
  if(!wanted){if(dataRestart)dataRestart=false;return;}
  if(st.data==device::DataPhase::Online)return;
  dataRestart=false;dataBusy=true;st.data=device::DataPhase::Connecting;st.message="";touch();
  auto bail=[this](const char *why){dataBusy=false;st.data=device::DataPhase::Failed;st.message=why;nextData=io.ms()+15000;touch();};
  std::string apn=cfg.apn;if(!modem_at::atSafe(apn))apn.clear();
  at(("AT+CGDCONT=1,\"IP\",\""+apn+"\"").c_str(),3000);
  if(!cfg.apnUser.empty()&&modem_at::atSafe(cfg.apnUser)&&modem_at::atSafe(cfg.apnPass))at(("AT+CGAUTH=1,1,\""+cfg.apnPass+"\",\""+cfg.apnUser+"\"").c_str(),3000);else at("AT+CGAUTH=1,0",3000);
  at("AT+CIPMODE=0",2000);at("AT+CIPRXGET=1",2000);
  at("AT+CGACT=1,1",30000,[bail](bool ok,const std::string&){if(!ok)bail("Could not activate data");});
  Cmd n;n.text="AT+NETOPEN";n.timeoutMs=10000;n.after="+NETOPEN:";n.afterMs=75000;
  n.done=[this,bail](bool ok,const std::string &r){bool already=r.find("already opened")!=std::string::npos;bool good=already||(ok&&r.find("+NETOPEN: 0")!=std::string::npos);
   if(!good){bail("Data connection refused");return;}
   at("AT+IPADDR",3000,[this](bool ok2,const std::string &r2){dataBusy=false;st.data=device::DataPhase::Online;st.message="";if(ok2)for(auto &l:modem_at::splitLines(r2)){std::string v;if(modem_at::afterPrefix(l,"+IPADDR",v))st.ip=v;}touch();});};
  queue.push_back(n);}
 void listSms(){Cmd c;c.text="AT+CMGL=\"ALL\"";c.timeoutMs=15000;c.done=[this](bool ok,const std::string &r){if(!ok)return;if(onSmsList)onSmsList(modem_at::parseCmglReply(r,ucs2));};queue.push_back(c);}
};
}
