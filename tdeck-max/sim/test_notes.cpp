#include <algorithm>
#include <cstdint>
#include <ctime>
#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#define FILE_READ "r"
#define FILE_WRITE "w"
#include "services/sd_replace.h"
#include "services/notes.h"
#include "services/notes_store.h"
using namespace mm_notes;
// A tiny SD card: files as name -> content, directories as a set, last write = a counter (the order of the writes).
struct MemFS;
struct File {
 MemFS *fs=nullptr;std::string path,buf;size_t pos=0;bool writing=false,dir=false;std::vector<std::string> entries;size_t next=0;
 operator bool() const {return fs!=nullptr;}
 bool isDirectory() const {return dir;}
 size_t size() const {return buf.size();}
 const char *name() const {return path.c_str();}
 time_t getLastWrite();
 size_t read(uint8_t *out,size_t n){n=std::min(n,buf.size()-pos);for(size_t i=0;i<n;i++)out[i]=uint8_t(buf[pos+i]);pos+=n;return n;}
 size_t write(const uint8_t *in,size_t n){buf.append(reinterpret_cast<const char*>(in),n);return n;}
 File openNextFile();
 void close();
};
struct MemFS {
 std::map<std::string,std::string> files;std::map<std::string,int> stamp;std::set<std::string> dirs;int clock=0;std::set<std::pair<std::string,std::string>> failRename;
 bool exists(const char *p){return files.count(p)||dirs.count(p);}
 bool remove(const char *p){stamp.erase(p);return files.erase(p)>0;}
 bool rename(const char *a,const char *b){if(failRename.erase({a,b})||!files.count(a)||files.count(b))return false;files[b]=files[a];stamp[b]=stamp[a];files.erase(a);stamp.erase(a);return true;}
 bool mkdir(const char *p){dirs.insert(p);return true;}
 File open(const char *p,const char *mode="r"){File f;if(dirs.count(p)){f.fs=this;f.dir=true;f.path=p;std::string prefix=std::string(p)+"/";for(auto &e:files)if(e.first.compare(0,prefix.size(),prefix)==0)f.entries.push_back(e.first);return f;}
  if(mode[0]=='w'){if(!dirs.count(std::string(p).substr(0,std::string(p).rfind('/'))))return f;f.fs=this;f.path=p;f.writing=true;return f;}
  if(!files.count(p))return f;f.fs=this;f.path=p;f.buf=files[p];return f;}
};
time_t File::getLastWrite(){return fs->stamp[path];}
File File::openNextFile(){if(next>=entries.size())return File();File f=fs->open(entries[next++].c_str());return f;}
void File::close(){if(fs&&writing){fs->files[path]=buf;fs->stamp[path]=++fs->clock;}fs=nullptr;}
// Wrap: every 5 bytes, a stand-in for the font metrics.
static std::vector<std::string> wrap5(const std::string &s,int w){std::vector<std::string> out;for(size_t i=0;i<s.size();i+=w)out.push_back(s.substr(i,w));if(out.empty())out.push_back("");return out;}
int main(){
 // ---- lines and checklist syntax
 assert(splitLines("a\r\nb\n").size()==3&&splitLines("a\r\nb\n")[0]=="a");
 assert(joinLines(splitLines("a\nb\n"))=="a\nb\n"&&joinLines(splitLines(""))=="");
 {auto l=parseLine("- [ ] milk");assert(l.kind==Kind::Open&&l.text=="milk");}
 {auto l=parseLine("- [x] eggs");assert(l.kind==Kind::Done&&l.text=="eggs");}
 {auto l=parseLine("- [X] eggs");assert(l.kind==Kind::Done&&l.text=="eggs");}
 {auto l=parseLine("- [ ]");assert(l.kind==Kind::Open&&l.text.empty());}
 for(auto s:{"-[ ] x","- [] x","- [y] x","  - [ ] x","- [ ]x","hello","- plain"})assert(parseLine(s).kind==Kind::Text&&parseLine(s).text==s);
 assert(formatLine({Kind::Open,"milk"})=="- [ ] milk"&&formatLine({Kind::Done,"eggs"})=="- [x] eggs"&&formatLine({Kind::Text,"- x"})=="- x");
 // ---- title, progress, preview
 assert(title("Shopping\n- [ ] a")=="Shopping"&&title("")=="Untitled"&&title("\n\n")=="Untitled"&&title("- [x] foo")=="foo"&&title("  spaced  \nx")=="spaced");
 {auto p=progress("T\n- [ ] a\n- [x] b\n- [X] c\ntext\n- [ ] d");assert(p.done==2&&p.total==4);}
 {auto p=progress("- [x] title is no item\n- [ ] a");assert(p.done==0&&p.total==1);}
 {auto p=progress("just text");assert(p.done==0&&p.total==0);}
 assert(preview("T\nhello world")=="hello world"&&preview("T")==""&&preview("T\n\n- [ ] milk")=="milk"&&preview("T\n- [x] eggs\n- [ ] milk")=="eggs");
 assert(preview("T\n"+std::string(100,'a')).size()==PreviewBytes);
 // ---- toggle / add / clear / enter
 {std::string t="T\n- [ ] a\n- [x] b\nplain";
  assert(toggle(t,1)&&t=="T\n- [x] a\n- [x] b\nplain");assert(toggle(t,2)&&t=="T\n- [x] a\n- [ ] b\nplain");
  std::string same=t;assert(!toggle(t,0)&&!toggle(t,3)&&!toggle(t,9)&&t==same);}
 {std::string t;addItem(t);assert(t=="- [ ] ");}
 {std::string t="T";addItem(t);assert(t=="T\n- [ ] ");}
 {std::string t="T\n";addItem(t);assert(t=="T\n- [ ] ");}
 {std::string t="T\n- [ ] ";addItem(t);assert(t=="T\n- [ ] ");}
 {std::string t="T\n- [x] a";addItem(t);assert(t=="T\n- [x] a\n- [ ] ");}
 {std::string t="T\n- [x] a\n- [ ] b\n- [x] c";assert(clearChecked(t)==2&&t=="T\n- [ ] b");assert(clearChecked(t)==0&&t=="T\n- [ ] b");}
 {std::string t="T\n- [x] a";assert(clearChecked(t)==1&&t=="T");}
 {std::string t="T";enterNewline(t);assert(t=="T\n");}
 {std::string t="T\n- [ ] milk";enterNewline(t);assert(t=="T\n- [ ] milk\n- [ ] ");}
 {std::string t="T\n- [x] milk";enterNewline(t);assert(t=="T\n- [x] milk\n- [ ] ");}
 {std::string t="T\n- [ ] milk\n- [ ] ";enterNewline(t);assert(t=="T\n- [ ] milk\n");}
 {std::string t="- [ ] titleish";enterNewline(t);assert(t=="- [ ] titleish\n");}
 {std::string t="";enterNewline(t);assert(t=="\n");}
 // ---- normalize and rename
 assert(normalize("\n\nT \r\n- [ ]\n- [ ] a\n\n")=="T\n- [ ] a");
 assert(normalize("")==""&&normalize("\n \n")==""&&normalize("a\n\n\nb")=="a\n\n\nb");
 assert(normalize(std::string(MaxBytes+500,'a')).size()==MaxBytes);
 {std::string euro;for(int i=0;i<MaxBytes;i++)euro+="\xe2\x82\xac";auto n=normalize(euro);assert(n.size()==(MaxBytes/3)*3);}
 assert(setTitle("T\n- [ ] a","New")=="New\n- [ ] a"&&setTitle("","X")=="X"&&setTitle("T","a\nb")=="a b"&&setTitle("T\nbody","  x  ")=="x\nbody");
 // ---- file names
 assert(safeStem("Shopping List")=="Shopping-List"&&safeStem("  a/b\\c: d?  ")=="a-b-c-d"&&safeStem("Za\xc5\xbc\xc3\xb3")=="Za"&&safeStem("\xe6\x97\xa5\xe6\x9c\xac")=="note"&&safeStem("")=="note"&&safeStem("a.b_c--d")=="ab_c-d");
 assert(safeStem(std::string(50,'x')).size()==StemMax);
 assert(uniqueStem("a",[](const std::string &s){return s=="a"||s=="a-2";})=="a-3"&&uniqueStem("a",[](const std::string &){return false;})=="a");
 // ---- summary and sorting
 {auto i=summary("Trip\n- [x] tent\n- [ ] stove\n- [ ] map");assert(i.title=="Trip"&&i.done==1&&i.total==3&&i.preview=="tent");}
 {std::vector<Info> v(3);v[0].title="a";v[0].modified=5;v[1].title="B";v[1].modified=9;v[2].title="c";v[2].modified=1;
  sortInfos(v,false);assert(v[0].title=="B"&&v[1].title=="a"&&v[2].title=="c");
  v[2].modified=5;sortInfos(v,false);assert(v[1].title=="a"&&v[2].title=="c");
  sortInfos(v,true);assert(v[0].title=="a"&&v[1].title=="B"&&v[2].title=="c");}
 // ---- the Home widget text
 assert(homeText("Shopping\n- [ ] milk\n- [x] eggs\ntext\n\nmore",true)=="Shopping\n[ ] milk\n[x] eggs\ntext\nmore");
 assert(homeText("Shopping\n- [ ] milk\n- [x] eggs\ntext\n\nmore",false)=="Shopping\n[ ] milk\ntext\nmore");
 assert(homeText(std::string(HomeBytes+100,'a'),true).size()==HomeBytes&&homeText("",true)=="Untitled");
 // ---- visual rows: wrapping, indent for the checkbox, hidden checked items, scrolling
 {auto lines=parseLines("T\n- [ ] abcdefgh\nxy\n- [x] zz");
  auto rows=visualRows(lines,true,wrap5,5,0);assert(rows.size()==4);
  assert(rows[0].line==1&&rows[0].first&&rows[0].kind==Kind::Open&&rows[0].text=="abcde");assert(rows[1].line==1&&!rows[1].first&&rows[1].text=="fgh");
  assert(rows[2].line==2&&rows[2].kind==Kind::Text&&rows[3].line==3&&rows[3].kind==Kind::Done);
  assert(visualRows(lines,false,wrap5,5,0).size()==3);
  auto narrow=visualRows(lines,true,wrap5,5,2);assert(narrow[0].text=="abc");// items wrap narrower by the checkbox column
  auto items=itemLines(lines,true);assert(items.size()==2&&items[0]==1&&items[1]==3);assert(itemLines(lines,false).size()==1);
  assert(rowOfLine(rows,3)==3&&rowOfLine(rows,1)==0&&rowOfLine(rows,7)==-1);
  assert(lineAtRow(rows,1)==1&&lineAtRow(rows,3)==3&&lineAtRow(rows,9)==-1&&lineAtRow(rows,-1)==-1);}
 assert(scrollFor(20,10,0,5)==6&&scrollFor(20,2,6,5)==2&&scrollFor(20,8,6,5)==6&&scrollFor(3,2,0,5)==0&&scrollFor(20,19,0,5)==15&&scrollFor(20,-1,4,5)==4);
 assert(clampScroll(30,20,5)==15&&clampScroll(-3,20,5)==0&&clampScroll(2,3,5)==0);
 // ---- the store: crash-safe save, listing, recovery, delete
 {MemFS fs;fs.mkdir("/mattermesh");
  assert(save(fs,"a","Alpha\n- [ ] one")&&fs.files["/mattermesh/notes/a.txt"]=="Alpha\n- [ ] one\n"&&!fs.exists("/mattermesh/notes/a.tmp")&&!fs.exists("/mattermesh/notes/a.bak"));
  std::string out;assert(load(fs,"a",out)&&out=="Alpha\n- [ ] one"&&!load(fs,"zz",out));
  assert(save(fs,"a","Alpha\n- [x] one")&&load(fs,"a",out)&&out=="Alpha\n- [x] one");
  // a failing replace leaves the old text in place and no temporary file
  fs.failRename.insert({"/mattermesh/notes/a.tmp","/mattermesh/notes/a.txt"});assert(!save(fs,"a","LOST")&&load(fs,"a",out)&&out=="Alpha\n- [x] one"&&!fs.exists("/mattermesh/notes/a.tmp"));
  assert(save(fs,"b","Beta"));fs.files["/mattermesh/notes/readme.md"]="x";
  auto list=listNotes(fs);assert(list.size()==2);std::set<std::string> stems;for(auto &i:list)stems.insert(i.stem);assert(stems==std::set<std::string>({"a","b"}));
  for(auto &i:list){if(i.stem=="a"){assert(i.title=="Alpha"&&i.done==1&&i.total==1);}else assert(i.title=="Beta"&&i.total==0);assert(i.modified>0);}
  // power cut between the two renames: only the backup is left, listing brings it back
  fs.files["/mattermesh/notes/c.bak"]="Gamma\n";assert(listNotes(fs).size()==3&&fs.exists("/mattermesh/notes/c.txt")&&!fs.exists("/mattermesh/notes/c.bak"));
  assert(noteExists(fs,"c")&&!noteExists(fs,"d"));
  assert(removeNote(fs,"c")&&!noteExists(fs,"c")&&listNotes(fs).size()==2);
  // at most MaxNotes are listed
  for(int i=0;i<MaxNotes+5;i++)save(fs,"n"+std::to_string(i),"N"+std::to_string(i));assert(int(listNotes(fs).size())==MaxNotes);}
 {MemFS fs;assert(listNotes(fs).empty());assert(save(fs,"first","One")&&fs.dirs.count("/mattermesh/notes")&&fs.dirs.count("/mattermesh"));}// the folders are created
 std::cout<<"PASS: notes: checklist parsing, toggle/add/clear/enter, normalize, titles, safe file names, sorting, Home text, visual rows and scrolling, crash-safe store\n";
}
