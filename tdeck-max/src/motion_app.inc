// Settings > Motion / compass (MotionSection) and the motion glue of the main loop: raise to wake, repaint when the heading
// changes. Included by main.cpp before the Maps app, whose arrow and look-ahead read the heading.
void showLockHint();
// (motionFocus, the keyboard focus row 0 sensor, 1 raise to wake, 2 sensitivity, 3 orientation, 4 telemetry, lives in main.cpp: uiState reads it)
// dir: -1 / +1 steps a "< value >" row backwards / forwards (the two-value rows just flip), anything else toggles.
static void motionAction(int row,int dir){
 auto s=device::motionState();bool ok=false;const char *text="";
 switch(row){
  case 0:ok=device::motionEnable(!s.enabled);text=s.enabled?"Motion sensor off":"Motion sensor on";break;
  case 1:ok=device::motionRaiseToWake(!s.raise);text=s.raise?"Raise to wake off":"Raise to wake on";break;
  case 2:ok=device::motionSetSensitivity((s.sensitivity+3+(dir<0?-1:1))%3);text="Sensitivity saved";break;
  case 3:ok=device::motionHeadingUp(!s.headingUp);text=s.headingUp?"Map: north up":"Map: heading up";break;
  case 4:ok=device::motionShareTelemetry(!s.telemetry);text=s.telemetry?"Motion not shared":"Motion shared in telemetry";break;}
 notice=ok?text:"Cannot save setting";render();
}
static void motionKey(char c){
 if(c=='w'||c=='k'){motionFocus=(motionFocus+mm::MotionRows-1)%mm::MotionRows;render();}
 else if(c=='s'||c=='j'){motionFocus=(motionFocus+1)%mm::MotionRows;render();}
 else if(c=='a'||c=='h')motionAction(motionFocus,-1);
 else if(c=='d'||c=='l'||c=='\n'||c==' ')motionAction(motionFocus,1);
}
static bool motionTap(int x,int y){
 for(int row=0;row<mm::MotionRows;row++){auto r=mm::motionRowRect(row);if(r.contains(x,y)){motionFocus=row;motionAction(row,row==2?mm::stepRowTap(r,x):0);return true;}}
 return false;
}
// A raise gesture wakes the device like a key press: lights back on after the display timeout, or the lock hint on the lock screen.
static void motionWake(){
 if(!device::motionRaiseTake()||screen==Screen::Welcome||welcomeWifi)return;
 if(deviceLocked){if(!quickReply.active)showLockHint();return;}
 if(!device::powerState().displayAwake){device::activity();Serial.println("MOTION_WAKE");}
}
// Map: repaint when the heading moved 12 degrees (at most every 1.5 s: the e-paper refresh is slow) or appeared / went.
static int mapHeadingPainted=-1;static uint32_t mapHeadingAt=0;
static void mapHeadingTick(const device::MotionState &m){
 if(screen!=Screen::Section||section!=9)return;
 int now=m.headingValid?int(lround(m.heading))%360:-1;
 bool changed=now!=mapHeadingPainted&&(now<0||mapHeadingPainted<0||fabsf(device::angleDelta(float(now),float(mapHeadingPainted)))>=12);
 if(changed&&uint32_t(millis()-mapHeadingAt)>=1500){mapHeadingAt=millis();markRadioDirty();}
}
static void motionTick(){
 motionWake();if(deviceLocked)return;
 auto m=device::motionState();static uint32_t revision=0,paintedAt=0;
 if(screen==Screen::Section&&section==mm::MotionSection&&m.revision!=revision&&uint32_t(millis()-paintedAt)>=1000){revision=m.revision;paintedAt=millis();markRadioDirty();}
 mapHeadingTick(m);
}
