#ifndef LEDREACTOR_H
#define LEDREACTOR_H

#include <painlessMesh.h>
#include "LedWriter.h"

// Mesh network information
#define MESH_PREFIX     "reactor"
#define MESH_PASSWORD   "reactPass"
#define MESH_SUBGROUP   "0x0000"
#define MESH_PORT       20002

class LedReactor : public SimpleSerialBase {
    public:
        static TaskHandle_t meshUpdater, ledUpdater, statusUpdater;
        static SemaphoreHandle_t semaphore;
        static const TickType_t maxWaitTime = 100;
        static bool verbose, connected, reset, multicore;
        static uint32_t statusIndex, meshBridge;
        static painlessMesh mesh;
        static LedWriter* writer;
        LedReactor();
        LedReactor(LedWriter&);
        ~LedReactor();
        static void init(
                uint8_t redPin=15,
                uint8_t greenPin=13,
                uint8_t bluePin=12,
                uint8_t resolution=15
            );
        static void stop();
        static void restart();
        static void monitorMesh();
        static void sync(int32_t);
        static double selectMode(int, double);
        static void hold(double, double timeIndex=1, bool all=false);
        static void parseMessage(const char*);
        static void receiveMesh(const uint32_t&, const String&);
        static void startMulticoreTasks();
        static void updateLeds(void*);
        static void updateMesh(void*);
        static void updateStatus(void*);
        static void run();
        static void status();
};

#endif
