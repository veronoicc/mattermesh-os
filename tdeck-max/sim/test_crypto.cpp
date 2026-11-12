#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "services/mm_crypto.h"
using namespace mmcrypto;
static std::string hex(const uint8_t *p,size_t n){static const char *d="0123456789abcdef";std::string s;for(size_t i=0;i<n;i++){s+=d[p[i]>>4];s+=d[p[i]&15];}return s;}
static const uint8_t *u(const std::string &s){return reinterpret_cast<const uint8_t*>(s.data());}
int main(){
 uint8_t out[64];
 // SHA-256, HMAC-SHA256 (RFC 4231 case 2 and a key longer than the block), PBKDF2-HMAC-SHA256 against hashlib.
 sha256(u("abc"),3,out);assert(hex(out,32)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
 {std::string a(1000,'a');sha256(u(a),a.size(),out);assert(hex(out,32)=="41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");}
 hmacSha256(u("Jefe"),4,u("what do ya want for nothing?"),28,out);assert(hex(out,32)=="5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
 {std::string k(100,'k');hmacSha256(u(k),k.size(),u("msg"),3,out);assert(hex(out,32)=="bd56a1782c2830e8abc6ed866a57a1230661e650b84c62f7ee3accc5fa5af491");}
 pbkdf2Sha256(u("password"),8,u("salt"),4,1,out,32);assert(hex(out,32)=="120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
 pbkdf2Sha256(u("password"),8,u("salt"),4,2,out,32);assert(hex(out,32)=="ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
 {std::string pw="passwordPASSWORDpassword",salt="saltSALTsaltSALTsaltSALTsaltSALTsalt";pbkdf2Sha256(u(pw),pw.size(),u(salt),salt.size(),4096,out,40);assert(hex(out,40)=="348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9");}
 assert(crc32(u("123456789"),9)==0xcbf43926u);
 {std::string a="1234",b="56789";assert(crc32(u(b),5,crc32(u(a),4))==0xcbf43926u);}// chunked
 {uint8_t a[3]={1,2,3},b[3]={1,2,3},c[3]={1,2,4};assert(equal(a,b,3)&&!equal(a,c,3));}
 // AES-256-GCM against cryptography's AESGCM (key 0..31, iv 0..11, AAD "header-bytes", plaintext (i*7+3)&255).
 uint8_t key[32],iv[12];for(int i=0;i<32;i++)key[i]=uint8_t(i);for(int i=0;i<12;i++)iv[i]=uint8_t(i);
 struct V{size_t n;const char *ct,*tag;};
 const V vs[]={{0,"","04f1a578cf35032651d6f0f1d5804fc2"},{16,"4408c703dac3ef2fb603dedbe6b71d01","8df7f0046dda0dc60faee771dc26bc1c"},
  {100,"4408c703dac3ef2fb603dedbe6b71d01f0ac06bc7fedc2d893d55c45daa7d56ee2fa5f0450c71f8c6f8656ddbfb96d74bd0301e535a0de5eb405b3b9bf4d405233f617a20c37cb95e756a37ef8912bc40bb0882a39350e0c968fe74cff7e2c116852e85e","21ec6514a57707fc7096b96ddaf5124f"}};
 for(auto &v:vs){std::vector<uint8_t> pt(v.n),ct(v.n);for(size_t i=0;i<v.n;i++)pt[i]=uint8_t((i*7+3)&255);uint8_t tag[16];
  gcmEncrypt(key,iv,u("header-bytes"),12,pt.data(),v.n,ct.data(),tag);assert(hex(ct.data(),v.n)==v.ct&&hex(tag,16)==v.tag);
  std::vector<uint8_t> back(v.n);assert(gcmDecrypt(key,iv,u("header-bytes"),12,ct.data(),v.n,back.data(),tag)&&back==pt);
  // In place both ways.
  std::vector<uint8_t> buf=pt;uint8_t t2[16];gcmEncrypt(key,iv,u("header-bytes"),12,buf.data(),v.n,buf.data(),t2);assert(hex(buf.data(),v.n)==v.ct);assert(gcmDecrypt(key,iv,u("header-bytes"),12,buf.data(),v.n,buf.data(),t2)&&buf==pt);
  // Any change is refused and leaves the output untouched: tag, ciphertext, AAD, key, iv.
  if(v.n){std::vector<uint8_t> bad=ct;bad[v.n/2]^=1;std::vector<uint8_t> o(v.n,0xee);assert(!gcmDecrypt(key,iv,u("header-bytes"),12,bad.data(),v.n,o.data(),tag));for(auto b:o)assert(b==0xee);}
  uint8_t badTag[16];memcpy(badTag,tag,16);badTag[15]^=0x80;std::vector<uint8_t> o(v.n);assert(!gcmDecrypt(key,iv,u("header-bytes"),12,ct.data(),v.n,o.data(),badTag));
  assert(!gcmDecrypt(key,iv,u("header-byteS"),12,ct.data(),v.n,o.data(),tag));uint8_t k2[32];memcpy(k2,key,32);k2[0]^=1;assert(!gcmDecrypt(k2,iv,u("header-bytes"),12,ct.data(),v.n,o.data(),tag));uint8_t iv2[12];memcpy(iv2,iv,12);iv2[11]^=1;assert(!gcmDecrypt(key,iv2,u("header-bytes"),12,ct.data(),v.n,o.data(),tag));}
 std::cout<<"PASS: SHA-256, HMAC, PBKDF2, CRC-32 and AES-256-GCM match the reference vectors; tampering with tag, data, header, key or iv is refused\n";
}
