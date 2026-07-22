#pragma once

#include "bot_logic.h"

#include <string>
#include <vector>

struct RuntimeInstanceSnapshot {
    int instanceId = -1;
    bool active = false;
    bool running = false;
    bool hasAdbSerial = false;
    bool hasCurrentAccount = false;
    bool needsAttention = false;
    int attentionLevel = 0; // 0=ok, 1=warning, 2=critical
    int currentSlot = 0;
    int savedAccounts = 0;
    int runnableAccounts = 0;
    int cyclesSinceStart = 0;
    int salesCycleCount = 0;
    int cropMode = 0;
    int farmStateAgeSeconds = -1;
    int salesStateAgeSeconds = -1;
    int accountRunSeconds = -1;
    int restRemainingSeconds = 0;
    int tomRemainingSeconds = 0;
    int pairRotationRemainingSeconds = 0;
    std::string accountName;
    std::string statusText;
    std::string modeLabel;
    std::string cropLabel;
    std::string emulatorLabel;
    std::string adbSerial;
    std::string vmName;
    std::string farmState;
    std::string farmDetail;
    std::string salesState;
    std::string salesDetail;
    std::string primaryIssue;
};

struct RuntimeHealthSnapshot {
    int activeInstances = 0;
    int runningInstances = 0;
    int waitingInstances = 0;
    int attentionInstances = 0;
    int criticalInstances = 0;
    int savedAccounts = 0;
    int runnableAccounts = 0;
    std::vector<RuntimeInstanceSnapshot> instances;
};

RuntimeHealthSnapshot BuildRuntimeHealthSnapshot();
std::string FormatRuntimeSeconds(int seconds);
