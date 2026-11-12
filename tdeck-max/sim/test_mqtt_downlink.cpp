#include "services/mqtt_downlink.h"
#include <cassert>
#include <iostream>
#include <vector>
using namespace mqtt_downlink;
static const std::string T="Tok3nTok3nTok3nTok3n1234";
// A fake transport/host: records what the commands asked for, like the firmware's send path would.
struct Fake:Host {
 std::vector<std::string> log;bool fail=false;
 bool sendChannel(const std::string &channel,const std::string &text,std::string &result,std::string &err)override{log.push_back("ch:"+channel+":"+text);if(fail){err="Channel key required";return false;}result="{\"token\":\"0000002a\"}";return true;}
 bool sendDm(const std::string &to,const std::string &text,std::string &result,std::string &err)override{log.push_back("dm:"+to+":"+text);if(fail){err="No such contact";return false;}result="{\"token\":\"0000002b\"}";return true;}
 bool advert(bool position,std::string &err)override{log.push_back(position?"advert+pos":"advert");if(fail){err="Radio offline";return false;}return true;}
 std::string status()override{log.push_back("status");return "{\"name\":\"n\"}";}
 std::string listChannels()override{log.push_back("channels");return "{\"items\":[]}";}
 std::string listContacts()override{log.push_back("contacts");return "{\"items\":[]}";}
};
static std::string run(const std::string &payload,Fake &f,RateLimiter &rl,uint32_t now=0,bool allowSend=true,const std::string &secret=T){static uint32_t clock=1000;if(!now)now=clock+=1000;return handle(payload,secret,allowSend,rl,now,f);}
static std::string run(const std::string &payload,Fake &f){RateLimiter rl;return run(payload,f,rl);}
static std::string cmd(const std::string &body){return "{\"id\":\"7\",\"token\":\""+T+"\","+body+"}";}
static bool has(const std::string &s,const std::string &part){return s.find(part)!=std::string::npos;}
int main(){
 // Token comparison: equal only for the exact bytes, any length difference fails.
 assert(constantTimeEquals("abc","abc")&&!constantTimeEquals("abc","abd")&&!constantTimeEquals("abc","ab")&&!constantTimeEquals("ab","abc")&&constantTimeEquals("",""));
 // Token generator: 24 chars of the unambiguous alphabet, different seeds give different tokens, rejects nothing silently.
 {uint32_t seed=1;auto rng=[&]()->uint32_t{seed=seed*1664525u+1013904223u;return seed;};
  std::string a=generateToken(rng),b=generateToken(rng);assert(a.size()==TokenLen&&b.size()==TokenLen&&a!=b);for(char c:a+b)assert(std::string("23456789ABCDEFGHJKLMNPQRSTUVWXYZ").find(c)!=std::string::npos);assert(validToken(a));}
 assert(validToken("12345678")&&!validToken("1234567")&&!validToken(std::string(TokenMax+1,'a'))&&!validToken("with space1")&&!validToken("quote\"quote")&&validToken(std::string(TokenMax,'a')));
 // Rate limiter: N per sliding window, old entries expire, a refused attempt is not counted, clock wrap is fine.
 {RateLimiter rl(3,1000);assert(rl.allow(0)&&rl.allow(100)&&rl.allow(200)&&!rl.allow(300)&&!rl.allow(999));assert(rl.allow(1000));assert(!rl.allow(1050));assert(rl.allow(1100));assert(rl.allow(1200));assert(!rl.allow(1201));
  RateLimiter w(2,1000);assert(w.allow(0xfffffff0u)&&w.allow(0xfffffff8u)&&!w.allow(5)&&w.allow(0xfffffff0u+1000));
  RateLimiter d;int ok=0;for(int i=0;i<20;i++)ok+=d.allow(i);assert(ok==10);assert(d.allow(60000)&&d.allow(60001));}
 // JSON reader: strings with escapes (\u incl. surrogate pairs), numbers, bools, null, nested values skipped.
 {std::map<std::string,Value> m;assert(parseObject("  {\"a\":\"x\\ny\\\"\\u00e9\\ud83d\\ude00\",\"b\":-12.5e3,\"c\":true,\"d\":null,\"e\":{\"x\":[1,{\"y\":\"}\"}]},\"f\":[1,2]} ",m));
  assert(m["a"].type=='s'&&m["a"].s=="x\ny\"\xc3\xa9\xf0\x9f\x98\x80"&&m["b"].type=='n'&&m["b"].s=="-12.5e3"&&m["c"].type=='b'&&m["c"].b&&m["d"].type=='z'&&m["e"].type=='o'&&m["f"].type=='o');
  std::map<std::string,Value> bad;for(const char *s:{"","{","{\"a\":}","{\"a\" 1}","{\"a\":\"x\"","[1]","{\"a\":\"x\"} trailing","{\"a\":\"\\u12\"}","{\"a\":tru}","{\"a\":\"\\q\"}","{'a':1}","{\"a\":1,}","{\"a\":\"\x01\"}","{\"a\":[1,2}"}){bad.clear();assert(!parseObject(s,bad));}
  std::string deep="{\"a\":";for(int i=0;i<40;i++)deep+="[";for(int i=0;i<40;i++)deep+="]";deep+="}";assert(!parseObject(deep,bad));}
 // Happy paths reach the host once and answer with the caller's id.
 {Fake f;RateLimiter rl(16,10);
  auto r=run(cmd("\"cmd\":\"send_channel\",\"channel\":\"Public\",\"text\":\"hello \\\"mesh\\\"\""),f,rl);assert(r=="{\"id\":\"7\",\"ok\":true,\"result\":{\"token\":\"0000002a\"}}");assert(f.log.back()=="ch:Public:hello \"mesh\"");
  r=run(cmd("\"cmd\":\"send_channel\",\"channel\":2,\"text\":\"by index\""),f,rl);assert(has(r,"\"ok\":true")&&f.log.back()=="ch:2:by index");
  r=run(cmd("\"cmd\":\"send_dm\",\"to\":\"ab12cd34\",\"text\":\"psst\""),f,rl);assert(has(r,"\"ok\":true")&&f.log.back()=="dm:ab12cd34:psst");
  r=run(cmd("\"cmd\":\"advert\""),f,rl);assert(has(r,"\"ok\":true")&&!has(r,"result")&&f.log.back()=="advert");
  r=run(cmd("\"cmd\":\"advert\",\"position\":true"),f,rl);assert(f.log.back()=="advert+pos");
  r=run(cmd("\"cmd\":\"ping\""),f,rl);assert(r=="{\"id\":\"7\",\"ok\":true,\"result\":{\"name\":\"n\"}}");
  r=run(cmd("\"cmd\":\"list_channels\""),f,rl);assert(has(r,"\"ok\":true")&&f.log.back()=="channels");
  r=run(cmd("\"cmd\":\"list_contacts\""),f,rl);assert(has(r,"\"ok\":true")&&f.log.back()=="contacts");
  // A numeric id is echoed as a number.
  r=run("{\"id\":42,\"token\":\""+T+"\",\"cmd\":\"ping\"}",f,rl);assert(has(r,"{\"id\":42,\"ok\":true"));}
 // Malformed JSON, missing / wrong token, missing id: the host is never reached.
 {Fake f;RateLimiter rl(16,10);
  assert(run("{not json",f,rl)=="{\"id\":null,\"ok\":false,\"error\":\"malformed JSON\"}");assert(run("",f,rl)==run("[]",f,rl));
  auto r=run("{\"id\":\"1\",\"cmd\":\"ping\"}",f,rl);assert(r=="{\"id\":\"1\",\"ok\":false,\"error\":\"missing token\"}");
  r=run("{\"id\":\"1\",\"token\":5,\"cmd\":\"ping\"}",f,rl);assert(has(r,"missing token"));
  r=run("{\"id\":\"1\",\"token\":\"nope\",\"cmd\":\"ping\"}",f,rl);assert(r=="{\"id\":\"1\",\"ok\":false,\"error\":\"unauthorized\"}");
  r=run("{\"id\":\"1\",\"token\":\""+T+"x\",\"cmd\":\"ping\"}",f,rl);assert(has(r,"unauthorized"));
  r=run("{\"token\":\""+T+"\",\"cmd\":\"ping\"}",f,rl);assert(r=="{\"id\":null,\"ok\":false,\"error\":\"missing id\"}");
  r=run("{\"id\":\"\",\"token\":\""+T+"\",\"cmd\":\"ping\"}",f,rl);assert(has(r,"missing id"));
  r=run("{\"id\":\""+std::string(65,'x')+"\",\"token\":\""+T+"\",\"cmd\":\"ping\"}",f,rl);assert(has(r,"missing id"));
  assert(f.log.empty());
  // No configured token: nothing is ever authorised, not even an empty token.
  r=run("{\"id\":\"1\",\"token\":\"\",\"cmd\":\"ping\"}",f,rl,0,true,"");assert(has(r,"unauthorized")&&f.log.empty());
  // An unauthenticated caller learns nothing about commands.
  r=run("{\"id\":\"1\",\"token\":\"bad\",\"cmd\":\"selfdestruct\"}",f,rl);assert(has(r,"unauthorized")&&!has(r,"unknown"));}
 // Validation after auth: unknown command, missing / wrong-typed / empty / oversize fields.
 {Fake f;RateLimiter rl(16,10);
  assert(has(run(cmd("\"cmd\":\"selfdestruct\""),f,rl),"\"error\":\"unknown command\""));assert(has(run(cmd("\"x\":1"),f,rl),"\"error\":\"unknown command\""));assert(has(run(cmd("\"cmd\":5"),f,rl),"unknown command"));
  assert(has(run(cmd("\"cmd\":\"send_channel\",\"text\":\"x\""),f,rl),"\"error\":\"missing channel\""));
  assert(has(run(cmd("\"cmd\":\"send_channel\",\"channel\":\"\",\"text\":\"x\""),f,rl),"missing channel"));
  assert(has(run(cmd("\"cmd\":\"send_channel\",\"channel\":\"Public\""),f,rl),"\"error\":\"missing text\""));
  assert(has(run(cmd("\"cmd\":\"send_channel\",\"channel\":\"Public\",\"text\":5"),f,rl),"missing text"));
  assert(has(run(cmd("\"cmd\":\"send_channel\",\"channel\":\"Public\",\"text\":\"  \\n \""),f,rl),"\"error\":\"empty text\""));
  assert(has(run(cmd("\"cmd\":\"send_dm\",\"text\":\"x\""),f,rl),"missing to"));
  assert(has(run(cmd("\"cmd\":\"send_dm\",\"to\":\"bob\""),f,rl),"missing text"));
  std::string ok(matter::MESSAGE_MAX_TEXT_BYTES,'a'),big(matter::MESSAGE_MAX_TEXT_BYTES+1,'a');
  assert(has(run(cmd("\"cmd\":\"send_dm\",\"to\":\"bob\",\"text\":\""+ok+"\""),f,rl),"\"ok\":true"));
  auto r=run(cmd("\"cmd\":\"send_dm\",\"to\":\"bob\",\"text\":\""+big+"\""),f,rl);assert(r=="{\"id\":\"7\",\"ok\":false,\"error\":\"Message full / 160-byte limit\"}");
  r=run(cmd("\"cmd\":\"send_channel\",\"channel\":\"c\",\"text\":\""+big+"\""),f,rl);assert(has(r,"160-byte limit"));
  // Multi-byte text counts bytes, as the composer does.
  std::string two;for(size_t i=0;i<matter::MESSAGE_MAX_TEXT_BYTES/2+1;i++)two+="\\u00e9";assert(has(run(cmd("\"cmd\":\"send_dm\",\"to\":\"bob\",\"text\":\""+two+"\""),f,rl),"160-byte limit"));
  assert(has(run(cmd("\"cmd\":\"advert\",\"position\":\"yes\""),f,rl),"position must be true or false"));
  size_t sent=f.log.size();assert(sent==1);}// only the 160-byte message got through
 // Allow sending off: sends are refused, read-only commands still work.
 {Fake f;RateLimiter rl(16,10);
  auto r=run(cmd("\"cmd\":\"send_channel\",\"channel\":\"Public\",\"text\":\"x\""),f,rl,0,false);assert(r=="{\"id\":\"7\",\"ok\":false,\"error\":\"sending is disabled\"}");
  assert(has(run(cmd("\"cmd\":\"send_dm\",\"to\":\"a\",\"text\":\"x\""),f,rl,0,false),"sending is disabled"));assert(has(run(cmd("\"cmd\":\"advert\""),f,rl,0,false),"sending is disabled"));assert(f.log.empty());
  assert(has(run(cmd("\"cmd\":\"ping\""),f,rl,0,false),"\"ok\":true")&&has(run(cmd("\"cmd\":\"list_channels\""),f,rl,0,false),"\"ok\":true")&&has(run(cmd("\"cmd\":\"list_contacts\""),f,rl,0,false),"\"ok\":true"));}
 // Host failures come back as errors with the host's reason.
 {Fake f;f.fail=true;RateLimiter rl(16,10);
  assert(run(cmd("\"cmd\":\"send_channel\",\"channel\":\"x\",\"text\":\"y\""),f,rl)=="{\"id\":\"7\",\"ok\":false,\"error\":\"Channel key required\"}");assert(has(run(cmd("\"cmd\":\"send_dm\",\"to\":\"x\",\"text\":\"y\""),f,rl),"No such contact"));assert(has(run(cmd("\"cmd\":\"advert\""),f,rl),"Radio offline"));}
 // Rate limit: counted per received payload (good or bad), answered with an error, recovers after the window.
 {Fake f;RateLimiter rl;int ok=0;for(int i=0;i<12;i++){auto r=run(i%2?"garbage":cmd("\"cmd\":\"ping\""),f,rl,5000+i);if(has(r,"rate limited")){assert(r=="{\"id\":null,\"ok\":false,\"error\":\"rate limited\"}");}else ok++;}
  assert(ok==10);assert(has(run(cmd("\"cmd\":\"ping\""),f,rl,5000+59999),"rate limited"));assert(has(run(cmd("\"cmd\":\"ping\""),f,rl,5000+60000),"\"ok\":true"));}
 // Oversize payloads are refused before parsing.
 {Fake f;assert(has(run(std::string(PayloadMax+1,' ')+cmd("\"cmd\":\"ping\""),f),"payload too large")&&f.log.empty());}
 // Responses escape what they echo.
 assert(response("a\"b",false,"x\\y\n","")=="{\"id\":\"a\\\"b\",\"ok\":false,\"error\":\"x\\\\y\\n\"}");
 // Bounded list builder: stops at the byte cap and says so.
 {ListBuilder l(60);assert(l.add("{\"n\":1}")&&l.add("{\"n\":2}")&&l.add("{\"n\":3}"));assert(!l.add(std::string(50,'x')));assert(l.done()=="{\"items\":[{\"n\":1},{\"n\":2},{\"n\":3}],\"count\":3,\"truncated\":true}");
  ListBuilder e(60);assert(e.done()=="{\"items\":[],\"count\":0,\"truncated\":false}");}
 // Message formatter.
 {Message m;m.token=0x2a;m.ts=1742083629;m.channel="Public";m.channelIndex=0;m.sender="Ana \"A\"";m.text="hi\nthere";m.hops=2;m.path="flood:ab,cd";m.hasSignal=true;m.snrTenths=55;m.rssiTenths=-912;
  assert(messageJson(m)=="{\"id\":\"0000002a\",\"event\":\"message\",\"ts\":1742083629,\"kind\":\"channel\",\"channel\":\"Public\",\"channel_index\":0,\"sender\":\"Ana \\\"A\\\"\",\"text\":\"hi\\nthere\",\"direction\":\"in\",\"hops\":2,\"route\":\"flood:ab,cd\",\"snr\":5.5,\"rssi\":-91.2}");
  Message d;d.token=0x2b;d.direct=true;d.out=true;d.contact="Bob";d.pubkey="A1B2C3D4E5F60718AAAA";d.sender="YOU";d.text="yo";d.state="Pending send";d.hops=-1;
  assert(messageJson(d)=="{\"id\":\"0000002b\",\"event\":\"message\",\"kind\":\"dm\",\"contact\":\"Bob\",\"pubkey\":\"a1b2c3d4e5f6\",\"sender\":\"YOU\",\"text\":\"yo\",\"direction\":\"out\",\"state\":\"Pending send\"}");
  Message e;e.event="delivery";e.token=0x2b;e.ts=5;e.state="Delivered";e.repeaters=2;e.heard="ab,cd";e.firstRepeater="ab";
  assert(messageJson(e)=="{\"id\":\"0000002b\",\"event\":\"delivery\",\"ts\":5,\"state\":\"Delivered\",\"repeaters\":2,\"heard\":\"ab,cd\",\"first_repeater\":\"ab\"}");
  // Every formatted message is a valid JSON object for the reader above.
  std::map<std::string,Value> back;assert(parseObject(messageJson(m),back)&&back["text"].s=="hi\nthere"&&back["snr"].s=="5.5");}
 // Target resolution: exact name (any case) first, else a hex key prefix of 6+ characters; none / ambiguous are told apart.
 {std::vector<Named> l={{"Ana","A1B2C3D4E5F6"},{"bob","a1b2ff0011"},{"BOB","99aabbccdd"},{"cafe","ca0000ffee"}};
  assert(resolve(l,"ana",6)==0&&resolve(l,"ANA",6)==0&&resolve(l,"cafe",6)==3);assert(resolve(l,"bob",6)==-2);assert(resolve(l,"nobody",6)==-1);
  assert(resolve(l,"a1b2c3",6)==0&&resolve(l,"A1B2FF",6)==1&&resolve(l,"a1b2",6)==-1&&resolve(l,"a1b2c3d4e5f6aa",6)==-1&&resolve(l,"a1b2",4)==-2);assert(resolve(l,"",6)==-1&&resolve(l,"zz",6)==-1);
  // A name wins over a key prefix that happens to spell it.
  std::vector<Named> m={{"a1b2c3","ffffffffffff"},{"x","a1b2c3000000"}};assert(resolve(m,"a1b2c3",6)==0);}
 // Topic leaves match the uplink's naming.
 assert(mqtt_uplink::topic("LIN","ab","cmd")=="meshcore/LIN/AB/cmd");
 std::cout<<"PASS: mqtt_downlink parser, auth, rate limit, command dispatch, response and message formatting\n";}
