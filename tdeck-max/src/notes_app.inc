// Notes (services/notes.h, services/notes_store.h): the Notes app (section 56). Included by main.cpp.
// Notes are /mattermesh/notes/<name>.txt on the SD card; the first line is the title, "- [ ] item" / "- [x] item" lines are a checklist.
// Keys: list W/K up, S/J down, ENTER or D/L opens, N new note, O options. Note: W/S/J/K the checkboxes then the actions, ENTER ticks / acts,
// D/L/TAB next action, H previous, [ ] page. Editor: ENTER new line (after an item a new item), TAB adds an item, BACK saves. Options: W/S rows, ENTER/D/H change.
// Touch: the cards, checkboxes and buttons themselves; a swipe pages the list / the note. NVS "mmnotes": sort, chk (show checked items), type (new note), pin (the note on Home).
using mm::NotesMode;
static NotesMode ntMode=NotesMode::List;static std::vector<mm_notes::Info> ntList;
static bool ntStale=true,ntDirect=false,ntPending=false,ntFresh=false,ntTitleOnly=false,ntSaveFailed=false,ntShow=true;// ntFresh: the open note has no file yet; ntTitleOnly: the title prompt of a new checklist; ntDirect: opened from Home, BACK returns there
static int ntFocus=0,ntScroll=0,ntVF=0,ntVScroll=0,ntEditFocus=-1,ntOptFocus=0,ntSort=0,ntType=0;
static std::string ntStem,ntText,ntPin;static uint32_t ntModified=0;static std::vector<mm_notes::Line> ntLines;static String ntBuf,ntHome;// ntHome: the pinned note as Home shows it
static bool ntPage(){return screen==Screen::Section&&section==mm::NotesSection&&!deviceLocked;}
static void ntLoadSettings(){ntSort=constrain(device::prefGetInt("mmnotes","sort",0),0,1);ntShow=device::prefGetInt("mmnotes","chk",1)!=0;ntType=constrain(device::prefGetInt("mmnotes","type",0),0,1);ntPin=device::prefGetString("mmnotes","pin").c_str();}
static bool ntSaveSettings(){return device::prefPutInt("mmnotes","sort",ntSort)&&device::prefPutInt("mmnotes","chk",ntShow)&&device::prefPutInt("mmnotes","type",ntType);}
// The Home note widget shows the pinned note (cached: Home repaints often); a pin whose file is gone is dropped.
static void ntHomeRefresh(){
 ntHome="";if(ntPin.empty()||!sdReady)return;std::string text;
 if(!mm_notes::load(SD,ntPin,text)){if(!mm_notes::noteExists(SD,ntPin)){ntPin.clear();device::prefPutString("mmnotes","pin",String());}return;}
 ntHome=mm_notes::homeText(text,ntShow).c_str();}
