#pragma once
#include <string>
namespace modememu {
// The emulated SIMCom A7682E on Serial1 (the firmware's modem service, src/services/modem.cpp). present=false: nothing answers.
void configure(bool present);
void attach();   // before setup()
// A text message arriving at once (unread, +CMTI), and one arriving `afterMs` after start (--sms).
void injectSms(const std::string &number,const std::string &text);
void scheduleSms(unsigned afterMs,const std::string &text);
}
