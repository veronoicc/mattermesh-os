#pragma once
namespace mm {
// Settings > LoRa > Device, Repeat row: flip the mode through the service setter; on refusal the notice carries its reason.
template<class Str,class Set> void repeatToggle(bool on,Set set,Str &notice){Str reason;if(set(!on,reason))notice=on?"Repeat off":"Repeat on";else notice=reason;}
}
