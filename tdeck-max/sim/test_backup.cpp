#include <cassert>
#include <iostream>
#include "services/backup_format.h"
using namespace device::backup;
using Bytes=std::vector<uint8_t>;
static Bytes bytes(const std::string &s){return Bytes(s.begin(),s.end());}
static Bytes le32(uint32_t v){return {uint8_t(v),uint8_t(v>>8),uint8_t(v>>16),uint8_t(v>>24)};}
static Contents sample(){
 Contents c;c.created=1789000000;c.flags=Passwords;
 c.nvs.push_back({"mmui","homewg",NvsI32,le32(5)});
 c.nvs.push_back({"mmconnect","networks",NvsStr,bytes("[{\"s\":\"home\",\"p\":\"hunter2-secret\"}]")});
 c.nvs.push_back({"mmapp","prefs",NvsBlob,Bytes{0,1,2,3,250,251,252,253,254,255}});
 c.nvs.push_back({"mmui","note",NvsStr,Bytes{}});
 c.nvs.push_back({"mmmotion","on",NvsU8,Bytes{1}});
 c.nvs.push_back({"mmlocation","latitude",NvsBlob,Bytes(8,0x40)});
 c.files.push_back({"/mattermesh/contacts.tsv",bytes("aa\tAlice\t1\nbb\tBob\t2\n")});
 c.files.push_back({"/maxchat/catalog.tsv",bytes("MAXCHAT1\nC\t*\tPublic\n")});
 c.files.push_back({"/mattermesh/radio.conf",Bytes{}});
 return c;
}
static void same(const Contents &a,const Contents &b){assert(a.created==b.created&&a.flags==b.flags&&a.nvs.size()==b.nvs.size()&&a.files.size()==b.files.size());
 for(size_t i=0;i<a.nvs.size();i++)assert(a.nvs[i].ns==b.nvs[i].ns&&a.nvs[i].key==b.nvs[i].key&&a.nvs[i].type==b.nvs[i].type&&a.nvs[i].data==b.nvs[i].data);
 for(size_t i=0;i<a.files.size();i++)assert(a.files[i].path==b.files[i].path&&a.files[i].data==b.files[i].data);}
