#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/navigation_catalog.h"
int main(){
 using namespace mm;
 // The shared contract: Settings has the Power profile row, Power / timeouts and Battery stay reachable.
 {bool profile=false,power=false,battery=false;for(auto r:SettingsItems){if(r.section==PowerProfileSection)profile=true;if(r.section==20)power=true;if(r.section==5)battery=true;}assert(profile&&power&&battery);}
 auto inside=[](Rect r){return r.x>=Margin&&r.x+r.w<=W-Margin&&r.y>=Top&&r.y+r.h<=Bottom;};
 auto apart=[](Rect a,Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 auto disjoint=[&](const std::vector<Rect> &page){for(size_t i=0;i<page.size();i++){assert(inside(page[i]));for(size_t j=i+1;j<page.size();j++)assert(apart(page[i],page[j]));}};
 // Touch targets: inside the content area, none overlapping, labels clear of the targets.
 {std::vector<Rect> power;for(int i=0;i<PowerRows;i++)power.push_back(powerRowRect(i));for(int i=0;i<6;i++)power.push_back(timeoutOptionRect(i));disjoint(power);
  assert(PowerChipsLabelY>=powerRowRect(1).y+powerRowRect(1).h&&timeoutOptionRect(0).y>=PowerChipsLabelY+13&&PowerSleepLabelY>=timeoutOptionRect(0).y+timeoutOptionRect(0).h&&powerRowRect(2).y>=PowerSleepLabelY+13&&PowerLightsLabelY+13<=powerRowRect(0).y);
  disjoint({ChatClearYes,ChatClearNo});}
 {std::vector<Rect> battery={BatteryHeader,BatteryGraph};for(int i=0;i<BatteryTabCount;i++)battery.push_back(batteryTabRect(i));for(int i=0;i<BatteryStatCount;i++)battery.push_back(batteryStatRect(i));disjoint(battery);
  std::vector<Rect> usage;for(int i=0;i<BatteryUsageRowCount;i++)usage.push_back(batteryUsageRect(i));disjoint(usage);
  assert(BatteryUsageLabelY>=batteryTabRect(0).y+batteryTabRect(0).h&&batteryUsageRect(0).y>=BatteryUsageLabelY+13&&BatteryTotalY>=batteryUsageRect(BatteryUsageRowCount-1).y+batteryUsageRect(BatteryUsageRowCount-1).h&&BatteryTotalY+14+13<=Bottom);
  assert(BatteryColumns*4+24+8<=BatteryGraph.w);}
 {std::vector<Rect> profile={ProfileList,ProfileApply};for(int i=0;i<ProfileCount;i++)profile.push_back(profileCardRect(i));disjoint(profile);
  assert(ProfileLabelY>=profileCardRect(ProfileCount-1).y+profileCardRect(ProfileCount-1).h&&ProfileList.y>=ProfileLabelY+13&&ProfileList.h>=device::PROFILE_ROWS*13+4);}
 GFXcanvas1 canvas(W,H);Renderer r(canvas);
 auto shot=[&](const State &s){r.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+(W+7)/8*H);};
 auto differs=[&](const State &a,const State &b){return shot(a)!=shot(b);};
 // ---- Power / timeouts (20)
 State p;p.page=Page::Power;auto base=shot(p);
 {State t=p;t.powerField=2;assert(differs(p,t));State u=p;u.powerField=5;assert(differs(t,u));}
 {State t=p;t.pw.sleepOn=false;assert(differs(p,t));State u=p;u.pw.stayUsb=false;assert(differs(p,u));State d=p;d.pw.sleepDelayS=300;assert(differs(p,d));}
 {State t=p;t.pw.sleepPercent=42;assert(differs(p,t));State u=p;u.pw.sleepHint="Wi-Fi, Bluetooth";assert(differs(p,u));}
 {State t=p;t.pw.field=1;assert(differs(p,t));State u=p;u.displayTimeout=300;assert(differs(p,u));}
 {State t=p;t.pw.confirm=true;t.pw.confirmFocus=1;assert(differs(p,t));State u=t;u.pw.confirmFocus=0;assert(differs(t,u));}
 // ---- Battery (5)
 State b;b.page=Page::Battery;b.status.battery=87;b.voltage=4012;auto graphTab=shot(b);
 {State t=b;t.pw.tab=1;assert(differs(b,t));State u=b;u.pw.timeValue="10 h";assert(differs(b,u));State v=b;v.pw.avgValue="-42 mA";assert(differs(b,v));State w=b;w.pw.nowValue="-38 mA";assert(differs(b,w));}
 {State t=b;t.pw.graph.assign(BatteryColumns,device::GraphColumn());for(int i=10;i<30;i++){t.pw.graph[i].soc=60+i;}assert(differs(b,t));State c=t;for(int i=15;i<20;i++)c.pw.graph[i].charging=true;assert(differs(t,c));State s=t;s.pw.stored=true;assert(differs(t,s));}
 {State t=b;t.batteryCharging=true;assert(differs(b,t));State u=b;u.status.battery=40;assert(differs(b,u));State v=b;v.pw.timeLabel="TIME TO FULL";assert(differs(b,v));}
 {State u=b;u.pw.tab=1;for(int i=0;i<BatteryUsageRowCount;i++){UsageRow row;row.name="Subsystem "+std::to_string(i);row.detail="1 h  12.0 mAh";row.percent=i*15;u.pw.usage.push_back(row);}u.pw.usageModel="Model avg 19 mA";u.pw.usageGauge="Gauge now -30 mA";
  State e=b;e.pw.tab=1;assert(differs(e,u));State f=u;f.pw.usage[0].percent=100;assert(differs(u,f));State g=u;g.pw.usageGauge="Gauge not readable";assert(differs(u,g));}
 assert(graphTab!=base);
 // ---- Power profile (57)
 State f;f.page=Page::PowerProfile;f.pw.rows=device::profileRows(device::profilePreset(device::Profile::Normal));f.pw.active=1;f.pw.preview=1;f.pw.focus=1;auto normal=shot(f);
 {State t=f;t.pw.preview=0;t.pw.rows=device::profileRows(device::profilePreset(device::Profile::Field));t.pw.focus=0;t.pw.applicable=true;assert(differs(f,t));
  State u=t;u.pw.focus=mm::ProfileApplyFocus;assert(differs(t,u));State v=u;v.pw.applicable=false;assert(differs(u,v));}
 {State t=f;t.pw.active=3;assert(differs(f,t));State u=f;u.pw.preview=3;assert(differs(f,u));}
 {State t=f;t.notice="Field profile applied";assert(differs(f,t));State u=f;u.pw.rows[0].value="Off";assert(differs(f,u));}
 assert(normal!=base);
 // ---- Settings list: the Power profile and Power rows tell their state
 {State l;l.page=Page::Settings;int row=0;for(int i=0;i<SettingsCount;i++)if(SettingsItems[i].section==PowerProfileSection)row=i;l.settingsFocus=row;l.settingsOffset=std::max(0,std::min(row,settingsMaxScroll()));State pr=l;pr.pw.profile="Custom";assert(differs(l,pr));
  int prow=0;for(int i=0;i<SettingsCount;i++)if(SettingsItems[i].section==20)prow=i;State m=l;m.settingsFocus=prow;m.settingsOffset=std::max(0,std::min(prow,settingsMaxScroll()));State off=m;off.pw.sleepOn=false;assert(differs(m,off));State delay=m;delay.pw.sleepDelayS=300;assert(differs(m,delay));}
 std::cout<<"PASS: power UI: Power / timeouts, Battery (graph, usage) and Power profile touch targets inside the content area and apart, every page redraws with its state, Settings rows show it\n";
}
