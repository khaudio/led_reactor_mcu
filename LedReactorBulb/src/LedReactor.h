#ifndef LEDREACTOR_H
#define LEDREACTOR_H

#include <painlessMesh.h>
#include <LedWriter.h>

// Mesh network information
#define MESH_PREFIX     "reactor"
#define MESH_PASSWORD   "reactPass"
#define MESH_SUBGROUP   "0x0000"
#define MESH_PORT       20002

// Prevent LedWriter from running itself
#define USE_TASKS       false

class LedReactor : public SimpleSerialBase {
    public:
        static bool verbose, connected, reset;
        static uint32_t statusIndex;
        static painlessMesh mesh;
        static LedWriter<4>* writer;
        LedReactor();
        LedReactor(LedWriter<4>&);
        ~LedReactor();
        static void init(
                uint8_t redPin=13,
                uint8_t greenPin=12,
                uint8_t bluePin=15,
                uint8_t whitePin=27,
                uint8_t resolution=10
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
