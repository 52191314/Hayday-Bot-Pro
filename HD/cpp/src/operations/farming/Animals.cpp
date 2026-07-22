#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "Animals.h"

#include "Farming.h"
#include "cpp/src/infra/Adb.h"
#include "cpp/src/infra/MinitouchClient.h"
#include "cpp/src/infra/NativeCapture.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

namespace fs = std::filesystem;

extern TemplateThresholds g_Thresholds;
extern BotInstance g_Bots[6];
extern std::string kAdbPath;

extern std::string not_enough_templatePath;
extern std::string cow_feed_templatePath;
extern std::string feed_mill_templatePath;
extern std::string cow_pasture_templatePath;
extern std::string feed_mill_cross_templatePath;
extern std::string dairy_templatePath;
extern std::string cream_templatePath;
extern std::string butter_templatePath;
extern std::string cheese_templatePath;

extern void AddLog(int instanceId, std::string message, ImVec4 color);
extern MatchResult FindBestCrossAny(const cv::Mat& screen);
bool WaitForBotActionWindow(int instanceId, int totalMs, bool ignoreBotState = false);
bool ZoomOutFarmView(int instanceId, const char* reason, int repeatCount = 2, bool ignoreBotState = false);
const char* ToString(CowFeedRoutineResult result) {
    switch (result) {
    case CowFeedRoutineResult::Completed: return "Completed";
    case CowFeedRoutineResult::NotEnoughResources: return "Not enough resources";
    case CowFeedRoutineResult::SkippedMissingTemplate: return "Skipped (template missing)";
    case CowFeedRoutineResult::Failed: return "Failed";
    default: return "Unknown";
    }
}

bool ShouldRunCowFeed(const AccountSlot& account) {
    if (!account.autoCowFeedEnabled) return false;
    if (account.lastCowFeedRun.time_since_epoch().count() == 0) return true;
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - account.lastCowFeedRun).count();
    return elapsed >= 60;
}

int GetAccountCropWaitSeconds(const AccountSlot& account, int configuredCropMode) {
    if (account.cowFeedCropRecoveryPlanted) {
        return (std::max)(
            GetCropRuntimeInfo(kCowFeedCornCropMode).growSeconds,
            GetCropRuntimeInfo(kCowFeedSoybeanCropMode).growSeconds);
    }
    return GetCropRuntimeInfo(configuredCropMode).growSeconds;
}

int GetDairyCooldownMinutes(int dairyProductMode) {
    switch ((std::max)(0, (std::min)(2, dairyProductMode))) {
    case 1: return 30; // Butter
    case 2: return 60; // Cheese
    default: return 20; // Cream
    }
}

const char* GetDairyProductName(int dairyProductMode) {
    switch ((std::max)(0, (std::min)(2, dairyProductMode))) {
    case 1: return "Butter";
    case 2: return "Cheese";
    default: return "Cream";
    }
}

bool ShouldRunDairy(const AccountSlot& account) {
    if (!account.autoDairyEnabled) return false;
    if (account.lastDairyRun.time_since_epoch().count() == 0) return true;
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - account.lastDairyRun).count();
    return elapsed >= GetDairyCooldownMinutes(account.dairyProductMode);
}

static cv::Rect BuildRectAroundTemplateMatch(const cv::Mat& screen, const std::string& templatePath, const MatchResult& match, int expandX = 0, int expandY = 0) {
    int halfWidth = 40;
    int halfHeight = 40;
    cv::Mat templ = cv::imread(ResolveTemplatePath(templatePath), cv::IMREAD_UNCHANGED);
    if (!templ.empty()) {
        halfWidth = templ.cols / 2;
        halfHeight = templ.rows / 2;
    }

    int left = match.x - halfWidth - expandX;
    int top = match.y - halfHeight - expandY;
    int width = (halfWidth * 2) + (expandX * 2);
    int height = (halfHeight * 2) + (expandY * 2);

    cv::Rect rect(left, top, width, height);
    return rect & cv::Rect(0, 0, screen.cols, screen.rows);
}

