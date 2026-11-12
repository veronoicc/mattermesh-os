// Settings > Backup / restore (59): one versioned file /mattermesh/backup/mm-YYYYMMDD-HHMMSS.mmb with every NVS entry of the
// firmware (nvs_entry_find over all "mm*" namespaces) plus the contact, channel, route, radio and identity files on the SD card.
// Restore lists the files, checks one completely (CRC, tag, every record) before anything is written, shows what it replaces,
// stages and swaps the files, writes the NVS entries (every old value kept, put back if one write fails) and restarts.
// Format: services/backup_format.h, layout: ui/backup_layout.h, drawing: ui/backup_renderer.inc. Included by main.cpp after security_app.inc.
#include "services/backup_format.h"
namespace bk=device::backup;
static int bkView=0,bkFocus=0,bkScroll=0,bkSel=0,bkButton=1,bkMode=0;// view 0 main, 1 list, 2 confirm, 3 passphrase (bkMode 0 for a new backup, 1 to open one)
static bool bkPw=true,bkKey=false,bkEnc=false;static String bkInput,bkStatus="Ready";
struct BkItem {std::string name;bk::Info info;};
static std::vector<BkItem> bkList;static std::vector<uint8_t> bkFile;static bk::Contents bkPending;static std::string bkPendingName;
static const char *BackupDir="/mattermesh/backup";
static bool bkPage(){return screen==Screen::Section&&section==mm::BackupSection;}
static std::string bkBakPath(const std::string &path){return path.substr(0,path.rfind('.'))+".bak";}
static bool bkKeyAvailable(){return bk::privateKeyAvailable();}
static void bkSaveOptions(){device::prefPutInt("mmbackup","pw",bkPw);device::prefPutInt("mmbackup","key",bkKey);device::prefPutInt("mmbackup","enc",bkEnc);}
static void bkScan(){
 bkList.clear();if(!sdReady)return;File dir=SD.open(BackupDir);if(!dir||!dir.isDirectory()){if(dir)dir.close();return;}
 for(File f=dir.openNextFile();f&&bkList.size()<64;f=dir.openNextFile()){
  std::string name=f.name();name=name.substr(name.rfind('/')+1);
  if(!f.isDirectory()&&bk::isBackupName(name)){uint8_t head[bk::detail::Crypt];size_t n=f.read(head,sizeof head);BkItem item;item.name=name;if(bk::peek(head,n,item.info)==bk::Status::Ok){item.info.size=uint32_t(f.size());bkList.push_back(item);}}
  f.close();}
 dir.close();std::sort(bkList.begin(),bkList.end(),[](const BkItem &a,const BkItem &b){return a.name>b.name;});
}
void backupOpen(){
 bkPw=device::prefGetInt("mmbackup","pw",1)!=0;bkKey=device::prefGetInt("mmbackup","key",0)!=0&&bkKeyAvailable();bkEnc=device::prefGetInt("mmbackup","enc",0)!=0;
 bkView=0;bkFocus=0;bkScroll=0;bkSel=0;bkButton=1;bkInput="";bkStatus=sdReady?"Ready":"No SD card";bkScan();
}
void backupLeft(){bkView=0;bkFocus=0;for(unsigned i=0;i<bkInput.length();i++)bkInput.setCharAt(i,0);bkInput="";std::vector<uint8_t>().swap(bkFile);bkPending=bk::Contents();bkList.clear();}
String *backupField(){return bkPage()&&bkView==3?&bkInput:nullptr;}
void backupCompose(mm::State &v){
 auto &b=v.backup;b.view=bkView;b.focus=bkFocus;b.scroll=bkScroll;b.selected=bkSel;b.button=bkButton;b.passwords=bkPw;b.keyAvailable=bkKeyAvailable();b.key=bkKey&&b.keyAvailable;b.encrypt=bkEnc;b.sd=sdReady;
 b.status=sdReady?bkStatus.c_str():"No SD card";b.hint=bkList.empty()?"No backups yet":(std::to_string(bkList.size())+(bkList.size()==1?" backup on the card":" backups on the card"));
 if(notice.length())b.lines.push_back(notice.c_str());if(!bkList.empty())b.lines.push_back("Last: "+bkList[0].name);
 for(auto &item:bkList)b.rows.push_back({bk::nameDate(item.name),mm::backupMeta(item.info)});
 b.title=bkView==2?bkPendingName:bkMode?"Open backup":"Encrypt backup";
 if(bkView==2){b.lines=mm::backupConfirmLines(bkPending);}
 if(bkView==3){b.hint=bkMode?"Passphrase of "+bkPendingName:"Choose a passphrase, 4 or more";b.input=std::string(bkInput.length(),'*');b.lines.clear();}
}
// ---- creating
static bool bkReadFile(const std::string &path,std::vector<uint8_t> &out){
 const std::string bak=bkBakPath(path);File f=SD.open(SD.exists(path.c_str())?path.c_str():bak.c_str());if(!f||f.isDirectory()){if(f)f.close();return false;}
 size_t n=f.size();out.resize(n);size_t got=n?f.read(out.data(),n):0;f.close();return got==n;
}
static bool bkTypeOf(PreferenceType t,nvs_type_t &out){switch(t){case PT_I8:out=NVS_TYPE_I8;break;case PT_U8:out=NVS_TYPE_U8;break;case PT_I16:out=NVS_TYPE_I16;break;case PT_U16:out=NVS_TYPE_U16;break;case PT_I32:out=NVS_TYPE_I32;break;case PT_U32:out=NVS_TYPE_U32;break;case PT_I64:out=NVS_TYPE_I64;break;case PT_U64:out=NVS_TYPE_U64;break;case PT_STR:out=NVS_TYPE_STR;break;case PT_BLOB:out=NVS_TYPE_BLOB;break;default:return false;}return true;}
static bool bkNvsRead(Preferences &p,const char *key,nvs_type_t t,bk::NvsRecord &r){
 auto number=[&](uint8_t type,const void *v,size_t n){r.type=type;r.data.assign(static_cast<const uint8_t*>(v),static_cast<const uint8_t*>(v)+n);return true;};
 switch(t){
  case NVS_TYPE_I8:{int8_t v=p.getChar(key);return number(bk::NvsI8,&v,1);}case NVS_TYPE_U8:{uint8_t v=p.getUChar(key);return number(bk::NvsU8,&v,1);}
  case NVS_TYPE_I16:{int16_t v=p.getShort(key);return number(bk::NvsI16,&v,2);}case NVS_TYPE_U16:{uint16_t v=p.getUShort(key);return number(bk::NvsU16,&v,2);}
  case NVS_TYPE_I32:{int32_t v=p.getInt(key);return number(bk::NvsI32,&v,4);}case NVS_TYPE_U32:{uint32_t v=p.getUInt(key);return number(bk::NvsU32,&v,4);}
  case NVS_TYPE_I64:{int64_t v=p.getLong64(key);return number(bk::NvsI64,&v,8);}case NVS_TYPE_U64:{uint64_t v=p.getULong64(key);return number(bk::NvsU64,&v,8);}
  case NVS_TYPE_STR:{String s=p.getString(key,"");r.type=bk::NvsStr;r.data.assign(s.c_str(),s.c_str()+s.length());return true;}
  case NVS_TYPE_BLOB:{size_t n=p.getBytesLength(key);if(!n)return false;r.data.resize(n);r.type=bk::NvsBlob;return p.getBytes(key,r.data.data(),n)==n;}
  default:return false;}
}
static bool bkNvsWrite(Preferences &p,const bk::NvsRecord &r){
 const char *key=r.key.c_str();const uint8_t *d=r.data.data();const size_t n=r.data.size();
 switch(r.type){
  case bk::NvsI8:{int8_t v;memcpy(&v,d,1);return p.putChar(key,v)==1;}case bk::NvsU8:{uint8_t v;memcpy(&v,d,1);return p.putUChar(key,v)==1;}
  case bk::NvsI16:{int16_t v;memcpy(&v,d,2);return p.putShort(key,v)==2;}case bk::NvsU16:{uint16_t v;memcpy(&v,d,2);return p.putUShort(key,v)==2;}
  case bk::NvsI32:{int32_t v;memcpy(&v,d,4);return p.putInt(key,v)==4;}case bk::NvsU32:{uint32_t v;memcpy(&v,d,4);return p.putUInt(key,v)==4;}
  case bk::NvsI64:{int64_t v;memcpy(&v,d,8);return p.putLong64(key,v)==8;}case bk::NvsU64:{uint64_t v;memcpy(&v,d,8);return p.putULong64(key,v)==8;}
  case bk::NvsStr:{std::string text(r.data.begin(),r.data.end());return p.putString(key,text.c_str())==n&&(n||p.isKey(key));}
  case bk::NvsBlob:if(!n)return !p.isKey(key)||p.remove(key);return p.putBytes(key,d,n)==n;}
 return false;
}
static std::vector<bk::NvsRecord> bkNvsCollect(){
 std::vector<bk::NvsRecord> out;
 for(nvs_iterator_t it=nvs_entry_find(NVS_DEFAULT_PART_NAME,nullptr,NVS_TYPE_ANY);it;){
  nvs_entry_info_t info;nvs_entry_info(it,&info);it=nvs_entry_next(it);
  bk::NvsRecord r;r.ns=info.namespace_name;r.key=info.key;if(!bk::restorableNvs(r.ns))continue;
  Preferences p;if(!p.begin(info.namespace_name,true))continue;if(bkNvsRead(p,info.key,info.type,r))out.push_back(std::move(r));p.end();}
 return out;
}
static bool bkMkdirs(){return (SD.exists("/mattermesh")||SD.mkdir("/mattermesh"))&&(SD.exists(BackupDir)||SD.mkdir(BackupDir));}
static bool bkCreate(const std::string &pass,std::string *nameOut=nullptr){
 if(!sdReady){bkStatus="No SD card";return false;}
 time_t now=time(nullptr);if(now<MM_EPOCH_MIN){bkStatus="Set the date first";return false;}
 bk::Contents c;c.created=uint32_t(now);c.flags=bkPw?bk::Passwords:0;c.nvs=bk::selectNvs(bkNvsCollect(),bkPw);
 static const char *files[]={"/mattermesh/contacts.tsv","/mattermesh/bindings.tsv","/mattermesh/dmroutes.tsv","/mattermesh/radio.conf","/maxchat/catalog.tsv","/mattermesh/identity.id"};
 for(auto path:files){bool identity=!strcmp(path,"/mattermesh/identity.id");if(identity&&!(bkKey&&bkKeyAvailable()))continue;
  bk::FileRecord f;f.path=path;if(!bkReadFile(path,f.data))continue;if(identity)c.flags|=bk::Identity;c.files.push_back(std::move(f));}
 uint8_t salt[16],iv[12];for(int i=0;i<16;i+=4){uint32_t r=esp_random();memcpy(salt+i,&r,4);}for(int i=0;i<12;i+=4){uint32_t r=esp_random();memcpy(iv+i,&r,4);}
 std::vector<uint8_t> bytes=bk::build(c,pass,salt,iv);
 struct tm tm;time_t t=now;localtime_r(&t,&tm);std::string name=bk::fileName(tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
 const std::string path=std::string(BackupDir)+"/"+name,part=path+".part";
 if(!bkMkdirs()){bkStatus="Cannot create the folder";return false;}
 SD.remove(part.c_str());File out=SD.open(part.c_str(),"w");bool ok=out&&out.write(bytes.data(),bytes.size())==bytes.size();if(out){out.flush();out.close();}
 if(ok){File check=SD.open(part.c_str());ok=check&&check.size()==bytes.size();if(check)check.close();}
 if(ok){SD.remove(path.c_str());ok=SD.rename(part.c_str(),path.c_str());}
 if(!ok){SD.remove(part.c_str());bkStatus="Backup failed / SD write";return false;}
 bkStatus=("Saved "+name).c_str();if(nameOut)*nameOut=name;bkScan();return true;
}
// ---- restoring
struct BkFileUndo {std::string path,bak;bool had;};
struct BkNvsUndo {bk::NvsRecord old;bool had;};
static void bkRevertFiles(const std::vector<BkFileUndo> &undo){for(auto it=undo.rbegin();it!=undo.rend();++it){SD.remove(it->path.c_str());if(it->had)SD.rename(it->bak.c_str(),it->path.c_str());}}
static void bkRevertNvs(const std::vector<BkNvsUndo> &undo){
 for(auto it=undo.rbegin();it!=undo.rend();++it){Preferences p;if(!p.begin(it->old.ns.c_str(),false))continue;if(it->had)bkNvsWrite(p,it->old);else if(p.isKey(it->old.key.c_str()))p.remove(it->old.key.c_str());p.end();}
}
static bool bkApplyFiles(const std::vector<bk::FileRecord> &files,std::vector<BkFileUndo> &undo){
 if(!SD.exists("/mattermesh"))SD.mkdir("/mattermesh");if(!SD.exists("/maxchat"))SD.mkdir("/maxchat");
 auto dropStaged=[&]{for(auto &f:files)SD.remove((f.path+".rst").c_str());};
 for(auto &f:files){File out=SD.open((f.path+".rst").c_str(),"w");bool ok=out&&(f.data.empty()||out.write(f.data.data(),f.data.size())==f.data.size());if(out){out.flush();out.close();}
  if(ok){File check=SD.open((f.path+".rst").c_str());ok=check&&check.size()==f.data.size();if(check)check.close();}
  if(!ok){dropStaged();return false;}}
 for(auto &f:files){const std::string bak=bkBakPath(f.path);undo.push_back({f.path,bak,SD.exists(f.path.c_str())});
  if(!device::sdReplace(SD,(f.path+".rst").c_str(),f.path.c_str(),bak.c_str())){undo.pop_back();dropStaged();return false;}}
 return true;
}
static bool bkApplyNvs(const std::vector<bk::NvsRecord> &records,std::vector<BkNvsUndo> &undo){
 for(auto &r:records){
  Preferences p;if(!p.begin(r.ns.c_str(),false))return false;
  BkNvsUndo u;u.old.ns=r.ns;u.old.key=r.key;u.had=false;nvs_type_t t;if(p.isKey(r.key.c_str())&&bkTypeOf(p.getType(r.key.c_str()),t))u.had=bkNvsRead(p,r.key.c_str(),t,u.old);
  undo.push_back(u);bool ok=bkNvsWrite(p,r);p.end();if(!ok)return false;}
 return true;
}
// bkPending was parsed whole (CRC, tag, every record) before this runs; a failed write puts everything back.
static bool bkRestore(){
 std::vector<BkFileUndo> files;std::vector<BkNvsUndo> nvs;
 if(!(bkApplyFiles(bkPending.files,files)&&bkApplyNvs(bkPending.nvs,nvs))){bkRevertNvs(nvs);bkRevertFiles(files);bkStatus="Restore failed / nothing changed";return false;}
 Serial.printf("BACKUP_RESTORED settings=%u files=%u\n",unsigned(bkPending.nvs.size()),unsigned(bkPending.files.size()));Serial.flush();
 return true;
}
static void bkRestartAfterRestore(){notice="Restored / restarting";render();delay(1200);ESP.restart();}
// Reads and checks one file; encrypted ones wait for the passphrase (view 3). false: bkStatus says why.
static bool bkLoad(const std::string &name,const std::string &pass){
 const std::string path=std::string(BackupDir)+"/"+name;File f=SD.open(path.c_str());if(!f||f.isDirectory()){if(f)f.close();bkStatus="Cannot read the file";return false;}
 size_t n=f.size();if(n>bk::MaxBody+256){f.close();bkStatus="File too large";return false;}
 bkFile.resize(n);size_t got=n?f.read(bkFile.data(),n):0;f.close();if(got!=n){bkStatus="Cannot read the file";return false;}
 bk::Info info;auto s=bk::inspect(bkFile.data(),bkFile.size(),info);
 if(s==bk::Status::Ok)s=bk::parse(bkFile.data(),bkFile.size(),pass,bkPending);
 if(s!=bk::Status::Ok){bkStatus=bk::statusText(s);return false;}
 bkPendingName=name;return true;
}
static void bkSelect(int index){
 if(index<0||index>=int(bkList.size()))return;const BkItem &item=bkList[index];bkPendingName=item.name;bkStatus="";
 if(item.info.encrypted){bkMode=1;bkView=3;bkInput="";bkButton=0;render();return;}
 if(bkLoad(item.name,"")){bkView=2;bkButton=1;notice="";}else{notice=bkStatus;}
 render();
}
static void bkPassSubmit(){
 const std::string pass=bkInput.c_str();
 if(bkMode==0){if(pass.size()<4){bkStatus="Use 4 or more characters";render();return;}
  bool ok=bkCreate(pass);bkInput="";bkView=0;notice=bkStatus;(void)ok;render();return;}
 if(pass.empty()){bkStatus="Type the passphrase";render();return;}
 if(bkLoad(bkPendingName,pass)){bkInput="";bkView=2;bkButton=1;bkStatus="";}else bkInput="";// a wrong try starts the field again
 render();
}
static void bkAction(int row){
 notice="";
 if(row==0){if(!sdReady){notice="No SD card";render();return;}if(bkEnc){bkMode=0;bkView=3;bkInput="";bkButton=0;bkStatus="";render();return;}bkCreate("");notice=bkStatus;render();return;}
 if(row==1){bkPw=!bkPw;bkSaveOptions();}
 else if(row==2){if(!bkKeyAvailable())notice="Private key export is off in this build";else{bkKey=!bkKey;bkSaveOptions();if(bkKey)notice="The private key is in the file: keep it safe";}}
 else if(row==3){bkEnc=!bkEnc;bkSaveOptions();}
 else if(row==4){bkScan();bkView=1;bkSel=0;bkScroll=0;if(bkList.empty())notice="No backups in /mattermesh/backup";}
 render();
}
static void bkListMove(int delta){if(bkList.empty())return;bkSel=constrain(bkSel+delta,0,int(bkList.size())-1);bkScroll=mm::backupFollow(bkSel);render();}
static void bkConfirmGo(){if(bkRestore())bkRestartAfterRestore();else{bkView=0;notice=bkStatus;render();}}
bool backupBack(){
 if(!bkPage()||bkView==0)return false;
 if(bkView==3){bkInput="";bkView=bkMode?1:0;}else if(bkView==2){bkPending=bk::Contents();bkView=1;}else bkView=0;
 notice="";render();return true;
}
bool backupKey(char c){
 if(!bkPage())return false;
 if(bkView==3){if(c=='\n'){bkPassSubmit();return true;}if(c>=32&&c<=126&&bkInput.length()<48){bkInput+=c;bkStatus="";repaintSettled();return true;}return false;}
 if(c=='\b'||c=='a')return false;// Back
 if(bkView==2){if(c=='h'||c=='k'||c=='w')bkButton=0;else if(c=='l'||c=='j'||c=='s'||c=='d')bkButton=1;else if(c=='\n'||c==' '){if(bkButton==0)bkConfirmGo();else backupBack();return true;}else return false;render();return true;}
 if(bkView==1){if(c=='j'||c=='s')bkListMove(1);else if(c=='k'||c=='w')bkListMove(-1);else if(c=='\n'||c=='l'||c=='d'||c==' ')bkSelect(bkSel);else if(c=='['||c==']')bkListMove(c==']'?mm::BackupVisible:-mm::BackupVisible);else return false;return true;}
 if(c=='j'||c=='s'){bkFocus=(bkFocus+1)%mm::BackupRows;render();return true;}
 if(c=='k'||c=='w'){bkFocus=(bkFocus+mm::BackupRows-1)%mm::BackupRows;render();return true;}
 if(c=='l'||c=='d'||c=='\n'||c==' '){bkAction(bkFocus);return true;}
 return false;
}
bool backupTap(int x,int y){
 if(!bkPage())return false;
 if(mm::BackButton.contains(x,y))return false;
 if(bkView==3){if(mm::BackupGo.contains(x,y))bkPassSubmit();else if(mm::BackupCancel.contains(x,y))backupBack();return true;}
 if(bkView==2){if(mm::BackupGo.contains(x,y)){bkButton=0;bkConfirmGo();}else if(mm::BackupCancel.contains(x,y))backupBack();return true;}
 if(bkView==1){
  for(int slot=0;slot<mm::BackupVisible;slot++)if(mm::backupListRow(slot).contains(x,y)&&bkScroll+slot<int(bkList.size())){bkSel=bkScroll+slot;bkSelect(bkSel);return true;}
  if(mm::BackupPrev.contains(x,y)||mm::BackupNext.contains(x,y)){bkScroll=mm::backupPageScroll(bkScroll,int(bkList.size()),mm::BackupNext.contains(x,y));bkSel=constrain(bkSel,bkScroll,std::min(int(bkList.size())-1,bkScroll+mm::BackupVisible-1));render();}
  return true;}
 for(int row=0;row<mm::BackupRows;row++)if(mm::backupRowRect(row).contains(x,y)){bkFocus=row;bkAction(row);return true;}
 return true;
}
// Harness commands (headless tests): backup create [passphrase], backup list, backup restore NAME [passphrase], backup status.
bool backupSerial(const String &s){
 if(s!="backup"&&!s.startsWith("backup "))return false;
 if(s.startsWith("backup create")){String pass=s.length()>14?s.substring(14):String();std::string name;bool ok=bkCreate(pass.c_str(),&name);Serial.printf("BACKUP_CREATE ok=%d name=%s status=%s\n",ok,name.c_str(),bkStatus.c_str());return true;}
 if(s=="backup list"){bkScan();for(auto &item:bkList)Serial.printf("BACKUP_ROW name=%s settings=%u files=%u size=%lu encrypted=%d\n",item.name.c_str(),item.info.nvsCount,item.info.fileCount,(unsigned long)item.info.size,item.info.encrypted);Serial.printf("BACKUP_LIST count=%u\n",unsigned(bkList.size()));return true;}
 if(s.startsWith("backup restore ")){String rest=s.substring(15);int sp=rest.indexOf(' ');String name=sp<0?rest:rest.substring(0,sp),pass=sp<0?String():rest.substring(sp+1);bool ok=bk::isBackupName(name.c_str())&&bkLoad(name.c_str(),pass.c_str())&&bkRestore();Serial.printf("BACKUP_RESTORE ok=%d status=%s\n",ok,bkStatus.c_str());if(ok){Serial.flush();delay(300);ESP.restart();}return true;}
 Serial.printf("BACKUP_STATUS view=%d passwords=%d key=%d key_available=%d encrypt=%d sd=%d count=%u status=%s\n",bkView,bkPw,bkKey,bkKeyAvailable(),bkEnc,sdReady,unsigned(bkList.size()),bkStatus.c_str());return true;
}
