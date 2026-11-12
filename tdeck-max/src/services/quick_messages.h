#pragma once
#include "../ui/utf8.h"
#include <algorithm>
#include <stddef.h>
#include <string>
#include <vector>
// Quick messages: up to eight canned chat texts, each with an optional position, stored in NVS "mmquick".
// The position is the chat's own text (device::gpsCoordinateMessage: "GPS: lat, lon", six decimals) appended after a
// space, so every client and the Share position action show the same words; compose never drops it silently.
namespace mm_quick {
constexpr size_t MaxPresets=8,MaxTextBytes=60,MaxBlobBytes=2048;
constexpr const char *ErrEmpty="Text is empty",*ErrFull="List is full (8)",*ErrIndex="No such message",*ErrNoFix="No GPS fix",*ErrTooLong="Too long",*ErrTooLongPos="Too long with position";
struct Preset {std::string text;bool position=false;Preset(std::string t=std::string(),bool p=false):text(std::move(t)),position(p){}};// constructor: the firmware builds as C++11
typedef std::vector<Preset> List;
// Defaults: none attaches the position, a message that refuses to go out without a GPS fix must not be "Need help".
inline List defaults(){return {{"OK",false},{"On my way",false},{"At camp",false},{"Need help",false},{"Call me",false},{"Where are you?",false}};}
inline size_t firstInvalid(const std::string &s){uint32_t cp;for(size_t at=0;at<s.size();){size_t next=mm::utf8Next(s,at,cp);if(cp=='?'&&s[at]!='?')return at;at=next;}return s.size();}
inline void trim(std::string &s){while(!s.empty()&&s.back()==' ')s.pop_back();size_t from=0;while(from<s.size()&&s[from]==' ')from++;s.erase(0,from);}
// Control characters become spaces, invalid UTF-8 ends the text, trimmed, cut at MaxTextBytes on a code point.
inline std::string clean(const std::string &in){
 std::string s;for(unsigned char c:in)s+=(c<32||c==127)?' ':char(c);
 s.resize(firstInvalid(s));trim(s);
 if(s.size()>MaxTextBytes){s.resize(mm::utf8Floor(s,MaxTextBytes));trim(s);}
 return s;}
// A typed byte is taken when it is printable and the text still has room (multi-byte letters arrive byte by byte; clean() drops a cut one).
inline bool accept(const std::string &text,char c){unsigned char u=c;return u>=32&&u!=127&&text.size()<MaxTextBytes;}
// List operations return "" when done or the reason.
inline std::string add(List &l,const std::string &text,bool position=false){if(l.size()>=MaxPresets)return ErrFull;std::string t=clean(text);if(t.empty())return ErrEmpty;l.push_back({t,position});return "";}
inline std::string edit(List &l,int i,const std::string &text){if(i<0||i>=int(l.size()))return ErrIndex;std::string t=clean(text);if(t.empty())return ErrEmpty;l[i].text=t;return "";}
inline bool togglePosition(List &l,int i){if(i<0||i>=int(l.size()))return false;l[i].position=!l[i].position;return true;}
inline bool remove(List &l,int i){if(i<0||i>=int(l.size()))return false;l.erase(l.begin()+i);return true;}
// The new index of the moved row; at an end (or out of range) nothing moves.
inline int moveUp(List &l,int i){if(i<=0||i>=int(l.size()))return i;std::swap(l[i],l[i-1]);return i-1;}
inline int moveDown(List &l,int i){if(i<0||i+1>=int(l.size()))return i;std::swap(l[i],l[i+1]);return i+1;}
// First visible row so `focus` shows in a window of `rows` out of `count`.
inline int scrollFor(int focus,int scroll,int rows,int count){int last=std::max(0,count-rows);if(focus<scroll)scroll=focus;else if(focus>=scroll+rows)scroll=focus-rows+1;return std::max(0,std::min(scroll,last));}
// Quick key: Alt + one of three keys whose symbol (# ) _) rarely opens a message; it acts only on an empty composer, so typing
// symbols mid-draft, Alt+Space, Alt+Shift+W/S, Alt+V (Wi-Fi password), Sym and the MIC key are untouched.
enum class Key {Off,AltQ,AltY,AltU};enum class Mode {Send,Insert};
constexpr int KeyCount=4,ModeCount=2;
inline Key keyFrom(int v){return v>=0&&v<KeyCount?Key(v):Key::Off;}
inline Mode modeFrom(int v){return v>=0&&v<ModeCount?Mode(v):Mode::Send;}
inline Key stepKey(Key k,int dir){return Key((int(k)+(dir<0?KeyCount-1:1))%KeyCount);}
inline Mode stepMode(Mode m,int dir){return Mode((int(m)+(dir<0?ModeCount-1:1))%ModeCount);}
inline char keyChar(Key k){return k==Key::AltQ?'q':k==Key::AltY?'y':k==Key::AltU?'u':0;}
inline const char *keyLabel(Key k){return k==Key::AltQ?"Alt+Q":k==Key::AltY?"Alt+Y":k==Key::AltU?"Alt+U":"Off";}
inline const char *modeLabel(Mode m){return m==Mode::Insert?"Insert":"Send now";}
// NVS string: one line per preset, '1' or '0' (position) then the text with \\ and \n escaped; "-" is an emptied list.
inline std::string serialize(const List &l){
 if(l.empty())return "-";
 std::string out;for(size_t i=0;i<l.size();i++){if(i)out+='\n';out+=l[i].position?'1':'0';for(char c:l[i].text){if(c=='\\')out+="\\\\";else if(c=='\n')out+="\\n";else out+=c;}}
 return out;}
// False (and the defaults) for anything not exactly what serialize wrote.
inline bool deserialize(const std::string &blob,List &out){
 out=defaults();if(blob.empty()||blob.size()>MaxBlobBytes)return false;
 if(blob=="-"){out.clear();return true;}
 List l;size_t at=0;
 while(true){size_t end=blob.find('\n',at);std::string line=blob.substr(at,end==std::string::npos?std::string::npos:end-at);
  if(line.size()<2||(line[0]!='0'&&line[0]!='1')||l.size()>=MaxPresets)return false;
  std::string text;for(size_t i=1;i<line.size();i++){char c=line[i];if(c!='\\'){text+=c;continue;}if(++i>=line.size())return false;if(line[i]=='\\')text+='\\';else if(line[i]=='n')text+='\n';else return false;}
  if(text!=clean(text))return false;
  l.push_back({text,line[0]=='1'});
  if(end==std::string::npos)break;at=end+1;}
 out=l;return true;}
struct Composed {bool ok=false;std::string text,error;};
// The message to send or insert: the text, plus " "+position when the preset asks for it. `position` is empty without a fresh fix.
inline Composed compose(const Preset &p,const std::string &position,size_t limit){
 Composed r;
 if(!p.position){if(p.text.size()>limit){r.error=ErrTooLong;return r;}r.ok=true;r.text=p.text;return r;}
 if(position.empty()){r.error=ErrNoFix;return r;}
 std::string full=p.text+" "+position;if(full.size()>limit){r.error=ErrTooLongPos;return r;}
 r.ok=true;r.text=full;return r;}
}
