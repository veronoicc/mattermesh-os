#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/rx_log_layout.h"
static std::vector<uint8_t> shot(GFXcanvas1 &canvas){return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+((mm::W+7)/8)*mm::H);}
int main(){
 GFXcanvas1 canvas(mm::W,mm::H);mm::Components c(canvas);mm::Renderer renderer(canvas);
 // Every touch target sits in the content area and no two targets of a page overlap (also with the options sheet / detail open).
 auto inside=[](mm::Rect r){return r.x>=mm::Margin&&r.x+r.w<=mm::W-mm::Margin&&r.y>=mm::Top&&r.y+r.h<=mm::Bottom;};
 auto apart=[](mm::Rect a,mm::Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 std::vector<std::vector<mm::Rect>> pages;
 {std::vector<mm::Rect> list={mm::RxFilterPrev,mm::RxFilterLabel,mm::RxFilterNext,mm::RxPauseButton};for(int i=0;i<mm::RxLogRows;i++)list.push_back(mm::rxRowRect(i));pages.push_back(list);}
 {std::vector<mm::Rect> options;for(int i=0;i<mm::RxOptionCount;i++)options.push_back(mm::rxOptionRect(i));pages.push_back(options);assert(inside(mm::RxOptionsPopup));for(auto r:options){assert(r.x>=mm::RxOptionsPopup.x&&r.x+r.w<=mm::RxOptionsPopup.x+mm::RxOptionsPopup.w&&r.y>=mm::RxOptionsPopup.y&&r.y+r.h<=mm::RxOptionsPopup.y+mm::RxOptionsPopup.h);}}
 pages.push_back({mm::rxDetailButton(0),mm::rxDetailButton(1),mm::rxDetailButton(2)});assert(inside(mm::RxDetailPanel));
 for(auto &page:pages)for(size_t i=0;i<page.size();i++){assert(inside(page[i]));for(size_t j=i+1;j<page.size();j++)assert(apart(page[i],page[j]));}
 // The toolbar and the rows stay clear of the header and of each other; the detail lines end above its buttons.
 assert(mm::RxFilterPrev.y>=mm::Top&&mm::RxStatusY>=mm::RxFilterPrev.y+mm::RxFilterPrev.h&&mm::RxListY>=mm::RxStatusY+c.lineHeight(mm::Font::ChatMeta)&&mm::rxRowRect(mm::RxLogRows-1).y+mm::rxRowRect(mm::RxLogRows-1).h<=mm::Bottom);
 assert(mm::RxDetailTextY+mm::RxDetailRows*mm::RxDetailLineH<=mm::rxDetailButton(0).y);
 // Row columns hold the widest strings (time, type name, RSSI, SNR, hash, route + hops, length).
 {int w=mm::rxRowRect(0).w-8;
  assert(c.width("22:13:20",mm::Font::ChatMeta)<=mm::RxColType-6&&c.width("-120dBm",mm::Font::ChatMeta)<=mm::RxColSnr-4&&c.width("-12.5dB",mm::Font::ChatMeta)<=mm::RxColHash-mm::RxColSnr-4);
  assert(mm::RxColHash+c.width("1A2B3C4D",mm::Font::ChatMeta)+6+c.width("1000B",mm::Font::ChatMeta)<=w);
  assert(mm::RxColType+c.width("MULTIPART",mm::Font::Bold)+6+c.width("F 40h",mm::Font::ChatMeta)<=w);}
 for(const char *hint:{"W/S ENT  H/L type  SPC pause  M menu","W/S move  H/L/ENTER change  A back","W/S scroll  A close"})assert(c.width(hint,mm::Font::Small)<=mm::W-2*mm::Margin);
 assert(c.width("RESUME",mm::Font::Bold)<=mm::RxPauseButton.w-4&&c.width("Adverts",mm::Font::Bold)<=mm::RxFilterLabel.w-4);
 // Pages render with empty and with full data, and the picture follows the state.
 auto draw=[&](const mm::State &s){renderer.draw(s);return shot(canvas);};
 mm::State s;s.page=mm::Page::RxLog;
 auto empty=draw(s);s.rxl.ready=false;auto notReady=draw(s);assert(empty!=notReady);s.rxl.ready=true;
 for(int i=0;i<mm::RxLogRows;i++){mm::RxLogRow row;row.time="22:13:2"+std::to_string(i);row.type=i%2?"ADVERT":"GRP_TXT";row.route=i%2?"F":"D";row.hops=std::to_string(i)+"h";row.rssi="-97dBm";row.snr="5.5dB";row.hash="1A2B3C4D";row.length="87B";s.rxl.rows.push_back(row);}
 s.rxl.shown=420;s.rxl.stored=500;
 auto live=draw(s);assert(live!=empty);
 s.rxl.rows[2].selected=true;auto selected=draw(s);assert(selected!=live);
 s.rxl.paused=true;s.rxl.fresh=12;auto paused=draw(s);assert(paused!=selected);s.rxl.paused=false;
 s.rxl.filterName="Adverts";s.rxl.filtered=true;auto filtered=draw(s);assert(filtered!=selected);
 s.rxl.first=100;auto scrolled=draw(s);assert(scrolled!=filtered);
 s.rxl.rows.clear();s.rxl.shown=0;draw(s);s.rxl.filtered=false;draw(s);
 // Options sheet: every row focused, value changes, the clear confirmation.
 s.rxl.options=true;std::vector<std::vector<uint8_t>> sheets;for(int f=0;f<mm::RxOptionCount;f++){s.rxl.optionFocus=f;sheets.push_back(draw(s));}
 for(size_t i=1;i<sheets.size();i++)assert(sheets[i]!=sheets[i-1]);
 s.rxl.optionFocus=4;auto before=draw(s);s.rxl.clearArmed=true;assert(draw(s)!=before);s.rxl.clearArmed=false;
 s.rxl.uptime=true;auto uptime=draw(s);assert(uptime!=before);s.rxl.autoPause=false;assert(draw(s)!=uptime);s.rxl.defaultFilterName="Group";draw(s);
 s.notice="rxlog-1700000000.csv (500)";assert(draw(s)!=before);s.notice="";
 s.rxl.options=false;
 // Detail: short and long (scrolling) content, every scroll position paints differently at the ends.
 s.rxl.detail=true;s.rxl.detailTitle="Packet #4242 MULTIPART";for(int i=0;i<10;i++)s.rxl.detailLines.push_back("Line "+std::to_string(i));
 auto shortDetail=draw(s);assert(shortDetail!=live);
 for(int i=10;i<60;i++)s.rxl.detailLines.push_back(std::to_string(i)+". A0A1 A very long repeater name that must be cut at the panel edge");
 auto top=draw(s);s.rxl.detailScroll=3;auto moved=draw(s);assert(top!=moved);s.rxl.detailScroll=999;auto end=draw(s);assert(end!=moved);
 s.rxl.detail=false;assert(draw(s)!=top);
 std::cout<<"PASS: rx log UI targets inside the content area and apart, columns fit, list/paused/filter/options/detail pages render and follow the state\n";
}
