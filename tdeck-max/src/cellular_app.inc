// Settings > Cellular (51): Modem switch, status card, SIM / APN / network / MQTT uplink rows. Every change is stored at once
// through device::cellularApply (NVS "mmcell"). Page: ui/cellular_layout.h, logic: ui/cellular_page.h, drawing: ui/cellular_renderer.inc.
// Included by main.cpp after radio_link_app.inc.
#include "services/modem.h"
#include "services/mqtt.h"
#include "ui/cellular_layout.h"
static int cellFocus=0,cellScroll=0,cellEditing=-1;static String cellInput;
static uint32_t cellSeenRevision=0xffffffffu,cellSeenPublished=0,cellSeenDropped=0;static size_t cellSeenQueued=0;static device::MqttState cellSeenMqtt=device::MqttState::Off;
static bool cellPage(){return screen==Screen::Section&&section==mm::CellularSection;}
// Stores a changed copy and tells the outcome; the page repaints either way.
static void cellStore(const device::CellularSettings &next,const char *saved="Saved"){notice=device::cellularApply(next)?saved:"Save failed";render();}
static void cellSetFocus(int f){cellFocus=f;cellScroll=mm::cellFollow(cellScroll,f);}
void cellularCompose(mm::State &v){
 auto &c=device::cellularSettings();auto st=device::modemStatus();auto mq=device::mqttStatus();mm::CellularView &w=v.cell;
 w.modem=c.modem;w.phase=st.phase;w.data=st.data;w.model=st.model;w.op=st.operatorName;w.ip=st.ip;w.message=st.message;w.csq=st.csq;w.bars=st.bars;w.rat=st.rat;w.simReady=st.simReady;w.roaming=st.roaming;
 w.focus=cellFocus;w.scroll=cellScroll;w.editing=cellEditing;w.input=cellInput.c_str();
 for(int r=0;r<mm::CellRows;r++){w.value[r]=mm::cellRowText(c,r);w.on[r]=mm::cellRowOn(c,r);}
 w.mqttState=std::string("MQTT: ")+device::mqttStateText(mq.state);
 w.mqttCounts=std::to_string(mq.published)+" sent / "+std::to_string(mq.dropped)+" dropped / "+std::to_string(unsigned(mq.queued))+" queued";}
String *cellularField(){return cellPage()&&cellEditing>=0?&cellInput:nullptr;}
void cellularLeft(){cellEditing=-1;cellInput="";cellFocus=0;cellScroll=0;}
void cellularTick(){if(!cellPage())return;auto st=device::modemStatus();auto mq=device::mqttStatus();
 if(st.revision!=cellSeenRevision||mq.published!=cellSeenPublished||mq.dropped!=cellSeenDropped||mq.queued!=cellSeenQueued||mq.state!=cellSeenMqtt){cellSeenRevision=st.revision;cellSeenPublished=mq.published;cellSeenDropped=mq.dropped;cellSeenQueued=mq.queued;cellSeenMqtt=mq.state;markRadioDirty();}}
static void cellStartEdit(int row){cellEditing=row;cellInput=mm::cellEditText(device::cellularSettings(),row).c_str();cellSetFocus(row+1);render();}
// ENTER, a tap elsewhere or the same card again: the text is validated and stored; invalid keeps the old value and says why.
static void cellFinishEdit(){int row=cellEditing;cellEditing=-1;std::string text=cellInput.c_str();cellInput="";if(row<0)return;
 device::CellularSettings next=device::cellularSettings();std::string why;bool ok=mm::cellCommit(next,row,text,why);
 if(!ok){notice=why.c_str();render();return;}
 if(why.empty())cellStore(next);else{device::cellularApply(next);notice=why.c_str();render();}}
static void cellModemToggle(){auto next=device::cellularSettings();next.modem=!next.modem;cellStore(next,next.modem?"Modem on":"Modem off");}
static void cellStep(int dir){auto next=device::cellularSettings();next.network=mm::cellCycleNetMode(next.network,dir);cellStore(next);}
static void cellToggleRow(int row){auto next=device::cellularSettings();mm::cellToggle(next,row);cellStore(next);}
// Activates a row: switch, step forward or edit.
static void cellActivate(int row){mm::CellKind k=mm::cellKind(row);if(k==mm::CellToggle)cellToggleRow(row);else if(k==mm::CellStep)cellStep(1);else if(k==mm::CellText)cellStartEdit(row);}
bool cellularKey(char c){
 if(c=='\b')return false;// Backspace: the generic field edit / Back
 if(!cellPage())return false;
 if(cellEditing>=0){if(c=='\n'){cellFinishEdit();return true;}
  char ch=mm::cellAcceptChar(cellEditing,cellInput.c_str(),c);if(ch){cellInput+=ch;repaintSettled();}return true;}
 if(c=='a')return false;// Back
 if(c=='j'||c=='s'||c=='l'){if(c=='l'&&cellFocus>0&&mm::cellKind(mm::cellFocusRow(cellFocus))==mm::CellStep){cellStep(1);return true;}cellSetFocus(mm::cellMoveFocus(cellFocus,1));render();return true;}
 if(c=='k'||c=='w'||c=='h'){if(c=='h'&&cellFocus>0&&mm::cellKind(mm::cellFocusRow(cellFocus))==mm::CellStep){cellStep(-1);return true;}cellSetFocus(mm::cellMoveFocus(cellFocus,-1));render();return true;}
 if(c=='d'&&cellFocus>0&&mm::cellKind(mm::cellFocusRow(cellFocus))==mm::CellStep){cellStep(1);return true;}
 if(c=='\n'||c==' '){if(cellFocus==0)cellModemToggle();else cellActivate(mm::cellFocusRow(cellFocus));return true;}
 return false;}
bool cellularTap(int x,int y){
 if(!cellPage())return false;
 if(mm::BackButton.contains(x,y))return false;
 int hit=-1;for(int slot=0;slot<mm::CellVisible;slot++)if(mm::cellularRow(slot).contains(x,y)&&cellScroll+slot<mm::CellRows)hit=cellScroll+slot;
 if(cellEditing>=0){if(hit==cellEditing){cellFinishEdit();return true;}cellFinishEdit();if(hit<0)return true;}
 if(mm::CellularSwitch.contains(x,y)){cellSetFocus(0);cellModemToggle();return true;}
 if(mm::CellularUp.contains(x,y)||mm::CellularDown.contains(x,y)){cellScroll=mm::cellPageScroll(cellScroll,mm::CellularDown.contains(x,y));if(mm::cellFocusRow(cellFocus)>=0&&(mm::cellFocusRow(cellFocus)<cellScroll||mm::cellFocusRow(cellFocus)>=cellScroll+mm::CellVisible))cellFocus=std::min(mm::CellFocusCount-1,cellScroll+1);render();return true;}
 if(hit<0||mm::cellKind(hit)==mm::CellInfo)return true;
 cellSetFocus(hit+1);
 if(mm::cellKind(hit)==mm::CellStep){cellStep(mm::stepRowTap(mm::cellularRow(hit-cellScroll),x));return true;}
 cellActivate(hit);return true;}
