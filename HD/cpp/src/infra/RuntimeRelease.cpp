#include "RuntimeRelease.h"

#include "bot_logic.h"

#include <chrono>
#include <cstdio>
#include <string>

static std::chrono::steady_clock::time_point g_BotStartTime = std::chrono::steady_clock::now();

void ResetRuntimeClock() {
    g_BotStartTime = std::chrono::steady_clock::now();
}

std::string GetRuntimeStr() {
    bool anyRunning = false;
    for (int i = 0; i < 6; i++) if (g_Bots[i].isRunning) anyRunning = true;
    if (!anyRunning) return "00:00:00";

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_BotStartTime).count();
    int h = static_cast<int>(elapsed / 3600);
    int m = static_cast<int>((elapsed % 3600) / 60);
    int s = static_cast<int>(elapsed % 60);
    char buf[32];
    sprintf_s(buf, "%02d:%02d:%02d", h, m, s);
    return std::string(buf);
}
