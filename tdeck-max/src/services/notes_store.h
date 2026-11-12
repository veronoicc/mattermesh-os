#pragma once
#include "notes.h"
#include "sd_replace.h"
// Notes on the SD card: /mattermesh/notes/<stem>.txt, one note per file. Every save writes <stem>.tmp and replaces the
// file through sdReplace (a power cut leaves the old text, or only <stem>.bak which listNotes puts back).
// FS: the Arduino fs::FS (or a stand-in with exists / remove / rename / mkdir / open and the File calls used here).
namespace mm_notes {
constexpr const char *Dir="/mattermesh/notes";
inline std::string pathOf(const std::string &stem,const char *ext){return std::string(Dir)+"/"+stem+ext;}
template<class FS> bool ensureDir(FS &fs){return (fs.exists("/mattermesh")||fs.mkdir("/mattermesh"))&&(fs.exists(Dir)||fs.mkdir(Dir));}
template<class FS> bool noteExists(FS &fs,const std::string &stem){return fs.exists(pathOf(stem,".txt").c_str())||fs.exists(pathOf(stem,".bak").c_str());}
template<class FS> bool save(FS &fs,const std::string &stem,const std::string &text){
 if(!ensureDir(fs))return false;std::string tmp=pathOf(stem,".tmp"),path=pathOf(stem,".txt"),bak=pathOf(stem,".bak"),body=text+"\n";
 auto f=fs.open(tmp.c_str(),FILE_WRITE);if(!f)return false;bool ok=f.write(reinterpret_cast<const uint8_t*>(body.data()),body.size())==body.size();f.close();
 if(!ok){fs.remove(tmp.c_str());return false;}
 return device::sdReplace(fs,tmp.c_str(),path.c_str(),bak.c_str(),false);}
// The text without its final line break (at most MaxBytes of it).
template<class FS> bool load(FS &fs,const std::string &stem,std::string &out){
 std::string path=pathOf(stem,".txt"),bak=pathOf(stem,".bak");auto f=fs.open(device::sdSavedPath(fs,path.c_str(),bak.c_str()));if(!f||f.isDirectory())return false;
 size_t n=std::min<size_t>(f.size(),MaxBytes);out.assign(n,'\0');size_t got=n?f.read(reinterpret_cast<uint8_t*>(&out[0]),n):0;f.close();out.resize(got);
 while(!out.empty()&&(out.back()=='\n'||out.back()=='\r'))out.pop_back();return true;}
template<class FS> bool removeNote(FS &fs,const std::string &stem){for(const char *ext:{".txt",".bak",".tmp"})fs.remove(pathOf(stem,ext).c_str());return !noteExists(fs,stem);}
// The newest MaxNotes notes (stem, title, preview, progress, last write); a note left only as .bak by a power cut comes back first.
template<class FS> std::vector<Info> listNotes(FS &fs){
 std::vector<Info> out;std::vector<std::pair<std::string,uint32_t>> stems;std::vector<std::string> orphans;
 {auto d=fs.open(Dir);if(!d||!d.isDirectory())return out;
  for(auto e=d.openNextFile();e;e=d.openNextFile()){std::string n=e.name();n=n.substr(n.rfind('/')+1);bool dir=e.isDirectory();uint32_t t=uint32_t(e.getLastWrite());e.close();if(dir||n.size()<5)continue;std::string ext=n.substr(n.size()-4),stem=n.substr(0,n.size()-4);if(ext==".txt")stems.push_back({stem,t});else if(ext==".bak")orphans.push_back(stem);}
  d.close();}
 for(auto &s:orphans){std::string txt=pathOf(s,".txt");if(fs.exists(txt.c_str())||!fs.rename(pathOf(s,".bak").c_str(),txt.c_str()))continue;auto f=fs.open(txt.c_str());stems.push_back({s,f?uint32_t(f.getLastWrite()):0});if(f)f.close();}
 std::stable_sort(stems.begin(),stems.end(),[](const std::pair<std::string,uint32_t> &a,const std::pair<std::string,uint32_t> &b){return a.second>b.second;});
 if(int(stems.size())>MaxNotes)stems.resize(MaxNotes);
 for(auto &s:stems){std::string text;if(!load(fs,s.first,text))continue;Info i=summary(text);i.stem=s.first;i.modified=s.second;out.push_back(i);}
 return out;}
}
