// PC emulator for src/mesh/companion_state.h: the companion commands that report or change device state (stats,
// tuning, custom vars, advert paths, self telemetry, factory reset, private key export/import, CLI) and the contact
// pushes. Every expected frame is written out byte by byte from examples/companion_radio/MyMesh.cpp, not computed by
// the code under test. Real MeshCore core; a repeater forwards a second node's real advert to the "phone" node.
#define ENABLE_PRIVATE_KEY_EXPORT 1
#define ENABLE_PRIVATE_KEY_IMPORT 1
#include "companion_sim.h"
// CayenneLPP as the firmware links it (the harness has no include path for it).
#include "../.pio/libdeps/mattermesh-free/CayenneLPP/src/CayenneLPP.cpp"
#include "../.pio/libdeps/mattermesh-free/CayenneLPP/src/CayenneLPPPolyline.cpp"
#include "mesh/companion_state.h"
namespace {
using Bytes=std::vector<uint8_t>;
namespace cs=companion_state;
Bytes u32(uint32_t v){Bytes b(4);memcpy(b.data(),&v,4);return b;}
Bytes cat(std::initializer_list<Bytes> parts){Bytes r;for(auto &p:parts)r.insert(r.end(),p.begin(),p.end());return r;}

// Test doubles for the firmware facilities, recording what the module did and in which order.
struct Env {
 std::vector<std::string> log;cs::Stats stats;uint16_t mv=3900;float temp=31.5f;
 bool saveOk=true,wipeOk=true,identityOk=true,gpsHere=false,gpsOn=false,gpsFail=false;uint8_t blob[8]={};size_t blobLen=0;mesh::LocalIdentity installed;bool haveInstalled=false;
 std::string cliSeen;std::string cliReply="OK - ok";
 static Env &of(void *c){return *static_cast<Env*>(c);}
 cs::Hooks hooks(){cs::Hooks h;h.ctx=this;
  h.stats=[](void *c,cs::Stats &s){s=of(c).stats;};
  h.batteryMv=[](void *c){return of(c).mv;};
  h.mcuTemperature=[](void *c){return of(c).temp;};
  h.saveTuning=[](void *c,const uint8_t *b,size_t n){auto &e=of(c);e.log.push_back("save");if(!e.saveOk)return false;memcpy(e.blob,b,n);e.blobLen=n;return true;};
  h.disableTransports=[](void *c){of(c).log.push_back("disable");};
  h.wipe=[](void *c){of(c).log.push_back("wipe");return of(c).wipeOk;};
  h.reboot=[](void *c){of(c).log.push_back("reboot");};
  h.installIdentity=[](void *c,const mesh::LocalIdentity &id){auto &e=of(c);e.log.push_back("identity");if(!e.identityOk)return false;e.installed=id;e.haveInstalled=true;return true;};
  h.runCli=[](void *c,const char *cmd,char *reply,size_t cap){auto &e=of(c);e.cliSeen=cmd;if(e.cliReply.empty())return size_t(0);snprintf(reply,cap,"%s",e.cliReply.c_str());return strlen(reply);};
  h.gpsAvailable=[](void *c){return of(c).gpsHere;};h.gpsEnabled=[](void *c){return of(c).gpsOn;};
  h.setGps=[](void *c,bool on){auto &e=of(c);if(e.gpsFail)return false;e.gpsOn=on;return true;};
  return h;}
};
struct Rig {
 Env env;mesh::LocalIdentity self;cs::Module mod;std::vector<Bytes> out;
 Rig(){Rng r;self=mesh::LocalIdentity(&r);mod.self=&self;mod.hooks=env.hooks();}
 // One command in; the single reply frame out (asserts the module claimed the frame).
 Bytes ask(Bytes in){out.clear();bool mine=cs::handle<CayenneLPP>(mod,in.data(),in.size(),[&](const uint8_t *f,size_t n){out.emplace_back(f,f+n);});assert(mine&&out.size()==1);return out[0];}
 bool claims(Bytes in){out.clear();return cs::handle<CayenneLPP>(mod,in.data(),in.size(),[&](const uint8_t *f,size_t n){out.emplace_back(f,f+n);});}
};
const Bytes UNSUPPORTED={1,1};
Bytes err(uint8_t code){return {1,code};}

// The phone-side node: MeshCore callbacks wired exactly as companion_state.h documents.
class Phone:public Parts,public BaseChatMesh {public:bool autoAdd=true;
public:Rig rig;App app;
 Phone():BaseChatMesh(radio,ms,rng,clock,pool,tables){rng.g.seed(1);self_id=mesh::LocalIdentity(&rng);begin();rig.mod.self=&self_id;}
protected:
 void onDiscoveredContact(ContactInfo &c,bool isNew,uint8_t pathLen,const uint8_t *path)override{
  uint8_t f[cs::MAX_FRAME];size_t n=isNew?cs::contactFrame(cs::PUSH_NEW_ADVERT,c,f):cs::advertFrame(c.id.pub_key,f);app.write(f,n);
  rig.mod.adverts.record(c.id.pub_key,c.name,clock.getCurrentTime(),path,pathLen);}
 void onContactPathUpdated(const ContactInfo &c)override{uint8_t f[cs::MAX_FRAME];app.write(f,cs::pathUpdatedFrame(c.id.pub_key,f));}
 void onContactOverwrite(const uint8_t *key)override{uint8_t f[cs::MAX_FRAME];app.write(f,cs::contactDeletedFrame(key,f));}
 void onContactsFull()override{uint8_t f[cs::MAX_FRAME];app.write(f,cs::contactsFullFrame(f));}
 void sendFloodScoped(const ContactInfo&,mesh::Packet *p,uint32_t delay=0)override{sendFlood(p,delay);}
 void sendFloodScoped(const mesh::GroupChannel&,mesh::Packet *p,uint32_t delay=0)override{sendFlood(p,delay);}
 float getAirtimeBudgetFactor()const override{return 0;}int calcRxDelay(float,uint32_t)const override{return 0;}bool allowPacketForward(const mesh::Packet*)override{return false;}
 ContactInfo *processAck(const uint8_t*)override{return nullptr;}
 void onMessageRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*)override{}void onCommandDataRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*)override{}
 void onCLICommandRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*,char *reply)override{reply[0]=0;}void onSignedMessageRecv(const ContactInfo&,mesh::Packet*,uint32_t,const uint8_t*,const char*)override{}
 void onChannelMessageRecv(const mesh::GroupChannel&,mesh::Packet*,uint32_t,const char*)override{}
 uint32_t calcFloodTimeoutMillisFor(uint32_t)const override{return 3000;}uint32_t calcDirectTimeoutMillisFor(uint32_t,uint8_t)const override{return 3000;}void onSendTimeout()override{}
 uint8_t onContactRequest(const ContactInfo&,uint32_t,const uint8_t*,uint8_t,uint8_t*)override{return 0;}
 void onContactResponse(const ContactInfo&,const uint8_t*,uint8_t)override{}
 bool shouldAutoAddContactType(uint8_t)const override{return autoAdd;}
};
// Advertises itself when asked; a plain chat node.
class Peer:public Phone {public:void advert(const char *name){if(auto *p=createSelfAdvert(name))sendFlood(p);}};
// Forwards everything it hears (a repeater), so the phone sees the advert with a one hop path.
class Repeater:public Parts,public mesh::Mesh {
public:Repeater():mesh::Mesh(radio,ms,rng,clock,pool,tables){rng.g.seed(7);self_id=mesh::LocalIdentity(&rng);begin();}
protected:bool allowPacketForward(const mesh::Packet*)override{return true;}};

