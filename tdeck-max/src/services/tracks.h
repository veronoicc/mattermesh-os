#pragma once
#include <FS.h>
#include <string>
#include <vector>
#include "track_log.h"
// GPS breadcrumb tracks on the SD card: /mattermesh/tracks/trk-*.mmt, one file per track (track_log.h rows), GPX exports
// next to them. The recording is fed from the location service; RAM holds only the statistics and a thinned polyline.
namespace device {
struct TrackInfo {std::string name;uint32_t startT=0;mm_track::Summary sum;bool closed=false,live=false;};// name: file stem
struct TracksState {bool sd=false,recording=false,fileOpen=false,fixNow=false;uint32_t points=0,durationS=0,startT=0;double distanceM=0;std::string name,error;};
// One track read for the detail view and the map: statistics and at most ~256 points.
struct TrackView {std::string name;bool live=false;mm_track::Stats stats;std::vector<mm_track::Point> line;};
void tracksBegin(fs::FS &fs,bool sdReady);// settings from NVS, tracks left open by a power cut closed
void tracksLoop();// ~1 Hz: auto start, sampling, SD appends
mm_track::Settings tracksSettings();
bool tracksSetSettings(const mm_track::Settings &settings);// stored in NVS; the GPS hold follows Keep GPS on while recording
bool tracksStart();void tracksStop();// start: the file is created at the first kept fix; stop writes the summary row
TracksState tracksState();
std::vector<TrackInfo> tracksList(size_t max);// newest first
bool tracksSelect(const std::string &name);void tracksDeselect();const TrackView *tracksSelected();
// The polyline the Maps app draws: the selected track, else the track being recorded (when Show track on map is on).
const std::vector<mm_track::Point> &tracksMapLine();
bool tracksExportGpx(const std::string &name,std::string &path);// path: the .gpx written (or the reason on failure)
bool tracksRemove(const std::string &name);// the .mmt only; an exported GPX stays; the track being recorded is refused
std::string tracksGpxPath(const std::string &name);
}
