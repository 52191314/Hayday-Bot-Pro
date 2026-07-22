#pragma once

#include <atomic>

extern std::atomic<unsigned long long> g_TestGeneration[6];
bool IsTestStillCurrent(int instanceId, unsigned long long token);
