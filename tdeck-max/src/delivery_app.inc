// Message Delivery history (section 62, mesh/delivery_log.h): the timeline of one of your own messages. Included by main.cpp.
// Opened from the own-message panel (DELIVERY / key 6); BACK returns to the chat. Keys: W/K up, S/J down, ENTER resend, A back.
// Events of recent sends live in RAM (a small FIFO, times relative to the queue moment) and reach the journal (E1 rows)
// shortly after the last change; older messages are read back from there, messages from before timelines are synthesized.
struct DeliveryLive {uint32_t token=0,queuedAt=0,changedAt=0;matter::delivery::Timeline timeline;bool dirty=false;};
static std::vector<DeliveryLive> deliveryLive;static uint32_t deliveryToken=0;static int deliveryOffset=0;
constexpr size_t DeliveryLiveMax=16;constexpr uint32_t DeliverySettleMs=500;
static bool onDeliveryPage(){return !deviceLocked&&screen==Screen::Section&&section==mm::DeliverySection;}
static void deliveryFlush(DeliveryLive &e){if(!e.dirty)return;e.dirty=false;radioJournal.timeline(e.token,e.timeline.encode().c_str());}
static DeliveryLive *deliveryEntry(uint32_t token){
 for(auto &e:deliveryLive)if(e.token==token)return &e;
 matter::Record r;if(!loadedRecord(token,r)||!r.mine||r.timeline.isEmpty())return nullptr;
 DeliveryLive e;e.token=token;e.timeline=matter::delivery::Timeline::decode(r.timeline.c_str());if(e.timeline.events.empty())return nullptr;
 e.queuedAt=millis()-e.timeline.events.back().at;// the clock of an older send restarts from its last event
 if(deliveryLive.size()>=DeliveryLiveMax){deliveryFlush(deliveryLive.front());deliveryLive.erase(deliveryLive.begin());}
 deliveryLive.push_back(e);return &deliveryLive.back();}
static void deliveryAdd(uint32_t token,matter::delivery::Kind kind,const String &arg){
 auto *e=deliveryEntry(token);if(!e)return;
 if(e->timeline.add(millis()-e->queuedAt,kind,arg.c_str())){e->dirty=true;e->changedAt=millis();if(onDeliveryPage()&&deliveryToken==token)repaintSettled();}}
// A message was queued for sending: its history starts.
void deliveryQueued(uint32_t token){
 if(deliveryLive.size()>=DeliveryLiveMax){deliveryFlush(deliveryLive.front());deliveryLive.erase(deliveryLive.begin());}
 DeliveryLive e;e.token=token;e.queuedAt=millis();e.timeline.add(0,matter::delivery::Kind::Queued);e.dirty=true;e.changedAt=millis();deliveryLive.push_back(e);}
void deliveryRecord(uint32_t token,matter::delivery::Kind kind,const String &arg){deliveryAdd(token,kind,arg);}
// One mesh callback (service.cpp Delivery) or companion outcome: the typed event, plus every repeater of the heard list.
void deliveryNote(const matter::Delivery &d){
 using matter::delivery::Kind;
 if(d.hasEvent)deliveryAdd(d.token,d.event,d.eventArg);
 else if(d.txComplete)deliveryAdd(d.token,d.state=="Delivered"?Kind::AckReceived:d.state=="No ACK"?Kind::NoAck:d.txSuccess?Kind::TxDone:Kind::Failed,d.state=="Delivered"||d.state=="No ACK"||d.txSuccess?String("Companion"):d.state);// companion link: only the outcome is known
 int at=0;while(d.repeaters>=0&&at<int(d.heard.length())){int comma=d.heard.indexOf(',',at);if(comma<0)comma=d.heard.length();String prefix=d.heard.substring(at,comma);prefix.toLowerCase();at=comma+1;if(prefix.length())deliveryAdd(d.token,Kind::HeardRepeater,prefix);}}
// The timeline shown for a message: the live one, the stored one, or one made from the old status line.
static bool deliveryTimeline(uint32_t token,matter::delivery::Timeline &out,matter::Record &record){
 if(!loadedRecord(token,record)||!record.mine)return false;
 for(auto &e:deliveryLive)if(e.token==token){out=e.timeline;return true;}
 out=record.timeline.isEmpty()?matter::delivery::Timeline::legacy(record.status.c_str(),record.repeaters,record.heard.c_str()):matter::delivery::Timeline::decode(record.timeline.c_str());return true;}
static std::string deliveryRepeaterLabel(const std::string &prefix){int matches=0;String name=matter::nodeNameForPrefix(prefix.c_str(),matches,2);return matches==1&&name.length()?mm::senderDisplayName(name.c_str()):std::string();}
void deliveryShow(uint32_t token){deliveryToken=token;openSection(mm::DeliverySection);}
void deliveryOpen(int id){(void)id;deliveryOffset=0;}
void deliveryLeft(int next){(void)next;deliveryOffset=0;}
void deliveryCompose(mm::State &v){
 if(!onDeliveryPage())return;
 matter::delivery::Timeline t;matter::Record r;auto &d=v.delivery;d.offset=deliveryOffset;if(!deliveryTimeline(deliveryToken,t,r))return;
 d.found=true;String first=r.body;int nl=first.indexOf('\n');if(nl>=0)first=first.substring(0,nl);d.preview=first.c_str();d.summary=t.summary();
 std::string route=t.route();int heard=t.heard();d.detail=route.empty()?(heard?std::to_string(heard)+(heard==1?" repeater heard":" repeaters heard"):std::string()):"Route: "+route+(heard?"  /  "+std::to_string(heard)+(heard==1?" repeater":" repeaters"):std::string());
 for(auto &e:t.events){mm::DeliveryView::Row row;row.when=matter::delivery::relative(e.at);row.marker=matter::delivery::marker(e.kind);row.text=matter::delivery::describe(e,e.kind==matter::delivery::Kind::HeardRepeater?deliveryRepeaterLabel(e.arg):std::string());d.rows.push_back(row);}}