void stats(){
 Rig r;auto &s=r.env.stats;s.batteryMv=0x0F3C;s.uptimeSecs=0x01020304;s.errFlags=0x0506;s.queueLen=7;
 s.noiseFloor=-118;s.lastRssi=-71.6f;s.lastSnr=9.5f;s.txAirSecs=0x11223344;s.rxAirSecs=0x55667788;
 s.recv=1;s.sent=2;s.sentFlood=3;s.sentDirect=4;s.recvFlood=5;s.recvDirect=6;s.recvErrors=7;
 // core: {24}{0}{battery mV u16}{uptime u32}{err flags u16}{queue u8}
 assert(r.ask({56,0})==(Bytes{24,0,0x3C,0x0F,0x04,0x03,0x02,0x01,0x06,0x05,7}));
 // radio: {24}{1}{noise floor i16}{last rssi i8 (truncated)}{last snr*4 i8}{tx air s u32}{rx air s u32}
 assert(r.ask({56,1})==(Bytes{24,1,uint8_t(-118),0xFF,uint8_t(-71),38,0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55}));
 // packets: {24}{2}{recv}{sent}{sent flood}{sent direct}{recv flood}{recv direct}{recv errors}, u32 each
 assert(r.ask({56,2})==cat({{24,2},u32(1),u32(2),u32(3),u32(4),u32(5),u32(6),u32(7)}));
 assert(r.ask({56,3})==err(6)&&r.ask({56,255})==err(6));// invalid sub-type
 assert(r.ask({56})==UNSUPPORTED);// MyMesh needs len >= 2, else the generic unknown-command answer
 printf("stats: core/radio/packets frames match MyMesh byte for byte, bad sub-type ERR_ILLEGAL_ARG, short frame unsupported\n");
}

