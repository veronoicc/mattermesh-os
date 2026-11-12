#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
// Notes app (pure logic): a note is plain UTF-8 text, its title is the first line, checklist lines are Markdown tasks
// "- [ ] item" / "- [x] item". Storage is in notes_store.h, the app in notes_app.inc.
namespace mm_notes {
constexpr size_t MaxBytes=8000,MaxTitle=80,PreviewBytes=60,HomeBytes=400,StemMax=32;constexpr int MaxNotes=64;
enum class Kind:uint8_t {Text,Open,Done};
struct Line {Kind kind=Kind::Text;std::string text;Line(Kind k=Kind::Text,std::string t=std::string()):kind(k),text(std::move(t)){}};// text: the line without its "- [ ] " marker; constructors: the firmware builds as C++11
struct Progress {int done=0,total=0;};
struct Info {std::string stem,title,preview;int done=0,total=0;uint32_t modified=0;};
struct VisualRow {int line=0;Kind kind=Kind::Text;std::string text;bool first=true;VisualRow(int l=0,Kind k=Kind::Text,std::string t=std::string(),bool f=true):line(l),kind(k),text(std::move(t)),first(f){}};// one wrapped row of a note line; first: the row that carries the checkbox
// Lines split on '\n' (a '\r' is dropped); joinLines(splitLines(x))==x for text without '\r'.
inline std::vector<std::string> splitLines(const std::string &t){std::vector<std::string> out(1);for(char c:t){if(c=='\n')out.emplace_back();else if(c!='\r')out.back()+=c;}return out;}
inline std::string joinLines(const std::vector<std::string> &lines){std::string out;for(size_t i=0;i<lines.size();i++){if(i)out+='\n';out+=lines[i];}return out;}
// "- [ ] x", "- [x] x" and "- [X] x" are items (the marker alone is an empty item); everything else is text.
inline Line parseLine(const std::string &s){if(s.size()>=5&&s.compare(0,3,"- [")==0&&s[4]==']'&&(s[3]==' '||s[3]=='x'||s[3]=='X')&&(s.size()==5||s[5]==' '))return {s[3]==' '?Kind::Open:Kind::Done,s.size()>6?s.substr(6):std::string()};return {Kind::Text,s};}
inline std::string formatLine(const Line &l){return l.kind==Kind::Open?"- [ ] "+l.text:l.kind==Kind::Done?"- [x] "+l.text:l.text;}
inline std::vector<Line> parseLines(const std::string &t){std::vector<Line> out;for(auto &s:splitLines(t))out.push_back(parseLine(s));return out;}
inline std::string trimmed(const std::string &s){size_t b=0,e=s.size();while(b<e&&(s[b]==' '||s[b]=='\t'))b++;while(e>b&&(s[e-1]==' '||s[e-1]=='\t'))e--;return s.substr(b,e-b);}
// At most `max` bytes, never inside a UTF-8 sequence.
inline std::string utf8Cut(const std::string &s,size_t max){if(s.size()<=max)return s;size_t cut=max;while(cut>0&&(uint8_t(s[cut])&0xC0)==0x80)cut--;return s.substr(0,cut);}
// The first line is the title and never an item: checklist progress counts the lines below it.
inline std::string title(const std::string &t){auto first=trimmed(parseLine(splitLines(t)[0]).text);return first.empty()?"Untitled":first;}
inline Progress progress(const std::string &t){Progress p;auto lines=parseLines(t);for(size_t i=1;i<lines.size();i++)if(lines[i].kind!=Kind::Text){p.total++;if(lines[i].kind==Kind::Done)p.done++;}return p;}
// The first non-blank line under the title (its checkbox marker removed), for the list.
inline std::string preview(const std::string &t){auto lines=parseLines(t);for(size_t i=1;i<lines.size();i++){auto s=trimmed(lines[i].text);if(!s.empty())return utf8Cut(s,PreviewBytes);}return "";}
inline Info summary(const std::string &t){Info i;i.title=title(t);i.preview=preview(t);auto p=progress(t);i.done=p.done;i.total=p.total;return i;}
// Flip item `line` (never the title line); false when it is not an item.
inline bool toggle(std::string &t,size_t line){auto lines=splitLines(t);if(line==0||line>=lines.size())return false;auto p=parseLine(lines[line]);if(p.kind==Kind::Text)return false;p.kind=p.kind==Kind::Open?Kind::Done:Kind::Open;lines[line]=formatLine(p);t=joinLines(lines);return true;}
// Append "- [ ] " on a line of its own (an empty last item is reused).
inline void addItem(std::string &t){if(t.empty()){t="- [ ] ";return;}auto lines=splitLines(t);auto p=parseLine(lines.back());if(lines.size()>1&&p.kind==Kind::Open&&p.text.empty())return;if(t.back()!='\n')t+='\n';t+="- [ ] ";}
// Remove the checked items below the title; how many went.
inline int clearChecked(std::string &t){auto lines=splitLines(t);std::vector<std::string> kept;int n=0;for(size_t i=0;i<lines.size();i++){if(i>0&&parseLine(lines[i]).kind==Kind::Done){n++;continue;}kept.push_back(lines[i]);}if(n)t=joinLines(kept);return n;}
// ENTER while editing: after an item a new empty item starts; ENTER on an empty item ends the list.
inline void enterNewline(std::string &t){auto lines=splitLines(t);if(lines.size()>1){auto p=parseLine(lines.back());if(p.kind!=Kind::Text){if(p.text.empty()){lines.back().clear();t=joinLines(lines);return;}t+="\n- [ ] ";return;}}t+='\n';}
// What is saved: no blank lines before the title or at the end, no trailing blanks, no empty items, at most MaxBytes.
inline std::string normalize(const std::string &text){
 auto lines=splitLines(text);for(auto &l:lines)while(!l.empty()&&(l.back()==' '||l.back()=='\t'))l.pop_back();
 size_t b=0;while(b<lines.size()&&lines[b].empty())b++;
 std::vector<std::string> kept;for(size_t i=b;i<lines.size();i++){if(i>b){auto p=parseLine(lines[i]);if(p.kind==Kind::Open&&p.text.empty())continue;}kept.push_back(lines[i]);}
 while(!kept.empty()&&kept.back().empty())kept.pop_back();
 return utf8Cut(joinLines(kept),MaxBytes);}
// Rename: the first line replaced (a new line in the name becomes a space).
inline std::string setTitle(const std::string &t,std::string name){for(auto &c:name)if(c=='\n'||c=='\r')c=' ';auto lines=splitLines(t);lines[0]=trimmed(name);return joinLines(lines);}
// File name stem from a title: ASCII letters, digits and '_'; anything else (dots dropped) becomes one '-'; "note" when nothing is left.
inline std::string safeStem(const std::string &t){
 std::string s;for(unsigned char c:t){if(c=='.')continue;bool ok=(c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';if(ok)s+=char(c);else if(!s.empty()&&s.back()!='-')s+='-';}
 if(s.size()>StemMax)s.resize(StemMax);while(!s.empty()&&s.back()=='-')s.pop_back();return s.empty()?"note":s;}
// The stem, or stem-2, stem-3 ... when `taken(name)` says the file exists.
template<class F> inline std::string uniqueStem(const std::string &stem,F taken){if(!taken(stem))return stem;for(int n=2;n<1000;n++){std::string cand=stem+"-"+std::to_string(n);if(!taken(cand))return cand;}return stem;}
inline std::string lowered(std::string s){for(auto &c:s)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return s;}
// Newest first, or by title (ASCII case ignored); the other key breaks ties.
inline void sortInfos(std::vector<Info> &v,bool byTitle){std::stable_sort(v.begin(),v.end(),[&](const Info &a,const Info &b){auto ta=lowered(a.title),tb=lowered(b.title);if(byTitle)return ta!=tb?ta<tb:a.modified>b.modified;return a.modified!=b.modified?a.modified>b.modified:ta<tb;});}
// The note as the Home note widget shows it: title, then the lines ("[ ] " / "[x] " for items), blank lines and (if asked) checked items left out.
inline std::string homeText(const std::string &t,bool showChecked){
 std::string out=title(t);auto lines=parseLines(t);
 for(size_t i=1;i<lines.size();i++){auto &l=lines[i];if(l.kind==Kind::Done&&!showChecked)continue;if(l.kind==Kind::Text&&trimmed(l.text).empty())continue;out+="\n"+(l.kind==Kind::Open?"[ ] "+l.text:l.kind==Kind::Done?"[x] "+l.text:l.text);}
 return utf8Cut(out,HomeBytes);}
// The rows of the note view (the title line is drawn in the header): every line wrapped to `width` (items `indent` narrower
// for the checkbox column); wrap(text,width) -> rows. Checked items are left out when !showChecked.
template<class W> inline std::vector<VisualRow> visualRows(const std::vector<Line> &lines,bool showChecked,W wrap,int width,int indent){
 std::vector<VisualRow> rows;for(size_t i=1;i<lines.size();i++){const Line &l=lines[i];if(l.kind==Kind::Done&&!showChecked)continue;auto parts=wrap(l.text,l.kind==Kind::Text?width:width-indent);if(parts.empty())parts.push_back("");for(size_t j=0;j<parts.size();j++)rows.push_back({int(i),l.kind,parts[j],j==0});}
 return rows;}
// Line numbers of the items on show, in order.
inline std::vector<int> itemLines(const std::vector<Line> &lines,bool showChecked){std::vector<int> out;for(size_t i=1;i<lines.size();i++)if(lines[i].kind==Kind::Open||(lines[i].kind==Kind::Done&&showChecked))out.push_back(int(i));return out;}
inline int rowOfLine(const std::vector<VisualRow> &rows,int line){for(size_t i=0;i<rows.size();i++)if(rows[i].line==line&&rows[i].first)return int(i);return -1;}
inline int lineAtRow(const std::vector<VisualRow> &rows,int row){return row>=0&&row<int(rows.size())?rows[row].line:-1;}
inline int clampScroll(int scroll,int rows,int visible){return std::max(0,std::min(scroll,std::max(0,rows-visible)));}
// First row shown so that `row` (-1: none) is on screen.
inline int scrollFor(int rows,int row,int scroll,int visible){if(row>=0){if(row<scroll)scroll=row;else if(row>=scroll+visible)scroll=row-visible+1;}return clampScroll(scroll,rows,visible);}
}
