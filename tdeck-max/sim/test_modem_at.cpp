#include "services/modem_at.h"
#include <cassert>
#include <iostream>
using namespace modem_at;
using std::string;

int main(){
  assert(trim(" \r\n a b \r\n")=="a b"&&trim("")==""&&trim("  ")=="");
  int a=0,b=0;
  assert(parseCsq("+CSQ: 20,99\r",a,b)&&a==20&&b==99);
  assert(parseCsq("  +CSQ:5,1 ",a,b)&&a==5&&b==1);
  assert(!parseCsq("+CSQ: 20",a,b)&&!parseCsq("+CSQ: x,y",a,b)&&!parseCsq("",a,b)&&!parseCsq("+CSQ:",a,b)&&!parseCsq("OK",a,b)&&!parseCsq("+CSQ: 1,2,3",a,b));
  assert(csqBars(99)==0&&csqBars(0)==0&&csqBars(1)==1&&csqBars(9)==1&&csqBars(10)==2&&csqBars(14)==2&&csqBars(15)==3&&csqBars(19)==3&&csqBars(20)==4&&csqBars(31)==4&&csqBars(-3)==0);
  assert(csqDbm(20)==-73&&csqDbm(99)==0&&csqDbm(32)==0&&csqDbm(-1)==0&&csqDbm(1)==-111);

  int st=-9,act=-9;
  assert(parseReg("+CEREG: 0,1","+CEREG",st,act)&&st==1&&act==-1);
  assert(parseReg("+CEREG: 2,5,\"1A2B\",\"01234567\",7","+CEREG",st,act)&&st==5&&act==7);
  assert(parseReg("+CEREG: 1","+CEREG",st,act)&&st==1&&act==-1);
  assert(parseReg("+CEREG: 2,\"1A2B\",\"0123\",7\r","+CEREG",st,act)&&st==2&&act==7);
  assert(parseReg("+CEREG: 5,\"1A2B\",\"0123\"","+CEREG",st,act)&&st==5&&act==-1);
  assert(parseReg("+CREG: 0,1","+CREG",st,act)&&st==1);
  assert(!parseReg("+CGREG: 0,1","+CREG",st,act)&&!parseReg("+CREG: 0,1","+CGREG",st,act)&&parseReg("+CGREG: 0,5,\"1\",\"2\",2","+CGREG",st,act)&&st==5&&act==2);
  assert(!parseReg("+CEREG:","+CEREG",st,act)&&!parseReg("+CEREG: a,b","+CEREG",st,act)&&!parseReg("","+CEREG",st,act));
  assert(regOk(1)&&regOk(5)&&!regOk(0)&&!regOk(2)&&regRoaming(5)&&!regRoaming(1)&&regDenied(3)&&!regDenied(1)&&regSearching(2)&&!regSearching(3));
  assert(ratClass(-1)==0&&ratClass(99)==0&&ratClass(0)==1&&ratClass(1)==1&&ratClass(3)==1&&ratClass(2)==2&&ratClass(4)==2&&ratClass(6)==2&&ratClass(7)==3&&ratClass(13)==3&&ratClass(14)==0);

  string op;
  assert(parseCops("+COPS: 0,0,\"vodafone IT\",7",op,act)&&op=="vodafone IT"&&act==7);
  assert(parseCops("+COPS: 0,0,\"Acme, Inc\"",op,act)&&op=="Acme, Inc"&&act==-1);
  assert(parseCops("+COPS: 0,0,\"Acme, Inc\",2\r",op,act)&&act==2);
  assert(!parseCops("+COPS: 0",op,act)&&!parseCops("",op,act)&&!parseCops("+COPS: 0,0,\"open",op,act)&&!parseCops("+CREG: 0,0,\"x\"",op,act));

  assert(parseCpin("+CPIN: READY")==Cpin::Ready&&parseCpin("+CPIN: SIM PIN\r")==Cpin::Pin&&parseCpin("+CPIN: SIM PUK")==Cpin::Puk&&parseCpin("+CPIN: SIM PIN2")==Cpin::Pin2&&parseCpin("+CPIN: SIM PUK2")==Cpin::Puk2);
  assert(parseCpin("+CPIN: PH-NET PIN")==Cpin::Other&&parseCpin("OK")==Cpin::Unknown&&parseCpin("")==Cpin::Unknown);

  int code=-1;
  assert(parseCmeError("+CME ERROR: 10",code)&&code==10);
  assert(parseCmeError("+CME ERROR: SIM not inserted",code)&&code==10);
  assert(parseCmeError("+CME ERROR: SIM PIN required",code)&&code==11);
  assert(parseCmeError("+CME ERROR: sim puk required",code)&&code==12);
  assert(parseCmeError("+CME ERROR: incorrect password",code)&&code==16);
  assert(parseCmeError("+CME ERROR: SIM failure",code)&&code==13);
  assert(parseCmeError("+CME ERROR: SIM busy",code)&&code==14);
  assert(parseCmeError("+CME ERROR: SIM wrong",code)&&code==15);
  assert(parseCmeError("+CME ERROR: something odd",code)&&code==0);
  assert(!parseCmeError("ERROR",code)&&!parseCmeError("",code));
  assert(simMissing(10)&&simMissing(13)&&!simMissing(11)&&!simMissing(0));

  const string ati="Manufacturer: SIMCOM INCORPORATED\r\nModel: A7682E\r\nRevision: A7682E-ABC_V1.05\r\nIMEI: 861234567890123\r\n\r\nOK\r\n";
  assert(parseModel(ati)==Model::A7682E&&modelLabel(ati)=="A7682E");
  assert(parseModel("Model: A7670G-LLSE\r\nOK")==Model::A7670G&&modelLabel("Model: A7670G-LABE")=="A7670G");
  assert(parseModel("Model: a7670g-labe")==Model::A7670G);
  assert(parseModel("Model: A7672E\r\nOK")==Model::Other&&modelLabel("Model: A7672E\r\nOK")=="A7672E");
  assert(parseModel("OK")==Model::Unknown&&modelLabel("")==""&&modelLabel("OK")=="");
  assert(modelLabel("Manufacturer: SIMCOM INCORPORATED\r\nOK")=="SIMCom"&&parseModel("Manufacturer: SIMCOM INCORPORATED")==Model::Unknown);

  // UCS2 / UTF-8
  assert(ucs2Hex("")==""&&ucs2Hex("+39")=="002B00330039");
  assert(ucs2Hex("\xC3\xA8")=="00E8");
  assert(ucs2Hex("\xE4\xB8\xAD")=="4E2D");
  assert(ucs2Hex("\xF0\x9F\x98\x80")=="D83DDE00");
  assert(ucs2Hex("\xFF")=="FFFD"&&ucs2Hex("\xC3")=="FFFD"&&ucs2Hex("a\x80""b")=="0061FFFD0062");
  string u;
  assert(utf8FromUcs2Hex("002B00330039",u)&&u=="+39");
  assert(utf8FromUcs2Hex("00e8",u)&&u=="\xC3\xA8");
  assert(utf8FromUcs2Hex("4E2D",u)&&u=="\xE4\xB8\xAD");
  assert(utf8FromUcs2Hex("D83DDE00",u)&&u=="\xF0\x9F\x98\x80");
  assert(utf8FromUcs2Hex("",u)&&u.empty());
  assert(!utf8FromUcs2Hex("002",u)&&!utf8FromUcs2Hex("00ZZ",u)&&!utf8FromUcs2Hex("D83D",u)&&!utf8FromUcs2Hex("DE00",u)&&!utf8FromUcs2Hex("D83D0041",u));
  { string s="Ciao \xC3\xA8 \xE4\xB8\xAD \xF0\x9F\x98\x80 z"; assert(utf8FromUcs2Hex(ucs2Hex(s),u)&&u==s); }

  // GSM7
  assert(gsm7Representable("")&&gsm7Representable("Hello, world 123 @$_!?"));
  assert(gsm7Representable("\xC3\xA8\xC3\xA9\xC3\xB9\xC3\xAC\xC3\xB2\xC3\xA0\xC3\x87\xC3\xB1\xC3\xBC\xC3\xB6\xC3\xA4\xC3\x85\xC3\xA5\xC3\x98\xC3\xB8\xC3\x86\xC3\xA6\xC3\x9F\xC3\x89\xC3\x84\xC3\x96\xC3\x91\xC3\x9C\xC2\xA7"));
  assert(gsm7Representable("|^{}[]~\\\xE2\x82\xAC")&&gsm7Representable("a\nb"));
  assert(!gsm7Representable("\xF0\x9F\x98\x80")&&!gsm7Representable("\xD0\x9F")&&!gsm7Representable("\xC3\xAA")&&!gsm7Representable("`")&&!gsm7Representable("\xE4\xB8\xAD")&&!gsm7Representable("\xFF"));
  assert(smsUnits("")==0&&smsUnits("abc")==3&&smsUnits("a{b}")==6&&smsUnits("\xC3\xA8")==1&&smsUnits("\xE2\x82\xAC")==2);
  assert(smsUnits("\xF0\x9F\x98\x80")==2&&smsUnits("a\xE4\xB8\xAD")==2&&smsUnits("{\xE4\xB8\xAD")==2);
  assert(smsLimit("hi")==160&&smsLimit("\xE4\xB8\xAD")==70&&smsLimit("")==160);
  assert(validSmsNumber("+393491234567")&&validSmsNumber("112")&&validSmsNumber("12345678901234567890")&&!validSmsNumber("12")&&!validSmsNumber("123456789012345678901")&&!validSmsNumber("+")&&!validSmsNumber("+12")&&!validSmsNumber("34 91")&&!validSmsNumber("")&&!validSmsNumber("12a45")&&!validSmsNumber("++123")&&!validSmsNumber("123+45"));
  assert(smsTimeLabel("26/11/11,10:20:30+04")=="11/11 10:20"&&smsTimeLabel("26/01/02,03:04:05")=="02/01 03:04");
  assert(smsTimeLabel("")==""&&smsTimeLabel("garbage")==""&&smsTimeLabel("26/11/11 10:20:30")==""&&smsTimeLabel("2a/11/11,10:20:30")=="");
  assert(atSafe("")&&atSafe("+39 349 1234")&&atSafe("caff\xC3\xA8")&&!atSafe("a\"b")&&!atSafe("a\\b")&&!atSafe("a\nb")&&!atSafe(string("a\0b",3))&&!atSafe("\x7F"));

  // CMTI / CMGS
  int idx=0;
  assert(parseCmti("+CMTI: \"SM\",3",op,idx)&&op=="SM"&&idx==3);
  assert(parseCmti(" +CMTI:\"ME\", 12 \r",op,idx)&&op=="ME"&&idx==12);
  assert(!parseCmti("+CMTI: SM,3",op,idx)&&!parseCmti("+CMTI: \"SM\"",op,idx)&&!parseCmti("",op,idx)&&!parseCmti("+CMTI: \"SM\",x",op,idx));
  assert(parseCmgs("+CMGS: 12\r",idx)&&idx==12&&!parseCmgs("+CMGS:",idx)&&!parseCmgs("OK",idx)&&!parseCmgs("+CMGS: x",idx));

  // CMGL, UCS2 mode, 3 blocks
  {
    string r="+CMGL: 1,\"REC UNREAD\",\"002B00330039003300340039\",\"\",\"26/11/11,10:20:30+04\",145,12\r\n"
             "004300690061006F002000E8\r\n"
             "+CMGL: 2,\"REC READ\",\"002B003300390033\",\"\",\"26/11/10,09:01:02+04\",145,4\r\n"
             "4E2D\r\n"
             "+CMGL: 7,\"STO SENT\",\"002B0033\",\"\",\"\"\r\n"
             "D83DDE00\r\n\r\nOK\r\n";
    auto v=parseCmglReply(r,true);
    assert(v.size()==3);
    assert(v[0].index==1&&v[0].unread&&v[0].number=="+39349"&&v[0].scts=="26/11/11,10:20:30+04"&&v[0].text=="Ciao \xC3\xA8"&&v[0].dcs==-1);
    assert(v[1].index==2&&!v[1].unread&&v[1].number=="+393"&&v[1].text=="\xE4\xB8\xAD");
    assert(v[2].index==7&&!v[2].unread&&v[2].scts==""&&v[2].text=="\xF0\x9F\x98\x80");
  }
  // CMGL plain text, multi-line body, empty body, malformed block skipped
  {
    string r="+CMGL: 1,\"REC UNREAD\",\"+39349\",\"\",\"26/11/11,10:20:30+04\"\r\nriga uno\r\nriga due\r\n"
             "+CMGL: 2,\"REC READ\",\"+39350\",\"\",\"26/11/11,10:21:30+04\"\r\n\r\n"
             "+CMGL: zz,\"REC READ\"\r\nskipped\r\n"
             "+CMGL: 4,\"STO UNSENT\",\"+39351\",\"\",\"\"\r\nOK\r\nOK\r\n";
    auto v=parseCmglReply(r,false);
    assert(v.size()==3);
    assert(v[0].index==1&&v[0].text=="riga uno\nriga due");
    assert(v[1].index==2&&v[1].text.empty()&&!v[1].unread);
    assert(v[2].index==4&&!v[2].unread&&v[2].text=="OK");
  }
  { // not valid hex stays as is in ucs2 mode; empty and garbage replies
    auto v=parseCmglReply("+CMGL: 1,\"REC READ\",\"+39349\",\"\",\"x\"\r\nhello\r\nOK\r\n",true);
    assert(v.size()==1&&v[0].number=="+39349"&&v[0].text=="hello");
    assert(parseCmglReply("",true).empty()&&parseCmglReply("OK\r\n",true).empty()&&parseCmglReply("\xff\x01+CMGL:\r\n\"\"\",,,",false).empty()&&parseCmglReply("+CMS ERROR: 500",true).empty());
  }
  // CMGR
  {
    ParsedSms m;
    string r="+CMGR: \"REC UNREAD\",\"002B00330039\",\"\",\"26/11/11,10:20:30+04\",145,4,0,8,\"+39349\",145,10\r\n0043006900610073\r\n\r\nOK\r\n";
    assert(parseCmgrReply(r,true,m)&&m.index==0&&m.unread&&m.number=="+39"&&m.scts=="26/11/11,10:20:30+04"&&m.dcs==8&&m.text=="Cias");
    ParsedSms n;
    assert(parseCmgrReply("+CMGR: \"REC READ\",\"+39349\",\"\",\"26/11/11,10:20:30+04\"\r\nline1\r\nline2\r\n\r\nOK\r\n",false,n)&&!n.unread&&n.dcs==-1&&n.text=="line1\nline2");
    ParsedSms e;
    assert(parseCmgrReply("+CMGR: \"REC READ\",\"+39349\",\"\",\"26/11/11,10:20:30+04\",145,4,0,0,\"\",145,0\r\n\r\nOK\r\n",false,e)&&e.text.empty()&&e.dcs==0);
    ParsedSms g;
    assert(!parseCmgrReply("",true,g)&&!parseCmgrReply("+CMS ERROR: 321\r\n",true,g)&&!parseCmgrReply("+CMGR: junk\r\nbody\r\nOK",true,g)&&!parseCmgrReply("+CMGR: 0,\"x\"\r\n",true,g)&&!parseCmgrReply("\x01\x02\"\"\"",true,g));
  }
  // fuzz: never crash
  {
    const char *junk[]={"+CMGL:","+CMGL: 1,\"","+CMGL: 1,\"a\",\"b\",\"c\",\"d","+CMGR: \"","+COPS: 0,0,\"","+CEREG: ,,,,,","+CEREG: 1,\"","+CME ERROR:","+CMTI:\"","\"\"\"\"",",,,,","+CSQ: 99999999999999999999,1"};
    for(const char *j:junk){int x,y;string s;ParsedSms m;parseCsq(j,x,y);parseReg(j,"+CEREG",x,y);parseCops(j,s,x);parseCpin(j);parseCmeError(j,x);parseCmti(j,s,x);parseCmgs(j,x);parseCmglReply(j,true);parseCmglReply(j,false);parseCmgrReply(j,true,m);parseModel(j);modelLabel(j);smsTimeLabel(j);utf8FromUcs2Hex(j,s);ucs2Hex(j);smsUnits(j);}
    int x,y;assert(!parseCsq("+CSQ: 99999999999999999999,1",x,y));
  }
  std::cout<<"PASS: modem_at AT parsers and SMS encoders"<<"\n";
}
