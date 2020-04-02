/*
    LED Reactor Bridge
    Copyright (C) 2018 K Hughes Production, LLC

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
    multicast to broker
    broker udp or tcp response
    init mqtt
    commence lighting
*/

#include <Arduino.h>
#include <painlessMesh.h>
#include <PubSubClient.h>
#include "Private.h"
#include <WiFiClient.h>
#include <stdlib.h>
// #include <ESPAsyncUDP.h>
// #include <ESP8266SSDP.h>

// #define MQTT_MAX_PACKET_SIZE    512

// Multicast information for finding broker
#define MULTICAST_PORT  20001

// Broker IP Address for subscription
#define BROKER_IP       172, 24, 1, 1

// Mesh network information
#define MESH_PREFIX     "reactor"
#define MESH_PASSWORD   "reactPass"
#define MESH_SUBGROUP   "0x0000"
#define MESH_PORT       20002

// Prototypes
void setMqtt(int mqttPort=1883);
void receiveMqtt(char*, uint8_t*, unsigned int);
void receiveMesh(const uint32_t&, const String&);
void sendMulticast();

// Global variables
IPAddress* multicastIp;
IPAddress ip(0, 0, 0, 0);
painlessMesh mesh;
uint32_t deferenceBuffer = 4000000;
const size_t jsonBufferCapacity = 2048;

PubSubClient* mqttClient;
std::string hostname("reactorBridge");
std::string fromTopic("reactor/from/");

// Scheduler scheduler;
// AsyncUDP udp;
// WiFiServer server(20004);
// WiFiClient infoReceiver;

// WiFiClient wifiClient;
// IPAddress broker(BROKER_IP);
// PubSubClient* mqttClient = new PubSubClient(broker, MQTT_PORT, receiveMqtt, wifiClient);

// Tasks
// Task finder(5000, TASK_FOREVER, &sendMulticast, &scheduler);


void setMqtt(int mqttPort) {
    IPAddress* broker = new IPAddress(BROKER_IP);
    WiFiClient* wifiClient = new WiFiClient;
    mqttClient = new PubSubClient(*broker, mqttPort, receiveMqtt, *wifiClient);
}


void initialize() {
    multicastIp = new IPAddress(239, 16, 72, 1);
    setMqtt(1883);
    mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, STATION_CHANNEL);
    mesh.onReceive(&receiveMesh);
    mesh.stationManual(STATION_SSID, STATION_PSK);
    mesh.setHostname(hostname.c_str());
    mesh.setRoot(true);
    mesh.setContainsRoot(true);
}


void publish(const char* message) {
    mqttClient->publish(
            (std::string("reactor/from/") + MESH_SUBGROUP + "/" + hostname).c_str(),
            message
        );
    Serial.printf(
            "\nPublishing to: %s:\n\n\t%s\n\n",
            (std::string("reactor/from/") + hostname).c_str(),
            message
        );
}


void copyValues(
        StaticJsonDocument<jsonBufferCapacity>& parser,
        StaticJsonDocument<jsonBufferCapacity>& outgoing,
        const char* key
    ) {
    if (parser.containsKey(key)) {
        if (!outgoing.containsKey(key)) {
            outgoing[key] = parser[key];
        }
    } else {
        outgoing[key] = (char*)NULL;
    }
}


void parseMessage(const char* json, std::string targetRecipient) {
    StaticJsonDocument<jsonBufferCapacity> parser;
    std::vector<const char*> keys = {
            "rgbw", "fx", "test", "status", "clear",
            "save", "recall", "restart"
        };
    auto error = deserializeJson(parser, json);
    if (!error) {
        StaticJsonDocument<jsonBufferCapacity> outgoing;
        char serialized[512];
        if (parser["test"]) {
            publish("Remote bridge node test successful");
        }
        if (parser["restartBridge"]) {
            ESP.restart();
        }
        if (parser.containsKey("rgbw")) {
            JsonArray color = parser["rgbw"];
            outgoing["rgbw"] = color;
            // std::array<uint16_t, 4> target = {color[0], color[1], color[2], color[3]};
            Serial.printf("Received Target R: %u G: %u B: %u W: %u\n", color[0], color[1], color[2], color[3]);
        }
        if (parser.containsKey("fx")) {
            JsonArray effect = parser["fx"];
            // Extract relative start time in seconds
            double start = effect[2];
            // Extract duration in seconds
            double duration = effect[0];
            uint64_t absoluteStart = (mesh.getNodeTime() + (static_cast<uint32_t>(start * 1000000)));
            // Check if end occurs after clock rollover
            uint32_t end = (absoluteStart + (static_cast<uint32_t>(duration * 1000000)));
            if (end > (4294967295 - deferenceBuffer)) {
                absoluteStart = 1;
            }
            // Swap relative effect start for absolute start time, relative to current mesh time
            effect[2] = static_cast<uint32_t>(absoluteStart);
            // Copy effect settings to output buffer
            outgoing["fx"] = effect;
        }
        if (parser.containsKey("status")) {
            char buffer[11];
            std::string response("Status request received; absolute mesh time: ");
            sprintf(buffer, "%u", mesh.getNodeTime());
            response += buffer;
            publish(response.c_str());
        } else if (parser.containsKey("time")) {
            char buffer[11];
            publish(itoa(mesh.getNodeTime(), buffer, 10));
        }
        bool empty(true);
        for (auto key: keys) {
            copyValues(parser, outgoing, key);
            if (empty && (outgoing[key] != (char*)NULL)) {
                empty = false;
            }
        }
        serializeJson(outgoing, serialized);
        Serial.printf("Serialized: %s\n", serialized);
        // Broadcast to mesh
        if (!empty && (targetRecipient == "broadcast")) {
            mesh.sendBroadcast(serialized);
            Serial.println("Broadcast message sent");
        }
        Serial.println("Message parsed successfully\n");
    } else {
        Serial.println("Message parsing failed\n");
        mesh.sendBroadcast(json);
    }
}