CowFeedRoutineResult RunCowFeedRoutine(int instanceId, int accountIndex, bool isTestRun) {
    BotInstance& bot = g_Bots[instanceId];
    if (!bot.isRunning && !isTestRun) return CowFeedRoutineResult::Failed;
    AccountSlot& account = bot.accounts[accountIndex];
    const int queueTarget = account.cowFeedUseFourSlots ? 4 : 3;
    const std::string grassTemplatePath = "templates\\grass.png";
    const std::string notEnoughResourcesTemplatePath = "templates\\not_enough_resources.png";
    const POINT harvestToolOffsetFromFeed{ -45, 29 };

    auto fullFind = [&](const cv::Mat& frame, const std::string& path, float thr, bool gray = false) {
        return FindImage(frame, path, thr, gray, 1.0f, false);
    };
    auto templateExists = [&](const std::string& path) {
        std::error_code ec;
        return fs::exists(ResolveTemplatePath(path), ec);
    };
    auto findNotEnough = [&](const cv::Mat& frame) {
        MatchResult best = fullFind(frame, not_enough_templatePath, g_Thresholds.notEnoughThreshold, false);
        if (!best.found && templateExists(notEnoughResourcesTemplatePath)) {
            best = fullFind(frame, notEnoughResourcesTemplatePath, g_Thresholds.notEnoughThreshold, false);
        }
        return best;
    };
    auto findPastureFeedTool = [&](const cv::Mat& frame) {
        return fullFind(frame, cow_feed_templatePath, g_Thresholds.cowFeedThreshold, false);
    };
    auto findFeedMills = [&](const cv::Mat& frame) {
        std::vector<MatchResult> found = FindAllImages(frame, feed_mill_templatePath, g_Thresholds.feedMillThreshold, 90, false);
        if (found.empty()) {
            MatchResult single = fullFind(frame, feed_mill_templatePath, g_Thresholds.feedMillThreshold, false);
            if (single.found) found.push_back(single);
        }
        if (found.size() > 2) found.resize(2);
        return found;
    };
    auto findCowPastures = [&](const cv::Mat& frame) {
        std::vector<MatchResult> found = FindAllImages(frame, cow_pasture_templatePath, g_Thresholds.cowPastureThreshold, 90, false);
        if (found.empty()) {
            MatchResult single = fullFind(frame, cow_pasture_templatePath, g_Thresholds.cowPastureThreshold, false);
            if (single.found) found.push_back(single);
        }
        return found;
    };
    auto buildPastureSweep = [&](const cv::Rect& pastureRect) {
        int left = pastureRect.x + (std::max)(12, pastureRect.width / 8);
        int right = pastureRect.x + pastureRect.width - (std::max)(12, pastureRect.width / 8);
        int top = pastureRect.y + (std::max)(12, pastureRect.height / 8);
        int bottom = pastureRect.y + pastureRect.height - (std::max)(12, pastureRect.height / 8);
        int midX = pastureRect.x + pastureRect.width / 2;
        int midY = pastureRect.y + pastureRect.height / 2;
        return std::vector<MatchResult>{
            MatchResult{ true, left, top, 1.0 },
            MatchResult{ true, midX, top, 1.0 },
            MatchResult{ true, right, top, 1.0 },
            MatchResult{ true, left, midY, 1.0 },
            MatchResult{ true, midX, midY, 1.0 },
            MatchResult{ true, right, midY, 1.0 },
            MatchResult{ true, left, bottom, 1.0 },
            MatchResult{ true, midX, bottom, 1.0 },
            MatchResult{ true, right, bottom, 1.0 }
        };
    };
    auto closeVisiblePanel = [&](const char* reason) {
        cv::Mat closeScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult closeRes = fullFind(closeScreen, feed_mill_cross_templatePath, g_Thresholds.feedMillCrossThreshold, false);
        if (!closeRes.found) closeRes = FindBestCrossAny(closeScreen);
        if (closeRes.found) {
            AddLog(instanceId, std::string("Cow Feed: closing panel after ") + reason + ".", ImVec4(0.8f, 0.8f, 0.4f, 1.0f));
            AdbTap(instanceId, closeRes.x, closeRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    };
    auto clickNeutralGrass = [&](const char* reason) {
        if (!templateExists(grassTemplatePath)) return false;
        cv::Mat grassScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult grass = fullFind(grassScreen, grassTemplatePath, (std::max)(0.65f, g_Thresholds.soilThreshold - 0.08f), false);
        if (!grass.found) return false;
        AddLog(instanceId, std::string("Cow Feed: clicking grass after ") + reason + ".", ImVec4(0.6f, 0.9f, 0.6f, 1.0f));
        // AdbTap(instanceId, grass.x, grass.y);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return true;
    };
    auto harvestReadyCows = [&]() -> int {
        cv::Mat harvestScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        std::vector<MatchResult> pastures = findCowPastures(harvestScreen);
        if (pastures.empty() && ZoomOutFarmView(instanceId, "cow harvest pasture recovery", 1, isTestRun)) {
            if (!WaitForBotActionWindow(instanceId, 500, isTestRun)) return 0;
            harvestScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            pastures = findCowPastures(harvestScreen);
        }
        if (pastures.empty()) {
            AddLog(instanceId, "Cow Feed: no cow pasture found for pre-feed harvest pass.", ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
            return 0;
        }

        int harvestedPastures = 0;
        for (size_t pastureIndex = 0; pastureIndex < pastures.size() && pastureIndex < 2; ++pastureIndex) {
            if (!bot.isRunning && !isTestRun) break;

            MatchResult pasture = pastures[pastureIndex];
            cv::Rect pastureRect = BuildRectAroundTemplateMatch(harvestScreen, cow_pasture_templatePath, pasture, 18, 18);
            std::vector<MatchResult> pastureSweep = buildPastureSweep(pastureRect);

            AddLog(instanceId, "Cow Feed: opening pasture for pre-feed harvest " + std::to_string(pastureIndex + 1) + "/2...", ImVec4(0.2f, 0.9f, 1.0f, 1.0f));
            AdbTap(instanceId, pasture.x, pasture.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(850));

            cv::Mat openScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult feedTool = findPastureFeedTool(openScreen);
            if (!feedTool.found) {
                AddLog(instanceId, "Cow Feed: pasture tool anchor not found; skipping harvest for this pasture.", ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
                closeVisiblePanel("missing pasture tool during cow harvest");
                harvestScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                continue;
            }

            int harvestX = feedTool.x + harvestToolOffsetFromFeed.x;
            int harvestY = feedTool.y + harvestToolOffsetFromFeed.y;
            if (!openScreen.empty()) {
                harvestX = (std::max)(0, (std::min)(openScreen.cols - 1, harvestX));
                harvestY = (std::max)(0, (std::min)(openScreen.rows - 1, harvestY));
            }
            AddLog(instanceId, "Cow Feed: harvesting cows with offset tool anchor (" + std::to_string(harvestToolOffsetFromFeed.x) + "," + std::to_string(harvestToolOffsetFromFeed.y) + ")...", ImVec4(0.2f, 0.9f, 0.6f, 1.0f));
            ExecuteDenseGridGesture(instanceId, harvestX, harvestY, pastureSweep);
            std::this_thread::sleep_for(std::chrono::milliseconds(650));

            closeVisiblePanel("pre-feed cow harvest");
            ++harvestedPastures;
            harvestScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        }

        return harvestedPastures;
    };

    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    int harvestedPastures = harvestReadyCows();
    if (harvestedPastures > 0) {
        AddLog(instanceId, "Cow Feed: pre-feed cow harvest pass completed for " + std::to_string(harvestedPastures) + " pasture(s).", ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    }

    std::vector<MatchResult> feedMills = findFeedMills(screen);
    if (feedMills.empty()) {
        AddLog(instanceId, "Cow Feed: feed mill templates not found. Attempting zoom-out recovery...", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        if (ZoomOutFarmView(instanceId, "feed mill recovery", 1, isTestRun)) {
            if (!WaitForBotActionWindow(instanceId, 700, isTestRun)) {
                return CowFeedRoutineResult::Failed;
            }
            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            feedMills = findFeedMills(screen);
        }
    }
    if (feedMills.empty()) {
        AddLog(instanceId, "Cow Feed: feed mill template not found.", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        return CowFeedRoutineResult::SkippedMissingTemplate;
    }

    int processedMills = 0;
    for (size_t millIndex = 0; millIndex < feedMills.size() && millIndex < 2; ++millIndex) {
        MatchResult feedMill = feedMills[millIndex];
        AddLog(instanceId,
            "Cow Feed: opening feed mill " + std::to_string(millIndex + 1) + "/" + std::to_string((std::min)(feedMills.size(), static_cast<size_t>(2))) + "...",
            ImVec4(0.2f, 0.9f, 1.0f, 1.0f));
        AdbTap(instanceId, feedMill.x, feedMill.y);
        std::this_thread::sleep_for(std::chrono::milliseconds(900));

        for (int queueIndex = 1; queueIndex <= queueTarget; ++queueIndex) {
            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult cowFeed = fullFind(screen, cow_feed_templatePath, g_Thresholds.cowFeedThreshold, false);
            if (!cowFeed.found) {
                AddLog(instanceId, "Cow Feed: cow feed template not found in the feed mill dialog.", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
                closeVisiblePanel("missing cow feed template");
                return processedMills > 0 ? CowFeedRoutineResult::Completed : CowFeedRoutineResult::SkippedMissingTemplate;
            }

            std::vector<POINT> millDrag = {
                POINT{ cowFeed.x, cowFeed.y },
                POINT{ (cowFeed.x + feedMill.x) / 2, (cowFeed.y + feedMill.y) / 2 },
                POINT{ feedMill.x, feedMill.y }
            };
            AddLog(instanceId,
                "Cow Feed: queueing cow feed in mill " + std::to_string(millIndex + 1) + " drag " +
                std::to_string(queueIndex) + "/" + std::to_string(queueTarget) + "...",
                ImVec4(0.2f, 0.9f, 0.6f, 1.0f));
            if (!Minitouch::DragPath(instanceId, millDrag)) {
                AddLog(instanceId, "Cow Feed: minitouch drag failed while queueing feed.", ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                closeVisiblePanel("failed feed-mill drag");
                return CowFeedRoutineResult::Failed;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult notEnough = findNotEnough(screen);
            if (notEnough.found) {
                AddLog(instanceId, "Cow Feed: not enough resources detected. Closing the feed mill and skipping.", ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
                closeVisiblePanel("not enough resources");
                clickNeutralGrass("feed-mill resource shortage");
                return CowFeedRoutineResult::NotEnoughResources;
            }
        }

        ++processedMills;
        if (!clickNeutralGrass("feed mill")) {
            closeVisiblePanel("feed mill");
        }
        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    }

    screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    std::vector<MatchResult> pastures = findCowPastures(screen);
    if (pastures.empty()) {
        AddLog(instanceId, "Cow Feed: cow pasture template not found.", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        return CowFeedRoutineResult::SkippedMissingTemplate;
    }

    int fedPastures = 0;
    for (size_t pastureIndex = 0; pastureIndex < pastures.size() && pastureIndex < 2; ++pastureIndex) {
        MatchResult pasture = pastures[pastureIndex];
        cv::Rect pastureRect = BuildRectAroundTemplateMatch(screen, cow_pasture_templatePath, pasture, 18, 18);

        AddLog(instanceId, "Cow Feed: opening cow pasture " + std::to_string(pastureIndex + 1) + "/" + std::to_string((std::min)(pastures.size(), static_cast<size_t>(2))) + "...", ImVec4(0.2f, 0.9f, 1.0f, 1.0f));
        AdbTap(instanceId, pasture.x, pasture.y);
        std::this_thread::sleep_for(std::chrono::milliseconds(900));

        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult pastureFeed = findPastureFeedTool(screen);
        if (!pastureFeed.found) {
            AddLog(instanceId, "Cow Feed: cow feed template not found after opening the pasture.", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
            closeVisiblePanel("opening cow pasture");
            return fedPastures > 0 ? CowFeedRoutineResult::Completed : CowFeedRoutineResult::SkippedMissingTemplate;
        }

        std::vector<MatchResult> pastureSweep = buildPastureSweep(pastureRect);

        AddLog(instanceId, "Cow Feed: feeding pasture with 2-finger sweep...", ImVec4(0.2f, 0.9f, 0.6f, 1.0f));
        ExecuteDenseGridGesture(instanceId, pastureFeed.x, pastureFeed.y, pastureSweep);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult pastureNotEnough = findNotEnough(screen);
        if (pastureNotEnough.found) {
            AddLog(instanceId, "Cow Feed: not enough feed/resources while feeding pasture. Closing popup.", ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
            closeVisiblePanel("pasture not enough resources");
            clickNeutralGrass("pasture resource shortage");
            return CowFeedRoutineResult::NotEnoughResources;
        }

        if (!clickNeutralGrass("feeding cows")) {
            closeVisiblePanel("feeding cows");
        }
        ++fedPastures;
        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    }

    AddLog(instanceId, isTestRun ? "TEST COW FEED: routine completed." : "Cow Feed: routine completed.", ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
    return CowFeedRoutineResult::Completed;
}


bool RunDairyRoutine(int instanceId, int accountIndex, bool isTestRun) {
    BotInstance& bot = g_Bots[instanceId];
    if (!bot.isRunning && !isTestRun) return false;
    AccountSlot& account = bot.accounts[accountIndex];

    const int productMode = (std::max)(0, (std::min)(2, account.dairyProductMode));
    const int queueTarget = (std::max)(1, (std::min)(4, account.dairyQueueCount));
    const std::array<POINT, 3> productOffsetsFromCream = {
        POINT{0, 0},       // Cream, user-provided anchor center: 225,172
        POINT{-51, 24},    // Butter center: 174,196
        POINT{-90, 58}     // Cheese center: 135,230
    };
    std::array<std::string*, 3> productTemplates = {
        &cream_templatePath,
        &butter_templatePath,
        &cheese_templatePath
    };

    auto fullFind = [&](const cv::Mat& frame, const std::string& path, float thr, bool gray = false) {
        return FindImage(frame, path, thr, gray, 1.0f, false);
    };
    auto templateExists = [&](const std::string& path) {
        std::error_code ec;
        return fs::exists(ResolveTemplatePath(path), ec);
    };
    auto findNotEnough = [&](const cv::Mat& frame) {
        MatchResult best = fullFind(frame, not_enough_templatePath, g_Thresholds.notEnoughThreshold, false);
        const std::string notEnoughResourcesTemplatePath = "templates\\not_enough_resources.png";
        if (!best.found && templateExists(notEnoughResourcesTemplatePath)) {
            best = fullFind(frame, notEnoughResourcesTemplatePath, g_Thresholds.notEnoughThreshold, false);
        }
        return best;
    };
    auto closeVisiblePanel = [&](const char* reason) {
        cv::Mat closeScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult closeRes = fullFind(closeScreen, feed_mill_cross_templatePath, g_Thresholds.feedMillCrossThreshold, false);
        if (!closeRes.found) closeRes = FindBestCrossAny(closeScreen);
        if (closeRes.found) {
            AddLog(instanceId, std::string("Dairy: closing panel after ") + reason + ".", ImVec4(0.8f, 0.8f, 0.4f, 1.0f));
            AdbTap(instanceId, closeRes.x, closeRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    };

    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    MatchResult dairy = fullFind(screen, dairy_templatePath, g_Thresholds.dairyThreshold, false);
    if (!dairy.found) {
        AddLog(instanceId, "Dairy: dairy template not found. Attempting zoom-out recovery...", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        if (ZoomOutFarmView(instanceId, "dairy recovery", 1, isTestRun)) {
            if (!WaitForBotActionWindow(instanceId, 700, isTestRun)) return false;
            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            dairy = fullFind(screen, dairy_templatePath, g_Thresholds.dairyThreshold, false);
        }
    }
    if (!dairy.found) {
        AddLog(instanceId, "Dairy: dairy template not found.", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        return false;
    }

    AddLog(instanceId, std::string("Dairy: opening dairy for ") + GetDairyProductName(productMode) + "...", ImVec4(0.2f, 0.9f, 1.0f, 1.0f));
    AdbTap(instanceId, dairy.x, dairy.y);
    std::this_thread::sleep_for(std::chrono::milliseconds(900));

    for (int queueIndex = 1; queueIndex <= queueTarget; ++queueIndex) {
        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult product = fullFind(screen, *productTemplates[productMode], g_Thresholds.creamThreshold, false);
        bool usedDirectProductTemplate = product.found;
        POINT offset = productOffsetsFromCream[productMode];
        int sourceX = product.x;
        int sourceY = product.y;
        if (!usedDirectProductTemplate) {
            MatchResult cream = fullFind(screen, cream_templatePath, g_Thresholds.creamThreshold, false);
            if (!cream.found) {
                AddLog(instanceId, std::string("Dairy: ") + GetDairyProductName(productMode) + " template not found, and cream anchor fallback also failed.", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
                closeVisiblePanel("missing dairy product template");
                return false;
            }
            sourceX = cream.x + offset.x;
            sourceY = cream.y + offset.y;
            if (productMode != 0) {
                AddLog(instanceId, std::string("Dairy: using cream-anchor offset fallback for ") + GetDairyProductName(productMode) + ".", ImVec4(0.8f, 0.85f, 0.3f, 1.0f));
            }
        }
        if (!screen.empty()) {
            sourceX = (std::max)(0, (std::min)(screen.cols - 1, sourceX));
            sourceY = (std::max)(0, (std::min)(screen.rows - 1, sourceY));
        }

        std::vector<POINT> dairyDrag = {
            POINT{ sourceX, sourceY },
            POINT{ (sourceX + dairy.x) / 2, (sourceY + dairy.y) / 2 },
            POINT{ dairy.x, dairy.y }
        };
        AddLog(instanceId,
            std::string("Dairy: queueing ") + GetDairyProductName(productMode) + " " +
            std::to_string(queueIndex) + "/" + std::to_string(queueTarget) +
            (usedDirectProductTemplate
                ? " from detected product template..."
                : " using offset (" + std::to_string(offset.x) + "," + std::to_string(offset.y) + ")..."),
            ImVec4(0.2f, 0.9f, 0.6f, 1.0f));

        if (!Minitouch::DragPath(instanceId, dairyDrag)) {
            AddLog(instanceId, "Dairy: minitouch drag failed while queueing product.", ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            closeVisiblePanel("failed dairy drag");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult notEnough = findNotEnough(screen);
        if (notEnough.found) {
            AddLog(instanceId, "Dairy: not enough resources/milk detected. Closing the dairy and skipping.", ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
            closeVisiblePanel("not enough resources");
            return false;
        }
    }

    closeVisiblePanel("queueing dairy products");
    AddLog(instanceId, isTestRun ? "TEST DAIRY: routine completed." : "Dairy: routine completed.", ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
    return true;
}

