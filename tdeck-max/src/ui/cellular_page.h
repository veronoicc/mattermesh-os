#pragma once
#include <algorithm>
#include <string>
#include "services/cellular_settings.h"
#include "services/mqtt_downlink.h"
#include "services/modem_status.h"
// Settings > Cellular (51): the pure logic behind the page. Focus 0 is the Modem switch, focus 1.. the rows (the last
// row, the MQTT status line, is informative and has no focus). The rows scroll CellVisible at a time.
namespace mm {
enum CellRow {CellPin,CellApn,CellApnUser,CellApnPass,CellNetwork,CellRoaming,CellMqtt,CellHost,CellPort,CellMqttUser,CellMqttPass,CellRegion,CellTls,CellDownlink,CellDlToken,CellDlGenerate,CellDlSend,CellDlPublish,CellMqttInfo,CellRows};
enum CellKind {CellText,CellStep,CellToggle,CellInfo,CellAction};
constexpr int CellVisible=4,CellFocusCount=CellRows;
inline CellKind cellKind(int row){return row==CellNetwork?CellStep:row==CellRoaming||row==CellMqtt||row==CellTls||row==CellDownlink||row==CellDlSend||row==CellDlPublish?CellToggle:row==CellMqttInfo?CellInfo:row==CellDlGenerate?CellAction:CellText;}
inline const char *cellRowLabel(int row){static const char *n[CellRows]={"SIM PIN","APN","APN user","APN pass","Network","Roaming","MQTT uplink","Broker","Port","MQTT user","MQTT pass","Region","TLS","Downlink","Cmd token","Generate","Allow send","Publish","MQTT"};return row>=0&&row<CellRows?n[row]:"";}
inline const char *cellGroup(int row){return row<CellNetwork?"SIM & APN":row<CellMqtt?"NETWORK":row<CellDownlink?"MQTT UPLINK":"MQTT DOWNLINK";}
inline int cellFocusRow(int focus){return focus-1;}// -1: the Modem switch
inline int cellMoveFocus(int focus,int dir){focus+=dir;return focus<0?0:focus>=CellFocusCount?CellFocusCount-1:focus;}
inline int cellMaxScroll(){return CellRows-CellVisible;}
inline int cellFollow(int scroll,int focus){int row=cellFocusRow(focus);if(row<0)return scroll;if(row<scroll)return row;if(row>=scroll+CellVisible)return row-CellVisible+1;return scroll;}
inline int cellPageScroll(int scroll,bool down){return down?std::min(cellMaxScroll(),scroll+CellVisible):std::max(0,scroll-CellVisible);}
inline device::NetMode cellCycleNetMode(device::NetMode m,int dir){return device::NetMode((int(m)+(dir<0?device::NetModeCount-1:1))%device::NetModeCount);}
// Port text: valid keeps it, empty / invalid restores the default of the current TLS mode (false then).
inline bool cellApplyPortText(device::CellularSettings &c,const std::string &text){std::string t;for(char ch:text)if(ch!=' ')t+=ch;
 long v=0;bool ok=!t.empty()&&t.size()<=5;for(char ch:t){if(ch<'0'||ch>'9'){ok=false;break;}v=v*10+(ch-'0');}
 if(ok&&device::validMqttPort(v)){c.port=uint16_t(v);return true;}c.port=device::defaultMqttPort(c.mqttTls);return false;}
// Switching TLS moves the port only while it is still the other mode's default.
inline uint16_t cellNextTlsPort(uint16_t port,bool newTls){return port==device::defaultMqttPort(!newTls)?device::defaultMqttPort(newTls):port;}
inline void cellToggle(device::CellularSettings &c,int row){if(row==CellRoaming)c.roaming=!c.roaming;else if(row==CellMqtt)c.mqtt=!c.mqtt;else if(row==CellDownlink)c.downlink=!c.downlink;else if(row==CellDlSend)c.dlSend=!c.dlSend;else if(row==CellDlPublish)c.dlPublish=!c.dlPublish;else if(row==CellTls){c.mqttTls=!c.mqttTls;c.port=cellNextTlsPort(c.port,c.mqttTls);}}
inline std::string cellMaskedText(const std::string &s){return std::string(s.size(),'*');}
inline bool cellSecret(int row){return row==CellPin||row==CellApnPass||row==CellMqttPass||row==CellDlToken;}
inline std::string cellRowText(const device::CellularSettings &c,int row){
 auto or_none=[](const std::string &s){return s.empty()?std::string("not set"):s;};
 switch(row){case CellPin:return or_none(cellMaskedText(c.pin));case CellApn:return or_none(c.apn);case CellApnUser:return or_none(c.apnUser);case CellApnPass:return or_none(cellMaskedText(c.apnPass));
  case CellNetwork:return device::netModeName(c.network);case CellHost:return or_none(c.host);case CellPort:return std::to_string(c.port);case CellMqttUser:return or_none(c.mqttUser);
  case CellMqttPass:return or_none(cellMaskedText(c.mqttPass));case CellRegion:return or_none(c.region);case CellDlToken:return or_none(cellMaskedText(c.dlToken));case CellDlGenerate:return "Press ENTER";}return "";}
inline bool cellRowOn(const device::CellularSettings &c,int row){return row==CellRoaming?c.roaming:row==CellMqtt?c.mqtt:row==CellTls?c.mqttTls:row==CellDownlink?c.downlink:row==CellDlSend?c.dlSend:row==CellDlPublish?c.dlPublish:false;}
// What the editor starts with: the real value (secrets are shown plainly while edited).
inline std::string cellEditText(const device::CellularSettings &c,int row){switch(row){case CellPin:return c.pin;case CellApn:return c.apn;case CellApnUser:return c.apnUser;case CellApnPass:return c.apnPass;case CellHost:return c.host;
 case CellPort:return std::to_string(c.port);case CellMqttUser:return c.mqttUser;case CellMqttPass:return c.mqttPass;case CellRegion:return c.region;case CellDlToken:return c.dlToken;}return "";}
// Typing: the character to append (0 = rejected).
inline char cellAcceptChar(int row,const std::string &cur,char ch){uint8_t u=uint8_t(ch);if(u<32||u>126)return 0;
 size_t cap=row==CellPin?device::CellularPinMax:row==CellPort?5:row==CellRegion?device::CellularRegionMax:row==CellDlToken?mqtt_downlink::TokenMax:device::CellularTextMax;if(cur.size()>=cap)return 0;
 if(row==CellPin||row==CellPort)return ch>='0'&&ch<='9'?ch:0;
 if(row==CellDlToken)return ch==' '||ch=='"'||ch=='\\'?0:ch;
 if(row==CellRegion){if(ch>='a'&&ch<='z')return char(ch-32);return (ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')?ch:0;}
 if(row==CellHost)return ch==' '?0:ch;
 if(row==CellApn||row==CellApnUser||row==CellApnPass)return ch=='"'?0:ch;// the AT command quotes them
 return ch;}
inline std::string cellTrim(const std::string &s){size_t a=s.find_first_not_of(' '),b=s.find_last_not_of(' ');return a==std::string::npos?std::string():s.substr(a,b-a+1);}
// Commit an edited text into c: false (c unchanged, why = the reason) when invalid. A port that is empty or invalid is
// reset to the default (true; why explains an invalid, not an empty, text).
inline bool cellCommit(device::CellularSettings &c,int row,const std::string &raw,std::string &why){
 std::string t=cellSecret(row)&&row!=CellPin?raw:cellTrim(raw);
 switch(row){
  case CellPin:if(!device::validSimPin(t)){why="PIN: 4-8 digits";return false;}c.pin=t;return true;
  case CellApn:c.apn=t;return true;case CellApnUser:c.apnUser=t;return true;case CellApnPass:c.apnPass=t;return true;
  case CellHost:if(!t.empty()&&!device::validMqttHost(t)){why="Broker: no spaces, up to 63";return false;}c.host=t;return true;
  case CellPort:if(!cellApplyPortText(c,t)&&!t.empty())why="Port: 1-65535, reset to "+std::to_string(c.port);return true;
  case CellMqttUser:c.mqttUser=t;return true;case CellMqttPass:c.mqttPass=t;return true;
  case CellRegion:{t=device::normalizeRegion(t);if(!t.empty()&&!device::validRegion(t)){why="Region: A-Z 0-9, up to 8";return false;}c.region=t;return true;}
  case CellDlToken:{t=cellTrim(t);if(!t.empty()&&!mqtt_downlink::validToken(t)){why="Token: 8-64 characters, no spaces";return false;}c.dlToken=t;return true;}}
 return false;}
inline std::string cellDbm(int csq){return csq<0||csq>31?"--":std::to_string(-113+2*csq)+" dBm";}
inline std::string cellSimText(device::ModemPhase p,bool simReady){using P=device::ModemPhase;return p==P::NoSim?"No SIM":p==P::PinNeeded?"PIN needed":p==P::PukNeeded?"PUK locked":p==P::Off||p==P::Powering||p==P::Starting||p==P::Failed?"--":simReady?"Ready":"Checking";}
inline const char *cellRatText(int rat){return rat==1?"2G":rat==2?"3G":rat==3?"4G":"--";}
}
