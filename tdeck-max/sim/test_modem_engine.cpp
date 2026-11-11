#include <cassert>
#include <iostream>
#include "services/modem_engine.h"
using namespace modem;
// A scripted A7682E: answers the AT commands the engine sends; `silent` until its PWRKEY is pressed when `coldStart`.
struct Fake:Io {
 uint32_t t=0;std::string rx,tx,cmds;bool railOn=false,key=false,keyPulsed=false,coldStart=true,responsive=false,wasKey=false;
 std::string cpin="+CPIN: READY",pinReply,cereg="+CEREG: 0,1,\"1A2B\",\"0123\",7",creg="+CREG: 0,0",model="A7682E";int pins=0;bool wrongPin=false;
 std::string cmgl="+CMGL: 1,\"REC UNREAD\",\"002B00330039\",\"\",\"26/11/11,10:20:30+04\",145,4\r\n00480069\r\n";std::string rxData;bool netOpenFail=false;std::string mute;
 int read()override{if(rx.empty())return -1;int c=(unsigned char)rx[0];rx.erase(0,1);return c;}
 uint32_t ms()override{return t;}
 void rail(bool on)override{railOn=on;if(!on){responsive=false;coldStart=true;}else if(!coldStart)responsive=true;}
 void pwrkey(bool p)override{if(wasKey&&!p){keyPulsed=true;responsive=true;}wasKey=p;key=p;}
 void reply(const std::string &s){rx+=s;}
 void write(const std::string &bytes)override{tx+=bytes;if(!responsive)return;std::string b=bytes;size_t p;while((p=b.find('\r'))!=std::string::npos){std::string c=b.substr(0,p);b.erase(0,p+1);if(!c.empty())cmds+=c+"\n";answer(c);}}
 void answer(const std::string &c){
  if(!mute.empty()&&c==mute)return;
  auto ok=[&](const std::string &body=""){reply(body.empty()?"\r\nOK\r\n":"\r\n"+body+"\r\n\r\nOK\r\n");};
  if(c=="ATI")ok("Manufacturer: SIMCOM INCORPORATED\r\nModel: "+model+"\r\nRevision: V1");
  else if(c=="AT+CPIN?"){if(cpin.rfind("+CME",0)==0)reply("\r\n"+cpin+"\r\n");else ok(cpin);}
  else if(c.rfind("AT+CPIN=\"",0)==0){pins++;if(wrongPin)reply("\r\n+CME ERROR: 16\r\n");else{cpin="+CPIN: READY";ok();}}
  else if(c=="AT+CEREG?")ok(cereg);else if(c=="AT+CREG?")ok(creg);
  else if(c=="AT+CSQ")ok("+CSQ: 18,99");else if(c=="AT+COPS?")ok("+COPS: 0,0,\"Test Net\",7");
  else if(c=="AT+NETOPEN"){if(netOpenFail)reply("\r\n+IP ERROR: Network is already opened\r\n\r\nERROR\r\n");else reply("\r\nOK\r\n\r\n+NETOPEN: 0\r\n");}
  else if(c=="AT+NETCLOSE")reply("\r\nOK\r\n\r\n+NETCLOSE: 0\r\n");
  else if(c=="AT+IPADDR")ok("+IPADDR: 10.1.2.3");
  else if(c=="AT+CMGL=\"ALL\"")reply("\r\n"+cmgl+"\r\nOK\r\n");
  else if(c.rfind("AT+CIPOPEN=",0)==0)reply("\r\nOK\r\n\r\n+CIPOPEN: 0,0\r\n");
  else if(c.rfind("AT+CIPSEND=",0)==0)reply("\r\n>");
  else if(c=="AT+CIPCLOSE=0")reply("\r\nOK\r\n\r\n+CIPCLOSE: 0,0\r\n");
  else if(c.rfind("AT+CIPRXGET=2,",0)==0){reply("\r\n+CIPRXGET: 2,0,"+std::to_string(rxData.size())+",0\r\n"+rxData+"\r\nOK\r\n");}
  else if(c=="AT+CPOF")ok();
  else ok();}
};
static void run(Fake &f,Engine &e,uint32_t ms){for(uint32_t i=0;i<ms;i+=10){f.t+=10;e.tick();}}
static bool sent(Fake &f,const char *s){return f.cmds.find(s)!=std::string::npos;}
static device::CellularSettings cfgOn(){device::CellularSettings c;c.modem=true;c.apn="internet.test";c.network=device::NetMode::CellOnly;return c;}
int main(){
 using device::ModemPhase;using device::DataPhase;
 {// Cold start: silent modem, the rail comes on, AT probes fail, PWRKEY is pulsed, then init, SIM, registration.
  Fake f;Engine e(f);e.configure(cfgOn());e.setWanted(true);run(f,e,100);assert(f.railOn&&e.status().phase==ModemPhase::Powering);
  run(f,e,6000);assert(f.keyPulsed&&!f.key);
  run(f,e,15000);auto s=e.status();assert(s.phase==ModemPhase::Registered&&s.simReady&&s.model=="A7682E"&&s.csq==18&&s.bars==3&&s.operatorName=="Test Net"&&s.rat==3&&!s.roaming);
  assert(sent(f,"ATE0")&&sent(f,"AT+CSCS=\"UCS2\"")&&sent(f,"AT+CMGF=1")&&sent(f,"AT+CSDH=1")&&sent(f,"AT+CNMI=2,1,0,0,0"));
 }
 {// Modem already on (warm restart): no PWRKEY pulse.
  Fake f;f.coldStart=false;Engine e(f);e.configure(cfgOn());e.setWanted(true);run(f,e,12000);assert(!f.keyPulsed&&e.status().phase==ModemPhase::Registered);
 }
 {// A7670G is recognised from ATI.
  Fake f;f.coldStart=false;f.model="A7670G-LLSE";Engine e(f);e.configure(cfgOn());e.setWanted(true);run(f,e,12000);assert(e.status().model=="A7670G");
 }
 {// SIM PIN: sent once with a saved PIN; wrong PIN is not retried; no PIN waits.
  Fake f;f.coldStart=false;f.cpin="+CPIN: SIM PIN";auto c=cfgOn();c.pin="1234";Engine e(f);e.configure(c);e.setWanted(true);run(f,e,12000);assert(f.pins==1&&e.status().phase==ModemPhase::Registered);
  Fake g;g.coldStart=false;g.cpin="+CPIN: SIM PIN";g.wrongPin=true;Engine h(g);h.configure(c);h.setWanted(true);run(g,h,20000);assert(g.pins==1&&h.status().phase==ModemPhase::PinNeeded&&h.status().message=="Wrong SIM PIN");
  c.pin="";Fake k;k.coldStart=false;k.cpin="+CPIN: SIM PIN";Engine m(k);m.configure(c);m.setWanted(true);run(k,m,12000);assert(k.pins==0&&m.status().phase==ModemPhase::PinNeeded);
  g.wrongPin=false;c.pin="4321";h.configure(c);run(g,h,12000);assert(g.pins==2&&h.status().phase==ModemPhase::Registered);// a new PIN is tried
  Fake p;p.coldStart=false;p.cpin="+CPIN: SIM PUK";Engine q(p);q.configure(cfgOn());q.setWanted(true);run(p,q,12000);assert(q.status().phase==ModemPhase::PukNeeded);
 }
 {// No SIM, then the card appears.
  Fake f;f.coldStart=false;f.cpin="+CME ERROR: 10";Engine e(f);e.configure(cfgOn());e.setWanted(true);run(f,e,12000);assert(e.status().phase==ModemPhase::NoSim&&!e.status().simReady);
  f.cpin="+CPIN: READY";run(f,e,12000);assert(e.status().phase==ModemPhase::Registered);
 }
 {// Roaming, denied, searching.
  Fake f;f.coldStart=false;f.cereg="+CEREG: 0,5,\"1\",\"2\",7";Engine e(f);e.configure(cfgOn());e.setWanted(true);run(f,e,12000);assert(e.status().phase==ModemPhase::Registered&&e.status().roaming);
  Fake g;g.coldStart=false;g.cereg="+CEREG: 0,3";Engine h(g);h.configure(cfgOn());h.setWanted(true);run(g,h,12000);assert(h.status().phase==ModemPhase::Denied);
  Fake k;k.coldStart=false;k.cereg="+CEREG: 0,2";Engine m(k);m.configure(cfgOn());m.setWanted(true);run(k,m,12000);assert(m.status().phase==ModemPhase::Searching);
  k.cereg="+CEREG: 0,1";run(k,m,16000);assert(m.status().phase==ModemPhase::Registered);
 }
 {// Data bearer: only when wanted; APN and PAP login sent; torn down when no longer wanted; roaming gate.
  Fake f;f.coldStart=false;auto c=cfgOn();c.apnUser="u";c.apnPass="p";Engine e(f);e.configure(c);e.setWanted(true);run(f,e,12000);assert(e.status().data==DataPhase::Idle&&!sent(f,"NETOPEN"));
  e.setDataWanted(true);run(f,e,5000);auto s=e.status();assert(s.data==DataPhase::Online&&s.ip=="10.1.2.3"&&e.bearerUp());
  assert(sent(f,"AT+CGDCONT=1,\"IP\",\"internet.test\"")&&sent(f,"AT+CGAUTH=1,1,\"p\",\"u\"")&&sent(f,"AT+CIPRXGET=1")&&sent(f,"AT+CGACT=1,1"));
  e.setDataWanted(false);run(f,e,5000);assert(e.status().data==DataPhase::Idle&&sent(f,"AT+NETCLOSE"));
  Fake g;g.coldStart=false;g.cereg="+CEREG: 0,5";Engine h(g);h.configure(cfgOn());h.setWanted(true);h.setDataWanted(true);run(g,h,12000);assert(h.status().roaming&&h.status().data==DataPhase::Idle&&!sent(g,"NETOPEN"));
  auto r=cfgOn();r.roaming=true;h.configure(r);run(g,h,12000);assert(h.status().data==DataPhase::Online);
  Fake k;k.coldStart=false;k.netOpenFail=true;Engine m(k);m.configure(cfgOn());m.setWanted(true);m.setDataWanted(true);run(k,m,12000);assert(m.status().data==DataPhase::Online);// "already opened" counts as open
  // A lost network URC drops the bearer and it reopens later.
  e.setDataWanted(true);run(f,e,5000);assert(e.status().data==DataPhase::Online);f.reply("\r\n+CIPEVENT: NETWORK CLOSED UNEXPECTEDLY\r\n");run(f,e,100);assert(e.status().data==DataPhase::Failed);run(f,e,40000);assert(e.status().data==DataPhase::Online);
  // No APN login when the user is empty.
  Fake n;n.coldStart=false;Engine o(n);o.configure(cfgOn());o.setWanted(true);o.setDataWanted(true);run(n,o,12000);assert(sent(n,"AT+CGAUTH=1,0")&&!sent(n,"AT+CGAUTH=1,1"));
 }
 {// SMS: the inbox is listed after registration and again after +CMTI.
  Fake f;f.coldStart=false;Engine e(f);int lists=0;std::vector<modem_at::ParsedSms> last;e.onSmsList=[&](const std::vector<modem_at::ParsedSms> &v){lists++;last=v;};
  int alerts=0;e.onSmsAlert=[&]{alerts++;};e.configure(cfgOn());e.setWanted(true);run(f,e,12000);assert(lists>=1&&last.size()==1&&last[0].number=="+39"&&last[0].text=="Hi"&&last[0].unread);
  int before=lists;f.reply("\r\n+CMTI: \"SM\",2\r\n");run(f,e,3000);assert(alerts==1&&lists==before+1);
  e.refreshSms();run(f,e,3000);assert(lists==before+2);
 }
 {// Socket commands: open (OK then +CIPOPEN), send (prompt payload then +CIPSEND), read with binary data holding "\r\nOK\r\n", close.
  Fake f;f.coldStart=false;Engine e(f);e.configure(cfgOn());e.setWanted(true);e.setDataWanted(true);run(f,e,12000);
  bool opened=false,sentOk=false,readOk=false,closed=false;std::string got;
  Cmd o;o.text="AT+CIPOPEN=0,\"TCP\",\"example.org\",443";o.timeoutMs=5000;o.after="+CIPOPEN:";o.afterMs=20000;o.done=[&](bool ok,const std::string &r){opened=ok&&r.find("+CIPOPEN: 0,0")!=std::string::npos;};e.submit(o);run(f,e,200);assert(opened);
  Cmd s;s.text="AT+CIPSEND=0,5";s.payload="hello";s.prompt=true;s.timeoutMs=5000;s.after="+CIPSEND:";s.afterMs=5000;s.done=[&](bool ok,const std::string&){sentOk=ok;};
  f.rx.clear();e.submit(s);run(f,e,50);assert(f.tx.find("hello")!=std::string::npos);f.reply("\r\nOK\r\n\r\n+CIPSEND: 0,5,5\r\n");run(f,e,100);assert(sentOk);
  f.rxData=std::string("A\r\nOK\r\nB\0C",10);Cmd r;r.text="AT+CIPRXGET=2,0,1500";r.rawPrefix="+CIPRXGET: 2,";r.done=[&](bool ok,const std::string &t){readOk=ok;size_t p=t.find('\n');got=t.substr(p+1);};e.submit(r);run(f,e,200);assert(readOk&&got==f.rxData);
  f.reply("\r\n+CIPRXGET: 1,3\r\n");run(f,e,50);assert(e.linkData(3));e.linkDrained(3);assert(!e.linkData(3));
  e.linkOpened(0);assert(e.linkOpen(0));f.reply("\r\n+IPCLOSE: 0,1\r\n");run(f,e,50);assert(!e.linkOpen(0));
  Cmd c;c.text="AT+CIPCLOSE=0";c.timeoutMs=3000;c.after="+CIPCLOSE:";c.afterMs=5000;c.done=[&](bool ok,const std::string&){closed=ok;};e.submit(c);run(f,e,100);assert(closed);
 }
 {// A command that is never answered ends by timeout and the queue goes on.
  Fake f;f.coldStart=false;Engine e(f);e.configure(cfgOn());e.setWanted(true);run(f,e,12000);bool res=true,second=false;
  Cmd a;a.text="AT+SILENT";a.timeoutMs=500;a.done=[&](bool ok,const std::string&){res=ok;};Cmd b;b.text="AT";b.done=[&](bool ok,const std::string&){second=ok;};
  f.mute="AT+SILENT";f.cmds.clear();e.submit(a);e.submit(b);run(f,e,100);assert(res&&!second);run(f,e,700);assert(!res&&second);
 }
 {// Disabling: AT+CPOF, then the rail goes off; enabling again powers it back.
  Fake f;f.coldStart=false;Engine e(f);e.configure(cfgOn());e.setWanted(true);run(f,e,12000);e.setWanted(false);run(f,e,6000);assert(sent(f,"AT+CPOF")&&!f.railOn&&e.status().phase==ModemPhase::Off&&!e.status().simReady);
  e.setWanted(true);run(f,e,25000);assert(f.railOn&&e.status().phase==ModemPhase::Registered);
 }
 {// A modem that never answers ends in Failed after the sync window, then retries.
  Fake f;f.coldStart=true;Engine e(f);e.configure(cfgOn());e.setWanted(true);
  for(int i=0;i<5000;i++){f.t+=10;f.responsive=false;e.tick();}assert(e.status().phase==ModemPhase::Failed);
 }
 std::cout<<"PASS: modem engine power/PIN/registration/data/SMS/socket sequences\n";
}
