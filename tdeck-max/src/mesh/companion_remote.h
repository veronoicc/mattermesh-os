#pragma once
// Companion app commands aimed at remote nodes (repeaters, room servers), byte-compatible with MeshCore's
// examples/companion_radio/MyMesh.cpp (protocol v14): login / logout / has-connection, status, telemetry, binary
// and path-discovery requests with their pushes, trace path, CLI over TXT_MSG (txt_type 1 / 3), and the queued message
// frames for CLI replies and signed room posts. Header-only and generic over the mesh so the host emulator
// (sim/companion_remote_emulator.cpp) runs this same code against real MeshCore nodes.
//
// The firmware feeds it from the ChatMesh callbacks of service.cpp: the responses, path returns, trace results, CLI
// replies and signed room posts a node receives become pushes or queued message frames for the app, and the app's
// commands in service.cpp's companion.inc call command and sendCommand. The ChatMesh exposes the connection calls and a trace
// timeout to it, the pending requests live in one Pending that the older anonymous request path (companion_air) shares,
// and commands that transmit are refused while the radio sleeps for the companion link (see transmits).
// Shorter frames than MyMesh needs answer unsupported like MyMesh's final else, an unknown contact answers not found.
// MyMesh never calls the connection keep-alive check, and neither does this.
// No persistence or flash is involved; connections live in BaseChatMesh and die with the node.
// Quirks kept from MyMesh: one pending request per kind and every send clears all others; login is matched by the
// first 4 bytes of the contact key and status by the same prefix (the status reply carries no usable tag), binary
// / telemetry / path discovery by tag. Not reproduced: MyMesh pushes responses nobody waits for as PUSH_BINARY_RESPONSE
// (it forwards telemetry broadcasts); here such responses are dropped, so a stale tag never reaches the app.
#include <Mesh.h>
#include <helpers/BaseChatMesh.h>
#include <string.h>
#include "companion_air.h"
namespace companion_remote {
enum : uint8_t {CMD_SEND_TXT_MSG=2,CMD_SEND_LOGIN=26,CMD_SEND_STATUS_REQ=27,CMD_HAS_CONNECTION=28,CMD_LOGOUT=29,CMD_SEND_TRACE_PATH=36,
 CMD_SEND_TELEMETRY_REQ=39,CMD_SEND_BINARY_REQ=50,CMD_SEND_PATH_DISCOVERY_REQ=52,
 RESP_CONTACT_MSG_RECV=7,RESP_CONTACT_MSG_RECV_V3=16,
 PUSH_LOGIN_SUCCESS=0x85,PUSH_LOGIN_FAIL=0x86,PUSH_STATUS_RESPONSE=0x87,PUSH_TRACE_DATA=0x89,PUSH_TELEMETRY_RESPONSE=0x8B,PUSH_PATH_DISCOVERY_RESPONSE=0x8D,
 ERR_UNSUPPORTED=1,ERR_NOT_FOUND=2,ERR_TABLE_FULL=3,ERR_ILLEGAL_ARG=6,
 REQ_GET_TELEMETRY=0x03,PERM_BASE=0x01};// txt_type / status request codes: MeshCore's TXT_TYPE_* and REQ_TYPE_GET_STATUS macros
// The app's outstanding requests. req is MyMesh's pending_req: anonymous (companion_air) and binary requests.
struct Pending {
 uint32_t login=0,status=0,telemetry=0,discovery=0;companion_air::Pending req;
 void anonSent(){login=status=telemetry=discovery=0;}// the anonymous request itself sets req.tag
 void clear(){anonSent();req.tag=0;}
};
// Commands that put a packet on air (the 4-byte telemetry request is the node's own sensors).
inline bool transmits(uint8_t cmd,size_t len){
 return cmd==CMD_SEND_LOGIN||cmd==CMD_SEND_STATUS_REQ||cmd==CMD_SEND_TRACE_PATH||cmd==CMD_SEND_BINARY_REQ||cmd==CMD_SEND_PATH_DISCOVERY_REQ||(cmd==CMD_SEND_TELEMETRY_REQ&&len!=4);
}
inline size_t error(uint8_t *out,uint8_t code){return companion_air::error(out,code);}
// {6}{flood?}{tag 4}{estimated timeout 4}
inline size_t sent(uint8_t *out,int result,uint32_t tag,uint32_t timeout){
 out[0]=companion_air::RESP_SENT;out[1]=result==MSG_SEND_SENT_FLOOD?1:0;memcpy(out+2,&tag,4);memcpy(out+6,&timeout,4);return 10;
}
namespace detail {
constexpr size_t KEY=PUB_KEY_SIZE;
template<class M> size_t login(M &node,Pending &p,const uint8_t *in,size_t len,uint8_t *out){
 ContactInfo *to=node.lookupContactByPubKey(in+1,KEY);if(!to)return error(out,ERR_NOT_FOUND);
 char password[256];size_t n=len-1-KEY;if(n>=sizeof(password))return error(out,ERR_TABLE_FULL);memcpy(password,in+1+KEY,n);password[n]=0;
 uint32_t timeout=0;int result=node.sendLogin(*to,password,timeout);if(result==MSG_SEND_FAILED)return error(out,ERR_TABLE_FULL);
 p.clear();memcpy(&p.login,to->id.pub_key,4);return sent(out,result,p.login,timeout);// the "tag" is the key prefix
}
// Status / telemetry / binary: one request, then the pending slot of its kind is armed.
template<class M> size_t request(M &node,Pending &p,const uint8_t *key,uint8_t cmd,const uint8_t *data,size_t dataLen,uint8_t *out){
 ContactInfo *to=node.lookupContactByPubKey(key,KEY);if(!to)return error(out,ERR_NOT_FOUND);
 uint32_t tag=0,timeout=0;int result;
 if(cmd==CMD_SEND_BINARY_REQ){if(dataLen>MAX_PACKET_PAYLOAD)return error(out,ERR_TABLE_FULL);result=node.sendRequest(*to,data,uint8_t(dataLen),tag,timeout);}
 else result=node.sendRequest(*to,cmd==CMD_SEND_STATUS_REQ?uint8_t(REQ_TYPE_GET_STATUS):uint8_t(REQ_GET_TELEMETRY),tag,timeout);
 if(result==MSG_SEND_FAILED)return error(out,ERR_TABLE_FULL);
 p.clear();
 if(cmd==CMD_SEND_STATUS_REQ)memcpy(&p.status,to->id.pub_key,4);// legacy matching scheme
 else if(cmd==CMD_SEND_TELEMETRY_REQ)p.telemetry=tag;else p.req.tag=tag;
 return sent(out,result,tag,timeout);
}
// 'Path discovery' is a flood telemetry request for base data, whatever path is known.
template<class M> size_t discovery(M &node,Pending &p,const uint8_t *in,uint8_t *out){
 ContactInfo *to=node.lookupContactByPubKey(in+2,KEY);if(!to)return error(out,ERR_NOT_FOUND);
 uint8_t req[9];req[0]=REQ_GET_TELEMETRY;req[1]=uint8_t(~PERM_BASE);memset(req+2,0,3);node.getRNG()->random(req+5,4);// inverse permission mask, reserved, random blob
 uint32_t tag=0,timeout=0;uint8_t saved=to->out_path_len;to->out_path_len=OUT_PATH_UNKNOWN;
 int result=node.sendRequest(*to,req,sizeof(req),tag,timeout);to->out_path_len=saved;
 if(result==MSG_SEND_FAILED)return error(out,ERR_TABLE_FULL);
 p.clear();p.discovery=tag;return sent(out,result,tag,timeout);
}
// {36}{tag 4}{auth 4}{flags}{path}: a zero-hop "ping" is a path with just the repeater's hash.
template<class M> size_t trace(M &node,const uint8_t *in,size_t len,uint8_t *out){
 uint8_t pathLen=uint8_t(len-10),flags=in[9],sz=flags&3;
 if((pathLen>>sz)>MAX_PATH_SIZE||(pathLen%(1<<sz))!=0)return error(out,ERR_ILLEGAL_ARG);
 uint32_t tag,auth;memcpy(&tag,in+1,4);memcpy(&auth,in+5,4);
 mesh::Packet *pkt=node.createTrace(tag,auth,flags);if(!pkt)return error(out,ERR_TABLE_FULL);
 node.sendDirect(pkt,in+10,pathLen);
 uint32_t timeout=node.traceTimeout(int(pkt->payload_len+pkt->path_len+2),uint8_t(pathLen>>sz));
 out[0]=companion_air::RESP_SENT;out[1]=0;memcpy(out+2,&tag,4);memcpy(out+6,&timeout,4);return 10;
}
}
// One app frame for opcodes 26 27 28 29 36 39(remote) 50 52 in, the direct reply (RESP_SENT / OK / error) out.
template<class M> size_t command(M &node,Pending &p,const uint8_t *in,size_t len,uint8_t *out){
 using namespace detail;
 switch(in[0]){
 case CMD_SEND_LOGIN:if(len>=1+KEY)return login(node,p,in,len,out);break;
 case CMD_SEND_STATUS_REQ:if(len>=1+KEY)return request(node,p,in+1,in[0],nullptr,0,out);break;
 case CMD_SEND_TELEMETRY_REQ:if(len>=4+KEY)return request(node,p,in+4,in[0],nullptr,0,out);break;
 case CMD_SEND_BINARY_REQ:if(len>=2+KEY)return request(node,p,in+1,in[0],in+1+KEY,len-1-KEY,out);break;
 case CMD_SEND_PATH_DISCOVERY_REQ:if(len>=2+KEY&&in[1]==0)return discovery(node,p,in,out);break;
 case CMD_HAS_CONNECTION:if(len>=1+KEY){if(node.hasConnectionTo(in+1)){out[0]=companion_air::RESP_OK;return 1;}return error(out,ERR_NOT_FOUND);}break;
 case CMD_LOGOUT:if(len>=1+KEY){node.stopConnection(in+1);out[0]=companion_air::RESP_OK;return 1;}break;
 case CMD_SEND_TRACE_PATH:if(len>10&&len-10<MAX_PACKET_PAYLOAD-5)return trace(node,in,len,out);break;
 }
 return error(out,ERR_UNSUPPORTED);
}
// CMD_SEND_TXT_MSG {2}{txt_type}{attempt}{app time 4}{recipient prefix 6}{text} with txt_type 1 or 3: sent as command data
// stamped with the node's clock (the app's time would trip the server's replay check), no ack expected.
template<class M> size_t sendCommand(M &node,const uint8_t *in,size_t len,uint8_t *out){
 if(len<14)return error(out,ERR_UNSUPPORTED);uint8_t type=in[1];ContactInfo *to=node.lookupContactByPubKey(in+7,6);
 if(!to)return error(out,ERR_NOT_FOUND);if(type!=TXT_TYPE_CLI_DATA&&type!=TXT_TYPE_CLI_COMMAND)return error(out,ERR_UNSUPPORTED);
 char text[256];size_t n=len-13;if(n>=sizeof(text))return error(out,ERR_TABLE_FULL);memcpy(text,in+13,n);text[n]=0;
 uint32_t timeout=0;int result=node.sendCommandData(*to,node.getRTCClock()->getCurrentTimeUnique(),in[2],type,text,timeout);
 if(result==MSG_SEND_FAILED)return error(out,ERR_TABLE_FULL);
 return sent(out,result,0,timeout);// expected ack 0
}
// A response {tag 4}{data} from a contact: the push for the app when it answers the pending request, else 0.
// A server's login answer starts a keep-alive connection when it asks for one (node.startConnection).
template<class M> size_t responseFrame(M &node,Pending &p,const ContactInfo &c,const uint8_t *data,uint8_t len,uint8_t *out,size_t cap){
 if(len<=4)return 0;uint32_t tag;memcpy(&tag,data,4);const uint8_t *key=c.id.pub_key;
 if(p.login&&!memcmp(&p.login,key,4)){
  size_t i=0;
  if(!memcmp(data+4,"OK",2)){out[i++]=PUSH_LOGIN_SUCCESS;out[i++]=0;memcpy(out+i,key,6);i+=6;}// legacy repeater
  else if(data[4]==RESP_SERVER_LOGIN_OK){
   if(len<13)return 0;uint16_t keepAlive=uint16_t(data[5])*16;if(keepAlive>0)node.startConnection(c,keepAlive);
   out[i++]=PUSH_LOGIN_SUCCESS;out[i++]=data[6];memcpy(out+i,key,6);i+=6;memcpy(out+i,&tag,4);i+=4;out[i++]=data[7];out[i++]=data[12];}
  else{out[i++]=PUSH_LOGIN_FAIL;out[i++]=0;memcpy(out+i,key,6);i+=6;}
  p.login=0;return i;}
 bool status=p.status&&!memcmp(&p.status,key,4),telemetry=p.telemetry&&tag==p.telemetry;
 if(status||telemetry){
  if(size_t(len)+4>cap)return 0;
  out[0]=status?PUSH_STATUS_RESPONSE:PUSH_TELEMETRY_RESPONSE;out[1]=0;memcpy(out+2,key,6);memcpy(out+8,data+4,len-4);
  if(status)p.status=0;else p.telemetry=0;return size_t(len)+4;}
 return companion_air::responseFrame(p.req,data,len,out,cap);
}
// Path returned with a response (onContactPathRecv). True when it answers the pending path discovery: the caller then
// returns false (MyMesh sends no reciprocal path and the base class keeps the old one); n is the push length, 0 if the
// path sizes are invalid or out of room.
inline bool pathResponse(Pending &p,const ContactInfo &c,const uint8_t *inPath,uint8_t inLen,const uint8_t *outPath,uint8_t outLen,uint8_t extraType,const uint8_t *extra,uint8_t extraLen,uint8_t *out,size_t cap,size_t &n){
 n=0;uint32_t tag;if(extraType!=PAYLOAD_TYPE_RESPONSE||extraLen<=4)return false;memcpy(&tag,extra,4);if(!p.discovery||tag!=p.discovery)return false;
 p.discovery=0;if(!mesh::Packet::isValidPathLen(inLen)||!mesh::Packet::isValidPathLen(outLen))return true;
 auto bytes=[](uint8_t l){return size_t(l&63)*((l>>6)+1);};if(8+bytes(outLen)+1+bytes(inLen)>cap)return true;
 size_t i=0;out[i++]=PUSH_PATH_DISCOVERY_RESPONSE;out[i++]=0;memcpy(out+i,c.id.pub_key,6);i+=6;
 out[i++]=outLen;i+=mesh::Packet::writePath(out+i,outPath,outLen);out[i++]=inLen;i+=mesh::Packet::writePath(out+i,inPath,inLen);
 n=i;return true;// telemetry in 'extra' is discarded, as MyMesh
}
// Mesh::onTraceRecv: {0x89}{0}{path bytes}{flags}{tag 4}{auth 4}{hashes}{snr per hop}{snr to this node}. cap = the buffer size.
inline size_t traceFrame(const mesh::Packet *packet,uint32_t tag,uint32_t auth,uint8_t flags,const uint8_t *snrs,const uint8_t *hashes,uint8_t pathLen,uint8_t *out,size_t cap){
 uint8_t sz=flags&3;if(12+pathLen+(pathLen>>sz)+1>cap)return 0;size_t i=0;
 out[i++]=PUSH_TRACE_DATA;out[i++]=0;out[i++]=pathLen;out[i++]=flags;memcpy(out+i,&tag,4);i+=4;memcpy(out+i,&auth,4);i+=4;
 memcpy(out+i,hashes,pathLen);i+=pathLen;memcpy(out+i,snrs,pathLen>>sz);i+=pathLen>>sz;out[i++]=uint8_t(int8_t(packet->getSNR()*4));return i;
}
// MyMesh::queueMessage: the frame CMD_SYNC_NEXT_MESSAGE returns for a message from a contact. v3: app version >= 3.
// extra is the signed post's 4-byte author prefix. cap = MAX_FRAME_SIZE; long text is cut to fit. 0 if header and extra do not fit.
inline size_t contactMsgFrame(bool v3,const mesh::Packet *pkt,const uint8_t *fromKey,uint8_t txtType,uint32_t senderTime,const uint8_t *extra,int extraLen,const char *text,uint8_t *out,size_t cap){
 size_t i=0;if((v3?4:0)+1+6+1+1+4+size_t(extraLen)>cap)return 0;
 if(v3){out[i++]=RESP_CONTACT_MSG_RECV_V3;out[i++]=uint8_t(int8_t(pkt->getSNR()*4));out[i++]=0;out[i++]=0;}else out[i++]=RESP_CONTACT_MSG_RECV;
 memcpy(out+i,fromKey,6);i+=6;out[i++]=pkt->isRouteFlood()?pkt->path_len:0xFF;out[i++]=txtType;memcpy(out+i,&senderTime,4);i+=4;
 if(extraLen>0){memcpy(out+i,extra,extraLen);i+=extraLen;}
 size_t tlen=strlen(text);if(i+tlen>cap)tlen=cap-i;memcpy(out+i,text,tlen);return i+tlen;
}
}
