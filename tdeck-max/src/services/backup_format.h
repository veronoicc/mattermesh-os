#pragma once
#include <algorithm>
#include <stdint.h>
#include <string>
#include <vector>
#include "mm_crypto.h"
// Settings > Backup / restore: the .mmb file (/mattermesh/backup/mm-YYYYMMDD-HHMMSS.mmb). Pure format code, no hardware.
//   header  "MMBK", version, flags, NVS record count u16, file record count u16, u16 0, created (UTC epoch) u32, body length u32,
//           encrypted files add: PBKDF2 iterations u32, salt 16, iv 12
//   body    records: NVS (1, namespace, key, type, length u32, data) and file (2, path length u16, path, length u32, data)
//   tail    encrypted: AES-256-GCM tag 16 (the header is the authenticated data); always: CRC-32 of everything before it
// Numbers are little endian. parse() writes nothing to its output unless the whole file is valid, so a restore never applies half a file.
#ifndef ENABLE_PRIVATE_KEY_EXPORT
#define ENABLE_PRIVATE_KEY_EXPORT 0
#endif
namespace device { namespace backup {
constexpr uint8_t Version=1;constexpr uint32_t MaxBody=6u<<20,BackupIterations=20000;
enum Flags {Encrypted=1,Passwords=2,Identity=4};
enum NvsType {NvsI8=1,NvsU8,NvsI16,NvsU16,NvsI32,NvsU32,NvsI64,NvsU64,NvsStr,NvsBlob};
struct NvsRecord {std::string ns,key;uint8_t type;std::vector<uint8_t> data;};
struct FileRecord {std::string path;std::vector<uint8_t> data;};
struct Contents {uint32_t created=0;uint8_t flags=0;std::vector<NvsRecord> nvs;std::vector<FileRecord> files;};
struct Info {uint8_t version=0;bool encrypted=false,passwords=false,identity=false;uint32_t created=0,size=0;uint16_t nvsCount=0,fileCount=0;};
enum class Status {Ok,Truncated,BadMagic,BadVersion,BadCrc,NeedPassphrase,WrongPassphrase,BadRecord};
// What a backup holds. The PIN namespace is never saved; Wi-Fi networks and the credentials of the modem, MQTT and the links go only with passwords.
struct Options {bool passwords=true,privateKey=false,encrypt=false;};
inline constexpr bool privateKeyAvailable(){return ENABLE_PRIVATE_KEY_EXPORT;}
inline bool secretKey(const std::string &ns,const std::string &key){
 return (ns=="mmconnect"&&key=="networks")||(ns=="mmcell"&&(key=="pin"||key=="apnp"||key=="mpass"))||(ns=="mmble"&&key=="pin")||(ns=="mmrlink"&&key=="pin")||(ns=="mmweb"&&(key=="code"||key=="tok"));}
inline bool restorableNvs(const std::string &ns){return ns.size()>2&&ns[0]=='m'&&ns[1]=='m'&&ns!="mmsec";}
inline std::vector<NvsRecord> selectNvs(const std::vector<NvsRecord> &all,bool passwords){std::vector<NvsRecord> out;for(auto &r:all)if(restorableNvs(r.ns)&&(passwords||!secretKey(r.ns,r.key)))out.push_back(r);return out;}
// The only files a backup carries and a restore writes: contacts, channels and their keys, direct-message routes, radio settings, identity.
inline bool allowedPath(const std::string &p){static const char *list[]={"/mattermesh/identity.id","/mattermesh/contacts.tsv","/mattermesh/bindings.tsv","/mattermesh/dmroutes.tsv","/mattermesh/radio.conf","/maxchat/catalog.tsv"};for(auto *l:list)if(p==l)return true;return false;}
inline std::string fileName(int y,int mo,int d,int h,int mi,int s){char t[32];snprintf(t,sizeof t,"mm-%04d%02d%02d-%02d%02d%02d.mmb",y,mo,d,h,mi,s);return t;}
inline bool isBackupName(const std::string &n){if(n.size()!=22||n.compare(0,3,"mm-")||n[11]!='-'||n.compare(18,4,".mmb"))return false;for(size_t i=3;i<18;i++)if(i!=11&&(n[i]<'0'||n[i]>'9'))return false;return true;}
inline std::string nameDate(const std::string &n){return isBackupName(n)?n.substr(3,4)+"-"+n.substr(7,2)+"-"+n.substr(9,2)+" "+n.substr(12,2)+":"+n.substr(14,2):std::string();}
namespace detail {
constexpr size_t Plain=20,Crypt=52;
inline void put16(std::vector<uint8_t> &o,uint32_t v){o.push_back(uint8_t(v));o.push_back(uint8_t(v>>8));}
inline void put32(std::vector<uint8_t> &o,uint32_t v){for(int i=0;i<4;i++)o.push_back(uint8_t(v>>(8*i)));}
inline uint32_t get32(const uint8_t *p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
inline uint32_t get16(const uint8_t *p){return uint32_t(p[0])|uint32_t(p[1])<<8;}
inline size_t numericSize(uint8_t t){switch(t){case NvsI8:case NvsU8:return 1;case NvsI16:case NvsU16:return 2;case NvsI32:case NvsU32:return 4;case NvsI64:case NvsU64:return 8;}return 0;}
inline bool name(const std::string &s){if(s.empty()||s.size()>15)return false;for(unsigned char c:s)if(c<0x21||c>0x7e)return false;return true;}
struct Reader {
 const uint8_t *p;size_t n,at;bool ok;
 Reader(const uint8_t *data,size_t size):p(data),n(size),at(0),ok(true){}
 bool need(size_t k){if(!ok||n-at<k)ok=false;return ok;}
 uint32_t u8(){return need(1)?p[at++]:0;}
 uint32_t u16(){if(!need(2))return 0;uint32_t v=get16(p+at);at+=2;return v;}
 uint32_t u32(){if(!need(4))return 0;uint32_t v=get32(p+at);at+=4;return v;}
 const uint8_t *take(size_t k){if(!need(k))return nullptr;const uint8_t *r=p+at;at+=k;return r;}
};
}
inline std::vector<uint8_t> build(const Contents &c,const std::string &passphrase,const uint8_t salt[16],const uint8_t iv[12],uint32_t iterations=BackupIterations){
 using namespace detail;std::vector<uint8_t> body;
 for(auto &r:c.nvs){body.push_back(1);body.push_back(uint8_t(r.ns.size()));body.insert(body.end(),r.ns.begin(),r.ns.end());body.push_back(uint8_t(r.key.size()));body.insert(body.end(),r.key.begin(),r.key.end());body.push_back(r.type);put32(body,uint32_t(r.data.size()));body.insert(body.end(),r.data.begin(),r.data.end());}
 for(auto &f:c.files){body.push_back(2);put16(body,uint32_t(f.path.size()));body.insert(body.end(),f.path.begin(),f.path.end());put32(body,uint32_t(f.data.size()));body.insert(body.end(),f.data.begin(),f.data.end());}
 const bool sealed=!passphrase.empty();std::vector<uint8_t> out={'M','M','B','K',Version,uint8_t((c.flags&~Encrypted)|(sealed?Encrypted:0))};
 put16(out,uint32_t(c.nvs.size()));put16(out,uint32_t(c.files.size()));put16(out,0);put32(out,c.created);put32(out,uint32_t(body.size()));
 uint8_t tag[16];
 if(sealed){put32(out,iterations);out.insert(out.end(),salt,salt+16);out.insert(out.end(),iv,iv+12);uint8_t key[32];mmcrypto::pbkdf2Sha256(reinterpret_cast<const uint8_t*>(passphrase.data()),passphrase.size(),salt,16,iterations,key,32);
  mmcrypto::gcmEncrypt(key,iv,out.data(),out.size(),body.data(),body.size(),body.data(),tag);}
 out.insert(out.end(),body.begin(),body.end());if(sealed)out.insert(out.end(),tag,tag+16);
 put32(out,mmcrypto::crc32(out.data(),out.size()));return out;
}
// Header and CRC only: what the restore list shows, readable without the passphrase.
inline Status inspect(const uint8_t *f,size_t n,Info &info){
 using namespace detail;if(n<4)return Status::Truncated;if(memcmp(f,"MMBK",4))return Status::BadMagic;if(n<6)return Status::Truncated;if(f[4]!=Version)return Status::BadVersion;
 const bool sealed=f[5]&Encrypted;const size_t head=sealed?Crypt:Plain;if(n<head)return Status::Truncated;
 uint32_t body=get32(f+16);if(body>MaxBody)return Status::BadRecord;size_t total=head+body+(sealed?16:0)+4;if(n<total)return Status::Truncated;if(n>total)return Status::BadRecord;
 if(mmcrypto::crc32(f,n-4)!=get32(f+n-4))return Status::BadCrc;
 info.version=f[4];info.encrypted=sealed;info.passwords=f[5]&Passwords;info.identity=f[5]&Identity;info.nvsCount=uint16_t(get16(f+6));info.fileCount=uint16_t(get16(f+8));info.created=get32(f+12);info.size=uint32_t(n);return Status::Ok;
}
inline const char *statusText(Status s){switch(s){case Status::Ok:return "OK";case Status::Truncated:return "File cut short";case Status::BadMagic:return "Not a backup file";case Status::BadVersion:return "Made by another version";case Status::BadCrc:return "File damaged";case Status::NeedPassphrase:return "Passphrase needed";case Status::WrongPassphrase:return "Wrong passphrase";case Status::BadRecord:return "File damaged";}return "";}
// The header fields only, no CRC and no size check: the restore list reads about 50 bytes of each file instead of all of it.
inline Status peek(const uint8_t *f,size_t n,Info &info){
 using namespace detail;if(n<4)return Status::Truncated;if(memcmp(f,"MMBK",4))return Status::BadMagic;if(n<6)return Status::Truncated;if(f[4]!=Version)return Status::BadVersion;
 if(n<Plain||((f[5]&Encrypted)&&n<Crypt))return Status::Truncated;
 info.version=f[4];info.encrypted=f[5]&Encrypted;info.passwords=f[5]&Passwords;info.identity=f[5]&Identity;info.nvsCount=uint16_t(get16(f+6));info.fileCount=uint16_t(get16(f+8));info.created=get32(f+12);return Status::Ok;
}
inline Status parse(const uint8_t *f,size_t n,const std::string &passphrase,Contents &out){
 using namespace detail;Info info;Status s=inspect(f,n,info);if(s!=Status::Ok)return s;
 const size_t head=info.encrypted?Crypt:Plain,bodyLen=get32(f+16);std::vector<uint8_t> plain;const uint8_t *body=f+head;
 if(info.encrypted){
  uint32_t iterations=get32(f+20);if(iterations<1||iterations>1000000||get16(f+10))return Status::BadRecord;if(passphrase.empty())return Status::NeedPassphrase;
  uint8_t key[32];mmcrypto::pbkdf2Sha256(reinterpret_cast<const uint8_t*>(passphrase.data()),passphrase.size(),f+24,16,iterations,key,32);
  plain.resize(bodyLen);if(!mmcrypto::gcmDecrypt(key,f+40,f,head,body,bodyLen,plain.data(),f+head+bodyLen))return Status::WrongPassphrase;body=plain.data();
 }
 Reader r(body,bodyLen);Contents c;c.created=info.created;c.flags=f[5]&~Encrypted;
 while(r.ok&&r.at<bodyLen){
  uint32_t kind=r.u8();
  if(kind==1){NvsRecord rec;uint32_t k=r.u8();auto *a=r.take(k);if(!a)return Status::BadRecord;rec.ns.assign(reinterpret_cast<const char*>(a),k);k=r.u8();a=r.take(k);if(!a)return Status::BadRecord;rec.key.assign(reinterpret_cast<const char*>(a),k);rec.type=uint8_t(r.u8());uint32_t len=r.u32();a=r.take(len);if(!a&&len)return Status::BadRecord;if(!r.ok)return Status::BadRecord;rec.data.assign(a,a+len);
   if(!name(rec.ns)||!restorableNvs(rec.ns)||!name(rec.key)||rec.type<NvsI8||rec.type>NvsBlob)return Status::BadRecord;
   if(rec.type<NvsStr&&rec.data.size()!=numericSize(rec.type))return Status::BadRecord;
   if(rec.type==NvsStr&&std::find(rec.data.begin(),rec.data.end(),0)!=rec.data.end())return Status::BadRecord;
   for(auto &o:c.nvs)if(o.ns==rec.ns&&o.key==rec.key)return Status::BadRecord;c.nvs.push_back(std::move(rec));}
  else if(kind==2){FileRecord rec;uint32_t k=r.u16();auto *a=r.take(k);if(!a)return Status::BadRecord;rec.path.assign(reinterpret_cast<const char*>(a),k);uint32_t len=r.u32();a=r.take(len);if(!a&&len)return Status::BadRecord;if(!r.ok)return Status::BadRecord;rec.data.assign(a,a+len);
   if(!allowedPath(rec.path))return Status::BadRecord;for(auto &o:c.files)if(o.path==rec.path)return Status::BadRecord;c.files.push_back(std::move(rec));}
  else return Status::BadRecord;
 }
 if(!r.ok||c.nvs.size()!=info.nvsCount||c.files.size()!=info.fileCount)return Status::BadRecord;
 out=std::move(c);return Status::Ok;
}
}}
