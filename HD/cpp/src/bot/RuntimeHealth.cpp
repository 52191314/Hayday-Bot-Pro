#define NOMINMAX
#include "cpp/src/bot/RuntimeHealth.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstring>
#include <utility>

namespace {
    const char* CropModeName(int cropMode) {
        switch (cropMode) {
        case 0: return "Wheat";
        case 1: return "Corn";
        case 2: return "Carrot";
        case 3: return "Soybean";
        case 4: return "Sugarcane";
        default: return "Unknown";
        }
    }

    std::string SafeCharBuffer(const char* value) {
        return value && value[0] != '\0' ? std::string(value) : "";
    }

    std::string DisplayAccountName(const AccountSlot& account) {
        if (account.hasFile && !account.farmName.empty() && account.farmName != "Unknown") return account.farmName;
        if (account.hasFile && !account.name.empty()) return account.name;
        if (account.hasFile) return "Saved account";
        return "Empty slot";
    }

    int CountSavedAccounts(const BotInstance& bot) {
        int count = 0;
        for (int slot = 0; slot < MAX_ACCOUNT_SLOTS; ++slot) {
            if (bot.accounts[slot].hasFile) ++count;
        }
        return count;
    }

    int CountRunnableAccounts(const BotInstance& bot) {
        int count = 0;
        auto skipMask = ParseMultiModeSkipMask(bot.multiModeSkipSlots);
        for (int slot = 0; slot < MAX_ACCOUNT_SLOTS; ++slot) {
            if (bot.accounts[slot].hasFile && !IsMultiModeSlotSkipped(skipMask, slot)) ++count;
        }
        return count;
    }

    int ElapsedSeconds(std::chrono::steady_clock::time_point startedAt, std::chrono::steady_clock::time_point now) {
        if (startedAt.time_since_epoch().count() == 0) return -1;
        long long seconds = std::chrono::duration_cast<std::chrono::seconds>(now - startedAt).count();
        if (seconds < 0) return -1;
        return static_cast<int>((std::min)(seconds, static_cast<long long>(INT_MAX)));
    }

    int RemainingSeconds(std::chrono::steady_clock::time_point until, std::chrono::steady_clock::time_point now) {
        if (until.time_since_epoch().count() == 0 || until <= now) return 0;
        long long seconds = std::chrono::duration_cast<std::chrono::seconds>(until - now).count();
        return static_cast<int>((std::min)(seconds, static_cast<long long>(INT_MAX)));
    }

    bool ContainsStatusToken(const std::string& status, const char* token) {
        return status.find(token) != std::string::npos;
    }

    void RaiseIssue(RuntimeInstanceSnapshot& snapshot, int level, const std::string& issue) {
        if (level > snapshot.attentionLevel) {
            snapshot.attentionLevel = level;
            snapshot.primaryIssue = issue;
        }
        if (snapshot.primaryIssue.empty()) snapshot.primaryIssue = issue;
        snapshot.needsAttention = snapshot.attentionLevel > 0;
    }

