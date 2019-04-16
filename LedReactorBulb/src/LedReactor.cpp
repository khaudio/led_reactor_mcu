#include "LedReactor.h"

painlessMesh LedReactor::mesh;
LedWriter* LedReactor::writer = nullptr;
bool
    LedReactor::verbose = false,
    LedReactor::connected = false,
    LedReactor::reset = false,
    LedReactor::multicore = false;
uint32_t
    LedReactor::statusIndex = 0,
    LedReactor::meshBridge = 0;
TaskHandle_t
    LedReactor::meshUpdater,
    LedReactor::ledUpdater,
    LedReactor::statusUpdater;
SemaphoreHandle_t LedReactor::semaphore = nullptr;

LedReactor::LedReactor() {}

LedReactor::LedReactor(LedWriter& ledWriter) {
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

void LedReactor::init(uint8_t redPin, uint8_t greenPin, uint8_t bluePin, uint8_t resolution) {
    // Static initializer; required, since so many static elements exist.
    prints("Initializing LedReactor...");
    mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_STA);
    if (writer == nullptr) {
        prints("Creating LedWriter");
        writer = new LedWriter(redPin, greenPin, bluePin, resolution, true);
    }
    staticVerbose = verbose;
    writer->verbose = verbose;
    mesh.onReceive(&LedReactor::receiveMesh);
    prints("Initialized LedReactor");
    // mesh.onNodeTimeAdjusted(&LedReactor::sync);
    mesh.onChangedConnections(&LedReactor::monitorMesh);
    // if (multicore) {
    //     startMulticoreTasks();
    // }
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
    StaticJsonBuffer<2048> buffer;
    JsonObject& parser = buffer.parseObject(json);
    prints("Parsing message...", "");
    if (parser.success()) {
        JsonArray &color = parser["rgb"], &effect = parser["fx"];
        std::array<uint16_t, 3> target = writer->getCurrent();
        if (color.success()) {
            target = {color[0], color[1], color[2]};
            if (verbose) {
                sout << "\tR: " << target[0] << " G: " << target[1] << " B: " << target[2] << std::endl;
            }
        } else {
            prints("");
        }
        if (parser["restart"]) {
            restart();
        }
        if (parser["clear"]) {
            writer->clearEffects(true);
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
            writer->save();
        }
        if (parser["recall"]) {
            writer->recall();
        }
        if (effect.success()) {
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
            std::array<uint16_t, 3> inverse = {effect[11], effect[12], effect[13]};
            duration = selectMode(mode, duration);
            double inverseWidth = selectMode(mode, width);
            repetitions = repetitions > 0 ? repetitions : 1;
            if (recall && (repetitions > 1)) {
                recall = false;
                writer->globalSave->enable(
                        uid, (uid + ((repetitions - 1) * 2))
                    );
            }
            // Use loop for mode 1
            // Effect* created = writer->createEffectAbsolute(
            //         target, duration, recall, start,
            //         startVariation, durationVariation,
            //         uid, updateUID, loop
            //     );
            // if (repetitions > 1 || loop > 0) {
            //     start += static_cast<uint32_t>(duration * 1000000);
            //     if (mode == 1) {
            //         created->hold(width, 1);
            //     }
            // }
            for (int i = 0; i < repetitions; i++) {
                Effect* created = writer->createEffectAbsolute(
                        target, duration, recall, start,
                        startVariation, durationVariation,
                        uid, updateUID, loop
                    );
                if (i < (repetitions - 1)) {
                    width = width ? width : duration;
                    start += static_cast<uint32_t>(duration * 1000000);
                    if ((mode > 0) && (created != nullptr)) {
                        created->hold(width, 1);
                    }
                    Effect* created = writer->createEffectAbsolute(
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
        } else if (color.success()) {
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
}

// void LedReactor::startMulticoreTasks() {
//     prints("Creating tasks");
//     semaphore = xSemaphoreCreateBinary();
//     xTaskCreatePinnedToCore(
//             LedReactor::updateMesh, "updateMesh", 10000, nullptr, 2, &meshUpdater, 0
//         );
//     xTaskCreatePinnedToCore(
//             LedReactor::updateLeds, "updateLeds", 10000, nullptr, 2, &ledUpdater, 1
//         );
//     // xTaskCreatePinnedToCore(
//     //         LedReactor::updateStatus, "updateStatus", 10000, nullptr, 1, &statusUpdater, 1
//     //     );
//     prints("Tasks created");
// }


// void LedReactor::updateMesh(void* parameter) {
//     while (true) {
//         if (xSemaphoreTake(semaphore, portMAX_DELAY) == pdTRUE) {
//             prints("Updating mesh");
//             mesh.update();
//             xSemaphoreGive(semaphore);
//         } else {
//             prints("Mesh could not get semaphore");
//         }
//         vTaskDelay(1000);
//     }
// }

// void LedReactor::updateLeds(void* parameter) {
//     while (true) {
//         if (xSemaphoreTake(semaphore, portMAX_DELAY) == pdTRUE) {
//             prints("Updating LedWriter");
//             uint32_t timeIndex = mesh.getNodeTime();
//             writer->updateClock(&timeIndex);
//             writer->run();
//             xSemaphoreGive(semaphore);
//         } else {
//             prints("LedWriter could not get semaphore");
//         }
//         vTaskDelay(1000);
//     }
// }
// void LedReactor::updateStatus(void* parameter) {
//     while (true) {
//         if (
//                 (verbose && (++statusIndex % 500000 == 0))
//                 && (xSemaphoreTake(semaphore, static_cast<TickType_t>(100)) == pdTRUE)
//             ) {
//             status();
//             xSemaphoreGive(semaphore);
//         } else {
//             prints("Status updater could not get semaphore");
//         }
//         vTaskDelay(100000);
//     }
// }

void LedReactor::run() {
    // Runs necessary processes; required loop for operation.
    // if (!multicore) {
        mesh.update();
        uint32_t timeIndex = mesh.getNodeTime();
        writer->updateClock(&timeIndex);
        writer->run();
        if (verbose && ++statusIndex % 1000000 == 0) {
            status();
        }
    // } else {
    //     vTaskSuspend(NULL);
    // }
}

void LedReactor::status() {
    // Outputs status
    if (verbose) {
        writer->status();
        sout << "Mesh Time: " << mesh.getNodeTime() << "\t";
        sout << "Memory free: " << ESP.getFreeHeap() << std::endl;
    }
}