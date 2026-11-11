#pragma once
#include <stdint.h>
#include "services/cellular_settings.h"
// Which link carries the data of weather, map tiles and the MQTT uplink (Settings > Cellular > Network).
namespace device {
enum class NetPath:uint8_t {None,Wifi,Cell};
inline NetPath netRoute(NetMode mode,bool wifiUp,bool cellUp){switch(mode){case NetMode::WifiOnly:return wifiUp?NetPath::Wifi:NetPath::None;case NetMode::CellOnly:return cellUp?NetPath::Cell:NetPath::None;default:return wifiUp?NetPath::Wifi:cellUp?NetPath::Cell:NetPath::None;}}
// Whether the modem should hold its data bearer open. Wi-Fi first: the bearer opens when Wi-Fi is down and closes only after
// Wi-Fi has stayed up for WifiGraceMs (a flapping access point must not open and close the bearer each time).
struct DataWish {
 static constexpr uint32_t WifiGraceMs=30000;
 bool wanted=false;uint32_t wifiSince=0;bool wifiWas=false;
 bool update(NetMode mode,bool wifiUp,uint32_t now){
  if(wifiUp&&!wifiWas)wifiSince=now;wifiWas=wifiUp;
  if(mode==NetMode::WifiOnly)wanted=false;else if(mode==NetMode::CellOnly)wanted=true;
  else if(!wifiUp)wanted=true;else if(wanted&&uint32_t(now-wifiSince)>=WifiGraceMs)wanted=false;
  return wanted;}
};
}