bool notesPinned(){return ntHome.length()>0;}
const char *notesHomeText(){return ntHome.c_str();}
void notesBegin(){ntLoadSettings();ntHomeRefresh();}
static std::string ntDate(uint32_t t){if(t<946684800)return "--";time_t tt=t;struct tm tm;localtime_r(&tt,&tm);char b[24];strftime(b,sizeof b,"%d %b %H:%M",&tm);return b;}
static void ntRefresh(){ntList=sdReady?mm_notes::listNotes(SD):std::vector<mm_notes::Info>();mm_notes::sortInfos(ntList,ntSort==1);int n=int(ntList.size());ntFocus=std::min(ntFocus,n+1);ntScroll=mm_notes::clampScroll(ntScroll,n,mm::NoteRows);ntStale=false;}
static void ntSetText(const std::string &t){ntText=t;ntLines=mm_notes::parseLines(t);}
static std::vector<mm_notes::VisualRow> ntRows(){mm::Components c(frame);return mm_notes::visualRows(ntLines,ntShow,[&](const std::string &t,int w){return c.lines(t,w,mm::Font::ChatBody);},mm::NoteTextW,mm::NoteCheckW);}
static int ntItemCount(){return int(mm_notes::itemLines(ntLines,ntShow).size());}
static int ntCheckedCount(){int n=0;for(size_t i=1;i<ntLines.size();i++)if(ntLines[i].kind==mm_notes::Kind::Done)n++;return n;}
static void ntClampVF(){ntVF=std::max(0,std::min(ntVF,ntItemCount()+mm::NoteActionCount-1));}
// The focused checkbox scrolls into view.
static void ntViewSync(){auto items=mm_notes::itemLines(ntLines,ntShow);auto rows=ntRows();int line=ntVF<int(items.size())?items[ntVF]:-1;ntVScroll=mm_notes::scrollFor(int(rows.size()),mm_notes::rowOfLine(rows,line),ntVScroll,mm::NoteViewRows);}
// List focus on the open note, after the list was read again.
static void ntToList(){ntMode=NotesMode::List;ntRefresh();notice="";for(size_t i=0;i<ntList.size();i++)if(ntList[i].stem==ntStem){ntFocus=int(i)+2;ntScroll=mm_notes::scrollFor(int(ntList.size()),int(i),ntScroll,mm::NoteRows);}render();}
static bool ntOpenNote(const std::string &stem,bool paint=true){
 std::string text;if(!mm_notes::load(SD,stem,text)){notice="Note unreadable";if(paint)render();return false;}
 ntStem=stem;ntSetText(text);ntModified=0;{File f=SD.open(mm_notes::pathOf(stem,".txt").c_str());if(f){ntModified=uint32_t(f.getLastWrite());f.close();}}
 ntMode=NotesMode::View;ntFresh=false;ntVF=0;ntVScroll=0;ntClampVF();notice="";if(paint)render();return true;}
// Saves the text normalized: a new note gets its file name from its title; an empty new note is not saved at all.
static bool ntCommit(std::string text){
 text=mm_notes::normalize(text);if(!sdReady){notice="No SD card";return false;}
 if(ntFresh){if(text.empty())return true;std::string stem=mm_notes::uniqueStem(mm_notes::safeStem(mm_notes::title(text)),[&](const std::string &s){return mm_notes::noteExists(SD,s);});
  if(!mm_notes::save(SD,stem,text)){notice="Save failed";return false;}ntStem=stem;ntFresh=false;}
 else if(!mm_notes::save(SD,ntStem,text)){notice="Save failed";return false;}
 ntSetText(text);ntModified=uint32_t(time(nullptr));ntStale=true;if(ntStem==ntPin)ntHomeRefresh();return true;}
static void ntEditDone(){
 if(!ntCommit(ntBuf.c_str())){ntSaveFailed=true;render();return;}
 ntBuf="";ntSaveFailed=false;notice="";ntEditFocus=-1;
 if(ntFresh){ntFresh=false;ntStem.clear();ntToList();return;}// nothing was typed: no note
 ntMode=NotesMode::View;ntClampVF();render();}
static void ntNew(){
 if(!sdReady){notice="No SD card";render();return;}if(int(ntList.size())>=mm_notes::MaxNotes){notice="Notes full";render();return;}
 ntFresh=true;ntStem.clear();ntSetText("");ntBuf="";notice="";ntEditFocus=-1;ntSaveFailed=false;ntTitleOnly=ntType==1;ntMode=ntTitleOnly?NotesMode::Rename:NotesMode::Edit;render();}
