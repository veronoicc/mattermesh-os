#pragma once
#include <ExtensionIOXL9555.hpp>
#include "services/cellular_settings.h"
#include "services/modem_status.h"
#include "services/net_route.h"
// LTE modem (SIMCom A7682E or A7670G, UART1 on GPIO10/11, supply and PWRKEY through the XL9555). Everything here is
// called from the main loop except where noted; the AT work runs in its own task, so none of it blocks the UI.
namespace device {
void modemBegin(ExtensionIOXL9555 &io,bool ioReady);// loads the settings; starts the modem when switched on
void modemLoop();// main loop: applies the XL9555 pin requests of the modem task, the RI pin, the data bearer wish
ModemStatus modemStatus();// a copy; cheap
bool modemEnabled();
// Link state for the other services (any task).
bool cellularDataUp();// the bearer is open and the roaming rule allows it
// Which link the data takes right now (Settings > Cellular > Network, Wi-Fi state, bearer state); any task.
NetPath netPath();
inline bool netUp(){return netPath()!=NetPath::None;}
// Cellular sockets (AT+CIPOPEN links 0..9) for services/cell_client.cpp. Any task; each call blocks the calling task until the module answers.
int cellOpen(const std::string &host,uint16_t port,uint32_t timeoutMs);// link number or -1
int cellSend(int link,const uint8_t *data,size_t n);// bytes sent, -1 on failure
int cellRecv(int link,uint8_t *out,size_t n);// bytes read, 0 when nothing is waiting
bool cellLinkAlive(int link);// open, or closed by the peer with data still waiting
bool cellDataWaiting(int link);// the module holds received bytes for this link
void cellClose(int link);
// SMS (RAM cache of the SIM store; refreshed on +CMTI, after send/delete and every few minutes).
std::vector<SmsMessage> smsInbox();// newest first
bool smsSend(const std::string &number,const std::string &utf8Text,std::string &reason);// false + reason: not ready / invalid / busy; progress in smsSendState()
SmsSendState smsSendState();
void smsDelete(int index);void smsMarkRead(int index);void smsRefresh();
bool smsTakeIncoming(SmsMessage &m);// one call per newly arrived message (banner, ringtone, vibration)
}
