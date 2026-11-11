#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/noise_layout.h"
int main(){
 auto inside=[](mm::Rect r){return r.x>=mm::Margin&&r.x+r.w<=mm::W-mm::Margin&&r.y>=mm::Top&&r.y+r.h<=mm::Bottom;};
 auto apart=[](mm::Rect a,mm::Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 auto within=[](mm::Rect a,mm::Rect b){return a.x>=b.x&&a.y>=b.y&&a.x+a.w<=b.x+b.w&&a.y+a.h<=b.y+b.h;};
 // Page targets: step rows and buttons; the card, status row and chart (display only) do not overlap them either.
 std::vector<mm::Rect> page;for(int i=0;i<mm::NoiseControlCount;i++)page.push_back(mm::noiseControlRect(i));
 for(size_t i=0;i<page.size();i++){assert(inside(page[i]));for(size_t j=i+1;j<page.size();j++)assert(apart(page[i],page[j]));}
 for(auto r:{mm::NoiseCard,mm::NoiseStatus,mm::NoiseChart}){assert(inside(r));for(auto t:page)assert(apart(r,t));}
 assert(apart(mm::NoiseCard,mm::NoiseStatus)&&apart(mm::NoiseStatus,mm::NoiseChart));
 assert(within(mm::NoisePlot,mm::NoiseChart)&&mm::NoisePlotLabelY+9<=mm::NoiseChart.y+mm::NoiseChart.h&&mm::NoisePlotLabelY>=mm::NoisePlot.y+mm::NoisePlot.h);
 assert(mm::NoisePacketY>=mm::NoiseChart.y+mm::NoiseChart.h&&mm::NoisePacketY+13<=mm::NoiseRate.y);
 assert(mm::noiseStepTap(mm::NoiseRate,mm::NoiseRate.x+2)==-1&&mm::noiseStepTap(mm::NoiseRate,mm::NoiseRate.x+mm::NoiseRate.w-2)==1);
 // Options sheet: inside the content area, rows apart and inside the panel.
 assert(inside(mm::NoisePopup));
 for(int i=0;i<mm::NoiseOptionRows;i++){auto r=mm::noiseOptionRow(i);assert(inside(r)&&within(r,mm::NoisePopup));for(int j=i+1;j<mm::NoiseOptionRows;j++)assert(apart(r,mm::noiseOptionRow(j)));}
 GFXcanvas1 canvas(mm::W,mm::H);mm::Renderer renderer(canvas);mm::State s;s.page=mm::Page::NoiseFloor;
 auto shot=[&](){renderer.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+(mm::W*mm::H+7)/8);};
 // Empty state renders, every status too.
 auto empty=shot();
 for(int status:{mm::NoiseLive,mm::NoisePaused,mm::NoiseOffline,mm::NoiseSuspended,mm::NoiseBusy}){s.noise.status=status;shot();}
 s.noise.status=mm::NoiseLive;
 // Full data: a noisy chart with spikes, floor, packet markers, warning, toast.
 noise::Ring ring;ring.reset(600);for(int i=0;i<600;i++)ring.push({int16_t(i%97==0?-700:i%31==0?noise::Gap:-1100+(i*7)%90),-110});
 auto &n=s.noise;n.cols=noise::downsample(ring,mm::NoisePlot.w);auto st=noise::stats(ring);n.valid=st.valid;n.hasCurrent=st.hasCurrent;n.current=st.current;n.min=st.min;n.max=st.max;n.avg=int(st.avg);
 n.floorDbm=-110;n.scale=noise::yScale(true,st.min,st.max,-110);n.window=1;n.windowSecs=600;n.rate=0;n.rateSecs=1;n.stride=noise::linkStride(600,mm::NoisePlot.w);
 n.hasPacket=true;n.packetRssi=-923;n.packetSnr=55;n.packetAgeSecs=12;n.marks={12000,90000,500000};
 auto data=shot();assert(data!=empty);
 n.warn=true;n.excess=23;auto warned=shot();assert(warned!=data);
 n.warn=false;n.toast="Saved noise-1.csv";auto toasted=shot();assert(toasted!=data);n.toast="";
 // The canvas follows the state: current value, floor line, paused, packet markers, focus, rate and window labels.
 auto base=shot();assert(base==data);
 n.current=-800;assert(shot()!=base);n.current=st.current;
 n.floorDbm=0;assert(shot()!=base);n.floorDbm=-110;
 n.status=mm::NoisePaused;assert(shot()!=base);n.status=mm::NoiseLive;
 n.marks.clear();assert(shot()!=base);n.marks={12000,90000,500000};
 for(int focus=1;focus<mm::NoiseControlCount;focus++){n.focus=focus;assert(shot()!=base);}n.focus=0;
 n.rate=2;assert(shot()!=base);n.rate=0;n.window=2;n.windowSecs=3600;assert(shot()!=base);n.window=1;n.windowSecs=600;
 n.hasPacket=false;assert(shot()!=base);n.hasPacket=true;
 // Options sheet: every focus row, both switch states.
 n.options=true;auto opt=shot();assert(opt!=base);
 for(int f=0;f<mm::NoiseOptionRows;f++){n.optionFocus=f;assert(shot()!=opt||f==0);}
 n.optionFocus=0;n.warnOn=false;assert(shot()!=opt);n.warnOn=true;n.threshold=20;assert(shot()!=opt);n.options=false;
 // Extremes: an all-gap buffer, a single sample, the full 1 h chart width.
 n.cols=std::vector<noise::Column>(mm::NoisePlot.w);n.valid=0;n.hasCurrent=false;shot();
 ring.reset(3600);for(int i=0;i<3600;i++)ring.push({int16_t(-1000-(i*13)%200),-105});n.cols=noise::downsample(ring,mm::NoisePlot.w);n.valid=3600;n.hasCurrent=true;shot();
 std::cout<<"PASS: noise floor UI (targets inside and apart, options sheet, every status/focus/option renders, canvas follows the chart state)\n";
}