static void ntOptions(){ntMode=NotesMode::Options;ntOptFocus=0;notice="";render();}
static void ntEditBegin(bool add){std::string t=ntText;if(add)mm_notes::addItem(t);ntBuf=t.c_str();ntMode=NotesMode::Edit;ntEditFocus=-1;ntSaveFailed=false;notice="";render();}
static void ntRenameStart(){ntBuf=mm_notes::title(ntText).c_str();ntTitleOnly=false;ntMode=NotesMode::Rename;ntEditFocus=-1;notice="";render();}
static void ntRenameCancel(){if(ntTitleOnly){ntTitleOnly=false;ntFresh=false;ntBuf="";ntToList();return;}ntBuf="";ntMode=NotesMode::View;notice="";render();}
// A new checklist: its title starts the text, the first item is ready to be typed. Rename: the first line and the file name change, a pin follows.
static void ntRenameDone(){
 std::string name=mm_notes::trimmed(ntBuf.c_str());
 if(ntTitleOnly){ntTitleOnly=false;ntBuf=((name.empty()?std::string("Checklist"):name)+"\n- [ ] ").c_str();ntMode=NotesMode::Edit;ntEditFocus=-1;notice="";render();return;}
 if(name.empty()){notice="Name needed";render();return;}
 std::string text=mm_notes::setTitle(ntText,name),stem=mm_notes::uniqueStem(mm_notes::safeStem(name),[&](const std::string &s){return s!=ntStem&&mm_notes::noteExists(SD,s);});
 if(!mm_notes::save(SD,stem,text)){notice="Save failed";render();return;}
 if(stem!=ntStem){mm_notes::removeNote(SD,ntStem);if(ntStem==ntPin){ntPin=stem;device::prefPutString("mmnotes","pin",String(stem.c_str()));}ntStem=stem;}
 ntSetText(text);ntModified=uint32_t(time(nullptr));ntStale=true;if(ntStem==ntPin)ntHomeRefresh();ntBuf="";ntMode=NotesMode::View;notice="Renamed";render();}
static void ntPinToggle(){
 if(ntPin==ntStem){bool ok=device::prefPutString("mmnotes","pin",String());if(ok)ntPin.clear();ntHomeRefresh();notice=ok?"Unpinned from Home":"Save failed";}
 else{bool ok=device::prefPutString("mmnotes","pin",String(ntStem.c_str()));if(ok){ntPin=ntStem;setHomeMain(mm::MainNote);homeMainFocus=homeMain;ntHomeRefresh();}notice=ok?"Pinned to Home":"Save failed";}
 render();}
static void ntDelete(){
 if(mm_notes::removeNote(SD,ntStem)){if(ntStem==ntPin){ntPin.clear();device::prefPutString("mmnotes","pin",String());ntHomeRefresh();}ntStem.clear();ntToList();notice="Note deleted";render();return;}
 ntMode=NotesMode::View;notice="Delete failed";render();}
static void ntClear(){std::string t=ntText;int n=mm_notes::clearChecked(t);if(!n){notice="No checked items";render();return;}if(!ntCommit(t)){render();return;}ntClampVF();notice=String("Cleared ")+n;render();}
static void ntToggle(int line){std::string t=ntText;if(!mm_notes::toggle(t,size_t(line)))return;if(!ntCommit(t)){render();return;}ntClampVF();notice="";render();}
static void ntViewAct(int a){
 if(a==mm::NoteEdit)ntEditBegin(false);else if(a==mm::NoteAdd)ntEditBegin(true);else if(a==mm::NoteClear)ntClear();else if(a==mm::NoteRename)ntRenameStart();else if(a==mm::NotePin)ntPinToggle();
 else{ntMode=NotesMode::Confirm;ntEditFocus=1;notice="";render();}}