static void deliveryScrollBy(int rows){int n=0;matter::delivery::Timeline t;matter::Record r;if(deliveryTimeline(deliveryToken,t,r))n=int(t.events.size());deliveryOffset=constrain(deliveryOffset+rows,0,std::max(0,n-mm::DeliveryRows));render();}
// RESEND: the same text and scope again as a new message, whose history opens next.
static void deliveryResend(){
 matter::Record r;if(!loadedRecord(deliveryToken,r)||!r.mine){notice="Message not found";render();return;}
 std::vector<matter::Record> one{r};auto payload=device::resendPayload(r,r.chat,r.chat,one);if(!payload.valid){notice=payload.error.c_str();render();return;}
 String scope=payload.scope.c_str();uint32_t queued=0;
 if(queueRadioMessage(payload.body.c_str(),payload.scoped?&scope:nullptr,r.chat,&queued)){deliveryToken=queued;deliveryOffset=0;notice="Resent / "+notice;}else chatToast(notice,3000);
 render();}
bool deliveryBack(){return false;}// the page has no sub-state: BACK is the shared navigation back to the chat
bool deliveryKey(char c){
 if(!onDeliveryPage())return false;
 if(c=='w'||c=='k'){deliveryScrollBy(-1);return true;}if(c=='s'||c=='j'){deliveryScrollBy(1);return true;}
 if(c=='\n'){deliveryResend();return true;}if(c=='a'||c=='h'){back();return true;}
 return false;}
bool deliveryTap(int x,int y){
 if(!onDeliveryPage())return false;
 if(mm::BackButton.contains(x,y)){back();return true;}
 if(mm::DeliveryResend.contains(x,y))deliveryResend();
 return true;}
bool deliveryScroll(int dy){if(!onDeliveryPage())return false;deliveryScrollBy(dy<0?mm::DeliveryRows:-mm::DeliveryRows);return true;}
// Timelines are written once a burst of events has settled (a flood echo can bring many in a second).
void deliveryLoop(){for(auto &e:deliveryLive)if(e.dirty&&uint32_t(millis()-e.changedAt)>=DeliverySettleMs){deliveryFlush(e);break;}}
// USB: delivery status | delivery open <token hex|last> | delivery dump <token hex|last> | delivery resend (the open view's message); last: your newest message of the open chat
bool deliverySerial(const String &s){
 if(!s.startsWith("delivery"))return false;
 using matter::delivery::Timeline;
 auto tokenOf=[&](const String &rest){String a=rest;a.trim();if(a=="last"){auto *sum=radioJournal.summary(activeChat);return sum?sum->lastOwn:uint32_t(0);}return uint32_t(strtoul(a.c_str(),nullptr,16));};
 if(s=="delivery status"){Timeline t;matter::Record r;bool found=onDeliveryPage()&&deliveryTimeline(deliveryToken,t,r);unsigned pending=0;for(auto &e:deliveryLive)pending+=e.dirty;
  Serial.printf("DELIVERY_STATUS open=%d token=%08lx found=%d events=%u offset=%d live=%u unsaved=%u summary=%s\n",onDeliveryPage(),(unsigned long)deliveryToken,found,found?unsigned(t.events.size()):0u,deliveryOffset,unsigned(deliveryLive.size()),pending,found?t.summary().c_str():"");return true;}
 if(s.startsWith("delivery open ")){uint32_t token=tokenOf(s.substring(14));Timeline t;matter::Record r;bool ok=deliveryTimeline(token,t,r);if(ok)deliveryShow(token);Serial.printf("DELIVERY_OPEN ok=%d token=%08lx\n",ok,(unsigned long)token);return true;}
 if(s.startsWith("delivery dump ")){uint32_t token=tokenOf(s.substring(14));Timeline t;matter::Record r;bool ok=deliveryTimeline(token,t,r);
  if(ok){for(auto &e:deliveryLive)if(e.token==token)deliveryFlush(e);int i=0;for(auto &e:t.events)Serial.printf("DELIVERY_EVENT token=%08lx index=%d at=%lu kind=%s arg=%s\n",(unsigned long)token,i++,(unsigned long)e.at,matter::delivery::marker(e.kind),e.arg.c_str());}
  Serial.printf("DELIVERY_DUMP ok=%d token=%08lx events=%u heard=%d hops=%d summary=%s\n",ok,(unsigned long)token,ok?unsigned(t.events.size()):0u,ok?t.heard():0,ok?t.hops():-1,ok?t.summary().c_str():"");return true;}
 if(s=="delivery resend"){if(!onDeliveryPage()){Serial.println("DELIVERY_RESEND ok=0 reason=view closed");return true;}uint32_t was=deliveryToken;deliveryResend();Serial.printf("DELIVERY_RESEND ok=%d token=%08lx\n",deliveryToken!=was,(unsigned long)deliveryToken);return true;}
 return false;}
