#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "theme.h"
#include "services/backup_format.h"
// Settings > Backup / restore (59). view 0: Create backup card, three option rows, Restore row; 1: the backups on the SD card
// (four rows a page); 2: what a restore replaces, RESTORE / CANCEL; 3: the passphrase editor (creating or opening a backup).
namespace mm {
constexpr Rect BackupCreate=SettingsSwitch,BackupPasswords={Margin,Top+70,W-2*Margin,28},BackupKey={Margin,Top+101,W-2*Margin,28},BackupEncrypt={Margin,Top+132,W-2*Margin,28},BackupRestore={Margin,Top+184,W-2*Margin,34};
constexpr int BackupRows=5,BackupLabelY=Top+168,BackupInfoY=Top+226;// focus: 0 create, 1 passwords, 2 private key, 3 encrypt, 4 restore
constexpr Rect backupRowRect(int row){return row==0?BackupCreate:row==1?BackupPasswords:row==2?BackupKey:row==3?BackupEncrypt:BackupRestore;}
constexpr Rect backupPill(Rect r){return {r.x+r.w-62,r.y+3,54,22};}
constexpr int BackupVisible=4,BackupRowH=52;
constexpr Rect backupListRow(int slot){return {Margin,Top+4+slot*BackupRowH,W-2*Margin,BackupRowH-4};}
constexpr Rect BackupPrev={W-Margin-72,Top+4+BackupVisible*BackupRowH+4,34,24},BackupNext={W-Margin-34,Top+4+BackupVisible*BackupRowH+4,34,24};
constexpr Rect BackupConfirmCard={Margin,Top+4,W-2*Margin,196},BackupPassCard={Margin,Top+60,W-2*Margin,36};
constexpr Rect BackupGo={Margin,Top+212,(W-2*Margin-6)/2,34},BackupCancel={W/2+3,Top+212,(W-2*Margin-6)/2,34};
struct BackupRow {std::string when,meta;};
struct BackupView {
 int view=0,focus=0,scroll=0,selected=0,button=0;bool passwords=true,key=false,keyAvailable=false,encrypt=false,sd=true,creating=false;
 std::string status,last,title,hint,input;std::vector<BackupRow> rows;std::vector<std::string> lines;
};
inline int backupMaxScroll(int count){return count>BackupVisible?(count-1)/BackupVisible*BackupVisible:0;}
inline int backupFollow(int scroll,int selected){return selected/BackupVisible*BackupVisible;(void)scroll;}
inline int backupPageScroll(int scroll,int count,bool down){return down?std::min(backupMaxScroll(count),scroll+BackupVisible):std::max(0,scroll-BackupVisible);}
inline std::string backupSizeText(uint32_t bytes){char t[16];if(bytes<1024)snprintf(t,sizeof t,"%u B",unsigned(bytes));else if(bytes<1048576)snprintf(t,sizeof t,"%u KB",unsigned((bytes+512)/1024));else snprintf(t,sizeof t,"%.1f MB",bytes/1048576.0);return t;}
inline int backupFollow(int selected){return selected/BackupVisible*BackupVisible;}
inline std::string backupMeta(const device::backup::Info &i){return std::to_string(i.nvsCount)+" settings, "+std::to_string(i.fileCount)+(i.fileCount==1?" file, ":" files, ")+backupSizeText(i.size)+(i.encrypted?" / locked":"");}
// What a restore replaces, one line each; the identity line warns because the node's address changes.
inline std::vector<std::string> backupConfirmLines(const device::backup::Contents &c){
 std::vector<std::string> out;bool identity=false,contacts=false,channels=false,routes=false,radio=false;
 for(auto &f:c.files){if(f.path.find("identity")!=std::string::npos)identity=true;else if(f.path.find("contacts")!=std::string::npos)contacts=true;else if(f.path.find("bindings")!=std::string::npos||f.path.find("catalog")!=std::string::npos)channels=true;else if(f.path.find("dmroutes")!=std::string::npos)routes=true;else if(f.path.find("radio")!=std::string::npos)radio=true;}
 out.push_back("Settings: "+std::to_string(c.nvs.size())+" entries");
 if(radio)out.push_back("Radio settings");if(contacts)out.push_back("Contacts");if(channels)out.push_back("Channels and keys");if(routes)out.push_back("Direct message routes");
 out.push_back(c.flags&device::backup::Passwords?"Wi-Fi, cellular, link passwords":"Passwords stay as they are");
 out.push_back(identity?"PRIVATE KEY: your address changes":"Private key stays as it is");
 return out;}
}
