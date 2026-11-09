// MeshCore companion protocol (the MeshCore phone / web app), shared by BLE, USB and Wi-Fi (TCP 5000).
// Included by service.cpp inside namespace matter. Frames follow meshcore-base/examples/companion_radio/MyMesh.cpp
// (protocol version 14): every opcode of that list is answered, the only "unsupported" answers are the ones MeshCore
// gives too (a 256-bit channel secret, send-text types other than plain, CLI and signed).
// This file keeps the hand-shake, node / device info, contacts (list, add, import, export, share, reset path, remove),
// channels (SET_CHANNEL adds or edits a 128-bit channel), clock, advert, name, radio parameters (with client repeat),
// TX power, path hash size, battery and storage, reboot, BLE PIN, the offline queue of the newest 32 messages
// (V3 frames) with its message-waiting and send-confirmed pushes, and the app's preferences. The rest is in
// header-only modules that the host emulators run against real MeshCore nodes: companion_remote.h (login, status,
// telemetry, binary and path requests, trace, CLI texts, signed room posts), companion_packets.h (signing, raw data and
// packets, channel data, flood scope), companion_state.h (stats, tuning, advert paths, factory reset, key import and
// export, run CLI, contact pushes) and companion_air.h (control data, anonymous requests). Long pushes use the RX log ring.
namespace companion {
enum : uint8_t {
 CMD_APP_START=1,CMD_SEND_TXT_MSG=2,CMD_SEND_CHANNEL_TXT_MSG=3,CMD_GET_CONTACTS=4,CMD_GET_DEVICE_TIME=5,CMD_SET_DEVICE_TIME=6,
 CMD_SEND_SELF_ADVERT=7,CMD_SET_ADVERT_NAME=8,CMD_ADD_UPDATE_CONTACT=9,CMD_SYNC_NEXT_MESSAGE=10,CMD_SET_RADIO_PARAMS=11,CMD_SET_RADIO_TX_POWER=12,
 CMD_RESET_PATH=13,CMD_REMOVE_CONTACT=15,CMD_SHARE_CONTACT=16,CMD_EXPORT_CONTACT=17,CMD_IMPORT_CONTACT=18,CMD_GET_CONTACT_BY_KEY=30,
 CMD_REBOOT=19,CMD_GET_BATT_AND_STORAGE=20,CMD_DEVICE_QUERY=22,CMD_GET_CHANNEL=31,CMD_SET_CHANNEL=32,CMD_SET_DEVICE_PIN=37,
 CMD_SET_ADVERT_LATLON=14,CMD_SET_OTHER_PARAMS=38,CMD_SET_AUTOADD_CONFIG=58,CMD_GET_AUTOADD_CONFIG=59,RESP_AUTOADD_CONFIG=25,
 CMD_SET_PATH_HASH_MODE=61,CMD_GET_ALLOWED_REPEAT_FREQ=60,RESP_ALLOWED_REPEAT_FREQ=26,
 RESP_SENT=6,RESP_CONTACT_MSG_RECV=7,RESP_CHANNEL_MSG_RECV=8,RESP_CONTACT_MSG_RECV_V3=16,RESP_CHANNEL_MSG_RECV_V3=17,PUSH_SEND_CONFIRMED=0x82,PUSH_MSG_WAITING=0x83,PUSH_LOG_RX_DATA=0x88,
 TXT_PLAIN=0,ERR_TABLE_FULL=3,
 RESP_OK=0,RESP_ERR=1,RESP_CONTACTS_START=2,RESP_CONTACT=3,RESP_END_OF_CONTACTS=4,RESP_SELF_INFO=5,RESP_CURR_TIME=9,
 RESP_EXPORT_CONTACT=11,RESP_NO_MORE_MESSAGES=10,RESP_BATT_AND_STORAGE=12,RESP_DEVICE_INFO=13,RESP_CHANNEL_INFO=18,
 ERR_UNSUPPORTED=1,ERR_NOT_FOUND=2,ERR_BAD_STATE=4,ERR_FILE_IO=5,ERR_ILLEGAL_ARG=6};
constexpr uint8_t ProtocolVersion=14;
static CompanionHooks hooks;
struct Out {uint8_t b[COMPANION_FRAME_MAX];size_t n=0;void u8(uint8_t v){if(n<sizeof(b))b[n++]=v;}void put(const void *p,size_t k){k=std::min(k,sizeof(b)-n);memcpy(b+n,p,k);n+=k;}
 void u32(uint32_t v){put(&v,4);}void i32(int32_t v){put(&v,4);}void u16(uint16_t v){put(&v,2);}void text(const char *s,size_t field){uint8_t z[64]={};size_t k=std::min(strlen(s),field-1);memcpy(z,s,k);put(z,field);}};
static void send(const Out &o,CompanionWriter w,void *ctx){if(w)w(ctx,o.b,o.n);}
static void ok(CompanionWriter w,void *ctx){Out o;o.u8(RESP_OK);send(o,w,ctx);}
// Preferences MeshCore keeps for the app (CMD_SET_OTHER_PARAMS, CMD_SET_AUTOADD_CONFIG): saved and reported back, so a
// configuration exported from another companion imports here. Manual add and the overwrite bit of auto add also drive
// the Contacts menu; the rest (telemetry modes, location in adverts, multi ACKs) are not acted on.
struct AppPrefs {uint8_t manualAdd=0,telemetry=0,locationPolicy=0,multiAcks=0,autoadd=0,autoaddHops=0;};
static AppPrefs appPrefs;static bool appPrefsLoaded=false;
static AppPrefs &prefs(){if(!appPrefsLoaded){appPrefsLoaded=true;Preferences p;if(p.begin("mmapp",true)){p.getBytes("prefs",&appPrefs,sizeof(appPrefs));p.end();}}return appPrefs;}
// Saved first, kept in RAM only when saved: an error answer leaves the reported values as they were.
static bool savePrefs(const AppPrefs &next){Preferences p;if(!p.begin("mmapp",false))return false;bool saved=p.putBytes("prefs",&next,sizeof(next))==sizeof(next);p.end();if(saved)appPrefs=next;return saved;}
static void err(uint8_t code,CompanionWriter w,void *ctx){Out o;o.u8(RESP_ERR);o.u8(code);send(o,w,ctx);}
// Apply a changed radio / identity setting through the normal path (saved to the SD, radio reconfigured).
static void apply(Config c,CompanionWriter w,void *ctx){String reason;if(configure(c,reason))ok(w,ctx);else err(ERR_ILLEGAL_ARG,w,ctx);}
// Offline queue for the app (MeshCore's frames: V3 when the app announced protocol >= 3) and the pushes every
// active link sends on its next pump (message waiting, send confirmed).
constexpr int QueueMax=32,PushMax=8,AckMax=8;
struct Queued {uint8_t len=0;uint8_t b[COMPANION_FRAME_MAX];};
static Queued *queue=nullptr;static int queueHead=0,queueCount=0;static uint8_t appVersion=3;
struct Push {uint8_t len=0;uint8_t b[12];};static Push pushes[PushMax];static uint32_t pushSeq=0;
struct Ack {uint32_t ack=0,sentMs=0;};static Ack acks[AckMax];static int nextAck=0;
// Raw RX log (PUSH_CODE_LOG_RX_DATA, MeshCore's logRxRaw: snr*4, rssi, packet bytes). The app finds the repeater
// echoes of its own channel messages in it; without it a sent channel message stays on "Sent". Kept only once an
// app has connected (a ring of the newest LogMax packets, each link sends what it has not sent yet).
constexpr int LogMax=32;struct LogFrame {uint8_t len=0;uint8_t b[COMPANION_FRAME_MAX];};// 32 frames: the pushes of the modules (up to a full frame each) share the ring with the RX log
static LogFrame *logs=nullptr;
// Queue and RX log / push ring: about 11 KB, in PSRAM when there is some (internal RAM otherwise), allocated on first use
// (an app connecting, or the first message to queue). Each buffer is checked on its own.
template<class T> static T *appAlloc(int n){return static_cast<T*>(device::psramCalloc(n,sizeof(T)));}
static bool queueBuffer(){if(!queue)queue=appAlloc<Queued>(QueueMax);return queue!=nullptr;}
static bool logBuffer(){if(!logs)logs=appAlloc<LogFrame>(LogMax);return logs!=nullptr;}
static uint32_t logSeq=0;static bool appSeen=false;
void logRaw(float snr,float rssi,const uint8_t *raw,int len){if(!appSeen||!logs||len<=0||size_t(len)+3>COMPANION_FRAME_MAX)return;auto &l=logs[logSeq%LogMax];l.b[0]=PUSH_LOG_RX_DATA;l.b[1]=uint8_t(int8_t(snr*4));l.b[2]=uint8_t(int8_t(rssi));memcpy(l.b+3,raw,len);l.len=uint8_t(len+3);logSeq++;}
// Zero-hop control data, anonymous requests and every module push (companion_air.h, companion_remote.h,
// companion_state.h): longer than the 1-byte ones, so they go through the RX log ring, only while an app has connected.
static void pushFrame(const uint8_t *frame,size_t n){if(!appSeen||!logs||!n||n>COMPANION_FRAME_MAX)return;auto &l=logs[logSeq%LogMax];memcpy(l.b,frame,n);l.len=uint8_t(n);logSeq++;}
// The app's pending requests: anonymous and binary (one slot, as MeshCore), login, status, telemetry, path discovery.
static companion_remote::Pending pend;
void controlData(const mesh::Packet *packet,float snr,float rssi){uint8_t f[COMPANION_FRAME_MAX];pushFrame(f,companion_air::controlFrame(packet,snr,rssi,f,sizeof(f)));}
static void push(const uint8_t *frame,size_t n){auto &p=pushes[pushSeq%PushMax];p.len=uint8_t(std::min(n,sizeof(p.b)));memcpy(p.b,frame,p.len);pushSeq++;}
// Any message frame (contact, channel, channel data) waits in the queue; the app is told by a message-waiting push.
static void enqueueFrame(const uint8_t *f,size_t n){if(!n||n>COMPANION_FRAME_MAX||!queueBuffer())return;if(queueCount==QueueMax){queueHead=(queueHead+1)%QueueMax;queueCount--;}auto &q=queue[(queueHead+queueCount)%QueueMax];q.len=uint8_t(n);memcpy(q.b,f,n);queueCount++;uint8_t tickle=PUSH_MSG_WAITING;push(&tickle,1);}
static void enqueue(const Out &o){enqueueFrame(o.b,o.n);}
static void text(Out &o,const char *t){size_t n=strlen(t);o.put(t,std::min(n,sizeof(o.b)-o.n));}
// txtType 0 plain, 1 CLI reply of a server, 2 signed room post (prefix: the author's 4 key bytes).
void queueContact(const ContactInfo &from,mesh::Packet *pkt,uint32_t ts,const char *t,uint8_t txtType,const uint8_t *prefix){uint8_t f[COMPANION_FRAME_MAX];
 enqueueFrame(f,companion_remote::contactMsgFrame(appVersion>=3,pkt,from.id.pub_key,txtType,ts,prefix,prefix?4:0,t,f,sizeof(f)));}
// The app numbers channels 0..n-1 without holes; our mesh slots follow the conversation list, where direct chats leave
// empty slots. App index k is the k-th configured mesh slot.
static int meshSlot(int app){ChannelDetails ch;for(int i=0,k=0;i<MAX_GROUP_CHANNELS;i++)if(chatMesh().getChannel(i,ch)&&ch.name[0]){if(k==app)return i;k++;}return -1;}
static int appSlot(int mesh){ChannelDetails ch;int k=0;for(int i=0;i<mesh&&i<MAX_GROUP_CHANNELS;i++)if(chatMesh().getChannel(i,ch)&&ch.name[0])k++;return k;}
void queueChannel(int idx,mesh::Packet *pkt,uint32_t ts,const char *t){idx=appSlot(idx);Out o;
 if(appVersion>=3){o.u8(RESP_CHANNEL_MSG_RECV_V3);o.u8(uint8_t(int8_t(pkt->getSNR()*4)));o.u8(0);o.u8(0);}else o.u8(RESP_CHANNEL_MSG_RECV);
 o.u8(uint8_t(idx));o.u8(pkt->isRouteFlood()?pkt->path_len:0xFF);o.u8(TXT_PLAIN);o.u32(ts);text(o,t);enqueue(o);}
void ackReceived(const uint8_t *data){for(auto &a:acks)if(a.ack&&!memcmp(data,&a.ack,4)){uint8_t f[9];f[0]=PUSH_SEND_CONFIRMED;memcpy(f+1,data,4);uint32_t trip=millis()-a.sentMs;memcpy(f+5,&trip,4);push(f,9);a.ack=0;}}
static void contactFrame(Out &o,const ContactInfo &c){o.u8(RESP_CONTACT);o.put(c.id.pub_key,PUB_KEY_SIZE);o.u8(c.type);o.u8(c.flags);o.u8(c.out_path_len);o.put(c.out_path,MAX_PATH_SIZE);
 o.text(c.name,32);o.u32(c.last_advert_timestamp);o.i32(c.gps_lat);o.i32(c.gps_lon);o.u32(c.lastmod);}
// Module state: tuning (rx delay base, airtime factor) in the app preferences, the hooks the modules need.
static bool saveTuning(void*,const uint8_t *blob,size_t n){Preferences p;if(!p.begin("mmapp",false))return false;bool saved=p.putBytes("tuning",blob,n)==n;p.end();return saved;}
// A new key is saved over the old one and installed; the shared secrets cached in the contacts belonged to the old key.
static bool installIdentity(void*,const mesh::LocalIdentity &id){
 if(!storage)return false;File out=storage->open("/mattermesh/identity.tmp","w");bool written=out&&id.writeTo(out);if(out){out.flush();out.close();}
 if(!written||!device::sdReplace(*storage,"/mattermesh/identity.tmp","/mattermesh/identity.id","/mattermesh/identity.bak",false)){storage->remove("/mattermesh/identity.tmp");return false;}
 chatMesh().self_id=id;ContactInfo c;for(int i=0;i<chatMesh().getTotalContactSlots();i++)if(chatMesh().getContactByIdx(i,c))if(auto *p=chatMesh().lookupContactByPubKey(c.id.pub_key,PUB_KEY_SIZE))p->shared_secret_valid=false;
 return true;}
// CMD_GET_STATS: what MyMesh reports, from the same sources (the Mesh / Dispatcher counters and the radio driver).
static void statsHook(void*,companion_state::Stats &s){auto &m=chatMesh();
 s.batteryMv=hooks.batteryMv?hooks.batteryMv():0;s.uptimeSecs=millis()/1000;s.errFlags=m.errFlags();s.queueLen=uint8_t(pool.getOutboundTotal());
 s.noiseFloor=int16_t(driver.getNoiseFloor());s.lastRssi=driver.getLastRSSI();s.lastSnr=driver.getLastSNR();s.txAirSecs=m.getTotalAirTime()/1000;s.rxAirSecs=m.getReceiveAirTime()/1000;
 s.recv=driver.getPacketsRecv();s.sent=driver.getPacketsSent();s.sentFlood=m.getNumSentFlood();s.sentDirect=m.getNumSentDirect();s.recvFlood=m.getNumRecvFlood();s.recvDirect=m.getNumRecvDirect();s.recvErrors=driver.getPacketsRecvErrors();}
static float mcuTemperature(void*){
#if MM_EMULATOR
 return NAN;// the PC has no chip sensor
#else
 return temperatureRead();
#endif
}
static companion_state::Module &stateModule(){
 static companion_state::Module m;static bool ready=false;
 if(!ready){ready=true;m.tuning.airtime=9.0f;// the fixed factor this firmware always used (TX <=10% airtime); MeshCore's own default is 1.0
  auto &h=m.hooks;h.stats=statsHook;h.batteryMv=[](void*)->uint16_t{return hooks.batteryMv?hooks.batteryMv():0;};h.mcuTemperature=mcuTemperature;h.saveTuning=saveTuning;
  h.disableTransports=[](void*){if(hooks.disableTransports)hooks.disableTransports();};h.wipe=[](void*)->bool{return hooks.wipe&&hooks.wipe();};
  h.reboot=[](void*){if(hooks.reboot)hooks.reboot();};h.installIdentity=installIdentity;
  h.runCli=[](void*,const char *command,char *reply,size_t cap)->size_t{return hooks.runCli?hooks.runCli(command,reply,cap):0;};
  Preferences p;uint8_t blob[8];if(p.begin("mmapp",true)){if(p.getBytes("tuning",blob,sizeof(blob))==sizeof(blob))m.tuning.fromBlob(blob,sizeof(blob));p.end();}}
 if(meshPtr)m.self=&meshPtr->self_id;return m;}
float airtimeFactor(){return stateModule().tuning.airtimeFactor();}
int rxDelay(float score,uint32_t airTime){return stateModule().tuning.calcRxDelay(score,airTime);}
// App flood scope (override key, unscoped, default scope): the default is kept in the app preferences, the override
// and unscoped are per connection and start cleared.
static companion_packets::ScopeState scope;
static companion_packets::ScopeState &scopeState(){
 static bool loaded=false;if(!loaded){loaded=true;Preferences p;uint8_t blob[companion_packets::ScopeState::BLOB_SIZE];
  if(p.begin("mmapp",true)){if(p.getBytes("scope",blob,sizeof(blob))==sizeof(blob)&&scope.deserialize(blob,sizeof(blob))){memset(scope.overrideKey,0,sizeof(scope.overrideKey));scope.unscoped=false;}p.end();}}
 return scope;}
static bool saveScope(){auto &sc=scopeState();if(!sc.dirty)return true;uint8_t blob[companion_packets::ScopeState::BLOB_SIZE];sc.serialize(blob);
 Preferences p;if(!p.begin("mmapp",false))return false;bool saved=p.putBytes("scope",blob,sizeof(blob))==sizeof(blob);p.end();if(saved)sc.dirty=false;return saved;}
bool scopeKey(TransportKey &out,const TransportKey *deviceRegion){return scopeState().effective(out,deviceRegion);}
static companion_packets::Signer signer;// one signing buffer for all links (an app signs one thing at a time)
// The packets module and the app number channels the same way: app index k is the k-th configured mesh slot.
struct AppChannels {ChatMesh &mesh;int received;
 bool getChannel(int app,ChannelDetails &d){int slot=meshSlot(app);return slot>=0&&mesh.getChannel(slot,d);}
 bool sendGroupData(mesh::GroupChannel &c,uint8_t *path,uint8_t pathLen,uint16_t type,const uint8_t *data,int n){return mesh.sendGroupData(c,path,pathLen,type,data,n);}
 int findChannelIdx(const mesh::GroupChannel&){return appSlot(received);}};
void channelData(int meshIdx,const mesh::GroupChannel &ch,mesh::Packet *packet,uint16_t type,const uint8_t *data,size_t len){
 AppChannels c{chatMesh(),meshIdx};uint8_t f[COMPANION_FRAME_MAX];enqueueFrame(f,companion_packets::channelDataFrame(c,ch,packet,type,data,len,f,sizeof(f)));}
void rawData(const mesh::Packet *packet,float snr,float rssi){uint8_t f[COMPANION_FRAME_MAX];pushFrame(f,companion_packets::rawDataFrame(snr,rssi,packet,f,sizeof(f)));}
void contactResponse(const ContactInfo &from,const uint8_t *data,uint8_t len){uint8_t f[COMPANION_FRAME_MAX];pushFrame(f,companion_remote::responseFrame(chatMesh(),pend,from,data,len,f,sizeof(f)));}
bool pathRecv(const ContactInfo &from,const uint8_t *in,uint8_t inLen,const uint8_t *out,uint8_t outLen,uint8_t type,const uint8_t *extra,uint8_t len){
 uint8_t f[COMPANION_FRAME_MAX];size_t n=0;if(!companion_remote::pathResponse(pend,from,in,inLen,out,outLen,type,extra,len,f,sizeof(f),n))return false;pushFrame(f,n);return true;}
void traceRecv(const mesh::Packet *packet,uint32_t tag,uint32_t auth,uint8_t flags,const uint8_t *snrs,const uint8_t *hashes,uint8_t len){
 uint8_t f[COMPANION_FRAME_MAX];pushFrame(f,companion_remote::traceFrame(packet,tag,auth,flags,snrs,hashes,len,f,sizeof(f)));}
// Pushes for contact changes. Every advert also feeds the table behind CMD_GET_ADVERT_PATH.
void discovered(const ContactInfo &c,bool isNew,uint8_t pathLen,const uint8_t *path){uint8_t f[COMPANION_FRAME_MAX];
 pushFrame(f,isNew?companion_state::contactFrame(companion_state::PUSH_NEW_ADVERT,c,f):companion_state::advertFrame(c.id.pub_key,f));
 stateModule().adverts.record(c.id.pub_key,c.name,clock.getCurrentTime(),path,pathLen);}
void pathUpdated(const ContactInfo &c){uint8_t f[COMPANION_FRAME_MAX];pushFrame(f,companion_state::pathUpdatedFrame(c.id.pub_key,f));}
void contactDeleted(const uint8_t *pub){uint8_t f[COMPANION_FRAME_MAX];pushFrame(f,companion_state::contactDeletedFrame(pub,f));}
void contactsFull(){uint8_t f[COMPANION_FRAME_MAX];pushFrame(f,companion_state::contactsFullFrame(f));}
// A telemetry request from another node: answered as the app's telemetry modes allow (the byte of CMD_SET_OTHER_PARAMS).
uint8_t contactRequest(const ContactInfo &from,uint32_t ts,const uint8_t *data,uint8_t len,uint8_t *reply){
 if(!len||data[0]!=companion_remote::REQ_GET_TELEMETRY)return 0;
 return uint8_t(companion_state::contactTelemetry<CayenneLPP>(stateModule(),prefs().telemetry,from.flags,len>1?data[1]:0,ts,reply));}
// MyMesh::updateContactFromFrame: key, type, flags, path, name, last advert, then optional lat/lon and lastmod.
static void contactFromFrame(ContactInfo &c,uint32_t &lastmod,const uint8_t *in,size_t len){
 size_t i=1;memcpy(c.id.pub_key,in+i,PUB_KEY_SIZE);i+=PUB_KEY_SIZE;c.type=in[i++];c.flags=in[i++];c.out_path_len=in[i++];
 if(len<i+MAX_PATH_SIZE+32+4)return;memcpy(c.out_path,in+i,MAX_PATH_SIZE);i+=MAX_PATH_SIZE;memcpy(c.name,in+i,32);c.name[31]=0;i+=32;
 memcpy(&c.last_advert_timestamp,in+i,4);i+=4;if(len>=i+8){memcpy(&c.gps_lat,in+i,4);memcpy(&c.gps_lon,in+i+4,4);i+=8;if(len>=i+4)memcpy(&lastmod,in+i,4);}}
static void contactsChanged(){contactsDirty=true;saveAt=millis();}
}
void companionHooks(const CompanionHooks &h){companion::hooks=h;}
void companionFrame(CompanionSession &s,const uint8_t *in,size_t len,CompanionWriter w,void *ctx){
 using namespace companion;if(!len)return;
 if(!meshPtr){err(ERR_BAD_STATE,w,ctx);return;}
 const uint8_t cmd=in[0];auto cfg=snapshot().config;Out o;
 auto read32=[&](size_t at){uint32_t v=0;if(at+4<=len)memcpy(&v,in+at,4);return v;};
 auto reply=[&](const uint8_t *f,size_t n){if(w)w(ctx,f,n);};// the modules answer through this
 if(state.suspended&&(cmd==CMD_SEND_TXT_MSG||cmd==CMD_SEND_CHANNEL_TXT_MSG||cmd==CMD_SEND_SELF_ADVERT||cmd==CMD_SHARE_CONTACT||cmd==companion_air::CMD_SEND_CONTROL_DATA||cmd==companion_air::CMD_SEND_ANON_REQ||companion_remote::transmits(cmd,len)||cmd==companion_packets::CMD_SEND_RAW_DATA||cmd==companion_packets::CMD_SEND_RAW_PACKET||cmd==companion_packets::CMD_SEND_CHANNEL_DATA)){err(ERR_BAD_STATE,w,ctx);return;}
 switch(cmd){
 case CMD_DEVICE_QUERY:{
  appVersion=len>=2?in[1]:0;s.active=true;s.pushSeq=pushSeq;s.logSeq=logSeq;appSeen=true;queueBuffer();logBuffer();// pushes from before this connection are not replayed
  o.u8(RESP_DEVICE_INFO);o.u8(ProtocolVersion);o.u8(uint8_t(std::min(255,MAX_CONTACTS/2)));o.u8(MAX_GROUP_CHANNELS);o.u32(hooks.pin?hooks.pin():0);
  o.text(__DATE__,12);o.text("LilyGO " MM_DEVICE_MODEL " / MatterMesh OS",40);o.text("MatterMesh " MM_BUILD_VERSION,20);o.u8(cfg.repeat?1:0);o.u8(uint8_t(cfg.pathBytes?cfg.pathBytes-1:0));
  send(o,w,ctx);break;}
 case CMD_APP_START:{
  s.listing=false;int32_t lat=0,lon=0;if(hooks.location)hooks.location(lat,lon);
  o.u8(RESP_SELF_INFO);o.u8(ADV_TYPE_CHAT);o.u8(uint8_t(cfg.power));o.u8(uint8_t(MAX_TX_POWER));o.put(chatMesh().self_id.pub_key,PUB_KEY_SIZE);o.i32(lat);o.i32(lon);
  {auto &p=prefs();o.u8(p.multiAcks);o.u8(p.locationPolicy);o.u8(p.telemetry);o.u8(p.manualAdd);}// multi acks, advert location policy, telemetry modes, manual add contacts
  o.u32(uint32_t(lroundf(cfg.frequency*1000)));o.u32(uint32_t(lroundf(cfg.bandwidth*1000)));o.u8(cfg.sf);o.u8(cfg.cr);o.put(cfg.name.c_str(),cfg.name.length());
  send(o,w,ctx);break;}
 case CMD_GET_CONTACTS:{
  // A new request restarts a running listing (MeshCore answers BAD_STATE): with ~1000 contacts the first listing
  // after connecting lasts long, and the app's refresh after a delete would otherwise wait on a spinner.
  s.since=len>=5?read32(1):0;s.mostRecent=0;s.next=MAX_ANON_CONTACTS;s.listing=true;
  o.u8(RESP_CONTACTS_START);o.u32(uint32_t(chatMesh().getNumContacts()));send(o,w,ctx);break;}
 case CMD_GET_DEVICE_TIME:o.u8(RESP_CURR_TIME);o.u32(clock.getCurrentTime());send(o,w,ctx);break;
 case CMD_SET_DEVICE_TIME:{if(len<5){err(ERR_ILLEGAL_ARG,w,ctx);break;}String reason;if(syncClock(read32(1),reason))ok(w,ctx);else err(ERR_ILLEGAL_ARG,w,ctx);break;}
 case CMD_SEND_SELF_ADVERT:{String reason;if(advertise(reason))ok(w,ctx);else err(ERR_BAD_STATE,w,ctx);break;}
 case CMD_SET_ADVERT_NAME:{if(len<2){err(ERR_ILLEGAL_ARG,w,ctx);break;}String name;for(size_t i=1;i<len&&i<=31;i++)name+=char(in[i]);name.trim();cfg.name=name;apply(cfg,w,ctx);break;}
 case CMD_SET_RADIO_PARAMS:{
  if(len<11){err(ERR_ILLEGAL_ARG,w,ctx);break;}uint32_t f=read32(1),bw=read32(5);uint8_t sf=in[9],cr=in[10],repeat=len>11?in[11]:0;
  if(repeat&&!repeat_policy::validFreq(f)){err(ERR_ILLEGAL_ARG,w,ctx);break;}// client repeat only at an allowed frequency; a frame without the byte clears it (MyMesh)
  cfg.frequency=f/1000.0f;cfg.bandwidth=bw/1000.0f;cfg.sf=sf;cfg.cr=cr;cfg.repeat=repeat!=0;apply(cfg,w,ctx);break;}
 case CMD_GET_ALLOWED_REPEAT_FREQ:o.n=repeat_policy::allowedFrame(o.b,sizeof(o.b));send(o,w,ctx);break;
 case CMD_SET_RADIO_TX_POWER:{if(len<2){err(ERR_ILLEGAL_ARG,w,ctx);break;}cfg.power=int8_t(in[1]);apply(cfg,w,ctx);break;}
 case CMD_SET_PATH_HASH_MODE:{if(len<3||in[1]!=0||in[2]>=3){err(ERR_ILLEGAL_ARG,w,ctx);break;}cfg.pathBytes=uint8_t(in[2]+1);apply(cfg,w,ctx);break;}
 case CMD_ADD_UPDATE_CONTACT:{
  if(len<1+PUB_KEY_SIZE+3){err(ERR_ILLEGAL_ARG,w,ctx);break;}
  uint32_t lastmod=clock.getCurrentTime();ContactInfo *known=chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE);
  if(known){contactFromFrame(*known,lastmod,in,len);known->lastmod=lastmod;contactsChanged();ok(w,ctx);break;}
  ContactInfo c={};contactFromFrame(c,lastmod,in,len);c.lastmod=lastmod;c.sync_since=0;
  if(chatMesh().addContact(c)){markInBook(c.id.pub_key,true);contactsChanged();ok(w,ctx);}else err(ERR_TABLE_FULL,w,ctx);break;}
 case CMD_RESET_PATH:{ContactInfo *c=len>=1+PUB_KEY_SIZE?chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE):nullptr;
  if(!c){err(ERR_NOT_FOUND,w,ctx);break;}c->out_path_len=OUT_PATH_UNKNOWN;dmRouteStore(c->id.pub_key,DmRouteMode::Auto,0,nullptr);/* the app's reset also ends Flood only / a manual path */contactsChanged();ok(w,ctx);break;}
 case CMD_REMOVE_CONTACT:{ContactInfo *c=len>=1+PUB_KEY_SIZE?chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE):nullptr;
  uint8_t pub[PUB_KEY_SIZE];if(c)memcpy(pub,c->id.pub_key,PUB_KEY_SIZE);if(c&&chatMesh().removeContact(*c)){dmRouteStore(pub,DmRouteMode::Auto,0,nullptr);markInBook(pub,false);contactsChanged();ok(w,ctx);}else err(ERR_NOT_FOUND,w,ctx);break;}
 case CMD_SHARE_CONTACT:{ContactInfo *c=len>=1+PUB_KEY_SIZE?chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE):nullptr;
  if(!c)err(ERR_NOT_FOUND,w,ctx);else if(chatMesh().shareContactZeroHop(*c))ok(w,ctx);else err(ERR_TABLE_FULL,w,ctx);break;}
 case CMD_GET_CONTACT_BY_KEY:{ContactInfo *c=len>=1+PUB_KEY_SIZE?chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE):nullptr;
  if(!c){err(ERR_NOT_FOUND,w,ctx);break;}contactFrame(o,*c);send(o,w,ctx);break;}
 case CMD_EXPORT_CONTACT:{
  uint8_t raw[MAX_TRANS_UNIT+1];uint8_t n=0;
  if(len<1+PUB_KEY_SIZE){// the app's "share my contact": our own advert, flood route as MyMesh
   int32_t lat=0,lon=0;if(hooks.location)hooks.location(lat,lon);
   mesh::Packet *pkt=lat||lon?chatMesh().createSelfAdvert(cfg.name.c_str(),lat/1e6,lon/1e6):chatMesh().createSelfAdvert(cfg.name.c_str());
   if(pkt){pkt->header|=ROUTE_TYPE_FLOOD;n=pkt->writeTo(raw);chatMesh().releasePacket(pkt);}
  }else{ContactInfo *c=chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE);if(c)n=chatMesh().exportContact(*c,raw);}
  if(!n||size_t(n)+1>sizeof(o.b)){err(len<1+PUB_KEY_SIZE?ERR_TABLE_FULL:ERR_NOT_FOUND,w,ctx);break;}
  o.u8(RESP_EXPORT_CONTACT);o.put(raw,n);send(o,w,ctx);break;}
 case CMD_IMPORT_CONTACT:{
  if(len>2+PUB_KEY_SIZE+64&&len<=256&&chatMesh().importContact(in+1,uint8_t(len-1))){contactImporting=true;contactsChanged();ok(w,ctx);}else err(ERR_ILLEGAL_ARG,w,ctx);break;}
 case CMD_SYNC_NEXT_MESSAGE:
  if(queueCount){auto &q=queue[queueHead];queueHead=(queueHead+1)%QueueMax;queueCount--;if(w)w(ctx,q.b,q.len);}else{o.u8(RESP_NO_MORE_MESSAGES);send(o,w,ctx);}break;
 case CMD_SEND_TXT_MSG:{
  if(len<14){err(ERR_ILLEGAL_ARG,w,ctx);break;}uint8_t type=in[1],attempt=in[2];uint32_t ts=read32(3);ContactInfo *to=chatMesh().lookupContactByPubKey(in+7,6);
  if(!to){err(ERR_NOT_FOUND,w,ctx);break;}
  if(type==TXT_TYPE_CLI_DATA||type==TXT_TYPE_CLI_COMMAND){o.n=companion_remote::sendCommand(chatMesh(),in,len,o.b);send(o,w,ctx);break;}// CLI text to a server, no ack
  if(type!=TXT_PLAIN){err(ERR_UNSUPPORTED,w,ctx);break;}// signed posts are not sent by an app, MeshCore answers the same
  char body[COMPANION_FRAME_MAX];size_t n=std::min(len-13,sizeof(body)-1);memcpy(body,in+13,n);body[n]=0;uint32_t ack=0,timeout=0;
  dmRouteApply(*to);/* the T-Deck route (Flood only / set by hand) also for the app's sends */int result=chatMesh().sendMessage(*to,ts,attempt,body,ack,timeout);if(result==MSG_SEND_FAILED){err(ERR_TABLE_FULL,w,ctx);break;}
  if(ack){acks[nextAck].ack=ack;acks[nextAck].sentMs=millis();nextAck=(nextAck+1)%AckMax;}
  o.u8(RESP_SENT);o.u8(result==MSG_SEND_SENT_FLOOD?1:0);o.u32(ack);o.u32(timeout);send(o,w,ctx);break;}
 case CMD_SEND_CHANNEL_TXT_MSG:{
  if(len<8){err(ERR_ILLEGAL_ARG,w,ctx);break;}uint8_t type=in[1],idx=in[2];uint32_t ts=read32(3);if(type!=TXT_PLAIN){err(ERR_UNSUPPORTED,w,ctx);break;}
  ChannelDetails ch;int slot=meshSlot(idx);if(slot<0||!chatMesh().getChannel(slot,ch)||!ch.name[0]){err(ERR_NOT_FOUND,w,ctx);break;}
  if(chatMesh().sendGroupMessage(ts,ch.channel,cfg.name.c_str(),reinterpret_cast<const char*>(in+7),int(len-7)))ok(w,ctx);else err(ERR_NOT_FOUND,w,ctx);break;}
 case CMD_GET_BATT_AND_STORAGE:o.u8(RESP_BATT_AND_STORAGE);o.u16(hooks.batteryMv?hooks.batteryMv():0);o.u32(hooks.storageUsedKb?hooks.storageUsedKb():0);o.u32(hooks.storageTotalKb?hooks.storageTotalKb():0);send(o,w,ctx);break;
 case CMD_GET_CHANNEL:{
  if(len<2||in[1]>=MAX_GROUP_CHANNELS){err(ERR_NOT_FOUND,w,ctx);break;}ChannelDetails ch={};int slot=meshSlot(in[1]);if(slot>=0)chatMesh().getChannel(slot,ch);
  o.u8(RESP_CHANNEL_INFO);o.u8(in[1]);o.text(ch.name,32);o.put(ch.channel.secret,16);send(o,w,ctx);break;}/* unused app slots read as empty, as in MeshCore */
 case CMD_SET_CHANNEL:{
  if(len>=2+32+32){err(ERR_UNSUPPORTED,w,ctx);break;}// 256-bit secrets: not in MeshCore either
  if(len<2+32+16||!hooks.setChannel){err(ERR_ILLEGAL_ARG,w,ctx);break;}
  char name[33]={};memcpy(name,in+2,32);int slot=meshSlot(in[1]);uint8_t code=hooks.setChannel(slot<0?0xFF:uint8_t(slot),name,in+2+32);if(code)err(code,w,ctx);else ok(w,ctx);break;}
 case CMD_REBOOT:if(len>=7&&!memcmp(in+1,"reboot",6)&&hooks.reboot){ok(w,ctx);hooks.reboot();}else err(ERR_ILLEGAL_ARG,w,ctx);break;
 case CMD_SET_ADVERT_LATLON:{int32_t lat=int32_t(read32(1)),lon=int32_t(read32(5));
  if(len<9||lat<-90000000||lat>90000000||lon<-180000000||lon>180000000)err(ERR_ILLEGAL_ARG,w,ctx);
  else if((!lat&&!lon)||(hooks.setLocation&&hooks.setLocation(lat,lon)))ok(w,ctx);/* 0,0: the app's "no position", the saved one stays */else err(ERR_ILLEGAL_ARG,w,ctx);break;}
 case CMD_SET_OTHER_PARAMS:{if(len<2){err(ERR_ILLEGAL_ARG,w,ctx);break;}auto p=prefs();p.manualAdd=in[1];if(len>=3)p.telemetry=in[2]&0x3F;/* base, location, environment: 2 bits each */if(len>=4)p.locationPolicy=in[3];if(len>=5)p.multiAcks=in[4];
  if(savePrefs(p))ok(w,ctx);else err(ERR_BAD_STATE,w,ctx);break;}
 case CMD_SET_AUTOADD_CONFIG:{if(len<2){err(ERR_ILLEGAL_ARG,w,ctx);break;}auto p=prefs();p.autoadd=in[1];if(len>=3)p.autoaddHops=std::min<uint8_t>(in[2],64);
  if(savePrefs(p))ok(w,ctx);else err(ERR_BAD_STATE,w,ctx);break;}
 case CMD_GET_AUTOADD_CONFIG:o.u8(RESP_AUTOADD_CONFIG);o.u8(prefs().autoadd);o.u8(prefs().autoaddHops);send(o,w,ctx);break;
 case CMD_SET_DEVICE_PIN:{uint32_t pin=read32(1);if(len<5||(pin&&(pin<100000||pin>999999))||!hooks.setPin||!hooks.setPin(pin))err(ERR_ILLEGAL_ARG,w,ctx);else ok(w,ctx);break;}
 case companion_packets::CMD_SET_FLOOD_SCOPE_KEY:o.n=scopeState().setKey(in,len,o.b);send(o,w,ctx);break;
 case companion_packets::CMD_SET_DEFAULT_FLOOD_SCOPE:o.n=scopeState().setDefault(in,len,o.b);if(scopeState().dirty&&!saveScope())err(ERR_FILE_IO,w,ctx);else send(o,w,ctx);break;
 case companion_packets::CMD_GET_DEFAULT_FLOOD_SCOPE:o.n=scopeState().getDefault(o.b);send(o,w,ctx);break;
 case companion_packets::CMD_SIGN_START:o.n=companion_packets::signStart(signer,o.b);send(o,w,ctx);break;
 case companion_packets::CMD_SIGN_DATA:o.n=companion_packets::signData(signer,in,len,o.b);send(o,w,ctx);break;
 case companion_packets::CMD_SIGN_FINISH:o.n=companion_packets::signFinish(signer,chatMesh().self_id,o.b);send(o,w,ctx);break;
 case companion_packets::CMD_SEND_RAW_DATA:o.n=companion_packets::sendRawData(chatMesh(),in,len,o.b);send(o,w,ctx);break;
 case companion_packets::CMD_SEND_RAW_PACKET:o.n=companion_packets::sendRawPacket(chatMesh(),in,len,o.b);send(o,w,ctx);break;
 case companion_packets::CMD_SEND_CHANNEL_DATA:{AppChannels channels{chatMesh(),-1};o.n=companion_packets::sendChannelData(channels,in,len,o.b);send(o,w,ctx);break;}
 case companion_remote::CMD_SEND_LOGIN:case companion_remote::CMD_SEND_STATUS_REQ:case companion_remote::CMD_HAS_CONNECTION:case companion_remote::CMD_LOGOUT:
 case companion_remote::CMD_SEND_TRACE_PATH:case companion_remote::CMD_SEND_BINARY_REQ:case companion_remote::CMD_SEND_PATH_DISCOVERY_REQ:
  o.n=companion_remote::command(chatMesh(),pend,in,len,o.b);send(o,w,ctx);break;
 case companion_remote::CMD_SEND_TELEMETRY_REQ:// the 4 byte form is this device's own telemetry, the long one a request to a contact
  if(!companion_state::handle<CayenneLPP>(stateModule(),in,len,reply)){o.n=companion_remote::command(chatMesh(),pend,in,len,o.b);send(o,w,ctx);}break;
 case companion_state::CMD_SET_TUNING_PARAMS:case companion_state::CMD_GET_TUNING_PARAMS:case companion_state::CMD_GET_CUSTOM_VARS:case companion_state::CMD_SET_CUSTOM_VAR:case companion_state::CMD_GET_ADVERT_PATH:
 case companion_state::CMD_EXPORT_PRIVATE_KEY:case companion_state::CMD_IMPORT_PRIVATE_KEY:case companion_state::CMD_FACTORY_RESET:case companion_state::CMD_GET_STATS:case companion_state::CMD_RUN_CLI_COMMAND:
  companion_state::handle<CayenneLPP>(stateModule(),in,len,reply);break;// answers every one of these opcodes itself
 case companion_air::CMD_SEND_CONTROL_DATA:o.n=companion_air::sendControl(chatMesh(),in,len,o.b);send(o,w,ctx);break;
 case companion_air::CMD_SEND_ANON_REQ:o.n=companion_air::sendAnon(chatMesh(),clock.getCurrentTime(),in,len,pend.req,o.b);if(o.b[0]==companion_air::RESP_SENT)pend.anonSent();send(o,w,ctx);break;
 default:err(ERR_UNSUPPORTED,w,ctx);break;
 }
}
// One contact per call while a GET_CONTACTS listing runs (the transport calls it when it can send again).
bool companionPump(CompanionSession &s,CompanionWriter w,void *ctx){
 using namespace companion;
 // Pushes first (message waiting, send confirmed), one per call; a link that fell behind skips to the last PushMax.
 if(s.active&&s.pushSeq!=pushSeq){if(pushSeq-s.pushSeq>PushMax)s.pushSeq=pushSeq-PushMax;auto &p=pushes[s.pushSeq%PushMax];s.pushSeq++;if(w)w(ctx,p.b,p.len);return true;}
 if(s.active&&logs&&s.logSeq!=logSeq){if(logSeq-s.logSeq>LogMax)s.logSeq=logSeq-LogMax;auto &l=logs[s.logSeq%LogMax];s.logSeq++;if(w)w(ctx,l.b,l.len);return true;}
 if(!s.listing||!meshPtr)return false;
 ContactInfo c;while(chatMesh().getContactByIdx(uint32_t(s.next),c)){s.next++;if(!s.since||c.lastmod>s.since){Out o;/* contacts loaded from the SD have lastmod 0: a full listing sends them all */contactFrame(o,c);send(o,w,ctx);if(c.lastmod>s.mostRecent)s.mostRecent=c.lastmod;return true;}}
 Out o;o.u8(RESP_END_OF_CONTACTS);o.u32(s.mostRecent);send(o,w,ctx);s.listing=false;return true;
}
bool contactsAddNew(){return !(companion::prefs().manualAdd&1);}
bool contactsSetAddNew(bool on){auto p=companion::prefs();p.manualAdd=on?0:1;return companion::savePrefs(p);}
bool contactsReplaceOldest(){return companion::prefs().autoadd&1;}// MeshCore AUTO_ADD_OVERWRITE_OLDEST
bool contactsSetReplaceOldest(bool on){auto p=companion::prefs();p.autoadd=on?(p.autoadd|1):(p.autoadd&~1);return companion::savePrefs(p);}
