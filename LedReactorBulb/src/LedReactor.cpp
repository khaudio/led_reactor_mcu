#include "LedReactor.h"

painlessMesh LedReactor::mesh;
LedWriter<4>* LedReactor::writer = nullptr;
bool
    LedReactor::verbose = false,
    LedReactor::connected = false,
    LedReactor::reset = false;
uint32_t LedReactor::statusIndex = 0;

LedReactor::LedReactor() {}

LedReactor::LedReactor(LedWriter<4>& ledWriter) {
    writer = &ledWriter;
}

LedReactor::~LedReactor() {
    stop();
}

void LedReactor::stop() {
    delete writer;
    writer = nullptr;
    mesh.stop();
}

void LedReactor::restart() {
    writer->clearEffects();
    prints("Resetting");
    esp_restart();
}

void LedReactor::monitorMesh() {
    std::list<uint32_t> nodeList = mesh.getNodeList();
    if (!nodeList.empty()) {
        connected = true;
    } else if (nodeList.empty() && connected && reset) {
        restart();
    }
}

void LedReactor::init(
        uint8_t redPin, uint8_t greenPin,
        uint8_t bluePin, uint8_t whitePin,
        uint8_t resolution
    ) {
    // Static initializer; required, since so many static elements exist.
    prints("Initializing LedReactor...");
    mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_STA);
    if (writer == nullptr) {
        prints("Creating LedWriter");
        std::array<uint8_t, 4> pins = {redPin, greenPin, bluePin, whitePin};
        writer = new LedWriter<4>(pins, resolution, true);
    }
    staticVerbose = verbose;
    writer->verbose = verbose;
    mesh.onReceive(&LedReactor::receiveMesh);
    prints("Initialized LedReactor");
    mesh.onChangedConnections(&LedReactor::monitorMesh);
    // mesh.onNodeTimeAdjusted(&LedReactor::sync);
    // xTaskCreate(updateLeds, "ledUpdater", 40000, NULL, 1, NULL);
}

void LedReactor::sync(int32_t offset) {
    prints("Synchronized with mesh");
    uint32_t now = mesh.getNodeTime();
    writer->updateClock(&now, true);
}

double LedReactor::selectMode(int mode, double duration) {
    // Selects effect waveshape
    switch (mode) {
        case 1: // Square wave pulse
            return 1e-6;
        default: // Default to linear fade
            return duration;
    }
}

void LedReactor::parseMessage(const char* json) {
    // Parses messages received by mesh and applies them
    prints("Parsing message...", "");
    StaticJsonDocument<2048> parser;
    auto error = deserializeJson(parser, json);
    if (!error) {
        JsonArray color = parser["rgbw"], effect = parser["fx"];
        std::array<uint16_t, 4> target = writer->getCurrent();
        if (parser["rgbw"]) {
            target = {color[0], color[1], color[2], color[3]};
            if (verbose) {
                sout << "\tR: " << target[0] << " G: " << target[1] << " B: " << target[2] << " W: " << target[3] << std::endl;
            }
        } else {
            prints("");
        }
        if (parser["restart"]) {
            prints("Restarting...");
            restart();
        }
        if (parser["clear"]) {
            prints("Clearing...");
            writer->clearEffects(true);
            prints("Cleared");
        }
        if (parser["test"]) {
            prints("Test message received");
            writer->test();
            status();
        }
        if (parser["status"]) {
            bool lastThis = staticVerbose, lastWriter = writer->verbose;
            staticVerbose = true;
            writer->verbose = true;
            status();
            staticVerbose = lastThis;
            writer->verbose = lastWriter;
        }
        if (parser["save"]) {
            prints("Saving...");
            writer->save();
            prints("Saved");
        }
        if (parser["recall"]) {
            prints("Recalling...");
            writer->recall();
            prints("Recalled");
        }
        if (parser["fx"]) {
            prints("Parsing effect...");
            double duration = effect[0];
            bool recall = effect[1];
            uint32_t start = effect[2];
            double startVariation = effect[3];
            double durationVariation = effect[4];
            uint32_t uid = effect[5];
            bool updateUID = effect[6];
            uint32_t repetitions = effect[7];
            int mode = effect[8];
            double width = effect[9];
            int32_t loop = effect[10];
            std::array<uint16_t, 4> inverse = {effect[11], effect[12], effect[13], effect[14]};
            duration = selectMode(mode, duration);
            double inverseWidth = selectMode(mode, width);
            repetitions = repetitions > 0 ? repetitions : 1;
            if (recall && (repetitions > 1)) {
                recall = false;
                writer->globalSave->enable(
                        uid, static_cast<uint32_t>(uid + ((repetitions - 1) * 2))
                    );
            }
            for (int i = 0; i < repetitions; i++) {
                Effect<4>* created = writer->createEffectAbsolute(
                        target, duration, recall, start,
                        startVariation, durationVariation,
                        uid, updateUID, loop
                    );
                if (i < (repetitions - 1)) {
                    inverseWidth = ((inverseWidth > 0) ? inverseWidth : duration);
                    start += static_cast<uint32_t>(duration * 1000000);
                    if ((mode > 0) && (created != nullptr)) {
                        created->hold(width, 1);
                    }
                    Effect<4>* created = writer->createEffectAbsolute(
                            inverse, inverseWidth, recall, start,
                            startVariation, durationVariation,
                            ++uid, updateUID, loop
                        );
                    start += static_cast<uint32_t>(width * 1000000);
                    if ((mode > 0) && (created != nullptr)) {
                        created->hold(width, 1);
                    }
                    uid++;
                } else if (width) {
                    created->hold(width, 1);
                }
            }
            prints("Set effect");
        } else if (parser["rgbw"]) {
            prints("Effect parsing failed");
            if (writer->looping() == -1) {
                prints("Updating looping effect targets");
                writer->updateEffects(target);
            } else {
                writer->set(target, false);
                prints("Set received values");
            }
        }
        prints("Message parsed successfully");
    } else {
        prints("Message parsing failed");
    }
}

void LedReactor::receiveMesh(const uint32_t& sender, const String& message) {
    // Callback for messages received by mesh network
    prints("Receiving...");
    parseMessage(message.c_str());
    prints("Received");
}

void LedReactor::updateLeds(void* parameter) {
    while (true) {
        uint32_t timeIndex = mesh.getNodeTime();
        writer->updateClock(&timeIndex);
        writer->run();
        vTaskDelay(2); // Ideally, use vTaskDelayUntil()
    }
}

void LedReactor::run() {
    // Runs necessary processes; required loop for operation.
    mesh.update();
    if (verbose && ++statusIndex % 1000000 == 0) {
        status();
    }
    
    uint32_t timeIndex = mesh.getNodeTime();
    writer->updateClock(&timeIndex);
    writer->run();
}

void LedReactor::status() {
    // Outputs status
    writer->status();
    sout << "Mesh Time: " << mesh.getNodeTime() << "\t";
    sout << "Memory free: " << ESP.getFreeHeap() << std::endl;
}
