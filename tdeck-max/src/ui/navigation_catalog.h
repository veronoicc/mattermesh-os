#pragma once
#include "radio_link_layout.h"
#include "components.h"
namespace mm {
struct NavItem {const char *label;Icon icon;int section;};
struct AppItem {const char *label;PackIcon icon;int section;};
inline constexpr AppItem HomeTabs[]={{"Chat",PackIcon::Chat,0},{"Contacts",PackIcon::Contacts,1},{"Settings",PackIcon::Settings,19},{"App",PackIcon::Apps,-1}};
static_assert(sizeof(HomeTabs)/sizeof(HomeTabs[0])==MenuHomeTabCount,"Home catalog/layout must agree");
// Donate QR: donations of any amount.
constexpr const char *DonateUrl="https://ko-fi.com/mattermeshos";
// Source builds carry every app: the OS apps, then the field tools (RPT Scanner, RX Log, Noise Floor), Tracks and SMS.
inline constexpr AppItem Applications[]={{"Maps",PackIcon::Map,9},{"Weather",PackIcon::Weather,10},{"Files",PackIcon::Files,7},{"CLI",PackIcon::Keyboard,30},{"RPT Scanner",PackIcon::Radio,RptScannerSection},{"RX Log",PackIcon::List,RxLogSection},{"Noise Floor",PackIcon::Signal,NoiseFloorSection},{"Tracks",PackIcon::Map,TracksAppSection},{"SMS",PackIcon::Chat,SmsSection}};
constexpr uint32_t AppKeepMs=600000;// an open app holds off the lock this long after the last input
constexpr int ApplicationCount=sizeof(Applications)/sizeof(Applications[0]);
// Grouped as theme.h SettingsGroupFirst: radio & mesh, connections, display, keyboard & sound, device, system.
inline constexpr NavItem SettingsItems[]={{"LoRa",Icon::Radio,2},{"Antenna",Icon::Signal,23},{"Companion link",Icon::Radio,RadioLinkSection},{"Wi-Fi",Icon::Wifi,4},{"Bluetooth",Icon::Bluetooth,14},{"GPS",Icon::GPS,3},{"GPS tracks",Icon::GPS,TracksSection},{"Cellular",Icon::Signal,CellularSection},{"MatterMesh Web",Icon::Desktop,WebAppSection},{"Wallpaper",Icon::Mountain,22},{"Home widgets",Icon::Weather,HomeWidgetsSection},{"Lights / vibration",Icon::Vibration,6},{"Power / timeouts",Icon::Power,20},{"Keyboard languages",Icon::Keyboard,KeyboardSection},{"Accent key",Icon::Keyboard,AccentKeySection},{"Sound / dictation",Icon::Messages,21},{"Battery",Icon::Battery,5},{"Motion / compass",Icon::Vibration,MotionSection},{"Date / time",Icon::Clock,16},{"Storage",Icon::Files,24},{"OTA update",Icon::Tools,17},{"Diagnostics",Icon::Terminal,11},{"Donate",Icon::Lock,SupportSection},{"About",Icon::Info,8}};
static_assert(sizeof(SettingsItems)/sizeof(SettingsItems[0])==SettingsCount,"Settings catalog/layout must agree");
}
