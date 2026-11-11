// Noise Floor app (section 55), part of the OS. Included by renderer.cpp.
// The layout (noise_layout.h) arrives through noise_view.h (model.h), outside namespace mm.
void Renderer::noiseApp(const State &s){
 const NoiseView &n=s.noise;const Rect P=NoisePlot;const noise::Scale &sc=n.scale;
 auto rateText=[](int i){return std::to_string(noise::rateSeconds(i))+" s";};auto windowText=[](int i){return noise::ageLabel(noise::windowSeconds(i)).substr(1);};
 shell(s,"Noise Floor",true,Icon::Menu);
 // Level card: the current value big, min / avg / max beside it.
 c.card(NoiseCard,false,8);
 c.text(NoiseCard.x+8,NoiseCard.y+4,n.hasCurrent?noise::dbmText(n.current):"--",Font::ChatTitle,FG,84);c.text(NoiseCard.x+8,NoiseCard.y+26,"dBm",Font::ChatMeta);
 {const char *names[]={"MIN","AVG","MAX"};int values[]={n.min,n.avg,n.max};
  for(int i=0;i<3;i++){int y=NoiseCard.y+2+i*13;c.text(NoiseCard.x+104,y,names[i],Font::ChatMeta);std::string v=n.valid?noise::dbmText(values[i]):"--";c.text(NoiseCard.x+NoiseCard.w-8-c.width(v,Font::ChatMeta),y,v,Font::ChatMeta);}}
 // Status row: a message (export result), why nothing is sampled, the interference warning, or the live line.
 {std::string text;bool banner=false;
  if(!n.toast.empty())text=n.toast;
  else if(n.status==NoiseOffline)text="Radio off / not receiving";else if(n.status==NoiseSuspended)text="Companion link: radio in use";else if(n.status==NoisePaused)text="Paused";
  else if(n.warn){banner=true;text="INTERFERENCE +"+std::to_string(n.excess)+" dB";}
  else if(n.status==NoiseBusy)text="Radio busy / sample skipped";
  else text=n.floorDbm<0?"Live / floor "+std::to_string(n.floorDbm)+" dBm / "+rateText(n.rate):"Live / floor not calibrated yet";
  if(banner){g.fillRoundRect(NoiseStatus.x,NoiseStatus.y,NoiseStatus.w,NoiseStatus.h,4,FG);c.centered(NoiseStatus.x,NoiseStatus.y+2,NoiseStatus.w,text,Font::Bold,BG);}
  else c.text(NoiseStatus.x+2,NoiseStatus.y+2,text,Font::ChatMeta,FG,NoiseStatus.w-4);}
 // Chart: dotted 10 dB gridlines with labels, dashed calibrated floor, min/max column spans joined by a line, a triangle per packet heard.
 c.card(NoiseChart,false,4);
 for(int d=sc.lo;d<=sc.hi;d+=sc.step){int y=P.y+noise::yPixel(sc,d*10,P.h);for(int x=P.x;x<P.x+P.w;x+=3)g.drawPixel(x,y,FG);std::string t=std::to_string(d);c.text(P.x-4-c.width(t,Font::Tiny),y-4,t,Font::Tiny);}
 if(n.floorDbm<0&&n.floorDbm>=sc.lo&&n.floorDbm<=sc.hi){int y=P.y+noise::yPixel(sc,n.floorDbm*10,P.h);for(int x=P.x;x<P.x+P.w;x+=8){g.drawFastHLine(x,y,5,FG);g.drawFastHLine(x,y+1,5,FG);}}
 {int prev=-1,prevY=0;
  for(int i=0;i<int(n.cols.size())&&i<P.w;i++){const auto &col=n.cols[i];if(!col.has)continue;
   int x=P.x+i,top=P.y+noise::yPixel(sc,col.hi,P.h),bottom=P.y+noise::yPixel(sc,col.lo,P.h),first=P.y+noise::yPixel(sc,col.first,P.h);
   g.drawFastVLine(x,top,bottom-top+1,FG);
   if(prev>=0&&i-prev<=n.stride){g.drawLine(P.x+prev,prevY,x,first,FG);g.drawLine(P.x+prev,prevY+1,x,first+1,FG);}
   prev=i;prevY=P.y+noise::yPixel(sc,col.last,P.h);}}
 for(size_t i=0;i<n.marks.size();i++){int x=noise::xForAge(n.marks[i],n.windowSecs,P.w);if(x<0)continue;int px=P.x+x,base=P.y+P.h-1;g.fillTriangle(px-3,base,px+3,base,px,base-6,FG);if(i==0)for(int y=P.y;y<base-8;y+=4)g.drawPixel(px,y,FG);}
 {std::string left=noise::ageLabel(n.windowSecs),mid=noise::ageLabel(n.windowSecs/2),right="now";
  c.text(P.x,NoisePlotLabelY,left,Font::Tiny);c.text(P.x+(P.w-c.width(mid,Font::Tiny))/2,NoisePlotLabelY,mid,Font::Tiny);c.text(P.x+P.w-c.width(right,Font::Tiny),NoisePlotLabelY,right,Font::Tiny);}
 // Last packet heard: level, SNR and age.
 {std::string age=n.packetAgeSecs?noise::ageLabel(int(n.packetAgeSecs)).substr(1)+" ago":"now";
  c.text(Margin+2,NoisePacketY,n.hasPacket?"Last "+noise::dbmText(n.packetRssi)+" dBm SNR "+noise::tenthsText(n.packetSnr)+" "+age:"No packet heard yet",Font::ChatMeta,FG,W-2*Margin-4);}
 // Controls: rate and window steppers, then RESET / EXPORT / PAUSE.
 auto stepper=[&](int focus,const std::string &label){Rect r=noiseControlRect(focus);bool on=n.focus==focus;c.card(r,on);auto ink=on?BG:FG;int y=r.y+(r.h-13)/2;
  c.text(r.x+7,y,"<",Font::Bold,ink);c.text(r.x+r.w-7-c.width(">",Font::Bold),y,">",Font::Bold,ink);c.centered(r.x+14,y,r.w-28,label,Font::Bold,ink);};
 stepper(0,"RATE "+rateText(n.rate));stepper(1,"WIN "+windowText(n.window));
 c.button(noiseControlRect(2),"RESET",n.focus==2);c.button(noiseControlRect(3),"EXPORT",n.focus==3);c.button(noiseControlRect(4),n.status==NoisePaused?"RESUME":"PAUSE",n.focus==4);
 if(!n.options)return;
 // Options sheet: rate, window and threshold steppers (< >) and the warning switch.
 g.fillRect(NoisePopup.x,NoisePopup.y,NoisePopup.w,NoisePopup.h,BG);c.card(NoisePopup);c.text(NoisePopup.x+9,NoisePopup.y+5,"Noise Floor options",Font::ChatMeta,FG,NoisePopup.w-18);
 auto item=[&](int i,const std::string &label,const std::string &detail,int on){Rect r=noiseOptionRow(i);bool focused=n.optionFocus==i;auto ink=focused?BG:FG;if(focused)g.fillRoundRect(r.x,r.y,r.w,r.h,Radius,FG);int textW=r.w-16,mid=r.y+(r.h-13)/2;
  if(on>=0){int pw=40,px=r.x+r.w-pw-6,py=r.y+(r.h-22)/2;textW-=pw+4;if(on)g.fillRoundRect(px,py,pw,22,11,ink);else g.drawRoundRect(px,py,pw,22,11,ink);c.centered(px,py+5,pw,on?"ON":"OFF",Font::Bold,on?(focused?FG:BG):ink);}
  else{textW-=60;c.text(r.x+r.w-44,mid,"<",Font::Bold,ink);c.text(r.x+r.w-20,mid,">",Font::Bold,ink);}
  c.text(r.x+6,r.y+3,label,Font::ChatBody,ink,textW);c.text(r.x+6,r.y+20,detail,Font::ChatMeta,ink,textW);};
 item(0,"Sample rate","every "+rateText(n.rate),-1);item(1,"Window","last "+windowText(n.window),-1);item(2,"Interference level","floor + "+std::to_string(n.threshold)+" dB",-1);item(3,"Interference warning",n.warnOn?"banner when above":"never",n.warnOn?1:0);
}
