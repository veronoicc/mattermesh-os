// MQTT downlink glue (services/mqtt_downlink.h, services/mqtt.cpp): runs the .../cmd commands on the main loop through the UI's own send
// path (queueRadioMessage: journal, delivery tracking, the chat view) and hands the readable chat traffic to the broker session.
// Settings: Settings > Cellular > MQTT DOWNLINK (cellular_app.inc). Included by main.cpp after cellular_app.inc.
#include "services/mqtt_downlink.h"
static mqtt_downlink::RateLimiter mqttCmdLimit;// 10 payloads per minute, good or bad
static uint32_t mqttDlEpoch(){time_t t=time(nullptr);return t>=MM_EPOCH_MIN?uint32_t(t):0;}
static std::string mqttDlHex(uint32_t token){char id[9];snprintf(id,sizeof id,"%08x",unsigned(token));return id;}
static void mqttDlChat(mqtt_downlink::Message &o,int chat){o.direct=chats[chat].direct;if(o.direct){o.contact=chats[chat].name.c_str();o.pubkey=peerKeys[chat].c_str();}else{o.channel=chats[chat].name.c_str();o.channelIndex=chat;}}
// The send path of the UI; the reply carries the message id that .../messages uses for the same message.
static bool mqttDlSend(int chat,const std::string &text,std::string &result,std::string &err){
 uint32_t token=0;bool ok=queueRadioMessage(text.c_str(),nullptr,chat,&token);String why=notice;notice="";if(!deviceLocked)render();
 if(!ok){err=why.length()?why.c_str():"not sent";return false;}
 result="{\"message_id\":\""+mqttDlHex(token)+"\",\"state\":\""+mqtt_downlink::esc(why.c_str())+"\"}";return true;}
struct MqttDlHost:mqtt_downlink::Host {
 bool sendChannel(const std::string &spec,const std::string &text,std::string &result,std::string &err)override{
  std::vector<mqtt_downlink::Named> names;std::vector<int> ids;for(int i=0;i<chatCount;i++)if(!chats[i].direct){names.push_back({chats[i].name.c_str(),""});ids.push_back(i);}
  int hit=mqtt_downlink::resolve(names,spec,size_t(-1)),chat=-1;// a name first, then a slot number
  if(hit==-2){err="ambiguous channel";return false;}
  if(hit>=0)chat=ids[hit];else if(spec.size()<=3&&spec.find_first_not_of("0123456789")==std::string::npos){int n=atoi(spec.c_str());if(n<chatCount&&!chats[n].direct)chat=n;}
  if(chat<0){err="unknown channel";return false;}
  return mqttDlSend(chat,text,result,err);}
 bool sendDm(const std::string &spec,const std::string &text,std::string &result,std::string &err)override{
  auto nodes=matter::nodes(1,0,matter::NODE_CAPACITY);std::vector<mqtt_downlink::Named> names;for(auto &n:nodes)names.push_back({n.name.c_str(),n.key.c_str()});
  int hit=mqtt_downlink::resolve(names,spec,6);if(hit==-2){err="ambiguous contact";return false;}if(hit<0){err="unknown contact";return false;}
  int chat=directConversation(nodes[hit].key,nodes[hit].name);if(chat<0){err="conversation limit reached";return false;}
  return mqttDlSend(chat,text,result,err);}
 bool advert(bool position,std::string &err)override{auto g=device::locationState();String reason;bool ok=matter::advertise(reason,position&&g.configured,g.latitude,g.longitude);if(!ok)err=reason.c_str();return ok;}
 std::string status()override{
  auto r=matter::snapshot();int channels=0;for(int i=0;i<chatCount;i++)if(!chats[i].direct)channels++;
  return "{\"name\":\""+mqtt_downlink::esc(r.config.name.c_str())+"\",\"pubkey\":\""+mqtt_uplink::detail::lower(matter::ownKey().c_str())+"\",\"uptime_s\":"+std::to_string(millis()/1000)+",\"epoch\":"+std::to_string(mqttDlEpoch())+",\"radio_ready\":"+(r.ready?"true":"false")+",\"received\":"+std::to_string(r.received)+",\"sent\":"+std::to_string(r.sent)+",\"channels\":"+std::to_string(channels)+",\"contacts\":"+std::to_string(matter::nodeCount(1))+",\"allow_send\":"+(device::cellularSettings().dlSend?"true":"false")+"}";}
 std::string listChannels()override{mqtt_downlink::ListBuilder l(1500);for(int i=0;i<chatCount;i++)if(!chats[i].direct&&!l.add("{\"index\":"+std::to_string(i)+",\"name\":\""+mqtt_downlink::esc(chats[i].name.c_str())+"\",\"ready\":"+(channelReady[i]?"true":"false")+",\"muted\":"+((mutedChannels>>i)&1?"true":"false")+"}"))break;return l.done();}
 std::string listContacts()override{mqtt_downlink::ListBuilder l(1500);for(auto &n:matter::contacts(1,"",0,40).rows)if(!l.add("{\"name\":\""+mqtt_downlink::esc(n.name.c_str())+"\",\"pubkey\":\""+mqtt_uplink::detail::lower(std::string(n.key.c_str()).substr(0,12))+"\",\"live\":"+(n.live?"true":"false")+",\"hops\":"+std::to_string(n.hops)+"}"))break;return l.done();}
};
static std::string mqttDlRun(const std::string &payload){MqttDlHost host;auto &c=device::cellularSettings();return mqtt_downlink::handle(payload,c.dlToken,c.dlSend,mqttCmdLimit,millis(),host);}
// Main loop: one received command per pass, the reply goes out with the next publish step.
void mqttDlLoop(){std::string payload;if(device::mqttPollCommand(payload))device::mqttRespond(mqttDlRun(payload));}
// A chat message this node can read (channel or DM), after it is in the journal.
void mqttDlIncoming(const matter::Received &m,int chat,uint32_t token){
 if(!device::mqttPublishing()||chat<0||chat>=chatCount)return;
 mqtt_downlink::Message o;mqttDlChat(o,chat);o.token=token;o.ts=m.timestamp?m.timestamp:mqttDlEpoch();o.sender=m.sender.c_str();o.text=m.body.c_str();o.hops=m.hops;o.path=m.path.c_str();
 auto level=matter::radioLevels();if(level.received){o.hasSignal=true;o.snrTenths=int(lroundf(level.snr*10));o.rssiTenths=int(lroundf(level.rssi*10));}// the packet just decoded
 device::mqttOfferMessage(mqtt_downlink::messageJson(o));}