void tuning(){
 Rig r;
 assert(r.ask({43})==cat({{23},u32(0),u32(1000)}));// MyMesh defaults: rx delay 0, airtime factor 1.0
 assert(r.mod.tuning.airtimeFactor()==1.0f&&r.mod.tuning.calcRxDelay(0.5f,100)==0);
 assert(r.ask(cat({{21},u32(1500),u32(2500)}))==Bytes{0});
 assert(r.ask({43})==cat({{23},u32(1500),u32(2500)}));
 // persistence blob: two LE floats, rx delay base then airtime factor, saved before OK
 assert(r.env.blobLen==8);float f[2];memcpy(f,r.env.blob,8);assert(f[0]==1.5f&&f[1]==2.5f);
 assert(r.mod.tuning.airtimeFactor()==2.5f&&r.mod.tuning.calcRxDelay(0.5f,100)==int((pow(1.5,0.85f-0.5f)-1.0)*100));
 // units: milli-units in, float seconds/factor stored; get truncates float*1000 exactly like MyMesh
 r.ask(cat({{21},u32(333),u32(777)}));float rx=333/1000.0f,af=777/1000.0f;uint32_t rx2=rx*1000,af2=af*1000;
 assert(r.ask({43})==cat({{23},u32(rx2),u32(af2)}));
 // reload from the blob (as boot does): same values; out of range values are sanitised like MyMesh boot (0..20, 0..9)
 cs::Tuning t;assert(t.fromBlob(r.env.blob,8)&&t.rxDelayBase==rx&&t.airtimeFactor()==af);
 float wild[2]={50.f,20.f};uint8_t b[8];memcpy(b,wild,8);assert(t.fromBlob(b,8)&&t.rxDelayBase==20.f&&t.airtimeFactor()==9.f);
 assert(!t.fromBlob(b,7));
 uint8_t out[8];assert(t.toBlob(out)==8);memcpy(f,out,8);assert(f[0]==20.f&&f[1]==9.f);
 // a failed save: error, the old values stay
 r.env.saveOk=false;auto before=r.ask({43});assert(r.ask(cat({{21},u32(9000),u32(9000)}))==err(5)&&r.ask({43})==before);
 assert(r.ask({21,1,2,3})==err(6));// too short: MyMesh would read past the frame
 printf("tuning: set/get round trip, persistence blob, reload sanitising, save failure, accessors\n");
}

void customVars(){
 Rig r;// no GPS module: MyMesh's sensors.getNumSettings() is 0
 assert(r.ask({40})==Bytes{21});
 assert(r.ask({41,'g','p','s',':','1'})==err(6));// setSettingValue fails: no such setting
 assert(r.ask({41,'a','b','c','d'})==err(6));// no ':' separator
 assert(r.ask({41,'a','b'})==UNSUPPORTED);// len < 4
 // a GPS module: the single setting "gps:<0|1>" (EnvironmentSensorManager), 0 stops it, anything else starts it
 Rig g;g.env.gpsHere=true;g.env.gpsOn=true;
 assert(g.ask({40})==Bytes({21,'g','p','s',':','1'}));
 assert(g.ask({41,'g','p','s',':','0'})==Bytes{0}&&!g.env.gpsOn&&g.ask({40})==Bytes({21,'g','p','s',':','0'}));
 assert(g.ask({41,'g','p','s',':','1'})==Bytes{0}&&g.env.gpsOn&&g.ask({40})==Bytes({21,'g','p','s',':','1'}));
 assert(g.ask({41,'g','p','s',':','x'})==Bytes{0}&&g.env.gpsOn);// not "0": start
 assert(g.ask({41,'a','b','c',':','1'})==err(6)&&g.ask({41,'g','p','s','s'})==err(6));// other names, no separator
 g.env.gpsFail=true;assert(g.ask({41,'g','p','s',':','0'})==err(6)&&g.env.gpsOn);// the GPS refused: setSettingValue false
 printf("custom vars: empty without GPS, gps:1/0 get and set with one, other names ERR_ILLEGAL_ARG, short frame unsupported\n");
}

