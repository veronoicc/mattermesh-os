#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
// MQTT uplink of received mesh packets in the community meshcoretomqtt format (https://github.com/Cisien/meshcoretomqtt).
// Pure (no Arduino): payload formatting, bounded queue and reconnect backoff, shared by the firmware and the host tests.
namespace mqtt_uplink {
constexpr size_t RawMax=256;
struct Packet {uint32_t epoch=0;uint16_t ms=0;int16_t snrTenths=0,rssiTenths=0;uint8_t hash[8]={};uint16_t len=0;uint8_t raw[RawMax]={};};// raw = packet as sent on air
namespace detail {
inline std::string hex(const uint8_t *b,size_t n){const char *d="0123456789ABCDEF";std::string s;s.reserve(n*2);for(size_t i=0;i<n;i++){s+=d[b[i]>>4];s+=d[b[i]&15];}return s;}
inline std::string upper(std::string s){for(char &c:s)if(c>='a'&&c<='z')c=char(c-32);return s;}
inline std::string lower(std::string s){for(char &c:s)if(c>='A'&&c<='Z')c=char(c+32);return s;}
inline std::string esc(const std::string &in){std::string o;for(char ch:in){uint8_t c=uint8_t(ch);if(c=='"'||c=='\\'){o+='\\';o+=ch;}else if(c=='\n')o+="\\n";else if(c=='\r')o+="\\r";else if(c=='\t')o+="\\t";else if(c<0x20){o+="\\u00";o+="0123456789ABCDEF"[c>>4];o+="0123456789ABCDEF"[c&15];}else o+=ch;}return o;}
inline std::string num(long v,int w=0){std::string s=std::to_string(v);while(int(s.size())<w)s.insert(s.begin(),'0');return s;}
inline std::string tenths(int t){bool neg=t<0;int a=neg?-t:t;std::string s=(neg?"-":"")+std::to_string(a/10);if(a%10)s+="."+std::to_string(a%10);return s;}
struct Civil {long y;int mo,d,h,mi,s;};
inline Civil civil(uint32_t epoch){long days=epoch/86400;int sod=int(epoch%86400);long z=days+719468,era=z/146097,doe=z-era*146097,yoe=(doe-doe/1460+doe/36524-doe/146096)/365,y=yoe+era*400,doy=doe-(365*yoe+yoe/4-yoe/100),mp=(5*doy+2)/153;int d=int(doy-(153*mp+2)/5+1),mo=int(mp<10?mp+3:mp-9);return {mo<=2?y+1:y,mo,d,sod/3600,sod/60%60,sod%60};}
inline std::string stamp(uint32_t epoch,uint16_t ms){Civil c=civil(epoch);return num(c.y,4)+"-"+num(c.mo,2)+"-"+num(c.d,2)+"T"+num(c.h,2)+":"+num(c.mi,2)+":"+num(c.s,2)+"."+num(ms%1000,3)+"000";}
}
// "meshcore/<REGION>/<PUBKEY upper-case>/<leaf>"
inline std::string topic(const std::string &region,const std::string &pubKeyHex,const char *leaf="packets"){return "meshcore/"+region+"/"+detail::upper(pubKeyHex)+"/"+leaf;}
// One packet as a meshcoretomqtt JSON line; false (out untouched) for a malformed or truncated packet.
inline bool format(const Packet &p,const std::string &origin,const std::string &pubKeyHex,std::string &out){
 if(p.len<2||p.len>RawMax)return false;
 uint8_t hd=p.raw[0];int route=hd&3;size_t i=1;
 if(route==0||route==3){if(p.len<6)return false;i=5;}
 uint8_t pl=p.raw[i++];size_t hops=pl&63,sz=size_t(pl>>6)+1,pathLen=hops*sz;
 if(i+pathLen>p.len)return false;
 static const char *const routes[4]={"TF","F","D","TD"};
 std::string path;for(size_t h=0;h<hops;h++){if(h)path+=" -> ";path+=detail::hex(p.raw+i+h*sz,sz);}
 detail::Civil c=detail::civil(p.epoch);
 std::string s="{\"origin\": \""+detail::esc(origin)+"\", \"origin_id\": \""+detail::upper(pubKeyHex)+"\", \"timestamp\": \""+detail::stamp(p.epoch,p.ms)+"\", \"type\": \"PACKET\", \"direction\": \"rx\", \"time\": \""+detail::num(c.h,2)+":"+detail::num(c.mi,2)+":"+detail::num(c.s,2)+"\", \"date\": \""+detail::num(c.d)+"/"+detail::num(c.mo)+"/"+detail::num(c.y)+"\", \"len\": \""+std::to_string(p.len)+"\", \"packet_type\": \""+std::to_string((hd>>2)&15)+"\", \"route\": \""+routes[route]+"\", \"payload_len\": \""+std::to_string(p.len-i-pathLen)+"\", \"raw\": \""+detail::hex(p.raw,p.len)+"\", \"SNR\": \""+detail::tenths(p.snrTenths)+"\", \"RSSI\": \""+detail::tenths(p.rssiTenths)+"\", \"score\": \"1000\", \"hash\": \""+detail::hex(p.hash,8)+"\"";
 if(hops)s+=", \"path\": \""+path+"\"";
 out=s+"}";return true;}
// Status payload (also the LWT): timestamp omitted while the clock is unset.
inline std::string statusJson(bool online,const std::string &origin,const std::string &pubKeyHex,uint32_t epoch){
 std::string s=std::string("{\"status\": \"")+(online?"online":"offline")+"\", \"origin\": \""+detail::esc(origin)+"\", \"origin_id\": \""+detail::upper(pubKeyHex)+"\"";
 if(epoch)s+=", \"timestamp\": \""+detail::stamp(epoch,0)+"\"";
 return s+"}";}
// Bounded FIFO over caller storage; a full queue drops the oldest entry so an outage keeps the newest packets.
class Queue {
 Packet *buf_=nullptr;size_t cap_=0,head_=0,n_=0;uint32_t dropped_=0;
public:
 void attach(Packet *storage,size_t capacity){buf_=storage;cap_=storage?capacity:0;head_=n_=0;}
 bool push(const Packet &p){if(!cap_){dropped_++;return false;}bool ok=true;if(n_==cap_){head_=(head_+1)%cap_;n_--;dropped_++;ok=false;}buf_[(head_+n_)%cap_]=p;n_++;return ok;}
 bool pop(Packet &out){if(!n_)return false;out=buf_[head_];head_=(head_+1)%cap_;n_--;return true;}
 size_t size()const{return n_;}
 size_t capacity()const{return cap_;}
 uint32_t dropped()const{return dropped_;}
 void clear(){head_=n_=0;}
};
// Reconnect delay: 5 s after the first failure, doubling to a 5 min cap.
struct Backoff {uint32_t delayMs=0;uint32_t next(){delayMs=delayMs?(delayMs>=150000?300000:delayMs*2):5000;return delayMs;}void reset(){delayMs=0;}};
// "mm-" + first 8 hex chars of the public key, lower case.
inline std::string clientId(const std::string &pubKeyHex){return "mm-"+detail::lower(pubKeyHex.substr(0,8));}
}