static void fixCrc(Bytes &f){uint32_t c=mmcrypto::crc32(f.data(),f.size()-4);for(int i=0;i<4;i++)f[f.size()-4+i]=uint8_t(c>>(8*i));}
static const uint8_t salt[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},iv[12]={9,8,7,6,5,4,3,2,1,0,1,2};
static bool contains(const Bytes &hay,const std::string &needle){return std::search(hay.begin(),hay.end(),needle.begin(),needle.end())!=hay.end();}
int main(){
 // Plain round trip; header summary readable without a passphrase.
 Contents c=sample();Bytes plain=build(c,"",salt,iv,100);Contents back;
 assert(parse(plain.data(),plain.size(),"",back)==Status::Ok);same(c,back);
 {Info i;assert(inspect(plain.data(),plain.size(),i)==Status::Ok&&i.version==Version&&!i.encrypted&&i.passwords&&!i.identity&&i.created==c.created&&i.nvsCount==6&&i.fileCount==3&&i.size==plain.size());}
 assert(contains(plain,"hunter2-secret"));
 // Encrypted round trip: the secret is not in the file, the summary is, the right passphrase opens it.
 Bytes sealed=build(c,"correct horse",salt,iv,100);Contents opened;
 assert(!contains(sealed,"hunter2-secret")&&!contains(sealed,"Alice")&&sealed.size()>plain.size());
 {Info i;assert(inspect(sealed.data(),sealed.size(),i)==Status::Ok&&i.encrypted&&i.nvsCount==6&&i.fileCount==3);}
 // peek reads the header only: a file cut after the header still lists, a cut inside it does not, a corrupt body is not noticed.
 {Info i;assert(peek(plain.data(),20,i)==Status::Ok&&i.nvsCount==6&&i.fileCount==3&&!i.encrypted&&i.passwords&&i.created==c.created&&i.size==0);assert(peek(plain.data(),19,i)==Status::Truncated&&peek(sealed.data(),51,i)==Status::Truncated&&peek(sealed.data(),52,i)==Status::Ok&&i.encrypted);
  Bytes f=plain;f[0]='X';assert(peek(f.data(),20,i)==Status::BadMagic);f=plain;f[4]=9;assert(peek(f.data(),20,i)==Status::BadVersion);f=plain;f[40]^=1;assert(peek(f.data(),f.size(),i)==Status::Ok);}
 assert(std::string(statusText(Status::Ok))=="OK"&&std::string(statusText(Status::BadCrc))=="File damaged"&&std::string(statusText(Status::WrongPassphrase))=="Wrong passphrase"&&std::string(statusText(Status::BadVersion))=="Made by another version"&&std::string(statusText(Status::NeedPassphrase))=="Passphrase needed");
 assert(parse(sealed.data(),sealed.size(),"correct horse",opened)==Status::Ok);same(c,opened);
 {Contents x;assert(parse(sealed.data(),sealed.size(),"",x)==Status::NeedPassphrase&&x.nvs.empty()&&x.files.empty());assert(parse(sealed.data(),sealed.size(),"correct horsE",x)==Status::WrongPassphrase&&x.nvs.empty());assert(parse(sealed.data(),sealed.size(),"wrong",x)==Status::WrongPassphrase);}
 // An unencrypted file ignores a passphrase that is not needed.
 {Contents x;assert(parse(plain.data(),plain.size(),"anything",x)==Status::Ok);}
 // Truncated at every length: never accepted, never read past the end.
 for(size_t n=0;n<plain.size();n++){Contents x;Status s=parse(plain.data(),n,"",x);assert(s!=Status::Ok&&x.nvs.empty()&&x.files.empty());}
 for(size_t n=0;n<sealed.size();n+=7){Contents x;assert(parse(sealed.data(),n,"correct horse",x)!=Status::Ok);}
 {Contents x;assert(parse(plain.data(),plain.size()-1,"",x)==Status::Truncated);Info i;assert(inspect(plain.data(),10,i)==Status::Truncated);}
 // Trailing bytes after the CRC are refused.
 {Bytes more=plain;more.push_back(0);Contents x;assert(parse(more.data(),more.size(),"",x)==Status::BadRecord);}
 // Bad CRC: any flipped bit in the file.
 for(size_t at:{size_t(0)+6,size_t(21),plain.size()/2,plain.size()-5,plain.size()-1}){Bytes f=plain;f[at]^=0x10;Contents x;Status s=parse(f.data(),f.size(),"",x);assert(s!=Status::Ok&&x.nvs.empty());}
 {Bytes f=plain;f[plain.size()/2]^=1;Contents x;assert(parse(f.data(),f.size(),"",x)==Status::BadCrc);}
 {Bytes f=plain;f[plain.size()-1]^=1;Contents x;assert(parse(f.data(),f.size(),"",x)==Status::BadCrc);}
 // Magic and version.
 {Bytes f=plain;f[0]='X';fixCrc(f);Contents x;assert(parse(f.data(),f.size(),"",x)==Status::BadMagic);}
 {Bytes f=plain;f[4]=Version+1;fixCrc(f);Contents x;assert(parse(f.data(),f.size(),"",x)==Status::BadVersion);Info i;assert(inspect(f.data(),f.size(),i)==Status::BadVersion);}
 {Bytes f=plain;f[4]=0;Contents x;assert(parse(f.data(),f.size(),"",x)==Status::BadVersion);}
 // Encrypted: a flipped ciphertext bit with a repaired CRC, a changed header (AAD) or a changed tag fail the authentication.
 {Bytes f=sealed;f[60]^=1;fixCrc(f);Contents x;assert(parse(f.data(),f.size(),"correct horse",x)==Status::WrongPassphrase&&x.nvs.empty());}
 {Bytes f=sealed;f[12]^=1;fixCrc(f);Contents x;assert(parse(f.data(),f.size(),"correct horse",x)==Status::WrongPassphrase);}
 {Bytes f=sealed;f[f.size()-5]^=1;fixCrc(f);Contents x;assert(parse(f.data(),f.size(),"correct horse",x)==Status::WrongPassphrase);}
 {Bytes f=sealed;f[20]=0;f[21]=0;f[22]=0;f[23]=0;fixCrc(f);Contents x;assert(parse(f.data(),f.size(),"correct horse",x)==Status::BadRecord);}// zero iterations
 // Every record is validated: path outside the list, the PIN namespace, bad names, types and lengths, duplicates, counts.
 auto bad=[&](Contents x){Bytes f=build(x,"",salt,iv,100);Contents r;Status s=parse(f.data(),f.size(),"",r);assert(s==Status::BadRecord&&r.nvs.empty()&&r.files.empty());};
 {Contents x=sample();x.files[0].path="/etc/passwd";bad(x);}
 {Contents x=sample();x.files[0].path="/mattermesh/../contacts.tsv";bad(x);}
 {Contents x=sample();x.files[0].path="/mattermesh/messages.tsv";bad(x);}
 {Contents x=sample();x.files.push_back(x.files[0]);bad(x);}
 {Contents x=sample();x.nvs[0].ns="mmsec";bad(x);}
 {Contents x=sample();x.nvs[0].ns="other";bad(x);}
 {Contents x=sample();x.nvs[0].ns="";bad(x);}
 {Contents x=sample();x.nvs[0].key="a-key-longer-than-15";bad(x);}
 {Contents x=sample();x.nvs[0].ns=std::string("mm\0x",4);bad(x);}
 {Contents x=sample();x.nvs[0].type=0;bad(x);}
 {Contents x=sample();x.nvs[0].type=11;bad(x);}
 {Contents x=sample();x.nvs[0].data=Bytes{1,2};bad(x);}
 {Contents x=sample();x.nvs[4].data=Bytes{1,2};bad(x);}
 {Contents x=sample();x.nvs.push_back(x.nvs[0]);bad(x);}
 {Contents x=sample();x.nvs[1].data=bytes(std::string("a\0b",3));bad(x);}
 {Bytes f=plain;f[6]++;fixCrc(f);Contents x;assert(parse(f.data(),f.size(),"",x)==Status::BadRecord);}
 {Bytes f=plain;f[8]--;fixCrc(f);Contents x;assert(parse(f.data(),f.size(),"",x)==Status::BadRecord);}
 {Bytes f=plain;f[16]--;f.erase(f.end()-5);fixCrc(f);Contents x;assert(parse(f.data(),f.size(),"",x)==Status::BadRecord);}// body cut inside the last record
 {Bytes f=plain;f[20]=0x7f;fixCrc(f);Contents x;assert(parse(f.data(),f.size(),"",x)==Status::BadRecord);}// unknown record kind
 // A restore is all or nothing: parse() hands over nothing at the first bad record, even one at the very end.
 {Contents x=sample();x.files.back().path="/nope";Bytes f=build(x,"",salt,iv,100);Contents r;assert(parse(f.data(),f.size(),"",r)==Status::BadRecord&&r.nvs.empty()&&r.files.empty());}
 // Oversized: a body longer than the limit is refused before any allocation of that size.
 {Bytes f=plain;uint32_t huge=MaxBody+1;for(int i=0;i<4;i++)f[16+i]=uint8_t(huge>>(8*i));fixCrc(f);Contents x;assert(parse(f.data(),f.size(),"",x)==Status::BadRecord||parse(f.data(),f.size(),"",x)==Status::Truncated);}
 // File names: mm-YYYYMMDD-HHMMSS.mmb; only those are listed and restored.
 assert(fileName(2026,11,12,9,5,3)=="mm-20261112-090503.mmb");
 assert(isBackupName("mm-20261112-090503.mmb")&&!isBackupName("mm-20261112-090503.mmb.part")&&!isBackupName("mm-2026111-090503.mmb")&&!isBackupName("../mm-20261112-090503.mmb")&&!isBackupName("mm-20261112-0905a3.mmb")&&!isBackupName("x.mmb")&&!isBackupName(""));
 assert(nameDate("mm-20261112-090503.mmb")=="2026-11-12 09:05");
 assert(fileName(0,0,0,0,0,0)=="mm-00000000-000000.mmb");
 // Options: what goes into the file.
 {Options o;assert(o.passwords&&!o.privateKey&&!o.encrypt);}
 assert(secretKey("mmconnect","networks")&&secretKey("mmcell","pin")&&secretKey("mmcell","apnp")&&secretKey("mmcell","mpass")&&secretKey("mmble","pin")&&secretKey("mmrlink","pin")&&secretKey("mmweb","code")&&secretKey("mmweb","tok")&&!secretKey("mmcell","apn")&&!secretKey("mmui","homewg"));
 {std::vector<NvsRecord> all=sample().nvs;all.push_back({"mmsec","hash",NvsBlob,Bytes(32,1)});all.push_back({"mmcell","pin",NvsStr,bytes("1234")});all.push_back({"mmcell","apn",NvsStr,bytes("internet")});
  auto with=selectNvs(all,true),without=selectNvs(all,false);
  auto has=[](const std::vector<NvsRecord> &v,const char *ns,const char *key){for(auto &r:v)if(r.ns==ns&&r.key==key)return true;return false;};
  assert(!has(with,"mmsec","hash")&&!has(without,"mmsec","hash"));assert(has(with,"mmconnect","networks")&&has(with,"mmcell","pin")&&has(with,"mmcell","apn"));assert(!has(without,"mmconnect","networks")&&!has(without,"mmcell","pin")&&has(without,"mmcell","apn")&&has(without,"mmui","homewg")&&without.size()==with.size()-2);}
 // The private key option exists only in builds that export keys.
 assert(privateKeyAvailable()==bool(ENABLE_PRIVATE_KEY_EXPORT));
 assert(allowedPath("/mattermesh/identity.id")&&allowedPath("/mattermesh/contacts.tsv")&&allowedPath("/mattermesh/bindings.tsv")&&allowedPath("/mattermesh/dmroutes.tsv")&&allowedPath("/mattermesh/radio.conf")&&allowedPath("/maxchat/catalog.tsv")&&!allowedPath("/mattermesh/messages.tsv")&&!allowedPath(""));
 std::cout<<"PASS: backup format round trip plain and AES-GCM, truncated / bad CRC / wrong passphrase / version / bad record rejected without partial output, names, options\n";
}
