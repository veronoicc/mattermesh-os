#include <cassert>
#include <iostream>
#include "mesh/journal.h"
#include "mesh/delivery_log.h"
int main(){
 using matter::delivery::Timeline;using matter::delivery::Kind;
 fs::FS storage;matter::Journal journal;journal.begin(storage);
 // Rows written before timelines existed still read exactly as before: no timeline.
 {auto file=storage.open("/mattermesh/messages.tsv",FILE_APPEND);file.print("R1\tbc\t0\t1\t0\t-1\tYOU\t*\tSent\tOld message\nC1\tbc\t3\n");}
 journal.begin(storage);
 assert(journal.records.size()==1&&journal.records[0].timeline.empty()&&journal.records[0].repeaters==3&&journal.records[0].status=="Sent");
 matter::Record legacyRead;assert(journal.readRecord(0xbc,legacyRead,false)&&legacyRead.timeline.empty()&&legacyRead.body=="Old message"&&legacyRead.repeaters==3);
 // A message without a timeline row; unknown tokens are refused.
 matter::Record m;m.token=0x10;m.chat=0;m.mine=true;m.sender="YOU";m.body="Hello mesh";m.status="Pending send";assert(journal.append(m));
 matter::Record plain;assert(journal.readRecord(0x10,plain)&&plain.timeline.empty());
 assert(!journal.timeline(0xdead,"0,Q,;")&&!journal.timeline(0x10,"")&&!journal.timeline(0x10,"0,Q,;1,T")&&!journal.timeline(0x10,std::string(matter::delivery::MaxEncoded+1,';')));
 // Round trip: RAM cache, a fresh replay and an uncached read all return the same text; the newest row wins.
 Timeline t;t.add(0,Kind::Queued);t.add(1800,Kind::TxDone,"2 hops");
 assert(journal.timeline(0x10,t.encode().c_str()));
 matter::Record r;assert(journal.readRecord(0x10,r)&&r.timeline==t.encode().c_str());
 t.add(2500,Kind::HeardRepeater,"aa");t.add(4200,Kind::AckReceived,"2 hops");assert(journal.timeline(0x10,t.encode().c_str()));
 assert(journal.update(0x10,"2 hops: delivered")&&journal.repeats(0x10,1,"aa"));
 {matter::Journal boot;boot.begin(storage);
  matter::Record cached,fromFile;assert(boot.readRecord(0x10,cached,true)&&boot.readRecord(0x10,fromFile,false));
  assert(cached.timeline==t.encode().c_str()&&fromFile.timeline==cached.timeline&&fromFile.status=="2 hops: delivered"&&fromFile.heard=="aa"&&fromFile.body=="Hello mesh");
  assert(Timeline::decode(fromFile.timeline).summary()=="Delivered via 2 hops");
  assert(boot.summary(0)->last.token==0x10&&boot.summary(0)->last.timeline==cached.timeline);
  // readRecordFrom on an open file matches readRecord.
  auto f=boot.openForRead();matter::Record viaFile;assert(boot.readRecordFrom(f,0x10,viaFile)&&viaFile.timeline==cached.timeline);f.close();
 }
 // The newest chat record also picks the timeline up in RAM right away (summary and cache).
 assert(journal.summary(0)->last.timeline==t.encode().c_str());
 // Outside the 16-record cache the timeline still comes from the SD.
 for(uint32_t i=0;i<20;i++){matter::Record x;x.token=0x100+i;x.chat=0;x.mine=true;x.body="filler";assert(journal.append(x));}
 {matter::Record old;assert(journal.readRecord(0x10,old,false)&&old.timeline==t.encode().c_str());bool cachedStill=false;for(auto &v:journal.records)cachedStill|=v.token==0x10;assert(!cachedStill);}
 // A torn last row (cut mid-write: no new line, token or text cut) never touches a record and the next row starts cleanly.
 {auto file=storage.open("/mattermesh/messages.tsv",FILE_APPEND);file.print("E1\t10\t0,Q,;1800,T,2 ho");}
 {matter::Journal torn;torn.begin(storage);matter::Record x;assert(torn.readRecord(0x10,x,false)&&x.body=="Hello mesh"&&x.status=="2 hops: delivered");
  assert(x.timeline==t.encode().c_str());// the cut row is not a write that completed: the last whole timeline stays
  assert(torn.update(0x10,"after torn"));matter::Journal next;next.begin(storage);matter::Record y;assert(next.readRecord(0x10,y,false)&&y.status=="after torn"&&y.body=="Hello mesh");}
 {auto file=storage.open("/mattermesh/messages.tsv",FILE_APPEND);file.print("\nE1\t1\n");}// cut inside the token: no payload field, ignored
 {matter::Journal cut;cut.begin(storage);matter::Record x;assert(cut.readRecord(0x10,x,false)&&x.body=="Hello mesh"&&x.timeline==t.encode().c_str());}
 // Garbage text in a well formed row: the record stays intact, the decoder yields nothing.
 {auto file=storage.open("/mattermesh/messages.tsv",FILE_APPEND);file.print("\nE1\t10\t\xff\xfe;;;,,;\nE1\tnothex\tx;\nE1\t999999\t0,Q,;\n");}
 {matter::Journal junk;junk.begin(storage);matter::Record x;assert(junk.readRecord(0x10,x,false)&&x.body=="Hello mesh"&&x.status=="after torn");assert(Timeline::decode(x.timeline).events.empty());
  for(auto &v:junk.records)assert(v.body.length());}
 // clearChat removes the chat's rows, live and after a replay; other chats keep theirs.
 {matter::Journal other;other.begin(storage);matter::Record o;o.token=0x20;o.chat=1;o.mine=true;o.body="Other chat";assert(other.append(o));assert(other.timeline(0x20,"0,Q,;"));
  assert(other.timeline(0x10,"0,Q,;"));assert(other.clearChat(0));matter::Record gone;assert(!other.readRecord(0x10,gone,false)&&!other.readRecord(0x10,gone));
  matter::Record kept;assert(other.readRecord(0x20,kept,false)&&kept.timeline=="0,Q,;");
  matter::Journal after;after.begin(storage);assert(!after.readRecord(0x10,gone,false)&&after.readRecord(0x20,kept,false)&&kept.timeline=="0,Q,;");
  assert(!after.timeline(0x10,"0,Q,;"));}
 std::cout<<"PASS: delivery timeline journal rows: legacy rows unchanged, round trip through cache/replay/uncached read, newest row wins, torn and garbage rows ignored, clearChat removes them\n";
}
