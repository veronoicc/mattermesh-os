#include <cassert>
#include <iostream>
#include "services/selftest.h"
using namespace selftest;
struct Script {int starts=0,aborts=0;std::vector<int> order;std::vector<std::vector<Outcome>> steps;std::vector<size_t> polls=std::vector<size_t>(16,0);
 Outcome operator()(int i,Phase p,uint32_t){auto &s=steps[i];if(p==Phase::Abort){aborts++;return {};}if(p==Phase::Start){starts++;order.push_back(i);return s[0];}return s[std::min(++polls[i],s.size()-1)];}};
int main(){
 Runner r;
 r.add({"a","Alpha",1000,false,false});r.add({"b","Bravo",1000,true,false});r.add({"c","Charlie",1000,false,true});r.add({"d","Delta",300,true,false});
 assert(r.entries.size()==4&&r.find("c")==2&&r.find("zz")==-1&&!r.busy());
 // One check per tick; a finished check never delays the UI by running the next one in the same tick.
 Script s;s.steps={{pass("ok")},{wait("press"),wait("press"),fail("bad")},{skip("none")},{wait("tap")}};
 assert(r.runAll()&&r.busy()&&r.batchTotal==4&&!r.runAll());
 assert(r.entries[0].status==Status::Idle);
 uint32_t t=1000;assert(!r.tick(t,s));assert(r.entries[0].status==Status::Pass&&r.entries[0].detail=="ok"&&s.starts==1&&r.batchDone==1);
 assert(!r.tick(t+10,s));assert(r.entries[1].status==Status::Input&&r.entries[1].detail=="press"&&r.current()==1);
 assert(!r.tick(t+50,s));assert(r.entries[1].status==Status::Input);
 assert(!r.tick(t+90,s));assert(r.entries[1].status==Status::Fail&&r.entries[1].detail=="bad"&&r.entries[1].ms==80&&r.current()==-1);
 assert(!r.tick(t+100,s));assert(r.entries[2].status==Status::Skip&&r.entries[2].detail=="none");
 // An interactive check nobody answers is skipped at its timeout and the batch ends there.
 assert(!r.tick(t+110,s));assert(r.entries[3].status==Status::Input&&r.current()==3);
 assert(!r.tick(t+300,s));assert(r.entries[3].status==Status::Input);
 assert(r.tick(t+410,s));assert(r.entries[3].status==Status::Skip&&r.entries[3].detail.find("no input")==0&&s.aborts==1&&!r.busy());
 assert(r.tick(t+500,s)==false&&r.batchDone==4);
 assert((s.order==std::vector<int>{0,1,2,3}));
 auto c=r.counts();assert(c.pass==1&&c.fail==1&&c.skip==2&&c.idle==0);
 // The finished entries are reported in order, once.
 std::vector<int> done;for(int i;(i=r.takeDone())>=0;)done.push_back(i);assert((done==std::vector<int>{0,1,2,3}));assert(r.takeDone()==-1);
 // A Running check that never ends fails at its timeout.
 Runner q;q.add({"slow","Slow",200,false,false});Script slow;slow.steps={{Outcome{}}};
 assert(q.runOne(0));q.tick(0,slow);q.tick(150,slow);assert(q.entries[0].status==Status::Running);assert(q.tick(250,slow));assert(q.entries[0].status==Status::Fail&&q.entries[0].detail.find("timed out")==0);
 // Options: quiet mode and no interactive checks skip those entries on RUN ALL, but RUN ONE still runs them.
 Runner o;o.add({"x","X",1000,false,false});o.add({"in","In",1000,true,false});o.add({"out","Out",1000,false,true});o.options.interactive=false;o.options.output=false;
 Script os;os.steps={{pass()},{pass()},{pass()}};assert(o.runAll()&&o.batchTotal==1);
 assert(o.entries[1].status==Status::Skip&&o.entries[1].detail=="disabled in options"&&o.entries[2].status==Status::Skip);
 o.tick(0,os);assert(o.entries[0].status==Status::Pass&&!o.busy());int n=0;while(o.takeDone()>=0)n++;assert(n==3);// the disabled ones are reported too
 assert(o.runOne(1)&&o.runOne(1)==false);o.tick(5,os);assert(o.entries[1].status==Status::Pass);
 assert(!o.runOne(7)&&!o.runOne(-1));
 // Cancel: the running check is told to stop, nothing queued runs, the cancelled ones are not "run".
 Runner k;k.add({"a","A",9000,false,false});k.add({"b","B",9000,false,false});Script ks;ks.steps={{wait("busy")},{pass()}};
 k.runAll();k.tick(0,ks);assert(k.current()==0);k.cancel(ks);assert(!k.busy()&&ks.aborts==1&&k.entries[0].status==Status::Skip&&k.entries[0].detail=="cancelled"&&k.entries[1].status==Status::Idle);
 k.tick(10,ks);assert(ks.starts==1);
 // Result names and the machine-readable serial lines.
 assert(std::string(statusText(Status::Pass))=="pass"&&std::string(statusText(Status::Fail))=="fail"&&std::string(statusText(Status::Skip))=="skip"&&std::string(statusText(Status::Input))=="input"&&std::string(statusText(Status::Idle))=="idle"&&std::string(statusText(Status::Running))=="running");
 Entry e;e.spec={"sd","SD card",1000,false,false};e.status=Status::Pass;e.detail="mounted\n3.4 GB free";
 assert(serialLine(e)=="SELFTEST name=sd result=pass detail=mounted 3.4 GB free");
 e.detail=std::string(300,'x');assert(serialLine(e).size()<=std::string("SELFTEST name=sd result=pass detail=").size()+MaxDetail);
 assert(serialSummary(r.counts())=="SELFTEST_DONE pass=1 fail=1 skip=2 notrun=0");
 // The text report: header, one block per check, totals.
 std::string rep=report(r,"2026-11-12 09:30:05","1.2.3");
 assert(rep.find("MatterMesh OS self-test")==0&&rep.find("Firmware: 1.2.3")!=std::string::npos&&rep.find("Date: 2026-11-12 09:30:05")!=std::string::npos);
 assert(rep.find("[PASS] Alpha (a) 0 ms\n       ok\n")!=std::string::npos&&rep.find("[FAIL] Bravo (b) 80 ms\n       bad\n")!=std::string::npos&&rep.find("[SKIP] Charlie (c)")!=std::string::npos);
 assert(rep.find("Result: 1 pass, 1 fail, 2 skip, 0 not run\n")!=std::string::npos);
 Runner fresh;fresh.add({"a","Alpha",1,false,false});assert(report(fresh,"","").find("[----] Alpha (a)")!=std::string::npos&&report(fresh,"","").find("Result: 0 pass, 0 fail, 0 skip, 1 not run")!=std::string::npos);
 // File names: the clock when it is set, otherwise the uptime.
 assert(reportName(true,2026,11,12,9,30,5,0)=="selftest-20261112-093005.txt"&&reportName(false,0,0,0,0,0,0,77)=="selftest-up77.txt");
 // CRC-32 (the SD write / read-back check): the standard check value, chaining.
 const uint8_t digits[]={'1','2','3','4','5','6','7','8','9'};assert(crc32(digits,9)==0xCBF43926u);assert(crc32(digits+4,5,crc32(digits,4))==0xCBF43926u);assert(crc32(digits,0)==0);
 // Verdict helpers for the hardware checks.
 assert(rssiSane(-112.5f)&&rssiSane(-30)&&!rssiSane(0.5f)&&!rssiSane(-200)&&!rssiSane(NAN));
 assert(hapticChip(0xe0)==std::string("DRV2605L")&&hapticChip(0x60)==std::string("DRV2605")&&hapticChip(0x00)==nullptr&&hapticChip(0xff)!=nullptr);
 assert(batterySane(3920)&&batterySane(4350)&&!batterySane(0)&&!batterySane(65535)&&!batterySane(2000));
 assert(sizeText(3ull<<30)=="3.0 GB"&&sizeText(512ull<<20)=="512 MB"&&sizeText(8u<<20)=="8 MB"&&sizeText(100)=="100 B"&&sizeText(150000)=="146 KB");
 std::cout<<"PASS: self-test runner (ordered one-per-tick checks, input timeout, options, cancel, serial lines, report, file name, CRC-32, verdict helpers)\n";
}
