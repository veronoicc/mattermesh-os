// PC emulator for client repeat mode (src/mesh/repeat_policy.h) on the real MeshCore core: A - B - C in a line, A cannot hear C.
// B is the T-Deck: its allowPacketForward is repeat_policy::mayForward(repeat flag, outbound queue, remaining TX budget), as
// ChatMesh in service.cpp. A floods a signed advert; C only receives it (with B as the single hop) while B's repeat is on.
#include "companion_sim.h"
#include "mesh/repeat_policy.h"
namespace {

// A and C: plain MeshCore nodes that flood adverts and count the ones they receive.
class Endpoint:public Parts,public mesh::Mesh {
public:int heard=0,hops=-1;std::string text;
 Endpoint(uint32_t seed):mesh::Mesh(radio,ms,rng,clock,pool,tables){rng.g.seed(seed);self_id=mesh::LocalIdentity(&rng);begin();}
 void say(const char *t){auto *p=createAdvert(self_id,(const uint8_t*)t,strlen(t));assert(p);sendFlood(p);}
protected:
 void onAdvertRecv(mesh::Packet *p,const mesh::Identity&,uint32_t,const uint8_t *d,size_t n)override{heard++;text.assign((const char*)d,n);hops=p->isRouteFlood()?p->getPathHashCount():-1;}
};

// B: a plain MeshCore node with the firmware's forwarding rule and the firmware's airtime budget factor (9).
class Repeater:public Parts,public mesh::Mesh {
public:bool repeat=false;float factor;uint32_t forwarded=0;
 Repeater(uint32_t seed,float f):mesh::Mesh(radio,ms,rng,clock,pool,tables),factor(f){rng.g.seed(seed);self_id=mesh::LocalIdentity(&rng);begin();}
 // CMD_SET_RADIO_PARAMS: repeat only at an allowed frequency (kHz); clearing always works.
 bool setRepeat(bool on,uint32_t khz){if(on&&!repeat_policy::validFreq(khz))return false;repeat=on;return true;}
 // Own traffic parked in the queue (never sent within the test), as a busy node.
 void park(int n){for(int i=0;i<n;i++){uint8_t d[4]={1,2,3,uint8_t(i)};auto *p=createRawData(d,4);assert(p);sendFlood(p,10000000);}}
 int queued(){return pool.getOutboundTotal();}
protected:
 bool allowPacketForward(const mesh::Packet*)override{bool ok=repeat_policy::mayForward(repeat,pool.getOutboundTotal(),getRemainingTxBudget());if(ok)forwarded++;return ok;}
 float getAirtimeBudgetFactor()const override{return factor;}int calcRxDelay(float,uint32_t)const override{return 0;}
};
}

int main(){
 setvbuf(stdout,nullptr,_IONBF,0);
 auto *a=new Endpoint(1);auto *b=new Repeater(2,9.0f);auto *c=new Endpoint(3);link(a->radio,b->radio);link(b->radio,c->radio);
 auto go=[&](int ms){run(ms,*a,*b,*c);};
 // Repeat off (the default): B hears A, nobody passes it on.
 a->say("one");go(3000);assert(c->heard==0&&b->forwarded==0);
 // Repeat on at an allowed frequency (869.495 MHz): C gets it, one hop.
 assert(b->setRepeat(true,869495));a->say("two");go(3000);assert(c->heard==1&&c->text=="two"&&c->hops==1&&b->forwarded==1);
 // Cleared again: the next flood stops at B.
 assert(b->setRepeat(false,869618));a->say("three");go(3000);assert(c->heard==1);
 // A frequency outside the allowed list cannot switch repeat on.
 assert(!b->setRepeat(true,869618)&&!b->repeat);a->say("four");go(3000);assert(c->heard==1);
 // The queue keeps room for B's own messages: with ForwardQueueMax-1 packets waiting B still forwards, at ForwardQueueMax it does not.
 assert(b->setRepeat(true,918000));b->park(repeat_policy::ForwardQueueMax-1);assert(b->queued()==repeat_policy::ForwardQueueMax-1);
 a->say("five");go(3000);assert(c->heard==2&&c->text=="five");
 b->park(1);assert(b->queued()==repeat_policy::ForwardQueueMax);
 a->say("six");go(3000);assert(c->heard==2);
 // Airtime: a node whose TX budget is below the reserve for own messages does not forward even with repeat on.
 auto *a2=new Endpoint(4);auto *b2=new Repeater(5,1e6f);auto *c2=new Endpoint(6);link(a2->radio,b2->radio);link(b2->radio,c2->radio);
 assert(b2->getRemainingTxBudget()<repeat_policy::ForwardBudgetReserveMs&&b2->setRepeat(true,433000));
 a2->say("seven");run(3000,*a2,*b2,*c2);assert(c2->heard==0&&b2->forwarded==0);
 delete a;delete b;delete c;delete a2;delete b2;delete c2;
 printf("PASS: repeat emulator - A-B-C line: C hears A's flood only while B's repeat is on (one hop), not after it is cleared, not at a disallowed frequency, not with the forwarding queue full or the airtime budget under the reserve\n");
}
