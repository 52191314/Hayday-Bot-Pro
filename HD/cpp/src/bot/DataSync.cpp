#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "cpp/src/bot/RuntimeDeps.h"
#include "cpp/src/bot/DataSync.h"
#include "cpp/src/bot/TestControl.h"

namespace fs = std::filesystem;

static int ReadAnchoredCountSafe(const cv::Mat& screen, const std::string& templatePath) {
    if (screen.empty()) return 0;
    try { return ReadItemCountByAnchor(screen, templatePath); } catch (...) { return 0; }
}

static std::string FormatGameNumberDebugLabel(const char* label, const GameNumberDebugInfo& info) {
    std::string method = info.readMethod.empty() ? "unreadable" : info.readMethod;
    std::string digits = info.digits.empty() ? "-" : info.digits;
    return std::string(label) + "=" + std::to_string(info.count) + " [" + method + " " + std::to_string(info.score) + " | digits=" + digits + "]";
}

std::string BuildHudReadDetail(const GameNumberDebugInfo& coinInfo, const GameNumberDebugInfo& diaInfo, const GameNumberDebugInfo& lvlInfo) {
    return FormatGameNumberDebugLabel("Coin", coinInfo) + " | " +
        FormatGameNumberDebugLabel("Dia", diaInfo) + " | " +
        FormatGameNumberDebugLabel("Lvl", lvlInfo);
}

std::string BuildPreserveReadDetail(const ItemCountDebugInfo& info) {
    std::string method = info.readMethod.empty() ? "unreadable" : info.readMethod;
    std::string digits = info.digits.empty() ? "-" : info.digits;
    std::string roiText = info.roiValid
        ? ("roi=" + std::to_string(info.roi.x) + "," + std::to_string(info.roi.y) + " " + std::to_string(info.roi.width) + "x" + std::to_string(info.roi.height))
        : "roi=invalid";
    return "count=" + std::to_string(info.count) + " [" + method + " " + std::to_string(info.ocrScore) + " | digits=" + digits + " | " + roiText + "]";
}

