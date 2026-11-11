#include "services/modem.h"
#include "services/modem_engine.h"
#include "services/sms_inbox.h"
#include "services/connectivity.h"
#include <Arduino.h>
#include <Preferences.h>
#include <TDeckMaxBoard.h>
#include <atomic>
#include <deque>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
// NVS "mmcell": the settings (modem, net, roam, pin, apn, apnu, apnp, mqtt, mtls, host, mport, muser, mpass, region) and the
// unread SMS hashes ("unread"). The engine (modem_engine.h) runs in the "matter-modem" task; every access to it holds `lock`.
// The XL9555 and Wire belong to the main loop, so the task only requests pin levels and modemLoop() applies them.
namespace device {
static CellularSettings cfg;static modem::Engine *engine=nullptr;static SemaphoreHandle_t lock=nullptr;static ExtensionIOXL9555 *expander=nullptr;static bool uartOn=false,taskOn=false;
static std::atomic<bool> railWant{false},keyWant{false},dataUp{false};static std::atomic<uint8_t> netMode{uint8_t(NetMode::WifiThenCell)};
static bool railApplied=false,keyApplied=false;static SmsInbox inbox;static std::deque<SmsMessage> incoming;static SmsSendState sendState;static DataWish wish;static uint32_t wishAt=0,smsRev=0;static bool riLow=false;
static Preferences prefs;
struct Guard {Guard(){xSemaphoreTake(lock,portMAX_DELAY);}~Guard(){xSemaphoreGive(lock);}};
struct UartIo:modem::Io {
 int read()override{return Serial1.read();}
 void write(const std::string &bytes)override{Serial1.write(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size());}
 uint32_t ms()override{return millis();}
 void rail(bool on)override{railWant=on;}
 void pwrkey(bool pressed)override{keyWant=pressed;}
 bool pinsSettled()override{return !expander||(railApplied==railWant.load()&&keyApplied==keyWant.load());}
};
static UartIo uartIo;
static void load(){if(!prefs.begin("mmcell",false))return;cfg.modem=prefs.getBool("modem",false);cfg.network=NetMode(constrain(prefs.getUChar("net",uint8_t(NetMode::WifiThenCell)),0,NetModeCount-1));cfg.roaming=prefs.getBool("roam",false);
 cfg.pin=prefs.getString("pin","").c_str();cfg.apn=prefs.getString("apn","").c_str();cfg.apnUser=prefs.getString("apnu","").c_str();cfg.apnPass=prefs.getString("apnp","").c_str();
 cfg.mqtt=prefs.getBool("mqtt",false);cfg.mqttTls=prefs.getBool("mtls",false);cfg.host=prefs.getString("host","").c_str();cfg.port=uint16_t(prefs.getUInt("mport",defaultMqttPort(cfg.mqttTls)));cfg.mqttUser=prefs.getString("muser","").c_str();cfg.mqttPass=prefs.getString("mpass","").c_str();cfg.region=prefs.getString("region","").c_str();
 size_t n=prefs.getBytesLength("unread")/sizeof(uint32_t);if(n&&n<=SmsUnreadMax){inbox.unreadHashes.resize(n);prefs.getBytes("unread",inbox.unreadHashes.data(),n*sizeof(uint32_t));}prefs.end();}
static bool store(const CellularSettings &c){if(!prefs.begin("mmcell",false))return false;bool ok=true;
 ok&=prefs.putBool("modem",c.modem)==1;ok&=prefs.putUChar("net",uint8_t(c.network))==1;ok&=prefs.putBool("roam",c.roaming)==1;
 auto text=[&](const char *key,const std::string &v){if(v.empty())ok&=!prefs.isKey(key)||prefs.remove(key);else ok&=prefs.putString(key,v.c_str())==v.size();};
 text("pin",c.pin);text("apn",c.apn);text("apnu",c.apnUser);text("apnp",c.apnPass);ok&=prefs.putBool("mqtt",c.mqtt)==1;ok&=prefs.putBool("mtls",c.mqttTls)==1;text("host",c.host);ok&=prefs.putUInt("mport",c.port)==sizeof(uint32_t);text("muser",c.mqttUser);text("mpass",c.mqttPass);text("region",c.region);prefs.end();return ok;}
static void saveUnread(){if(!prefs.begin("mmcell",false))return;if(inbox.unreadHashes.empty()){if(prefs.isKey("unread"))prefs.remove("unread");}else prefs.putBytes("unread",inbox.unreadHashes.data(),inbox.unreadHashes.size()*sizeof(uint32_t));prefs.end();}
static void startUart(){if(uartOn)return;uartOn=true;pinMode(BOARD_A7682E_DTR,OUTPUT);digitalWrite(BOARD_A7682E_DTR,LOW);pinMode(BOARD_A7682E_RI,INPUT_PULLUP);Serial1.setRxBufferSize(4096);Serial1.begin(115200,SERIAL_8N1,BOARD_A7682E_TXD,BOARD_A7682E_RXD);}
static void task(void*){for(;;){{Guard g;engine->tick();dataUp=engine->bearerUp();}vTaskDelay(pdMS_TO_TICKS(10));}}
static void ensureTask(){if(taskOn||!engine)return;taskOn=true;xTaskCreatePinnedToCore(task,"matter-modem",8192,nullptr,2,nullptr,0);}
void modemBegin(ExtensionIOXL9555 &io,bool ioReady){
 lock=xSemaphoreCreateMutex();load();netMode=uint8_t(cfg.network);expander=ioReady?&io:nullptr;
 if(expander){expander->pinMode(BOARD_XL9555_00_6609_EN,OUTPUT);expander->pinMode(BOARD_XL9555_10_PWRKEY_EN,OUTPUT);expander->digitalWrite(BOARD_XL9555_10_PWRKEY_EN,LOW);if(!cfg.modem)expander->digitalWrite(BOARD_XL9555_00_6609_EN,LOW);}// a modem left powered stays so until the engine asks (railApplied false: the first request is written)
 // The audio route and amplifier pins stay with audio.cpp: AUDIO_SEL low keeps the codec on the speaker.
 engine=new modem::Engine(uartIo);
 engine->onSmsList=[](const std::vector<modem_at::ParsedSms> &list){auto fresh=inbox.merge(list);for(auto &m:fresh){if(incoming.size()>=8)incoming.pop_front();incoming.push_back(m);}saveUnread();smsRev++;};
 engine->configure(cfg);engine->setWanted(cfg.modem);
 if(cfg.modem)startUart();
 ensureTask();}
bool modemEnabled(){return cfg.modem;}
const CellularSettings &cellularSettings(){return cfg;}
CellularSettings cellularSettingsCopy(){if(!lock)return cfg;Guard g;return cfg;}
bool cellularApply(const CellularSettings &next){
 bool ok=store(next);{Guard g;cfg=next;netMode=uint8_t(next.network);if(next.modem)startUart();engine->configure(next);engine->setWanted(next.modem);}
 return ok;}
void modemLoop(){
 if(!engine)return;
 if(expander){bool r=railWant,k=keyWant;if(r!=railApplied){expander->digitalWrite(BOARD_XL9555_00_6609_EN,r?HIGH:LOW);railApplied=r;}if(k!=keyApplied){expander->digitalWrite(BOARD_XL9555_10_PWRKEY_EN,k?HIGH:LOW);keyApplied=k;}}
 uint32_t now=millis();
 if(uartOn){bool low=digitalRead(BOARD_A7682E_RI)==LOW;if(low&&!riLow){Guard g;engine->refreshSms();}riLow=low;}
 if(uint32_t(now-wishAt)>=500){wishAt=now;bool w=wish.update(cfg.network,wifiConnected(),now)&&cfg.modem;Guard g;engine->setDataWanted(w);}}
ModemStatus modemStatus(){if(!engine)return ModemStatus();Guard g;ModemStatus s=engine->status();s.smsCount=int(inbox.list.size());s.smsUnread=inbox.unreadCount();s.revision=s.revision*31u+uint32_t(s.smsCount)*7u+uint32_t(s.smsUnread)*13u+smsRev;return s;}
bool cellularDataUp(){return dataUp.load();}
NetPath netPath(){return netRoute(NetMode(netMode.load()),wifiConnected(),dataUp.load());}
// ---- blocking command from another task ----
struct Waiter {SemaphoreHandle_t sem;bool ok=false;std::string reply;Waiter(){sem=xSemaphoreCreateBinary();}~Waiter(){vSemaphoreDelete(sem);}};
static bool exec(modem::Cmd c,uint32_t waitMs,std::string &reply){
 if(!engine)return false;auto w=std::make_shared<Waiter>();auto inner=std::move(c.done);
 c.done=[w,inner](bool ok,const std::string &r){w->ok=ok;w->reply=r;if(inner)inner(ok,r);xSemaphoreGive(w->sem);};
 {Guard g;engine->submit(c);}
 if(xSemaphoreTake(w->sem,pdMS_TO_TICKS(waitMs))!=pdTRUE)return false;reply=w->reply;return w->ok;}
static std::atomic<uint16_t> linksUsed{0};
int cellOpen(const std::string &host,uint16_t port,uint32_t timeoutMs){
 if(!cellularDataUp()||!modem_at::atSafe(host)||host.empty())return -1;
 int link=-1;for(int i=0;i<modem::Links;i++){uint16_t bit=uint16_t(1u<<i);uint16_t old=linksUsed.fetch_or(bit);if(!(old&bit)){link=i;break;}}
 if(link<0)return -1;
 modem::Cmd c;c.text="AT+CIPOPEN="+std::to_string(link)+",\"TCP\",\""+host+"\","+std::to_string(port);c.timeoutMs=10000;c.after="+CIPOPEN:";c.afterMs=timeoutMs;
 c.done=[link](bool ok,const std::string &r){if(ok&&r.find("+CIPOPEN: "+std::to_string(link)+",0")!=std::string::npos)engine->linkOpened(link);};
 std::string r;bool ok=exec(c,timeoutMs+12000,r)&&r.find("+CIPOPEN: "+std::to_string(link)+",0")!=std::string::npos;
 if(!ok){linksUsed.fetch_and(uint16_t(~(1u<<link)));return -1;}
 return link;}
int cellSend(int link,const uint8_t *data,size_t n){
 size_t done=0;while(done<n){size_t chunk=std::min<size_t>(n-done,1400);modem::Cmd c;c.text="AT+CIPSEND="+std::to_string(link)+","+std::to_string(chunk);c.payload.assign(reinterpret_cast<const char*>(data+done),chunk);c.prompt=true;c.timeoutMs=20000;c.after="+CIPSEND:";c.afterMs=20000;
  std::string r;bool ok=exec(c,45000,r);int req=0,cnf=-1;auto p=r.rfind("+CIPSEND:");if(ok&&p!=std::string::npos){auto f=modem_at::splitFields(r.substr(p+9));if(f.size()>=3){modem_at::toInt(f[1],req);modem_at::toInt(f[2],cnf);}}
  if(!ok||cnf!=int(chunk)){Guard g;engine->linkClosed(link);return -1;}done+=chunk;}
 return int(done);}
int cellRecv(int link,uint8_t *out,size_t n){
 if(!n||!cellDataWaiting(link))return 0;
 modem::Cmd c;c.text="AT+CIPRXGET=2,"+std::to_string(link)+","+std::to_string(std::min<size_t>(n,1500));c.rawPrefix="+CIPRXGET: 2,";c.timeoutMs=8000;
 std::string r;bool ok=exec(c,12000,r);
 if(!ok){Guard g;engine->linkDrained(link);return 0;}
 size_t nl=r.find('\n');if(nl==std::string::npos)return 0;auto f=modem_at::splitFields(r.substr(0,nl));int got=0,rest=0;if(f.size()>=4){modem_at::toInt(f[2],got);modem_at::toInt(f[3],rest);}
 size_t have=std::min<size_t>(size_t(std::max(got,0)),std::min(n,r.size()-nl-1));memcpy(out,r.data()+nl+1,have);
 if(rest<=0){Guard g;engine->linkDrained(link);}
 return int(have);}
bool cellLinkAlive(int link){if(!engine||link<0)return false;Guard g;return engine->linkOpen(link)||engine->linkData(link);}
bool cellDataWaiting(int link){if(!engine||link<0)return false;Guard g;return engine->linkData(link);}
void cellClose(int link){if(link<0||link>=modem::Links||!engine)return;
 modem::Cmd c;c.text="AT+CIPCLOSE="+std::to_string(link);c.timeoutMs=5000;c.after="+CIPCLOSE:";c.afterMs=8000;std::string r;exec(c,15000,r);
 {Guard g;engine->linkClosed(link);engine->linkDrained(link);}linksUsed.fetch_and(uint16_t(~(1u<<link)));}
// ---- SMS ----
std::vector<SmsMessage> smsInbox(){if(!engine)return {};Guard g;return inbox.list;}
bool smsSend(const std::string &number,const std::string &text,std::string &reason){
 if(!engine){reason="Modem off";return false;}
 if(!modem_at::validSmsNumber(number)){reason="Invalid number";return false;}
 if(text.empty()){reason="Message empty";return false;}
 if(modem_at::smsUnits(text)>modem_at::smsLimit(text)){reason="Message too long";return false;}
 Guard g;
 if(sendState.phase==SmsSendPhase::Sending){reason="Busy: still sending";return false;}
 if(engine->status().phase!=ModemPhase::Registered){reason=engine->status().phase==ModemPhase::Off?"Modem is off":"No network";return false;}
 bool ucs2=engine->textUcs2();bool gsm=modem_at::gsm7Representable(text);
 modem::Cmd mode;mode.text=std::string("AT+CSMP=17,167,0,")+(gsm?"0":"8");mode.timeoutMs=3000;
 modem::Cmd send;send.text="AT+CMGS=\""+(ucs2?modem_at::ucs2Hex(number):number)+"\"";send.payload=(ucs2?modem_at::ucs2Hex(text):text)+char(0x1A);send.prompt=true;send.timeoutMs=60000;
 send.done=[](bool ok,const std::string &r){sendState.phase=ok?SmsSendPhase::Sent:SmsSendPhase::Failed;if(ok)sendState.message="Sent";else{size_t p=r.find("+CMS ERROR");sendState.message=p==std::string::npos?"Send failed":"Network refused ("+modem_at::trim(r.substr(p+11,r.find('\n',p)-p-11))+")";}};
 engine->submit(mode);engine->submit(send);sendState.phase=SmsSendPhase::Sending;sendState.message="Sending...";reason="";return true;}
SmsSendState smsSendState(){if(!engine)return SmsSendState();Guard g;return sendState;}
void smsDelete(int index){if(!engine)return;Guard g;inbox.remove(index);saveUnread();smsRev++;modem::Cmd c;c.text="AT+CMGD="+std::to_string(index);c.timeoutMs=9000;c.done=[](bool,const std::string&){engine->refreshSms();};engine->submit(c);}
void smsMarkRead(int index){if(!engine)return;Guard g;if(inbox.markRead(index)){saveUnread();smsRev++;}}
void smsRefresh(){if(!engine)return;Guard g;engine->refreshSms();}
bool smsTakeIncoming(SmsMessage &m){if(!engine)return false;Guard g;if(incoming.empty())return false;m=incoming.front();incoming.pop_front();return true;}
}
