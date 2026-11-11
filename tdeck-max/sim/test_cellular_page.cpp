#include <cassert>
#include <iostream>
#include "ui/cellular_page.h"
using namespace mm;using device::CellularSettings;using device::NetMode;
int main(){
 // Rows: kinds and labels; the info row is last and never focusable.
 assert(CellRows==14&&CellFocusCount==CellRows);
 assert(cellKind(CellPin)==CellText&&cellKind(CellApn)==CellText&&cellKind(CellApnPass)==CellText&&cellKind(CellNetwork)==CellStep);
 assert(cellKind(CellRoaming)==CellToggle&&cellKind(CellMqtt)==CellToggle&&cellKind(CellTls)==CellToggle&&cellKind(CellPort)==CellText&&cellKind(CellMqttInfo)==CellInfo);
 for(int r=0;r<CellRows;r++)assert(*cellRowLabel(r)&&*cellGroup(r));
 // Focus 0 is the Modem switch, 1..13 the rows 0..12; clamped; the info row has no focus.
 assert(cellMoveFocus(0,-1)==0&&cellMoveFocus(0,1)==1&&cellMoveFocus(CellFocusCount-1,1)==CellFocusCount-1&&cellMoveFocus(5,-1)==4);
 assert(cellFocusRow(0)==-1&&cellFocusRow(1)==CellPin&&cellFocusRow(CellFocusCount-1)==CellTls);
 // Scroll follows the focus so the focused row is always in the window; the switch leaves it alone.
 for(int start=0;start<=cellMaxScroll();start++)for(int f=0;f<CellFocusCount;f++){int s=cellFollow(start,f);assert(s>=0&&s<=cellMaxScroll());int row=cellFocusRow(f);if(row>=0)assert(row>=s&&row<s+CellVisible);else assert(s==start);}
 assert(cellMaxScroll()==CellRows-CellVisible&&cellPageScroll(0,false)==0&&cellPageScroll(0,true)==CellVisible&&cellPageScroll(cellMaxScroll(),true)==cellMaxScroll()&&cellPageScroll(cellMaxScroll(),false)==cellMaxScroll()-CellVisible);
 // Network selector cycles through the three modes both ways.
 assert(cellCycleNetMode(NetMode::WifiOnly,1)==NetMode::WifiThenCell&&cellCycleNetMode(NetMode::WifiThenCell,1)==NetMode::CellOnly&&cellCycleNetMode(NetMode::CellOnly,1)==NetMode::WifiOnly);
 assert(cellCycleNetMode(NetMode::WifiOnly,-1)==NetMode::CellOnly&&cellCycleNetMode(NetMode::CellOnly,-1)==NetMode::WifiThenCell);
 // Port text: valid 1..65535 kept, empty / invalid restores the default of the current TLS mode.
 CellularSettings c;c.port=1234;assert(cellApplyPortText(c,"8080")&&c.port==8080);assert(cellApplyPortText(c," 65535 ")&&c.port==65535);
 assert(!cellApplyPortText(c,"0")&&c.port==1883);c.port=5;assert(!cellApplyPortText(c,"70000")&&c.port==1883);c.port=5;assert(!cellApplyPortText(c,"")&&c.port==1883);
 c.mqttTls=true;assert(!cellApplyPortText(c,"abc")&&c.port==8883);
 // TLS toggle: the port follows only while it is still the other mode's default.
 assert(cellNextTlsPort(1883,true)==8883&&cellNextTlsPort(8883,false)==1883&&cellNextTlsPort(1234,true)==1234&&cellNextTlsPort(8883,true)==8883&&cellNextTlsPort(1883,false)==1883);
 {CellularSettings t;cellToggle(t,CellTls);assert(t.mqttTls&&t.port==8883);cellToggle(t,CellTls);assert(!t.mqttTls&&t.port==1883);t.port=9999;cellToggle(t,CellTls);assert(t.port==9999);
  cellToggle(t,CellRoaming);assert(t.roaming);cellToggle(t,CellMqtt);assert(t.mqtt);cellToggle(t,CellMqtt);assert(!t.mqtt);}
 // Masking and display values.
 assert(cellMaskedText("")==""&&cellMaskedText("1234")=="****");
 {CellularSettings t;assert(cellRowText(t,CellPin)=="not set"&&cellRowText(t,CellHost)=="not set"&&cellRowText(t,CellApn)=="not set");t.pin="1234";t.apnPass="secret";t.apn="internet";t.port=8883;t.network=NetMode::CellOnly;
  assert(cellRowText(t,CellPin)=="****"&&cellRowText(t,CellApnPass)=="******"&&cellRowText(t,CellApn)=="internet"&&cellRowText(t,CellPort)=="8883"&&cellRowText(t,CellNetwork)=="Cellular only");
  assert(!cellRowOn(t,CellRoaming));t.roaming=true;assert(cellRowOn(t,CellRoaming));}
 // Typing rules per field.
 assert(cellAcceptChar(CellPin,"",'5')=='5'&&!cellAcceptChar(CellPin,"",'x')&&!cellAcceptChar(CellPin,"12345678",'1'));
 assert(cellAcceptChar(CellPort,"",'8')=='8'&&!cellAcceptChar(CellPort,"",'a')&&!cellAcceptChar(CellPort,"65535",'1'));
 assert(cellAcceptChar(CellRegion,"",'z')=='Z'&&cellAcceptChar(CellRegion,"",'7')=='7'&&!cellAcceptChar(CellRegion,"",'-')&&!cellAcceptChar(CellRegion,"ABCDEFGH",'A'));
 assert(cellAcceptChar(CellApn,"",'a')=='a'&&cellAcceptChar(CellApn,"",' ')==' '&&!cellAcceptChar(CellApn,"",'"')&&!cellAcceptChar(CellApn,std::string(63,'a'),'a'));
 assert(!cellAcceptChar(CellHost,"",' ')&&cellAcceptChar(CellHost,"",'.')=='.'&&cellAcceptChar(CellMqttPass,"",'"')=='"'&&!cellAcceptChar(CellApn,"",char(200)));
 // Commit validates; invalid keeps the old value and says why.
 {CellularSettings t;std::string why;t.pin="1111";
  assert(!cellCommit(t,CellPin,"12",why)&&t.pin=="1111"&&why=="PIN: 4-8 digits");why="";assert(cellCommit(t,CellPin,"123456",why)&&t.pin=="123456");assert(cellCommit(t,CellPin,"",why)&&t.pin.empty());
  assert(cellCommit(t,CellApn," internet ",why)&&t.apn=="internet");assert(cellCommit(t,CellApnPass," pw ",why)&&t.apnPass==" pw ");
  assert(cellCommit(t,CellHost,"broker.example.org",why)&&t.host=="broker.example.org");assert(!cellCommit(t,CellHost,"a b",why)&&t.host=="broker.example.org"&&why=="Broker: no spaces, up to 63");
  assert(cellCommit(t,CellRegion,"lin",why)&&t.region=="LIN");assert(!cellCommit(t,CellRegion,"TOOLONGREGION",why)&&t.region=="LIN"&&why=="Region: A-Z 0-9, up to 8");
  assert(cellCommit(t,CellPort,"abc",why)&&t.port==1883&&why=="Port: 1-65535, reset to 1883");why="";assert(cellCommit(t,CellPort,"2000",why)&&t.port==2000&&why.empty());
  assert(cellCommit(t,CellMqttUser,"u",why)&&t.mqttUser=="u"&&cellCommit(t,CellMqttPass,"p",why)&&t.mqttPass=="p"&&cellCommit(t,CellApnUser,"au",why)&&t.apnUser=="au");}
 // Editor start text: secrets are shown plain while edited.
 {CellularSettings t;t.pin="1234";t.port=8883;assert(cellEditText(t,CellPin)=="1234"&&cellEditText(t,CellPort)=="8883");}
 // Status helpers.
 assert(cellDbm(99)=="--"&&cellDbm(0)=="-113 dBm"&&cellDbm(31)=="-51 dBm"&&cellDbm(20)=="-73 dBm");
 assert(cellSimText(device::ModemPhase::NoSim,false)=="No SIM"&&cellSimText(device::ModemPhase::PinNeeded,false)=="PIN needed"&&cellSimText(device::ModemPhase::PukNeeded,false)=="PUK locked"&&cellSimText(device::ModemPhase::Off,false)=="--"&&cellSimText(device::ModemPhase::Registered,true)=="Ready");
 assert(std::string(cellRatText(0))=="--"&&std::string(cellRatText(1))=="2G"&&std::string(cellRatText(2))=="3G"&&std::string(cellRatText(3))=="4G");
 std::cout<<"PASS: cellular page rows, focus/scroll follow, net mode, port rules, TLS default port, masking, typing and commit validation\n";
}
