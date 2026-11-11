#include "services/cell_client.h"
#ifndef MM_EMULATOR
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#endif
namespace device {
#ifndef MM_EMULATOR
struct CellClient::Tls {
 mbedtls_ssl_context ssl;mbedtls_ssl_config conf;mbedtls_x509_crt ca;mbedtls_entropy_context entropy;mbedtls_ctr_drbg_context drbg;bool up=false,closed=false;
 Tls(){mbedtls_ssl_init(&ssl);mbedtls_ssl_config_init(&conf);mbedtls_x509_crt_init(&ca);mbedtls_entropy_init(&entropy);mbedtls_ctr_drbg_init(&drbg);}
 ~Tls(){mbedtls_ssl_free(&ssl);mbedtls_ssl_config_free(&conf);mbedtls_x509_crt_free(&ca);mbedtls_ctr_drbg_free(&drbg);mbedtls_entropy_free(&entropy);}
};
static int bioSend(void *ctx,const unsigned char *buf,size_t len){int n=static_cast<CellClient*>(ctx)->rawWrite(buf,len);return n>0?n:MBEDTLS_ERR_SSL_INTERNAL_ERROR;}
static int bioRecv(void *ctx,unsigned char *buf,size_t len){auto *c=static_cast<CellClient*>(ctx);int n=c->rawRead(buf,len);if(n>0)return n;return c->rawAlive()?MBEDTLS_ERR_SSL_WANT_READ:0;}// 0: the peer closed
// Client handshake against the CA bundle in `pem`; false with the socket left to the caller to close.
static bool handshake(CellClient::Tls &t,CellClient *owner,const char *host,const char *pem,unsigned seconds){
 if(mbedtls_ctr_drbg_seed(&t.drbg,mbedtls_entropy_func,&t.entropy,reinterpret_cast<const unsigned char*>("mm-cell"),7))return false;
 if(mbedtls_ssl_config_defaults(&t.conf,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT))return false;
 mbedtls_ssl_conf_rng(&t.conf,mbedtls_ctr_drbg_random,&t.drbg);
 if(pem){if(mbedtls_x509_crt_parse(&t.ca,reinterpret_cast<const unsigned char*>(pem),strlen(pem)+1)<0)return false;mbedtls_ssl_conf_ca_chain(&t.conf,&t.ca,nullptr);mbedtls_ssl_conf_authmode(&t.conf,MBEDTLS_SSL_VERIFY_REQUIRED);}
 else mbedtls_ssl_conf_authmode(&t.conf,MBEDTLS_SSL_VERIFY_NONE);
 if(mbedtls_ssl_setup(&t.ssl,&t.conf)||mbedtls_ssl_set_hostname(&t.ssl,host))return false;
 mbedtls_ssl_set_bio(&t.ssl,owner,bioSend,bioRecv,nullptr);
 uint32_t end=millis()+seconds*1000UL;int rc;
 while((rc=mbedtls_ssl_handshake(&t.ssl))!=0){if((rc!=MBEDTLS_ERR_SSL_WANT_READ&&rc!=MBEDTLS_ERR_SSL_WANT_WRITE)||int32_t(millis()-end)>=0)return false;vTaskDelay(pdMS_TO_TICKS(10));}
 t.up=true;return true;}
#else
struct CellClient::Tls {};
#endif
CellClient::CellClient(bool tls,const char *rootCA,unsigned handshakeSeconds):tls(tls),ca(rootCA),handshakeS(handshakeSeconds){}
CellClient::~CellClient(){stop();}
int CellClient::connect(const char *host,uint16_t port,int32_t timeoutMs){
 stop();
#ifdef MM_EMULATOR
 if(tls)return 0;// the emulator has no TLS over the modem socket
#endif
 link=cellOpen(host,port,uint32_t(timeoutMs>0?timeoutMs:15000));if(link<0)return 0;
#ifndef MM_EMULATOR
 if(tls){secure.reset(new Tls);if(!handshake(*secure,this,host,ca,handshakeS)){stop();return 0;}}
#endif
 return 1;}
bool CellClient::rawAlive(){return link>=0&&cellLinkAlive(link);}
int CellClient::rawRead(uint8_t *out,size_t n){return link<0?0:cellRecv(link,out,n);}
int CellClient::rawWrite(const uint8_t *data,size_t n){return link<0?-1:cellSend(link,data,n);}
bool CellClient::fill(){
 if(plainAt<plain.size())return true;plain.clear();plainAt=0;if(link<0)return false;
 uint8_t tmp[1460];
#ifndef MM_EMULATOR
 if(tls&&secure){if(secure->closed)return false;int n=mbedtls_ssl_read(&secure->ssl,tmp,sizeof tmp);if(n>0){plain.assign(tmp,tmp+n);return true;}if(n!=MBEDTLS_ERR_SSL_WANT_READ&&n!=MBEDTLS_ERR_SSL_WANT_WRITE)secure->closed=true;return false;}
#endif
 int n=rawRead(tmp,sizeof tmp);if(n<=0)return false;plain.assign(tmp,tmp+n);return true;}
int CellClient::available(){fill();return int(plain.size()-plainAt);}
int CellClient::read(){if(!fill()){vTaskDelay(1);return -1;}return plain[plainAt++];}
int CellClient::read(uint8_t *buf,size_t size){if(!size)return 0;if(!fill()){vTaskDelay(1);return -1;}size_t n=std::min(size,plain.size()-plainAt);memcpy(buf,plain.data()+plainAt,n);plainAt+=n;return int(n);}
int CellClient::peek(){return fill()?plain[plainAt]:-1;}
size_t CellClient::write(const uint8_t *buf,size_t size){
 if(link<0||!size)return 0;
#ifndef MM_EMULATOR
 if(tls){if(!secure||!secure->up)return 0;size_t done=0;while(done<size){int n=mbedtls_ssl_write(&secure->ssl,buf+done,size-done);if(n>0)done+=size_t(n);else if(n==MBEDTLS_ERR_SSL_WANT_READ||n==MBEDTLS_ERR_SSL_WANT_WRITE)vTaskDelay(1);else return done;}return done;}
#endif
 int n=rawWrite(buf,size);return n>0?size_t(n):0;}
uint8_t CellClient::connected(){if(link<0)return 0;if(plainAt<plain.size())return 1;
#ifndef MM_EMULATOR
 if(tls&&secure&&secure->closed)return 0;
#endif
 return rawAlive()?1:0;}
void CellClient::stop(){
#ifndef MM_EMULATOR
 if(secure){if(secure->up&&!secure->closed&&rawAlive())mbedtls_ssl_close_notify(&secure->ssl);secure.reset();}
#endif
 if(link>=0){cellClose(link);link=-1;}plain.clear();plainAt=0;}
NetSecure::NetSecure(const char *rootCA,unsigned handshakeSeconds,bool tls):route(netPath()){
 if(route==NetPath::Cell)base.reset(new CellClient(tls,rootCA,handshakeSeconds));
 else if(tls){auto *secure=new WiFiClientSecure;if(rootCA)secure->setCACert(rootCA);else secure->setInsecure();secure->setHandshakeTimeout(handshakeSeconds);base.reset(secure);}
 else base.reset(new WiFiClient);}
}