void receiveMqtt(char* topic, uint8_t* payload, unsigned int length) {
    char interpreted[length];
    memcpy(interpreted, payload, length);
    interpreted[length] = '\0';
    std::string incomingTopic(topic);
    std::string group = incomingTopic.substr(11, 6);
    std::string targetRecipient = incomingTopic.substr(18);
    Serial.printf("Subgroup: %s\tTarget: %s\t", group.c_str(), targetRecipient.c_str());
    if (group == MESH_SUBGROUP) {
        parseMessage(interpreted, targetRecipient);
    } else {
        Serial.println("Group mismatch");
    }
    Serial.printf("%d memory free\n", ESP.getFreeHeap());
}


void receiveMesh(const uint32_t& sender, const String& message) {
    Serial.printf("Received from %u: %s\n", sender, message.c_str());
    String outgoingTopic("reactor/from/");
    outgoingTopic += static_cast<String>(sender);
    mqttClient->publish(outgoingTopic.c_str(), message.c_str());
}


// void setupMulticast() {
//     if(udp.listenMulticast(*multicastIp, MULTICAST_PORT, 2)) {
//         udp.onPacket([](AsyncUDPPacket packet) {
//             Serial.print("UDP Packet Type: ");
//             Serial.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast");
//             Serial.print(", From: ");
//             Serial.print(packet.remoteIP());
//             Serial.print(":");
//             Serial.print(packet.remotePort());
//             Serial.print(", To: ");
//             Serial.print(packet.localIP());
//             Serial.print(":");
//             Serial.print(packet.localPort());
//             Serial.print(", Length: ");
//             Serial.print(packet.length());
//             Serial.print(", Data: ");
//             Serial.write(packet.data(), packet.length());
//             Serial.println();
//             packet.printf("Got %u bytes of data", packet.length());
//         });
//     }
// }


// void setupMulticast() {
//     if(udp.listenMulticast(*multicastIp, MULTICAST_PORT, 2)) {
//         udp.onPacket([](AsyncUDPPacket packet) {
//             Serial.print("UDP Packet Type: ");
//             Serial.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast");
//             Serial.print(", From: ");
//             Serial.print(packet.remoteIP());
//             Serial.print(":");
//             Serial.print(packet.remotePort());
//             Serial.print(", To: ");
//             Serial.print(packet.localIP());
//             Serial.print(":");
//             Serial.print(packet.localPort());
//             Serial.print(", Length: ");
//             Serial.print(packet.length());
//             Serial.print(", Data: ");
//             Serial.write(packet.data(), packet.length());
//             Serial.println();
//             packet.printf("Got %u bytes of data", packet.length());
//         });
//     }
// }


// void sendMulticast() {
//     udp.print("nodeTest");
//     Serial.println("Sent Multicast");
// }


void run() {
    mqttClient->loop();
    mesh.update();
    if (ip != mesh.getStationIP()) {
        ip = IPAddress(mesh.getStationIP());
        Serial.printf("Connected to %s with IP %s\n", STATION_SSID, ip.toString().c_str());
        // finder.enable();
        // Serial.println("Enabling multicast finder");
        if (mqttClient->connect(hostname.c_str())) {
            publish("Initialized");
            Serial.println("Initialization notification published");
            mqttClient->subscribe("reactor/to/#");
            Serial.println("Subscribed");
            // finder.disable();
            // Serial.println("Disabling multicast finder");
        }
    }
    mqttClient->connect(hostname.c_str());
}


void setup() {
    // Serial.begin(115200);
    Serial.println("Initializing...");
    initialize();
    Serial.println("Initialized");
    // server.begin();
    // setupMulticast();
}


void loop() {
    run();
    static int i(0);
    if (++i % 500000 == 0) {
        Serial.print("Looping...\t");
        Serial.println(mesh.getNodeTime());
    }
    // scheduler.execute();
    // if (!infoReceiver.connected()) {
    //     infoReceiver = server.available();
    // } else {
    //     if (infoReceiver.available() > 0) {
    //         Serial.println(infoReceiver.read());
    //     }
    // }
}