bool ScanBarnAndSiloFromMarket(int instanceId, int accountIndex, unsigned long long token, bool enforceToken) {
    BotInstance& bot = g_Bots[instanceId];
    auto stillOk = [&]() -> bool {
        if (!bot.isRunning) return false;
        if (enforceToken && !IsTestStillCurrent(instanceId, token)) return false;
        return true;
    };
    auto waitMs = [&](int ms) -> bool {
        int elapsed = 0;
        while (elapsed < ms) {
            if (!stillOk()) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            elapsed += 50;
        }
        return true;
    };
    auto fullFind = [&](const cv::Mat& frame, const std::string& path, float thr, bool gray = false) {
        return FindImage(frame, path, thr, gray, 1.0f, false);
    };
    auto readBarnGroup = [&](const cv::Mat& fullScreen) {
        InventoryData& inv = bot.accounts[accountIndex].currentInv;
        inv.bolt = ReadAnchoredCountSafe(fullScreen, bolt_material_templatePath);
        inv.tape = ReadAnchoredCountSafe(fullScreen, tape_material_templatePath);
        inv.plank = ReadAnchoredCountSafe(fullScreen, plank_material_templatePath);
        inv.deed = ReadAnchoredCountSafe(fullScreen, deed_material_templatePath);
        inv.mallet = ReadAnchoredCountSafe(fullScreen, mallet_material_templatePath);
        inv.marker = ReadAnchoredCountSafe(fullScreen, marker_material_templatePath);
        inv.map = ReadAnchoredCountSafe(fullScreen, map_material_templatePath);
        inv.dynamite = ReadAnchoredCountSafe(fullScreen, dynamite_templatePath);
        inv.tnt = ReadAnchoredCountSafe(fullScreen, tnt_templatePath);
        inv.axe = ReadAnchoredCountSafe(fullScreen, axe_templatePath);
        inv.saw = ReadAnchoredCountSafe(fullScreen, saw_templatePath);
        inv.shovel = ReadAnchoredCountSafe(fullScreen, shovel_templatePath);
        AddLog(instanceId,
            "BARN/SILO SCAN: barn Bolt=" + std::to_string(inv.bolt) +
            ", Tape=" + std::to_string(inv.tape) +
            ", Plank=" + std::to_string(inv.plank) +
            ", Deed=" + std::to_string(inv.deed) +
            ", Mallet=" + std::to_string(inv.mallet) +
            ", Marker=" + std::to_string(inv.marker) +
            ", Map=" + std::to_string(inv.map) +
            ", Dynamite=" + std::to_string(inv.dynamite) +
            ", TNT=" + std::to_string(inv.tnt) +
            ", Axe=" + std::to_string(inv.axe) +
            ", Saw=" + std::to_string(inv.saw) +
            ", Shovel=" + std::to_string(inv.shovel),
            ImVec4(0.6f, 0.95f, 0.85f, 1.0f));
    };
    auto readSiloGroup = [&](const cv::Mat& fullScreen) {
        InventoryData& inv = bot.accounts[accountIndex].currentInv;
        inv.nail = ReadAnchoredCountSafe(fullScreen, nail_material_templatePath);
        inv.screw = ReadAnchoredCountSafe(fullScreen, screw_material_templatePath);
        inv.panel = ReadAnchoredCountSafe(fullScreen, panel_material_templatePath);
        AddLog(instanceId,
            "BARN/SILO SCAN: silo Nail=" + std::to_string(inv.nail) +
            ", Screw=" + std::to_string(inv.screw) +
            ", Panel=" + std::to_string(inv.panel),
            ImVec4(0.75f, 0.9f, 0.95f, 1.0f));
    };

    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    MatchResult emptyCrate = fullFind(screen, crate_templatePath, std::max(0.70f, g_Thresholds.crateThreshold - 0.10f), false);
    if (!emptyCrate.found) {
        MatchResult shopRes = fullFind(screen, shop_templatePath, std::max(0.70f, g_Thresholds.shopThreshold - 0.08f), false);
        if (!shopRes.found) {
            AddLog(instanceId, "BARN/SILO SCAN: market/shop not found.", ImVec4(1, 0.5f, 0, 1));
            return false;
        }
        AddLog(instanceId, "BARN/SILO SCAN: opening market/shop...", ImVec4(0, 1, 1, 1));
        AdbTap(instanceId, shopRes.x, shopRes.y);
        if (!waitMs(std::max(1800, g_Intervals.shopEnterWait))) return false;
        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        emptyCrate = fullFind(screen, crate_templatePath, std::max(0.70f, g_Thresholds.crateThreshold - 0.10f), false);
    }
    if (!emptyCrate.found) {
        AddLog(instanceId, "BARN/SILO SCAN: empty crate not found.", ImVec4(1, 0.5f, 0, 1));
        return false;
    }
    AddLog(instanceId, "BARN/SILO SCAN: opening empty crate first...", ImVec4(0, 1, 0, 1));
    AdbTap(instanceId, emptyCrate.x, emptyCrate.y);
    if (!waitMs(std::max(900, g_Intervals.crateClickWait))) return false;

    screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    MatchResult barnRes = fullFind(screen, barn_market_templatePath, std::max(0.65f, g_Thresholds.barnTabThreshold), false);
    if (!barnRes.found) {
        AddLog(instanceId, "BARN/SILO SCAN: barn tab not found.", ImVec4(1, 0.5f, 0, 1));
        return false;
    }
    AddLog(instanceId, "BARN/SILO SCAN: opening barn tab...", ImVec4(0, 1, 1, 1));
    AdbTap(instanceId, barnRes.x, barnRes.y);
    if (!waitMs(1200)) return false;
    screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    if (screen.empty()) return false;
    readBarnGroup(screen);

    MatchResult siloRes = fullFind(screen, silo_market_templatePath, std::max(0.65f, g_Thresholds.siloTabThreshold), false);
    if (!siloRes.found) {
        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        siloRes = fullFind(screen, silo_market_templatePath, std::max(0.65f, g_Thresholds.siloTabThreshold), false);
    }
    if (!siloRes.found) {
        AddLog(instanceId, "BARN/SILO SCAN: silo tab not found.", ImVec4(1, 0.5f, 0, 1));
        return false;
    }
    AddLog(instanceId, "BARN/SILO SCAN: opening silo tab...", ImVec4(0, 1, 1, 1));
    AdbTap(instanceId, siloRes.x, siloRes.y);
    if (!waitMs(1200)) return false;
    screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    if (screen.empty()) return false;
    readSiloGroup(screen);

    SaveInventoryData();

    AddLog(instanceId, "BARN/SILO SCAN: exiting with 2 direct taps...", ImVec4(0, 1, 1, 1));
    AdbTap(instanceId, g_CoordinateProfile.salePanelCloseX, g_CoordinateProfile.salePanelCloseY);
    if (!waitMs(500)) return false;
    AdbTap(instanceId, g_CoordinateProfile.marketCloseSecondX, g_CoordinateProfile.marketCloseSecondY);
    if (!waitMs(700)) return false;
    return true;
}