void advertPaths(){
 Rig r;auto key=[](int i){Bytes k(32,uint8_t(i));k[0]=uint8_t(i);return k;};
 const uint8_t path[3]={0xA1,0xB2,0xC3};
 for(int i=1;i<=16;i++)r.mod.adverts.record(key(i).data(),("n"+std::to_string(i)).c_str(),1000+i,path,3);
 auto get=[&](Bytes k){Bytes f={42,0};f.insert(f.end(),k.begin(),k.end());return r.ask(f);};
 assert(get(key(1))==cat({{22},u32(1001),{3,0xA1,0xB2,0xC3}}));
 // full: the 17th distinct key evicts the oldest receive time (key 1); the lookup uses the first 7 key bytes
 r.mod.adverts.record(key(17).data(),"n17",2000,path,2);
 assert(get(key(1))==err(2));assert(get(key(2))==cat({{22},u32(1002),{3,0xA1,0xB2,0xC3}}));
 assert(get(key(17))==cat({{22},u32(2000),{2,0xA1,0xB2}}));
 // a known key is updated in place, nothing evicted; a longer lookup key differing after byte 7 still matches
 r.mod.adverts.record(key(2).data(),"n2",3000,path,0);
 assert(get(key(2))==cat({{22},u32(3000),{0}}));assert(get(key(3))==cat({{22},u32(1003),{3,0xA1,0xB2,0xC3}}));
 Bytes k=key(3);k[20]^=0xFF;assert(get(k)[0]==22);
 // invalid path length (more hops than fit): not stored
 r.mod.adverts.record(key(30).data(),"bad",4000,path,0x3F+0x40);assert(get(key(30))==err(2));
 r.mod.adverts.record(key(31).data(),"null",4000,nullptr,0);assert(get(key(31))==err(2));
 // hash size in the top bits of the path length: 2 bytes per hop, 2 hops
 const uint8_t path2[4]={1,2,3,4};r.mod.adverts.record(key(4).data(),"n4",5000,path2,0x42);
 assert(get(key(4))==cat({{22},u32(5000),{0x42,1,2,3,4}}));
 assert(r.ask({42,0})==UNSUPPORTED&&r.ask(Bytes(33,42))==UNSUPPORTED);// needs len >= 34
 printf("advert path: fill 16, evict oldest, update in place, 7 byte prefix lookup, invalid paths skipped, frame layout\n");
}

void contactPushes(){
 uint8_t f[cs::MAX_FRAME];Bytes pub(32);for(int i=0;i<32;i++)pub[i]=uint8_t(0x40+i);
 assert(cs::advertFrame(pub.data(),f)==33&&f[0]==0x80&&!memcmp(f+1,pub.data(),32));
 assert(cs::pathUpdatedFrame(pub.data(),f)==33&&f[0]==0x81&&!memcmp(f+1,pub.data(),32));
 assert(cs::contactDeletedFrame(pub.data(),f)==33&&f[0]==0x8F&&!memcmp(f+1,pub.data(),32));// MyMesh sends the key only
 assert(cs::contactsFullFrame(f)==1&&f[0]==0x90);
 ContactInfo c;memset(&c,0,sizeof(c));memcpy(c.id.pub_key,pub.data(),32);strcpy(c.name,"Ada");c.type=ADV_TYPE_REPEATER;c.flags=3;c.out_path_len=2;c.out_path[0]=9;c.out_path[1]=8;
 c.last_advert_timestamp=0x01020304;c.gps_lat=-5;c.gps_lon=6;c.lastmod=0x0A0B0C0D;
 memset(f,0x5A,sizeof(f));// stale bytes in the output buffer must not leak into the frame
 size_t n=cs::contactFrame(cs::PUSH_NEW_ADVERT,c,f);Bytes want={0x8A};want.insert(want.end(),pub.begin(),pub.end());
 want.push_back(ADV_TYPE_REPEATER);want.push_back(3);want.push_back(2);Bytes path(MAX_PATH_SIZE,0);path[0]=9;path[1]=8;want.insert(want.end(),path.begin(),path.end());
 Bytes name(32,0);memcpy(name.data(),"Ada",3);want.insert(want.end(),name.begin(),name.end());
 for(uint32_t v:{0x01020304u,uint32_t(-5),6u,0x0A0B0C0Du}){Bytes w=u32(v);want.insert(want.end(),w.begin(),w.end());}
 assert(n==want.size()&&Bytes(f,f+n)==want);
 memset(c.name,'x',sizeof(c.name));n=cs::contactFrame(0x8A,c,f);assert(f[1+32+3+MAX_PATH_SIZE+31]==0&&f[1+32+3+MAX_PATH_SIZE+30]=='x');// 31 chars + NUL, like strzcpy
 printf("pushes: 0x80/0x81/0x8F key frames, 0x90, 0x8A contact frame layout\n");
}

