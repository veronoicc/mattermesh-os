#pragma once
#include <stdint.h>
#include <string>
#include "services/mqtt_uplink.h"
// MQTT uplink of the received mesh packets (services/mqtt.cpp over PubSubClient; formatting in services/mqtt_uplink.h).
namespace device {
enum class MqttState:uint8_t {Off,NotConfigured,WaitingNetwork,Connecting,Connected,Failed};
struct MqttStatus {MqttState state=MqttState::Off;uint32_t published=0,dropped=0;size_t queued=0;std::string message;};
inline const char *mqttStateText(MqttState s){switch(s){case MqttState::Off:return "Off";case MqttState::NotConfigured:return "Set broker and region";case MqttState::WaitingNetwork:return "Waiting for network";case MqttState::Connecting:return "Connecting";case MqttState::Connected:return "Connected";default:return "Failed";}}
MqttStatus mqttStatus();// a copy; cheap
void mqttBegin();// allocates the packet queue and starts the task
void mqttSetIdentity(const std::string &nodeName,const std::string &pubKeyHex);// main loop: the origin of the packets
void mqttOffer(const mqtt_uplink::Packet &p);// main loop, per received packet; ignored while the uplink is off
}
