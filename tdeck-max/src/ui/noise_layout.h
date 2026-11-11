#pragma once
#include "theme.h"
// Noise Floor app (section 55): card with the level, status / warning row, chart, last packet line, two step rows, three buttons.
namespace mm {
constexpr Rect NoiseCard={Margin,Top+3,W-2*Margin,42},NoiseStatus={Margin,Top+47,W-2*Margin,17},NoiseChart={Margin,Top+66,W-2*Margin,124};
constexpr Rect NoisePlot={Margin+32,Top+70,W-2*Margin-38,100};// y labels left of it, the x labels (Tiny) below it inside the chart card
constexpr int NoisePlotLabelY=Top+174,NoisePacketY=Top+193,NoiseControlCount=5;
constexpr Rect NoiseRate={Margin,Top+208,(W-2*Margin-6)/2,24},NoiseWindow={W/2+3,Top+208,(W-2*Margin-6)/2,24};
// Focus order: 0 rate, 1 window, 2 RESET, 3 EXPORT, 4 PAUSE.
constexpr Rect NoiseButton(int i){return {Margin+(i-2)*((W-2*Margin-12)/3+6),Top+234,(W-2*Margin-12)/3,26};}
inline Rect noiseControlRect(int i){return i==0?NoiseRate:i==1?NoiseWindow:NoiseButton(i);}
// Options sheet over the page: four rows (rate, window and threshold steppers with < >, the warning switch).
constexpr int NoiseOptionRows=4,NoiseOptionRowH=40,NoiseOptionHeadH=22;
constexpr Rect NoisePopup={10,Top+6,W-20,NoiseOptionHeadH+NoiseOptionRows*NoiseOptionRowH+2};
inline Rect noiseOptionRow(int i){return {NoisePopup.x+3,NoisePopup.y+NoiseOptionHeadH+i*NoiseOptionRowH,NoisePopup.w-6,NoiseOptionRowH-2};}
// A tap on a page step row: left half steps back, right half forward.
inline int noiseStepTap(Rect r,int x){return x<r.x+r.w/2?-1:1;}
}
