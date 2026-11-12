// Power pages, part of the OS: Settings > Power / timeouts (section 20), Battery (5) and Power profile (57). Included by renderer.cpp.
void Renderer::powerPages(const State &s){
 const PowerView &v=s.pw;
 shell(s,s.page==Page::Battery?"Battery":s.page==Page::PowerProfile?"Power profile":"Power / timeouts");g.fillRect(W-32,StatusH,32,HeaderH-1,BG);
 // A rounded pill (ON / OFF) in the ink of its card; on = filled.
 auto pill=[&](Rect r,const std::string &text,bool on,uint16_t ink){uint16_t paper=ink==FG?BG:FG;if(on)g.fillRoundRect(r.x,r.y,r.w,r.h,r.h/2,ink);else g.drawRoundRect(r.x,r.y,r.w,r.h,r.h/2,ink);c.centered(r.x,r.y+(r.h-13)/2,r.w,text,Font::Bold,on?paper:ink);};
 auto upper=[](std::string t){for(auto &ch:t)ch=char(toupper(static_cast<unsigned char>(ch)));return t;};
 auto valueCard=[&](Rect r,const std::string &label,const std::string &value){c.card(r);c.text(r.x+7,r.y+4,label,Font::ChatMeta,FG,r.w-14);c.text(r.x+7,r.y+20,value,Font::Bold,FG,r.w-14);};
 if(s.page==Page::Power){
  c.sectionLabel(PowerLightsLabelY,"LIGHT TIMERS","LOCKS AFTER 60 S");
  const char *names[]={"Display / front-light","Keyboard backlight"};
  for(int i=0;i<2;i++){Rect r=powerRowRect(i);bool focus=s.powerField==i;c.card(r,focus);auto ink=focus?BG:FG;if(i)c.icon(Icon::Keyboard,r.x+6,r.y+(r.h-16)/2,16,ink);else{g.drawRect(r.x+7,r.y+9,16,10,ink);g.drawFastHLine(r.x+12,r.y+22,6,ink);}
   c.text(r.x+30,r.y+2,names[i],Font::ChatBody,ink,r.w-50);uint32_t seconds=i?s.keyboardTimeout:s.displayTimeout;std::string value=seconds==0?"Never":seconds>=60?std::to_string(seconds/60)+" minute"+(seconds>=120?"s":""):std::to_string(seconds)+" seconds";
   c.text(r.x+30,r.y+17,value+(seconds==30?" / default":""),Font::ChatMeta,ink,r.w-50);c.icon(Icon::ChevronRight,r.x+r.w-16,r.y+(r.h-12)/2,12,ink);}
  c.sectionLabel(PowerChipsLabelY,v.field?"KEYBOARD TIMEOUT":"DISPLAY TIMEOUT");const char *labels[]={"15s","30s","1m","2m","5m","Never"};const uint32_t values[]={15,30,60,120,300,0};uint32_t selected=v.field?s.keyboardTimeout:s.displayTimeout;
  for(int i=0;i<6;i++){Rect r=timeoutOptionRect(i);c.card(r,selected==values[i]);c.centered(r.x,r.y+(r.h-c.lineHeight(Font::Small))/2,r.w,labels[i],Font::Small,selected==values[i]?BG:FG);}
  c.sectionLabel(PowerSleepLabelY,"SLEEP","ASLEEP "+std::to_string(v.sleepPercent)+"% LAST HOUR");
  const char *titles[]={"Sleep when idle","Sleep delay","Stay awake on USB","Deep sleep now"};
  std::string hints[]={v.sleepHint.empty()?"Naps; the mesh stays on":"Awake: "+v.sleepHint,"After screen off","No sleep on USB power","Mesh off until a key press"};
  for(int k=0;k<4;k++){int i=k+2;Rect r=powerRowRect(i);bool focus=s.powerField==i;c.card(r,focus);auto ink=focus?BG:FG;
   const bool toggle=k==0||k==2;int room=r.w-(toggle?66:k==1?96:30);
   c.text(r.x+8,r.y+2,titles[k],Font::Bold,ink,room);c.text(r.x+8,r.y+14,hints[k],Font::ChatMeta,ink,room);
   if(toggle)pill({r.x+r.w-50,r.y+4,44,20},(k==0?v.sleepOn:v.stayUsb)?"ON":"OFF",k==0?v.sleepOn:v.stayUsb,ink);
   else if(k==1){std::string value=device::timeoutText(v.sleepDelayS);c.text(r.x+r.w-56-c.width(value,Font::Bold),r.y+(r.h-13)/2,value,Font::Bold,ink);c.icon(Icon::Back,r.x+r.w-44,r.y+(r.h-12)/2,12,ink);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12,ink);}
   else c.icon(Icon::ChevronRight,r.x+r.w-16,r.y+(r.h-12)/2,12,ink);}
  if(v.confirm){Rect panel=overlayPanel();
   c.text(Margin+8,Top+15,"Deep sleep?",Font::ChatTitle,FG,panel.w-16);
   const char *lines[]={"The mesh radio and GPS stop.","No message arrives until you","press a key; the device then","restarts. Use it to store it","for weeks. Unsaved drafts are lost."};
   for(int i=0;i<5;i++)c.text(Margin+8,Top+50+i*16,lines[i],Font::ChatMeta,FG,panel.w-16);
   c.button(ChatClearYes,"DEEP SLEEP",v.confirmFocus==0);c.button(ChatClearNo,"CANCEL",v.confirmFocus==1);}
  return;}
 if(s.page==Page::Battery){
  for(int i=0;i<BatteryTabCount;i++)c.button(batteryTabRect(i),i?"USAGE":"GRAPH",v.tab==i);
  if(v.tab==0){
   {Rect r=BatteryHeader;c.card(r);c.icon(Icon::Battery,r.x+8,r.y+10,24);
    c.text(r.x+42,r.y+2,s.status.battery<0?"--%":std::to_string(s.status.battery)+"%",Font::Title,FG,70);
    c.text(r.x+42,r.y+25,s.batteryFull?"Full":s.batteryCharging||s.status.charging?"Charging":"On battery",Font::ChatMeta,FG,100);
    std::string now="NOW "+v.nowValue;c.text(r.x+r.w-8-c.width(now,Font::Bold),r.y+(r.h-13)/2,now,Font::Bold,FG,100);}
   {Rect r=BatteryGraph;c.card(r);const int px=r.x+24,py=r.y+6,pw=BatteryColumns*4,ph=r.h-28;
    c.text(r.x+4,py-5,"100",Font::ChatMeta);c.text(r.x+4+c.width("100",Font::ChatMeta)-c.width("50",Font::ChatMeta),py+ph/2-6,"50",Font::ChatMeta);c.text(r.x+4+c.width("100",Font::ChatMeta)-c.width("0",Font::ChatMeta),py+ph-9,"0",Font::ChatMeta);
    for(int x=px;x<px+pw;x+=4)g.drawPixel(x,py+ph/2,FG);g.drawFastHLine(px,py+ph,pw,FG);
    for(int i=0;i<BatteryColumns&&i<int(v.graph.size());i++){auto &col=v.graph[i];if(col.soc<0)continue;int h=std::max(1,std::min(100,col.soc)*ph/100),x=px+i*4;g.fillRect(x,py+ph-h,3,h,FG);if(col.charging)for(int yy=py+ph-h+1;yy<py+ph;yy+=2)g.drawFastHLine(x,yy,3,BG);}
    int ly=r.y+r.h-17;c.text(px,ly,"24 h ago",Font::ChatMeta);std::string right="now",mid=v.stored?"saved on SD":"RAM only";c.text(px+pw-c.width(right,Font::ChatMeta),ly,right,Font::ChatMeta);c.text(px+(pw-c.width(mid,Font::ChatMeta))/2,ly,mid,Font::ChatMeta);}
   valueCard(batteryStatRect(0),v.timeLabel,v.timeValue);valueCard(batteryStatRect(1),"AVG CURRENT",v.avgValue);
   valueCard(batteryStatRect(2),"VOLTAGE",s.voltage<0?"--":std::to_string(s.voltage)+" mV");valueCard(batteryStatRect(3),"SOURCE",v.source);
   return;}
  c.sectionLabel(BatteryUsageLabelY,"LAST 24 H","NOMINAL CURRENTS");
  for(int i=0;i<BatteryUsageRowCount&&i<int(v.usage.size());i++){Rect r=batteryUsageRect(i);auto &u=v.usage[i];int dw=c.width(u.detail,Font::ChatMeta);
   c.text(r.x+2,r.y,u.name,Font::ChatMeta,FG,r.w-dw-12);c.text(r.x+r.w-2-dw,r.y,u.detail,Font::ChatMeta);
   g.drawRoundRect(r.x+2,r.y+16,r.w-4,7,3,FG);int fill=(r.w-8)*std::max(0,std::min(100,u.percent))/100;if(fill>0)g.fillRoundRect(r.x+4,r.y+18,std::max(3,fill),3,1,FG);}
  c.text(Margin+2,BatteryTotalY,v.usageModel,Font::ChatMeta,FG,W-2*Margin-4);c.text(Margin+2,BatteryTotalY+14,v.usageGauge,Font::ChatMeta,FG,W-2*Margin-4);
  return;}
 // Power profile: the four profiles, what the chosen one sets (the rows of power_profile.h), APPLY.
 for(int i=0;i<ProfileCount;i++){Rect r=profileCardRect(i);bool active=v.active==i,shown=v.preview==i;uint16_t ink=active?BG:FG;c.card(r,active);
  c.text(r.x+8,r.y+3,device::profileName(device::Profile(i)),Font::Bold,ink,r.w-16);c.text(r.x+8,r.y+18,active?"Active":device::profileTagline(device::Profile(i)),Font::ChatMeta,ink,r.w-16);
  if(shown)g.drawRoundRect(r.x+2,r.y+2,r.w-4,r.h-4,Radius-1,ink);}
 c.sectionLabel(ProfileLabelY,!s.notice.empty()?std::string(s.notice):v.preview==int(device::Profile::Custom)?"YOUR CURRENT SETTINGS":upper(device::profileName(device::Profile(v.preview)))+" SETS");// a notice takes the label's place
 {Rect r=ProfileList;c.card(r);for(int i=0;i<int(v.rows.size())&&i<device::PROFILE_ROWS;i++){int y=r.y+4+i*13;c.text(r.x+8,y,v.rows[i].label,Font::ChatMeta,FG,r.w-110);c.text(r.x+r.w-8-c.width(v.rows[i].value,Font::ChatMeta),y,v.rows[i].value,Font::ChatMeta,FG,100);}}
 {const bool custom=v.preview==int(device::Profile::Custom);std::string label=v.preview==v.active?"ACTIVE":custom?"CHANGE A SETTING FOR CUSTOM":"APPLY "+upper(device::profileName(device::Profile(v.preview)));c.button(ProfileApply,label,v.focus==ProfileApplyFocus&&v.applicable);if(v.focus==ProfileApplyFocus&&!v.applicable)g.drawRoundRect(ProfileApply.x+2,ProfileApply.y+2,ProfileApply.w-4,ProfileApply.h-4,Radius-1,FG);}
}
