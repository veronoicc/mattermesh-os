#include <cassert>
#include <iostream>
#include "mesh/delivery_log.h"
using namespace matter::delivery;
int main(){
 // Relative time: tenths under a second, whole seconds, minutes, hours; Unknown (legacy rows) has no time.
 assert(relative(0)=="+0.0 s"&&relative(400)=="+0.4 s"&&relative(999)=="+0.9 s");
 assert(relative(1000)=="+1 s"&&relative(3000)=="+3 s"&&relative(59999)=="+59 s");
 assert(relative(60000)=="+1 min"&&relative(125000)=="+2 min"&&relative(3599999)=="+59 min"&&relative(3600000)=="+1 h");
 assert(relative(Unknown)=="--");
 // Events keep their order; Queued is only taken once and stays first.
 Timeline t;assert(t.events.empty()&&t.summary()=="No history");
 assert(t.add(0,Kind::Queued)&&t.add(10,Kind::Queued)==false&&t.events.size()==1);
 assert(t.summary()=="Queued");
 assert(t.add(1800,Kind::TxDone,"2 hops")&&t.summary()=="Transmitted");
 // A repeater is listed once; its first sighting keeps the time.
 assert(t.add(2500,Kind::HeardRepeater,"aa")&&!t.add(2900,Kind::HeardRepeater,"aa")&&t.add(3100,Kind::HeardRepeater,"bb"));
 assert(t.heard()==2&&t.events.size()==4&&t.events[2].at==2500);
 assert(t.summary()=="Heard by 2 repeaters");
 {Timeline one;one.add(0,Kind::Queued);one.add(5,Kind::HeardRepeater,"cc");assert(one.summary()=="Heard by 1 repeater");}
 // ACK: route and hops come from the route label.
 assert(t.add(4200,Kind::AckReceived,"2 hops")&&t.hops()==2&&t.route()=="2 hops"&&t.summary()=="Delivered via 2 hops");
 {Timeline a;a.add(0,Kind::AckReceived,"1 hop");assert(a.hops()==1&&a.summary()=="Delivered via 1 hop");}
 {Timeline a;a.add(0,Kind::AckReceived,"Direct");assert(a.hops()==0&&a.summary()=="Delivered direct");}
 {Timeline a;a.add(0,Kind::AckReceived,"Flood");assert(a.hops()==-1&&a.route()=="Flood"&&a.summary()=="Delivered via flood");}
 {Timeline a;a.add(0,Kind::AckReceived);assert(a.hops()==-1&&a.summary()=="Delivered");}
 assert(Timeline().hops()==-1&&Timeline().route().empty());
 // Failures and missing ACKs.
 {Timeline f;f.add(0,Kind::Queued);f.add(900,Kind::Failed,"TX failed");assert(f.summary()=="Failed: TX failed");}
 {Timeline f;f.add(0,Kind::Failed);assert(f.summary()=="Failed");}
 {Timeline n;n.add(0,Kind::Queued);n.add(1000,Kind::TxDone);n.add(9000,Kind::NoAck,"Flood");assert(n.summary()=="No ACK");n.add(9100,Kind::HeardRepeater,"aa");n.add(9200,Kind::HeardRepeater,"bb");assert(n.summary()=="No ACK, heard by 2 repeaters");}
 {Timeline r;r.add(0,Kind::Queued);r.add(1000,Kind::TxDone,"3 hops");r.add(8000,Kind::RetryFlood);assert(r.summary()=="Retrying by flood");r.add(9000,Kind::TxDone,"Flood (retry)");assert(r.summary()=="Transmitted");r.add(20000,Kind::NoAck,"Flood (retry)");assert(r.summary()=="No ACK");}
 {Timeline m;m.add(0,Kind::Queued);m.add(1,Kind::ManualPath,"2 hops");assert(m.summary()=="Queued");}
 // Descriptions: a name replaces the repeater prefix.
 assert(describe({0,Kind::Queued,""})=="Queued"&&describe({0,Kind::TxDone,""})=="Transmitted"&&describe({0,Kind::TxDone,"2 hops"})=="Transmitted (2 hops)");
 assert(describe({0,Kind::HeardRepeater,"aa"})=="Heard by [aa]"&&describe({0,Kind::HeardRepeater,"aa"},"Hill")=="Heard by Hill");
 assert(describe({0,Kind::AckReceived,"Direct"})=="ACK received via Direct"&&describe({0,Kind::AckReceived,""})=="ACK received");
 assert(describe({0,Kind::NoAck,"Flood"})=="No ACK (Flood)"&&describe({0,Kind::NoAck,""})=="No ACK");
 assert(describe({0,Kind::RetryFlood,""})=="No ACK, retrying by flood"&&describe({0,Kind::Failed,"TX failed"})=="Failed: TX failed"&&describe({0,Kind::ManualPath,"1 hop"})=="Manual path: 1 hop");
 assert(std::string(marker(Kind::Queued)).size()<=4&&std::string(marker(Kind::AckReceived))!=marker(Kind::NoAck));
 // Cap: heard repeaters make room first, then the oldest event after Queued; a repeater never pushes out other events.
 {Timeline c;c.add(0,Kind::Queued);c.add(1,Kind::TxDone);for(int i=0;i<10;i++)c.add(10+i,Kind::HeardRepeater,std::to_string(10+i));assert(c.events.size()==MaxEvents);
  assert(c.add(99,Kind::HeardRepeater,"zz")&&c.events.size()==MaxEvents&&c.heard()==10);
  assert(c.add(100,Kind::AckReceived,"Direct")&&c.events.size()==MaxEvents&&c.events.front().kind==Kind::Queued&&c.events.back().kind==Kind::AckReceived&&c.events[1].kind==Kind::TxDone&&c.events[2].arg=="12");
  assert(c.heard()==9);}
 {Timeline c;c.add(0,Kind::Queued);for(int i=0;i<11;i++)c.add(1+i,i%2?Kind::RetryFlood:Kind::TxDone);assert(c.events.size()==MaxEvents);
  assert(!c.add(50,Kind::HeardRepeater,"aa")&&c.heard()==0);
  assert(c.add(60,Kind::NoAck)&&c.events.size()==MaxEvents&&c.events.front().kind==Kind::Queued&&c.events.back().kind==Kind::NoAck&&c.events[1].at==2);}
 assert(Timeline().add(0,Kind::HeardRepeater,"")==false);// a repeater needs its prefix
 {Timeline long1;long1.add(0,Kind::Failed,std::string(100,'x'));assert(long1.events[0].arg.size()==MaxArg);}
 // Text form: one tab/newline-free journal field, separators escaped, bounded.
 {Timeline e;e.add(0,Kind::Queued);e.add(1800,Kind::TxDone,"2 hops");e.add(2500,Kind::HeardRepeater,"aa");e.add(4200,Kind::AckReceived,"2 hops");e.add(5000,Kind::Failed,"a;b,c%d\te\nf\r");
  auto text=e.encode();assert(text.find('\t')==std::string::npos&&text.find('\n')==std::string::npos&&text.find('\r')==std::string::npos);
  assert(text.rfind("0,Q,;1800,T,2 hops;2500,H,aa;4200,A,2 hops;5000,F,a%3Bb%2Cc%25d%09e%0Af%0D;",0)==0);
  auto back=Timeline::decode(text);assert(back.events.size()==5&&back.events[4].arg=="a;b,c%d\te\nf\r"&&back.events[3].at==4200&&back.events[1].kind==Kind::TxDone&&back.encode()==text);}
 {Timeline full;full.add(0,Kind::Queued);for(int i=0;i<30;i++)full.add(1000*i,Kind::Failed,std::string(24,'\n'));assert(full.encode().size()<=MaxEncoded&&Timeline::decode(full.encode()).events.size()==MaxEvents);}
 assert(Timeline().encode().empty()&&Timeline::decode("").events.empty());
 // Tolerant decode: garbage, torn tail, unknown kinds, bad numbers, bad escapes, oversized input; never throws.
 assert(Timeline::decode("garbage").events.empty()&&Timeline::decode(";;;,,,;;").events.empty()&&Timeline::decode("\xff\xfe\x01").events.empty());
 {auto torn=Timeline::decode("0,Q,;1800,T,2 hops;2500,H,a");assert(torn.events.size()==2&&torn.events[1].arg=="2 hops");}// last event cut: its terminator is missing
 {auto unknown=Timeline::decode("0,Q,;5,Z,x;7,T,;");assert(unknown.events.size()==2&&unknown.events[1].kind==Kind::TxDone);}
 {auto bad=Timeline::decode("x,Q,;-5,T,;99999999999,T,;0,QQ,;0,,;5,H,%zz;5,H,%4;6,A,ok;");assert(bad.events.size()==1&&bad.events[0].kind==Kind::AckReceived&&bad.events[0].arg=="ok");}
 assert(Timeline::decode(std::string(5000,';')).events.empty()&&Timeline::decode("0,Q,;"+std::string(5000,'a')).events.empty());
 {auto dup=Timeline::decode("0,Q,;1,H,aa;2,H,aa;3,Q,;");assert(dup.events.size()==2);}
 // Old rows (no timeline) become a short history from the stored status and heard list.
 {auto l=Timeline::legacy("Pending send",-1,"");assert(l.events.size()==1&&l.events[0].kind==Kind::Queued&&l.events[0].at==Unknown&&l.summary()=="Queued");}
 {auto l=Timeline::legacy("2 hops: delivered",1,"aa");assert(l.events.size()==3&&l.events[1].kind==Kind::AckReceived&&l.events[1].arg=="2 hops"&&l.hops()==2&&l.events[2].kind==Kind::HeardRepeater&&l.summary()=="Delivered via 2 hops");}
 {auto l=Timeline::legacy("Sent / no group ACK",2,"aa,bb");assert(l.events.size()==4&&l.events[1].kind==Kind::TxDone&&l.heard()==2&&l.summary()=="Heard by 2 repeaters");}
 {auto l=Timeline::legacy("Manual path: no ACK",0,"");assert(l.events[1].kind==Kind::NoAck&&l.events[1].arg=="Manual path");}
 {auto l=Timeline::legacy("No ACK: retrying by flood",-1,"");assert(l.events[1].kind==Kind::RetryFlood);}
 {auto l=Timeline::legacy("Not sent: Radio offline",-1,"");assert(l.events[1].kind==Kind::Failed&&l.events[1].arg=="Not sent: Radio offline");}
 {auto l=Timeline::legacy("Flood: queued, awaiting ACK",-1,"");assert(l.events.size()==1);}
 {auto l=Timeline::legacy("Sent",3,"");assert(l.events.size()==2&&l.heard()==0);}// names not recorded: the count alone has no prefixes
 {auto l=Timeline::legacy("Sent",3,"aa,,zz!,bb");assert(l.heard()==2);}// invalid prefixes are skipped
 std::cout<<"PASS: delivery timeline: bounded de-duplicated events, summaries, relative times, escaped torn-tolerant text form, legacy synthesis\n";
}
