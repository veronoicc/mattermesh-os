#include <initializer_list>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "mesh/repeat_policy.h"
using namespace repeat_policy;
int main(){
 // isValidClientRepeatFreq (MyMesh.cpp): kHz, default ranges are the three single points, edges inclusive.
 for(uint32_t f:{433000u,869495u,918000u})assert(validFreq(f));
 for(uint32_t f:{0u,432999u,433001u,869494u,869496u,869618u,917999u,918001u,915000u,UINT32_MAX})assert(!validFreq(f));
 // A real range: both ends in, one step either side out; a list is searched whole.
 const FreqRange wide[]={{868000,869000},{902000,928000}};
 for(uint32_t f:{868000u,869000u,902000u,928000u,915000u})assert(inRanges(wide,f));
 for(uint32_t f:{867999u,869001u,901999u,928001u})assert(!inRanges(wide,f));
 // RESP_ALLOWED_REPEAT_FREQ (26): code, then lower/upper uint32 LE pairs, as MyMesh writes them.
 uint8_t out[172];size_t n=allowedFrame(out,sizeof(out));
 const uint8_t expect[]={26, 0x68,0x9B,0x06,0x00, 0x68,0x9B,0x06,0x00, 0x77,0x44,0x0D,0x00, 0x77,0x44,0x0D,0x00, 0xF0,0x01,0x0E,0x00, 0xF0,0x01,0x0E,0x00};
 assert(n==sizeof(expect)&&!memcmp(out,expect,n));
 n=allowedFrame(wide,out,sizeof(out));uint32_t lo,hi;assert(n==17&&out[0]==26);memcpy(&lo,out+9,4);memcpy(&hi,out+13,4);assert(lo==902000&&hi==928000);
 // MyMesh stops while i+8 < sizeof(out_frame): a 172-byte frame holds 21 pairs, a short buffer never overruns.
 FreqRange many[30];for(int i=0;i<30;i++)many[i]={uint32_t(i),uint32_t(i)};
 assert(allowedFrame(many,out,172)==1+21*8);assert(allowedFrame(many,out,10)==9&&allowedFrame(many,out,9)==1);assert(allowedFrame(many,out,18)==17);
 static_assert(ForwardQueueMax<8,"own sends refuse above 8 queued: forwarded packets must leave room");
 assert(!mayForward(false,0,360000));
 assert(mayForward(true,0,360000)&&mayForward(true,ForwardQueueMax-1,ForwardBudgetReserveMs));
 assert(!mayForward(true,ForwardQueueMax,360000)&&!mayForward(true,0,ForwardBudgetReserveMs-1)&&!mayForward(true,0,0));
 // A repeating device keeps listening: the companion link may only switch the radio off while repeat is off.
 assert(mayStopRadio(false)&&!mayStopRadio(true));
 puts("PASS: repeat policy - client repeat frequency ranges (inclusive edges), RESP_ALLOWED_REPEAT_FREQ bytes as MeshCore, forwarding queue/airtime reserve, radio stays on while repeating");
}
