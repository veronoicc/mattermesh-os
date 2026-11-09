#pragma once
// Client repeat mode (MeshCore companion_radio protocol v9+): the phone app turns the T-Deck into a repeater.
// Pure decisions only; service.cpp / companion.inc call them:
//  - CMD_SET_RADIO_PARAMS: repeat byte (absent = 0). repeat && !validFreq(kHz) -> ERR_ILLEGAL_ARG, else the frequency, bandwidth, SF
//    and CR are validated as usual and the repeat flag is stored with them (a frame without the byte clears it, as MyMesh).
//  - CMD_GET_ALLOWED_REPEAT_FREQ (60): allowedFrame() is the whole reply (RESP_ALLOWED_REPEAT_FREQ 26).
//  - CMD_DEVICE_QUERY reports the stored flag in the byte after the firmware version string (the app reads it there).
//  - ChatMesh::allowPacketForward returns mayForward(repeat, queued packets, remaining TX budget).
//  - matter::radioSuspend(true) (the companion link switches the radio off) is refused unless mayStopRadio(repeat).
// MeshCore forwards a flood after getRetransmitDelay() at priority = path length (own sends are priority 0, sent first).
// service.cpp send()/advertise() refuse above 8 queued packets and the pool holds 24, so forwarding stops at ForwardQueueMax
// to leave queue and airtime to the node's own messages.
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace repeat_policy {
constexpr uint8_t RespAllowedRepeatFreq=26;
constexpr int ForwardQueueMax=6;
constexpr unsigned long ForwardBudgetReserveMs=30000;
struct FreqRange {uint32_t lower,upper;};
// kHz, as MyMesh.cpp's repeat_freq_ranges (same override macro).
constexpr FreqRange kRanges[]={
#ifdef ALLOWED_REPEAT_FREQ_RANGE
 ALLOWED_REPEAT_FREQ_RANGE
#else
 {433000,433000},{869495,869495},{918000,918000}
#endif
};
template<size_t N> bool inRanges(const FreqRange (&r)[N],uint32_t khz){for(auto &x:r)if(khz>=x.lower&&khz<=x.upper)return true;return false;}
inline bool validFreq(uint32_t khz){return inRanges(kRanges,khz);}
// Pairs stop while i+8 < capacity, as MyMesh's loop over its out_frame.
template<size_t N> size_t allowedFrame(const FreqRange (&r)[N],uint8_t *out,size_t cap){
 size_t i=0;out[i++]=RespAllowedRepeatFreq;
 for(auto &x:r){if(i+8>=cap)break;memcpy(out+i,&x.lower,4);i+=4;memcpy(out+i,&x.upper,4);i+=4;}
 return i;}
inline size_t allowedFrame(uint8_t *out,size_t cap){return allowedFrame(kRanges,out,cap);}
inline bool mayForward(bool repeat,int queued,unsigned long budgetMs){return repeat&&queued<ForwardQueueMax&&budgetMs>=ForwardBudgetReserveMs;}
inline bool mayStopRadio(bool repeat){return !repeat;}
}
