// Self-test app (section 60) and the serial "selftest" command: runs the hardware checks one step per main loop pass (services/selftest.h). Included by main.cpp.
#include <esp_heap_caps.h>
#include <cstdarg>
#include "services/selftest.h"
#include "ui/selftest_layout.h"
namespace sft=selftest;
enum SfIndex {SfRadio,SfGps,SfUbx,SfImu,SfModem,SfSd,SfCodec,SfSpeaker,SfMic,SfHaptics,SfBuzz,SfKeyboard,SfKeys,SfTouch,SfTap,SfFuel,SfCharger,SfExpander,SfPsram,SfFlash,SfHeap,SfEpd,SfWifi,SfBle,SfCount};
static bool sfBuilt=false,sfEcho=false,sfOptions=false,sfKeyHit=false,sfTapHit=false;static int sfFocus=0,sfButton=-1,sfFirst=0,sfOptFocus=0,sfStage=0;
static uint32_t sfA=0,sfB=0,sfPaintedAt=0,sfSignature=0,sfToastUntil=0;static std::string sfNote,sfToast;
static std::string sfFmt(const char *format,...){char text[160];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);return text;}
static bool sfAck(uint8_t address){Wire.beginTransmission(address);return Wire.endTransmission()==0;}
static bool sfRead(uint8_t address,uint8_t reg,uint8_t *bytes,uint8_t n){Wire.beginTransmission(address);Wire.write(reg);if(Wire.endTransmission(false)!=0||Wire.requestFrom(address,n)!=n)return false;for(int i=0;i<n;i++)bytes[i]=Wire.read();return true;}
static bool sfVisible(){return !deviceLocked&&screen==Screen::Section&&section==mm::SelfTestSection;}
// Every check is one function: Start prepares (and may already finish), Poll is called each pass until it returns a result, Abort releases what it holds.
static sft::Outcome sfRadioCheck(sft::Phase p,uint32_t el){
 if(p==sft::Phase::Abort)return {};
 if(p==sft::Phase::Start){if(!matter::snapshot().ready)return sft::fail("radio not initialised");auto d=matter::radioDiag();if(d.deviceErrors)return sft::fail(sfFmt("device errors 0x%04x (serial: radio clear errors)",d.deviceErrors));sfNote=sfFmt("TCXO %.1f V, boost %d, errors 0",d.tcxo,d.boosted);}
 float dbm=0;int floorDbm=0;auto got=matter::radioSampleRssi(dbm,floorDbm);
 if(got==matter::RssiSample::Ok)return sft::rssiSane(dbm)?sft::pass(sfFmt("RSSI %.0f dBm, floor %d dBm, %s",dbm,floorDbm,sfNote.c_str())):sft::fail(sfFmt("RSSI %.1f dBm out of range",dbm));
 if(got==matter::RssiSample::Offline)return sft::fail("radio is off or not receiving");
 if(got==matter::RssiSample::Suspended)return sft::pass("chip answers, radio in use by the companion link, "+sfNote);
 if(el>=2000)return sft::pass(sfFmt("chip answers, RSSI busy (floor %d dBm), %s",floorDbm,sfNote.c_str()));
 return {};
}
static sft::Outcome sfGpsCheck(sft::Phase p,uint32_t el){
 if(p==sft::Phase::Abort)return {};
 auto l=device::locationState();
 if(!l.available)return sft::fail("GPS power control (XL9555) unavailable");
 if(!l.enabled)return sft::skip("GPS is off: Settings > GPS");
 if(l.nmeaValid&&l.nmeaAgeMs<3000)return sft::pass(sfFmt("powered, %lu sentences (%lu bad), %u sats, %s",(unsigned long)l.nmeaValid,(unsigned long)l.nmeaBad,unsigned(l.satellites),l.fix?"fix":"no fix yet"));
 if(el>=10000)return sft::fail(l.serialBytes?sfFmt("%lu bytes but no valid NMEA: check the baud rate",(unsigned long)l.serialBytes):std::string("no data from the receiver: power or wiring"));
 return {};
}
// MON-VER through the read-only probe; a version older than this check is not accepted.
static sft::Outcome sfUbxCheck(sft::Phase p,uint32_t el){
 if(p==sft::Phase::Abort)return {};
 auto l=device::locationState();if(!l.available||!l.enabled)return sft::skip("GPS is off: Settings > GPS");
 if(p==sft::Phase::Start){sfA=device::locationProbe();return {};}
 if(!sfA){sfA=device::locationProbe();if(!sfA&&el>=2000)return sft::fail("receiver probe is busy");return {};}
 std::string d=device::locationReceiverDiagnostic();size_t at=d.find("version_age_ms=");
 if(at!=std::string::npos){uint32_t age=uint32_t(strtoul(d.c_str()+at+15,nullptr,10));size_t sp=d.find(' ',at),nl=d.find('\n',at);if(age<=el+500&&sp!=std::string::npos&&sp<nl)return sft::pass(d.substr(sp+1,nl-sp-1));}
 if(d.compare(0,9,"pending=0")==0)return sft::fail("no MON-VER reply from the receiver");
 return {};
}
static sft::Outcome sfImuCheck(sft::Phase p,uint32_t el){
 if(p==sft::Phase::Abort)return {};
 auto m=device::motionState();
 if(!m.enabled)return sft::skip("Motion is off: Settings > Motion / compass");
 using P=device::MotionPhase;
 if(m.phase==P::Absent)return sft::fail(std::string("BHI260AP not detected: ")+m.error);
 if(m.phase==P::Failed)return sft::fail(std::string("BHI260AP failed: ")+m.error);
 if(m.phase==P::Ready){if(m.accelFresh)return sft::pass(sfFmt("firmware loaded, accel %.2f %.2f %.2f g",m.ax/9.81f,m.ay/9.81f,m.az/9.81f));if(el>=4000)return sft::fail("ready but no fresh accelerometer samples");return {};}
 if(el>=12000)return sft::fail(sfFmt("still %s",m.phase==P::Loading?"loading firmware":"powering up"));
 return {};
}
static sft::Outcome sfModemCheck(sft::Phase p,uint32_t el){
 if(p==sft::Phase::Abort)return {};
 if(!device::modemEnabled())return sft::skip("Cellular modem is off: Settings > Cellular");
 auto m=device::modemStatus();using P=device::ModemPhase;
 if(m.phase==P::Failed)return sft::fail(m.message.empty()?"modem not responding to AT":m.message);
 if(m.phase==P::Off||m.phase==P::Powering||m.phase==P::Starting||m.model.empty()){if(el>=30000)return sft::fail("modem did not answer AT");return {};}
 std::string sim=m.simReady?"SIM ready":m.phase==P::NoSim?"no SIM":m.phase==P::PinNeeded?"SIM PIN needed":m.phase==P::PukNeeded?"SIM locked (PUK)":"SIM state unknown";
 std::string signal=m.csq==99?"no signal reading":sfFmt("CSQ %d (%d/4 bars)",m.csq,m.bars);
 return sft::pass(m.model+", "+sim+", "+signal+(m.operatorName.empty()?"":", "+m.operatorName));
}
static sft::Outcome sfSdCheck(sft::Phase p,uint32_t){
 if(p!=sft::Phase::Start)return {};
 if(!sdReady)return sft::fail(sdProblem.c_str());
 const char *path="/selftest.tmp";const int blocks=8,size=512;uint8_t buffer[512];uint32_t t0=millis(),wrote=0;
 auto fill=[&](int block){for(int i=0;i<size;i++)buffer[i]=uint8_t(i*31+7+block*13);};
 File f=SD.open(path,"w");if(!f)return sft::fail("cannot create /selftest.tmp");
 bool ok=true;for(int b=0;b<blocks;b++){fill(b);wrote=sft::crc32(buffer,size,wrote);ok=ok&&f.write(buffer,size)==size_t(size);}
 f.flush();f.close();if(!ok){SD.remove(path);return sft::fail("write error on /selftest.tmp");}
 f=SD.open(path,"r");if(!f){SD.remove(path);return sft::fail("cannot read back /selftest.tmp");}
 uint32_t back=0;size_t got=0;if(f.size()==size_t(blocks*size)){int n;while((n=f.read(buffer,size))>0){back=sft::crc32(buffer,size_t(n),back);got+=size_t(n);}}
 f.close();SD.remove(path);
 if(got!=size_t(blocks*size)||back!=wrote)return sft::fail(sfFmt("read-back mismatch (%u of %d bytes, CRC %08lx vs %08lx)",unsigned(got),blocks*size,(unsigned long)back,(unsigned long)wrote));
 uint64_t total=SD.totalBytes(),used=SD.usedBytes();
 return sft::pass(sfFmt("%s, %s free, 4 KB write+verify CRC %08lx in %lu ms",sft::sizeText(total).c_str(),sft::sizeText(total>used?total-used:0).c_str(),(unsigned long)wrote,(unsigned long)(millis()-t0)));
}
static sft::Outcome sfCodecCheck(sft::Phase p,uint32_t){
 if(p==sft::Phase::Abort)return {};
 auto a=device::audioState();if(!a.ready)return sft::fail("ES8311 not initialised: "+std::string(a.message.c_str()));
 if(a.busy)return {};// the codec is in use: its registers are read when it is free
 uint8_t id[2];// one read per register: the ES8311 does not auto-increment, a 2-byte burst from 0xFD returns FF as the second byte
 if(!sfRead(BOARD_I2C_ADDR_ES8311,0xFD,id,1)||!sfRead(BOARD_I2C_ADDR_ES8311,0xFE,id+1,1))return sft::fail("ES8311 does not answer at 0x18");
 return id[0]==0x83&&id[1]==0x11?sft::pass("ES8311 at 0x18, chip id 83 11"):sft::fail(sfFmt("unexpected chip id %02x %02x",id[0],id[1]));
}
static sft::Outcome sfSpeakerCheck(sft::Phase p,uint32_t){
 if(p==sft::Phase::Abort){if(device::audioBusy())device::audioCancel();return {};}
 if(p==sft::Phase::Start){auto a=device::audioState();if(!a.ready)return sft::fail("ES8311 not initialised");if(a.volume==0)return sft::skip("volume is 0: Settings > Sound");if(a.busy)return sft::fail("audio is busy");sfA=a.playbacks;if(!device::audioPreview(a.messageTone))return sft::fail("playback did not start");return {};}
 if(device::audioBusy())return {};
 auto a=device::audioState();if(a.lastError)return sft::fail(sfFmt("playback error %d",a.lastError));if(a.playbacks==sfA)return sft::fail("no playback was recorded");
 return sft::pass(sfFmt("tone played, %lu bytes at volume %d",(unsigned long)a.txBytes,a.volume));
}
static sft::Outcome sfMicCheck(sft::Phase p,uint32_t el){
 if(p==sft::Phase::Abort){if(device::audioBusy())device::audioCancel();return {};}
 if(p==sft::Phase::Start){auto a=device::audioState();if(!a.ready)return sft::fail("ES8311 not initialised");if(a.busy)return sft::fail("audio is busy");if(!device::audioProbeRecord())return sft::fail("capture could not start (audio memory?)");sfStage=0;return {};}
 if(sfStage==0){if(el>=device::DICTATION_WARMUP_MS+700){device::audioStop();sfStage=1;}return {};}// the first second is codec warm-up
 if(device::audioBusy())return {};
 auto a=device::audioState();if(a.lastError)return sft::fail(sfFmt("capture error %d",a.lastError));
 return a.peak>0?sft::pass(sfFmt("capture ok, peak %d (%d%%), %lu samples",a.peak,a.level,(unsigned long)a.samples)):sft::fail("microphone reads silence (peak 0)");
}
static sft::Outcome sfHapticsCheck(sft::Phase p,uint32_t){
 if(p!=sft::Phase::Start)return {};
 if(!hapticReady)return sft::fail("DRV2605 not found at 0x5A");
 uint8_t status=haptic.readRegister8(DRV2605_REG_STATUS);const char *chip=sft::hapticChip(status);
 return chip?sft::pass(sfFmt("%s at 0x5A, status 0x%02x",chip,status)):sft::fail(sfFmt("status 0x%02x is not a DRV260x",status));
}
static sft::Outcome sfBuzzCheck(sft::Phase p,uint32_t){
 if(p==sft::Phase::Abort){if(sfStage==1)stopVibration();sfStage=0;return {};}
 if(p==sft::Phase::Start){if(!hapticReady)return sft::fail("DRV2605 not found at 0x5A");if(!hapticEnabled||vibrationPercent==0)return sft::skip("vibration is off: Settings > Lights / vibration");if(!messageHaptic(true))return sft::fail("the motor did not start");sfB=activeVibration;sfStage=1;return {};}
 if(hapticActive)return {};
 sfStage=0;return sft::pass(sfFmt("buzzed: %s at %u%%",device::VIBRATIONS[sfB].name,unsigned(vibrationPercent)));
}
static sft::Outcome sfKeyboardCheck(sft::Phase p,uint32_t){
 if(p!=sft::Phase::Start)return {};
 if(!keyboardReady)return sft::fail("TCA8418 not initialised");
 if(!sfAck(BOARD_I2C_ADDR_KEYBOARD))return sft::fail("TCA8418 does not answer at 0x34");
 return sft::pass(sfFmt("TCA8418 at 0x34, CFG 0x%02x, %lu key events so far",keyboard.readRegister(TCA8418_REG_CFG),keyboardEvents));
}
static sft::Outcome sfKeysCheck(sft::Phase p,uint32_t){
 if(p==sft::Phase::Abort)return {};
 if(p==sft::Phase::Start){sfA=uint32_t(keyboardEvents);sfKeyHit=false;}
 if(sfKeyHit||uint32_t(keyboardEvents)!=sfA)return sft::pass("key event received");
 return sft::wait("Press any key on the keyboard");
}
static sft::Outcome sfTouchCheck(sft::Phase p,uint32_t){
 if(p!=sft::Phase::Start)return {};
 if(!touchBackend)return sft::fail("touch controller not initialised");
 if(!sfAck(BOARD_I2C_ADDR_TOUCH))return sft::fail("CST328 does not answer at 0x1A");
 return sft::pass("CST328 at 0x1A through the Hynitron driver");
}
static sft::Outcome sfTapCheck(sft::Phase p,uint32_t){
 if(p==sft::Phase::Abort)return {};
 if(p==sft::Phase::Start){sfA=uint32_t(touchEvents);sfTapHit=false;}
 if(sfTapHit||(!sfVisible()&&uint32_t(touchEvents)!=sfA))return sft::pass("tap received");// run from the serial port: any touch counts
 return sft::wait("Tap the square");
}
static sft::Outcome sfFuelCheck(sft::Phase p,uint32_t){
 if(p!=sft::Phase::Start)return {};
 auto b=device::batteryState();
 if(!b.gauge)return sft::fail("BQ27220 does not answer at 0x55");
 if(!sft::batterySane(b.voltage))return sft::fail(sfFmt("implausible cell voltage %d mV",b.voltage));
 return sft::pass(sfFmt("%d mV, %d%%%s",b.voltage,b.percent,b.charging?", charging":""));
}
static sft::Outcome sfChargerCheck(sft::Phase p,uint32_t){
 if(p!=sft::Phase::Start)return {};
 auto source=device::batteryPowerSource();
 if(source==device::PowerSource::Unknown)return sft::fail("SY6970 does not answer at 0x6A");
 return sft::pass(sfFmt("SY6970 at 0x6A, running on %s%s",source==device::PowerSource::USB?"USB":"battery",device::batteryState().charging?", charging":""));
}
static sft::Outcome sfExpanderCheck(sft::Phase p,uint32_t){
 if(p!=sft::Phase::Start)return {};
 uint8_t in[2];if(!expanderReady)return sft::fail("XL9555 not initialised");
 if(!sfRead(BOARD_I2C_ADDR_XL9555,0x00,in,2))return sft::fail("XL9555 does not answer at 0x20");
 return sft::pass(sfFmt("XL9555 at 0x20, inputs 0x%02x%02x",in[1],in[0]));
}
static sft::Outcome sfPsramCheck(sft::Phase p,uint32_t){
 if(p!=sft::Phase::Start)return {};
 uint64_t size=ESP.getPsramSize(),free=ESP.getFreePsram();
 return size>=7u*1024*1024?sft::pass(sft::sizeText(size)+", "+sft::sizeText(free)+" free"):sft::fail(size?"only "+sft::sizeText(size)+" of PSRAM (8 MB expected)":std::string("no PSRAM found"));
}
static sft::Outcome sfFlashCheck(sft::Phase p,uint32_t){
 if(p!=sft::Phase::Start)return {};
 uint64_t size=ESP.getFlashChipSize();
 return size>=16u*1024*1024?sft::pass(sft::sizeText(size)+" flash"):sft::fail(sft::sizeText(size)+" flash (16 MB expected)");
}
static sft::Outcome sfHeapCheck(sft::Phase p,uint32_t){
 if(p!=sft::Phase::Start)return {};
 uint32_t free=ESP.getFreeHeap(),block=uint32_t(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
 return free>=50000?sft::pass(sfFmt("%lu KB free, largest block %lu KB",(unsigned long)(free/1024),(unsigned long)(block/1024))):sft::fail(sfFmt("only %lu KB of heap free",(unsigned long)(free/1024)));
}
// Full refresh of a checkerboard (both the waveform and the busy line are exercised), then the screen comes back.
static void sfEpdRestore(){sfStage=0;forceFullRefresh=true;render();}
static sft::Outcome sfEpdCheck(sft::Phase p,uint32_t){
 if(p==sft::Phase::Abort){if(sfStage==1)sfEpdRestore();return {};}
 if(p==sft::Phase::Start){
  GFXcanvas1 pattern(mm::W,mm::H);pattern.fillScreen(mm::BG);for(int y=0;y<mm::H;y+=16)for(int x=0;x<mm::W;x+=16)if(((x+y)/16)%2==0)pattern.fillRect(x,y,16,16,mm::FG);
  mm::RefreshPlan plan;plan.changed=true;plan.full=true;uint32_t t0=millis();panelPainting=true;mm::paintFramebuffer(epd.epd2,pattern.getBuffer(),plan);panelPainting=false;refreshTracker.commit(pattern.getBuffer(),plan);
  sfA=millis()-t0;sfB=millis();sfStage=1;return {};
 }
 if(sfStage==1&&uint32_t(millis()-sfB)>=1500){sfEpdRestore();return sft::pass(sfFmt("full refresh in %lu ms, screen restored",(unsigned long)sfA));}
 return {};
}
static sft::Outcome sfWifiCheck(sft::Phase p,uint32_t el){
 if(p==sft::Phase::Abort)return {};
 auto c=device::connectivityStatus();
 if(!c.wifi)return sft::skip("Wi-Fi is off: Quick settings or Settings > Wi-Fi");
 if(c.connected)return sft::pass(sfFmt("radio up, connected to %s at %d dBm",c.ssid.c_str(),c.rssi));
 if(p==sft::Phase::Start){sfA=device::wifiScanCount();device::scan();return {};}
 if(device::wifiScanCount()!=sfA)return sft::pass(sfFmt("radio up, scan found %u networks",unsigned(c.networkCount)));
 if(el>=12000)return sft::fail(std::string("scan gave no result: ")+c.message.c_str());
 return {};
}
static sft::Outcome sfBleCheck(sft::Phase p,uint32_t el){
 if(p==sft::Phase::Abort)return {};
 auto c=device::connectivityStatus();
 if(!c.ble)return sft::skip("Bluetooth is off: Settings > Bluetooth");
 if(c.bleReady)return sft::pass(std::string("stack up, advertising as ")+device::bleCompanionName().c_str());
 if(el>=3000)return device::radioLinkStatus().phase!=device::RadioLinkPhase::Off?sft::skip("the companion link has Bluetooth"):sft::fail("Bluetooth stack did not start");
 return {};
}
struct SfCheck {sft::Spec spec;sft::Outcome (*run)(sft::Phase,uint32_t);};
// Order of SfIndex. Specs: name (serial), label, timeout ms, interactive, output (sound / vibration).
static const SfCheck sfChecks[SfCount]={
 {{"sx1262","SX1262 radio",4000,false,false},sfRadioCheck},{{"gps","GPS NMEA",13000,false,false},sfGpsCheck},{{"ubx","GPS UBX MON-VER",9000,false,false},sfUbxCheck},
 {{"imu","BHI260AP motion",14000,false,false},sfImuCheck},{{"modem","LTE modem",32000,false,false},sfModemCheck},{{"sd","SD card",6000,false,false},sfSdCheck},
 {{"codec","ES8311 codec",3000,false,false},sfCodecCheck},{{"speaker","Speaker tone",5000,false,true},sfSpeakerCheck},{{"mic","Microphone",6000,false,false},sfMicCheck},
 {{"haptics","DRV2605 haptics",2000,false,false},sfHapticsCheck},{{"buzz","Vibration motor",5000,false,true},sfBuzzCheck},
 {{"keyboard","TCA8418 keyboard",2000,false,false},sfKeyboardCheck},{{"keys","Press a key",10000,true,false},sfKeysCheck},
 {{"touch","CST328 touch",2000,false,false},sfTouchCheck},{{"tap","Tap the target",10000,true,false},sfTapCheck},
 {{"fuel","BQ27220 fuel gauge",2000,false,false},sfFuelCheck},{{"charger","SY6970 charger",2000,false,false},sfChargerCheck},{{"xl9555","XL9555 expander",2000,false,false},sfExpanderCheck},
 {{"psram","PSRAM",2000,false,false},sfPsramCheck},{{"flash","Flash",2000,false,false},sfFlashCheck},{{"heap","Free heap",2000,false,false},sfHeapCheck},
 {{"epd","E-paper refresh",8000,false,false},sfEpdCheck},{{"wifi","Wi-Fi radio",14000,false,false},sfWifiCheck},{{"ble","Bluetooth radio",5000,false,false},sfBleCheck}};
static sft::Runner sfRunner;
static sft::Outcome sfStep(int i,sft::Phase p,uint32_t el){return sfChecks[i].run(p,el);}
static void sfSaveOptions(){device::prefPutInt("mmselftest","int",sfRunner.options.interactive?1:0);device::prefPutInt("mmselftest","out",sfRunner.options.output?1:0);}
static void sfEnsure(){
 if(!sfBuilt){sfBuilt=true;for(auto &c:sfChecks)sfRunner.add(c.spec);}
 sfRunner.options.interactive=device::prefGetInt("mmselftest","int",1)!=0;sfRunner.options.output=device::prefGetInt("mmselftest","out",1)!=0;
}
static void sfSay(const std::string &text){sfToast=text;sfToastUntil=millis()+4000;}
static void sfShowFocus(){sfFocus=constrain(sfFocus,0,SfCount-1);if(sfFocus<sfFirst)sfFirst=sfFocus;if(sfFocus>=sfFirst+mm::SelfTestRows)sfFirst=sfFocus-mm::SelfTestRows+1;}
static void sfCancel(){sfRunner.cancel(sfStep);}
// The text report goes to the SD root, listed by Files.
static bool sfExport(std::string &name,std::string &why){
 if(!sdReady){why="SD card not available";return false;}
 time_t now=time(nullptr);bool clock=now>=MM_EPOCH_MIN;struct tm local={};if(clock)localtime_r(&now,&local);
 name=sft::reportName(clock,local.tm_year+1900,local.tm_mon+1,local.tm_mday,local.tm_hour,local.tm_min,local.tm_sec,millis()/1000);
 char stamp[24]="";if(clock)strftime(stamp,sizeof(stamp),"%Y-%m-%d %H:%M:%S",&local);
 std::string text=sft::report(sfRunner,stamp,MM_BUILD_VERSION);String path=String("/")+name.c_str();
 File f=SD.open(path,"w");if(!f){why="cannot open the file";return false;}
 bool ok=f.write((const uint8_t*)text.data(),text.size())==text.size();f.flush();f.close();if(!ok){SD.remove(path);why="SD write error";return false;}
 return true;
}
static void sfExportButton(){std::string name,why;if(sfExport(name,why))sfSay("Saved "+name);else sfSay("Export failed: "+why);}
static void sfRunAllButton(){if(sfRunner.busy()){sfCancel();sfSay("Stopped");}else{sfEcho=false;sfRunner.runAll();}}
static void sfRunFocused(){if(sfRunner.busy()){sfSay("A run is going: STOP it first");return;}sfEcho=false;sfRunner.runOne(sfFocus);}
static void sfActivate(int button){if(button==0)sfRunAllButton();else if(button==1)sfRunFocused();else sfExportButton();}
static void sfToggleOption(int row){if(row==0)sfRunner.options.interactive=!sfRunner.options.interactive;else sfRunner.options.output=!sfRunner.options.output;sfSaveOptions();}
void selfTestOpen(){sfEnsure();sfOptions=false;sfFocus=sfFirst=0;sfButton=-1;sfOptFocus=0;sfToast.clear();sfToastUntil=0;}
// Leaving the app stops a run started from it (the serial command keeps going).
void selfTestLeft(){if(!sfEcho)sfCancel();sfOptions=false;}
bool selfTestBack(){if(sfOptions){sfOptions=false;render();return true;}if(sfRunner.busy()&&!sfEcho){sfCancel();sfSay("Stopped");render();return true;}return false;}
// The running interactive check, 0 none / 1 key / 2 tap.
static int sfPromptKind(){int i=sfRunner.current();if(i<0||sfRunner.entries[i].status!=sft::Status::Input)return 0;return i==SfKeys?1:i==SfTap?2:0;}
bool selfTestKey(char c){
 if(c=='\b')return false;
 int prompt=sfPromptKind();
 if(prompt==1){sfKeyHit=true;return true;}
 if(prompt==2)return c!='a';// the tap check ignores keys, A still goes back
 bool left=c=='a'||c=='h',right=c=='d'||c=='l',up=c=='w'||c=='k',down=c=='s'||c=='j',act=c=='\n'||c==' ';
 if(sfOptions){
  if(c=='o')sfOptions=false;else if(up||down)sfOptFocus=(sfOptFocus+(up?mm::SelfTestOptionRows-1:1))%mm::SelfTestOptionRows;else if(left||right||act)sfToggleOption(sfOptFocus);
  render();return true;
 }
 if(c=='o')sfOptions=true,sfOptFocus=0;
 else if(up){if(sfButton>=0)sfButton=-1;else sfFocus--;}
 else if(down){if(sfButton<0){if(sfFocus>=SfCount-1)sfButton=0;else sfFocus++;}}
 else if(left){if(sfButton>0)sfButton--;else return false;}// A on the first control goes back
 else if(right){if(sfButton<0)sfButton=1;else if(sfButton<2)sfButton++;}
 else if(act){if(sfButton<0)sfRunFocused();else sfActivate(sfButton);}
 else return false;
 sfShowFocus();render();return true;
}
bool selfTestTap(int x,int y){
 if(mm::BackButton.contains(x,y))return false;
 if(sfPromptKind()==2){if(mm::SelfTestTarget.contains(x,y))sfTapHit=true;return true;}
 if(sfPromptKind()==1)return true;
 if(mm::ComposeButton.contains(x,y)){sfOptions=!sfOptions;sfOptFocus=0;render();return true;}
 if(sfOptions){
  for(int i=0;i<mm::SelfTestOptionRows;i++)if(mm::selfTestOptionRow(i).contains(x,y)){sfOptFocus=i;sfToggleOption(i);render();return true;}
  if(!mm::SelfTestPopup.contains(x,y))sfOptions=false;
  render();return true;
 }
 for(int i=0;i<mm::SelfTestRows&&sfFirst+i<SfCount;i++)if(mm::selfTestRow(i).contains(x,y)){sfFocus=sfFirst+i;sfButton=-1;render();return true;}
 for(int i=0;i<mm::SelfTestButtons;i++)if(mm::selfTestButton(i).contains(x,y)){sfButton=i;sfActivate(i);render();return true;}
 return false;
}
void selfTestScroll(int dy){if(sfOptions||sfPromptKind())return;sfFirst=constrain(sfFirst+(dy<0?mm::SelfTestRows:-mm::SelfTestRows),0,std::max(0,SfCount-mm::SelfTestRows));sfFocus=constrain(sfFocus,sfFirst,sfFirst+mm::SelfTestRows-1);sfButton=-1;render();}
void selfTestCompose(mm::State &v){
 sfEnsure();auto &t=v.selftest;t.rows.clear();for(auto &e:sfRunner.entries)t.rows.push_back({e.spec.label,e.detail,int(e.status),e.ms});
 auto c=sfRunner.counts();t.pass=c.pass;t.fail=c.fail;t.skip=c.skip;t.notRun=c.idle;
 t.first=sfFirst;t.focus=sfFocus;t.button=sfButton;t.optionFocus=sfOptFocus;t.busy=sfRunner.busy();t.options=sfOptions;t.interactive=sfRunner.options.interactive;t.output=sfRunner.options.output;
 int cur=sfRunner.current();
 if(t.busy)t.line=cur>=0?sfFmt("Running %d/%d: %s",sfRunner.batchDone+1,sfRunner.batchTotal,sfRunner.entries[cur].spec.label):"Starting...";
 else t.line=c.idle==SfCount?sfFmt("Ready: %d checks",SfCount):c.fail?sfFmt("Done: %d failed",c.fail):c.idle?sfFmt("%d not run",c.idle):"Done: nothing failed";
 t.prompt=sfPromptKind();if(t.prompt){auto &e=sfRunner.entries[cur];t.promptTitle=e.spec.label;t.promptText=e.detail;uint32_t spent=millis()-sfRunner.started();t.promptSecs=spent>=e.spec.timeoutMs?0:int((e.spec.timeoutMs-spent+999)/1000);}
 if(sfToastUntil&&int32_t(sfToastUntil-millis())>0)t.toast=sfToast;
}
// Called every main loop pass: one step of the running check, the serial echo, and a paced repaint while the app is on screen.
void selfTestTick(){
 if(!sfBuilt)return;
 uint32_t now=millis();bool ended=sfRunner.tick(now,sfStep);
 for(int i;(i=sfRunner.takeDone())>=0;)if(sfEcho)Serial.println(sft::serialLine(sfRunner.entries[i]).c_str());
 if(sfEcho&&(ended||!sfRunner.busy())){sfEcho=false;Serial.println(sft::serialSummary(sfRunner.counts()).c_str());}
 if(sfToastUntil&&int32_t(now-sfToastUntil)>=0){sfToastUntil=0;sfToast.clear();if(sfVisible()&&!sfOptions)markRadioDirty();}
 if(!sfVisible()||sfOptions||(sfStage==1&&sfRunner.current()==SfEpd))return;
 int cur=sfRunner.current();uint32_t signature=uint32_t(sfRunner.batchDone)*4096+uint32_t(cur+1)*64+(cur>=0?uint32_t(sfRunner.entries[cur].status):0)*8+(sfPromptKind()?uint32_t((sfRunner.entries[cur].spec.timeoutMs-std::min<uint32_t>(sfRunner.entries[cur].spec.timeoutMs,now-sfRunner.started()))/1000)%8:0);
 if(signature!=sfSignature&&now-sfPaintedAt>=1000){sfSignature=signature;sfPaintedAt=now;markRadioDirty();}
}
// Serial: "selftest" (all), "selftest <name>", "selftest list|stop|export|status|interactive on|off|output on|off".
bool selfTestSerial(const String &s){
 if(s!="selftest"&&!s.startsWith("selftest "))return false;
 sfEnsure();std::string arg=s.length()>9?std::string(s.c_str()+9):std::string();
 if(arg=="list"){for(auto &e:sfRunner.entries)Serial.printf("SELFTEST_CHECK name=%s label=\"%s\" interactive=%d output=%d\n",e.spec.name,e.spec.label,e.spec.interactive,e.spec.output);return true;}
 if(arg=="stop"){sfCancel();Serial.println("SELFTEST_STOPPED");return true;}
 if(arg=="status"){auto c=sfRunner.counts();Serial.printf("SELFTEST_STATUS busy=%d pass=%d fail=%d skip=%d notrun=%d interactive=%d output=%d\n",sfRunner.busy(),c.pass,c.fail,c.skip,c.idle,sfRunner.options.interactive,sfRunner.options.output);return true;}
 if(arg=="export"){std::string name,why;bool ok=sfExport(name,why);Serial.printf("SELFTEST_EXPORT ok=%d file=%s%s%s\n",ok,ok?name.c_str():"",ok?"":" reason=",ok?"":why.c_str());return true;}
 if(arg.compare(0,12,"interactive ")==0||arg.compare(0,7,"output ")==0){bool on=arg.size()>=3&&arg.compare(arg.size()-3,3,"off")!=0;if(arg[0]=='i')sfRunner.options.interactive=on;else sfRunner.options.output=on;sfSaveOptions();Serial.printf("SELFTEST_OPTIONS interactive=%d output=%d\n",sfRunner.options.interactive,sfRunner.options.output);return true;}
 if(sfRunner.busy()){Serial.println("SELFTEST_BUSY");return true;}
 if(arg.empty()){sfEcho=true;sfRunner.runAll();return true;}
 int index=sfRunner.find(arg);if(index<0){Serial.printf("SELFTEST_ERROR unknown=%s\n",arg.c_str());return true;}
 sfEcho=true;sfRunner.runOne(index);return true;
}
