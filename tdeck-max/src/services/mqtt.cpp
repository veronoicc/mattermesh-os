#include "services/mqtt.h"
#include "services/modem.h"
#include "services/cell_client.h"
#include "services/mqtt_downlink.h"
#include <deque>
#include "services/root_ca.h"
#include "services/clock_limits.h"
#include <Arduino.h>
#include <PubSubClient.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
// Publishes every received mesh packet as meshcoretomqtt JSON (QoS 0) to meshcore/<region>/<public key>/packets, over Wi-Fi or the
// modem as Settings > Cellular > Network allows. Packets wait in a bounded queue (oldest dropped) while there is no broker.
// TLS checks the broker's certificate against ISRG Root X1 (Let's Encrypt).
namespace device {
constexpr size_t QueueCapacity=48,BufferBytes=2048;constexpr int PerStep=6;
static mqtt_uplink::Queue queue;static SemaphoreHandle_t mutex=nullptr;static MqttStatus shown;static std::string origin,key;static std::atomic<bool> active{false};static mqtt_uplink::Backoff backoff;
// Downlink: received command payloads wait for the main loop, which answers them; the replies and the chat events wait for the next publish step.
constexpr size_t InboxMax=4,OutboxMax=8,MessageMax=24;static std::deque<std::string> inbox,outbox,messages;static std::atomic<bool> publishing{false};
static void publishStatus(MqttState s,const std::string &message){xSemaphoreTake(mutex,portMAX_DELAY);shown.state=s;shown.message=message;xSemaphoreGive(mutex);}
static const char *refusal(int state){switch(state){case -4:return "Broker did not answer";case -3:return "Connection lost";case -2:return "Cannot reach the broker";case -1:return "Disconnected";case 1:return "Broker rejects the protocol";case 2:return "Broker rejects the client id";case 3:return "Broker unavailable";case 4:return "Bad user name or password";case 5:return "Not authorised";default:return "Connection failed";}}
struct Session {
 std::unique_ptr<NetSecure> link;std::unique_ptr<PubSubClient> mqtt;NetPath path=NetPath::None;CellularSettings cfg;bool subscribed=false;
 void drop(){if(mqtt&&mqtt->connected())mqtt->disconnect();mqtt.reset();link.reset();subscribed=false;}
};
static bool sameBroker(const CellularSettings &a,const CellularSettings &b){return a.host==b.host&&a.port==b.port&&a.mqttTls==b.mqttTls&&a.mqttUser==b.mqttUser&&a.mqttPass==b.mqttPass&&a.region==b.region;}
// Runs inside PubSubClient::loop() in the task: only queues, the main loop validates and executes (payloads past the limit stay oversize and are refused there).
static void onCommand(char*,uint8_t *p,unsigned n){if(!mutex)return;std::string payload(reinterpret_cast<char*>(p),n>mqtt_downlink::PayloadMax+1?mqtt_downlink::PayloadMax+1:n);xSemaphoreTake(mutex,portMAX_DELAY);if(inbox.size()<InboxMax)inbox.push_back(std::move(payload));xSemaphoreGive(mutex);}
static void task(void*){
 Session s;uint32_t retryAt=0;
 for(;;){
  vTaskDelay(pdMS_TO_TICKS(100));
  CellularSettings c=cellularSettingsCopy();active=c.mqtt;
  publishing=c.mqtt&&c.downlink&&c.dlPublish;
  if(!c.mqtt||!c.downlink||!c.dlPublish){xSemaphoreTake(mutex,portMAX_DELAY);if(!c.mqtt||!c.downlink){inbox.clear();outbox.clear();}messages.clear();xSemaphoreGive(mutex);}
  if(!c.mqtt){s.drop();queue.clear();publishStatus(MqttState::Off,"");continue;}
  if(!mqttConfigured(c)){s.drop();publishStatus(MqttState::NotConfigured,"");continue;}
  NetPath path=netPath();
  if(s.mqtt&&(path!=s.path||!sameBroker(c,s.cfg)))s.drop();
  if(path==NetPath::None){s.drop();publishStatus(MqttState::WaitingNetwork,"");continue;}
  std::string name,pub;xSemaphoreTake(mutex,portMAX_DELAY);name=origin;pub=key;xSemaphoreGive(mutex);
  if(pub.size()!=64){publishStatus(MqttState::WaitingNetwork,"No node identity");continue;}
  std::string region=normalizeRegion(c.region);
  if(!s.mqtt||!s.mqtt->connected()){
   if(s.mqtt){s.drop();}
   if(int32_t(millis()-retryAt)<0)continue;
   publishStatus(MqttState::Connecting,"");
   s.path=path;s.cfg=c;s.link.reset(new NetSecure(c.mqttTls?MM_CA_ISRG_ROOT_X1:nullptr,10,c.mqttTls));s.link->client().setTimeout(10);
   s.mqtt.reset(new PubSubClient(s.link->client()));s.mqtt->setServer(c.host.c_str(),c.port);s.mqtt->setKeepAlive(60);s.mqtt->setSocketTimeout(10);
   s.mqtt->setCallback(onCommand);
   if(!s.mqtt->setBufferSize(BufferBytes)){s.drop();publishStatus(MqttState::Failed,"Out of memory");retryAt=millis()+backoff.next();continue;}
   std::string statusTopic=mqtt_uplink::topic(region,pub,"status"),will=mqtt_uplink::statusJson(false,name,pub,0);
   bool ok=s.mqtt->connect(mqtt_uplink::clientId(pub).c_str(),c.mqttUser.empty()?nullptr:c.mqttUser.c_str(),c.mqttUser.empty()?nullptr:c.mqttPass.c_str(),statusTopic.c_str(),0,true,will.c_str());
   if(!ok){const char *why=c.mqttTls&&s.mqtt->state()==-2?"Cannot reach the broker or TLS certificate not trusted":refusal(s.mqtt->state());s.drop();publishStatus(MqttState::Failed,why);retryAt=millis()+backoff.next();continue;}
   time_t now=time(nullptr);std::string online=mqtt_uplink::statusJson(true,name,pub,now>=MM_EPOCH_MIN?uint32_t(now):0);s.mqtt->publish(statusTopic.c_str(),reinterpret_cast<const uint8_t*>(online.data()),online.size(),true);
   backoff.reset();publishStatus(MqttState::Connected,"");}
  s.mqtt->loop();
  {std::string cmd=mqtt_uplink::topic(region,pub,"cmd");if(c.downlink!=s.subscribed&&(c.downlink?s.mqtt->subscribe(cmd.c_str()):s.mqtt->unsubscribe(cmd.c_str())))s.subscribed=c.downlink;}// QoS 0: a lost command is answered by silence, a repeated one never runs twice
  std::string topic=mqtt_uplink::topic(region,pub,"packets"),json;
  for(int i=0;i<PerStep;i++){mqtt_uplink::Packet p;bool got;xSemaphoreTake(mutex,portMAX_DELAY);got=queue.pop(p);xSemaphoreGive(mutex);if(!got)break;
   if(!mqtt_uplink::format(p,name,pub,json))continue;
   if(!s.mqtt->publish(topic.c_str(),reinterpret_cast<const uint8_t*>(json.data()),json.size(),false)){xSemaphoreTake(mutex,portMAX_DELAY);shown.dropped++;xSemaphoreGive(mutex);break;}
   xSemaphoreTake(mutex,portMAX_DELAY);shown.published++;xSemaphoreGive(mutex);}
  for(int i=0;i<PerStep&&s.mqtt->connected();i++){std::string json,leaf;bool got=false;xSemaphoreTake(mutex,portMAX_DELAY);
   if(!outbox.empty()){json=std::move(outbox.front());outbox.pop_front();leaf="resp";got=true;}else if(!messages.empty()){json=std::move(messages.front());messages.pop_front();leaf="messages";got=true;}xSemaphoreGive(mutex);
   if(!got)break;if(!s.mqtt->publish(mqtt_uplink::topic(region,pub,leaf.c_str()).c_str(),reinterpret_cast<const uint8_t*>(json.data()),json.size(),false)){xSemaphoreTake(mutex,portMAX_DELAY);shown.dropped++;xSemaphoreGive(mutex);}}
 }}
void mqttBegin(){
 mutex=xSemaphoreCreateMutex();auto *slots=static_cast<mqtt_uplink::Packet*>(heap_caps_malloc(sizeof(mqtt_uplink::Packet)*QueueCapacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(slots)queue.attach(slots,QueueCapacity);
 xTaskCreatePinnedToCore(task,"matter-mqtt",12288,nullptr,1,nullptr,0);}
void mqttSetIdentity(const std::string &nodeName,const std::string &pubKeyHex){if(!mutex)return;xSemaphoreTake(mutex,portMAX_DELAY);origin=nodeName;key=pubKeyHex;xSemaphoreGive(mutex);}
void mqttOffer(const mqtt_uplink::Packet &p){if(!active.load()||!mutex)return;xSemaphoreTake(mutex,portMAX_DELAY);queue.push(p);xSemaphoreGive(mutex);}
bool mqttPublishing(){return publishing.load()&&mutex;}
void mqttOfferMessage(const std::string &json){if(!mutex||!publishing.load())return;xSemaphoreTake(mutex,portMAX_DELAY);if(messages.size()>=MessageMax){messages.pop_front();shown.dropped++;}messages.push_back(json);xSemaphoreGive(mutex);}
bool mqttPollCommand(std::string &payload){if(!mutex)return false;xSemaphoreTake(mutex,portMAX_DELAY);bool got=!inbox.empty();if(got){payload=std::move(inbox.front());inbox.pop_front();shown.commands++;}xSemaphoreGive(mutex);return got;}
void mqttRespond(const std::string &json){if(!mutex)return;xSemaphoreTake(mutex,portMAX_DELAY);if(outbox.size()<OutboxMax)outbox.push_back(json);else shown.dropped++;xSemaphoreGive(mutex);}
std::vector<std::string> mqttPendingMessages(){std::vector<std::string> out;if(!mutex)return out;xSemaphoreTake(mutex,portMAX_DELAY);out.assign(messages.begin(),messages.end());xSemaphoreGive(mutex);return out;}
MqttStatus mqttStatus(){if(!mutex)return MqttStatus();xSemaphoreTake(mutex,portMAX_DELAY);MqttStatus s=shown;s.queued=queue.size();s.dropped+=queue.dropped();xSemaphoreGive(mutex);return s;}
}
