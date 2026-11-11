#pragma once
#include "theme.h"
namespace mm {
// Tracks app (TracksAppSection): the record card on top, the list of tracks below (TrackRows rows, a count line at the
// bottom); the detail of one track: title card, six stat cards, a sketch of the route, three actions.
constexpr int TrackRowH=38,TrackRows=5,TrackListY=Top+54;
constexpr Rect TracksRecord={Margin,Top+4,W-2*Margin,44};
inline Rect trackRowRect(int i){return {Margin,TrackListY+i*TrackRowH,W-2*Margin,TrackRowH-2};}
constexpr int TracksCountY=Bottom-13;
constexpr Rect TracksTitle={Margin,Top+4,W-2*Margin,32};
constexpr int TrackStatCount=6;
inline Rect trackStatRect(int i){return settingsHalf(i%2,Top+42+(i/2)*42,38);}
constexpr Rect TracksShape={Margin,Top+170,W-2*Margin,44};
constexpr int TracksNoticeY=Top+217;
constexpr int TrackActionCount=3;
inline Rect trackActionRect(int i){int w=(W-2*Margin-2*6)/3;return {Margin+i*(w+6),Bottom-30,w,30};}
// Detail view: MAP, GPX, DELETE (STOP while the track is recording). Delete asks first: DELETE / CANCEL under the text.
constexpr Rect TracksDeleteYes=ChatClearYes,TracksDeleteNo=ChatClearNo;
// Settings > GPS tracks (TracksSection): the Recording switch card, three ON/OFF rows, then the two < value > rows.
constexpr int TracksSettingCount=5;
inline Rect tracksSettingRect(int i){return {Margin,i<3?Top+70+i*36:Top+194+(i-3)*36,W-2*Margin,32};}
constexpr int TracksSamplingLabelY=Top+178;
}