    RuntimeInstanceSnapshot BuildInstanceSnapshot(int instanceId, std::chrono::steady_clock::time_point steadyNow, long long epochNow) {
        RuntimeInstanceSnapshot snapshot;
        snapshot.instanceId = instanceId;

        const BotInstance& bot = g_Bots[instanceId];
        int slot = (std::max)(0, (std::min)(MAX_ACCOUNT_SLOTS - 1, bot.currentAccountIndex));
        const AccountSlot& account = bot.accounts[slot];

        snapshot.active = bot.isActive;
        snapshot.running = bot.isRunning.load();
        snapshot.currentSlot = slot;
        snapshot.savedAccounts = CountSavedAccounts(bot);
        snapshot.runnableAccounts = CountRunnableAccounts(bot);
        snapshot.cyclesSinceStart = account.cyclesSinceStart;
        snapshot.salesCycleCount = account.salesCycleCount;
        snapshot.cropMode = bot.testCropMode;
        snapshot.accountName = DisplayAccountName(account);
        snapshot.statusText = bot.statusText.empty() ? (snapshot.running ? "Running" : "Idle") : bot.statusText;
        snapshot.modeLabel = bot.useMultiMode ? (bot.enablePairRotation ? "Pair rotation" : "Rotation") : "Single slot";
        snapshot.cropLabel = CropModeName(bot.testCropMode);
        snapshot.emulatorLabel = bot.emulatorType == 1 ? "LDPlayer" : "MEmu";
        snapshot.adbSerial = SafeCharBuffer(bot.adbSerial);
        snapshot.vmName = SafeCharBuffer(bot.vmName);
        snapshot.hasAdbSerial = !snapshot.adbSerial.empty();
        snapshot.hasCurrentAccount = account.hasFile;
        snapshot.farmState = ToString(account.farmFlowState);
        snapshot.farmDetail = account.farmFlowDetail;
        snapshot.salesState = ToString(account.salesFlowState);
        snapshot.salesDetail = account.salesFlowDetail;
        snapshot.farmStateAgeSeconds = ElapsedSeconds(account.farmFlowStateStartedAt, steadyNow);
        snapshot.salesStateAgeSeconds = ElapsedSeconds(account.salesFlowStateStartedAt, steadyNow);
        snapshot.accountRunSeconds = ElapsedSeconds(account.activeSessionStartedAt, steadyNow);
        snapshot.restRemainingSeconds = RemainingSeconds(account.restUntil, steadyNow);
        snapshot.tomRemainingSeconds = account.nextTomTime > epochNow ? static_cast<int>((std::min)(account.nextTomTime - epochNow, static_cast<long long>(INT_MAX))) : 0;
        if (bot.enablePairRotation && bot.pairRotationStartedAt.time_since_epoch().count() != 0) {
            int elapsed = ElapsedSeconds(bot.pairRotationStartedAt, steadyNow);
            int total = (std::max)(1, bot.pairRotationMinutes) * 60;
            snapshot.pairRotationRemainingSeconds = elapsed >= 0 ? (std::max)(0, total - elapsed) : 0;
        }

        if (snapshot.active && !snapshot.hasAdbSerial) {
            RaiseIssue(snapshot, 2, "Missing ADB serial");
        }
        if (snapshot.active && snapshot.runnableAccounts == 0) {
            RaiseIssue(snapshot, 2, "No runnable saved accounts");
        }
        if (snapshot.running && !snapshot.hasCurrentAccount) {
            RaiseIssue(snapshot, 2, "Current slot is empty");
        }
        if (snapshot.running &&
            (ContainsStatusToken(snapshot.statusText, "ERROR") ||
             ContainsStatusToken(snapshot.statusText, "FAIL") ||
             ContainsStatusToken(snapshot.statusText, "CRITICAL"))) {
            RaiseIssue(snapshot, 2, "Runtime error state");
        }
        if (snapshot.running &&
            (ContainsStatusToken(snapshot.statusText, "WAIT") ||
             ContainsStatusToken(snapshot.statusText, "LOAD") ||
             ContainsStatusToken(snapshot.statusText, "BOOT"))) {
            RaiseIssue(snapshot, 1, "Waiting on emulator/game");
        }
        if (snapshot.running) {
            int longestStateAge = (std::max)(snapshot.farmStateAgeSeconds, snapshot.salesStateAgeSeconds);
            if (longestStateAge > 30 * 60) {
                RaiseIssue(snapshot, 1, "Flow state unchanged 30m+");
            }
        }
        if (account.isShopFullStuck) {
            RaiseIssue(snapshot, 1, "Shop marked full/stuck");
        }
        if (snapshot.restRemainingSeconds > 0) {
            RaiseIssue(snapshot, 1, "Account resting");
        }

        if (snapshot.primaryIssue.empty()) {
            snapshot.primaryIssue = snapshot.running ? "Running normally" : (snapshot.active ? "Ready" : "Disabled");
        }

        return snapshot;
    }
}

std::string FormatRuntimeSeconds(int seconds) {
    if (seconds < 0) return "-";

    int days = seconds / 86400;
    seconds %= 86400;
    int hours = seconds / 3600;
    seconds %= 3600;
    int minutes = seconds / 60;
    seconds %= 60;

    if (days > 0) return std::to_string(days) + "d " + std::to_string(hours) + "h";
    if (hours > 0) return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    if (minutes > 0) return std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    return std::to_string(seconds) + "s";
}

RuntimeHealthSnapshot BuildRuntimeHealthSnapshot() {
    RuntimeHealthSnapshot snapshot;
    auto steadyNow = std::chrono::steady_clock::now();
    long long epochNow = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    snapshot.instances.reserve(6);
    for (int instanceId = 0; instanceId < 6; ++instanceId) {
        RuntimeInstanceSnapshot instance = BuildInstanceSnapshot(instanceId, steadyNow, epochNow);
        if (instance.active) ++snapshot.activeInstances;
        if (instance.running) ++snapshot.runningInstances;
        if (instance.needsAttention) ++snapshot.attentionInstances;
        if (instance.attentionLevel >= 2) ++snapshot.criticalInstances;
        if (instance.running && instance.primaryIssue == "Waiting on emulator/game") ++snapshot.waitingInstances;
        snapshot.savedAccounts += instance.savedAccounts;
        snapshot.runnableAccounts += instance.runnableAccounts;
        snapshot.instances.push_back(std::move(instance));
    }

    return snapshot;
}
