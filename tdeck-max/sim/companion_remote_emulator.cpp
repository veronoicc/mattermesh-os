// PC emulator for the companion app's remote-node commands (login, status, telemetry, binary and path-discovery
// requests, trace path, CLI, room posts) against MatterMesh OS's code (src/mesh/companion_remote.h).
// Real MeshCore core on both ends over an in-memory radio: the phone runs the same functions the firmware's
// companion.inc / ChatMesh call; servers answer like simple_repeater / simple_room_server (ACL with admin and guest
// passwords, replay checks, status / telemetry / owner requests, CLI, signed room posts); a relay repeater sits
// between the phone and most servers so paths, hop counts and SNR lists are real. The test plays the phone app
// and checks every reply and push frame byte for byte against companion_radio/MyMesh.cpp.
#include "companion_sim.h"
#include "mesh/companion_remote.h"
namespace {
using V=std::vector<uint8_t>;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
V cat(std::initializer_list<V> parts){V r;for(auto &p:parts)r.insert(r.end(),p.begin(),p.end());return r;}
V str(const char *s){return V(s,s+strlen(s));}
V u32(uint32_t v){V r(4);memcpy(r.data(),&v,4);return r;}
uint32_t rd32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}

// Remote node (repeater or room server) with the ACL logic of examples/simple_repeater and simple_room_server.
struct Stats{uint16_t batt=4100,txq=1;int16_t noise=-110,rssi=-70;uint32_t recv=11,sent=12,air=13,up=14,sentFlood=15,sentDirect=16,recvFlood=17,recvDirect=18;uint16_t err=0;int16_t snr=38;uint16_t dupD=1,dupF=2;uint32_t rxAir=19,recvErr=0;};
class Server:public Parts,public mesh::Mesh{
 struct Client{mesh::Identity id;uint8_t secret[PUB_KEY_SIZE]={};uint8_t perms=0;uint32_t lastTs=0;uint8_t outPath[MAX_PATH_SIZE]={};uint8_t outLen=OUT_PATH_UNKNOWN;};
 std::vector<Client> acl;std::vector<int> match;
 Client *find(const mesh::Identity &id){for(auto &c:acl)if(c.id.matches(id))return &c;return nullptr;}
 void respond(const mesh::Packet *in,const mesh::Identity &to,const uint8_t *secret,uint8_t outLen,const uint8_t *outPath,const uint8_t *reply,int rlen){
  if(in->isRouteFlood()){if(auto *p=createPathReturn(to,secret,in->path,in->path_len,PAYLOAD_TYPE_RESPONSE,reply,rlen))sendFlood(p,300);return;}
  if(auto *r=createDatagram(PAYLOAD_TYPE_RESPONSE,to,secret,reply,rlen)){if(outLen!=OUT_PATH_UNKNOWN)sendDirect(r,outPath,outLen,300);else sendFlood(r,300);}}
 void toClient(Client &c,mesh::Packet *p,uint32_t delay){if(!p)return;if(c.outLen!=OUT_PATH_UNKNOWN)sendDirect(p,c.outPath,c.outLen,delay);else sendFlood(p,delay);}
 int handleRequest(Client &c,uint32_t ts,const uint8_t *payload,uint8_t *reply){
  memcpy(reply,&ts,4);
  if(payload[0]==0x01){memcpy(reply+4,&stats,sizeof(stats));return 4+sizeof(stats);}
  if(payload[0]==0x03){const uint8_t lpp[]={1,0x74,0x01,0x9C};memcpy(reply+4,lpp,4);return 8;}// 4.12 V on channel 1
  if(payload[0]==0x07){int n=sprintf((char*)reply+4,"%s\n%s\n%s","MeshSim-1.0",name.c_str(),owner.c_str());return 4+n;}
  return 0;}
public:
 std::string name,owner="the owner",regions="*,IT-VDA,";bool room=false,legacy=false,rejectWith=false;uint8_t keepAlive=0;Stats stats;uint32_t loginTs=0,cliReplyTs=0;
 std::string password="admin",guest="guest";std::vector<std::string> cli;
 Server(uint32_t seed,const char *n,bool isRoom=false):mesh::Mesh(radio,ms,rng,clock,pool,tables){name=n;room=isRoom;rng.g.seed(seed);self_id=mesh::LocalIdentity(&rng);begin();}
 // The room pushes a signed post (txt_type 2) to a logged-in client: {ts}{type<<2}{author 4}{text}.
 void post(const uint8_t *clientKey,const uint8_t *author,const char *text){for(auto &c:acl)if(!memcmp(c.id.pub_key,clientKey,PUB_KEY_SIZE)){
  uint8_t t[200];uint32_t ts=getRTCClock()->getCurrentTimeUnique();memcpy(t,&ts,4);t[4]=2<<2;memcpy(t+5,author,4);memcpy(t+9,text,strlen(text));
  toClient(c,createDatagram(PAYLOAD_TYPE_TXT_MSG,c.id,c.secret,t,9+strlen(text)),0);}}
protected:
 bool allowPacketForward(const mesh::Packet *p)override{return p->getPayloadType()==PAYLOAD_TYPE_TRACE;}// forwards traces (and direct hops), not floods
 int searchPeersByHash(const uint8_t *hash)override{match.clear();for(int i=0;i<int(acl.size());i++)if(acl[i].id.isHashMatch(hash))match.push_back(i);return int(match.size());}
 void getPeerSharedSecret(uint8_t *dest,int idx)override{memcpy(dest,acl[match[idx]].secret,PUB_KEY_SIZE);}
 void onAnonDataRecv(mesh::Packet *p,const uint8_t *secret,const mesh::Identity &sender,uint8_t *data,size_t len)override{
  if(p->getPayloadType()!=PAYLOAD_TYPE_ANON_REQ||len<5)return;uint32_t ts;memcpy(&ts,data,4);data[len]=0;uint8_t reply[MAX_PACKET_PAYLOAD];int rlen=0;
  const char *pw=(const char*)data+(room?8:4);
  if(!room&&data[4]==0x01&&p->isRouteDirect()){// ANON_REQ_TYPE_REGIONS {reply path}: the existing "Discover regions" request
   uint8_t pl=data[5];uint8_t path[MAX_PATH_SIZE];mesh::Packet::writePath(path,data+6,pl);memcpy(reply,&ts,4);uint32_t now=getRTCClock()->getCurrentTime();memcpy(reply+4,&now,4);memcpy(reply+8,regions.data(),regions.size());
   if(auto *r=createDatagram(PAYLOAD_TYPE_RESPONSE,sender,secret,reply,8+regions.size()))sendDirect(r,path,pl,300);return;}
  if(!(pw[0]==0||pw[0]>=' '))return;
  Client *c=find(sender);uint8_t perms=0;
  if(pw[0]==0){if(!c)return;perms=c->perms;}
  else{if(password==pw)perms=3;else if(guest==pw)perms=0;else{
    if(rejectWith){memcpy(reply,&ts,4);reply[4]=1;respond(p,sender,secret,OUT_PATH_UNKNOWN,nullptr,reply,5);}return;}// a real repeater stays silent
   if(!c){acl.emplace_back();c=&acl.back();c->id=sender;memcpy(c->secret,secret,PUB_KEY_SIZE);}
   if(ts<=c->lastTs)return;c->lastTs=ts;c->perms=perms;}
  if(p->isRouteFlood())c->outLen=OUT_PATH_UNKNOWN;
  uint32_t now=getRTCClock()->getCurrentTimeUnique();loginTs=now;memcpy(reply,&now,4);
  if(legacy){reply[4]='O';reply[5]='K';rlen=6;}
  else{reply[4]=0;reply[5]=keepAlive;reply[6]=perms==3?1:0;reply[7]=perms;getRNG()->random(reply+8,4);reply[12]=9;rlen=13;}
  respond(p,sender,secret,c->outLen,c->outPath,reply,rlen);}
 void onPeerDataRecv(mesh::Packet *p,uint8_t type,int idx,const uint8_t *secret,uint8_t *data,size_t len)override{
  Client &c=acl[match[idx]];
  if(type==PAYLOAD_TYPE_REQ&&len>4){uint32_t ts=rd32(data);if(ts<=c.lastTs)return;uint8_t reply[MAX_PACKET_PAYLOAD];int n=handleRequest(c,ts,data+4,reply);if(!n)return;c.lastTs=ts;respond(p,c.id,secret,c.outLen,c.outPath,reply,n);}
  else if(type==PAYLOAD_TYPE_TXT_MSG&&len>5&&c.perms==3){uint32_t ts=rd32(data);uint8_t flags=data[4]>>2;if(!(flags==0||flags==1||flags==3)||ts<c.lastTs)return;c.lastTs=ts;data[len]=0;
   std::string cmd=(const char*)data+5;cli.push_back(cmd);std::string rep=cmd=="ver"?"MeshSim Repeater v1":cmd=="clock"?"12:00 - 1/1/2026 UTC":"Unknown command";
   uint8_t t[166];uint32_t now=getRTCClock()->getCurrentTimeUnique();if(now==ts)now++;cliReplyTs=now;memcpy(t,&now,4);t[4]=1<<2;memcpy(t+5,rep.data(),rep.size());
   toClient(c,createDatagram(PAYLOAD_TYPE_TXT_MSG,c.id,secret,t,5+rep.size()),600);}}
 bool onPeerPathRecv(mesh::Packet*,int idx,const uint8_t*,uint8_t *path,uint8_t plen,uint8_t,uint8_t*,uint8_t)override{Client &c=acl[match[idx]];c.outLen=mesh::Packet::copyPath(c.outPath,path,plen);return false;}
};
// Repeater that forwards everything (the hop between phone and servers).
class Relay:public Parts,public mesh::Mesh{
public:Relay(uint32_t seed):mesh::Mesh(radio,ms,rng,clock,pool,tables){rng.g.seed(seed);self_id=mesh::LocalIdentity(&rng);begin();}
protected:bool allowPacketForward(const mesh::Packet*)override{return true;}};

