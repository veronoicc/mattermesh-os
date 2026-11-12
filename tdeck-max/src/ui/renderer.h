#pragma once
#include "model.h"
#include "artwork.h"
namespace mm {
struct MessageHit {Rect area,sender;Message message;int index;};
class Renderer {
 Adafruit_GFX &g;Components c;uint8_t *bits=nullptr;// the canvas pixels, when drawing on a GFXcanvas1 (fast paths)
 std::vector<std::pair<Rect,int>> bubbleHits;std::vector<std::pair<Rect,int>> senderHits;
public:
  Rect dmPathLineOut{0,0,0,0};// direct chat: the "DM path" line drawn above the composer (a tap opens Path)
 int chatComposerTop=ComposerY;// top of the composer card painted by chat(): ComposerY unless a draft expanded it
private:
 void dmPathPage(const State &s);// Chat options > Path (43)
 void homeHeader(const State &s,int dy,int id);// Home clock band (Classic, or a smaller clock and the widget box), moved down by dy (previews)
 void homeWidget(const State &s,int id,int x,int top,int w);// one Home widget in the box right of the clock
 void homeChats(const State &s,int dy);// Home chat cards, moved down by dy
 void mainWidget(const State &s,int id,Rect r);// one Main area widget in r
 void shell(const State &s,const std::string &title,bool back=true,Icon right=Icon::Menu);
 Rect overlayPanel();// clears below the header and draws the card shared by the chat overlays
 void mapView(const State &s); // MapArea: tiles, route, pins, own position, status, scale
 void heroClock(const std::string &text,int y);
 void wallpaperThumb(int index,int x,int y); // lock-screen clock glyphs, centred
public:
 explicit Renderer(Adafruit_GFX &canvas):g(canvas),c(canvas){}
 explicit Renderer(GFXcanvas1 &canvas):g(canvas),c(canvas),bits(canvas.getBuffer()){}
 void lock(const State &s);
 void quickReply(const State &s);
 void home(const State &s);
 void launcher(const State &s);
 void messages(const State &s);
 void chat(const State &s);
 void channelOptions(const State &s);
 void channelKey(const State &s);
 void contactPicker(const State &s);
 void radioLinkPages(const State &s);// Settings > Companion link (41, 42)
 void tracksPages(const State &s);// Tracks app (49) and Settings > GPS tracks (50)
 void cellularPage(const State &s);// Settings > Cellular (51)
 void smsPage(const State &s);// SMS app (52)
 void notesPage(const State &s);// Notes app (56)
 void quickPage(const State &s);// Settings > Quick messages (61), quickmsg_renderer.inc; quickPicker(s) draws the picker over the chat
 void quickPicker(const State &s);
 void deliveryPage(const State &s);// message Delivery history (62), delivery_renderer.inc
 void scheduledPage(const State &s);// Scheduled app (63), scheduled_renderer.inc
 void pinPad(const State &s);// PIN pad: over the lock screen, and inside Settings > Security (58)
 void securityPage(const State &s);// Settings > Security (58)
 void backupPage(const State &s);// Settings > Backup / restore (59)
 void powerPages(const State &s);// Settings > Power / timeouts (20), Battery (5), Power profile (57)
 void radioLinkAsk();// over any page: download the companion's messages?
 void cliApp(const State &s);
 void rxLogApp(const State &s);// RX Log (rx_log_renderer.inc)
 void noiseApp(const State &s);// Noise Floor (noise_renderer.inc)
 void rptApp(const State &s);// RPT Scanner (rpt_renderer.inc)
 void selfTestApp(const State &s);// Self-test (selftest_renderer.inc)
 int cliLineAt(const State &s,int y); // raw CLI line under y, -1 if none
 int cliMaxScroll(const State &s);
 const std::vector<std::pair<Rect,int>> &messageBounds()const{return bubbleHits;}
 bool senderHit(int x,int y)const{for(auto &r:senderHits)if(r.first.contains(x,y))return true;return false;}
 std::vector<MessageHit> snapshotMessages(const State &s)const{std::vector<MessageHit> out;for(auto &hit:bubbleHits){if(hit.second<0||hit.second>=int(s.messages.size()))continue;Rect sender={0,0,0,0};for(auto &name:senderHits)if(name.second==hit.second){sender=name.first;break;}out.push_back({hit.first,sender,s.messages[hit.second],hit.second});}return out;}
 void form(const State &s);
 void settings(const State &s);
 void section(const State &s);
 void quickSettings(const State &s);
 void quickSlider(int row,bool sun,const std::string &label,int step,bool off);
 void quickTile(int index,const std::string &label,bool on,int glyph);
 void draw(const State &s);
 void welcome(const State &s);// first-boot setup
 void timeZoneList(const State &s,int rows,Rect (*row)(int));// the zone list of the welcome and Date / time
 void holdBanner(const std::string &label);// one-line notice card under the status bar
 void unlockTapBanner(int taps);
 void notificationBanner(Rect r,int clearTo,Icon icon,const std::string &title,const std::string &tag,const std::string &body);// card over the status bar and header, down to clearTo
};
}