static void ntOptStep(int i){if(i==0)ntSort^=1;else if(i==1)ntShow=!ntShow;else ntType^=1;notice=ntSaveSettings()?"":"Save failed";ntHomeRefresh();ntStale=true;render();}
static void ntAddItemToBuffer(){std::string t=ntBuf.c_str();mm_notes::addItem(t);if(t.size()<=mm_notes::MaxBytes)ntBuf=t.c_str();notice="";repaintSettled();}
// Typing goes to the editor and to the rename line: the accent key, other keyboard languages and dictated text arrive through notesAppend.
bool notesTyping(){return ntPage()&&(ntMode==NotesMode::Edit||ntMode==NotesMode::Rename);}
String *notesField(){return notesTyping()?&ntBuf:nullptr;}
void notesAppend(const std::string &utf8){if(!notesTyping())return;size_t limit=ntMode==NotesMode::Rename?mm_notes::MaxTitle:mm_notes::MaxBytes;if(ntBuf.length()+utf8.size()>limit)return;ntBuf+=utf8.c_str();notice="";ntSaveFailed=false;repaintSettled();}
// A dictated transcript (MIC held in the editor): joined like the chat composer does, trimmed at the note's byte limit.
bool notesDictated(const String &text){if(!notesTyping())return false;bool trimmed=false;size_t limit=ntMode==NotesMode::Rename?mm_notes::MaxTitle:mm_notes::MaxBytes;ntBuf=device::appendDictation(ntBuf.c_str(),text.c_str(),limit,trimmed).c_str();notice=trimmed?"Dictation trimmed / note limit":"";ntSaveFailed=false;repaintSettled();return true;}
void notesOpen(){
 notice="";ntBuf="";ntFresh=ntTitleOnly=ntSaveFailed=false;ntFocus=ntScroll=0;ntMode=NotesMode::List;ntRefresh();ntDirect=ntPending;ntPending=false;
 if(ntDirect&&!ntPin.empty()&&!ntOpenNote(ntPin,false))ntDirect=false;}
// The Home note widget opens the pinned note directly (openSection turns NoteSection into this while a note is pinned).
bool notesWantPinned(){if(ntHome.length()==0)return false;ntPending=true;return true;}
// Leaving the app any way at all: an editor in progress is saved, the next visit starts at the list.
void notesLeft(){if(ntMode==NotesMode::Edit&&!ntSaveFailed)ntCommit(ntBuf.c_str());ntBuf="";ntFresh=ntTitleOnly=ntDirect=false;ntMode=NotesMode::List;ntStale=true;}
void notesCompose(mm::State &v){
 mm::NotesView &w=v.notes;w=mm::NotesView();w.mode=ntMode;w.sd=sdReady;w.newType=ntType;w.sort=ntSort;w.showChecked=ntShow;w.optFocus=ntOptFocus;
 if(ntMode==NotesMode::List){
  if(ntStale)ntRefresh();
  for(auto &i:ntList){mm::NoteRow row;row.title=i.title;row.preview=i.preview;row.progress=i.total?std::to_string(i.done)+"/"+std::to_string(i.total):"";row.date=ntDate(i.modified);row.pinned=i.stem==ntPin;w.rows.push_back(row);}
  w.total=int(ntList.size());w.focus=ntFocus;w.scroll=ntScroll;return;}
 if(ntMode==NotesMode::Edit||ntMode==NotesMode::Rename){w.text=ntBuf.c_str();w.editFocus=ntEditFocus;w.heading=ntTitleOnly?"Title of the new checklist":"Rename note";return;}
 if(ntMode==NotesMode::Options)return;
 w.title=mm_notes::title(ntText);auto p=mm_notes::progress(ntText);w.meta=(p.total?std::to_string(p.done)+"/"+std::to_string(p.total)+" done   ":"")+ntDate(ntModified);
 w.pinned=ntStem==ntPin;w.lines=ntLines;w.viewScroll=ntVScroll;w.checked=ntCheckedCount();w.editFocus=ntEditFocus;
 int items=ntItemCount();if(ntMode==NotesMode::View){if(ntVF<items){auto lines=mm_notes::itemLines(ntLines,ntShow);w.focusLine=lines[ntVF];}else w.focusAction=ntVF-items;}}
bool notesBack(){
 if(!ntPage())return false;notice="";
 if(ntMode==NotesMode::Edit){if(ntSaveFailed){ntSaveFailed=false;ntBuf="";ntEditFocus=-1;if(ntFresh){ntFresh=false;ntStem.clear();ntToList();}else{ntMode=NotesMode::View;render();}return true;}ntEditDone();return true;}// a second BACK after a failed save drops the edit
 if(ntMode==NotesMode::Rename){ntRenameCancel();return true;}
 if(ntMode==NotesMode::Confirm){ntMode=NotesMode::View;render();return true;}
 if(ntMode==NotesMode::Options){ntMode=NotesMode::List;render();return true;}
 if(ntMode==NotesMode::View){if(ntDirect)return false;ntToList();return true;}
 return false;}
