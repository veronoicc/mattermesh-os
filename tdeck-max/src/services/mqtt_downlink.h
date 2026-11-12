#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <map>
#include <string>
#include <vector>
#include "services/mqtt_uplink.h"
#include "mesh/message_limits.h"
// MQTT downlink: JSON commands in (meshcore/<REGION>/<PUBKEY>/cmd), replies out (.../resp) and the readable chat traffic out
// (.../messages). Pure (no Arduino): the reader, token check, rate limit, dispatch and formatters are shared by services/mqtt.cpp,
// the main loop's executor (mqtt_downlink_app.inc) and the host tests; README documents the wire format.
namespace mqtt_downlink {
constexpr size_t PayloadMax=1024,TokenMin=8,TokenMax=64,TokenLen=24,IdMax=64;
using mqtt_uplink::detail::esc;
// Shared secret compare that does not stop at the first differing byte.
inline bool constantTimeEquals(const std::string &a,const std::string &b){size_t n=a.size()>b.size()?a.size():b.size();uint8_t d=a.size()!=b.size();for(size_t i=0;i<n;i++)d|=uint8_t((i<a.size()?a[i]:0)^(i<b.size()?b[i]:0));return d==0;}
// 24 characters of a 32 letter alphabet (no 0/O/1/I): 120 bits, and 32 divides 2^32 so every character is unbiased. rng returns 32 random bits.
template<class Rng> std::string generateToken(Rng rng){static const char alpha[]="23456789ABCDEFGHJKLMNPQRSTUVWXYZ";std::string s;for(size_t i=0;i<TokenLen;i++)s+=alpha[rng()&31];return s;}
inline bool validToken(const std::string &s){if(s.size()<TokenMin||s.size()>TokenMax)return false;for(char c:s)if(uint8_t(c)<=32||uint8_t(c)>=127||c=='"'||c=='\\')return false;return true;}
// At most `max` commands in any `windowMs` (sliding). A refused attempt is not remembered, so waiting always recovers.
class RateLimiter {
 static constexpr size_t Cap=16;uint32_t at_[Cap]={};size_t head_=0,n_=0,max_,window_;
public:
 explicit RateLimiter(size_t max=10,uint32_t windowMs=60000):max_(max>Cap?Cap:max),window_(windowMs){}
 bool allow(uint32_t now){while(n_&&uint32_t(now-at_[head_])>=window_){head_=(head_+1)%Cap;n_--;}
  if(n_>=max_)return false;at_[(head_+n_)%Cap]=now;n_++;return true;}
};
// A value of the command object: type s(tring) n(umber, kept as text) b(ool) z(null) o(bject/array, skipped).
struct Value {char type='z';std::string s;bool b=false;};
namespace detail {
struct Reader {
 const std::string &t;size_t i=0;explicit Reader(const std::string &text):t(text){}
 void ws(){while(i<t.size()&&(t[i]==' '||t[i]=='\t'||t[i]=='\n'||t[i]=='\r'))i++;}
 bool lit(const char *w){size_t n=0;while(w[n])n++;if(t.compare(i,n,w)!=0)return false;i+=n;return true;}
 static void utf8(std::string &o,uint32_t c){if(c<0x80)o+=char(c);else if(c<0x800){o+=char(0xc0|c>>6);o+=char(0x80|(c&63));}else if(c<0x10000){o+=char(0xe0|c>>12);o+=char(0x80|(c>>6&63));o+=char(0x80|(c&63));}else{o+=char(0xf0|c>>18);o+=char(0x80|(c>>12&63));o+=char(0x80|(c>>6&63));o+=char(0x80|(c&63));}}
 bool hex4(uint32_t &v){if(i+4>t.size())return false;v=0;for(int k=0;k<4;k++){char c=t[i++];v<<=4;if(c>='0'&&c<='9')v|=c-'0';else if(c>='a'&&c<='f')v|=c-'a'+10;else if(c>='A'&&c<='F')v|=c-'A'+10;else return false;}return true;}
 bool str(std::string &o){if(i>=t.size()||t[i]!='"')return false;i++;o.clear();
  while(i<t.size()){uint8_t c=uint8_t(t[i++]);if(c=='"')return true;if(c<0x20)return false;if(c!='\\'){o+=char(c);continue;}
   if(i>=t.size())return false;char e=t[i++];
   switch(e){case '"':o+='"';break;case '\\':o+='\\';break;case '/':o+='/';break;case 'b':o+='\b';break;case 'f':o+='\f';break;case 'n':o+='\n';break;case 'r':o+='\r';break;case 't':o+='\t';break;
    case 'u':{uint32_t v;if(!hex4(v))return false;if(v>=0xd800&&v<0xdc00){uint32_t lo;if(i+1>=t.size()||t[i]!='\\'||t[i+1]!='u')return false;i+=2;if(!hex4(lo)||lo<0xdc00||lo>=0xe000)return false;v=0x10000+((v-0xd800)<<10)+(lo-0xdc00);}else if(v>=0xdc00&&v<0xe000)return false;utf8(o,v);break;}
    default:return false;}}
  return false;}
 bool value(Value &v,int depth){ws();if(i>=t.size())return false;char c=t[i];
  if(c=='"'){v.type='s';return str(v.s);}
  if(c=='t'){v.type='b';v.b=true;return lit("true");}if(c=='f'){v.type='b';v.b=false;return lit("false");}if(c=='n'){v.type='z';return lit("null");}
  if(c=='-'||(c>='0'&&c<='9')){size_t a=i;while(i<t.size()&&(t[i]=='+'||t[i]=='-'||t[i]=='.'||t[i]=='e'||t[i]=='E'||(t[i]>='0'&&t[i]<='9')))i++;v.type='n';v.s=t.substr(a,i-a);return true;}
  if(c=='{'||c=='['){v.type='o';return skip(depth);}return false;}
 // Nested containers are validated and ignored; no command needs them.
 bool skip(int depth){if(depth>=8)return false;char open=t[i++],close=open=='{'?'}':']';ws();if(i<t.size()&&t[i]==close){i++;return true;}
  for(;;){ws();if(open=='{'){std::string k;if(!str(k))return false;ws();if(i>=t.size()||t[i++]!=':')return false;}
   Value v;if(!value(v,depth+1))return false;ws();if(i>=t.size())return false;char d=t[i++];if(d==close)return true;if(d!=',')return false;}}
};
inline std::string trim(const std::string &s){size_t a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");return a==std::string::npos?std::string():s.substr(a,b-a+1);}
}
// One JSON object (nothing after it) into its top-level members; false for anything malformed. A repeated key keeps the last value.
inline bool parseObject(const std::string &text,std::map<std::string,Value> &out){
 out.clear();detail::Reader r(text);r.ws();if(r.i>=text.size()||text[r.i]!='{')return false;r.i++;r.ws();
 if(r.i<text.size()&&text[r.i]=='}')r.i++;
 else for(;;){r.ws();std::string k;if(!r.str(k))return false;r.ws();if(r.i>=text.size()||text[r.i++]!=':')return false;Value v;if(!r.value(v,1))return false;out[k]=v;
  r.ws();if(r.i>=text.size())return false;char d=text[r.i++];if(d=='}')break;if(d!=',')return false;}
 r.ws();return r.i==text.size();}
// Reply: {"id":...,"ok":...,"error":...,"result":...}; idJson is already JSON (a quoted string, a number or null), result is JSON.
inline std::string responseRaw(const std::string &idJson,bool ok,const std::string &error,const std::string &result){
 std::string s="{\"id\":"+idJson+",\"ok\":"+(ok?"true":"false");if(!error.empty())s+=",\"error\":\""+esc(error)+"\"";if(!result.empty())s+=",\"result\":"+result;return s+"}";}
inline std::string response(const std::string &id,bool ok,const std::string &error,const std::string &result){return responseRaw("\""+esc(id)+"\"",ok,error,result);}
// Implemented by the firmware (main loop): the same send path as the UI, so the journal, delivery tracking and the chat view stay consistent.
struct Host {
 virtual ~Host(){}
 virtual bool sendChannel(const std::string &channel,const std::string &text,std::string &result,std::string &err)=0;// channel: name or slot number
 virtual bool sendDm(const std::string &to,const std::string &text,std::string &result,std::string &err)=0;// to: contact name or public key prefix
 virtual bool advert(bool position,std::string &err)=0;
 virtual std::string status()=0;virtual std::string listChannels()=0;virtual std::string listContacts()=0;// result JSON
};
// One received payload to its reply. Order: rate limit (every payload counts), size, JSON, token, id, command, fields, "Allow sending", host.
// Unauthenticated callers only ever learn "rate limited", "malformed JSON", "payload too large", "missing token" or "unauthorized".
inline std::string handle(const std::string &payload,const std::string &secret,bool allowSend,RateLimiter &limit,uint32_t nowMs,Host &host){
 auto fail=[](const std::string &id,const std::string &why){return responseRaw(id,false,why,"");};
 if(!limit.allow(nowMs))return fail("null","rate limited");
 if(payload.size()>PayloadMax)return fail("null","payload too large");
 std::map<std::string,Value> m;if(!parseObject(payload,m))return fail("null","malformed JSON");
 auto str=[&](const char *k,std::string &out){auto it=m.find(k);if(it==m.end()||it->second.type!='s')return false;out=it->second.s;return true;};
 std::string idJson="null",id;{auto it=m.find("id");if(it!=m.end()){if(it->second.type=='s'&&!it->second.s.empty()&&it->second.s.size()<=IdMax){id=it->second.s;idJson="\""+esc(id)+"\"";}else if(it->second.type=='n'&&it->second.s.size()<=20){id=it->second.s;idJson=id;}}}
 std::string token;if(!str("token",token))return fail(idJson,"missing token");
 if(secret.empty()||!constantTimeEquals(token,secret))return fail(idJson,"unauthorized");
 if(id.empty())return fail("null","missing id");
 std::string cmd,err,result;str("cmd",cmd);
 bool send=cmd=="send_channel"||cmd=="send_dm";
 if(!send&&cmd!="advert"&&cmd!="ping"&&cmd!="list_channels"&&cmd!="list_contacts")return fail(idJson,"unknown command");
 std::string target,text;
 if(send){const char *key=cmd=="send_channel"?"channel":"to";auto it=m.find(key);
  if(it!=m.end()&&(it->second.type=='s'||it->second.type=='n'))target=detail::trim(it->second.s);if(target.empty())return fail(idJson,std::string("missing ")+key);
  if(!str("text",text))return fail(idJson,"missing text");if(detail::trim(text).empty())return fail(idJson,"empty text");
  if(text.size()>matter::MESSAGE_MAX_TEXT_BYTES)return fail(idJson,matter::messageLimitError(matter::MESSAGE_MAX_TEXT_BYTES));}
 bool position=false;if(cmd=="advert"){auto it=m.find("position");if(it!=m.end()){if(it->second.type!='b')return fail(idJson,"position must be true or false");position=it->second.b;}}
 if((send||cmd=="advert")&&!allowSend)return fail(idJson,"sending is disabled");
 bool ok=true;
 if(cmd=="send_channel")ok=host.sendChannel(target,text,result,err);else if(cmd=="send_dm")ok=host.sendDm(target,text,result,err);else if(cmd=="advert")ok=host.advert(position,err);
 else if(cmd=="ping")result=host.status();else if(cmd=="list_channels")result=host.listChannels();else result=host.listContacts();
 return ok?responseRaw(idJson,true,"",result):fail(idJson,err.empty()?"failed":err);}
// {"items":[...],"count":N,"truncated":bool} that never grows past capBytes: add() refuses the item that would not fit.
class ListBuilder {
 std::string items_;size_t cap_;int n_=0;bool cut_=false;
public:
 explicit ListBuilder(size_t capBytes):cap_(capBytes){}
 bool add(const std::string &item){if(items_.size()+item.size()+1>cap_){cut_=true;return false;}if(n_)items_+=",";items_+=item;n_++;return true;}
 std::string done()const{return "{\"items\":["+items_+"],\"count\":"+std::to_string(n_)+",\"truncated\":"+(cut_?"true":"false")+"}";}
};
// One chat event for .../messages. event "message": a chat message (direction in/out); event "delivery": a state change of a sent one.
struct Message {
 std::string event="message";uint32_t token=0,ts=0;bool direct=false,out=false;
 std::string channel;int channelIndex=-1;// channel chats
 std::string contact,pubkey;// direct chats: name and public key (hex, any case; only a 12 character prefix is published)
 std::string sender,text,path,state,heard,firstRepeater;int hops=-1,repeaters=-1;bool hasSignal=false;int snrTenths=0,rssiTenths=0;
};
inline std::string messageJson(const Message &m){
 using namespace mqtt_uplink::detail;
 char id[9];snprintf(id,sizeof id,"%08x",unsigned(m.token));
 std::string s=std::string("{\"id\":\"")+id+"\",\"event\":\""+esc(m.event)+"\"";if(m.ts)s+=",\"ts\":"+std::to_string(m.ts);
 if(m.event=="delivery"){s+=",\"state\":\""+esc(m.state)+"\"";if(m.repeaters>=0)s+=",\"repeaters\":"+std::to_string(m.repeaters);if(!m.heard.empty())s+=",\"heard\":\""+esc(m.heard)+"\"";if(!m.firstRepeater.empty())s+=",\"first_repeater\":\""+esc(m.firstRepeater)+"\"";return s+"}";}
 if(m.direct){s+=",\"kind\":\"dm\",\"contact\":\""+esc(m.contact)+"\"";if(!m.pubkey.empty())s+=",\"pubkey\":\""+lower(m.pubkey.substr(0,12))+"\"";}
 else{s+=",\"kind\":\"channel\",\"channel\":\""+esc(m.channel)+"\"";if(m.channelIndex>=0)s+=",\"channel_index\":"+std::to_string(m.channelIndex);}
 s+=",\"sender\":\""+esc(m.sender)+"\",\"text\":\""+esc(m.text)+"\",\"direction\":\""+(m.out?"out":"in")+"\"";
 if(m.hops>=0)s+=",\"hops\":"+std::to_string(m.hops);if(!m.path.empty())s+=",\"route\":\""+esc(m.path)+"\"";
 if(m.hasSignal)s+=",\"snr\":"+tenths(m.snrTenths)+",\"rssi\":"+tenths(m.rssiTenths);
 if(!m.state.empty())s+=",\"state\":\""+esc(m.state)+"\"";return s+"}";}
// A channel or contact named by the caller. resolve: index of the exact name (any case); else of the single entry whose key (hex) starts with spec
// (at least prefixMin characters); -1 none, -2 ambiguous. A name always wins over a key prefix that spells it.
struct Named {std::string name,key;};
inline int resolve(const std::vector<Named> &list,const std::string &spec,size_t prefixMin){
 if(spec.empty())return -1;using mqtt_uplink::detail::lower;std::string want=lower(spec);int found=-1;
 for(size_t i=0;i<list.size();i++)if(lower(list[i].name)==want){if(found>=0)return -2;found=int(i);}
 if(found>=0||spec.size()<prefixMin)return found;
 for(size_t i=0;i<list.size();i++)if(!list[i].key.empty()&&lower(list[i].key).compare(0,want.size(),want)==0&&list[i].key.size()>=want.size()){if(found>=0)return -2;found=int(i);}
 return found;}
}
