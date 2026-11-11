// Noise Floor app (section 55): samples the radio's instantaneous RSSI into a ring and charts it live. Included by main.cpp.
#include "services/noise_stats.h"
#include "ui/noise_layout.h"
static noise::Ring noiseRing;static noise::Detector noiseDetector;// freed while the app is closed
static int noiseRate=1,noiseWindow=0,noiseThreshold=noise::ThresholdDefault,noiseFocus=0,noiseOptFocus=0,noiseStatus=mm::NoiseLive,noiseFloorDbm=0;// persisted (NVS "mmnoise"): rate, window, threshold, warn
static bool noiseWarnOn=true,noisePaused=false,noiseOptions=false,noiseOpened=false;
static uint32_t noiseNext=0,noisePaintedAt=0,noisePausedAt=0,noiseToastUntil=0;static std::string noiseToast;
static void noiseSave(){device::prefPutInt("mmnoise","rate",noiseRate);device::prefPutInt("mmnoise","win",noiseWindow);device::prefPutInt("mmnoise","thr",noiseThreshold);device::prefPutInt("mmnoise","warn",noiseWarnOn?1:0);}
static void noiseSay(const std::string &text){noiseToast=text;noiseToastUntil=millis()+3000;}
static void noiseOpen(){
 noiseRate=noise::stepIndex(device::prefGetInt("mmnoise","rate",1),0);noiseWindow=noise::stepIndex(device::prefGetInt("mmnoise","win",0),0);noiseThreshold=noise::clampThreshold(device::prefGetInt("mmnoise","thr",noise::ThresholdDefault));noiseWarnOn=device::prefGetInt("mmnoise","warn",1)!=0;
 noiseRing.reset(noise::capacityFor(noiseRate,noiseWindow));noiseDetector=noise::Detector();noisePaused=noiseOptions=false;noiseFocus=noiseOptFocus=0;noiseStatus=mm::NoiseLive;noiseFloorDbm=0;noiseToast.clear();noiseToastUntil=0;noiseNext=millis();noisePaintedAt=0;noiseOpened=true;
}
// Leaving the app (Back, Home, lock...): sampling stops and the buffer goes.
static void noiseClose(){noiseOpened=noisePaused=noiseOptions=false;noiseRing=noise::Ring();noiseToast.clear();}
// One tick of the chart: a sample, or a gap when the radio is asleep, off, sending or busy with a packet.
static void noiseSample(){
 float dbm=0;int floorDbm=0;auto got=matter::radioSampleRssi(dbm,floorDbm);noiseFloorDbm=floorDbm;
 noise::Sample s;s.floor=int8_t(std::max(-128,std::min(0,floorDbm)));if(got==matter::RssiSample::Ok&&std::isfinite(dbm))s.rssi=int16_t(std::max(-2000.0f,std::min(0.0f,std::round(dbm*10))));
 noiseRing.push(s);noiseDetector.update(s.rssi,floorDbm,noiseThreshold);
 noiseStatus=got==matter::RssiSample::Ok?mm::NoiseLive:got==matter::RssiSample::Offline?mm::NoiseOffline:got==matter::RssiSample::Suspended?mm::NoiseSuspended:mm::NoiseBusy;
}
// Rate or window index changed: a new rate starts an empty chart (the time scale differs), a new window keeps the newest samples.
static void noiseSetIndex(int which,int index){
 index=noise::stepIndex(index,0);
 if(which==0&&index!=noiseRate){noiseRate=index;noiseRing.reset(noise::capacityFor(noiseRate,noiseWindow));noiseDetector=noise::Detector();noiseNext=millis();}
 else if(which==1&&index!=noiseWindow){noiseWindow=index;noiseRing.resize(noise::capacityFor(noiseRate,noiseWindow));}
 noiseSave();
}
static void noiseStep(int which,int dir){if(which==2){noiseThreshold=noise::stepThreshold(noiseThreshold,dir);noiseSave();}else noiseSetIndex(which,(which==0?noiseRate:noiseWindow)+dir);}
static void noiseExport(){
 if(!sdReady){noiseSay("SD card not available");return;}
 size_t n=noiseRing.size();if(!n){noiseSay("Nothing to export yet");return;}
 time_t now=time(nullptr);bool clock=now>=MM_EPOCH_MIN;uint32_t rate=noise::rateSeconds(noiseRate);
 String path=String("/noise-")+(clock?String((unsigned long)now):String("up")+String((unsigned long)(millis()/1000)))+".csv";
 File f=SD.open(path,"w");if(!f){noiseSay("Export failed: cannot open file");return;}
 bool ok=true;std::string chunk=noise::csvHeader();
 auto flush=[&](){if(!chunk.empty())ok=ok&&f.write((const uint8_t*)chunk.data(),chunk.size())==chunk.size();chunk.clear();};
 for(size_t i=0;i<n;i++){const auto &s=noiseRing.at(i);if(s.rssi==noise::Gap)continue;uint32_t ago=uint32_t(n-1-i)*rate;
  chunk+=noise::csvLine(clock?uint32_t(now)-ago:0,ago,s.rssi,s.floor,s.floor<0&&s.rssi>s.floor*10+noiseThreshold*10);if(chunk.size()>=1024)flush();}
 flush();f.flush();f.close();
 if(!ok){SD.remove(path);noiseSay("Export failed: SD write error");return;}
 noiseSay(std::string("Saved ")+(path.c_str()+18));
}
static void noiseButton(int control){
 if(control==2){noiseRing.clear();noiseDetector=noise::Detector();noiseSay("Chart cleared");}
 else if(control==3)noiseExport();
 else{noisePaused=!noisePaused;if(noisePaused){noisePausedAt=millis();noiseStatus=mm::NoisePaused;}else{noiseStatus=mm::NoiseLive;noiseNext=millis();}}
}
static bool noiseKey(char c){
 if(c=='\b')return false;
 bool left=c=='a'||c=='h',right=c=='d'||c=='l',up=c=='w'||c=='k',down=c=='s'||c=='j',act=c=='\n'||c==' ';
 if(noiseOptions){
  if(c=='o')noiseOptions=false;
  else if(up||down)noiseOptFocus=(noiseOptFocus+(up?mm::NoiseOptionRows-1:1))%mm::NoiseOptionRows;
  else if(noiseOptFocus==3&&(left||right||act)){noiseWarnOn=!noiseWarnOn;noiseSave();}
  else if(left||right)noiseStep(noiseOptFocus,left?-1:1);
  else if(act){if(noiseOptFocus==2)noiseStep(2,1);else noiseSetIndex(noiseOptFocus,((noiseOptFocus==0?noiseRate:noiseWindow)+1)%noise::RateCount);}
  render();return true;
 }
 if(c=='o')noiseOptions=true,noiseOptFocus=0;
 else if(up||down)noiseFocus=(noiseFocus+(up?mm::NoiseControlCount-1:1))%mm::NoiseControlCount;
 else if(c==' ')noiseButton(4);
 else if(left&&noiseFocus<2)noiseStep(noiseFocus,-1);else if(right&&noiseFocus<2)noiseStep(noiseFocus,1);
 else if(left&&noiseFocus>2)noiseFocus--;else if(right&&noiseFocus>=2){if(noiseFocus<4)noiseFocus++;}
 else if(act){if(noiseFocus<2)noiseSetIndex(noiseFocus,((noiseFocus==0?noiseRate:noiseWindow)+1)%noise::RateCount);else noiseButton(noiseFocus);}
 else return false;// A on RESET (and any other key) goes back to the main handler
 render();return true;
}
static bool noiseTap(int x,int y){
 if(mm::BackButton.contains(x,y))return false;
 if(mm::ComposeButton.contains(x,y)){noiseOptions=!noiseOptions;noiseOptFocus=0;render();return true;}
 if(noiseOptions){
  for(int i=0;i<mm::NoiseOptionRows;i++){mm::Rect r=mm::noiseOptionRow(i);if(!r.contains(x,y))continue;noiseOptFocus=i;if(i==3){noiseWarnOn=!noiseWarnOn;noiseSave();}else noiseStep(i,mm::stepRowTap(r,x));render();return true;}
  if(!mm::NoisePopup.contains(x,y))noiseOptions=false;
  render();return true;
 }
 for(int i=0;i<mm::NoiseControlCount;i++){mm::Rect r=mm::noiseControlRect(i);if(!r.contains(x,y))continue;noiseFocus=i;if(i<2)noiseStep(i,mm::noiseStepTap(r,x));else noiseButton(i);render();return true;}
 return false;
}
static bool noiseBack(){if(!noiseOptions)return false;noiseOptions=false;render();return true;}
static void noiseScroll(int dy){}
// Called every main loop pass: only an open app samples. Ticks lost while the display refreshes are not back-filled (the axis is nominal).
static void noiseTick(){
 if(!noiseOpened||screen!=Screen::Section||section!=mm::NoiseFloorSection)return;
 uint32_t now=millis();bool changed=false;
 if(noiseToastUntil&&int32_t(now-noiseToastUntil)>=0){noiseToastUntil=0;noiseToast.clear();changed=true;}
 if(!noisePaused&&int32_t(now-noiseNext)>=0){int before=noiseStatus;noiseNext=now+uint32_t(noise::rateSeconds(noiseRate))*1000;noiseSample();changed=changed||noiseStatus==mm::NoiseLive||noiseStatus!=before||noiseDetector.consecutive>0;}
 // E-paper cadence: a repaint at most every 2 s at 1 s sampling, 1.5 s otherwise, none under the options sheet.
 if(changed&&!noiseOptions&&now-noisePaintedAt>=(noiseRate==0?2000u:1500u)){noisePaintedAt=now;markRadioDirty();}
}
static void noiseCompose(mm::State &v){
 auto &n=v.noise;n.rate=noiseRate;n.window=noiseWindow;n.threshold=noiseThreshold;n.warnOn=noiseWarnOn;n.options=noiseOptions;n.focus=noiseFocus;n.optionFocus=noiseOptFocus;
 n.rateSecs=noise::rateSeconds(noiseRate);n.windowSecs=noise::windowSeconds(noiseWindow);n.status=noiseStatus;n.floorDbm=noiseFloorDbm;n.stride=noise::linkStride(noiseRing.capacity(),mm::NoisePlot.w);
 auto st=noise::stats(noiseRing);n.valid=st.valid;n.hasCurrent=st.hasCurrent;n.current=st.current;n.min=st.min;n.max=st.max;n.avg=int(std::lround(st.avg));
 n.scale=noise::yScale(st.valid>0,st.min,st.max,noiseFloorDbm);n.cols=noise::downsample(noiseRing,mm::NoisePlot.w);
 n.warn=noiseWarnOn&&!noisePaused&&noiseDetector.consecutive>=2;n.excess=noiseDetector.excess;
 n.toast=noiseToastUntil&&int32_t(noiseToastUntil-millis())>0?noiseToast:std::string();
 // Packets heard inside the window (newest first) mark the chart; the newest one is also written out, whatever its age.
 uint32_t now=noisePaused?noisePausedAt:millis(),limit=uint32_t(n.windowSecs)*1000;auto &log=matter::rxLog();n.hasPacket=false;n.marks.clear();
 for(int i=0;i<log.count()&&n.marks.size()<48;i++){auto p=log.newest(i);uint32_t age=int32_t(now-p->uptimeMs)<0?0:now-p->uptimeMs;
  if(!i){n.hasPacket=true;n.packetRssi=p->rssiTenths;n.packetSnr=p->snrTenths;n.packetAgeSecs=age/1000;}
  if(age>limit)break;n.marks.push_back(age);}
}