bool notesKey(char c){
 if(c=='\b'||!ntPage())return false;
 const bool down=c=='s'||c=='j',up=c=='w'||c=='k',next=c=='d'||c=='l',prev=c=='h',ok=c=='\n'||c==' ';
 if(ntMode==NotesMode::Edit||ntMode==NotesMode::Rename){
  if(c=='\n'){if(ntMode==NotesMode::Rename)ntRenameDone();else{std::string t=ntBuf.c_str();mm_notes::enterNewline(t);if(t.size()<=mm_notes::MaxBytes){ntBuf=t.c_str();ntSaveFailed=false;repaintSettled();}}}
  else if(c=='\t'&&ntMode==NotesMode::Edit)ntAddItemToBuffer();
  else if(c>=32&&c<=126)notesAppend(std::string(1,c));
  return true;}
 if(ntMode==NotesMode::Options){
  if(down||up){ntOptFocus=constrain(ntOptFocus+(down?1:-1),0,mm::NoteOptionCount-1);render();return true;}
  if(ok||next||prev){ntOptStep(ntOptFocus);return true;}
  return false;}
 if(ntMode==NotesMode::Confirm){
  if(ok){if(ntEditFocus==0)ntDelete();else{ntMode=NotesMode::View;render();}return true;}
  if(down||up||next||prev||c=='\t'){ntEditFocus=1-ntEditFocus;render();return true;}
  return false;}
 if(ntMode==NotesMode::List){
  int n=int(ntList.size());
  if(down||up){ntFocus=mm::noteListMove(ntFocus,down?1:-1,n);ntScroll=mm_notes::scrollFor(n,ntFocus-2,ntScroll,mm::NoteRows);render();return true;}
  if(c==']'||c=='['){ntScroll=mm_notes::clampScroll(ntScroll+(c==']'?mm::NoteRows:-mm::NoteRows),n,mm::NoteRows);render();return true;}
  if(c=='n'){ntNew();return true;}if(c=='o'){ntOptions();return true;}
  if(ok||next){if(ntFocus==0)ntNew();else if(ntFocus==1)ntOptions();else if(ntFocus-2<n)ntOpenNote(ntList[ntFocus-2].stem);return true;}
  return false;}
 int items=ntItemCount();
 if((down||up)&&!items){auto rows=ntRows();ntVScroll=mm_notes::clampScroll(ntVScroll+(down?1:-1),int(rows.size()),mm::NoteViewRows);render();return true;}// a note without items: W / S scroll it
 if(down||up){ntVF=mm::noteViewStep(ntVF,down?1:-1,items);ntViewSync();render();return true;}
 if(next||c=='\t'){ntVF=mm::noteViewAction(ntVF,1,items);ntViewSync();render();return true;}
 if(prev){ntVF=mm::noteViewAction(ntVF,-1,items);ntViewSync();render();return true;}
 if(c==']'||c=='['){auto rows=ntRows();ntVScroll=mm_notes::clampScroll(ntVScroll+(c==']'?1:-1)*(mm::NoteViewRows-1),int(rows.size()),mm::NoteViewRows);render();return true;}
 if(ok){if(ntVF<items)ntToggle(mm_notes::itemLines(ntLines,ntShow)[ntVF]);else ntViewAct(ntVF-items);return true;}
 return false;}
