#include <cassert>
#include <iostream>
#include "services/net_route.h"
using namespace device;
int main(){
 // Every mode against every link state.
 for(bool w:{false,true})for(bool c:{false,true}){
  assert(netRoute(NetMode::WifiOnly,w,c)==(w?NetPath::Wifi:NetPath::None));
  assert(netRoute(NetMode::CellOnly,w,c)==(c?NetPath::Cell:NetPath::None));
  assert(netRoute(NetMode::WifiThenCell,w,c)==(w?NetPath::Wifi:c?NetPath::Cell:NetPath::None));}
 // The bearer wish: Wi-Fi only never wants it, Cellular only always, Wi-Fi first only while Wi-Fi is away (plus a grace period).
 DataWish a;for(uint32_t t=0;t<100000;t+=1000){assert(!a.update(NetMode::WifiOnly,false,t));}
 DataWish b;for(uint32_t t=0;t<100000;t+=1000){assert(b.update(NetMode::CellOnly,t%7000<3000,t));}
 DataWish c;assert(!c.update(NetMode::WifiThenCell,true,0));assert(c.update(NetMode::WifiThenCell,false,1000));
 assert(c.update(NetMode::WifiThenCell,true,2000));assert(c.update(NetMode::WifiThenCell,true,31999));assert(!c.update(NetMode::WifiThenCell,true,32000));
 assert(c.update(NetMode::WifiThenCell,false,33000));assert(c.update(NetMode::WifiThenCell,true,34000));assert(c.update(NetMode::WifiThenCell,false,35000));assert(c.update(NetMode::WifiThenCell,true,36000));assert(c.update(NetMode::WifiThenCell,true,65999));assert(!c.update(NetMode::WifiThenCell,true,66000));
 DataWish d;d.update(NetMode::CellOnly,false,0);assert(!d.update(NetMode::WifiOnly,false,1));assert(d.update(NetMode::WifiThenCell,false,2));
 std::cout<<"PASS: net routing for every mode and link state, data bearer wish with Wi-Fi grace\n";
}
