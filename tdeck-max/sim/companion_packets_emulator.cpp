// PC emulator for the packet-level companion commands (src/mesh/companion_packets.h): signing, raw data, raw packets,
// channel datagrams and flood scope. Real MeshCore core (Mesh, BaseChatMesh, Ed25519/AES) on an in-memory radio; two or
// three companion nodes play app + peers. Frame layouts are checked byte for byte against companion_radio/MyMesh.cpp.
#include "companion_sim.h"
#include "mesh/companion_packets.h"
struct TransportKey {uint8_t key[16];};// MeshCore's helpers/TransportKeyStore.h needs the firmware's filesystem; only key[16] matters here
namespace cp=companion_packets;
using Frame=std::vector<uint8_t>;
namespace {
// Companion node: calls the module the way companion.inc / service.cpp will (see the wiring list in the header).
// The script builds BaseChatMesh.cpp without MAX_GROUP_CHANNELS, so this node keeps its own channel table (the same
// getChannel/findChannelIdx/searchChannelsByHash the firmware has through MAX_GROUP_CHANNELS).
class Companion:public Parts,public BaseChatMesh {
public:App app;std::vector<Frame> queue;cp::ScopeState scope;cp::Signer signer;ChannelDetails ch[4];bool fwd=false;
 Companion(uint32_t seed):BaseChatMesh(radio,ms,rng,clock,pool,tables){rng.g.seed(seed);self_id=mesh::LocalIdentity(&rng);begin();memset(ch,0,sizeof(ch));}
 void addChannel(int idx,uint8_t fill){ChannelDetails &c=ch[idx];memset(c.channel.secret,0,sizeof(c.channel.secret));memset(c.channel.secret,fill,16);mesh::Utils::sha256(c.channel.hash,sizeof(c.channel.hash),c.channel.secret,16);strcpy(c.name,"chan");}
 bool getChannel(int i,ChannelDetails &d){if(i<0||i>=4)return false;d=ch[i];return true;}
 int findChannelIdx(const mesh::GroupChannel &g){for(int i=0;i<4;i++)if(!memcmp(g.secret,ch[i].channel.secret,sizeof(g.secret)))return i;return -1;}
 // One app frame in, the reply frame out (what the switch in companion.inc does).
 Frame frame(const Frame &in){uint8_t out[200];size_t n=0;const uint8_t *p=in.data();size_t len=in.size();
  switch(p[0]){
  case cp::CMD_SIGN_START:n=cp::signStart(signer,out);break;
  case cp::CMD_SIGN_DATA:n=cp::signData(signer,p,len,out);break;
  case cp::CMD_SIGN_FINISH:n=cp::signFinish(signer,self_id,out);break;
  case cp::CMD_SEND_RAW_DATA:n=cp::sendRawData(*this,p,len,out);break;
  case cp::CMD_SEND_RAW_PACKET:n=cp::sendRawPacket(*this,p,len,out);break;
  case cp::CMD_SEND_CHANNEL_DATA:n=cp::sendChannelData(*this,p,len,out);break;
  case cp::CMD_SET_FLOOD_SCOPE_KEY:n=scope.setKey(p,len,out);break;
  case cp::CMD_SET_DEFAULT_FLOOD_SCOPE:n=scope.setDefault(p,len,out);break;
  case cp::CMD_GET_DEFAULT_FLOOD_SCOPE:n=scope.getDefault(out);break;
  }
  return Frame(out,out+n);}
protected:
 void onRawDataRecv(mesh::Packet *p)override{uint8_t f[177];size_t n=cp::rawDataFrame(radio.getLastSNR(),radio.getLastRSSI(),p,f,sizeof(f));if(n)app.write(f,n);}
 void onChannelDataRecv(const mesh::GroupChannel &c,mesh::Packet *p,uint16_t type,const uint8_t *d,size_t n)override{uint8_t f[177];size_t m=cp::channelDataFrame(*this,c,p,type,d,n,f,sizeof(f));if(m)queue.emplace_back(f,f+m);}
 int searchChannelsByHash(const uint8_t *hash,mesh::GroupChannel dest[],int max)override{int n=0;for(int i=0;i<4&&n<max;i++)if(ch[i].name[0]&&ch[i].channel.hash[0]==hash[0])dest[n++]=ch[i].channel;return n;}
 void sendScoped(mesh::Packet *p,uint32_t delay){TransportKey k;if(scope.effective(k)){uint16_t codes[2]={0x1234,0};sendFlood(p,codes,delay);}else sendFlood(p,delay);}
 void sendFloodScoped(const ContactInfo&,mesh::Packet *p,uint32_t delay=0)override{sendScoped(p,delay);}
 void sendFloodScoped(const mesh::GroupChannel&,mesh::Packet *p,uint32_t delay=0)override{sendScoped(p,delay);}
 float getAirtimeBudgetFactor()const override{return 0;}int calcRxDelay(float,uint32_t)const override{return 0;}bool allowPacketForward(const mesh::Packet*)override{return fwd;}
 void onDiscoveredContact(ContactInfo&,bool,uint8_t,const uint8_t*)override{}void onContactPathUpdated(const ContactInfo&)override{}ContactInfo *processAck(const uint8_t*)override{return nullptr;}
 void onMessageRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*)override{}void onCommandDataRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*)override{}
 void onCLICommandRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*,char *reply)override{reply[0]=0;}void onSignedMessageRecv(const ContactInfo&,mesh::Packet*,uint32_t,const uint8_t*,const char*)override{}
 void onChannelMessageRecv(const mesh::GroupChannel&,mesh::Packet*,uint32_t,const char*)override{}
 uint32_t calcFloodTimeoutMillisFor(uint32_t)const override{return 3000;}uint32_t calcDirectTimeoutMillisFor(uint32_t,uint8_t)const override{return 3000;}void onSendTimeout()override{}
 void onContactResponse(const ContactInfo&,const uint8_t*,uint8_t)override{}uint8_t onContactRequest(const ContactInfo&,uint32_t,const uint8_t*,uint8_t,uint8_t*)override{return 0;}
};
Frame ok(){return {cp::RESP_OK};}
Frame err(uint8_t code){return {cp::RESP_ERR,code};}
Frame cat(Frame a,const Frame &b){a.insert(a.end(),b.begin(),b.end());return a;}
Frame bytes(size_t n,uint8_t seed){Frame f(n);for(size_t i=0;i<n;i++)f[i]=uint8_t(seed+i*7);return f;}
// Fill the packet pool so the next obtainNewPacket() fails, run fn, give the packets back.
template<class F> void withPoolFull(Companion &n,F fn){std::vector<mesh::Packet*> held;while(auto *p=n.obtainNewPacket())held.push_back(p);fn();for(auto *p:held)n.releasePacket(p);}
Frame withKey(uint8_t cmd,uint8_t sub,const uint8_t *key){Frame f={cmd,sub};if(key)f.insert(f.end(),key,key+16);return f;}
}