bool SyncHudDataForSlot(int instanceId, int slotIndex) {
    if (instanceId < 0 || instanceId >= 6 || slotIndex < 0 || slotIndex >= 15) return false;

    BotInstance& bot = g_Bots[instanceId];
    if (bot.isRunning) {
        AddLog(instanceId, "Manual HUD sync skipped because the bot is currently running.", ImVec4(1.0f, 0.55f, 0.2f, 1.0f));
        return false;
    }

    AccountSlot& account = bot.accounts[slotIndex];
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    if (screen.empty()) {
        AddLog(instanceId, "Manual HUD sync failed: screen capture was empty.", ImVec4(1, 0.3f, 0.3f, 1));
        return false;
    }

    bot.currentAccountIndex = slotIndex;
    GameNumberDebugInfo coinInfo = ReadGameNumberDebug(screen, cv::Rect(507, 2, 72, 21), "coin");
    GameNumberDebugInfo diaInfo = ReadGameNumberDebug(screen, cv::Rect(547, 22, 34, 21), "dia");
    GameNumberDebugInfo levelInfo = ReadGameNumberDebug(screen, cv::Rect(16, 8, 22, 20), "lvl");

    account.coinAmount = coinInfo.count;
    account.diamondAmount = diaInfo.count;
    if (levelInfo.count > 0) account.level = levelInfo.count;
    account.hudReadDetail = BuildHudReadDetail(coinInfo, diaInfo, levelInfo);

    SaveConfig();
    AddLog(instanceId,
        "Manual HUD sync complete for slot " + std::to_string(slotIndex + 1) +
        ": coins=" + std::to_string(account.coinAmount) +
        ", diamonds=" + std::to_string(account.diamondAmount) +
        ", level=" + std::to_string(account.level) + " | " + account.hudReadDetail + ".",
        ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
    return true;
}

bool SyncBarnSiloDataForSlot(int instanceId, int slotIndex) {
    if (instanceId < 0 || instanceId >= 6 || slotIndex < 0 || slotIndex >= 15) return false;

    BotInstance& bot = g_Bots[instanceId];
    if (bot.isRunning) {
        AddLog(instanceId, "Manual Barn/Silo sync skipped because the bot is currently running.", ImVec4(1.0f, 0.55f, 0.2f, 1.0f));
        return false;
    }

    bot.currentAccountIndex = slotIndex;
    AddLog(instanceId, "Manual Barn/Silo sync: opening market and scanning stored materials...", ImVec4(0.2f, 0.9f, 1.0f, 1.0f));
    bool ok = ScanBarnAndSiloFromMarket(instanceId, slotIndex, 0ULL, false);
    if (ok) {
        AddLog(instanceId, "Manual Barn/Silo sync complete for slot " + std::to_string(slotIndex + 1) + ".", ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
    }
    else {
        AddLog(instanceId, "Manual Barn/Silo sync failed for slot " + std::to_string(slotIndex + 1) + ".", ImVec4(1, 0.3f, 0.3f, 1));
    }
    return ok;
}

bool SyncAllDataForSlot(int instanceId, int slotIndex) {
    bool hudOk = SyncHudDataForSlot(instanceId, slotIndex);
    bool barnOk = SyncBarnSiloDataForSlot(instanceId, slotIndex);
    return hudOk && barnOk;
}
