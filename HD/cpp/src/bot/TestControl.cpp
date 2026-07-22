#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "cpp/src/bot/RuntimeDeps.h"
#include "cpp/src/bot/TestControl.h"

std::atomic<unsigned long long> g_TestGeneration[6]{};

bool IsTestStillCurrent(int instanceId, unsigned long long token) {
    return g_TestGeneration[instanceId].load() == token;
}
