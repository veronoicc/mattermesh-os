#pragma once
#include <stdint.h>
#include <string>
#include <vector>
// What the LTE modem service (services/modem.cpp) reports to the Cellular page and the SMS app. Pure: no Arduino.
namespace device {
enum class ModemPhase:uint8_t {Off,Powering,Starting,NoSim,PinNeeded,PukNeeded,Searching,Registered,Denied,Failed};
enum class DataPhase:uint8_t {Idle,Connecting,Online,Failed};
inline const char *modemPhaseText(ModemPhase p){switch(p){case ModemPhase::Off:return "Off";case ModemPhase::Powering:return "Powering on";case ModemPhase::Starting:return "Starting";case ModemPhase::NoSim:return "No SIM card";case ModemPhase::PinNeeded:return "SIM PIN needed";case ModemPhase::PukNeeded:return "SIM locked (PUK)";case ModemPhase::Searching:return "Searching network";case ModemPhase::Registered:return "Registered";case ModemPhase::Denied:return "Registration denied";default:return "Modem not responding";}}
inline const char *dataPhaseText(DataPhase p){return p==DataPhase::Connecting?"Connecting":p==DataPhase::Online?"Online":p==DataPhase::Failed?"Failed":"Idle";}
struct ModemStatus {
 ModemPhase phase=ModemPhase::Off;DataPhase data=DataPhase::Idle;
 std::string model;// "A7682E", "A7670G" or "SIMCom" when ATI is not recognised; empty until known
 std::string operatorName,ip,message;// message: last problem, "" when none
 int csq=99,bars=0;// raw +CSQ (99 unknown) and 0..4
 int rat=0;// 0 unknown, 1 2G, 2 3G, 3 4G
 bool simReady=false,roaming=false;
 int smsCount=0,smsUnread=0;
 uint32_t revision=0;// changes with every field above
};
// One text message, newest first in smsInbox(). time is "DD/MM HH:MM" in the network's local time ("" if unknown).
struct SmsMessage {int index=0;bool unread=false;std::string number,time,text;};
enum class SmsSendPhase:uint8_t {Idle,Sending,Sent,Failed};
struct SmsSendState {SmsSendPhase phase=SmsSendPhase::Idle;std::string message;};
}
