#pragma once
// Companion commands that report or change device state, and the contact pushes, as MeshCore's companion_radio
// (examples/companion_radio/MyMesh.cpp, protocol v14). Header-only; sim/companion_state_emulator.cpp runs this same
// code against real MeshCore nodes.
// The firmware keeps one Module, sets its hooks (all required, each takes the hook context first) and loads the tuning
// blob at boot. companion.inc hands the opcodes this module owns to handle, which answers through a writer; it
// declines only a telemetry request addressed to a contact. Frames shorter than MyMesh requires get MyMesh's
// unsupported answer, as its if-chain falls to the last else. The hooks give the counters of the stats frame, the
// battery and chip temperature, persistence of the 8 byte tuning blob, stopping every app transport, wiping the
// filesystem, rebooting, installing an imported identity and running one CLI line.
// Tuning feeds the Mesh through its rx delay and airtime budget overrides. The defaults are MyMesh's, a firmware with
// another fixed airtime factor sets it before loading the blob.
// The ChatMesh callbacks for adverts, path updates, overwritten contacts and a full contact table produce the pushes,
// and every advert is recorded for the advert path query. A new advert frame is for adverts the mesh did not store
// (manual add mode, hop limit, contacts full). A telemetry request from another node is answered from the app's
// telemetry modes byte, the one of the set other params command.
// Private key export and import follow MeshCore's compile flags: undefined means off, and the commands answer
// disabled. Defining both as 1 enables them, the firmware only has to persist the identity in its install hook.
// Not reproducible here: the sensor manager (no settings, no extra telemetry channels) and the gps custom variables.
#include <helpers/BaseChatMesh.h>
#include <math.h>
#include <string.h>
#ifndef ENABLE_PRIVATE_KEY_EXPORT
#define ENABLE_PRIVATE_KEY_EXPORT 0
#endif
#ifndef ENABLE_PRIVATE_KEY_IMPORT
#define ENABLE_PRIVATE_KEY_IMPORT 0
#endif
namespace companion_state {
constexpr size_t MAX_FRAME=172;// MeshCore's MAX_FRAME_SIZE
constexpr size_t CLI_REPLY_MAX=166;// MyMesh's reply_buf
constexpr int ADVERT_PATH_TABLE_SIZE=16;
enum : uint8_t {CMD_SET_TUNING_PARAMS=21,CMD_EXPORT_PRIVATE_KEY=23,CMD_IMPORT_PRIVATE_KEY=24,CMD_SEND_TELEMETRY_REQ=39,CMD_GET_CUSTOM_VARS=40,
 CMD_SET_CUSTOM_VAR=41,CMD_GET_ADVERT_PATH=42,CMD_GET_TUNING_PARAMS=43,CMD_FACTORY_RESET=51,CMD_GET_STATS=56,CMD_RUN_CLI_COMMAND=66,
 RESP_OK=0,RESP_ERR=1,RESP_PRIVATE_KEY=14,RESP_DISABLED=15,RESP_CUSTOM_VARS=21,RESP_ADVERT_PATH=22,RESP_TUNING_PARAMS=23,RESP_STATS=24,RESP_CLI_REPLY=29,
 PUSH_ADVERT=0x80,PUSH_PATH_UPDATED=0x81,PUSH_NEW_ADVERT=0x8A,PUSH_TELEMETRY_RESPONSE=0x8B,PUSH_CONTACT_DELETED=0x8F,PUSH_CONTACTS_FULL=0x90,
 STATS_CORE=0,STATS_RADIO=1,STATS_PACKETS=2,
 ERR_UNSUPPORTED=1,ERR_NOT_FOUND=2,ERR_FILE_IO=5,ERR_ILLEGAL_ARG=6,
 MODE_DENY=0,MODE_ALLOW_FLAGS=1,MODE_ALLOW_ALL=2,PERM_BASE=0x01,PERM_LOCATION=0x02,PERM_ENVIRONMENT=0x04,TELEM_CHANNEL_SELF=1};
inline size_t error(uint8_t *out,uint8_t code){out[0]=RESP_ERR;out[1]=code;return 2;}
inline size_t put(uint8_t *out,const void *v,size_t n){memcpy(out,v,n);return n;}

// Counters the firmware fills for CMD_GET_STATS (units as MyMesh reports them).
struct Stats {
 uint16_t batteryMv=0;uint32_t uptimeSecs=0;uint16_t errFlags=0;uint8_t queueLen=0;
 int16_t noiseFloor=0;float lastRssi=0,lastSnr=0;uint32_t txAirSecs=0,rxAirSecs=0;
 uint32_t recv=0,sent=0,sentFlood=0,sentDirect=0,recvFlood=0,recvDirect=0,recvErrors=0;};
inline size_t statsFrame(const Stats &s,uint8_t type,uint8_t *out){
 size_t i=0;out[i++]=RESP_STATS;out[i++]=type;
 if(type==STATS_CORE){i+=put(out+i,&s.batteryMv,2);i+=put(out+i,&s.uptimeSecs,4);i+=put(out+i,&s.errFlags,2);out[i++]=s.queueLen;}
 else if(type==STATS_RADIO){int8_t rssi=int8_t(s.lastRssi),snr=int8_t(s.lastSnr*4);// snr in quarter dB
  i+=put(out+i,&s.noiseFloor,2);out[i++]=uint8_t(rssi);out[i++]=uint8_t(snr);i+=put(out+i,&s.txAirSecs,4);i+=put(out+i,&s.rxAirSecs,4);}
 else if(type==STATS_PACKETS){for(uint32_t v:{s.recv,s.sent,s.sentFlood,s.sentDirect,s.recvFlood,s.recvDirect,s.recvErrors})i+=put(out+i,&v,4);}
 else return error(out,ERR_ILLEGAL_ARG);
 return i;
}

// CMD_SET/GET_TUNING_PARAMS: the app sends and reads milli-units (rx delay base, airtime factor).
struct Tuning {
 float rxDelayBase=0,airtime=1.0f;
 float airtimeFactor()const{return airtime;}
 int calcRxDelay(float score,uint32_t airTime)const{if(rxDelayBase<=0.0f)return 0;return int((pow(rxDelayBase,0.85f-score)-1.0)*airTime);}
 // Persistence blob: rx delay base then airtime factor, two little endian floats.
 size_t toBlob(uint8_t *out)const{memcpy(out,&rxDelayBase,4);memcpy(out+4,&airtime,4);return 8;}
 // Boot load; out of range values are sanitised as MyMesh does (rx delay 0..20, airtime factor 0..9).
 bool fromBlob(const uint8_t *in,size_t n){if(n!=8)return false;float rx,af;memcpy(&rx,in,4);memcpy(&af,in+4,4);
  rxDelayBase=rx<0?0:rx>20.0f?20.0f:rx;airtime=af<0?0:af>9.0f?9.0f:af;return true;}
};

// Recent advert paths, newest per sender (MyMesh's advert_paths): the app's "path of the last advert".
struct AdvertPath {uint8_t pubkey_prefix[7];uint8_t path_len;char name[32];uint32_t recv_timestamp;uint8_t path[MAX_PATH_SIZE];};
struct AdvertPaths {
 AdvertPath e[ADVERT_PATH_TABLE_SIZE]={};
 // Same key: updated in place; else the entry with the oldest receive time is replaced.
 void record(const uint8_t *pub,const char *name,uint32_t now,const uint8_t *path,uint8_t pathLen){
  if(!path||!mesh::Packet::isValidPathLen(pathLen))return;
  AdvertPath *p=e;uint32_t oldest=0xFFFFFFFF;
  for(auto &x:e){if(!memcmp(x.pubkey_prefix,pub,sizeof(x.pubkey_prefix))){p=&x;break;}if(x.recv_timestamp<oldest){oldest=x.recv_timestamp;p=&x;}}
  memcpy(p->pubkey_prefix,pub,sizeof(p->pubkey_prefix));strncpy(p->name,name,sizeof(p->name)-1);p->name[sizeof(p->name)-1]=0;
  p->recv_timestamp=now;p->path_len=mesh::Packet::copyPath(p->path,path,pathLen);}
 const AdvertPath *find(const uint8_t *pub)const{for(auto &x:e)if(!memcmp(x.pubkey_prefix,pub,sizeof(x.pubkey_prefix)))return &x;return nullptr;}
};

struct Hooks {
 void *ctx=nullptr;
 void (*stats)(void*,Stats&)=nullptr;
 uint16_t (*batteryMv)(void*)=nullptr;
 float (*mcuTemperature)(void*)=nullptr;
 bool (*saveTuning)(void*,const uint8_t*,size_t)=nullptr;
 void (*disableTransports)(void*)=nullptr;
 bool (*wipe)(void*)=nullptr;
 void (*reboot)(void*)=nullptr;
 bool (*installIdentity)(void*,const mesh::LocalIdentity&)=nullptr;
 size_t (*runCli)(void*,const char*,char*,size_t)=nullptr;
 bool (*gpsAvailable)(void*)=nullptr;// a GPS module is present: only then is "gps" a custom variable
 bool (*gpsEnabled)(void*)=nullptr;
 bool (*setGps)(void*,bool)=nullptr;
};
struct Module {Hooks hooks;Tuning tuning;AdvertPaths adverts;mesh::LocalIdentity *self=nullptr;};

// CMD_EXPORT_PRIVATE_KEY {23} -> {14}{private key 64}; off: {15}.
template<bool Enabled> size_t exportPrivateKey(mesh::LocalIdentity &self,uint8_t *out){
 if(!Enabled){out[0]=RESP_DISABLED;return 1;}
 out[0]=RESP_PRIVATE_KEY;self.writeTo(out+1,64);return 65;}
// CMD_IMPORT_PRIVATE_KEY {24}{private key 64}: validated, then persisted and installed by the firmware.
template<bool Enabled> size_t importPrivateKey(Module &m,const uint8_t *in,size_t len,uint8_t *out){
 if(len<65)return error(out,ERR_UNSUPPORTED);
 if(!Enabled){out[0]=RESP_DISABLED;return 1;}
 if(!mesh::LocalIdentity::validatePrivateKey(in+1))return error(out,ERR_ILLEGAL_ARG);
 mesh::LocalIdentity id;id.readFrom(in+1,64);
 if(!m.hooks.installIdentity(m.hooks.ctx,id))return error(out,ERR_FILE_IO);
 out[0]=RESP_OK;return 1;}

// Telemetry: channel 1 battery voltage, then MCU temperature when the board has one.
template<class L> size_t lppFill(Module &m,uint8_t *out){
 L lpp(uint8_t(MAX_FRAME-8));lpp.reset();lpp.addVoltage(TELEM_CHANNEL_SELF,float(m.hooks.batteryMv(m.hooks.ctx))/1000.0f);
 float t=m.hooks.mcuTemperature(m.hooks.ctx);if(!isnan(t))lpp.addTemperature(TELEM_CHANNEL_SELF,t);
 return lpp.copy(out);}
// CMD_SEND_TELEMETRY_REQ {39,0,0,0}: the app asks for this device's own telemetry. MyMesh answers it whatever the
// telemetry modes say (they only gate other nodes' requests).
template<class L> size_t selfTelemetry(Module &m,uint8_t *out){
 out[0]=PUSH_TELEMETRY_RESPONSE;out[1]=0;memcpy(out+2,m.self->pub_key,6);return 8+lppFill<L>(m,out+8);}
// A contact asks for our telemetry: modes = base | loc<<2 | env<<4 (0 deny, 1 contact flags, 2 allow all), flags
// the contact's flags (bit 0 is "favourite", the permissions follow), inverseMask = data[1] of the request.
// {sender timestamp 4}{LPP} or 0 when base telemetry is not allowed.
template<class L> size_t contactTelemetry(Module &m,uint8_t modes,uint8_t contactFlags,uint8_t inverseMask,uint32_t senderTimestamp,uint8_t *reply){
 uint8_t cp=contactFlags>>1,perm=0;
 auto grant=[&](uint8_t mode,uint8_t bit){if(mode==MODE_ALLOW_ALL)perm|=bit;else if(mode==MODE_ALLOW_FLAGS)perm|=cp&bit;};
 grant(modes&3,PERM_BASE);grant((modes>>2)&3,PERM_LOCATION);grant((modes>>4)&3,PERM_ENVIRONMENT);
 perm&=uint8_t(~inverseMask);if(!(perm&PERM_BASE))return 0;
 memcpy(reply,&senderTimestamp,4);return 4+lppFill<L>(m,reply+4);}

// Pushes. A full contact frame, as MyMesh's writeContactRespFrame.
inline size_t contactFrame(uint8_t code,const ContactInfo &c,uint8_t *out){
 size_t i=0;out[i++]=code;memcpy(out+i,c.id.pub_key,PUB_KEY_SIZE);i+=PUB_KEY_SIZE;out[i++]=c.type;out[i++]=c.flags;out[i++]=c.out_path_len;
 memcpy(out+i,c.out_path,MAX_PATH_SIZE);i+=MAX_PATH_SIZE;
 memset(out+i,0,32);strncpy(reinterpret_cast<char*>(out+i),c.name,31);i+=32;// 31 characters and NUL, zero padded
 i+=put(out+i,&c.last_advert_timestamp,4);i+=put(out+i,&c.gps_lat,4);i+=put(out+i,&c.gps_lon,4);i+=put(out+i,&c.lastmod,4);return i;}
inline size_t keyFrame(uint8_t code,const uint8_t *pub,uint8_t *out){out[0]=code;memcpy(out+1,pub,PUB_KEY_SIZE);return 1+PUB_KEY_SIZE;}
inline size_t advertFrame(const uint8_t *pub,uint8_t *out){return keyFrame(PUSH_ADVERT,pub,out);}
inline size_t pathUpdatedFrame(const uint8_t *pub,uint8_t *out){return keyFrame(PUSH_PATH_UPDATED,pub,out);}
inline size_t contactDeletedFrame(const uint8_t *pub,uint8_t *out){return keyFrame(PUSH_CONTACT_DELETED,pub,out);}
inline size_t contactsFullFrame(uint8_t *out){out[0]=PUSH_CONTACTS_FULL;return 1;}

// One command frame in; every reply goes through write(frame,len). Returns false when the frame is not this module's.
template<class L,class W> bool handle(Module &m,const uint8_t *in,size_t len,W &&write){
 if(!len)return false;uint8_t out[MAX_FRAME];size_t n;
 switch(in[0]){
  case CMD_GET_STATS:{if(len<2){n=error(out,ERR_UNSUPPORTED);break;}Stats s;m.hooks.stats(m.hooks.ctx,s);n=statsFrame(s,in[1],out);break;}
  case CMD_SET_TUNING_PARAMS:{// MyMesh reads 9 bytes without checking the length
   if(len<9){n=error(out,ERR_ILLEGAL_ARG);break;}uint32_t rx,af;memcpy(&rx,in+1,4);memcpy(&af,in+5,4);
   Tuning next;next.rxDelayBase=float(rx)/1000.0f;next.airtime=float(af)/1000.0f;uint8_t blob[8];next.toBlob(blob);
   if(!m.hooks.saveTuning(m.hooks.ctx,blob,sizeof(blob))){n=error(out,ERR_FILE_IO);break;}// the old values stay
   m.tuning=next;out[0]=RESP_OK;n=1;break;}
  case CMD_GET_TUNING_PARAMS:{uint32_t rx=m.tuning.rxDelayBase*1000,af=m.tuning.airtime*1000;out[0]=RESP_TUNING_PARAMS;memcpy(out+1,&rx,4);memcpy(out+5,&af,4);n=9;break;}
  case CMD_GET_CUSTOM_VARS:out[0]=RESP_CUSTOM_VARS;n=1;if(m.hooks.gpsAvailable(m.hooks.ctx))n+=snprintf((char*)out+1,16,"gps:%d",m.hooks.gpsEnabled(m.hooks.ctx)?1:0);break;// the GPS is the only sensor setting
  case CMD_SET_CUSTOM_VAR:{
   if(len<4){n=error(out,ERR_UNSUPPORTED);break;}
   char t[MAX_FRAME+1];memcpy(t,in+1,len-1);t[len-1]=0;char *v=strchr(t,':');// "name:value", like setSettingValue()
   if(!v||strncmp(t,"gps:",4)||!m.hooks.gpsAvailable(m.hooks.ctx)||!m.hooks.setGps(m.hooks.ctx,strcmp(v+1,"0")!=0)){n=error(out,ERR_ILLEGAL_ARG);break;}
   out[0]=RESP_OK;n=1;break;}
  case CMD_GET_ADVERT_PATH:{
   if(len<2+PUB_KEY_SIZE){n=error(out,ERR_UNSUPPORTED);break;}auto *p=m.adverts.find(in+2);if(!p){n=error(out,ERR_NOT_FOUND);break;}
   out[0]=RESP_ADVERT_PATH;memcpy(out+1,&p->recv_timestamp,4);out[5]=p->path_len;n=6+mesh::Packet::writePath(out+6,p->path,p->path_len);break;}
  case CMD_SEND_TELEMETRY_REQ:
   if(len>=4+PUB_KEY_SIZE)return false;// a request to a contact
   n=len==4?selfTelemetry<L>(m,out):error(out,ERR_UNSUPPORTED);break;
  case CMD_FACTORY_RESET:{
   if(len<6||memcmp(in+1,"reset",5)){n=error(out,ERR_UNSUPPORTED);break;}
   m.hooks.disableTransports(m.hooks.ctx);// the app disconnects before it could read the answer, as in MyMesh
   if(!m.hooks.wipe(m.hooks.ctx)){n=error(out,ERR_FILE_IO);break;}
   out[0]=RESP_OK;write(out,size_t(1));m.hooks.reboot(m.hooks.ctx);return true;}
  case CMD_EXPORT_PRIVATE_KEY:n=exportPrivateKey<bool(ENABLE_PRIVATE_KEY_EXPORT)>(*m.self,out);break;
  case CMD_IMPORT_PRIVATE_KEY:n=importPrivateKey<bool(ENABLE_PRIVATE_KEY_IMPORT)>(m,in,len,out);break;
  case CMD_RUN_CLI_COMMAND:{
   if(len<3){n=error(out,ERR_UNSUPPORTED);break;}
   char text[MAX_FRAME+1],reply[CLI_REPLY_MAX];memcpy(text,in+1,len-1);text[len-1]=0;reply[0]=0;
   size_t r=m.hooks.runCli(m.hooks.ctx,text,reply,sizeof(reply));
   if(!r){strcpy(reply,"Unknown command");r=strlen(reply);}
   if(r>=sizeof(reply))r=sizeof(reply)-1;out[0]=RESP_CLI_REPLY;memcpy(out+1,reply,r);n=1+r;break;}
  default:return false;
 }
 write(out,n);return true;
}
}
