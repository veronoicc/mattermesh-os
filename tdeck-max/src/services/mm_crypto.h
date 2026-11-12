#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
// SHA-256, HMAC, PBKDF2, CRC-32 and AES-256-GCM in plain C++: the same code runs on the device, in the emulator and in the
// host tests (lock PIN hash, backup files). Checked against hashlib and the cryptography package by sim/test_crypto.cpp.
// ponytail: software AES and a bitwise GHASH, about 1 MB per second on the ESP32-S3; a table GHASH or the hardware AES when backups grow past a few MB.
namespace mmcrypto {
struct Sha256 {
 uint32_t h[8];uint8_t buf[64];uint64_t total;size_t fill;
 static uint32_t ror(uint32_t x,int n){return (x>>n)|(x<<(32-n));}
 Sha256(){reset();}
 void reset(){static const uint32_t iv[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};memcpy(h,iv,sizeof h);total=0;fill=0;}
 void block(const uint8_t *p){
  static const uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
   0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
   0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
   0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  uint32_t w[64];for(int i=0;i<16;i++)w[i]=uint32_t(p[4*i])<<24|uint32_t(p[4*i+1])<<16|uint32_t(p[4*i+2])<<8|p[4*i+3];
  for(int i=16;i<64;i++){uint32_t s0=ror(w[i-15],7)^ror(w[i-15],18)^(w[i-15]>>3),s1=ror(w[i-2],17)^ror(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
  uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],x=h[7];
  for(int i=0;i<64;i++){uint32_t t1=x+(ror(e,6)^ror(e,11)^ror(e,25))+((e&f)^(~e&g))+k[i]+w[i],t2=(ror(a,2)^ror(a,13)^ror(a,22))+((a&b)^(a&c)^(b&c));x=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
  h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=x;
 }
 void update(const uint8_t *p,size_t n){total+=n;while(n){size_t take=64-fill<n?64-fill:n;memcpy(buf+fill,p,take);fill+=take;p+=take;n-=take;if(fill==64){block(buf);fill=0;}}}
 void finish(uint8_t out[32]){uint64_t bits=total*8;uint8_t pad=0x80;update(&pad,1);pad=0;while(fill!=56)update(&pad,1);uint8_t len[8];for(int i=0;i<8;i++)len[i]=uint8_t(bits>>(56-8*i));update(len,8);for(int i=0;i<8;i++){out[4*i]=uint8_t(h[i]>>24);out[4*i+1]=uint8_t(h[i]>>16);out[4*i+2]=uint8_t(h[i]>>8);out[4*i+3]=uint8_t(h[i]);}}
};
inline void sha256(const uint8_t *p,size_t n,uint8_t out[32]){Sha256 s;s.update(p,n);s.finish(out);}
// HMAC with the two padded-key states kept, so PBKDF2 does not hash the key again on every iteration.
struct Hmac {
 Sha256 inner,outer;
 Hmac(const uint8_t *key,size_t keyLen){uint8_t k[64]={},ipad[64],opad[64];if(keyLen>64)sha256(key,keyLen,k);else memcpy(k,key,keyLen);for(int i=0;i<64;i++){ipad[i]=k[i]^0x36;opad[i]=k[i]^0x5c;}inner.update(ipad,64);outer.update(opad,64);}
 void mac(const uint8_t *msg,size_t n,uint8_t out[32])const{Sha256 i=inner,o=outer;uint8_t d[32];i.update(msg,n);i.finish(d);o.update(d,32);o.finish(out);}
};
inline void hmacSha256(const uint8_t *key,size_t keyLen,const uint8_t *msg,size_t n,uint8_t out[32]){Hmac(key,keyLen).mac(msg,n,out);}
inline void pbkdf2Sha256(const uint8_t *pw,size_t pwLen,const uint8_t *salt,size_t saltLen,uint32_t iterations,uint8_t *out,size_t outLen){
 Hmac prf(pw,pwLen);
 for(uint32_t block=1;outLen;block++){
  uint8_t u[32],t[32];Sha256 first=prf.inner;uint8_t idx[4]={uint8_t(block>>24),uint8_t(block>>16),uint8_t(block>>8),uint8_t(block)};
  first.update(salt,saltLen);first.update(idx,4);first.finish(u);{Sha256 o=prf.outer;o.update(u,32);o.finish(u);}memcpy(t,u,32);
  for(uint32_t i=1;i<iterations;i++){prf.mac(u,32,u);for(int j=0;j<32;j++)t[j]^=u[j];}
  size_t take=outLen<32?outLen:32;memcpy(out,t,take);out+=take;outLen-=take;
 }
}
inline uint32_t crc32(const uint8_t *p,size_t n,uint32_t crc=0){crc=~crc;while(n--){crc^=*p++;for(int i=0;i<8;i++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));}return ~crc;}
// Constant time: a tag or hash check never reveals how many leading bytes matched.
inline bool equal(const uint8_t *a,const uint8_t *b,size_t n){uint8_t d=0;for(size_t i=0;i<n;i++)d|=a[i]^b[i];return d==0;}
// AES-256, encrypt direction only (GCM needs nothing else); the S-box is computed once from the field inverse.
struct Aes256 {
 uint8_t sbox[256],rk[240];
 static uint8_t xtime(uint8_t x){return uint8_t((x<<1)^((x>>7)*0x1b));}
 Aes256(const uint8_t key[32]){
  uint8_t p=1,q=1;do{p=uint8_t(p^(p<<1)^((p&0x80)?0x1b:0));q^=q<<1;q^=q<<2;q^=q<<4;if(q&0x80)q^=0x09;uint8_t r=uint8_t(q^(q<<1|q>>7)^(q<<2|q>>6)^(q<<3|q>>5)^(q<<4|q>>4));sbox[p]=uint8_t(r^0x63);}while(p!=1);sbox[0]=0x63;
  memcpy(rk,key,32);uint8_t rcon=1;
  for(int i=32;i<240;i+=4){uint8_t t[4];memcpy(t,rk+i-4,4);if(i%32==0){uint8_t s=t[0];t[0]=uint8_t(sbox[t[1]]^rcon);t[1]=sbox[t[2]];t[2]=sbox[t[3]];t[3]=sbox[s];rcon=xtime(rcon);}else if(i%32==16){for(int j=0;j<4;j++)t[j]=sbox[t[j]];}for(int j=0;j<4;j++)rk[i+j]=uint8_t(rk[i-32+j]^t[j]);}
 }
 void encrypt(const uint8_t in[16],uint8_t out[16])const{
  uint8_t s[16];for(int i=0;i<16;i++)s[i]=in[i]^rk[i];
  for(int round=1;round<=14;round++){
   uint8_t t[16];for(int c=0;c<4;c++)for(int r=0;r<4;r++)t[4*c+r]=sbox[s[4*((c+r)&3)+r]];// SubBytes and ShiftRows
   if(round<14)for(int c=0;c<4;c++){uint8_t *a=t+4*c,x=a[0]^a[1]^a[2]^a[3],o=a[0];a[0]^=x^xtime(a[0]^a[1]);a[1]^=x^xtime(a[1]^a[2]);a[2]^=x^xtime(a[2]^a[3]);a[3]^=x^xtime(a[3]^o);}// MixColumns
   for(int i=0;i<16;i++)s[i]=t[i]^rk[16*round+i];
  }
  memcpy(out,s,16);
 }
};
// GHASH: multiplication in GF(2^128), bit by bit (the reduction polynomial is 0xe1 in the top byte).
struct Ghash {
 uint8_t h[16],y[16];
 Ghash(const uint8_t key[16]){memcpy(h,key,16);memset(y,0,16);}
 void mul(){uint8_t z[16]={},v[16];memcpy(v,h,16);for(int i=0;i<128;i++){if(y[i>>3]&(0x80>>(i&7)))for(int j=0;j<16;j++)z[j]^=v[j];bool lsb=v[15]&1;for(int j=15;j>0;j--)v[j]=uint8_t(v[j]>>1|v[j-1]<<7);v[0]>>=1;if(lsb)v[0]^=0xe1;}memcpy(y,z,16);}
 void block(const uint8_t *p,size_t n){for(size_t i=0;i<16;i++)y[i]^=i<n?p[i]:0;mul();}
 void data(const uint8_t *p,size_t n){for(;n>=16;p+=16,n-=16)block(p,16);if(n)block(p,n);}
};
namespace detail {
inline void gcmTag(const Aes256 &aes,const uint8_t iv[12],const uint8_t *aad,size_t aadLen,const uint8_t *ct,size_t n,uint8_t tag[16]){
 uint8_t zero[16]={},hk[16],j0[16],ek[16];aes.encrypt(zero,hk);memcpy(j0,iv,12);j0[12]=j0[13]=j0[14]=0;j0[15]=1;
 Ghash g(hk);g.data(aad,aadLen);g.data(ct,n);uint8_t lens[16];uint64_t a=uint64_t(aadLen)*8,c=uint64_t(n)*8;for(int i=0;i<8;i++){lens[i]=uint8_t(a>>(56-8*i));lens[8+i]=uint8_t(c>>(56-8*i));}g.block(lens,16);
 aes.encrypt(j0,ek);for(int i=0;i<16;i++)tag[i]=g.y[i]^ek[i];
}
inline void gcmCtr(const Aes256 &aes,const uint8_t iv[12],const uint8_t *in,size_t n,uint8_t *out){
 uint8_t ctr[16],ks[16];memcpy(ctr,iv,12);uint32_t counter=2;
 for(size_t at=0;at<n;at+=16,counter++){ctr[12]=uint8_t(counter>>24);ctr[13]=uint8_t(counter>>16);ctr[14]=uint8_t(counter>>8);ctr[15]=uint8_t(counter);aes.encrypt(ctr,ks);size_t take=n-at<16?n-at:16;for(size_t i=0;i<take;i++)out[at+i]=in[at+i]^ks[i];}
}
}
// out may be in. The 12-byte iv must never repeat under one key.
inline void gcmEncrypt(const uint8_t key[32],const uint8_t iv[12],const uint8_t *aad,size_t aadLen,const uint8_t *in,size_t n,uint8_t *out,uint8_t tag[16]){
 Aes256 aes(key);detail::gcmCtr(aes,iv,in,n,out);detail::gcmTag(aes,iv,aad,aadLen,out,n,tag);
}
// The tag is checked before anything is written to out.
inline bool gcmDecrypt(const uint8_t key[32],const uint8_t iv[12],const uint8_t *aad,size_t aadLen,const uint8_t *in,size_t n,uint8_t *out,const uint8_t tag[16]){
 Aes256 aes(key);uint8_t want[16];detail::gcmTag(aes,iv,aad,aadLen,in,n,want);if(!equal(want,tag,16))return false;detail::gcmCtr(aes,iv,in,n,out);return true;
}
}