void selfTelemetry(){
 Rig r;
 const Bytes head={0x8B,0};Bytes got=r.ask({39,0,0,0});
 assert(got.size()==2+6+8&&Bytes(got.begin(),got.begin()+2)==head&&!memcmp(&got[2],r.self.pub_key,6));
 // CayenneLPP: channel 1 voltage 3.90 V (0x74, 0.01), channel 1 temperature 31.5 C (0x67, 0.1)
 assert(Bytes(got.begin()+8,got.end())==(Bytes{1,0x74,0x01,0x86,1,0x67,0x01,0x3B}));
 CayenneLPP dec(64);std::map<uint8_t,CayenneLPPMessage> m;assert(dec.decode(&got[8],8,m)==2&&m.count(1));assert(fabsf(m[1].voltage-3.9f)<0.005f&&fabsf(m[1].temperature-31.5f)<0.05f);
 // no MCU temperature (NAN): voltage only
 r.env.temp=NAN;got=r.ask({39,0,0,0});assert(got.size()==8+4&&Bytes(got.begin()+8,got.end())==(Bytes{1,0x74,0x01,0x86}));
 // MyMesh answers the self request without consulting the telemetry modes (the module has no such state)
 assert(r.ask({39,0,0,0}).size()==12);
 // a request for another contact (len >= 36) is not this module's; 5..35 bytes is the generic unsupported answer
 assert(!r.claims(Bytes(36,39))&&r.ask(Bytes(5,39))==UNSUPPORTED&&r.ask(Bytes(35,39))==UNSUPPORTED);assert(r.ask({39,0,0})==UNSUPPORTED);
 printf("self telemetry: 0x8B frame, CayenneLPP voltage+temperature, NaN temperature skipped, other forms not claimed\n");
}

// onContactRequest(REQ_TYPE_GET_TELEMETRY_DATA): the telemetry mode bits decide, as in MyMesh.
void remoteTelemetry(){
 Rig r;uint8_t reply[100];const uint32_t ts=0xCAFEBABE;const uint8_t base=1;
 auto ask=[&](uint8_t modes,uint8_t contactFlags,uint8_t mask){return cs::contactTelemetry<CayenneLPP>(r.mod,modes,contactFlags,mask,ts,reply);};
 // modes: bits 0-1 base, 2-3 location, 4-5 environment; 0 deny, 1 per-contact flags, 2 allow all
 assert(ask(0x00,0xFE,0)==0);// base denied
 assert(ask(0x01,0x00,0)==0);// base per-contact flags, contact flags>>1 lacks the base bit
 assert(ask(0x01,0x02,0)==12);// flags>>1 has base
 assert(ask(0x02,0x00,0)==12&&!memcmp(reply,&ts,4)&&Bytes(reply+4,reply+12)==(Bytes{1,0x74,0x01,0x86,1,0x67,0x01,0x3B}));// allow all, timestamp reflected
 assert(ask(0x02,0x00,base)==0);// the requester's inverse mask removes base: no reply
 assert(ask(0x3C,0xFE,0)==0);// location/environment modes alone never grant base
 assert(ask(0x01,0x01,0)==0);// bit 0 of the flags is the favourite bit, not a permission
 printf("remote telemetry: permission bits from CMD_SET_OTHER_PARAMS gate the reply, mask applied, timestamp reflected\n");
}

