#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
using namespace mm;
using namespace device::backup;
int main(){
 auto inside=[](Rect r){return r.x>=Margin&&r.x+r.w<=W-Margin&&r.y>=Top&&r.y+r.h<=Bottom;};
 auto apart=[](Rect a,Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 assert(BackupCreate.y==SettingsSwitch.y&&BackupCreate.h==SettingsSwitch.h);
 // Main view.
 {std::vector<Rect> t;for(int i=0;i<BackupRows;i++)t.push_back(backupRowRect(i));for(size_t i=0;i<t.size();i++){assert(inside(t[i]));for(size_t j=i+1;j<t.size();j++)assert(apart(t[i],t[j]));}
  assert(SettingsSectionY>=BackupCreate.y+BackupCreate.h&&SettingsSectionY+13<=BackupPasswords.y&&BackupLabelY>=BackupEncrypt.y+BackupEncrypt.h&&BackupLabelY+13<=BackupRestore.y&&BackupInfoY>=BackupRestore.y+BackupRestore.h&&BackupInfoY+3*13<=H);}
 // List view: four rows and the pager.
 {std::vector<Rect> t;for(int i=0;i<BackupVisible;i++)t.push_back(backupListRow(i));t.push_back(BackupPrev);t.push_back(BackupNext);for(size_t i=0;i<t.size();i++){assert(inside(t[i]));for(size_t j=i+1;j<t.size();j++)assert(apart(t[i],t[j]));}}
 // Confirm / editor buttons and the field.
 {std::vector<Rect> t={BackupGo,BackupCancel};for(size_t i=0;i<t.size();i++){assert(inside(t[i]));for(size_t j=i+1;j<t.size();j++)assert(apart(t[i],t[j]));}assert(inside(BackupConfirmCard)&&inside(BackupPassCard)&&apart(BackupConfirmCard,BackupGo)&&apart(BackupConfirmCard,BackupCancel));}
 // Paging.
 assert(backupMaxScroll(0)==0&&backupMaxScroll(4)==0&&backupMaxScroll(5)==4&&backupMaxScroll(9)==8&&backupMaxScroll(8)==4);
 assert(backupPageScroll(0,9,true)==4&&backupPageScroll(4,9,true)==8&&backupPageScroll(8,9,true)==8&&backupPageScroll(8,9,false)==4&&backupPageScroll(0,9,false)==0);
 assert(backupFollow(0)==0&&backupFollow(3)==0&&backupFollow(4)==4&&backupFollow(7)==4&&backupFollow(8)==8);
 {int seen=BackupVisible,scroll=0;while(scroll<backupMaxScroll(11)){int next=backupPageScroll(scroll,11,true);assert(next>scroll&&next<=seen);scroll=next;seen=scroll+BackupVisible;}assert(seen>=11);}
 // Texts.
 assert(backupSizeText(0)=="0 B"&&backupSizeText(1023)=="1023 B"&&backupSizeText(1024)=="1 KB"&&backupSizeText(18*1024)=="18 KB"&&backupSizeText(1048576)=="1.0 MB"&&backupSizeText(3*1048576+524288)=="3.5 MB");
 {Info i;i.nvsCount=41;i.fileCount=5;i.size=18*1024;assert(backupMeta(i)=="41 settings, 5 files, 18 KB");i.fileCount=1;i.encrypted=true;assert(backupMeta(i)=="41 settings, 1 file, 18 KB / locked");}
 {Contents c;c.flags=Passwords;c.nvs.resize(3);c.files={{"/mattermesh/contacts.tsv",{}},{"/maxchat/catalog.tsv",{}},{"/mattermesh/radio.conf",{}}};
  auto l=backupConfirmLines(c);assert(l[0]=="Settings: 3 entries");auto has=[&](const char *t){for(auto &s:l)if(s==t)return true;return false;};
  assert(has("Radio settings")&&has("Contacts")&&has("Channels and keys")&&!has("Direct message routes")&&has("Wi-Fi, cellular, link passwords")&&has("Private key stays as it is"));
  c.files.push_back({"/mattermesh/identity.id",{}});c.files.push_back({"/mattermesh/dmroutes.tsv",{}});c.flags=Identity;l=backupConfirmLines(c);assert(has("Direct message routes")&&has("PRIVATE KEY: your address changes")&&has("Passwords stay as they are")&&!has("Wi-Fi, cellular, link passwords"));assert(l.size()<=7);}
 GFXcanvas1 canvas(W,H);Renderer renderer(canvas);
 auto shot=[&](const State &s){renderer.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+W*H/8);};
 auto base=[&](){State s;s.page=Page::Backup;s.backup.status="Ready";s.backup.hint="3 backups on the card";s.backup.lines={"Last: mm-20261112-090503.mmb"};s.backup.keyAvailable=true;s.backup.rows={{"2026-11-12 09:05","41 settings, 5 files, 18 KB"},{"2026-11-11 21:40","41 settings, 5 files, 18 KB / locked"},{"2026-11-10 07:00","40 settings, 4 files, 9 KB"},{"2026-11-09 07:00","a"},{"2026-11-08 07:00","b"}};
  s.backup.lines.push_back("l2");s.backup.lines.push_back("l3");s.backup.lines.push_back("l4-ignored");s.backup.title="mm-20261112-090503.mmb";s.backup.lines.shrink_to_fit();return s;};
 // Every view renders with long strings, from any scroll and focus.
 {State s=base();std::string l(80,'W');s.backup.status=l;s.backup.hint=l;s.backup.title=l;s.backup.input=l;for(auto &x:s.backup.lines)x=l;for(auto &r:s.backup.rows){r.when=l;r.meta=l;}
  for(int view=0;view<4;view++)for(int f=0;f<BackupRows;f++)for(int sc:{0,4,8})for(int sel:{0,4})for(int on=0;on<2;on++){s.backup.view=view;s.backup.focus=f;s.backup.scroll=sc;s.backup.selected=sel;s.backup.key=on;s.backup.keyAvailable=on;s.backup.encrypt=on;s.backup.passwords=on;s.backup.sd=on;s.backup.button=on;shot(s);}
  s.backup.rows.clear();s.backup.view=1;shot(s);s.backup.lines.assign(20,"x");s.backup.view=2;shot(s);}
 // Main view follows the state.
 {State a=base();auto ref=shot(a);State b=base();b.backup.passwords=false;assert(shot(b)!=ref);b=base();b.backup.key=true;assert(shot(b)!=ref);b=base();b.backup.encrypt=true;assert(shot(b)!=ref);b=base();b.backup.sd=false;assert(shot(b)!=ref);b=base();b.backup.keyAvailable=false;assert(shot(b)!=ref);
  b=base();b.backup.keyAvailable=false;auto off=shot(b);b.backup.key=true;assert(shot(b)==off);// no private key option in a build that cannot export it
  b=base();b.backup.status="Saved";assert(shot(b)!=ref);b=base();b.backup.hint="none";assert(shot(b)!=ref);b=base();b.backup.lines[0]="other";assert(shot(b)!=ref);
  for(int f=0;f<BackupRows;f++){State s=base();s.backup.focus=f;renderer.draw(s);Rect r=backupRowRect(f);assert(canvas.getPixel(3,r.y+r.h/2));for(int o=0;o<BackupRows;o++)if(o!=f){Rect q=backupRowRect(o);assert(!canvas.getPixel(3,q.y+q.h/2));}}}
 // List: the selected row is inverted, the page changes with the scroll, an empty list says so.
 {State a=base();a.backup.view=1;a.backup.selected=1;auto ref=shot(a);Rect r1=backupListRow(1),r0=backupListRow(0);assert(canvas.getPixel(r1.x+3,r1.y+r1.h/2)&&!canvas.getPixel(r0.x+3,r0.y+r0.h/2));
  State b=a;b.backup.selected=0;assert(shot(b)!=ref);b=a;b.backup.scroll=4;b.backup.selected=4;assert(shot(b)!=ref);b=a;b.backup.rows.clear();assert(shot(b)!=ref);
  b=a;b.backup.rows[1].meta="other";assert(shot(b)!=ref);}
 // Confirm and editor.
 {State a=base();a.backup.view=2;a.backup.lines={"Settings: 3 entries","Contacts"};auto ref=shot(a);State b=a;b.backup.button=1;assert(shot(b)!=ref);b=a;b.backup.lines.push_back("PRIVATE KEY: your address changes");assert(shot(b)!=ref);
  State e=base();e.backup.view=3;e.backup.title="Passphrase";e.backup.hint="to open this backup";auto e0=shot(e);e.backup.input="ab";auto e1=shot(e);assert(e1!=e0);e.backup.input="abc";assert(shot(e)!=e1);State f=e;f.backup.status="Too short";assert(shot(f)!=shot(e));f=e;f.backup.button=1;assert(shot(f)!=shot(e));}
 std::cout<<"PASS: Backup / restore touch targets inside and apart for all four views, paging, size and summary texts, state changes repaint, private key option unavailable without export\n";
}
