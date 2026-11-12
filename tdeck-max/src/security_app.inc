// Settings > Security (58): lock PIN (4-8 digits, salted PBKDF2 hash in NVS "mmsec"), wrong-PIN backoff that survives a reboot, "ask
// after" timing, hide message previews on the lock screen, block USB data commands while locked. The PIN pad is drawn by the
// renderer over the lock screen (after the two-tap unlock gesture) and inside this page (set / change / remove).
// Logic: services/pin_policy.h, layout: ui/security_layout.h, drawing: ui/security_renderer.inc. Included by main.cpp after cellular_app.inc.
// NVS "mmsec": on, req, hide, usb (int), fails (uint, the wrong PINs in a row), salt (16), hash (32), iter (uint).
#include "services/pin_policy.h"
static device::PinRecord secRec;static device::PinGuard secGuard;static device::PinEntry secEntry;static device::PinSetup secSetup;
static bool secPin=false,secHide=false,secUsb=false,secBoot=false;static int secRequire=0,secFocus=0;
static uint32_t secLockedAt=0,secPadAt=0,secWrongUntil=0;static std::string secNote;static int secWaitPainted=-1;
constexpr uint32_t SecPadIdleMs=30000,SecWrongMs=1500;
static bool secPage(){return screen==Screen::Section&&section==mm::SecuritySection;}
static bool secSetupOpen(){return secSetup.stage!=device::PinSetup::Idle;}
bool usbBlocked(){return deviceLocked&&secUsb;}
bool lockReplyBlocked(){return secHide||secPin;}
// While locked and blocked only the status lines of the harness answer; nothing that reads messages, keys or files.
bool serialAllowedLocked(const String &s){return s=="status"||s=="lock status"||s=="device status"||s=="radio status"||s=="ui stats"||s=="security status";}
void securityBegin(){
 secHide=device::prefGetInt("mmsec","hide",0)!=0;secUsb=device::prefGetInt("mmsec","usb",0)!=0;secRequire=constrain(device::prefGetInt("mmsec","req",0),0,device::PinRequireCount-1);secPin=false;
 Preferences p;if(device::prefGetInt("mmsec","on",0)&&p.begin("mmsec",true)){if(p.getBytesLength("salt")==16&&p.getBytesLength("hash")==32&&p.getUInt("iter",0)){p.getBytes("salt",secRec.salt,16);p.getBytes("hash",secRec.hash,32);secRec.iterations=p.getUInt("iter",0);secPin=true;}p.end();}
 secGuard.restore(device::prefGetUInt("mmsec","fails",0),millis());secBoot=secPin;// after a restart the PIN is always asked
}
static bool secStorePin(const std::string &pin){
 uint8_t salt[16];for(int i=0;i<16;i+=4){uint32_t r=esp_random();memcpy(salt+i,&r,4);}
 device::PinRecord next=device::pinMake(pin,salt);Preferences p;if(!p.begin("mmsec",false))return false;
 bool ok=p.putBytes("salt",next.salt,16)==16&&p.putBytes("hash",next.hash,32)==32&&p.putUInt("iter",next.iterations)==4&&p.putInt("on",1)==4&&p.putUInt("fails",0)==4;p.end();
 if(ok){secRec=next;secPin=true;secGuard.success();}return ok;
}
static bool secClearPin(){Preferences p;if(!p.begin("mmsec",false))return false;bool ok=p.putInt("on",0)==4&&p.putUInt("fails",0)==4;if(ok){if(p.isKey("salt"))p.remove("salt");if(p.isKey("hash"))p.remove("hash");}p.end();if(ok){secPin=false;secBoot=false;secGuard.success();}return ok;}
// One try: the failure is stored before the check, so cutting the power mid-way still counts it.
static bool pinCheck(const std::string &digits,std::string &why){
 if(!secGuard.allowed(millis())){why="";return false;}
 if(!device::prefPutUInt("mmsec","fails",secGuard.fails+1)){why="Storage error";return false;}
 if(device::pinVerify(secRec,digits)){secGuard.success();device::prefPutUInt("mmsec","fails",0);return true;}
 secGuard.fail(millis());why="";return false;
}
static std::string secWrongText(){if(secGuard.waitMs(millis()))return "";uint32_t left=device::pinTriesLeft(secGuard.fails);return left==1?"Wrong PIN / 1 try left":"Wrong PIN / "+std::to_string(left)+" tries left";}
static void padRepaint(){if(deviceLocked)lockScreenDirty=true;else repaintSettled();}
static std::string padTitle(){using S=device::PinSetup;if(deviceLocked)return "Enter PIN";return secSetup.stage==S::Old?"Current PIN":secSetup.stage==S::New?"New PIN":"Confirm PIN";}
static std::string padHint(){using S=device::PinSetup;if(deviceLocked)return secGuard.fails?std::to_string(secGuard.fails)+(secGuard.fails==1?" wrong try":" wrong tries"):"Unlock MatterMesh";
 return secSetup.stage==S::Old?(secSetup.mode==S::Remove?"To remove the PIN":"To change the PIN"):secSetup.stage==S::New?"4 to 8 digits":"Enter it once more";}