void factoryReset(){
 Rig r;
 for(Bytes bad:{Bytes{51},Bytes{51,'r','e','s','e'},Bytes{51,'r','e','s','e','x'},Bytes{51,'R','E','S','E','T'}}){assert(r.ask(bad)==UNSUPPORTED);}
 assert(r.env.log.empty());// nothing touched without the magic
 // OK reaches the app before the reboot, the transports are disabled before the wipe
 std::vector<std::string> order;Bytes in={51,'r','e','s','e','t'};
 bool mine=cs::handle<CayenneLPP>(r.mod,in.data(),in.size(),[&](const uint8_t *f,size_t n){order.push_back(Bytes(f,f+n)==Bytes{0}?"ok":"other");r.env.log.push_back("write");});assert(mine);
 assert((r.env.log==std::vector<std::string>{"disable","wipe","write","reboot"})&&order==std::vector<std::string>{"ok"});
 r.env.log.clear();r.env.wipeOk=false;assert(r.ask(in)==err(5));
 assert((r.env.log==std::vector<std::string>{"disable","wipe"}));// no reboot after a failed wipe
 printf("factory reset: magic checked, disable -> wipe -> OK -> reboot order, wipe failure ERR_FILE_IO_ERROR without reboot\n");
}

void privateKey(){
 Rig r;mesh::LocalIdentity mine=r.self;uint8_t prv[64];mine.writeTo(prv,64);
 // export: {14}{private key 64}
 Bytes want={14};want.insert(want.end(),prv,prv+64);assert(r.ask({23})==want);
 // import of a fresh identity: validated, handed to the firmware, OK
 Rng rng2;rng2.g.seed(99);mesh::LocalIdentity fresh(&rng2);uint8_t fprv[64];fresh.writeTo(fprv,64);assert(mesh::LocalIdentity::validatePrivateKey(fprv));
 Bytes imp={24};imp.insert(imp.end(),fprv,fprv+64);assert(r.ask(imp)==Bytes{0});
 assert(r.env.haveInstalled&&!memcmp(r.env.installed.pub_key,fresh.pub_key,32));
 // the export of the new identity round trips
 mesh::LocalIdentity back=r.env.installed;uint8_t bprv[64];back.writeTo(bprv,64);assert(!memcmp(bprv,fprv,64));
 // an invalid key is refused before anything is saved
 r.env.haveInstalled=false;r.env.log.clear();Bytes zero(65,0);zero[0]=24;assert(!mesh::LocalIdentity::validatePrivateKey(&zero[1]));
 assert(r.ask(zero)==err(6)&&r.env.log.empty()&&!r.env.haveInstalled);
 // a failing store: FILE_IO_ERROR; a short frame: generic unsupported
 r.env.identityOk=false;assert(r.ask(imp)==err(5));
 assert(r.ask(Bytes(64,24))==UNSUPPORTED);
 // flag off (MeshCore's default when the flags are undefined): RESP_CODE_DISABLED, nothing happens
 Bytes dis={15};r.env.log.clear();
 uint8_t o[cs::MAX_FRAME];assert(cs::exportPrivateKey<false>(r.self,o)==1&&o[0]==15);
 assert(cs::importPrivateKey<false>(r.mod,imp.data(),imp.size(),o)==1&&o[0]==15&&r.env.log.empty());
 assert(cs::importPrivateKey<false>(r.mod,imp.data(),10,o)==2&&o[0]==1&&o[1]==1);// short frames are unsupported even when disabled
 (void)dis;
 printf("private key: export frame, import validate/OK/FILE_IO_ERROR, round trip, disabled variant answers 15\n");
}