int main(){
 setvbuf(stdout,nullptr,_IONBF,0);
 auto *a=new Companion(1);auto *b=new Companion(2);auto *c=new Companion(3);link(a->radio,b->radio);

 // 1. Signing: a multi-chunk buffer signed by the node's own key verifies with the library's Ed25519 verify.
 {Frame r=a->frame({cp::CMD_SIGN_START});assert(r==(Frame{cp::RESP_SIGN_START,0,0x00,0x20,0x00,0x00}));// reserved, max 8192 LE
  Frame d1=bytes(175,1),d2=bytes(100,9),d3=bytes(1,33),all=cat(cat(d1,d2),d3);
  for(auto &d:{d1,d2,d3})assert(a->frame(cat({cp::CMD_SIGN_DATA},d))==ok());
  r=a->frame({cp::CMD_SIGN_FINISH});assert(r.size()==1+64&&r[0]==cp::RESP_SIGNATURE);
  mesh::Identity id(a->self_id.pub_key);assert(id.verify(&r[1],all.data(),int(all.size())));
  assert(!id.verify(&r[1],d1.data(),int(d1.size())));// the signature covers exactly what was streamed
  assert(!mesh::Identity(b->self_id.pub_key).verify(&r[1],all.data(),int(all.size())));
  // state errors
  assert(a->frame({cp::CMD_SIGN_FINISH})==err(cp::ERR_BAD_STATE));// finish twice
  assert(a->frame(cat({cp::CMD_SIGN_DATA},d1))==err(cp::ERR_BAD_STATE));// data after finish
  assert(b->frame(cat({cp::CMD_SIGN_DATA},d1))==err(cp::ERR_BAD_STATE));// data before start
  assert(b->frame({cp::CMD_SIGN_FINISH})==err(cp::ERR_BAD_STATE));// finish before start
  // restarting drops the earlier data
  a->frame({cp::CMD_SIGN_START});a->frame(cat({cp::CMD_SIGN_DATA},d1));a->frame({cp::CMD_SIGN_START});a->frame(cat({cp::CMD_SIGN_DATA},d2));
  r=a->frame({cp::CMD_SIGN_FINISH});assert(id.verify(&r[1],d2.data(),int(d2.size())));
  // oversize: 46*175+142 = 8192 fills the buffer exactly; one more byte (or a chunk crossing the end) is TABLE_FULL and changes nothing
  a->frame({cp::CMD_SIGN_START});Frame big;
  for(int i=0;i<46;i++){Frame d=bytes(175,uint8_t(i));big=cat(big,d);assert(a->frame(cat({cp::CMD_SIGN_DATA},d))==ok());}
  assert(a->frame(cat({cp::CMD_SIGN_DATA},bytes(175,0)))==err(cp::ERR_TABLE_FULL));
  Frame tail=bytes(142,5);big=cat(big,tail);assert(a->frame(cat({cp::CMD_SIGN_DATA},tail))==ok());assert(big.size()==cp::MAX_SIGN_DATA_LEN);
  assert(a->frame({cp::CMD_SIGN_DATA,1})==err(cp::ERR_TABLE_FULL));
  r=a->frame({cp::CMD_SIGN_FINISH});assert(r[0]==cp::RESP_SIGNATURE&&id.verify(&r[1],big.data(),int(big.size())));
  // MyMesh needs a payload byte for SIGN_DATA; a bare opcode falls through to "unsupported"
  assert(a->frame({cp::CMD_SIGN_DATA})==err(cp::ERR_UNSUPPORTED));
  // finish with nothing streamed signs the empty message
  a->frame({cp::CMD_SIGN_START});r=a->frame({cp::CMD_SIGN_FINISH});assert(r[0]==cp::RESP_SIGNATURE&&id.verify(&r[1],nullptr,0));
  printf("sign: multi-chunk Ed25519 signature verifies, state errors, 8 KiB limit\n");}

 // 2. CMD_SEND_RAW_DATA (direct only) -> PUSH_RAW_DATA {snr x4}{rssi}{0xFF}{payload} on the receiving node.
 {Frame payload=bytes(12,40);assert(a->frame(cat({cp::CMD_SEND_RAW_DATA,0},payload))==ok());
  run(2000,*a,*b);assert(b->app.frames.size()==1);
  assert(b->app.frames[0]==cat({cp::PUSH_RAW_DATA,38,uint8_t(int8_t(-71)),0xFF},payload));// 9.5 dB*4, -71 dBm, reserved 0xFF
  // a path with one hop goes on air as a direct packet with that path (the node in the path forwards it; the peer is not the final hop)
  Frame p2=bytes(5,70);b->radio.in.clear();assert(a->frame(cat({cp::CMD_SEND_RAW_DATA,1,0xAB},p2))==ok());run(1000,*a);
  assert(b->radio.in.size()==1&&b->radio.in.front()==cat({uint8_t((PAYLOAD_TYPE_RAW_CUSTOM<<PH_TYPE_SHIFT)|ROUTE_TYPE_DIRECT),1,0xAB},p2));b->radio.in.clear();
  // errors, as MyMesh: short frame and flood (invalid path length) are "unsupported", payload under 4 bytes is illegal
  assert(a->frame({cp::CMD_SEND_RAW_DATA,0,1,2,3})==err(cp::ERR_UNSUPPORTED));
  assert(a->frame({cp::CMD_SEND_RAW_DATA,0xFF,1,2,3,4,5})==err(cp::ERR_UNSUPPORTED));
  assert(a->frame({cp::CMD_SEND_RAW_DATA,0xC1,1,2,3,4,5})==err(cp::ERR_UNSUPPORTED));
  assert(a->frame({cp::CMD_SEND_RAW_DATA,1,0xAB,1,2,3})==err(cp::ERR_ILLEGAL_ARG));
  assert(a->frame({cp::CMD_SEND_RAW_DATA,40,1,2,3,4})==err(cp::ERR_ILLEGAL_ARG));// path longer than the frame
  withPoolFull(*a,[&]{assert(a->frame(cat({cp::CMD_SEND_RAW_DATA,0},bytes(8,1)))==err(cp::ERR_TABLE_FULL));});
  // receiver side: a payload that does not fit {0x84,snr,rssi,0xFF}+payload into a 177-byte frame is not pushed
  size_t before=b->app.frames.size();
  for(size_t n:{size_t(173),size_t(174)}){mesh::Packet *pk=a->createRawData(bytes(n,uint8_t(n)).data(),n);assert(pk);a->sendDirect(pk,nullptr,0);run(2000,*a,*b);}
  assert(b->app.frames.size()==before+1&&b->app.frames.back().size()==177);
  printf("raw data: 0x84 push layout, path/short/pool errors, receive-side size limit\n");}

 // 3. CMD_SEND_RAW_PACKET {priority}{wire packet}: a raw custom direct packet built by the app reaches the peer.
 {Frame wire=cat({uint8_t((PAYLOAD_TYPE_RAW_CUSTOM<<PH_TYPE_SHIFT)|ROUTE_TYPE_DIRECT),0},bytes(9,90));size_t before=b->app.frames.size();
  assert(a->frame(cat({cp::CMD_SEND_RAW_PACKET,3},wire))==ok());run(2000,*a,*b);
  assert(b->app.frames.size()==before+1&&b->app.frames.back()==cat({cp::PUSH_RAW_DATA,38,uint8_t(int8_t(-71)),0xFF},bytes(9,90)));
  assert(a->frame({cp::CMD_SEND_RAW_PACKET,0,0})==err(cp::ERR_UNSUPPORTED));// needs at least 4 bytes
  assert(a->frame(cat({cp::CMD_SEND_RAW_PACKET,0},{uint8_t(0x40|(PAYLOAD_TYPE_RAW_CUSTOM<<2)|ROUTE_TYPE_DIRECT),0,1,2}))==err(cp::ERR_ILLEGAL_ARG));// payload version 2
  assert(a->frame({cp::CMD_SEND_RAW_PACKET,0,uint8_t(ROUTE_TYPE_DIRECT),0xC0,1,2})==err(cp::ERR_ILLEGAL_ARG));// reserved path mode
  assert(a->frame({cp::CMD_SEND_RAW_PACKET,0,uint8_t(ROUTE_TYPE_DIRECT),5,1,2})==err(cp::ERR_ILLEGAL_ARG));// path longer than the packet
  assert(a->frame(cat({cp::CMD_SEND_RAW_PACKET,0},cat({uint8_t(ROUTE_TYPE_DIRECT),0},bytes(MAX_PACKET_PAYLOAD+1,0))))==err(cp::ERR_ILLEGAL_ARG));// payload too big
  withPoolFull(*a,[&]{assert(a->frame(cat({cp::CMD_SEND_RAW_PACKET,0},wire))==err(cp::ERR_TABLE_FULL));});
  printf("raw packet: peer receives the packet, malformed packets rejected\n");}

 // 4. Channel datagrams: a and b share a channel (index 0 on a, 1 on b); c sits between a and the far node d.
 {a->addChannel(0,0x11);b->addChannel(1,0x11);b->addChannel(0,0x22);
  Frame payload=bytes(20,60);
  // direct, empty path: the receiver reports path length 0xFF
  assert(a->frame(cat({cp::CMD_SEND_CHANNEL_DATA,0,0,0x34,0x12},payload))==ok());run(2000,*a,*b);
  assert(b->queue.size()==1);assert(b->queue[0]==cat({cp::RESP_CHANNEL_DATA_RECV,38,0,0,1,0xFF,0x34,0x12,20},payload));
  // flood, heard straight from the sender: path length 0
  Frame p2=bytes(7,80);assert(a->frame(cat({cp::CMD_SEND_CHANNEL_DATA,0,0xFF,0xFF,0xFF},p2))==ok());run(2000,*a,*b);
  assert(b->queue.size()==2&&b->queue[1]==cat({cp::RESP_CHANNEL_DATA_RECV,38,0,0,1,0,0xFF,0xFF,7},p2));
  // empty payload and the largest payload
  assert(a->frame({cp::CMD_SEND_CHANNEL_DATA,0,0xFF,0x01,0x00})==ok());run(2000,*a,*b);
  assert(b->queue.size()==3&&b->queue[2]==Frame({cp::RESP_CHANNEL_DATA_RECV,38,0,0,1,0,0x01,0x00,0}));
  Frame mx=bytes(MAX_GROUP_DATA_LENGTH,3);assert(a->frame(cat({cp::CMD_SEND_CHANNEL_DATA,0,0xFF,0x02,0x00},mx))==ok());run(2000,*a,*b);
  assert(b->queue.size()==4&&b->queue[3]==cat({cp::RESP_CHANNEL_DATA_RECV,38,0,0,1,0,0x02,0x00,uint8_t(MAX_GROUP_DATA_LENGTH)},mx));
  // errors, in MyMesh's order
  assert(a->frame({cp::CMD_SEND_CHANNEL_DATA,0,0xFF})==err(cp::ERR_ILLEGAL_ARG));// under 4 bytes
  assert(a->frame({cp::CMD_SEND_CHANNEL_DATA,0,0xC1,1,0})==err(cp::ERR_ILLEGAL_ARG));// invalid path length
  assert(a->frame({cp::CMD_SEND_CHANNEL_DATA,0,3,1,2,3,4})==err(cp::ERR_ILLEGAL_ARG));// path runs past the frame
  assert(a->frame({cp::CMD_SEND_CHANNEL_DATA,9,0xFF,1,0})==err(cp::ERR_NOT_FOUND));
  assert(a->frame({cp::CMD_SEND_CHANNEL_DATA,0,0xFF,0,0,1})==err(cp::ERR_ILLEGAL_ARG));// reserved data type 0
  assert(a->frame(cat({cp::CMD_SEND_CHANNEL_DATA,0,0xFF,1,0},bytes(MAX_GROUP_DATA_LENGTH+1,0)))==err(cp::ERR_ILLEGAL_ARG));
  withPoolFull(*a,[&]{assert(a->frame({cp::CMD_SEND_CHANNEL_DATA,0,0xFF,1,0,7})==err(cp::ERR_TABLE_FULL));});
  assert(b->queue.size()==4);
  // through a repeater: a - c - d, path length 1 when it arrives
  auto *d=new Companion(4);d->addChannel(2,0x11);link(a->radio,c->radio);link(c->radio,d->radio);c->fwd=true;
  a->radio.hears.erase(std::find(a->radio.hears.begin(),a->radio.hears.end(),&b->radio));b->radio.hears.clear();
  Frame p3=bytes(9,120);assert(a->frame(cat({cp::CMD_SEND_CHANNEL_DATA,0,0xFF,0x77,0x00},p3))==ok());run(4000,*a,*c,*d);
  assert(d->queue.size()==1&&d->queue[0]==cat({cp::RESP_CHANNEL_DATA_RECV,38,0,0,2,1,0x77,0x00,9},p3));
  delete d;
  printf("channel data: type-27 layout (idx, path length 0xFF/0/1, data type), limits, errors\n");}

 // 5. Flood scope: override key, unscoped, default scope, and which one a send uses.
 {uint8_t k1[16],k2[16],dev[16];for(int i=0;i<16;i++){k1[i]=uint8_t(i+1);k2[i]=uint8_t(0xA0+i);dev[i]=uint8_t(0x50+i);}
  cp::ScopeState &s=a->scope;TransportKey k;TransportKey device;memcpy(device.key,dev,16);
  assert(!s.effective(k)&&s.effective(k,&device));// nothing set: only the device's own region applies
  assert(s.effective(k,&device)&&!memcmp(k.key,dev,16));
  assert(a->frame(withKey(cp::CMD_SET_FLOOD_SCOPE_KEY,0,k1))==ok());assert(s.effective(k)&&!memcmp(k.key,k1,16));
  assert(s.effective(k,&device)&&!memcmp(k.key,k1,16));// override wins over the device region
  assert(a->frame({cp::CMD_SET_FLOOD_SCOPE_KEY,0})==ok());assert(!s.effective(k));// no key: clears the override
  assert(a->frame(withKey(cp::CMD_SET_FLOOD_SCOPE_KEY,0,k1))==ok());
  assert(a->frame(Frame({cp::CMD_SET_FLOOD_SCOPE_KEY,0,1,2,3}))==ok());assert(!s.effective(k));// short key counts as "no key", as MyMesh
  assert(a->frame({cp::CMD_SET_FLOOD_SCOPE_KEY,1})==ok());assert(!s.effective(k)&&!s.effective(k,&device));// send unscoped, even without key
  assert(a->frame(withKey(cp::CMD_SET_FLOOD_SCOPE_KEY,0,k2))==ok());assert(s.effective(k)&&!memcmp(k.key,k2,16));// setting a key leaves unscoped mode
  assert(a->frame({cp::CMD_SET_FLOOD_SCOPE_KEY,2})==err(cp::ERR_UNSUPPORTED));assert(a->frame({cp::CMD_SET_FLOOD_SCOPE_KEY})==err(cp::ERR_UNSUPPORTED));
  assert(a->frame({cp::CMD_SET_FLOOD_SCOPE_KEY,0})==ok());
  // default scope: empty, set, read back, bad names, clear
  assert(a->frame({cp::CMD_GET_DEFAULT_FLOOD_SCOPE})==Frame({cp::RESP_DEFAULT_FLOOD_SCOPE}));
  Frame set={cp::CMD_SET_DEFAULT_FLOOD_SCOPE};Frame name(31,0);memcpy(name.data(),"usa-west",8);set=cat(cat(set,name),Frame(k2,k2+16));
  assert(a->frame(set)==ok());assert(s.dirty);
  assert(a->frame({cp::CMD_GET_DEFAULT_FLOOD_SCOPE})==cat({cp::RESP_DEFAULT_FLOOD_SCOPE},cat(name,Frame(k2,k2+16))));
  assert(s.effective(k)&&!memcmp(k.key,k2,16));// default applies when there is no override
  assert(a->frame(withKey(cp::CMD_SET_FLOOD_SCOPE_KEY,0,k1))==ok());assert(s.effective(k)&&!memcmp(k.key,k1,16));// override wins over default
  assert(a->frame({cp::CMD_SET_FLOOD_SCOPE_KEY,1})==ok());assert(!s.effective(k));// unscoped wins over both
  {Frame bad=set;bad[1]=0;assert(a->frame(bad)==err(cp::ERR_ILLEGAL_ARG));// empty name
   Frame full={cp::CMD_SET_DEFAULT_FLOOD_SCOPE};full=cat(cat(full,Frame(31,'x')),Frame(k2,k2+16));assert(a->frame(full)==err(cp::ERR_ILLEGAL_ARG));// name with no terminator
   assert(a->frame({cp::CMD_GET_DEFAULT_FLOOD_SCOPE})==cat({cp::RESP_DEFAULT_FLOOD_SCOPE},cat(name,Frame(k2,k2+16))));}// failed sets change nothing
  // serialization of the whole state
  uint8_t blob[cp::ScopeState::BLOB_SIZE];assert(s.serialize(blob)==sizeof(blob));
  cp::ScopeState t;assert(t.deserialize(blob,sizeof(blob)));
  assert(t.unscoped&&!memcmp(t.overrideKey,k1,16)&&!strcmp(t.defaultName,"usa-west")&&!memcmp(t.defaultKey,k2,16)&&!t.effective(k));
  t.unscoped=false;assert(t.effective(k)&&!memcmp(k.key,k1,16));
  {Frame get;uint8_t out[64];size_t n=t.getDefault(out);get.assign(out,out+n);assert(get==cat({cp::RESP_DEFAULT_FLOOD_SCOPE},cat(name,Frame(k2,k2+16))));}
  uint8_t shortBlob[cp::ScopeState::BLOB_SIZE-1];memcpy(shortBlob,blob,sizeof(shortBlob));cp::ScopeState u;assert(!u.deserialize(shortBlob,sizeof(shortBlob)));
  uint8_t badVer[cp::ScopeState::BLOB_SIZE];memcpy(badVer,blob,sizeof(blob));badVer[0]^=0xFF;assert(!u.deserialize(badVer,sizeof(badVer)));
  uint8_t noTerm[cp::ScopeState::BLOB_SIZE];memcpy(noTerm,blob,sizeof(blob));memset(noTerm+cp::ScopeState::BLOB_SIZE-16-32,'x',32);assert(!u.deserialize(noTerm,sizeof(noTerm)));
  assert(!u.unscoped&&!u.defaultName[0]);// rejected blobs leave the state untouched
  // clearing form (anything shorter than name+key)
  assert(a->frame({cp::CMD_SET_DEFAULT_FLOOD_SCOPE})==ok());assert(a->frame({cp::CMD_GET_DEFAULT_FLOOD_SCOPE})==Frame({cp::RESP_DEFAULT_FLOOD_SCOPE}));
  assert(a->frame(Frame({cp::CMD_SET_DEFAULT_FLOOD_SCOPE,'a','b'}))==ok());assert(!s.defaultName[0]);
  // the node's own sends follow the state: channel flood with no scope / override / unscoped / default
  b->radio.in.clear();a->radio.hears={&b->radio};b->radio.hears={&a->radio};
  auto route=[&](uint8_t seed){a->frame(cat({cp::CMD_SEND_CHANNEL_DATA,0,0xFF,1,0},Frame{seed}));run(1000,*a);assert(b->radio.in.size()==1);uint8_t r=b->radio.in.front()[0]&PH_ROUTE_MASK;b->radio.in.clear();return r;};
  a->frame({cp::CMD_SET_FLOOD_SCOPE_KEY,0});assert(route(1)==ROUTE_TYPE_FLOOD);
  a->frame(withKey(cp::CMD_SET_FLOOD_SCOPE_KEY,0,k1));assert(route(2)==ROUTE_TYPE_TRANSPORT_FLOOD);
  a->frame({cp::CMD_SET_FLOOD_SCOPE_KEY,1});assert(route(3)==ROUTE_TYPE_FLOOD);
  a->frame({cp::CMD_SET_FLOOD_SCOPE_KEY,0});a->frame(set);assert(route(4)==ROUTE_TYPE_TRANSPORT_FLOOD);
  printf("scope: override/unscoped/default round trips, device-region fallback, blob serialization, sends follow the state\n");}

 delete a;delete b;delete c;
 printf("PASS: companion packets emulator - Ed25519 signing (multi-chunk, state errors, 8 KiB limit), raw data 0x84 push, raw packet send, channel data type-27 frames, flood scope key / default scope with serialization, malformed frames\n");
}