// Keys 1-9 / 0 (or the letters under them), DEL, OK: on the lock screen OK unlocks, in the page it takes the next setup step.
static void padSubmit(){
 const std::string digits=secEntry.digits;secEntry.clear();secWrongUntil=0;
 if(deviceLocked){
  if(!device::pinValid(digits)){secNote="Use 4 to 8 digits";padRepaint();return;}
  std::string why;if(pinCheck(digits,why)){secNote.clear();pinPadOpen=false;secBoot=false;exitLockMode();return;}
  secNote=why.empty()?secWrongText():why;secWrongUntil=millis()+SecWrongMs;padRepaint();return;}
 using S=device::PinSetup;bool oldOk=false;std::string why;
 if(secSetup.stage==S::Old){if(!device::pinValid(digits)){secNote="Use 4 to 8 digits";padRepaint();return;}oldOk=pinCheck(digits,why);}
 const auto mode=secSetup.mode;auto r=secSetup.submit(digits,oldOk);secNote.clear();
 if(r==S::Wrong){secNote=why.empty()?secWrongText():why;secWrongUntil=millis()+SecWrongMs;}
 else if(r==S::Short)secNote="Use 4 to 8 digits";else if(r==S::Mismatch){secNote="PINs differ / start again";secWrongUntil=millis()+SecWrongMs;}
 else if(r==S::Save){bool ok=secStorePin(secSetup.pin);secSetup.cancel();notice=ok?(mode==S::Change?"PIN changed":"PIN set"):"Save failed";}
 else if(r==S::Removed){notice=secClearPin()?"PIN removed":"Save failed";}
 padRepaint();
}
static void pinClosePad(){
 pinPadOpen=false;secEntry.clear();secNote.clear();secWrongUntil=0;unlockTaps.reset();unlockCountdownAt=lockHintUntil=0;device::unlockLightPreview(false);setTouchEnabled(false);restoreLockedCpu();lockScreenDirty=true;render();
}
static void padKey(char c){
 secPadAt=millis();secWrongUntil=0;
 if(c==3){if(deviceLocked)pinClosePad();else{secSetup.cancel();secEntry.clear();secNote.clear();render();}return;}
 if(c=='\n'){padSubmit();return;}
 if(c=='\b'){secEntry.erase();secNote.clear();}
 else if(char d=device::pinKeyDigit(c)){if(secGuard.allowed(millis())){secEntry.add(d);secNote.clear();}}
 else return;
 padRepaint();
}
static void padTap(int x,int y){
 if(mm::PinCancel.contains(x,y)){padKey(3);return;}
 for(int i=0;i<mm::PinKeys;i++)if(mm::pinKeyRect(i).contains(x,y)){padKey(mm::pinKeyChar(i));return;}
}
// ---- lock screen
bool pinNeededNow(){return secPin&&device::pinDue(secRequire,millis()-secLockedAt,secBoot);}
void pinOpenPad(){
 pinPadOpen=true;secEntry.clear();secNote.clear();secWrongUntil=0;secPadAt=millis();unlockTaps.reset();unlockCountdownAt=lockHintUntil=0;keyboardModifiers.reset();deleteHeld=false;secWaitPainted=-1;
 setCpuFrequencyMhz(device::NORMAL_CPU_FREQUENCY);device::unlockLightPreview(true);setTouchEnabled(true);lockScreenDirty=true;render();
}
void securityLocked(){secLockedAt=millis();pinPadOpen=false;secEntry.clear();secNote.clear();}
void securityUnlocked(){pinPadOpen=false;secEntry.clear();secNote.clear();secBoot=false;}
// A key of the lock screen with the pad open, from the keyboard matrix (code) or the serial harness.
void pinLockKeyEvent(int code,bool pressed){
 if(!pressed||code==device::SPACE_KEYCODE)return;int k=code-1;if(k<0||k>=40)return;char ch=keys[k/10][9-k%10];if(ch==3||ch=='\b'||ch=='\n'||device::pinKeyDigit(ch))padKey(ch);
}
void pinLockKey(char c){padKey(c);}
void pinLockTap(int x,int y){secPadAt=millis();padTap(x,y);}
// Countdown repaint once per shown second, the wrong-PIN frame going away, and the pad closing after 30 s of nothing.
void pinLockPoll(){
 uint32_t now=millis();int wait=int((secGuard.waitMs(now)+999)/1000);
 if(wait!=secWaitPainted){secWaitPainted=wait;lockScreenDirty=true;}
 static bool wrongShown=false;bool wrong=secWrongUntil&&int32_t(secWrongUntil-now)>0;if(wrong!=wrongShown){wrongShown=wrong;lockScreenDirty=true;}
 if(uint32_t(now-secPadAt)>=SecPadIdleMs)pinClosePad();
}
// ---- Settings > Security
static void secStore(const char *key,int value){notice=device::prefPutInt("mmsec",key,value)?"Saved":"Save failed";}
static void secAction(int row,int dir){
 using S=device::PinSetup;notice="";
 if(row==0||row==1){secSetup.begin(!secPin?S::Set:row==0?S::Remove:S::Change);secEntry.clear();secNote.clear();secWrongUntil=0;secPadAt=millis();render();return;}
 if(row==2){int next=(secRequire+device::PinRequireCount+(dir<0?-1:1))%device::PinRequireCount;if(device::prefPutInt("mmsec","req",next)){secRequire=next;notice="Ask PIN: "+String(device::pinRequireName(next));}else notice="Save failed";}
 else if(row==3){bool next=!secHide;if(device::prefPutInt("mmsec","hide",next)){secHide=next;notice=next?"Previews hidden":"Previews shown";}else notice="Save failed";}
 else if(row==4){bool next=!secUsb;if(device::prefPutInt("mmsec","usb",next)){secUsb=next;notice=next?"USB data blocked while locked":"USB data allowed";}else notice="Save failed";}
 render();
}
void securityCompose(mm::State &v){
 auto &s=v.sec;s.on=secPin;s.hide=secHide;s.usb=secUsb;s.require=secRequire;s.requireName=device::pinRequireName(secRequire);s.focus=secFocus;
 if(secPage()){
  s.status=secPin?"PIN is set":"No PIN";s.detail=(notice.length()?std::string(notice.c_str())+"\n":std::string())+"Wrong PIN: 4 free tries,\nthen 30 s, doubling every try.";
  if(secGuard.fails&&secPin)s.detail+="\nWrong in a row: "+std::to_string(secGuard.fails);
 }
 if(deviceLocked&&secHide&&v.lockHasMessage){v.receivedName="New message";v.receivedPreview="Unlock to read it";v.receivedChannel="Mesh messages";v.receivedHops=-1;}
 bool pad=(deviceLocked&&pinPadOpen)||(!deviceLocked&&secPage()&&secSetupOpen());
 if(pad){auto &p=v.pin;p.active=true;p.digits=int(secEntry.digits.size());p.wrong=int32_t(secWrongUntil-millis())>0&&secWrongUntil;p.waitSeconds=int((secGuard.waitMs(millis())+999)/1000);p.title=padTitle();p.hint=padHint();p.notice=secNote;}
}
bool securityPadKey(char c){if(deviceLocked||!secPage()||!secSetupOpen())return false;padKey(c);return true;}
bool securityKey(char c){
 if(!secPage())return false;
 if(c=='\b'||c=='a')return false;// Back
 if(c=='j'||c=='s'){secFocus=(secFocus+1)%mm::SecRows;render();return true;}
 if(c=='k'||c=='w'){secFocus=(secFocus+mm::SecRows-1)%mm::SecRows;render();return true;}
 if(c=='h'){if(secFocus==2)secAction(2,-1);else return false;return true;}
 if(c=='l'||c=='d'||c=='\n'||c==' '){secAction(secFocus,1);return true;}
 return false;
}
bool securityTap(int x,int y){
 if(!secPage())return false;
 if(mm::BackButton.contains(x,y))return false;
 if(secSetupOpen()){padTap(x,y);return true;}
 for(int row=0;row<mm::SecRows;row++){auto r=mm::secRowRect(row);if(r.contains(x,y)){secFocus=row;secAction(row,row==2?mm::stepRowTap(r,x):1);return true;}}
 return true;
}
bool securityBack(){if(!secPage()||!secSetupOpen())return false;secSetup.cancel();secEntry.clear();secNote.clear();notice="";render();return true;}
void securityLeft(){secSetup.cancel();secEntry.clear();secNote.clear();secFocus=0;}
void securityOpen(){secSetup.cancel();secEntry.clear();secNote.clear();secFocus=0;}
bool securitySerial(const String &s){
 if(s!="security status")return false;
 Serial.printf("SECURITY_STATUS pin=%d require=%d hide=%d usb=%d fails=%lu wait_ms=%lu locked=%d pad=%d boot=%d setup=%d preview=%s\n",secPin,secRequire,secHide,secUsb,(unsigned long)secGuard.fails,(unsigned long)secGuard.waitMs(millis()),deviceLocked,pinPadOpen,secBoot,int(secSetup.stage),deviceLocked?uiState().receivedPreview.c_str():"-");return true;
}