// The phone's node: what the firmware's ChatMesh does with companion_remote.h.
class Phone:public Parts,public BaseChatMesh{
public:App app;std::vector<V> queue;companion_remote::Pending pending;
 Phone():BaseChatMesh(radio,ms,rng,clock,pool,tables){rng.g.seed(1);self_id=mesh::LocalIdentity(&rng);begin();}
 using BaseChatMesh::startConnection;using BaseChatMesh::stopConnection;using BaseChatMesh::hasConnectionTo;using BaseChatMesh::markConnectionActive;
 uint32_t traceTimeout(int len,uint8_t hops){return calcDirectTimeoutMillisFor(radio.getEstAirtimeFor(len),hops);}
 void frame(const V &in){uint8_t out[256]={};size_t n;
  if(in[0]==2&&in.size()>=14&&(in[1]==1||in[1]==3))n=companion_remote::sendCommand(*this,in.data(),in.size(),out);
  else if(in[0]==companion_air::CMD_SEND_ANON_REQ){n=companion_air::sendAnon(*this,clock.getCurrentTime(),in.data(),in.size(),pending.req,out);if(out[0]==companion_air::RESP_SENT)pending.anonSent();}
  else n=companion_remote::command(*this,pending,in.data(),in.size(),out);
  app.write(out,n);}
protected:
 void onContactResponse(const ContactInfo &c,const uint8_t *data,uint8_t len)override{uint8_t f[256];size_t n=companion_remote::responseFrame(*this,pending,c,data,len,f,sizeof(f));if(n)app.write(f,n);}
 bool onContactPathRecv(ContactInfo &c,uint8_t *in,uint8_t inLen,uint8_t *out,uint8_t outLen,uint8_t type,uint8_t *extra,uint8_t extraLen)override{
  uint8_t f[256];size_t n=0;if(companion_remote::pathResponse(pending,c,in,inLen,out,outLen,type,extra,extraLen,f,sizeof(f),n)){if(n)app.write(f,n);return false;}
  return BaseChatMesh::onContactPathRecv(c,in,inLen,out,outLen,type,extra,extraLen);}
 void onTraceRecv(mesh::Packet *p,uint32_t tag,uint32_t auth,uint8_t flags,const uint8_t *snrs,const uint8_t *hashes,uint8_t len)override{uint8_t f[177];size_t n=companion_remote::traceFrame(p,tag,auth,flags,snrs,hashes,len,f,sizeof(f));if(n)app.write(f,n);}
 void onCommandDataRecv(const ContactInfo &from,mesh::Packet *p,uint32_t ts,const char *text)override{markConnectionActive(from);uint8_t f[176];size_t n=companion_remote::contactMsgFrame(true,p,from.id.pub_key,1,ts,nullptr,0,text,f,sizeof(f));queue.emplace_back(f,f+n);}
 void onSignedMessageRecv(const ContactInfo &from,mesh::Packet *p,uint32_t ts,const uint8_t *prefix,const char *text)override{markConnectionActive(from);uint8_t f[176];size_t n=companion_remote::contactMsgFrame(true,p,from.id.pub_key,2,ts,prefix,4,text,f,sizeof(f));queue.emplace_back(f,f+n);}
 ContactInfo *processAck(const uint8_t *data)override{return checkConnectionsAck(data);}
 void sendFloodScoped(const ContactInfo&,mesh::Packet *p,uint32_t delay=0)override{sendFlood(p,delay);}
 void sendFloodScoped(const mesh::GroupChannel&,mesh::Packet *p,uint32_t delay=0)override{sendFlood(p,delay);}
 float getAirtimeBudgetFactor()const override{return 0;}int calcRxDelay(float,uint32_t)const override{return 0;}bool allowPacketForward(const mesh::Packet*)override{return false;}
 void onDiscoveredContact(ContactInfo&,bool,uint8_t,const uint8_t*)override{}void onContactPathUpdated(const ContactInfo&)override{}
 void onMessageRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*)override{}
 void onCLICommandRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*,char *reply)override{reply[0]=0;}
 void onChannelMessageRecv(const mesh::GroupChannel&,mesh::Packet*,uint32_t,const char*)override{}
 uint32_t calcFloodTimeoutMillisFor(uint32_t)const override{return 3000;}uint32_t calcDirectTimeoutMillisFor(uint32_t,uint8_t)const override{return 3000;}void onSendTimeout()override{}
 uint8_t onContactRequest(const ContactInfo&,uint32_t,const uint8_t*,uint8_t,uint8_t*)override{return 0;}
};
Phone *P;Relay *relay;std::vector<Server*> servers;
void step(int ms){for(int t=0;t<ms;t+=5){simMs+=5;P->loop();relay->loop();for(auto *s:servers)s->loop();}}
V key(const mesh::Mesh *n){return V(n->self_id.pub_key,n->self_id.pub_key+PUB_KEY_SIZE);}
V prefix6(const mesh::Mesh *n){return V(n->self_id.pub_key,n->self_id.pub_key+6);}
const V &last(){return P->app.frames.back();}
std::vector<V> since(size_t from,uint8_t code){std::vector<V> r;for(size_t i=from;i<P->app.frames.size();i++)if(P->app.frames[i][0]==code)r.push_back(P->app.frames[i]);return r;}
void expectErr(int line,const V &frame,uint8_t code){P->frame(frame);if(!(last()==V{1,code})){printf("FAIL line %d: frame %02x len %zu answered %02x %02x (want error %d)\n",line,frame[0],frame.size(),last()[0],last().size()>1?last()[1]:0,code);std::exit(1);}}
#define ERR(...) expectErr(__LINE__,__VA_ARGS__)
// RESP_SENT {6}{flood?}{tag/ack 4}{timeout 4} -> tag
uint32_t sentTag(int flood){const V &r=last();CHECK(r.size()==10&&r[0]==6&&r[1]==flood);CHECK(rd32(&r[6])==3000);return rd32(&r[2]);}
V login(const Server *s,const char *pw){return cat({V{26},key(s),str(pw)});}
void logIn(const Server *s,const char *pw){size_t m=P->app.frames.size();P->frame(login(s,pw));step(4000);CHECK(!since(m,0x85).empty());}
bool zeros(const V &f,size_t from){for(size_t i=from;i<f.size();i++)if(f[i])return false;return true;}
}

