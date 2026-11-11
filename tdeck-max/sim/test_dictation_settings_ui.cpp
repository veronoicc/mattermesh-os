// Settings > Sound / dictation: the language card fits the page without touching its neighbours, steps with taps, and the page
// and the Settings list change with the language and with the model on the SD card.
#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/navigation_catalog.h"
#include "services/dictation_language.h"
int main(){
 auto inside=[](mm::Rect r){return r.x>=mm::Margin&&r.x+r.w<=mm::W-mm::Margin&&r.y>=mm::Top&&r.y+r.h<=mm::Bottom;};
 auto apart=[](mm::Rect a,mm::Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 std::vector<mm::Rect> page={mm::AudioNotify,mm::AudioSlider,mm::AudioMinus,mm::AudioPlus,mm::AudioToneMessage,mm::AudioToneMention,mm::AudioBackend};
 for(size_t i=0;i<page.size();i++){assert(inside(page[i]));for(size_t j=i+1;j<page.size();j++)assert(apart(page[i],page[j]));}
 // Section labels (16 px above a card) must not run into the card above: volume row, ringtone rows, language card.
 assert(mm::AudioToneMessage.y-16>=mm::AudioSlider.y+mm::AudioSlider.h&&mm::AudioBackend.y-16>=mm::AudioToneMention.y+mm::AudioToneMention.h);
 // The step arrows sit like the ringtone rows': a tap on "<" steps back, anywhere else forward.
 assert(mm::stepRowTap(mm::AudioBackend,mm::AudioBackend.x+mm::AudioBackend.w-44)==-1&&mm::stepRowTap(mm::AudioBackend,mm::AudioBackend.x+mm::AudioBackend.w-20)==1&&mm::stepRowTap(mm::AudioBackend,mm::AudioBackend.x+10)==1);
 // Keys on the page: L / D next, A previous, others do nothing.
 assert(device::dictationLanguageKey('l')==1&&device::dictationLanguageKey('d')==1&&device::dictationLanguageKey('a')==-1&&device::dictationLanguageKey('j')==0&&device::dictationLanguageKey('\n')==0);
 GFXcanvas1 canvas(mm::W,mm::H);mm::Renderer renderer(canvas);mm::State s;s.page=mm::Page::Audio;s.offlineEngineCompiled=true;
 const size_t bytes=size_t(mm::W)*mm::H/8;auto shot=[&](){renderer.draw(s);return std::vector<uint8_t>(canvas.getBuffer(),canvas.getBuffer()+bytes);};
 auto card=[&](){auto all=shot();std::vector<uint8_t> part;for(int y=mm::AudioBackend.y;y<mm::AudioBackend.y+mm::AudioBackend.h;y++)for(int x=0;x<mm::W/8;x++)part.push_back(all[size_t(y)*(mm::W/8)+x]);return part;};
 std::vector<std::vector<uint8_t>> byLanguage;
 for(int language=0;language<device::DICTATION_LANGUAGE_COUNT;language++){s.dictationLanguage=language;s.dictationModelBytes=0;auto missing=card();s.dictationModelBytes=15278512;auto present=card();assert(missing!=present);byLanguage.push_back(present);}
 for(size_t i=0;i<byLanguage.size();i++)for(size_t j=i+1;j<byLanguage.size();j++)assert(byLanguage[i]!=byLanguage[j]);
 s.dictationModelBytes=15278512;auto compiled=card();s.offlineEngineCompiled=false;assert(card()!=compiled);s.offlineEngineCompiled=true;
 // The card never paints outside itself and the stripe below it: nothing under the page bottom margin changes with the language.
 s.dictationLanguage=0;auto english=shot();s.dictationLanguage=1;auto german=shot();
 for(int y=mm::Bottom;y<mm::H;y++)for(int x=0;x<mm::W/8;x++)assert(english[size_t(y)*(mm::W/8)+x]==german[size_t(y)*(mm::W/8)+x]);
 // Settings list: the Sound / dictation row names the language.
 int row=-1;for(int i=0;i<mm::SettingsCount;i++)if(mm::SettingsItems[i].section==21)row=i;assert(row>=0);
 int scroll=-1;for(int sc=0;sc<=mm::settingsMaxScroll();sc++)if(mm::settingsVisible(row,sc)){scroll=sc;break;}assert(scroll>=0);
 s.page=mm::Page::Settings;s.settingsOffset=scroll;s.settingsFocus=row;s.dictationLanguage=0;auto listEnglish=shot();s.dictationLanguage=1;auto listGerman=shot();s.dictationLanguage=2;auto listItalian=shot();
 assert(listEnglish!=listGerman&&listGerman!=listItalian&&listEnglish!=listItalian);
 std::cout<<"PASS: dictation language card inside the page, apart from its neighbours, step taps and keys, page and Settings row follow language and SD model state\n";
}