void cli(){
 Rig r;Bytes in={66,'g','e','t',' ','n','a','m','e'};Bytes got=r.ask(in);
 assert(r.env.cliSeen=="get name"&&got==(Bytes{29,'O','K',' ','-',' ','o','k'}));
 r.env.cliReply="";assert(r.ask(in)==cat({{29},Bytes{'U','n','k','n','o','w','n',' ','c','o','m','m','a','n','d'}}));// hook returned 0
 assert(r.ask({66,'x'})==UNSUPPORTED);// len < 3
 r.env.cliReply=std::string(300,'a');got=r.ask(in);assert(got.size()==1+165&&got[0]==29);// reply capped like MyMesh's 166 byte buffer
 Bytes tx(cs::MAX_FRAME,'z');tx[0]=66;r.env.cliReply="x";r.ask(tx);assert(r.env.cliSeen.size()==cs::MAX_FRAME-1);
 // Stock "xx|" prefix: reflected first in every reply, the hook sees only the text after it.
 r.env.cliReply="> 1";got=r.ask({66,'0','1','|','g','e','t',' ','w'});assert(r.env.cliSeen=="get w"&&got==(Bytes{29,'0','1','|','>',' ','1'}));
 r.env.cliReply="";got=r.ask({66,'0','1','|','g','e','t',' ','b'});assert(got==cat({{29},Bytes{'0','1','|','U','n','k','n','o','w','n',' ','c','o','m','m','a','n','d'}}));
 r.env.cliReply="x";r.ask({66,'a','b','|','z'});assert(r.env.cliSeen=="ab|z");// 4 chars or fewer: no prefix
 r.ask({66,'a','b','|','z','y'});assert(r.env.cliSeen=="zy");
 printf("cli: text forwarded NUL-terminated, reply framed 29, unknown command text, short frame, reply cap\n");
}

// A real advert from a second node, forwarded by a repeater. BaseChatMesh reports is_new=true only for a contact it did
// not store (manual add mode here): the phone gets 0x8A; once the contact is stored every advert is a plain 0x80.
void realAdvert(){
 auto *phone=new Phone();auto *peer=new Peer();auto *rep=new Repeater();
 peer->rng.g.seed(5);peer->self_id=mesh::LocalIdentity(&peer->rng);
 link(peer->radio,rep->radio);link(rep->radio,phone->radio);
 auto &app=phone->app;phone->autoAdd=false;peer->advert("Peer One");run(6000,*phone,*peer,*rep);
 assert(app.frames.size()==1);auto f=app.frames[0];
 assert(f.size()==148&&f[0]==0x8A&&!memcmp(&f[1],peer->self_id.pub_key,32));// not stored: full contact frame
 assert(f[33]==ADV_TYPE_CHAT&&std::string((char*)&f[1+32+3+MAX_PATH_SIZE])=="Peer One");
 auto *paths=&phone->rig.mod.adverts;auto *e=paths->find(peer->self_id.pub_key);
 assert(e&&e->path_len==1&&e->path[0]==rep->self_id.pub_key[0]&&!strcmp(e->name,"Peer One"));
 // the app asks for the path: {42}{0}{key}
 Bytes ask={42,0};ask.insert(ask.end(),peer->self_id.pub_key,peer->self_id.pub_key+32);
 auto r=phone->rig.ask(ask);assert(r.size()==1+4+1+1&&r[0]==22&&r[5]==1&&r[6]==rep->self_id.pub_key[0]);uint32_t ts;memcpy(&ts,&r[1],4);assert(ts==e->recv_timestamp);
 // auto add on: the contact is stored, PUSH_ADVERT with the key only, the table entry updated in place
 phone->autoAdd=true;run(3000,*phone,*peer,*rep);peer->advert("Peer One");run(6000,*phone,*peer,*rep);
 assert(app.frames.size()==2&&app.frames[1].size()==33&&app.frames[1][0]==0x80&&!memcmp(&app.frames[1][1],peer->self_id.pub_key,32));
 assert(paths->find(peer->self_id.pub_key)->recv_timestamp>ts);
 delete phone;delete peer;delete rep;
 printf("real advert: 0x8A for an unstored contact, 0x80 once stored, path (one hop through the repeater) recorded and served by CMD_GET_ADVERT_PATH\n");
}
}

int main(){
 setvbuf(stdout,nullptr,_IONBF,0);
 stats();tuning();customVars();advertPaths();contactPushes();selfTelemetry();remoteTelemetry();factoryReset();privateKey();cli();realAdvert();
 // an opcode this module does not own is left alone
 Rig r;assert(!r.claims({22})&&!r.claims({0})&&!r.claims(Bytes{}));
 printf("PASS: companion state emulator - stats, tuning, custom vars, advert paths, self/remote telemetry, factory reset, private key (on/off), CLI, contact pushes\n");
}
