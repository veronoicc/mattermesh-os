#pragma once
#include <cstddef>
#include <string>
#include <vector>
// Pure parsers/formatters for SIMCom A76xx AT replies (no Arduino, no I/O); every function tolerates garbage and returns false/empty on mismatch.
namespace modem_at {
inline std::string trim(const std::string &s){size_t a=0,b=s.size();while(a<b&&(s[a]==' '||s[a]=='\t'||s[a]=='\r'||s[a]=='\n'))a++;while(b>a&&(s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\r'||s[b-1]=='\n'))b--;return s.substr(a,b-a);}
inline std::string lower(std::string s){for(char &c:s)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return s;}
inline bool startsWith(const std::string &s,const char *p){size_t n=0;while(p[n])n++;return s.compare(0,n,p)==0;}
// strict decimal int: optional '-', 1..9 digits, nothing else (after trimming)
inline bool toInt(const std::string &f,int &v){std::string s=trim(f);size_t i=0;bool neg=false;if(i<s.size()&&s[i]=='-'){neg=true;i++;}if(i>=s.size()||s.size()-i>9)return false;int r=0;for(;i<s.size();i++){if(s[i]<'0'||s[i]>'9')return false;r=r*10+(s[i]-'0');}v=neg?-r:r;return true;}
// text after "<prefix>:" of a trimmed line; false when the line does not start with prefix+':'
inline bool afterPrefix(const std::string &line,const char *prefix,std::string &rest){std::string t=trim(line);if(!startsWith(t,prefix))return false;size_t n=std::string(prefix).size();if(n>=t.size()||t[n]!=':')return false;rest=trim(t.substr(n+1));return true;}
// comma split that respects double quotes; fields trimmed, quotes kept
inline std::vector<std::string> splitFields(const std::string &s){std::vector<std::string> v;std::string cur;bool q=false;for(char c:s){if(c=='"')q=!q;if(c==','&&!q){v.push_back(trim(cur));cur.clear();}else cur+=c;}v.push_back(trim(cur));return v;}
inline bool unquote(const std::string &f,std::string &out){if(f.size()<2||f.front()!='"'||f.back()!='"')return false;out=f.substr(1,f.size()-2);return true;}
inline std::vector<std::string> splitLines(const std::string &s){std::vector<std::string> v;std::string cur;for(size_t i=0;i<s.size();i++){char c=s[i];if(c=='\r'||c=='\n'){if(c=='\r'&&i+1<s.size()&&s[i+1]=='\n')i++;v.push_back(cur);cur.clear();}else cur+=c;}if(!cur.empty())v.push_back(cur);return v;}

inline bool parseCsq(const std::string &line,int &rssi,int &ber){std::string r;if(!afterPrefix(line,"+CSQ",r))return false;auto f=splitFields(r);int a,b;if(f.size()!=2||!toInt(f[0],a)||!toInt(f[1],b))return false;rssi=a;ber=b;return true;}
inline int csqBars(int rssi){if(rssi<=0||rssi>31)return 0;if(rssi<10)return 1;if(rssi<15)return 2;if(rssi<20)return 3;return 4;}
inline int csqDbm(int rssi){if(rssi<0||rssi>31)return 0;return -113+2*rssi;}

inline bool parseReg(const std::string &line,const char *prefix,int &stat,int &act){std::string r;if(!afterPrefix(line,prefix,r))return false;auto f=splitFields(r);int s=0,t=-1,x;
  if(f.size()>=2&&f[1].empty())return false;
  bool query=f.size()>=2&&f[1][0]!='"';// URC: <stat>[,<lac>,<ci>[,<act>]] (quoted lac); query: <n>,<stat>[,<lac>,<ci>[,<act>]]
  size_t si=query?1:0,ai=query?4:3;
  if(si>=f.size()||!toInt(f[si],s))return false;
  if(ai<f.size()&&toInt(f[ai],x))t=x;
  stat=s;act=t;return true;}
inline bool regOk(int stat){return stat==1||stat==5;}
inline bool regRoaming(int stat){return stat==5;}
inline bool regDenied(int stat){return stat==3;}
inline bool regSearching(int stat){return stat==2;}
inline int ratClass(int act){switch(act){case 0:case 1:case 3:return 1;case 2:case 4:case 5:case 6:return 2;case 7:case 8:case 9:case 10:case 11:case 12:case 13:return 3;default:return 0;}}

inline bool parseCops(const std::string &line,std::string &oper,int &act){std::string r;if(!afterPrefix(line,"+COPS",r))return false;size_t a=r.find('"');if(a==std::string::npos)return false;size_t b=r.find('"',a+1);if(b==std::string::npos)return false;std::string name=r.substr(a+1,b-a-1);int t=-1,x;size_t c=r.find(',',b+1);if(c!=std::string::npos&&toInt(r.substr(c+1),x))t=x;oper=name;act=t;return true;}

enum class Cpin {Unknown,Ready,Pin,Puk,Pin2,Puk2,Other};
inline Cpin parseCpin(const std::string &line){std::string r;if(!afterPrefix(line,"+CPIN",r))return Cpin::Unknown;r=lower(r);if(r=="ready")return Cpin::Ready;if(r=="sim pin")return Cpin::Pin;if(r=="sim puk")return Cpin::Puk;if(r=="sim pin2")return Cpin::Pin2;if(r=="sim puk2")return Cpin::Puk2;return Cpin::Other;}

inline bool parseCmeError(const std::string &line,int &code){std::string r;if(!afterPrefix(line,"+CME ERROR",r))return false;int n;if(toInt(r,n)){code=n;return true;}r=lower(r);
  code=r=="sim not inserted"?10:r=="sim pin required"?11:r=="sim puk required"?12:r=="sim failure"?13:r=="sim busy"?14:r=="sim wrong"?15:r=="incorrect password"?16:0;return true;}
inline bool simMissing(int cmeCode){return cmeCode==10||cmeCode==13;}

enum class Model {Unknown,A7682E,A7670G,Other};
// value of the "Model:" line (case-insensitive key), "" when absent
inline std::string modelValue(const std::string &reply){for(const std::string &l:splitLines(reply)){std::string t=trim(l);if(startsWith(lower(t),"model:")){return trim(t.substr(6));}}return "";}
inline Model parseModel(const std::string &reply){std::string l=lower(reply);if(l.find("a7682e")!=std::string::npos)return Model::A7682E;if(l.find("a7670g")!=std::string::npos)return Model::A7670G;return modelValue(reply).empty()?Model::Unknown:Model::Other;}
inline std::string modelLabel(const std::string &reply){Model m=parseModel(reply);if(m==Model::A7682E)return "A7682E";if(m==Model::A7670G)return "A7670G";if(m==Model::Other)return modelValue(reply);return lower(reply).find("simcom")!=std::string::npos?"SIMCom":"";}

// ---- UTF-8 / UTF-16 ----
// decodes one code point at s[i]; invalid, overlong, surrogate or >10FFFF sequences give U+FFFD and consume one byte
inline unsigned nextCp(const std::string &s,size_t &i){unsigned char c=(unsigned char)s[i];int n=0;unsigned cp=0,min=0;
  if(c<0x80){i++;return c;}
  if(c>=0xC2&&c<=0xDF){n=1;cp=c&0x1F;min=0x80;}else if(c>=0xE0&&c<=0xEF){n=2;cp=c&0x0F;min=0x800;}else if(c>=0xF0&&c<=0xF4){n=3;cp=c&0x07;min=0x10000;}else{i++;return 0xFFFD;}
  if(i+(size_t)n>=s.size()){i++;return 0xFFFD;}
  for(int k=1;k<=n;k++){unsigned char d=(unsigned char)s[i+k];if((d&0xC0)!=0x80){i++;return 0xFFFD;}cp=(cp<<6)|(d&0x3F);}
  if(cp<min||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF)){i++;return 0xFFFD;}
  i+=n+1;return cp;}
inline std::vector<unsigned> codePoints(const std::string &s){std::vector<unsigned> v;for(size_t i=0;i<s.size();)v.push_back(nextCp(s,i));return v;}
inline void appendUtf8(std::string &o,unsigned cp){if(cp<0x80)o+=char(cp);else if(cp<0x800){o+=char(0xC0|(cp>>6));o+=char(0x80|(cp&0x3F));}else if(cp<0x10000){o+=char(0xE0|(cp>>12));o+=char(0x80|((cp>>6)&0x3F));o+=char(0x80|(cp&0x3F));}else{o+=char(0xF0|(cp>>18));o+=char(0x80|((cp>>12)&0x3F));o+=char(0x80|((cp>>6)&0x3F));o+=char(0x80|(cp&0x3F));}}
inline std::string ucs2Hex(const std::string &utf8){static const char *H="0123456789ABCDEF";std::string o;auto unit=[&](unsigned u){o+=H[(u>>12)&15];o+=H[(u>>8)&15];o+=H[(u>>4)&15];o+=H[u&15];};
  for(unsigned cp:codePoints(utf8)){if(cp>=0x10000){cp-=0x10000;unit(0xD800|(cp>>10));unit(0xDC00|(cp&0x3FF));}else unit(cp);}return o;}
inline int hexVal(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
inline bool utf8FromUcs2Hex(const std::string &hex,std::string &utf8){if(hex.size()%4)return false;std::vector<unsigned> u;for(size_t i=0;i<hex.size();i+=4){unsigned v=0;for(int k=0;k<4;k++){int h=hexVal(hex[i+k]);if(h<0)return false;v=(v<<4)|unsigned(h);}u.push_back(v);}
  std::string o;for(size_t i=0;i<u.size();i++){unsigned v=u[i];if(v>=0xD800&&v<=0xDBFF){if(i+1>=u.size()||u[i+1]<0xDC00||u[i+1]>0xDFFF)return false;v=0x10000+((v-0xD800)<<10)+(u[i+1]-0xDC00);i++;}else if(v>=0xDC00&&v<=0xDFFF)return false;appendUtf8(o,v);}
  utf8=o;return true;}

// ---- GSM 03.38 ----
inline bool gsm7Basic(unsigned cp){if(cp==0x60)return false;if(cp>=0x20&&cp<=0x7E)return true;switch(cp){case '\n':case '\r':case 0xA3:case 0xA5:case 0xE8:case 0xE9:case 0xF9:case 0xEC:case 0xF2:case 0xC7:case 0xD8:case 0xF8:case 0xC5:case 0xE5:case 0x394:case 0x3A6:case 0x393:case 0x39B:case 0x3A9:case 0x3A0:case 0x3A8:case 0x3A3:case 0x398:case 0x39E:case 0xC6:case 0xE6:case 0xDF:case 0xC9:case 0xA4:case 0xA1:case 0xC4:case 0xD6:case 0xD1:case 0xDC:case 0xA7:case 0xBF:case 0xE4:case 0xF6:case 0xF1:case 0xFC:case 0xE0:return true;default:return false;}}
inline bool gsm7Ext(unsigned cp){switch(cp){case '^':case '{':case '}':case '\\':case '[':case '~':case ']':case '|':case 0x20AC:case 0x0C:return true;default:return false;}}
inline bool gsm7Representable(const std::string &utf8){for(unsigned cp:codePoints(utf8))if(!gsm7Basic(cp)&&!gsm7Ext(cp))return false;return true;}
inline size_t smsUnits(const std::string &utf8){bool g=gsm7Representable(utf8);size_t n=0;for(unsigned cp:codePoints(utf8))n+=g?(gsm7Ext(cp)?2:1):(cp>=0x10000?2:1);return n;}
inline size_t smsLimit(const std::string &utf8){return gsm7Representable(utf8)?160:70;}
inline bool validSmsNumber(const std::string &s){size_t i=s.size()&&s[0]=='+'?1:0;size_t n=s.size()-i;if(n<3||n>20)return false;for(;i<s.size();i++)if(s[i]<'0'||s[i]>'9')return false;return true;}
inline std::string smsTimeLabel(const std::string &scts){std::string t=trim(scts);if(t.size()<17)return "";static const char *m="dd/dd/dd,dd:dd:dd";for(int i=0;i<17;i++){if(m[i]=='d'?(t[i]<'0'||t[i]>'9'):t[i]!=m[i])return "";}if(t.size()>17&&t[17]!='+'&&t[17]!='-')return "";return t.substr(6,2)+"/"+t.substr(3,2)+" "+t.substr(9,5);}
inline bool atSafe(const std::string &s){for(unsigned char c:s)if(c=='"'||c=='\\'||c<32||c==127)return false;return true;}

// ---- SMS text-mode replies ----
struct ParsedSms {int index=0;bool unread=false;std::string number,scts,text;int dcs=-1;};
inline std::string smsText(const std::string &v,bool ucs2){std::string d;if(ucs2&&!v.empty()&&v.size()%4==0&&utf8FromUcs2Hex(v,d))return d;return v;}
inline std::string smsField(const std::string &f,bool ucs2){std::string v;if(!unquote(f,v))v=f;return smsText(v,ucs2);}
inline bool smsStat(const std::string &f,bool &unread){std::string s;if(!unquote(f,s))return false;unread=s=="REC UNREAD";return true;}
// body lines [b,e) joined by '\n'; trailing blank lines and (when last) the final OK are dropped
inline std::string smsBody(const std::vector<std::string> &L,size_t b,size_t e,bool last,bool ucs2){auto blank=[&](size_t i){return trim(L[i]).empty();};while(e>b&&blank(e-1))e--;if(last&&e>b&&trim(L[e-1])=="OK")e--;while(e>b&&blank(e-1))e--;std::string o;for(size_t i=b;i<e;i++){if(i>b)o+='\n';o+=L[i];}return smsText(o,ucs2);}
inline bool isCmglHeader(const std::string &l){std::string r;return afterPrefix(l,"+CMGL",r);}
inline std::vector<ParsedSms> parseCmglReply(const std::string &reply,bool ucs2){std::vector<ParsedSms> out;auto L=splitLines(reply);
  for(size_t i=0;i<L.size();i++){std::string r;if(!afterPrefix(L[i],"+CMGL",r))continue;size_t e=i+1;while(e<L.size()&&!isCmglHeader(L[e]))e++;
    auto f=splitFields(r);ParsedSms m;int idx;
    if(f.size()>=5&&toInt(f[0],idx)&&smsStat(f[1],m.unread)){m.index=idx;m.number=smsField(f[2],ucs2);unquote(f[4],m.scts);m.text=smsBody(L,i+1,e,e==L.size(),ucs2);out.push_back(m);}
    i=e-1;}
  return out;}
inline bool parseCmgrReply(const std::string &reply,bool ucs2,ParsedSms &m){auto L=splitLines(reply);
  for(size_t i=0;i<L.size();i++){std::string r;if(!afterPrefix(L[i],"+CMGR",r))continue;auto f=splitFields(r);ParsedSms p;
    if(f.size()<4||!smsStat(f[0],p.unread)){return false;}
    p.number=smsField(f[1],ucs2);unquote(f[3],p.scts);int d;if(f.size()>7&&toInt(f[7],d))p.dcs=d;p.text=smsBody(L,i+1,L.size(),true,ucs2);m=p;return true;}
  return false;}
inline bool parseCmti(const std::string &line,std::string &mem,int &index){std::string r;if(!afterPrefix(line,"+CMTI",r))return false;auto f=splitFields(r);std::string m;int i;if(f.size()!=2||!unquote(f[0],m)||!toInt(f[1],i))return false;mem=m;index=i;return true;}
inline bool parseCmgs(const std::string &line,int &mr){std::string r;int v;if(!afterPrefix(line,"+CMGS",r)||!toInt(r,v))return false;mr=v;return true;}
}
