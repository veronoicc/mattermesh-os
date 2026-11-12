#include <cassert>
#include <iostream>
#include "services/quick_messages.h"
#include "services/gps_position_policy.h"
using namespace mm_quick;
int main(){
 // Defaults: six presets, no position attached (a preset that refuses to send without GPS must not be the emergency one).
 {auto d=defaults();assert(d.size()==6&&d[0].text=="OK"&&d[1].text=="On my way"&&d[3].text=="Need help"&&d[5].text=="Where are you?");for(auto &p:d)assert(!p.position&&p.text.size()<=MaxTextBytes);}
 // clean: trimmed, control characters become spaces, cut at the byte limit on a code point, invalid UTF-8 dropped from where it starts.
 assert(clean("  hi  ")=="hi");assert(clean("a\tb\nc")=="a b c");assert(clean("   ")=="");assert(clean("")=="");
 assert(clean(std::string(100,'x'))==std::string(MaxTextBytes,'x'));
 {std::string s(MaxTextBytes-1,'x');s+=u8"é";std::string c=clean(s);assert(c==std::string(MaxTextBytes-1,'x'));}// the 2-byte letter would end at 61
 {std::string s(MaxTextBytes-2,'x');s+=u8"é";assert(clean(s)==s&&clean(s).size()==MaxTextBytes);}
 {std::string s=u8"🙂";assert(clean(s)==s);}
 assert(clean(std::string("ok\xc3"))=="ok");assert(clean(std::string("ok\xff more"))=="ok");
 assert(clean(std::string(MaxTextBytes-1,'x')+" y")==std::string(MaxTextBytes-1,'x'));// the cut leaves a trailing space: trimmed again
 // add: trimmed, non-empty, at most eight
 {List l;assert(add(l,"  Hi ",true)==""&&l.size()==1&&l[0].text=="Hi"&&l[0].position);assert(add(l,"   ")==ErrEmpty&&l.size()==1);
  for(int i=1;i<int(MaxPresets);i++)assert(add(l,"m"+std::to_string(i))=="");assert(l.size()==MaxPresets);assert(add(l,"nine")==ErrFull&&l.size()==MaxPresets);}
 // edit / position toggle / remove: bad index and empty text change nothing
 {List l=defaults();assert(edit(l,1,"  Back soon ")==""&&l[1].text=="Back soon"&&l[1].position==false);assert(edit(l,1,"")==ErrEmpty&&l[1].text=="Back soon");assert(edit(l,-1,"x")==ErrIndex&&edit(l,6,"x")==ErrIndex);
  assert(!l[2].position);assert(togglePosition(l,2)&&l[2].position);assert(togglePosition(l,2)&&!l[2].position);assert(!togglePosition(l,9));
  assert(remove(l,0)&&l.size()==5&&l[0].text=="Back soon");assert(!remove(l,5)&&!remove(l,-1)&&l.size()==5);}
 // move: returns the new index, clamps at both ends
 {List l=defaults();assert(moveUp(l,0)==0&&l[0].text=="OK");assert(moveUp(l,2)==1&&l[1].text=="At camp"&&l[2].text=="On my way");assert(moveDown(l,5)==5&&l[5].text=="Where are you?");assert(moveDown(l,1)==2&&l[2].text=="At camp"&&l[1].text=="On my way");assert(moveUp(l,9)==9&&moveDown(l,-1)==-1);}
 {List l;add(l,"one");assert(moveUp(l,0)==0&&moveDown(l,0)==0);List e;assert(moveUp(e,0)==0&&moveDown(e,0)==0);}
 // choices step with wrap and decode with clamping
 {assert(stepKey(Key::Off,1)==Key::AltQ&&stepKey(Key::AltU,1)==Key::Off&&stepKey(Key::Off,-1)==Key::AltU&&stepKey(Key::AltQ,-1)==Key::Off);
  assert(stepMode(Mode::Send,1)==Mode::Insert&&stepMode(Mode::Insert,1)==Mode::Send&&stepMode(Mode::Send,-1)==Mode::Insert);
  assert(keyFrom(-3)==Key::Off&&keyFrom(99)==Key::Off&&keyFrom(2)==Key::AltY&&modeFrom(7)==Mode::Send&&modeFrom(1)==Mode::Insert);
  assert(keyChar(Key::Off)==0&&keyChar(Key::AltQ)=='q'&&keyChar(Key::AltY)=='y'&&keyChar(Key::AltU)=='u');
  assert(std::string(keyLabel(Key::Off))=="Off"&&std::string(keyLabel(Key::AltQ))=="Alt+Q"&&std::string(modeLabel(Mode::Send))=="Send now"&&std::string(modeLabel(Mode::Insert))=="Insert");}
 // serialisation round trip incl. separators, escapes, UTF-8, positions
 {List l;add(l,"a\\b",true);add(l,u8"Où es-tu? 🙂");add(l,"x|y,z;1",true);std::string blob=serialize(l);assert(blob.size()<=MaxBlobBytes);List back;assert(deserialize(blob,back)&&back.size()==3);for(size_t i=0;i<3;i++)assert(back[i].text==l[i].text&&back[i].position==l[i].position);}
 {List d=defaults();List back;assert(deserialize(serialize(d),back)&&back.size()==d.size());for(size_t i=0;i<d.size();i++)assert(back[i].text==d[i].text);}
 {List e;assert(serialize(e)=="-");List back=defaults();assert(deserialize("-",back)&&back.empty());}// an emptied list stays empty
 // the worst case (eight presets of 60 bytes, every one needing an escape) fits the bound
 {List l;for(int i=0;i<int(MaxPresets);i++)l.push_back({std::string(MaxTextBytes,'\\'),true});assert(serialize(l).size()<=MaxBlobBytes);}
 // corrupt or missing blobs -> defaults
 for(std::string bad:{std::string(""),std::string("garbage"),std::string("2x"),std::string("1"),std::string("0a\\"),std::string("0a\\q"),std::string("0a\n"),std::string("0a\xff"),std::string("0ok\n\n0b"),std::string(MaxBlobBytes+1,'0'),std::string("0a\n0b\n0c\n0d\n0e\n0f\n0g\n0h\n0i")}){List out;out.push_back({"keep",false});bool ok=deserialize(bad,out);assert(!ok);assert(out.size()==defaults().size()&&out[0].text=="OK");}
 // compose: text as is, position appended after a space, strict limits, no silent drops
 {Preset p{"Need help",false},q{"Need help",true};
  assert(compose(p,"",160).ok&&compose(p,"",160).text=="Need help");
  assert(compose(p,"GPS: 1.000000, 2.000000",160).text=="Need help");// no position wanted: ignored
  auto r=compose(q,"",160);assert(!r.ok&&r.error==ErrNoFix&&r.text.empty());
  r=compose(q,"GPS: 45.123457, 7.654321",160);assert(r.ok&&r.text=="Need help GPS: 45.123457, 7.654321");
  size_t exact=std::string("Need help GPS: 45.123457, 7.654321").size();assert(compose(q,"GPS: 45.123457, 7.654321",exact).ok);
  r=compose(q,"GPS: 45.123457, 7.654321",exact-1);assert(!r.ok&&r.error==ErrTooLongPos&&r.text.empty());
  r=compose(p,"",5);assert(!r.ok&&r.error==ErrTooLong);assert(compose(p,"",9).ok&&!compose(p,"",8).ok);
  assert(!compose(p,"",0).ok);}
 // position text: the chat's existing format (GPS: lat, lon, six decimals), rounded, only for a fresh valid fix
 {auto t=device::gpsCoordinateMessage(true,true,true,1000,45.1234567,-7.6543219);assert(t=="GPS: 45.123457, -7.654322");
  assert(device::gpsCoordinateMessage(true,true,false,0,1,1).empty());assert(device::gpsCoordinateMessage(true,true,true,20000,1,1).empty());assert(device::gpsCoordinateMessage(true,false,true,0,1,1).empty());
  Preset q{"At camp",true};auto r=compose(q,t,160);assert(r.ok&&r.text=="At camp GPS: 45.123457, -7.654322");assert(r.text.size()<=160);
  // a message made of UTF-8 text plus the position counts bytes, not letters
  Preset u{u8"Où êtes-vous? 🙂",true};r=compose(u,t,u.text.size()+1+t.size());assert(r.ok);r=compose(u,t,u.text.size()+t.size());assert(!r.ok&&r.error==ErrTooLongPos);}
 // typed characters: control bytes refused, the limit enforced
 {assert(accept("",'a')&&accept("abc",'\xc3')&&!accept("abc",'\n')&&!accept("abc",char(127))&&!accept("abc",char(3))&&accept("abc",' '));assert(!accept(std::string(MaxTextBytes,'x'),'a')&&accept(std::string(MaxTextBytes-1,'x'),'a'));}
 // scroll keeps the focused row in a window of `rows`
 {assert(scrollFor(0,0,4,8)==0&&scrollFor(5,0,4,8)==2&&scrollFor(1,3,4,8)==1&&scrollFor(7,0,4,8)==4&&scrollFor(2,2,4,8)==2&&scrollFor(3,0,4,3)==0&&scrollFor(0,5,4,8)==0&&scrollFor(9,0,4,8)==4);}
 std::cout<<"PASS: quick messages: defaults, limits (UTF-8), edit/move/delete, choices, NVS blob round trip and corruption, position compose and byte limits\n";
}