int main(){
 setvbuf(stdout,nullptr,_IONBF,0);
 P=new Phone();relay=new Relay(20);auto *rep=new Server(2,"Hilltop");auto *oldSrv=new Server(3,"Legacy KA");auto *okSrv=new Server(4,"OK server");auto *badSrv=new Server(5,"Rejecting");auto *room=new Server(6,"Room",true);
 oldSrv->keepAlive=4;okSrv->legacy=true;badSrv->rejectWith=true;servers={rep,oldSrv,okSrv,badSrv,room};
 link(P->radio,relay->radio);for(auto *s:{rep,oldSrv,okSrv,badSrv})link(relay->radio,s->radio);link(P->radio,room->radio);// room: zero hop; the rest behind the relay
 { std::vector<const mesh::Mesh*> all={P,relay,rep,oldSrv,okSrv,badSrv,room};for(size_t i=0;i<all.size();i++)for(size_t j=i+1;j<all.size();j++)CHECK(all[i]->self_id.pub_key[0]!=all[j]->self_id.pub_key[0]);}// 1-byte path hashes must differ
 for(auto *s:servers){ContactInfo c={};memcpy(c.id.pub_key,s->self_id.pub_key,PUB_KEY_SIZE);strcpy(c.name,s->name.c_str());c.type=s->room?ADV_TYPE_ROOM:ADV_TYPE_REPEATER;c.out_path_len=OUT_PATH_UNKNOWN;c.lastmod=P->clock.getCurrentTime();CHECK(P->addContact(c));}
 const uint8_t rh=relay->self_id.pub_key[0];
 ContactInfo *rc=P->lookupContactByPubKey(rep->self_id.pub_key,PUB_KEY_SIZE);

 // 0. Which commands put a packet on air (the firmware refuses them while the radio is suspended).
 for(uint8_t c:{26,27,36,50,52})CHECK(companion_remote::transmits(c,40));
 CHECK(companion_remote::transmits(39,36)&&!companion_remote::transmits(39,4));// the 4-byte form is the node's own telemetry
 for(uint8_t c:{1,5,28,29,30,56})CHECK(!companion_remote::transmits(c,40));
 printf("ok: transmitting commands listed\n");

 // 1. Frames shorter than MyMesh needs fall through its if-chain to ERR_UNSUPPORTED_CMD (1); unknown contacts answer NOT_FOUND (2).
 V nokey(PUB_KEY_SIZE,0x77),k=key(rep);
 ERR(cat({V{26},V(31,1)}),1);ERR(cat({V{26},nokey,str("pw")}),2);
 ERR(cat({V{27},V(31,1)}),1);ERR(cat({V{27},nokey}),2);
 ERR(cat({V{28},V(31,1)}),1);ERR(cat({V{29},V(31,1)}),1);
 ERR(cat({V{39,0,0},k}),1);ERR(cat({V{39,0,0,0},V(31,1)}),1);ERR(cat({V{39,0,0,0},nokey}),2);ERR(V{39,0,0,0,0,0},1);
 ERR(cat({V{50},k}),1);ERR(cat({V{50},nokey,V{7}}),2);
 ERR(cat({V{52,0},V(31,1)}),1);ERR(cat({V{52,1},k}),1);ERR(V{52},1);ERR(cat({V{52,0},nokey}),2);
 ERR(V{36,1,2,3,4,5,6,7,8,0},1);/* no path */ERR(cat({V{36},V(8,1),V{1},V(179,1)}),1);/* payload too long */ERR(cat({V{36},V(8,1),V{1},V(177,1)}),6);/* 88 hops */
 ERR(cat({V{36},V(8,1),V{1},V(3,1)}),6);/* 3 bytes of 2-byte hashes */ERR(cat({V{36},V(8,1),V{2},V(7,1)}),6);ERR(cat({V{36},V(8,1),V{0},V(65,1)}),6);/* 65 hops */
 ERR(cat({V{2,3,0},u32(1),V(5,1)}),1);/* sendCommand: too short */
 ERR(cat({V{2,3,0},u32(1),V(6,1)}),1);/* 13 bytes: no text byte */
 { V f=cat({V{2,3,0},u32(1),V(nokey.begin(),nokey.begin()+6),str("ver")});uint8_t out[16];CHECK(companion_remote::sendCommand(*P,f.data(),f.size(),out)==2&&out[0]==1&&out[1]==2);}// unknown recipient
 { V f=cat({V{2,3,0},u32(1),V(k.begin(),k.begin()+6),V(161,'a')});uint8_t out[16];CHECK(companion_remote::sendCommand(*P,f.data(),f.size(),out)==2&&out[0]==1&&out[1]==3);}// text over MAX_TEXT_LEN: table full
 { V f=cat({V{2,2,0},u32(1),V(k.begin(),k.begin()+6),str("hi")});uint8_t out[16];CHECK(companion_remote::sendCommand(*P,f.data(),f.size(),out)==2&&out[0]==1&&out[1]==1);}// plain text is not this helper's job
 CHECK(P->app.frames.size()>20);{size_t m=P->app.frames.size();step(500);CHECK(P->app.frames.size()==m);}// nothing went on air
 printf("ok: malformed and unknown-contact frames\n");

 // 2. Login: wrong password (a repeater stays silent), admin, legacy "OK" server, rejecting server, guest.
 { size_t m=P->app.frames.size();P->frame(login(rep,"wrong"));uint32_t tag=sentTag(1);CHECK(!memcmp(&last()[2],rep->self_id.pub_key,4));/* tag = key prefix, as MyMesh */(void)tag;step(4000);CHECK(since(m,0x85).empty()&&since(m,0x86).empty());}
 { size_t m=P->app.frames.size();P->frame(login(rep,"admin"));sentTag(1);step(4000);auto l=since(m,0x85);CHECK(l.size()==1);const V &f=l[0];
   CHECK(f.size()==14&&f[1]==1&&V(f.begin()+2,f.begin()+8)==prefix6(rep));CHECK(rd32(&f[8])==rep->loginTs&&f[12]==3&&f[13]==9);// admin, server timestamp, ACL perms, firmware level
   CHECK(!P->hasConnectionTo(rep->self_id.pub_key));}// keep-alive 0 (current MeshCore servers): no connection
 CHECK(rc->out_path_len==1&&rc->out_path[0]==rh);// the login response carried the path through the relay
 { size_t m=P->app.frames.size();P->frame(login(okSrv,"admin"));sentTag(1);step(4000);auto l=since(m,0x85);CHECK(l.size()==1&&l[0]==cat({V{0x85,0},prefix6(okSrv)}));CHECK(!P->hasConnectionTo(okSrv->self_id.pub_key));}
 { size_t m=P->app.frames.size();P->frame(login(badSrv,"anything"));sentTag(1);step(4000);auto l=since(m,0x86);CHECK(l.size()==1&&l[0]==cat({V{0x86,0},prefix6(badSrv)})&&since(m,0x85).empty());}
 printf("ok: login success / fail pushes\n");

 // 3. Connection tracking: only a server that asks for keep-alive starts one; logout ends it.
 P->frame(cat({V{28},key(oldSrv)}));CHECK(last()==V({1,2}));
 { size_t m=P->app.frames.size();P->frame(login(oldSrv,"admin"));sentTag(1);step(4000);auto l=since(m,0x85);CHECK(l.size()==1&&l[0][1]==1);}
 P->frame(cat({V{28},key(oldSrv)}));CHECK(last()==V{0});
 P->frame(cat({V{28},key(rep)}));CHECK(last()==V({1,2}));
 P->frame(cat({V{29},key(oldSrv)}));CHECK(last()==V{0});P->frame(cat({V{28},key(oldSrv)}));CHECK(last()==V({1,2}));
 P->frame(cat({V{29},key(oldSrv)}));CHECK(last()==V{0});P->frame(cat({V{29},nokey}));CHECK(last()==V{0});// logout is always OK
 printf("ok: has-connection / logout\n");

 // 4. Status request: stats come back after the tag-less matching MyMesh uses (contact key prefix).
 { size_t m=P->app.frames.size();P->frame(cat({V{27},k}));sentTag(0);step(4000);auto l=since(m,0x87);CHECK(l.size()==1);const V &f=l[0];
   CHECK(f.size()>=8+sizeof(Stats)&&f[1]==0&&V(f.begin()+2,f.begin()+8)==prefix6(rep));CHECK(!memcmp(&f[8],&rep->stats,sizeof(Stats))&&zeros(f,8+sizeof(Stats)));}// then AES block padding
 printf("ok: status response\n");

 // 5. Telemetry request (remote form: {39}{3 reserved}{key}).
 { size_t m=P->app.frames.size();P->frame(cat({V{39,0,0,0},k}));sentTag(0);step(4000);auto l=since(m,0x8B);CHECK(l.size()==1);const V &f=l[0];
   CHECK(f.size()>=12&&f[1]==0&&V(f.begin()+2,f.begin()+8)==prefix6(rep)&&V(f.begin()+8,f.begin()+12)==V({1,0x74,0x01,0x9C})&&zeros(f,12));}
 printf("ok: telemetry response\n");

 // 6. Binary request (owner info) and the tag the app matches with.
 const V owner=str("MeshSim-1.0\nHilltop\nthe owner");
 { size_t m=P->app.frames.size();P->frame(cat({V{50},k,V{7}}));uint32_t tag=sentTag(0);step(4000);auto l=since(m,0x8C);CHECK(l.size()==1);const V &f=l[0];
   CHECK(f[1]==0&&rd32(&f[2])==tag&&V(f.begin()+6,f.begin()+6+owner.size())==owner&&zeros(f,6+owner.size()));}
 // Two in flight: MyMesh keeps one pending request, a new send replaces it; the older answer is stale and dropped.
 { size_t m=P->app.frames.size();P->frame(cat({V{50},k,V{7}}));uint32_t a=sentTag(0);P->frame(cat({V{50},k,V{7},V{0}}));uint32_t b=sentTag(0);CHECK(a!=b);step(4000);
   auto l=since(m,0x8C);CHECK(l.size()==1&&rd32(&l[0][2])==b);}
 // Status then telemetry: only the newest request is answered.
 { size_t m=P->app.frames.size();P->frame(cat({V{27},k}));sentTag(0);P->frame(cat({V{39,0,0,0},k}));sentTag(0);step(4000);CHECK(since(m,0x87).empty()&&since(m,0x8B).size()==1);}
 // A response nobody waits for, or with the wrong tag, produces nothing and keeps the pending request.
 { uint8_t f[256];uint8_t resp[20]={};memcpy(resp,"\x01\x02\x03\x04",4);memcpy(resp+4,"payload",7);
   CHECK(companion_remote::responseFrame(*P,P->pending,*rc,resp,16,f,sizeof(f))==0);// nothing pending
   size_t m=P->app.frames.size();P->frame(cat({V{50},k,V{7}}));uint32_t tag=sentTag(0);
   CHECK(companion_remote::responseFrame(*P,P->pending,*rc,resp,16,f,sizeof(f))==0);// stale tag
   uint32_t other=tag+1;memcpy(resp,&other,4);CHECK(companion_remote::responseFrame(*P,P->pending,*rc,resp,16,f,sizeof(f))==0);
   memcpy(resp,&tag,4);CHECK(companion_remote::responseFrame(*P,P->pending,*rc,resp,4,f,sizeof(f))==0);// tag alone, no data
   step(4000);CHECK(since(m,0x8C).size()==1);// the real answer still lands
   memcpy(resp,&tag,4);CHECK(companion_remote::responseFrame(*P,P->pending,*rc,resp,16,f,sizeof(f))==0);}// and only once
 printf("ok: binary response, two in flight, stale and unknown tags\n");

 // 7. The existing anonymous request (companion_air) shares the pending slot with binary requests.
 { size_t m=P->app.frames.size();V req=cat({V{57},k,V{0x01,1,rh}});// regions, reply path = the relay
   P->frame(req);CHECK(last()[0]==6);uint32_t tag=rd32(&last()[2]);CHECK(tag==P->pending.req.tag);
   P->frame(cat({V{39,0,0,0},k}));sentTag(0);CHECK(P->pending.req.tag==0);// a new request supersedes it, as MyMesh's clearPendingReqs
   step(4000);CHECK(since(m,0x8C).empty()&&since(m,0x8B).size()==1);
   m=P->app.frames.size();P->frame(req);tag=rd32(&last()[2]);CHECK(P->pending.telemetry==0&&P->pending.req.tag==tag);// and an anon request clears the others
   step(4000);auto l=since(m,0x8C);CHECK(l.size()==1&&rd32(&l[0][2])==tag&&V(l[0].begin()+10,l[0].begin()+19)==str("*,IT-VDA,"));}
 printf("ok: anonymous request coexists with remote requests\n");

 // 8. Path discovery: a flood telemetry request whose answer carries both paths; no reciprocal path is learned.
 { size_t m=P->app.frames.size();CHECK(rc->out_path_len==1);P->frame(cat({V{52,0},k}));sentTag(1);/* flood although a path is known */CHECK(rc->out_path_len==1&&rc->out_path[0]==rh);// restored
   step(4000);auto l=since(m,0x8D);CHECK(l.size()==1);CHECK(l[0]==cat({V{0x8D,0},prefix6(rep),V{1,rh,1,rh}}));CHECK(since(m,0x8B).empty());}
 rc->out_path_len=OUT_PATH_UNKNOWN;
 { size_t m=P->app.frames.size();P->frame(cat({V{52,0},k}));sentTag(1);step(4000);auto l=since(m,0x8D);CHECK(l.size()==1&&l[0]==cat({V{0x8D,0},prefix6(rep),V{1,rh,1,rh}}));CHECK(rc->out_path_len==OUT_PATH_UNKNOWN);}// MyMesh returns false: path not stored
 rc->out_path_len=1;rc->out_path[0]=rh;
 { size_t m=P->app.frames.size();P->frame(cat({V{50},k,V{7}}));step(4000);CHECK(since(m,0x8C).size()==1&&since(m,0x8D).empty());}// a plain request after discovery is a normal response
 { // a path return answering something else keeps the pending discovery and falls through to the base class
   size_t m=P->app.frames.size();P->frame(cat({V{52,0},k}));uint32_t tag=sentTag(1);uint8_t f[256];size_t n=99;uint8_t path[1]={rh};uint8_t extra[16]={};uint32_t wrong=tag+1;memcpy(extra,&wrong,4);
   CHECK(!companion_remote::pathResponse(P->pending,*rc,path,1,path,1,PAYLOAD_TYPE_RESPONSE,extra,16,f,sizeof(f),n)&&n==0&&P->pending.discovery==tag);
   memcpy(extra,&tag,4);CHECK(!companion_remote::pathResponse(P->pending,*rc,path,1,path,1,PAYLOAD_TYPE_ACK,extra,16,f,sizeof(f),n)&&P->pending.discovery==tag);// not a response
   CHECK(companion_remote::pathResponse(P->pending,*rc,path,1,path,1,PAYLOAD_TYPE_RESPONSE,extra,16,f,sizeof(f),n)&&n==8+1+1+1+1&&P->pending.discovery==0);
   CHECK(!companion_remote::pathResponse(P->pending,*rc,path,1,path,1,PAYLOAD_TYPE_RESPONSE,extra,16,f,sizeof(f),n));// answered once
   step(4000);CHECK(since(m,0x8D).empty());/* the real answer finds the slot consumed: no push */}
 printf("ok: path discovery response\n");

 // 9. Trace path: zero hop (the relay only), and a round trip relay > server > relay.
 { size_t m=P->app.frames.size();V t=cat({V{36},u32(0x11223344),u32(0x55667788),V{0},V{rh}});P->frame(t);CHECK(last()==cat({V{6,0},u32(0x11223344),u32(3000)}));step(2000);auto l=since(m,0x89);CHECK(l.size()==1);
   CHECK(l[0]==cat({V{0x89,0,1,0},u32(0x11223344),u32(0x55667788),V{rh},V{38},V{38}}));}// hop count 1, hash, SNR to the relay, SNR back to the phone
 { size_t m=P->app.frames.size();V t=cat({V{36},u32(0xAABBCCDD),u32(1),V{0},V{rh,rep->self_id.pub_key[0],rh}});P->frame(t);CHECK(rd32(&last()[2])==0xAABBCCDD);step(4000);auto l=since(m,0x89);CHECK(l.size()==1);
   CHECK(l[0]==cat({V{0x89,0,3,0},u32(0xAABBCCDD),u32(1),V{rh,rep->self_id.pub_key[0],rh},V{38,38,38},V{38}}));}
 { // 2-byte hashes (flags bit 0): hop count is half the byte length
   size_t m=P->app.frames.size();V h={rh,relay->self_id.pub_key[1]};P->frame(cat({V{36},u32(5),u32(6),V{1},h}));CHECK(last()[0]==6);step(2000);auto l=since(m,0x89);CHECK(l.size()==1&&l[0]==cat({V{0x89,0,2,1},u32(5),u32(6),h,V{38},V{38}}));}
 { uint8_t f[177];uint8_t snrs[200]={},hashes[200]={};mesh::Packet pk;pk._snr=1;CHECK(companion_remote::traceFrame(&pk,1,2,3,snrs,hashes,160,f,sizeof(f))==0);// 12+160+20+1 > 177
   CHECK(companion_remote::traceFrame(&pk,1,2,0,snrs,hashes,40,f,sizeof(f))==13+40+40);}
 printf("ok: trace path round trips and 0x89 push\n");

 // 10. CLI over txt_type 3 (command) and 1 (data), replies delivered as queued message frames, and non-admin ignored.
 { size_t m=P->app.frames.size();V f=cat({V{2,3,0},u32(0x99999999),V(k.begin(),k.begin()+6),str("ver")});P->frame(f);CHECK(last()==cat({V{6,0},u32(0),u32(3000)}));// direct, no ack expected
   step(4000);CHECK(rep->cli==std::vector<std::string>{"ver"});CHECK(P->queue.size()==1);const V &q=P->queue[0];
   CHECK(q==cat({V{16,38,0,0},prefix6(rep),V{0xFF,1},u32(rep->cliReplyTs),str("MeshSim Repeater v1")}));(void)m;}// V3 frame: SNR, prefix, direct => 0xFF, txt_type 1, server time
 { V f=cat({V{2,1,0},u32(0),V(k.begin(),k.begin()+6),str("clock")});P->frame(f);CHECK(last()[0]==6);step(4000);CHECK(P->queue.size()==2&&P->queue[1]==cat({V{16,38,0,0},prefix6(rep),V{0xFF,1},u32(rep->cliReplyTs),str("12:00 - 1/1/2026 UTC")}));}
 { // the node's own clock is used, not the app's timestamp: a second command with an old app time is not a replay
   V f=cat({V{2,3,0},u32(1),V(k.begin(),k.begin()+6),str("nope")});P->frame(f);step(4000);CHECK(rep->cli.size()==3&&rep->cli[2]=="nope"&&P->queue.size()==3);}
 { size_t m=P->app.frames.size();P->frame(login(rep,"guest"));step(4000);auto l=since(m,0x85);CHECK(l.size()==1&&l[0][1]==0&&l[0][12]==0);// guest: not admin, ACL perms 0
   P->frame(cat({V{2,3,0},u32(0),V(k.begin(),k.begin()+6),str("ver")}));step(4000);CHECK(rep->cli.size()==3&&P->queue.size()==3);}// a guest's CLI command is ignored by the server
 printf("ok: CLI command and data, replies queued\n");

 // 11. Signed plain post from a room server, with the author's 4-byte prefix.
 { size_t m=P->app.frames.size();P->frame(login(room,"admin"));sentTag(1);step(4000);CHECK(since(m,0x85).size()==1);const uint8_t author[4]={0xAA,0xBB,0xCC,0xDD};
   size_t q=P->queue.size();room->post(P->self_id.pub_key,author,"hello room");step(3000);CHECK(P->queue.size()==q+1);const V &f=P->queue[q];
   CHECK(f.size()==4+6+1+1+4+4+10&&f[0]==16&&f[1]==38&&f[2]==0&&f[3]==0&&V(f.begin()+4,f.begin()+10)==prefix6(room)&&f[10]==0xFF&&f[11]==2);
   CHECK(V(f.begin()+16,f.begin()+20)==V({0xAA,0xBB,0xCC,0xDD})&&V(f.begin()+20,f.end())==str("hello room"));}
 printf("ok: signed room post frame\n");

 // 12. Queue frame builders: V3 and pre-V3 layouts, flood paths report their hop count, long text is cut to the frame.
 { mesh::Packet pk;pk.header=ROUTE_TYPE_FLOOD;pk.path_len=2;pk._snr=-10;const uint8_t key6[32]={1,2,3,4,5,6};uint8_t f[176];
   size_t n=companion_remote::contactMsgFrame(false,&pk,key6,3,0x01020304,nullptr,0,"abc",f,sizeof(f));CHECK(V(f,f+n)==V({7,1,2,3,4,5,6,2,3,4,3,2,1,'a','b','c'}));
   n=companion_remote::contactMsgFrame(true,&pk,key6,2,0x01020304,(const uint8_t*)"\x09\x08\x07\x06",4,"abc",f,sizeof(f));CHECK(V(f,f+n)==V({16,uint8_t(int8_t(-10)),0,0,1,2,3,4,5,6,2,2,4,3,2,1,9,8,7,6,'a','b','c'}));
   std::string longText(300,'x');n=companion_remote::contactMsgFrame(true,&pk,key6,1,1,nullptr,0,longText.c_str(),f,sizeof(f));CHECK(n==sizeof(f)&&f[n-1]=='x');
   n=companion_remote::contactMsgFrame(false,&pk,key6,1,1,nullptr,0,longText.c_str(),f,20);CHECK(n==20);}
 printf("ok: queue frame builders\n");

 delete P;delete relay;for(auto *s:servers)delete s;
 printf("PASS: companion emulator - remote nodes: login (success, fail, guest, legacy OK), keep-alive connections, status / telemetry / binary / path-discovery responses matched by tag, trace path 0x89, CLI command and data, signed room posts, malformed frames\n");
}