bool notesTap(int x,int y){
 if(!ntPage()||mm::BackButton.contains(x,y))return false;
 if(ntMode==NotesMode::Edit||ntMode==NotesMode::Rename){
  if(mm::NoteEditDone.contains(x,y)){if(ntMode==NotesMode::Rename)ntRenameDone();else ntEditDone();}
  else if(mm::NoteEditSide.contains(x,y)){if(ntMode==NotesMode::Rename)ntRenameCancel();else ntAddItemToBuffer();}
  return true;}
 if(ntMode==NotesMode::Options){for(int i=0;i<mm::NoteOptionCount;i++)if(mm::noteOptionRect(i).contains(x,y)){ntOptFocus=i;ntOptStep(i);}return true;}
 if(ntMode==NotesMode::Confirm){if(mm::NotesDeleteYes.contains(x,y))ntDelete();else if(mm::NotesDeleteNo.contains(x,y)){ntMode=NotesMode::View;render();}return true;}
 if(ntMode==NotesMode::List){
  if(mm::NotesNew.contains(x,y)){ntFocus=0;ntNew();return true;}
  if(mm::NotesOptions.contains(x,y)){ntFocus=1;ntOptions();return true;}
  for(int i=0;i<mm::NoteRows;i++)if(mm::noteRowRect(i).contains(x,y)&&ntScroll+i<int(ntList.size())){ntFocus=ntScroll+i+2;ntOpenNote(ntList[ntScroll+i].stem);return true;}
  return true;}
 for(int i=0;i<mm::NoteActionCount;i++)if(mm::noteActionRect(i).contains(x,y)){ntVF=ntItemCount()+i;ntViewAct(i);return true;}
 auto rows=ntRows();int first=mm_notes::clampScroll(ntVScroll,int(rows.size()),mm::NoteViewRows);
 for(int i=0;i<mm::NoteViewRows&&first+i<int(rows.size());i++)if(mm::noteBodyRow(i).contains(x,y)){int line=rows[first+i].line;if(rows[first+i].kind!=mm_notes::Kind::Text){auto items=mm_notes::itemLines(ntLines,ntShow);for(size_t k=0;k<items.size();k++)if(items[k]==line)ntVF=int(k);ntToggle(line);}return true;}// any row of an item ticks it
 return true;}
// A swipe pages the list or the note.
bool notesScroll(int dy){
 if(!ntPage())return false;
 if(ntMode==NotesMode::List){int n=int(ntList.size());ntScroll=mm_notes::clampScroll(ntScroll+(dy<0?mm::NoteRows:-mm::NoteRows),n,mm::NoteRows);render();return true;}
 if(ntMode==NotesMode::View){auto rows=ntRows();ntVScroll=mm_notes::clampScroll(ntVScroll+(dy<0?mm::NoteViewRows-1:-(mm::NoteViewRows-1)),int(rows.size()),mm::NoteViewRows);render();return true;}
 return false;}
// Serial (tests): "notes status|list|show <name>|pin <name>|pin none".
bool notesSerial(const String &s){
 if(!s.startsWith("notes"))return false;
 if(s=="notes list"){ntRefresh();for(auto &i:ntList)Serial.printf("NOTE %s title=\"%s\" done=%d total=%d modified=%lu pinned=%d\n",i.stem.c_str(),i.title.c_str(),i.done,i.total,(unsigned long)i.modified,i.stem==ntPin);Serial.println("NOTES_END");}
 else if(s.startsWith("notes show ")){std::string t;bool ok=sdReady&&mm_notes::load(SD,s.substring(11).c_str(),t);Serial.printf("NOTE_TEXT ok=%d bytes=%u\n",ok,unsigned(t.size()));Serial.println(t.c_str());Serial.println("NOTE_TEXT_END");}
 else if(s.startsWith("notes pin ")){String name=s.substring(10);bool ok;if(name=="none"){ok=device::prefPutString("mmnotes","pin",String());if(ok)ntPin.clear();}else{ok=sdReady&&mm_notes::noteExists(SD,name.c_str())&&device::prefPutString("mmnotes","pin",name);if(ok){ntPin=name.c_str();setHomeMain(mm::MainNote);homeMainFocus=homeMain;}}ntHomeRefresh();Serial.printf("NOTES_PIN ok=%d pin=%s home_bytes=%u\n",ok,ntPin.c_str(),unsigned(ntHome.length()));}
 else Serial.printf("NOTES mode=%d stem=%s pin=%s home_bytes=%u sort=%d show_checked=%d new_type=%d notes=%u fresh=%d direct=%d\n",int(ntMode),ntStem.c_str(),ntPin.c_str(),unsigned(ntHome.length()),ntSort,ntShow,ntType,unsigned(ntList.size()),ntFresh,ntDirect);
 return true;}