// A message queued from this device (keyboard, web, quick reply or a command); `state` is what the journal says right now.
void mqttDlOutgoing(int chat,uint32_t token,const String &body,const String &state){
 if(!device::mqttPublishing()||chat<0||chat>=chatCount)return;
 mqtt_downlink::Message o;mqttDlChat(o,chat);o.token=token;o.ts=mqttDlEpoch();o.out=true;o.sender=matter::snapshot().config.name.c_str();o.text=body.c_str();o.state=state.c_str();
 device::mqttOfferMessage(mqtt_downlink::messageJson(o));}
// A delivery state change or repeat count of a message in the journal.
void mqttDlDelivery(const matter::Delivery &d){
 if(!device::mqttPublishing()||(!d.state.length()&&d.repeaters<0)||!radioJournal.index.find(d.token))return;
 mqtt_downlink::Message o;o.event="delivery";o.token=d.token;o.ts=mqttDlEpoch();o.state=d.state.c_str();o.repeaters=d.repeaters;o.heard=d.heard.c_str();o.firstRepeater=d.firstRepeater.c_str();
 device::mqttOfferMessage(mqtt_downlink::messageJson(o));}
// Serial: "mqtt cmd <json>" runs a command payload through the same handler and prints the reply (what the broker would receive);
// "mqtt messages" lists the chat events that wait for the broker (one MQTT_MSG line each).
bool mqttDlSerial(const String &s){
 if(s=="mqtt messages"){auto list=device::mqttPendingMessages();for(auto &json:list)Serial.printf("MQTT_MSG %s\n",json.c_str());Serial.printf("MQTT_MESSAGES count=%u\n",unsigned(list.size()));return true;}
 if(!s.startsWith("mqtt cmd "))return false;
 if(!device::cellularSettings().downlink){Serial.println("MQTT_RESP downlink off");return true;}
 Serial.printf("MQTT_RESP %s\n",mqttDlRun(s.substring(9).c_str()).c_str());return true;}
