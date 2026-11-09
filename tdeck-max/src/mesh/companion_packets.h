#pragma once
// Packet-level companion commands as MeshCore's companion_radio (MyMesh.cpp, protocol v14): signing, raw data, raw
// packets, channel datagrams and flood scope. Header-only and generic over the mesh, so the host emulator
// (sim/companion_packets_emulator.cpp) runs this same code against real MeshCore nodes. Every command function takes
// the whole app frame {opcode}{args} and writes the reply frame into `out` (at least 1+64 bytes), returning its length.
//
// Wiring (companion.inc / service.cpp):
//  state the firmware keeps:  cp::Signer signer;  cp::ScopeState scope;  (both one instance per companion link)
//  case 33 CMD_SIGN_START:               n=companion_packets::signStart(signer,out);
//  case 34 CMD_SIGN_DATA:                n=companion_packets::signData(signer,in,len,out);
//  case 35 CMD_SIGN_FINISH:              n=companion_packets::signFinish(signer,selfId,out);   // selfId = the node's mesh::LocalIdentity
//  case 25 CMD_SEND_RAW_DATA:            n=companion_packets::sendRawData(mesh,in,len,out);
//  case 65 CMD_SEND_RAW_PACKET:          n=companion_packets::sendRawPacket(mesh,in,len,out);
//  case 62 CMD_SEND_CHANNEL_DATA:        n=companion_packets::sendChannelData(mesh,in,len,out);  // mesh needs getChannel(int,ChannelDetails&)
//  case 54 CMD_SET_FLOOD_SCOPE_KEY:      n=scope.setKey(in,len,out);
//  case 63 CMD_SET_DEFAULT_FLOOD_SCOPE:  n=scope.setDefault(in,len,out);  if(scope.dirty){save scope.serialize(blob) to Preferences; scope.dirty=false;}
//  case 64 CMD_GET_DEFAULT_FLOOD_SCOPE:  n=scope.getDefault(out);
//  (all these opcodes are advertised by the firmware; frames MyMesh does not recognise, e.g. a bare SIGN_DATA, answer ERR_UNSUPPORTED here.)
//  MeshCore callbacks (ChatMesh overrides, both protected virtuals):
//   onRawDataRecv(mesh::Packet *p)     -> n=companion_packets::rawDataFrame(_radio->getLastSNR(),_radio->getLastRSSI(),p,f,cap);
//                                         if(n)write f[0..n) to the app right now (a push, only while an app is connected); cap = size of the app out frame (MyMesh: 177).
//   onChannelDataRecv(const mesh::GroupChannel &c,mesh::Packet *p,uint16_t type,const uint8_t *d,size_t len)
//                                      -> n=companion_packets::channelDataFrame(*this,c,p,type,d,len,f,cap);
//                                         if(n) append f[0..n) to the offline message queue (the same one as RESP_CODE_CHANNEL_MSG_RECV) and push 0x83 MSG_WAITING.
//                                         The frame is queued like channel messages, so the queue's "is this a message frame" test must also accept opcode 27.
//   sendFloodScoped(contact|channel,...) -> flood with companion_packets::ScopeState::effective(key,&deviceRegionKey): true = send with transport
//                                         codes {key.calcTransportCode(pkt),0}, false = plain sendFlood. deviceRegionKey is the key of the device's own
//                                         region setting (SHA256 of "#"+currentScope as service.cpp's flood() derives it, nullptr when currentScope is "*"
//                                         or empty). Precedence: app "unscoped" > app override key > app default scope key > device region.
//  Persistence: scope.serialize(blob[BLOB_SIZE]) after a successful case 63 (scope.dirty) and at least on every change the firmware wants kept;
//   scope.deserialize(blob,n) once at boot (false = keep the empty state). MyMesh keeps only the default scope across reboots (the override key and
//   "unscoped" are per-connection); the blob holds all four, so after deserialize() the firmware SHOULD zero scope.overrideKey and clear scope.unscoped
//   to match.
#include <Mesh.h>
#include <helpers/BaseChatMesh.h>
#include <stdlib.h>
#include <string.h>
namespace companion_packets {
enum : uint8_t {CMD_SEND_RAW_DATA=25,CMD_SIGN_START=33,CMD_SIGN_DATA=34,CMD_SIGN_FINISH=35,CMD_SET_FLOOD_SCOPE_KEY=54,CMD_SEND_CHANNEL_DATA=62,
 CMD_SET_DEFAULT_FLOOD_SCOPE=63,CMD_GET_DEFAULT_FLOOD_SCOPE=64,CMD_SEND_RAW_PACKET=65,
 RESP_OK=0,RESP_ERR=1,RESP_SIGN_START=19,RESP_SIGNATURE=20,RESP_CHANNEL_DATA_RECV=27,RESP_DEFAULT_FLOOD_SCOPE=28,PUSH_RAW_DATA=0x84,
 ERR_UNSUPPORTED=1,ERR_NOT_FOUND=2,ERR_TABLE_FULL=3,ERR_BAD_STATE=4,ERR_ILLEGAL_ARG=6};
constexpr size_t MAX_SIGN_DATA_LEN=8*1024;
constexpr size_t MAX_CHANNEL_DATA_LENGTH=176-9;// MeshCore MAX_FRAME_SIZE-9
inline size_t error(uint8_t *out,uint8_t code){out[0]=RESP_ERR;out[1]=code;return 2;}
inline size_t ok(uint8_t *out){out[0]=RESP_OK;return 1;}
inline size_t pathBytes(uint8_t pathLen){return size_t(pathLen&63)*((pathLen>>6)+1);}

// ---- signing: SIGN_START allocates the 8 KiB buffer, SIGN_DATA appends, SIGN_FINISH signs and frees it ----
struct Signer {uint8_t *data=nullptr;size_t len=0;Signer()=default;Signer(const Signer&)=delete;~Signer(){free(data);}};
inline size_t signStart(Signer &s,uint8_t *out){
 free(s.data);s.len=0;s.data=(uint8_t*)malloc(MAX_SIGN_DATA_LEN);if(!s.data)return error(out,ERR_TABLE_FULL);
 out[0]=RESP_SIGN_START;out[1]=0;uint32_t max=MAX_SIGN_DATA_LEN;memcpy(out+2,&max,4);return 6;
}
inline size_t signData(Signer &s,const uint8_t *in,size_t len,uint8_t *out){
 if(len<=1)return error(out,ERR_UNSUPPORTED);// MyMesh only answers SIGN_DATA with a payload byte
 if(!s.data)return error(out,ERR_BAD_STATE);if(s.len+len-1>MAX_SIGN_DATA_LEN)return error(out,ERR_TABLE_FULL);
 memcpy(s.data+s.len,in+1,len-1);s.len+=len-1;return ok(out);
}
inline size_t signFinish(Signer &s,const mesh::LocalIdentity &id,uint8_t *out){
 if(!s.data)return error(out,ERR_BAD_STATE);
 id.sign(out+1,s.data,int(s.len));free(s.data);s.data=nullptr;s.len=0;out[0]=RESP_SIGNATURE;return 1+SIGNATURE_SIZE;
}

// ---- raw data / raw packets ----
// {25}{path length}{path}{payload, at least 4 bytes}: raw custom packet sent direct along the given path. Flood is not supported (as MyMesh).
template<class M> size_t sendRawData(M &node,const uint8_t *in,size_t len,uint8_t *out){
 if(len<6)return error(out,ERR_UNSUPPORTED);
 uint8_t pathLen=in[1];if(!mesh::Packet::isValidPathLen(pathLen))return error(out,ERR_UNSUPPORTED);
 size_t i=2+pathBytes(pathLen);if(i+4>len)return error(out,ERR_ILLEGAL_ARG);
 uint8_t path[MAX_PATH_SIZE];mesh::Packet::writePath(path,in+2,pathLen);
 mesh::Packet *packet=node.createRawData(in+i,len-i);if(!packet)return error(out,ERR_TABLE_FULL);
 node.sendDirect(packet,path,pathLen);return ok(out);
}
// {65}{priority}{wire-format packet}: the app builds the whole packet (header, transport codes, path, payload).
template<class M> size_t sendRawPacket(M &node,const uint8_t *in,size_t len,uint8_t *out){
 if(len<4)return error(out,ERR_UNSUPPORTED);
 mesh::Packet *packet=node.obtainNewPacket();if(!packet)return error(out,ERR_TABLE_FULL);
 if(!node.tryParsePacket(packet,in+2,int(len-2))){node.releasePacket(packet);return error(out,ERR_ILLEGAL_ARG);}
 node.sendPacket(packet,in[1],0);return ok(out);
}
// Raw custom packet heard (direct only, MeshCore filters): {0x84}{snr x4}{rssi}{0xFF}{payload}; 0 = does not fit into `cap`, drop it.
inline size_t rawDataFrame(float snr,float rssi,const mesh::Packet *packet,uint8_t *out,size_t cap){
 if(size_t(packet->payload_len)+4>cap)return 0;
 out[0]=PUSH_RAW_DATA;out[1]=uint8_t(int8_t(snr*4));out[2]=uint8_t(int8_t(rssi));out[3]=0xFF;memcpy(out+4,packet->payload,packet->payload_len);return size_t(packet->payload_len)+4;
}

// ---- channel datagrams ----
// {62}{channel idx}{path length, 0xFF = flood}{path}{data type 2 LE}{payload}.
template<class M> size_t sendChannelData(M &node,const uint8_t *in,size_t len,uint8_t *out){
 if(len<4)return error(out,ERR_ILLEGAL_ARG);
 uint8_t pathLen=in[2];if(!mesh::Packet::isValidPathLen(pathLen)&&pathLen!=OUT_PATH_UNKNOWN)return error(out,ERR_ILLEGAL_ARG);
 size_t n=pathLen==OUT_PATH_UNKNOWN?0:pathBytes(pathLen),i=3+n;if(i+2>len)return error(out,ERR_ILLEGAL_ARG);// MyMesh reads past the frame here
 uint8_t path[MAX_PATH_SIZE];if(n)mesh::Packet::writePath(path,in+3,pathLen);
 uint16_t type=uint16_t(in[i]|in[i+1]<<8);i+=2;size_t payloadLen=len-i;
 ChannelDetails channel;
 if(!node.getChannel(in[1],channel))return error(out,ERR_NOT_FOUND);
 if(type==0||payloadLen>MAX_GROUP_DATA_LENGTH)return error(out,ERR_ILLEGAL_ARG);// type 0 = DATA_TYPE_RESERVED
 if(!node.sendGroupData(channel.channel,path,pathLen,type,in+i,int(payloadLen)))return error(out,ERR_TABLE_FULL);
 return ok(out);
}
// Datagram heard on a channel: {27}{snr x4}{0}{0}{channel idx}{path length, 0xFF if not flooded}{type 2 LE}{len}{payload}; 0 = drop.
template<class M> size_t channelDataFrame(M &node,const mesh::GroupChannel &channel,const mesh::Packet *packet,uint16_t type,const uint8_t *data,size_t len,uint8_t *out,size_t cap){
 if(len>MAX_CHANNEL_DATA_LENGTH||9+len>cap)return 0;
 out[0]=RESP_CHANNEL_DATA_RECV;out[1]=uint8_t(int8_t(packet->getSNR()*4));out[2]=0;out[3]=0;out[4]=uint8_t(node.findChannelIdx(channel));
 out[5]=packet->isRouteFlood()?packet->path_len:0xFF;out[6]=uint8_t(type&0xFF);out[7]=uint8_t(type>>8);out[8]=uint8_t(len);
 if(len)memcpy(out+9,data,len);return 9+len;
}

// ---- flood scope ----
inline bool isNull(const uint8_t *key){for(int i=0;i<16;i++)if(key[i])return false;return true;}
struct ScopeState {
 static constexpr size_t BLOB_SIZE=1+1+16+32+16;// version, unscoped, override key, default name, default key
 uint8_t overrideKey[16]={0};bool unscoped=false;char defaultName[32]={0};uint8_t defaultKey[16]={0};bool dirty=false;// dirty: default scope changed, persist it
 // {54}{0}[{key 16}]: set the send-scope override (no/short key clears it); {54}{1}: send unscoped (v12+). Both leave the other mode.
 size_t setKey(const uint8_t *in,size_t len,uint8_t *out){
  if(len<2||in[1]>1)return error(out,ERR_UNSUPPORTED);
  if(in[1]==1){unscoped=true;return ok(out);}
  if(len>=2+16)memcpy(overrideKey,in+2,16);else memset(overrideKey,0,16);
  unscoped=false;return ok(out);
 }
 // {63}{name 31, NUL terminated}{key 16}: set the default scope; any shorter frame clears it.
 size_t setDefault(const uint8_t *in,size_t len,uint8_t *out){
  if(len>=1+31+16){
   size_t n=strnlen((const char*)in+1,31);if(n==0||n>=31)return error(out,ERR_ILLEGAL_ARG);
   memset(defaultName,0,sizeof(defaultName));memcpy(defaultName,in+1,n);memcpy(defaultKey,in+1+31,16);
  }else{memset(defaultName,0,sizeof(defaultName));memset(defaultKey,0,sizeof(defaultKey));}
  dirty=true;return ok(out);
 }
 // {28}[{name 31}{key 16}]: no name and key means none set.
 size_t getDefault(uint8_t *out) const {
  out[0]=RESP_DEFAULT_FLOOD_SCOPE;if(!defaultName[0])return 1;
  memcpy(out+1,defaultName,31);memcpy(out+1+31,defaultKey,16);return 1+31+16;
 }
 // Which key a flood send uses: false = plain unscoped flood. K is MeshCore's TransportKey (anything with a key[16] member).
 // `device` = the device's own region key, used only when the app set nothing.
 template<class K> bool effective(K &key,const K *device=nullptr) const {
  if(unscoped)return false;
  if(!isNull(overrideKey)){memcpy(key.key,overrideKey,16);return true;}
  if(!isNull(defaultKey)){memcpy(key.key,defaultKey,16);return true;}
  if(device&&!isNull(device->key)){key=*device;return true;}
  return false;
 }
 size_t serialize(uint8_t *out) const {
  out[0]=1;out[1]=unscoped;memcpy(out+2,overrideKey,16);memcpy(out+18,defaultName,32);memcpy(out+50,defaultKey,16);return BLOB_SIZE;
 }
 // false (state untouched) for a wrong size, version or an unterminated name.
 bool deserialize(const uint8_t *in,size_t len){
  if(len!=BLOB_SIZE||in[0]!=1||in[1]>1||in[18+31]!=0)return false;
  unscoped=in[1];memcpy(overrideKey,in+2,16);memcpy(defaultName,in+18,32);memcpy(defaultKey,in+50,16);return true;
 }
};
}
