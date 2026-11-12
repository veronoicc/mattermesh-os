#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <ctype.h>
// Settings > Cellular (CellularSection): what the user chose, kept in NVS namespace "mmcell" by services/modem.cpp.
// Pure (no Arduino) so the page logic and the host tests share it.
namespace device {
// Which link the data uses (weather, map tiles, MQTT): see services/net_route.h.
enum class NetMode:uint8_t {WifiOnly=0,WifiThenCell=1,CellOnly=2};
constexpr int NetModeCount=3;
inline const char *netModeName(NetMode m){return m==NetMode::WifiOnly?"Wi-Fi only":m==NetMode::CellOnly?"Cellular only":"Wi-Fi, then cellular";}
struct CellularSettings {
 bool modem=false;// modem powered and started
 NetMode network=NetMode::WifiThenCell;
 bool roaming=false;// data allowed while roaming
 std::string pin,apn,apnUser,apnPass;
 bool mqtt=false,mqttTls=false;// MQTT uplink of the received mesh packets
 std::string host,mqttUser,mqttPass,region;uint16_t port=1883;
 bool downlink=false,dlSend=false,dlPublish=true;std::string dlToken;// MQTT downlink (mqtt_downlink.h): commands in with the shared token, "Allow sending", chat messages published
};
constexpr size_t CellularPinMax=8,CellularTextMax=63,CellularRegionMax=8;
inline bool validSimPin(const std::string &s){if(s.empty())return true;if(s.size()<4||s.size()>CellularPinMax)return false;for(char c:s)if(c<'0'||c>'9')return false;return true;}
inline bool validMqttHost(const std::string &s){if(s.empty()||s.size()>CellularTextMax)return false;for(char c:s)if(uint8_t(c)<=32||uint8_t(c)>=127)return false;return true;}
inline bool validMqttPort(long p){return p>=1&&p<=65535;}
// Region code (an IATA-style airport code in meshcoretomqtt topics): letters and digits, upper case, 1..CellularRegionMax.
inline bool validRegion(const std::string &s){if(s.empty()||s.size()>CellularRegionMax)return false;for(char c:s)if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')))return false;return true;}
inline std::string normalizeRegion(std::string s){for(auto &c:s)c=char(toupper(uint8_t(c)));return s;}
inline uint16_t defaultMqttPort(bool tls){return tls?8883:1883;}
// MQTT uplink is complete enough to run: a broker, a valid port and region.
inline bool mqttConfigured(const CellularSettings &c){return validMqttHost(c.host)&&validMqttPort(c.port)&&validRegion(c.region);}
const CellularSettings &cellularSettings();// current values (NVS-backed, loaded at modemBegin)
CellularSettings cellularSettingsCopy();// the same values, safe from any task
bool cellularApply(const CellularSettings &next);// stores them in NVS, restarts what changed; false if NVS refused
}
