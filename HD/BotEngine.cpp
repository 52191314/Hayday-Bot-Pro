#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "BotEngine.h"
#include "Language.h"
#include "Discord.h"
#include "tesseract_ocr.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include "imgui.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <windows.h>
#include <stdio.h>
#include <sstream>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <climits>


// GLOBAL VARIABLES
namespace fs = std::filesystem;
std::string g_StorageTag = ""; 
std::string kLDPlayerAdbPath = "C:\\LDPlayer\\LDPlayer9\\adb.exe";
std::string kLDConsolePath = "C:\\LDPlayer\\LDPlayer9\\ldconsole.exe";

extern TemplateThresholds g_Thresholds;
extern IntervalSettings g_Intervals;
extern BotInstance g_Bots[6];
extern CoordinateProfile g_CoordinateProfile;

extern std::string kAdbPath;
extern std::string kMEmuConsolePath;
extern std::string GetAppDataPath();
extern bool g_EnableBarnWebhook;
extern bool g_OnlySellIfSiloFull;

extern bool g_EnableWebhookImage;

// --- TEMPLATE PATHS ---
extern std::string f_templatePath; extern std::string w_templatePath; extern std::string s_templatePath;
extern std::string g_templatePath; extern std::string shop_templatePath; extern std::string roadside_shop_templatePath; extern std::string wheatshop_templatePath;
extern std::string soldcrate_templatePath; extern std::string crate_templatePath; extern std::string arrows_templatePath;
extern std::string plus_templatePath; extern std::string cross_templatePath; extern std::string advertise_templatePath; extern std::string occupied_crate_advertise_templatePath;
extern std::string create_sale_templatePath; extern std::string c_templatePath; extern std::string gc_templatePath;
extern std::string create_ad_templatePath; extern std::string occupied_crate_create_ad_templatePath;
extern std::string feed_mill_templatePath; extern std::string cow_feed_templatePath; extern std::string not_enough_templatePath;
extern std::string dairy_templatePath; extern std::string cream_templatePath; extern std::string butter_templatePath; extern std::string cheese_templatePath;
extern std::string feed_mill_cross_templatePath; extern std::string cow_pasture_templatePath;
extern std::string cornshop_templatePath; extern std::string barn_market_templatePath; extern std::string silo_market_templatePath;
extern std::string mailbox_templatePath; extern std::string loading_templatePath; extern std::string crate_wheat_templatePath; extern std::string crate_corn_templatePath;
extern std::string levelup_templatePath; extern std::string levelup_continue_templatePath; extern std::string carrot_templatePath;
extern std::string grown_carrot_templatePath; extern std::string carrot_shop_templatePath; extern std::string soybean_templatePath;
extern std::string grown_soybean_templatePath; extern std::string soybean_shop_templatePath; extern std::string sugarcane_templatePath;
extern std::string grown_sugarcane_templatePath; extern std::string sugarcane_shop_templatePath; extern std::string silo_full_templatePath;
extern std::string crate_carrot_templatePath; extern std::string crate_soybean_templatePath; extern std::string crate_sugarcane_templatePath;
extern std::string silo_full_cross_templatePath; extern std::string market_close_crosstemplatePath; extern std::string market_close_crosstemplatePath;
extern std::string bolt_material_templatePath; extern std::string tape_material_templatePath; extern std::string plank_material_templatePath;
extern std::string nail_material_templatePath; extern std::string screw_material_templatePath; extern std::string panel_material_templatePath;
extern std::string deed_material_templatePath; extern std::string mallet_material_templatePath; extern std::string marker_material_templatePath; extern std::string map_material_templatePath;
extern std::string dynamite_templatePath; extern std::string tnt_templatePath; extern std::string axe_templatePath; extern std::string saw_templatePath; extern std::string shovel_templatePath;


// --- MUTUAL FUNCTIONS ---
extern void AddLog(int instanceId, std::string message, ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
extern void SaveConfig();
extern void ApplyFarmConfigForSlot(int instanceId, int accountIndex);
extern void SaveInventoryData();
extern std::string GetClipboardText();

std::atomic<unsigned long long> g_TestGeneration[6]{};
static bool IsTestStillCurrent(int instanceId, unsigned long long token) { return g_TestGeneration[instanceId].load() == token; }

static int ReadAnchoredCountSafe(const cv::Mat& screen, const std::string& templatePath) {
    if (screen.empty()) return 0;
    try { return ReadItemCountByAnchor(screen, templatePath); } catch (...) { return 0; }
}

struct CropRuntimeInfo {
    int mode = 0;
    const char* name = "Wheat";
    int growSeconds = 120;
    std::string seedTemplate;
    float seedThreshold = 0.0f;
    std::string productTemplate;
    float productThreshold = 0.0f;
    std::string grownTemplate;
    float grownThreshold = 0.0f;
};

static CropRuntimeInfo GetCropRuntimeInfo(int cropMode) {
    CropRuntimeInfo crop;
    crop.mode = cropMode;
    crop.seedTemplate = w_templatePath;
    crop.seedThreshold = g_Thresholds.wheatThreshold;
    crop.productTemplate = wheatshop_templatePath;
    crop.productThreshold = g_Thresholds.wheatShopThreshold;
    crop.grownTemplate = g_templatePath;
    crop.grownThreshold = g_Thresholds.grownThreshold;

    if (cropMode == 1) {
        crop.name = "Corn";
        crop.growSeconds = 300;
        crop.seedTemplate = c_templatePath;
        crop.seedThreshold = g_Thresholds.cornThreshold;
        crop.productTemplate = cornshop_templatePath;
        crop.productThreshold = g_Thresholds.cornShopThreshold;
        crop.grownTemplate = gc_templatePath;
        crop.grownThreshold = g_Thresholds.grownCornThreshold;
    }
    else if (cropMode == 2) {
        crop.name = "Carrot";
        crop.growSeconds = 600;
        crop.seedTemplate = carrot_templatePath;
        crop.seedThreshold = g_Thresholds.carrotThreshold;
        crop.productTemplate = carrot_shop_templatePath;
        crop.productThreshold = g_Thresholds.carrotShopThreshold;
        crop.grownTemplate = grown_carrot_templatePath;
        crop.grownThreshold = g_Thresholds.grownCarrotThreshold;
    }
    else if (cropMode == 3) {
        crop.name = "Soybean";
        crop.growSeconds = 1200;
        crop.seedTemplate = soybean_templatePath;
        crop.seedThreshold = g_Thresholds.soybeanThreshold;
        crop.productTemplate = soybean_shop_templatePath;
        crop.productThreshold = g_Thresholds.soybeanShopThreshold;
        crop.grownTemplate = grown_soybean_templatePath;
        crop.grownThreshold = g_Thresholds.grownSoybeanThreshold;
    }
    else if (cropMode == 4) {
        crop.name = "Sugarcane";
        crop.growSeconds = 1800;
        crop.seedTemplate = sugarcane_templatePath;
        crop.seedThreshold = g_Thresholds.sugarcaneThreshold;
        crop.productTemplate = sugarcane_shop_templatePath;
        crop.productThreshold = g_Thresholds.sugarcaneShopThreshold;
        crop.grownTemplate = grown_sugarcane_templatePath;
        crop.grownThreshold = g_Thresholds.grownSugarcaneThreshold;
    }

    return crop;
}

static std::vector<MatchResult> FindGrownCandidatesForCrop(const cv::Mat& screen, int cropMode) {
    if (screen.empty()) return {};
    if (cropMode == 3) {
        std::vector<MatchResult> grown;
        MatchResult soybean = FindImage(screen, grown_soybean_templatePath, g_Thresholds.grownSoybeanThreshold, false);
        if (soybean.found) grown.push_back(soybean);
        return grown;
    }
    return FindGrownCrops(screen, cropMode);
}

static std::string FormatGameNumberDebugLabel(const char* label, const GameNumberDebugInfo& info) {
    std::string method = info.readMethod.empty() ? "unreadable" : info.readMethod;
    std::string digits = info.digits.empty() ? "-" : info.digits;
    return std::string(label) + "=" + std::to_string(info.count) + " [" + method + " " + std::to_string(info.score) + " | digits=" + digits + "]";
}

static std::string BuildHudReadDetail(const GameNumberDebugInfo& coinInfo, const GameNumberDebugInfo& diaInfo, const GameNumberDebugInfo& lvlInfo) {
    return FormatGameNumberDebugLabel("Coin", coinInfo) + " | " +
        FormatGameNumberDebugLabel("Dia", diaInfo) + " | " +
        FormatGameNumberDebugLabel("Lvl", lvlInfo);
}

static std::string BuildPreserveReadDetail(const ItemCountDebugInfo& info) {
    std::string method = info.readMethod.empty() ? "unreadable" : info.readMethod;
    std::string digits = info.digits.empty() ? "-" : info.digits;
    std::string roiText = info.roiValid
        ? ("roi=" + std::to_string(info.roi.x) + "," + std::to_string(info.roi.y) + " " + std::to_string(info.roi.width) + "x" + std::to_string(info.roi.height))
        : "roi=invalid";
    return "count=" + std::to_string(info.count) + " [" + method + " " + std::to_string(info.ocrScore) + " | digits=" + digits + " | " + roiText + "]";
}

static std::mutex g_ActionTimingLogMutex;

static std::string EscapeCsvField(const std::string& value) {
    bool needsQuotes = false;
    std::string escaped;
    escaped.reserve(value.size() + 2);
    for (char ch : value) {
        if (ch == '"' || ch == ',' || ch == '\n' || ch == '\r') {
            needsQuotes = true;
        }
        if (ch == '"') escaped += "\"\"";
        else escaped += ch;
    }
    return needsQuotes ? ("\"" + escaped + "\"") : escaped;
}

static std::string GetActionTimingTimestamp() {
    std::time_t rawTime = std::time(nullptr);
    std::tm timeInfo{};
    localtime_s(&timeInfo, &rawTime);

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
    return std::string(buffer);
}

static std::string FormatDurationSeconds(long long durationMs) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << (static_cast<double>(durationMs) / 1000.0);
    return out.str();
}

static int FindAccountSlotIndex(int instanceId, const AccountSlot& account) {
    if (instanceId < 0 || instanceId >= 6) return -1;

    const BotInstance& bot = g_Bots[instanceId];
    for (int slotIndex = 0; slotIndex < MAX_ACCOUNT_SLOTS; ++slotIndex) {
        if (&bot.accounts[slotIndex] == &account) return slotIndex;
    }
    return -1;
}

static void AppendActionTimingCsv(int instanceId, int slotIndex, const std::string& category, const std::string& action, long long durationMs, const std::string& detail) {
    std::lock_guard<std::mutex> lock(g_ActionTimingLogMutex);

    fs::path timingPath = fs::path(GetAppDataPath()) / "action_timing_log.csv";
    std::error_code ec;
    bool needsHeader = !fs::exists(timingPath, ec) || fs::file_size(timingPath, ec) == 0;

    std::ofstream out(timingPath, std::ios::app);
    if (!out.is_open()) return;

    if (needsHeader) {
        out << "timestamp,instance,slot,category,action,duration_ms,duration_s,detail\n";
    }

    out << EscapeCsvField(GetActionTimingTimestamp()) << ","
        << (instanceId + 1) << ","
        << (slotIndex >= 0 ? slotIndex + 1 : 0) << ","
        << EscapeCsvField(category) << ","
        << EscapeCsvField(action) << ","
        << durationMs << ","
        << EscapeCsvField(FormatDurationSeconds(durationMs)) << ","
        << EscapeCsvField(detail) << "\n";
}

static void RecordCompletedFlowAction(int instanceId, AccountSlot& account, const char* category, const char* action, const std::string& detail, std::chrono::steady_clock::time_point startedAt) {
    if (!startedAt.time_since_epoch().count() || std::string(action) == "Idle") return;

    auto now = std::chrono::steady_clock::now();
    long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - startedAt).count();
    if (durationMs < 0) return;

    int slotIndex = FindAccountSlotIndex(instanceId, account);
    AppendActionTimingCsv(instanceId, slotIndex, category, action, durationMs, detail);

    AddLog(instanceId,
        std::string("TIMING [") + category + "] " + action + " finished in " + FormatDurationSeconds(durationMs) + "s | " + detail,
        ImVec4(0.95f, 0.78f, 0.35f, 1.0f));
}

static void SetFarmFlowState(int instanceId, AccountSlot& account, FarmFlowState state, const std::string& detail = "", bool forceLog = false) {
    std::string nextDetail = detail.empty() ? ToString(state) : detail;
    bool changed = forceLog || account.farmFlowState != state || account.farmFlowDetail != nextDetail;
    if (changed) {
        FarmFlowState previousState = account.farmFlowState;
        std::string previousDetail = account.farmFlowDetail;
        std::chrono::steady_clock::time_point previousStartedAt = account.farmFlowStateStartedAt;

        account.farmFlowState = state;
        account.farmFlowDetail = nextDetail;
        account.farmFlowStateStartedAt = std::chrono::steady_clock::now();

        RecordCompletedFlowAction(instanceId, account, "FARM", ToString(previousState), previousDetail, previousStartedAt);
        AddLog(instanceId, std::string("FLOW [FARM] -> ") + ToString(state) + " | " + nextDetail, ImVec4(0.55f, 0.9f, 1.0f, 1.0f));
    }
}

static void SetSalesFlowState(int instanceId, AccountSlot& account, SalesFlowState state, const std::string& detail = "", bool forceLog = false) {
    std::string nextDetail = detail.empty() ? ToString(state) : detail;
    bool changed = forceLog || account.salesFlowState != state || account.salesFlowDetail != nextDetail;
    if (changed) {
        SalesFlowState previousState = account.salesFlowState;
        std::string previousDetail = account.salesFlowDetail;
        std::chrono::steady_clock::time_point previousStartedAt = account.salesFlowStateStartedAt;

        account.salesFlowState = state;
        account.salesFlowDetail = nextDetail;
        account.salesFlowStateStartedAt = std::chrono::steady_clock::now();

        RecordCompletedFlowAction(instanceId, account, "SALE", ToString(previousState), previousDetail, previousStartedAt);
        AddLog(instanceId, std::string("FLOW [SALE] -> ") + ToString(state) + " | " + nextDetail, ImVec4(0.75f, 0.95f, 0.55f, 1.0f));
    }
}

static MatchResult FindBestCrossAny(const cv::Mat& screen) {
    MatchResult best{false,0,0,0.0};
    auto consider = [&](const std::string& path, float thr) {
        MatchResult r = FindImage(screen, path, thr, false, 1.0f, false);
        if (r.found && (!best.found || r.score > best.score)) best = r;
    };
    consider(cross_templatePath, std::max(0.65f, g_Thresholds.crossThreshold));
    consider(market_close_crosstemplatePath, std::max(0.65f, g_Thresholds.marketCloseCrossThreshold));
    consider(silo_full_cross_templatePath, std::max(0.65f, g_Thresholds.siloFullCrossThreshold));
    return best;
}

// LAST-RESORT RECOVERY: Dismiss any unknown overlay/popup that the bot can't identify.
// Strategy: (1) Try to find any cross template and click it.
//           (2) If no cross found, tap the right-center of the screen (590,240 on 640x480)
//               which is nearly always grass in the zoomed-out Hay Day farm view.
// Returns true if any action was taken.
static bool DismissUnknownOverlay(int instanceId, const char* reason) {
    BotInstance& bot = g_Bots[instanceId];
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    if (screen.empty()) return false;

    // 1st resort: find any cross template
    MatchResult crossRes = FindBestCrossAny(screen);
    if (crossRes.found) {
        AddLog(instanceId, std::string("RECOVERY: cross found during ") + reason + ". Clicking it to dismiss.", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
        AdbTap(instanceId, crossRes.x, crossRes.y);
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        return true;
    }

    AddLog(instanceId, std::string("RECOVERY: no cross found during ") + reason + ". Not touching grass per user request. Overlay dismissal failed.", ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    return false;
}

static bool IsSiloFullPopupVisible(const cv::Mat& screen, MatchResult* outPopup = nullptr) {
    MatchResult popup = FindImage(screen, silo_full_templatePath, 0.75f, false, 1.0f, false);
    if (outPopup) *outPopup = popup;
    return popup.found;
}

static bool EnsureSiloFullPopupClosed(int instanceId, const char* contextLabel, int maxAttempts = 2) {
    BotInstance& bot = g_Bots[instanceId];
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (!bot.isRunning) return false;

        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult siloFullPopup;
        if (!IsSiloFullPopupVisible(screen, &siloFullPopup)) return true;

        if (attempt > 1) {
            AddLog(instanceId, std::string("SILO FULL popup still visible during ") + contextLabel + ". Clicking cross again...", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
        }

        MatchResult closeRes = FindImage(screen, silo_full_cross_templatePath, std::max(0.65f, g_Thresholds.siloFullCrossThreshold), false, 1.0f, false);
        if (!closeRes.found) closeRes = FindBestCrossAny(screen);
        if (closeRes.found) AdbTap(instanceId, closeRes.x, closeRes.y);
        else AdbTap(instanceId, siloFullPopup.x, siloFullPopup.y);

        std::this_thread::sleep_for(std::chrono::milliseconds(700));
    }

    cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    return !IsSiloFullPopupVisible(verifyScreen, nullptr);
}

static std::string GetFilledCrateTemplateForCrop(int cropMode) {
    if (cropMode == 1) return crate_corn_templatePath;
    if (cropMode == 2) return crate_carrot_templatePath;
    if (cropMode == 3) return crate_soybean_templatePath;
    if (cropMode == 4) return crate_sugarcane_templatePath;
    return crate_wheat_templatePath;
}

static MatchResult FindAnyFilledCrate(const cv::Mat& screen, int cropMode, float threshold = 0.75f) {
    MatchResult best{false, 0, 0, 0.0};
    std::vector<std::string> candidates;
    auto pushUnique = [&](const std::string& path) {
        if (std::find(candidates.begin(), candidates.end(), path) == candidates.end()) candidates.push_back(path);
    };

    pushUnique(GetFilledCrateTemplateForCrop(cropMode));
    pushUnique(crate_wheat_templatePath);
    pushUnique(crate_corn_templatePath);
    pushUnique(crate_carrot_templatePath);
    pushUnique(crate_soybean_templatePath);
    pushUnique(crate_sugarcane_templatePath);

    for (const auto& path : candidates) {
        MatchResult res = FindImage(screen, path, threshold, false, 1.0f, false);
        if (res.found && (!best.found || res.score > best.score)) best = res;
    }
    return best;
}

static bool HasAdCooldownElapsed(const AccountSlot& account, int cooldownMinutes = 5) {
    if (account.lastAdTime.time_since_epoch().count() == 0) return true;
    auto elapsedMinutes = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - account.lastAdTime).count();
    return elapsedMinutes >= cooldownMinutes;
}

static bool IsRoadsideShopBannerVisible(const cv::Mat& screen) {
    return FindImage(screen, roadside_shop_templatePath, std::max(0.65f, g_Thresholds.shopThreshold - 0.08f), false, 1.0f, false).found;
}

static cv::Rect BuildSaleDialogSearchRect(const cv::Mat& screen) {
    if (screen.empty()) return cv::Rect();
    int left = static_cast<int>(screen.cols * 0.28f);
    int top = static_cast<int>(screen.rows * 0.14f);
    int right = static_cast<int>(screen.cols * 0.77f);
    int bottom = static_cast<int>(screen.rows * 0.90f);
    left = (std::max)(0, left);
    top = (std::max)(0, top);
    right = (std::min)(screen.cols, right);
    bottom = (std::min)(screen.rows, bottom);
    int width = right - left;
    int height = bottom - top;
    if (width <= 0 || height <= 0) return cv::Rect();
    return cv::Rect(left, top, width, height);
}

static MatchResult FindTemplateInRect(const cv::Mat& screen, const cv::Rect& rect, const std::string& templatePath, float threshold, bool grayscale) {
    MatchResult res{ false, 0, 0, 0.0 };
    if (screen.empty() || rect.width <= 0 || rect.height <= 0) return res;
    cv::Rect bounded = rect & cv::Rect(0, 0, screen.cols, screen.rows);
    if (bounded.width <= 0 || bounded.height <= 0) return res;
    cv::Mat roi = screen(bounded);
    res = FindImage(roi, templatePath, threshold, grayscale, 1.0f, false);
    if (res.found) {
        res.x += bounded.x;
        res.y += bounded.y;
    }
    return res;
}

static cv::Rect BuildProbeRectAroundPoint(const cv::Mat& screen, int centerX, int centerY, int radiusX, int radiusY) {
    if (screen.empty() || centerX < 0 || centerY < 0) return cv::Rect();
    int left = (std::max)(0, centerX - radiusX);
    int top = (std::max)(0, centerY - radiusY);
    int right = (std::min)(screen.cols, centerX + radiusX);
    int bottom = (std::min)(screen.rows, centerY + radiusY);
    if (right <= left || bottom <= top) return cv::Rect();
    return cv::Rect(left, top, right - left, bottom - top);
}

static MatchResult FindTemplateNearCachedPoint(const cv::Mat& screen,
    int cachedX,
    int cachedY,
    int radiusX,
    int radiusY,
    const std::string& templatePath,
    float threshold,
    bool grayscale = false) {
    cv::Rect probeRect = BuildProbeRectAroundPoint(screen, cachedX, cachedY, radiusX, radiusY);
    if (probeRect.width <= 0 || probeRect.height <= 0) return MatchResult{ false, 0, 0, 0.0 };
    MatchResult cachedRes = FindTemplateInRect(screen, probeRect, templatePath, threshold, grayscale);
    if (!cachedRes.found && !grayscale) {
        cachedRes = FindTemplateInRect(screen, probeRect, templatePath, (std::max)(0.60f, threshold - 0.08f), false);
    }
    return cachedRes;
}

static MatchResult FindDialogButtonTemplate(const cv::Mat& screen, const std::string& templatePath, float threshold) {
    MatchResult best{ false, 0, 0, 0.0 };
    auto consider = [&](const MatchResult& candidate) {
        if (candidate.found && (!best.found || candidate.score > best.score)) best = candidate;
    };

    cv::Rect dialogRect = BuildSaleDialogSearchRect(screen);
    float relaxedThreshold = (std::max)(0.58f, threshold - 0.08f);

    consider(FindTemplateInRect(screen, dialogRect, templatePath, threshold, true));
    consider(FindTemplateInRect(screen, dialogRect, templatePath, relaxedThreshold, false));
    consider(FindImage(screen, templatePath, relaxedThreshold, true, 1.0f, false));
    consider(FindImage(screen, templatePath, relaxedThreshold, false, 1.0f, false));
    return best;
}

static MatchResult FindBestDialogTemplate(const cv::Mat& screen, const std::initializer_list<std::string>& templatePaths, float threshold) {
    MatchResult best{ false, 0, 0, 0.0 };
    for (const auto& path : templatePaths) {
        if (path.empty()) continue;
        MatchResult candidate = FindDialogButtonTemplate(screen, path, threshold);
        if (candidate.found && (!best.found || candidate.score > best.score)) best = candidate;
    }
    return best;
}

static std::string GetSaleProductTemplateForCrop(int cropMode) {
    if (cropMode == 1) return cornshop_templatePath;
    if (cropMode == 2) return carrot_shop_templatePath;
    if (cropMode == 3) return soybean_shop_templatePath;
    if (cropMode == 4) return sugarcane_shop_templatePath;
    return wheatshop_templatePath;
}

static float GetSaleProductThresholdForCrop(int cropMode) {
    if (cropMode == 1) return g_Thresholds.cornShopThreshold;
    if (cropMode == 2) return g_Thresholds.carrotShopThreshold;
    if (cropMode == 3) return g_Thresholds.soybeanShopThreshold;
    if (cropMode == 4) return g_Thresholds.sugarcaneShopThreshold;
    return g_Thresholds.wheatShopThreshold;
}

static MatchResult FindShopIconPreferCache(const cv::Mat& screen, AccountSlot& account) {
    MatchResult shopRes{ false, 0, 0, 0.0 };
    if (account.cachedShopX >= 0 && account.cachedShopY >= 0) {
        shopRes = FindTemplateNearCachedPoint(screen, account.cachedShopX, account.cachedShopY, 90, 90, shop_templatePath, g_Thresholds.shopThreshold);
    }
    if (!shopRes.found) {
        shopRes = FindImage(screen, shop_templatePath, g_Thresholds.shopThreshold);
    }
    if (shopRes.found) {
        account.cachedShopX = shopRes.x;
        account.cachedShopY = shopRes.y;
    }
    return shopRes;
}

static MatchResult FindEmptyCratePreferCache(const cv::Mat& screen, AccountSlot& account) {
    MatchResult emptyCrate{ false, 0, 0, 0.0 };
    if (account.cachedEmptyCrateX >= 0 && account.cachedEmptyCrateY >= 0) {
        emptyCrate = FindTemplateNearCachedPoint(screen, account.cachedEmptyCrateX, account.cachedEmptyCrateY, 150, 110, crate_templatePath, g_Thresholds.crateThreshold);
    }
    if (!emptyCrate.found) {
        emptyCrate = FindImage(screen, crate_templatePath, g_Thresholds.crateThreshold);
    }
    if (emptyCrate.found) {
        account.cachedEmptyCrateX = emptyCrate.x;
        account.cachedEmptyCrateY = emptyCrate.y;
    }
    return emptyCrate;
}

static MatchResult FindSaleProductPreferCache(const cv::Mat& screen, int cropMode, AccountSlot& account) {
    const std::string productTemplate = GetSaleProductTemplateForCrop(cropMode);
    const float productThreshold = GetSaleProductThresholdForCrop(cropMode);
    MatchResult productRes{ false, 0, 0, 0.0 };
    if (account.cachedSaleProductX >= 0 && account.cachedSaleProductY >= 0) {
        productRes = FindTemplateNearCachedPoint(screen, account.cachedSaleProductX, account.cachedSaleProductY, 190, 130, productTemplate, productThreshold);
    }
    if (!productRes.found) {
        productRes = FindImage(screen, productTemplate, productThreshold);
    }
    if (productRes.found) {
        account.cachedSaleProductX = productRes.x;
        account.cachedSaleProductY = productRes.y;
    }
    return productRes;
}

static bool IsSalePanelVisible(const cv::Mat& screen) {
    return FindImage(screen, create_sale_templatePath, g_Thresholds.createSaleThreshold, false, 1.0f, false).found ||
        FindBestDialogTemplate(screen, { advertise_templatePath, occupied_crate_advertise_templatePath }, g_Thresholds.advertiseThreshold).found ||
        FindBestDialogTemplate(screen, { create_ad_templatePath, occupied_crate_create_ad_templatePath }, g_Thresholds.createAdThreshold).found ||
        FindImage(screen, plus_templatePath, g_Thresholds.plusThreshold, false, 1.0f, false).found;
}

static bool IsOccupiedCrateEditPanelVisible(const cv::Mat& screen) {
    if (screen.empty()) return false;
    if (FindBestDialogTemplate(screen, { occupied_crate_advertise_templatePath, advertise_templatePath }, g_Thresholds.advertiseThreshold).found) return true;
    if (FindBestDialogTemplate(screen, { occupied_crate_create_ad_templatePath, create_ad_templatePath }, g_Thresholds.createAdThreshold).found) return true;
    cv::Rect dialogRect = BuildSaleDialogSearchRect(screen);
    MatchResult dialogCross = FindTemplateInRect(screen, dialogRect, cross_templatePath, std::max(0.65f, g_Thresholds.crossThreshold - 0.08f), false);
    return dialogCross.found;
}

static bool IsMarketViewVisible(const cv::Mat& screen, int cropMode) {
    return FindImage(screen, crate_templatePath, g_Thresholds.crateThreshold, false, 1.0f, false).found ||
        !FindAllImages(screen, soldcrate_templatePath, 0.80f, 10, false).empty() ||
        FindAnyFilledCrate(screen, cropMode).found ||
        IsRoadsideShopBannerVisible(screen) ||
        IsSalePanelVisible(screen);
}

static bool IsMarketViewLikelyVisible(const cv::Mat& screen, int cropMode) {
    if (IsMarketViewVisible(screen, cropMode)) return true;

    MatchResult anyCross = FindBestCrossAny(screen);
    if (anyCross.found) return true;

    MatchResult relaxedFilledCrate = FindAnyFilledCrate(screen, cropMode, 0.65f);
    if (relaxedFilledCrate.found) return true;

    MatchResult relaxedCrate = FindImage(screen, crate_templatePath, std::max(0.70f, g_Thresholds.crateThreshold - 0.10f), false, 1.0f, false);
    return relaxedCrate.found;
}

enum class CowFeedRoutineResult {
    Completed,
    NotEnoughResources,
    SkippedMissingTemplate,
    Failed
};

static const char* ToString(CowFeedRoutineResult result) {
    switch (result) {
    case CowFeedRoutineResult::Completed: return "Completed";
    case CowFeedRoutineResult::NotEnoughResources: return "Not enough resources";
    case CowFeedRoutineResult::SkippedMissingTemplate: return "Skipped (template missing)";
    case CowFeedRoutineResult::Failed: return "Failed";
    default: return "Unknown";
    }
}

static bool ShouldRunCowFeed(const AccountSlot& account) {
    if (!account.autoCowFeedEnabled) return false;
    if (account.lastCowFeedRun.time_since_epoch().count() == 0) return true;
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - account.lastCowFeedRun).count();
    return elapsed >= 60;
}

static constexpr int kCowFeedCornCropMode = 1;
static constexpr int kCowFeedSoybeanCropMode = 3;

static int GetAccountCropWaitSeconds(const AccountSlot& account, int configuredCropMode) {
    if (account.cowFeedCropRecoveryPlanted) {
        return (std::max)(
            GetCropRuntimeInfo(kCowFeedCornCropMode).growSeconds,
            GetCropRuntimeInfo(kCowFeedSoybeanCropMode).growSeconds);
    }
    return GetCropRuntimeInfo(configuredCropMode).growSeconds;
}

static int GetDairyCooldownMinutes(int dairyProductMode) {
    switch ((std::max)(0, (std::min)(2, dairyProductMode))) {
    case 1: return 30; // Butter
    case 2: return 60; // Cheese
    default: return 20; // Cream
    }
}

static const char* GetDairyProductName(int dairyProductMode) {
    switch ((std::max)(0, (std::min)(2, dairyProductMode))) {
    case 1: return "Butter";
    case 2: return "Cheese";
    default: return "Cream";
    }
}

static bool ShouldRunDairy(const AccountSlot& account) {
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

static bool ExecuteSingleFingerDragPath(int instanceId, const std::vector<POINT>& path, int holdMs = 260, int stepDelayMs = 18, int settleMs = 220) {
    if (path.size() < 2) return false;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    bool success = false;
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != INVALID_SOCKET) {
        int port = 1111 + instanceId;
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        server.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sock, reinterpret_cast<sockaddr*>(&server), sizeof(server)) == 0) {
            std::string pressCmd = "d 0 " + std::to_string(path.front().x) + " " + std::to_string(path.front().y) + " 50\nc\n";
            send(sock, pressCmd.c_str(), static_cast<int>(pressCmd.length()), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));

            for (size_t segment = 1; segment < path.size(); ++segment) {
                POINT from = path[segment - 1];
                POINT to = path[segment];
                float dist = std::hypot(static_cast<float>(to.x - from.x), static_cast<float>(to.y - from.y));
                int steps = (std::max)(12, static_cast<int>(dist / 6.0f));

                for (int i = 1; i <= steps; ++i) {
                    float t = static_cast<float>(i) / static_cast<float>(steps);
                    int cx = from.x + static_cast<int>((to.x - from.x) * t);
                    int cy = from.y + static_cast<int>((to.y - from.y) * t);
                    std::string moveCmd = "m 0 " + std::to_string(cx) + " " + std::to_string(cy) + " 50\nc\n";
                    send(sock, moveCmd.c_str(), static_cast<int>(moveCmd.length()), 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(stepDelayMs));
                }
            }

            std::string releaseCmd = "u 0\nc\n";
            send(sock, releaseCmd.c_str(), static_cast<int>(releaseCmd.length()), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(settleMs));
            success = true;
        }
    }

    if (sock != INVALID_SOCKET) closesocket(sock);
    WSACleanup();
    return success;
}

static bool WaitForBotActionWindow(int instanceId, int totalMs, bool ignoreBotState = false) {
    BotInstance& bot = g_Bots[instanceId];
    int elapsed = 0;
    while (elapsed < totalMs) {
        if (!ignoreBotState && !bot.isRunning) return false;
        int stepMs = (std::min)(100, totalMs - elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
        elapsed += stepMs;
    }
    return true;
}

static bool ExecuteTwoFingerPinchGesture(int instanceId, POINT fingerAStart, POINT fingerAEnd, POINT fingerBStart, POINT fingerBEnd, int holdMs = 220, int stepDelayMs = 18, int settleMs = 260) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    bool success = false;
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != INVALID_SOCKET) {
        int port = 1111 + instanceId;
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        server.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sock, reinterpret_cast<sockaddr*>(&server), sizeof(server)) == 0) {
            std::string pressCmd =
                "d 0 " + std::to_string(fingerAStart.x) + " " + std::to_string(fingerAStart.y) + " 50\n" +
                "d 1 " + std::to_string(fingerBStart.x) + " " + std::to_string(fingerBStart.y) + " 50\nc\n";
            send(sock, pressCmd.c_str(), static_cast<int>(pressCmd.length()), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));

            float distA = std::hypot(static_cast<float>(fingerAEnd.x - fingerAStart.x), static_cast<float>(fingerAEnd.y - fingerAStart.y));
            float distB = std::hypot(static_cast<float>(fingerBEnd.x - fingerBStart.x), static_cast<float>(fingerBEnd.y - fingerBStart.y));
            int steps = (std::max)(14, static_cast<int>((std::max)(distA, distB) / 6.0f));

            for (int i = 1; i <= steps; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(steps);
                int ax = fingerAStart.x + static_cast<int>((fingerAEnd.x - fingerAStart.x) * t);
                int ay = fingerAStart.y + static_cast<int>((fingerAEnd.y - fingerAStart.y) * t);
                int bx = fingerBStart.x + static_cast<int>((fingerBEnd.x - fingerBStart.x) * t);
                int by = fingerBStart.y + static_cast<int>((fingerBEnd.y - fingerBStart.y) * t);
                std::string moveCmd =
                    "m 0 " + std::to_string(ax) + " " + std::to_string(ay) + " 50\n" +
                    "m 1 " + std::to_string(bx) + " " + std::to_string(by) + " 50\nc\n";
                send(sock, moveCmd.c_str(), static_cast<int>(moveCmd.length()), 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(stepDelayMs));
            }

            std::string releaseCmd = "u 0\nu 1\nc\n";
            send(sock, releaseCmd.c_str(), static_cast<int>(releaseCmd.length()), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(settleMs));
            success = true;
        }
    }

    if (sock != INVALID_SOCKET) closesocket(sock);
    WSACleanup();
    return success;
}

static bool ZoomOutFarmView(int instanceId, const char* reason, int repeatCount = 2, bool ignoreBotState = false) {
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
    int width = screen.empty() ? 640 : screen.cols;
    int height = screen.empty() ? 480 : screen.rows;

    auto clampCoord = [](int value, int low, int high) {
        return (std::max)(low, (std::min)(high, value));
    };

    int centerX = width / 2;
    int centerY = height / 2;
    int startOffsetX = (std::max)(90, width / 3);
    int endOffsetX = (std::max)(45, width / 8);
    int startOffsetY = (std::max)(30, height / 10);
    int endOffsetY = (std::max)(12, height / 24);

    POINT fingerAStart{
        clampCoord(centerX - startOffsetX, 30, width - 30),
        clampCoord(centerY - startOffsetY, 30, height - 30)
    };
    POINT fingerAEnd{
        clampCoord(centerX - endOffsetX, 30, width - 30),
        clampCoord(centerY - endOffsetY, 30, height - 30)
    };
    POINT fingerBStart{
        clampCoord(centerX + startOffsetX, 30, width - 30),
        clampCoord(centerY + startOffsetY, 30, height - 30)
    };
    POINT fingerBEnd{
        clampCoord(centerX + endOffsetX, 30, width - 30),
        clampCoord(centerY + endOffsetY, 30, height - 30)
    };

    AddLog(instanceId, std::string("Normalizing farm zoom (") + reason + ")...", ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
    for (int pass = 0; pass < repeatCount; ++pass) {
        if (!ignoreBotState && !g_Bots[instanceId].isRunning) return false;
        if (!ExecuteTwoFingerPinchGesture(instanceId, fingerAStart, fingerAEnd, fingerBStart, fingerBEnd)) {
            AddLog(instanceId, "Farm zoom normalization failed. Minitouch pinch could not be sent.", ImVec4(1.0f, 0.4f, 0.3f, 1.0f));
            return false;
        }
        if (!WaitForBotActionWindow(instanceId, 350, ignoreBotState)) return false;
    }

    return true;
}

static CowFeedRoutineResult RunCowFeedRoutine(int instanceId, int accountIndex, bool isTestRun) {
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
        AdbTap(instanceId, grass.x, grass.y);
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
            if (!ExecuteSingleFingerDragPath(instanceId, millDrag)) {
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

static bool RunDairyRoutine(int instanceId, int accountIndex, bool isTestRun) {
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

        if (!ExecuteSingleFingerDragPath(instanceId, dairyDrag)) {
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

static bool TryOpenOccupiedCrateFallbackGrid(int instanceId, int cropMode, int tapWaitMs = -1) {
    BotInstance& bot = g_Bots[instanceId];
    static const std::array<POINT, 10> kFallbackCrateCenters = {
        POINT{115, 182}, POINT{198, 182}, POINT{281, 182}, POINT{364, 182}, POINT{447, 182},
        POINT{115, 265}, POINT{198, 265}, POINT{281, 265}, POINT{364, 265}, POINT{447, 265}
    };
    const int effectiveTapWaitMs = (tapWaitMs > 0) ? tapWaitMs : (std::max)(450, g_Intervals.crateClickWait);

    for (const auto& slot : kFallbackCrateCenters) {
        if (!bot.isRunning) return false;
        AdbTap(instanceId, slot.x, slot.y);
        std::this_thread::sleep_for(std::chrono::milliseconds(effectiveTapWaitMs));
        cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (IsOccupiedCrateEditPanelVisible(verifyScreen) || IsSalePanelVisible(verifyScreen)) return true;
        if (!IsMarketViewLikelyVisible(verifyScreen, cropMode)) return true;
    }
    return false;
}

static bool EnsureSalePanelClosed(int instanceId, int cropMode, const char* contextLabel, int maxAttempts = 2) {
    BotInstance& bot = g_Bots[instanceId];
    const int coordinateSettleMs = 420;
    const int templateSettleMs = 520;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (!bot.isRunning) return false;
        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (!IsSalePanelVisible(screen)) return true;

        if (attempt > 1) {
            AddLog(instanceId, std::string("Sale panel still visible during ") + contextLabel + ". Clicking cross again...", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
        }

        AdbTap(instanceId, g_CoordinateProfile.salePanelCloseX, g_CoordinateProfile.salePanelCloseY);
        std::this_thread::sleep_for(std::chrono::milliseconds(coordinateSettleMs));

        cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (!IsSalePanelVisible(verifyScreen)) return true;

        MatchResult closeRes = FindBestCrossAny(verifyScreen);
        if (closeRes.found) {
            AddLog(instanceId, std::string("Sale panel close coordinate missed during ") + contextLabel + ". Trying detected cross...", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
            AdbTap(instanceId, closeRes.x, closeRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(templateSettleMs));
        }
    }
    cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    return !IsSalePanelVisible(verifyScreen);
}

static bool EnsureMarketClosed(int instanceId, int cropMode, const char* contextLabel, int maxAttempts = 3) {
    BotInstance& bot = g_Bots[instanceId];
    const int coordinateSettleMs = 450;
    const int templateSettleMs = 560;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (!bot.isRunning) return false;
        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (!IsMarketViewVisible(screen, cropMode)) return true;

        if (attempt > 1) {
            AddLog(instanceId, std::string("Market view still visible during ") + contextLabel + ". Clicking close again...", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
        }

        AdbTap(instanceId, g_CoordinateProfile.marketCloseX, g_CoordinateProfile.marketCloseY);
        std::this_thread::sleep_for(std::chrono::milliseconds(coordinateSettleMs));

        cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (!IsMarketViewVisible(verifyScreen, cropMode)) return true;

        AdbTap(instanceId, g_CoordinateProfile.marketCloseSecondX, g_CoordinateProfile.marketCloseSecondY);
        std::this_thread::sleep_for(std::chrono::milliseconds(coordinateSettleMs));

        verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (!IsMarketViewVisible(verifyScreen, cropMode)) return true;

        MatchResult closeRes = FindImage(verifyScreen, market_close_crosstemplatePath, std::max(0.65f, g_Thresholds.marketCloseCrossThreshold), false, 1.0f, false);
        if (!closeRes.found) closeRes = FindBestCrossAny(verifyScreen);
        if (closeRes.found) {
            AddLog(instanceId, std::string("Market close coordinates missed during ") + contextLabel + ". Trying detected cross...", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
            AdbTap(instanceId, closeRes.x, closeRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(templateSettleMs));
        }
    }
    cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    return !IsMarketViewVisible(verifyScreen, cropMode);
}

static std::string SanitizeDebugLabel(std::string text) {
    for (char& c : text) {
        if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
    }
    return text;
}

static std::string SavePreserveDebugArtifacts(int instanceId, const std::string& reasonTag, const ItemCountDebugInfo& info, const cv::Mat& fullScreen, bool ok) {
    std::string debugDir = GetAppDataPath() + "\\ocr_debug";
    std::error_code ec;
    fs::create_directories(debugDir, ec);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    long long stamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::string suffix = ok ? "ok" : "low_score";
    std::string baseName = "inst" + std::to_string(instanceId + 1) + "_preserve_" + std::to_string(stamp) + "_" + SanitizeDebugLabel(reasonTag) + "_" + suffix;
    std::string basePath = debugDir + "\\" + baseName;

    if (!fullScreen.empty()) cv::imwrite(basePath + "_full.png", fullScreen);
    if (!info.rawCrop.empty()) cv::imwrite(basePath + "_raw.png", info.rawCrop);
    if (!info.finalCrop.empty()) cv::imwrite(basePath + "_final.png", info.finalCrop);

    std::ofstream meta(basePath + "_meta.txt");
    if (meta.is_open()) {
        meta << "count=" << info.count << "\n";
        meta << "score=" << info.ocrScore << "\n";
        meta << "digits=" << info.digits << "\n";
        meta << "method=" << info.readMethod << "\n";
        meta << "anchorFound=" << (info.anchorFound ? 1 : 0) << "\n";
        meta << "roiValid=" << (info.roiValid ? 1 : 0) << "\n";
        meta << "focused=" << (info.usedFocusedCrop ? "yes" : "no") << "\n";
        meta << "roi=" << info.roi.x << "," << info.roi.y << "," << info.roi.width << "," << info.roi.height << "\n";
    }

    return basePath;
}

namespace {
    constexpr int kMaxSaleStack = 10;
    constexpr int kSaleQuantityControlSpacingX = 113;
    constexpr int kSaleStackModeGameDefault = 0;
    constexpr int kSaleStackModeMax = 1;
    constexpr int kSaleStackModeFixed = 2;
    constexpr int kSaleStackModePreserveOcr = 3;

    const char* GetSaleStackModeName(int mode) {
        switch (mode) {
        case kSaleStackModeGameDefault: return "Game Default";
        case kSaleStackModeMax: return "Max Stack";
        case kSaleStackModeFixed: return "Fixed Stack";
        case kSaleStackModePreserveOcr: return "Preserve OCR";
        default: return "Unknown";
        }
    }

}

struct WheatPreservePlan {
    bool active = false;
    bool ocrReadable = false;
    bool skipSale = false;
    int currentCount = 0;
    int reserveCount = 0;
    int desiredStack = 0;
    int panelStart = 0;
    int ocrScore = 0;
    std::string digits;
    std::string readMethod;
};

static WheatPreservePlan BuildWheatPreservePlan(const cv::Mat& salePanelScreen, const AccountSlot& account, bool isWheatSale) {
    WheatPreservePlan plan;
    plan.active = (isWheatSale && account.saleStackMode == kSaleStackModePreserveOcr && account.keepWheatReserve > 0);
    if (!plan.active || salePanelScreen.empty()) return plan;

    ItemCountDebugInfo debug = ReadItemCountByAnchorDebug(salePanelScreen, wheatshop_templatePath, false);
    plan.ocrReadable = debug.count > 0;
    plan.currentCount = debug.count;
    plan.reserveCount = (std::max)(0, account.keepWheatReserve);
    plan.ocrScore = debug.ocrScore;
    plan.digits = debug.digits;
    plan.readMethod = debug.readMethod;

    if (!plan.ocrReadable) return plan;

    plan.panelStart = plan.currentCount >= 20 ? 10 : (std::max)(1, plan.currentCount / 2);
    int sellable = plan.currentCount - plan.reserveCount;
    if (sellable <= 0) {
        plan.skipSale = true;
        return plan;
    }

    plan.desiredStack = (std::max)(1, (std::min)(kMaxSaleStack, sellable));
    return plan;
}

static bool TryOpenSickleMenuFromCandidates(int instanceId, const std::vector<MatchResult>& grownCandidates, MatchResult& sickleRes, MatchResult* chosenAnchor = nullptr) {
    if (grownCandidates.empty()) return false;

    constexpr int kMaxHarvestCandidatesToTry = 6;
    float sickleThreshold = (std::max)(0.68f, g_Thresholds.sickleThreshold - 0.08f);
    int candidateCount = (std::min)(static_cast<int>(grownCandidates.size()), kMaxHarvestCandidatesToTry);

    for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex) {
        const MatchResult& candidate = grownCandidates[candidateIndex];
        AdbTap(instanceId, candidate.x, candidate.y);
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));

        for (int verifyTry = 0; verifyTry < 3; ++verifyTry) {
            cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
            sickleRes = FindImage(verifyScreen, s_templatePath, sickleThreshold, false);
            if (sickleRes.found) {
                if (chosenAnchor) *chosenAnchor = candidate;
                return true;
            }

            if (verifyTry == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
            else if (verifyTry == 1) {
                AdbTap(instanceId, candidate.x, candidate.y);
                std::this_thread::sleep_for(std::chrono::milliseconds(700));
            }
        }

        cv::Mat cleanupScreen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
        MatchResult crossRes = FindImage(cleanupScreen, cross_templatePath, g_Thresholds.crossThreshold, false);
        if (crossRes.found) {
            AdbTap(instanceId, crossRes.x, crossRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
        }
    }

    return false;
}

static bool ScanBarnAndSiloFromMarket(int instanceId, int accountIndex, unsigned long long token = 0ULL, bool enforceToken = false) {
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





// INJECT FOLDERS
const std::string GAME_DATA_PATH = "/data/data/com.supercell.hayday/shared_prefs/storage_new.xml";
const std::string ZOOM_DATA_PATH = "/data/data/com.supercell.hayday/update/data/game_config.csv";

// TRANSFER PART (NOT DONE )
int g_TransferThreshold = 10;
TransferRequest g_TransferRequest;


// ==============================================================================
std::string GetUniversalAdbPath(int instanceId) {
    if (g_Bots[instanceId].emulatorType == 1) return kLDPlayerAdbPath;
    // IF YOU WANT TO ADD BLUESTACKS SUPPORT, ADD else if (type == 2) HERE. I QUITTED THE PROJECT BEFORE ADDING BLUESTACKS.
    return kAdbPath; // MEMU AS DEFAULT
}
// ==============================================================================
// RUN CMD IN BACKGROUND BECAUSE WITHOUT THIS , THE CMD WINDOW WILL POP UP (FLICKER, OPEN AND CLOSE IMMEDIATELY) WHICH IS ANNOYING. THIS FUNCTION HIDES THE CMD WINDOW COMPLETELY.
bool RunCmdHidden(const std::string& command) {
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::string finalCmd = command;
    std::vector<char> cmdBuffer(finalCmd.begin(), finalCmd.end());
    cmdBuffer.push_back(0);

    BOOL ok = CreateProcessA(nullptr, cmdBuffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) return false;

    DWORD waitResult = WaitForSingleObject(pi.hProcess, 10000);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0;
}
// GET ADB OUTPUT AND RETURN OUTPUT AS STRING TO READ STUFF LIKE INPUT DEVICE EVENT ETC.
std::string GetAdbOutput(int instanceId, std::string args) {
    std::string serial = g_Bots[instanceId].adbSerial; // EXAMPLE: 127.0.0.1:21503
    std::string cmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + serial + " " + args + "\"";

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hRead = NULL;
    HANDLE hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return "";
    }

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    CloseHandle(hWrite);

    std::string result = "";
    DWORD read;
    char buffer[256];
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &read, NULL) && read != 0) {
        buffer[read] = '\0';
        result += buffer;
    }
    CloseHandle(hRead);
    return result;
}
// THIS FUNCTION HELPS TO MAKE SURE USER IS USING 640X480 100 DPI RESOLUTION. IF NOT, BOT WON'T START.
std::string GetMEmuConfig(int instanceId, std::string key) {
	// FIND MEMUC.EXE BASED ON THE ADB PATH (IN CASE USER MOVED MEmu FOLDER OR RENAMED IT, WE CAN STILL FIND MEMUC.EXE) BECAUSE MEMUC EXE IS IN THE SAME FOLDER AS ADB.EXE
    std::string memucPath = kAdbPath;
    size_t lastSlash = memucPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        memucPath = memucPath.substr(0, lastSlash + 1) + "memuc.exe";
        // EXAMPLE "D:\MEmu\adb.exe" -> "D:\MEmu\" + "memuc.exe"
    }

    // 2. PREPARE THE COMMAND
    std::string cmd = "cmd.exe /c \"\"" + memucPath + "\" getconfig -i " + std::to_string(instanceId) + " " + key + "\"";

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hRead = NULL;
    HANDLE hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return "";
    }

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE; // CMD DOESNT POPS UP

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 3000); //WAIT UP TO 3 SECONDS
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    CloseHandle(hWrite);

    DWORD read;
    char buffer[128];
    ZeroMemory(buffer, sizeof(buffer));
    ReadFile(hRead, buffer, sizeof(buffer) - 1, &read, NULL);
    CloseHandle(hRead);

    std::string rawResult = buffer;
    std::string cleanResult = "";

    // CHECK RETURNED STRING AND EXTRACT NECESSARY PART (WE NEED DIGITS ONLY)
    for (char c : rawResult) {
        if (isdigit(c)) { 
            cleanResult += c; 
        }
    }
    // FOR EXAMPLE: IF RETURNED STRING IS " 640\r\n", cleanResult WILL TURN TO "640".

    return cleanResult;
}


// ADB COMMAND RUNNER
bool RunAdbCommand(int instanceId, std::string args) {
    std::string serial = g_Bots[instanceId].adbSerial;
    if (serial.empty()) return false;

    std::string cmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + serial + " " + args + "\"";
    return RunCmdHidden(cmd);
}
// TAP FUNCTION TO TAP ON THE SCREEN
void AdbTap(int instanceId, int x, int y) {
    RunAdbCommand(instanceId, "shell input tap " + std::to_string(x) + " " + std::to_string(y));
}

static bool AdbTapRepeated(int instanceId, int x, int y, int count) {
    if (count <= 0) return true;
    if (count == 1) {
        AdbTap(instanceId, x, y);
        return true;
    }

    count = (std::min)(count, 30);
    std::ostringstream command;
    command << "shell ";
    for (int i = 0; i < count; ++i) {
        if (i > 0) command << "; ";
        command << "input tap " << x << " " << y;
    }
    return RunAdbCommand(instanceId, command.str());
}

static bool AdbTapRepeatedSlow(int instanceId, int x, int y, int count, int delayMs = 85) {
    if (count <= 0) return true;
    count = (std::min)(count, 30);
    delayMs = (std::max)(25, delayMs);
    for (int i = 0; i < count; ++i) {
        if (!g_Bots[instanceId].isRunning) return false;
        AdbTap(instanceId, x, y);
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    return true;
}

// FUNCTION USED IN AUTO ITEM TRANSFER, THIS FUNCTION CLOSES ALL THE MENUS BY SEARCHING CROSS AND TAP ON IT OVER AND OVER UNTIL THERES NO CROSS.
void ForceCloseAllMenus(int instanceId) {
    AddLog(instanceId, "Closing all menus to return to main screen...", ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
    for (int i = 0; i < 3; i++) {
        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
        MatchResult crossRes = FindBestCrossAny(screen);
        if (crossRes.found) {
            AdbTap(instanceId, crossRes.x, crossRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        }
        else {
			DismissUnknownOverlay(instanceId, "ForceCloseAllMenus last resort");
			break;
        }
    }
}

// TEMPLATE TEST BUTTONS USED IN GUI TO HELP USERS IF THEIR TEMPLATES ARE WORKING PROPERLY OR NOT
void PerformTemplateTest(int instanceId, std::string templatePath, std::string testName, float threshold, bool useGrayscale) {
    auto token = ++g_TestGeneration[instanceId];
    AddLog(instanceId, std::string(Tr("Running ")) + Tr(testName.c_str()) + "...", ImVec4(1, 1, 0, 1));

    std::thread([instanceId, templatePath, testName, threshold, useGrayscale, token]() {
        BotInstance& bot = g_Bots[instanceId];
        auto testSleep = [&](int ms) -> bool {
            int elapsed = 0;
            while (elapsed < ms) {
                if (!IsTestStillCurrent(instanceId, token)) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                elapsed += 50;
            }
            return true;
        };
        auto fullFind = [&](const cv::Mat& frame, const std::string& path, float thr, bool gray = false) {
            return FindImage(frame, path, thr, gray, 1.0f, false);
        };
        CropRuntimeInfo crop = GetCropRuntimeInfo(bot.testCropMode);
        auto currentProductTemplate = [&]() { return crop.productTemplate; };
        auto currentProductThreshold = [&]() { return crop.productThreshold; };
        auto currentSeedTemplate = [&]() { return crop.seedTemplate; };
        auto currentSeedThreshold = [&]() { return crop.seedThreshold; };
        auto closeMarketPanels = [&]() {
            for (int n = 0; n < 2; ++n) {
                if (!IsTestStillCurrent(instanceId, token)) return;
                cv::Mat scr = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                MatchResult closeRes = FindBestCrossAny(scr);
                if (closeRes.found) AdbTap(instanceId, closeRes.x, closeRes.y);
                else if (n == 0) AdbTap(instanceId, g_CoordinateProfile.salePanelCloseX, g_CoordinateProfile.salePanelCloseY);
                else AdbTap(instanceId, g_CoordinateProfile.marketCloseSecondX, g_CoordinateProfile.marketCloseSecondY);
                if (!testSleep(500)) return;
            }
        };
        auto ensureShopOpenForTest = [&](cv::Mat& screen) -> bool {
            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            if (IsMarketViewLikelyVisible(screen, bot.testCropMode)) return true;

            MatchResult shopRes = fullFind(screen, shop_templatePath, std::max(0.70f, g_Thresholds.shopThreshold - 0.08f), false);
            if (!shopRes.found) return false;

            AddLog(instanceId, "TEST SHOP: opening shop...", ImVec4(0, 1, 1, 1));
            AdbTap(instanceId, shopRes.x, shopRes.y);
            if (!testSleep(std::max(1800, g_Intervals.shopEnterWait))) return false;
            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            return IsMarketViewLikelyVisible(screen, bot.testCropMode);
        };
        auto openSalePanelForTest = [&](cv::Mat& saleScreen) -> bool {
            if (!ensureShopOpenForTest(saleScreen)) return false;

            MatchResult emptyCrate = fullFind(saleScreen, crate_templatePath, std::max(0.70f, g_Thresholds.crateThreshold - 0.10f), false);
            if (!emptyCrate.found) return false;

            AddLog(instanceId, "TEST SALE PANEL: empty crate found. Opening the panel...", ImVec4(0, 1, 0, 1));
            AdbTap(instanceId, emptyCrate.x, emptyCrate.y);
            if (!testSleep(std::max(900, g_Intervals.crateClickWait))) return false;

            saleScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult productRes = fullFind(saleScreen, currentProductTemplate(), std::max(0.65f, currentProductThreshold()), false);
            if (!productRes.found) productRes = fullFind(saleScreen, currentSeedTemplate(), std::max(0.65f, currentSeedThreshold() - 0.08f), false);
            if (!productRes.found) return false;

            AddLog(instanceId, std::string("TEST SALE PANEL: selecting ") + crop.name + "...", ImVec4(0, 1, 0, 1));
            AdbTap(instanceId, productRes.x, productRes.y);
            if (!testSleep(std::max(800, g_Intervals.productSelectWait + 300))) return false;

            saleScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            return IsSalePanelVisible(saleScreen);
        };

        if (testName == "Field Check") {
            cv::Mat frame = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            auto fields = FindEmptyFields(frame, false);
            if (!IsTestStillCurrent(instanceId, token)) return;
            if (!fields.empty()) {
                AddLog(instanceId, "TEST FIELD: empty field found at " + std::to_string(fields[0].x) + "," + std::to_string(fields[0].y), ImVec4(0, 1, 0, 1));
                AdbTap(instanceId, fields[0].x, fields[0].y);
            } else {
                AddLog(instanceId, "TEST FIELD: no empty fields detected.", ImVec4(1, 0.5f, 0, 1));
            }
            return;
        }

        if (testName == "Grow Check") {
            cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            auto fields = FindEmptyFields(screen, false);
            if (fields.empty()) {
                AddLog(instanceId, "TEST GROW: no empty fields detected.", ImVec4(1, 0.5f, 0, 1));
                return;
            }
            AddLog(instanceId, "TEST GROW: opening seed menu...", ImVec4(0, 1, 1, 1));
            AdbTap(instanceId, fields[0].x, fields[0].y);
            if (!testSleep(1200)) return;
            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult seedRes = fullFind(screen, currentSeedTemplate(), currentSeedThreshold(), false);
            if (!seedRes.found) {
                AddLog(instanceId, "TEST GROW: seed not found.", ImVec4(1, 0.5f, 0, 1));
                return;
            }
            AddLog(instanceId, "TEST GROW: seed found. Planting visible fields...", ImVec4(0, 1, 0, 1));
            ExecuteDenseGridGesture(instanceId, seedRes.x, seedRes.y, fields);
            testSleep(std::max(800, g_Intervals.afterPlantWait));
            return;
        }

        if (testName == "Barn/Silo Check") {
            AddLog(instanceId, "TEST BARN/SILO: starting market -> empty crate -> barn -> silo flow...", ImVec4(0, 1, 1, 1));
            bool ok = ScanBarnAndSiloFromMarket(instanceId, bot.currentAccountIndex, token, true);
            if (ok) AddLog(instanceId, "TEST BARN/SILO: completed.", ImVec4(0, 1, 0, 1));
            return;
        }

        if (testName == "Silo Full Cross Check") {
            AddLog(instanceId, "TEST SILO FULL CROSS: checking for silo-full popup and close button...", ImVec4(0, 1, 1, 1));
            cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult siloPopup;
            if (!IsSiloFullPopupVisible(screen, &siloPopup)) {
                AddLog(instanceId, "TEST SILO FULL CROSS: silo-full popup is not visible right now.", ImVec4(1, 0.5f, 0, 1));
                return;
            }

            for (int attempt = 1; attempt <= 2; ++attempt) {
                MatchResult closeRes = fullFind(screen, silo_full_cross_templatePath, std::max(0.65f, g_Thresholds.siloFullCrossThreshold), false);
                if (!closeRes.found) closeRes = FindBestCrossAny(screen);
                if (closeRes.found) {
                    AddLog(instanceId, "TEST SILO FULL CROSS: clicking close attempt " + std::to_string(attempt) + ".", ImVec4(0, 1, 0, 1));
                    AdbTap(instanceId, closeRes.x, closeRes.y);
                }
                else {
                    AddLog(instanceId, "TEST SILO FULL CROSS: close template missing, tapping popup fallback.", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                    AdbTap(instanceId, siloPopup.x, siloPopup.y);
                }
                if (!testSleep(700)) return;

                screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                if (!IsSiloFullPopupVisible(screen, &siloPopup)) {
                    AddLog(instanceId, "TEST SILO FULL CROSS: popup cleared successfully.", ImVec4(0, 1, 0, 1));
                    return;
                }
            }

            AddLog(instanceId, "TEST SILO FULL CROSS: popup is still visible after two close attempts.", ImVec4(1, 0.3f, 0.3f, 1));
            return;
        }

        if (testName == "Preserve OCR Check") {
            if (bot.testCropMode != 0) {
                AddLog(instanceId, "TEST PRESERVE OCR: this test is wheat-only. Switch Crop Mode to Wheat first.", ImVec4(1.0f, 0.55f, 0.2f, 1.0f));
                return;
            }

            AddLog(instanceId, "TEST PRESERVE OCR: opening sale panel and reading the wheat reserve count...", ImVec4(0, 1, 1, 1));
            cv::Mat saleScreen;
            if (!openSalePanelForTest(saleScreen)) {
                AddLog(instanceId, "TEST PRESERVE OCR: could not open the wheat sale panel.", ImVec4(1, 0.5f, 0, 1));
                return;
            }

            ItemCountDebugInfo debug = ReadItemCountByAnchorDebug(saleScreen, wheatshop_templatePath, false);
            AccountSlot& activeAccount = bot.accounts[bot.currentAccountIndex];
            activeAccount.preserveReadDetail = BuildPreserveReadDetail(debug);
            bool ok = debug.count > 0 && debug.ocrScore >= 35;
            std::string debugBase = SavePreserveDebugArtifacts(instanceId, "manual_test", debug, saleScreen, ok);

            AddLog(instanceId,
                "TEST PRESERVE OCR: " + activeAccount.preserveReadDetail + " | capture=" + debugBase,
                ok ? ImVec4(0.2f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.55f, 0.2f, 1.0f));
            closeMarketPanels();
            return;
        }

        if (testName == "Sale Mode Check") {
            AddLog(instanceId, std::string("TEST SALE MODE: opening the ") + crop.name + " sale panel...", ImVec4(0, 1, 1, 1));
            cv::Mat saleScreen;
            if (!openSalePanelForTest(saleScreen)) {
                AddLog(instanceId, "TEST SALE MODE: could not open the sale panel.", ImVec4(1, 0.5f, 0, 1));
                return;
            }

            AccountSlot& activeAccount = bot.accounts[bot.currentAccountIndex];
            int saleStackMode = (std::max)(kSaleStackModeGameDefault, (std::min)(kSaleStackModePreserveOcr, activeAccount.saleStackMode));
            int fixedSaleStack = (std::max)(1, (std::min)(kMaxSaleStack, activeAccount.fixedSaleStack));
            bool isWheatSale = (bot.testCropMode == 0);
            WheatPreservePlan preservePlan = BuildWheatPreservePlan(saleScreen, activeAccount, isWheatSale);

            std::string modeDetail = std::string("crop=") + crop.name + ", mode=" + GetSaleStackModeName(saleStackMode);
            if (saleStackMode == kSaleStackModeFixed) {
                modeDetail += ", targetStack=" + std::to_string(fixedSaleStack);
            }
            else if (saleStackMode == kSaleStackModeMax) {
                modeDetail += ", targetStack=10";
            }
            else if (saleStackMode == kSaleStackModePreserveOcr) {
                if (preservePlan.active) {
                    modeDetail += ", preserve=" + std::to_string(preservePlan.currentCount) +
                        ", reserve=" + std::to_string(preservePlan.reserveCount) +
                        ", desired=" + std::to_string(preservePlan.desiredStack) +
                        ", panelStart=" + std::to_string(preservePlan.panelStart) +
                        ", method=" + preservePlan.readMethod +
                        ", score=" + std::to_string(preservePlan.ocrScore) +
                        ", digits=\"" + preservePlan.digits + "\"";
                }
                else {
                    modeDetail += isWheatSale ? ", preserve inactive (reserve is 0)." : ", preserve only applies to wheat.";
                }
            }
            else {
                modeDetail += ", targetStack=game-default";
            }

            AddLog(instanceId, "TEST SALE MODE: " + modeDetail, ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
            closeMarketPanels();
            return;
        }

        if (testName == "Cow Feed Check") {
            AddLog(instanceId, "TEST COW FEED: opening the feed mill, queueing cow feed, and feeding the cow pasture...", ImVec4(0, 1, 1, 1));
            CowFeedRoutineResult result = RunCowFeedRoutine(instanceId, bot.currentAccountIndex, true);
            AddLog(instanceId, std::string("TEST COW FEED: ") + ToString(result), result == CowFeedRoutineResult::Completed ? ImVec4(0.2f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
            return;
        }

        if (testName == "Harvest Check") {
            cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            auto grown = FindGrownCandidatesForCrop(screen, bot.testCropMode);
            if (grown.empty()) {
                AddLog(instanceId, "TEST HARVEST: no grown crops detected.", ImVec4(1, 0.5f, 0, 1));
                return;
            }
            AddLog(instanceId, "TEST HARVEST: grown crops found. Verifying harvest candidates before using the sickle...", ImVec4(0, 1, 1, 1));
            MatchResult sickleRes;
            bool harvestAnchorFound = TryOpenSickleMenuFromCandidates(instanceId, grown, sickleRes, nullptr);
            if (!testSleep(50)) return;
            if (!harvestAnchorFound) {
                AddLog(instanceId, "TEST HARVEST: none of the grown-crop candidates opened the sickle menu.", ImVec4(1, 0.5f, 0, 1));
                return;
            }
            if (!sickleRes.found) {
                AddLog(instanceId, "TEST HARVEST: sickle not found.", ImVec4(1, 0.5f, 0, 1));
                return;
            }
            AddLog(instanceId, "TEST HARVEST: sickle found from a verified grown crop. Executing real harvest...", ImVec4(0, 1, 0, 1));
            ExecuteDenseGridGesture(instanceId, sickleRes.x, sickleRes.y, grown);
            testSleep(g_Intervals.afterHarvestWait);
            return;
        }

        if (testName == "Sell Check") {
            AddLog(instanceId, std::string("TEST SELL: opening the ") + crop.name + " sale panel...", ImVec4(0, 1, 1, 1));
            cv::Mat screen;
            if (!openSalePanelForTest(screen)) {
                AddLog(instanceId, "TEST SELL: could not open the sale panel.", ImVec4(1, 0.5f, 0, 1));
                return;
            }
            AddLog(instanceId, "TEST SELL: keeping the panel's default stack selection.", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
            AddLog(instanceId, "TEST SELL: applying max price.", ImVec4(0, 1, 1, 1));
            AdbTap(instanceId, g_CoordinateProfile.saleMaxPriceX, g_CoordinateProfile.saleMaxPriceY);
            if (!testSleep(250)) return;

            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult adRes = fullFind(screen, advertise_templatePath, g_Thresholds.advertiseThreshold, false);
            if (adRes.found) {
                AddLog(instanceId, "TEST SELL: placing advertisement.", ImVec4(0, 1, 1, 1));
                AdbTap(instanceId, adRes.x, adRes.y);
                if (!testSleep(300)) return;
            }

            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult saleBtn = fullFind(screen, create_sale_templatePath, g_Thresholds.createSaleThreshold, false);
            if (saleBtn.found) {
                AddLog(instanceId, "TEST SELL: clicking Put on sale.", ImVec4(0, 1, 0, 1));
                AdbTap(instanceId, saleBtn.x, saleBtn.y);
            } else {
                AddLog(instanceId, "TEST SELL: Put on Sale button not found, using fallback coordinates.", ImVec4(1, 0.5f, 0, 1));
                AdbTap(instanceId, g_CoordinateProfile.putOnSaleX, g_CoordinateProfile.putOnSaleY);
            }
            testSleep(std::max(800, g_Intervals.createSaleWait));
            return;
        }

        cv::Mat frame = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
        MatchResult res = FindImage(frame, templatePath, threshold, useGrayscale);
        if (!IsTestStillCurrent(instanceId, token)) return;

        if (res.found) {
            AddLog(instanceId, Tr(testName.c_str()) + std::string(Tr(" FOUND! Score: ")) + std::to_string((int)(res.score * 100)) + "% at " + std::to_string(res.x) + "," + std::to_string(res.y), ImVec4(0, 1, 0, 1));
            AdbTap(instanceId, res.x, res.y);
        } else {
            AddLog(instanceId, Tr(testName.c_str()) + std::string(Tr(" NOT Found.")), ImVec4(1, 0.5f, 0, 1));
        }
    }).detach();
}

// ANOTHER IMPORTANT FUNCTION. HELPS US TO USE TWO FINGER SWIPE. MINITOUCH IS A TOOL THAT CREATES A VIRTUAL TOUCHSCREEN DEVICE ON THE EMULATOR AND LETS US CONTROL IT WITH ADB COMMANDS.
void StartMinitouchStealth(int instanceId) {
    int minitouchPort = 1111 + instanceId;
    RunAdbCommand(instanceId, "forward tcp:" + std::to_string(minitouchPort) + " localabstract:minitouch");

    std::string serial = g_Bots[instanceId].adbSerial;
    std::string cmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + serial + " shell /data/local/tmp/minitouch\"";
    WinExec(cmd.c_str(), SW_HIDE);
}

typedef BOOL(WINAPI* IsHungAppWindowProc)(HWND);
// WATCHDOG FOR THE EMULATOR OR GAME CRASH
void EmulatorCrashWatchdog(int instanceId) {
    BotInstance& bot = g_Bots[instanceId];
    if (!bot.isRunning) return;

 
    static std::chrono::steady_clock::time_point hungStart[6];
    static bool isHungState[6] = { false };
    static int pidFailCount[6] = { 0 };

    // =====================================================================
	// 1. WINDOWS "NOT RESPONDING" WHITE SCREEN ERROR CHECK (EMULATOR CRASHED OR FROZE)
    // =====================================================================
    HWND hwnd = FindWindowA(NULL, bot.vmName);
    if (hwnd != NULL) {
        HMODULE hUser32 = GetModuleHandleA("user32.dll");
        if (hUser32) {
            IsHungAppWindowProc IsHung = (IsHungAppWindowProc)GetProcAddress(hUser32, "IsHungAppWindow");
            if (IsHung && IsHung(hwnd)) {
                if (!isHungState[instanceId]) {
                    
                    isHungState[instanceId] = true;
                    hungStart[instanceId] = std::chrono::steady_clock::now();
                    AddLog(instanceId, "[WATCHDOG] Emulator not responding! Waiting 15s to confirm...", ImVec4(1, 0.5f, 0, 1));
                }
                else {
					// CHECK HOW LONG IT HAS BEEN IN HUNG STATE
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - hungStart[instanceId]).count();

                    if (elapsed >= 15) { // IF ITS BEEN MORE THAN 15 SEC RESTART
                        AddLog(instanceId, "[WATCHDOG] CRITICAL: Emulator DEAD for 15s! Forcing restart...", ImVec4(1, 0, 0, 1));

                        if (bot.emulatorType == 1) { // LDPlayer
                            RunCmdHidden("cmd.exe /c \"\"" + kMEmuConsolePath + "\" quit --index " + std::to_string(bot.emuIndex) + "\"");
                            std::this_thread::sleep_for(std::chrono::seconds(5));
                            RunCmdHidden("cmd.exe /c \"\"" + kMEmuConsolePath + "\" launch --index " + std::to_string(bot.emuIndex) + "\"");
                        }
                        else { // MEmu
                            RunCmdHidden("cmd.exe /c \"\"" + kMEmuConsolePath + "\" stop " + bot.vmName + "\"");
                            std::this_thread::sleep_for(std::chrono::seconds(5));
                            RunCmdHidden("cmd.exe /c \"\"" + kMEmuConsolePath + "\" " + bot.vmName + "\"");
                        }

						isHungState[instanceId] = false; // CLEAR MEMORY OF HUNG STATE BECAUSE WE JUST RESTARTED THE EMULATOR
                        std::this_thread::sleep_for(std::chrono::seconds(30)); // WAIT FOR REBOOT
                        return;
                    }
                }
            }
            else {
				// EMULATOR IS RESPONDING FINE, BUT CHECK IF WE WERE IN HUNG STATE BEFORE. IF YES, LOG RECOVERY.
                if (isHungState[instanceId]) {
                    AddLog(instanceId, "[WATCHDOG] Emulator recovered. False alarm cancelled.", ImVec4(0, 1, 0, 1));
                    isHungState[instanceId] = false;
                }
            }
        }
    }

    // =====================================================================
    // CHECK IF GAME CRASHED AND EMULATOR IS IN THE HOME PAGE.
    // =====================================================================
    std::string serial = bot.adbSerial;
    std::string tempFile = "C:\\Users\\Public\\pid_check_" + std::to_string(instanceId) + ".txt";
    remove(tempFile.c_str());

    std::string cmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + serial + " shell pidof com.supercell.hayday > \"" + tempFile + "\"\"";
    RunCmdHidden(cmd);
    std::string currentAdb = kAdbPath;
    std::ifstream file(tempFile);
    std::string pidStr;
    if (file.is_open()) {
        std::getline(file, pidStr);
        file.close();
    }

    if (pidStr.empty() || pidStr.length() < 2) {
		pidFailCount[instanceId]++; // INCREMENT FAIL COUNT IF PID NOT FOUND

        if (pidFailCount[instanceId] >= 3) { // IF CANT FIND PID FOR 3 TIMES
            AddLog(instanceId, "[WATCHDOG] Hay Day crashed to desktop (3 Fails)! Relaunching...", ImVec4(1, 0.5f, 0, 1));
            std::string launchCmd = "cmd /c \"\"" + currentAdb + "\" -s " + serial + " shell monkey -p com.supercell.hayday -c android.intent.category.LAUNCHER 1\"";
            RunCmdHidden(launchCmd);

            pidFailCount[instanceId] = 0; // RESET
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        else {
            AddLog(instanceId, "[WATCHDOG] Game process missing... Warning " + std::to_string(pidFailCount[instanceId]) + "/3", ImVec4(1, 1, 0, 1));
        }
    }
    else {
		// PID IS BACK, RESET FAIL COUNT
        pidFailCount[instanceId] = 0;
    }
}
void HandleReviveHeartbeat(int instanceId) {
    BotInstance& bot = g_Bots[instanceId];
    if (!bot.useReviveMode || strlen(bot.reviveTemplatePath) < 5) return;

    AddLog(instanceId, "Revive: Checking custom anchor...", ImVec4(1, 1, 0, 1));

    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    MatchResult res = FindImage(screen, bot.reviveTemplatePath, 0.70f);

    if (res.found) {
        AddLog(instanceId, "Revive: System OK.", ImVec4(0, 1, 0, 1));
		bot.reviveFailCounter = 0; // SUCCESS, RESET FAIL COUNTER 
    }
    else {
        bot.reviveFailCounter++;
        AddLog(instanceId, "Revive: Anchor not found! Strike " + std::to_string(bot.reviveFailCounter) + "/3", ImVec4(1, 0.5f, 0, 1));

        if (bot.reviveFailCounter >= 3) {
            AddLog(instanceId, "CRITICAL: 3 Strikes! Restarting game engine...", ImVec4(1, 0, 0, 1));
            // REOPEN HAYDAY
            RunAdbCommand(instanceId, "shell am force-stop com.supercell.hayday");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            RunAdbCommand(instanceId, "shell monkey -p com.supercell.hayday -c android.intent.category.LAUNCHER 1");

            bot.reviveFailCounter = 0;
            std::this_thread::sleep_for(std::chrono::seconds(15)); // BOOT SLEEP
        }
    }
}

//IMPORTANT FILES INJECTOR. 
// THIS FUNCTION INJECTS(PUSHES) FILES TO THE ROOT.
void InjectImportantFiles(int instanceId) {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    std::string exeDir = std::string(buffer).substr(0, pos);

    std::string fontFile = exeDir + "\\injecthacks\\languages.csv";
    std::string zoomFile = exeDir + "\\injecthacks\\game_config.csv";
    std::string minitouchFile = exeDir + "\\injecthacks\\minitouch";

   
    std::vector<std::string> nxrthFiles = {
        "inject.nxrth", "inject2.nxrth", "inject3.nxrth", "inject4.nxrth", "inject5.nxrth"
    };

    for (const auto& file : nxrthFiles) {
        if (!fs::exists(exeDir + "\\injecthacks\\" + file)) {
            AddLog(instanceId, "Error: Missing encrypted " + file + " in injecthacks folder!", ImVec4(1, 0, 0, 1));
            return;
        }
    }
    if (!fs::exists(fontFile) || !fs::exists(zoomFile) || !fs::exists(minitouchFile)) {
        AddLog(instanceId, Tr("Error: Basic hack files (zoom/font/minitouch) missing!"), ImVec4(1, 0, 0, 1));
        return;
    }

    AddLog(instanceId, Tr("Starting NXRTH Ultimate Injection (This will take 10-15 seconds)..."), ImVec4(0.8f, 0.4f, 1.0f, 1.0f));

    std::thread([instanceId, exeDir, fontFile, zoomFile, minitouchFile]() {
        auto decryptNxrthToFile = [&](const std::string& nxPath, const std::string& tempPath) {
            std::ifstream inFile(nxPath, std::ios::binary);
            std::string encryptedData((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
            inFile.close();

            std::string rawData = DecryptXORHex(encryptedData, "NXRTH_NATURE_KEY");
            if (rawData.empty()) {
                AddLog(instanceId, "Error: Failed to decrypt " + nxPath, ImVec4(1, 0, 0, 1));
                return false;
            }

            std::ofstream outFile(tempPath, std::ios::binary);
            outFile.write(rawData.data(), rawData.size());
            outFile.close();
            return fs::exists(tempPath) && fs::file_size(tempPath) > 0;
        };

        auto installExtraTemplate = [&](const std::string& sourceName, const std::string& targetName) {
            std::string sourcePath = exeDir + "\\injecthacks\\extra_visuals\\templates\\" + sourceName;
            if (!fs::exists(sourcePath)) return false;

            std::string templateDir = exeDir + "\\templates";
            std::string backupDir = templateDir + "\\pre_extra_visuals_backup";
            std::string targetPath = templateDir + "\\" + targetName;
            std::string backupPath = backupDir + "\\" + targetName;

            fs::create_directories(templateDir);
            if (fs::exists(targetPath) && !fs::exists(backupPath)) {
                fs::create_directories(backupDir);
                fs::copy_file(targetPath, backupPath, fs::copy_options::overwrite_existing);
            }
            fs::copy_file(sourcePath, targetPath, fs::copy_options::overwrite_existing);
            return true;
        };

        int extraTemplateCount = 0;
        try {
            if (installExtraTemplate("cow_pasture.png", "cow_pasture.png")) extraTemplateCount++;
            if (installExtraTemplate("feed_mill.png", "feed_mill.png")) extraTemplateCount++;
        }
        catch (...) {
            AddLog(instanceId, "Warning: Could not install extra tagged templates.", ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
        }
        if (extraTemplateCount > 0) {
            AddLog(instanceId, "Extra visuals: installed tagged CP/FM templates.", ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
        }

		// 1. FORCE CLOSE HAY DAY TO AVOID FILE LOCKS
        AddLog(instanceId, Tr("Force stopping Hay Day..."), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        RunAdbCommand(instanceId, "shell am force-stop com.supercell.hayday");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // WAIT FOR EMULATOR JUST IN CASE

        // 2. CREATE TARGET FOLDERS
        std::string dataDir = "/data/data/com.supercell.hayday/update/data/";
        std::string scDir = "/data/data/com.supercell.hayday/update/sc/";
        RunAdbCommand(instanceId, "shell \"su -c 'mkdir -p " + dataDir + "'\"");
        RunAdbCommand(instanceId, "shell \"su -c 'mkdir -p " + scDir + "'\"");
        std::this_thread::sleep_for(std::chrono::milliseconds(800));

        // 3. FONT & LANGUAGE HACK
        AddLog(instanceId, Tr("1/4: Injecting Font & Language Hack..."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
        std::string tempFont = "/sdcard/temp_languages.csv";
        RunAdbCommand(instanceId, "push \"" + fontFile + "\" " + tempFont);
        RunAdbCommand(instanceId, "shell \"su -c 'cp " + tempFont + " " + dataDir + "languages.csv'\"");
        RunAdbCommand(instanceId, "shell \"su -c 'chmod 777 " + dataDir + "languages.csv'\"");
        RunAdbCommand(instanceId, "shell rm " + tempFont);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 

        // 4. ZOOM HACK (game_config.csv)
        AddLog(instanceId, Tr("2/4: Optimizing View Distance (Zoom Hack)..."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
        std::string tempZoom = "/sdcard/temp_config.csv";
        RunAdbCommand(instanceId, "push \"" + zoomFile + "\" " + tempZoom);
        RunAdbCommand(instanceId, "shell \"su -c 'cp " + tempZoom + " " + ZOOM_DATA_PATH + "'\"");
        RunAdbCommand(instanceId, "shell \"su -c 'chmod 777 " + ZOOM_DATA_PATH + "'\"");
        RunAdbCommand(instanceId, "shell rm " + tempZoom);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // 5. NATURE HACK (DECRYPTION ON THE FLY)
        AddLog(instanceId, Tr("3/4: Decrypting & Injecting Secure Visuals (Nature Hack)..."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));

        std::vector<std::pair<std::string, std::string>> natureMap = {
            {"inject.nxrth", "nature_new.sc"},
            {"inject2.nxrth", "nature_new_0.sctx"},
            {"inject3.nxrth", "nature_new_1.sctx"},
            {"inject4.nxrth", "nature_new_2.sctx"},
            {"inject5.nxrth", "nature_new_3.sctx"}
        };

        for (const auto& pair : natureMap) {
            std::string nxPath = exeDir + "\\injecthacks\\" + pair.first;
            std::string tempPath = exeDir + "\\injecthacks\\temp_" + pair.second;

            if (!decryptNxrthToFile(nxPath, tempPath)) {
                AddLog(instanceId, "Error: Could not prepare " + pair.second, ImVec4(1, 0, 0, 1));
                return;
            }

            // 4. PUSH FILES
            RunAdbCommand(instanceId, "push \"" + tempPath + "\" /data/local/tmp/" + pair.second);

            // 5. DELETE TEMP FILES
            fs::remove(tempPath);

            // FILES TOO BIG SO WE KINDA WAIT JUST IN CASE.
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        }

        // USE EMULATOR'S TEMP FOLDER
        AddLog(instanceId, "Applying decrypted assets to game engine...", ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        RunAdbCommand(instanceId, "shell su -c 'cp /data/local/tmp/nature_new* " + scDir + "'");
        RunAdbCommand(instanceId, "shell su -c 'chmod 777 " + scDir + "nature_new*'");

        // DELETE STUFF IN THAT TEMP FOLDER
        RunAdbCommand(instanceId, "shell rm /data/local/tmp/nature_new*");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        std::vector<std::pair<std::string, std::string>> extraVisualMap = {
            {"inject_extra_trees_0.nxrth", "nature_new_0.sctx"},
            {"inject_extra_trees_1.nxrth", "nature_new_1.sctx"},
            {"inject_extra_trees.nxrth", "nature_new_2.sctx"},
            {"inject_extra_animals_0.nxrth", "animals_0.sctx"},
            {"inject_extra_animals_1.nxrth", "animals_1.sctx"},
            {"inject_extra_animals_2.nxrth", "animals_2.sctx"},
            {"inject_extra_animals_3.nxrth", "animals_3.sctx"},
            {"inject_extra_animals_4.nxrth", "animals_4.sctx"},
            {"inject_extra_animals_5.nxrth", "animals_5.sctx"},
            {"inject_extra_animals_6.nxrth", "animals_6.sctx"},
            {"inject_extra_animals_7.nxrth", "animals_7.sctx"},
            {"inject_extra_animals_8.nxrth", "animals_8.sctx"},
            {"inject_extra_animals_9.nxrth", "animals_9.sctx"},
            {"inject_extra_animals_10.nxrth", "animals_10.sctx"},
            {"inject_extra_animals_11.nxrth", "animals_11.sctx"},
            {"inject_extra_animals_12.nxrth", "animals_12.sctx"},
            {"inject_extra_animals_13.nxrth", "animals_13.sctx"},
            {"inject_extra_animals_14.nxrth", "animals_14.sctx"},
            {"inject_extra_animals_15.nxrth", "animals_15.sctx"},
            {"inject_extra_animals_16.nxrth", "animals_16.sctx"},
            {"inject_extra_animals02_0.nxrth", "animals02_0.sctx"},
            {"inject_extra_animals02_1.nxrth", "animals02_1.sctx"},
            {"inject_extra_animals02_2.nxrth", "animals02_2.sctx"},
            {"inject_extra_animals02_3.nxrth", "animals02_3.sctx"},
            {"inject_extra_animals02_4.nxrth", "animals02_4.sctx"},
            {"inject_extra_animals03_0.nxrth", "animals03_0.sctx"},
            {"inject_extra_animals03_1.nxrth", "animals03_1.sctx"},
            {"inject_extra_animals03_2.nxrth", "animals03_2.sctx"},
            {"inject_extra_animals03_3.nxrth", "animals03_3.sctx"},
            {"inject_extra_animals03_4.nxrth", "animals03_4.sctx"},
            {"inject_extra_animals03_5.nxrth", "animals03_5.sctx"},
            {"inject_extra_animals03_6.nxrth", "animals03_6.sctx"},
            {"inject_extra_animals04_0.nxrth", "animals04_0.sctx"},
            {"inject_extra_animals04_1.nxrth", "animals04_1.sctx"},
            {"inject_extra_animals04_2.nxrth", "animals04_2.sctx"},
            {"inject_extra_animals04_3.nxrth", "animals04_3.sctx"},
            {"inject_extra_animals04_4.nxrth", "animals04_4.sctx"},
            {"inject_extra_animals04_5.nxrth", "animals04_5.sctx"},
            {"inject_extra_animals04_6.nxrth", "animals04_6.sctx"},
            {"inject_extra_animals04_7.nxrth", "animals04_7.sctx"},
            {"inject_extra_animals04_8.nxrth", "animals04_8.sctx"},
            {"inject_extra_animals04_9.nxrth", "animals04_9.sctx"},
            {"inject_extra_animals05_0.nxrth", "animals05_0.sctx"},
            {"inject_extra_animals05_1.nxrth", "animals05_1.sctx"},
            {"inject_extra_animals05_2.nxrth", "animals05_2.sctx"},
            {"inject_extra_animals05_3.nxrth", "animals05_3.sctx"},
            {"inject_extra_animals05_4.nxrth", "animals05_4.sctx"},
            {"inject_extra_animals05_5.nxrth", "animals05_5.sctx"},
            {"inject_extra_animals05_6.nxrth", "animals05_6.sctx"},
            {"inject_extra_animals05_7.nxrth", "animals05_7.sctx"},
            {"inject_extra_animals05_8.nxrth", "animals05_8.sctx"},
            {"inject_extra_animals05_9.nxrth", "animals05_9.sctx"},
            {"inject_extra_animal_accessories_0.nxrth", "animal_accessories_0.sctx"},
            {"inject_extra_animal_accessories_1.nxrth", "animal_accessories_1.sctx"},
            {"inject_extra_buildings_new_0.nxrth", "buildings_new_0.sctx"},
            {"inject_extra_buildings_new_6.nxrth", "buildings_new_6.sctx"},
            {"inject_extra_buildings_new_2.nxrth", "buildings_new_2.sctx"},
            {"inject_extra_map_2.nxrth", "map_2.sctx"}
        };

        for (const auto& pair : extraVisualMap) {
            std::string extraPayload = exeDir + "\\injecthacks\\" + pair.first;
            if (!fs::exists(extraPayload)) continue;

            AddLog(instanceId, "Extra visuals: applying " + pair.second + "...", ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
            std::string tempExtra = exeDir + "\\injecthacks\\temp_extra_" + pair.second;
            if (decryptNxrthToFile(extraPayload, tempExtra)) {
                RunAdbCommand(instanceId, "push \"" + tempExtra + "\" /data/local/tmp/" + pair.second);
                RunAdbCommand(instanceId, "shell \"su -c 'cp /data/local/tmp/" + pair.second + " " + scDir + pair.second + "'\"");
                RunAdbCommand(instanceId, "shell \"su -c 'chmod 777 " + scDir + pair.second + "'\"");
                RunAdbCommand(instanceId, "shell rm /data/local/tmp/" + pair.second);
                fs::remove(tempExtra);
            }
            else {
                AddLog(instanceId, "Warning: Extra visual payload could not be prepared: " + pair.first, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
            }
        }

        // 6. MINITOUCH PUSH
        AddLog(instanceId, Tr("4/4: Installing Minitouch Input Agent..."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
        RunAdbCommand(instanceId, "push \"" + minitouchFile + "\" /data/local/tmp/minitouch");
        RunAdbCommand(instanceId, "shell chmod 777 /data/local/tmp/minitouch");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        StartMinitouchStealth(instanceId);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        AddLog(instanceId, Tr("ALL HACKS INJECTED SUCCESSFULLY! Start the game and Set game Language To English."), ImVec4(0, 1, 0, 1));
        }).detach();
}
// ACCOUNT SAVER
void SaveAccountToSlot(int instanceId, int slotIndex) {
    AddLog(instanceId, Tr("Saving & Encrypting Account Data..."), ImVec4(1, 1, 0, 1));

    // DOĞRUDAN SENİN EXTERN FONKSİYONUNU KULLANIYORUZ
    std::string folderPath = GetAppDataPath() + "\\Backups\\Instance_" + std::to_string(instanceId);
    if (!fs::exists(folderPath)) fs::create_directories(folderPath);

    std::string pcFileName = folderPath + "\\account_" + std::to_string(slotIndex + 1) + ".nxrth";
    std::string tempRawFile = folderPath + "\\temp_raw.xml";
    std::string tempSdFile = "/sdcard/temp_backup_" + std::to_string(instanceId) + ".xml";

    std::string copyCmd = "shell \"su -c 'cat " + GAME_DATA_PATH + " > " + tempSdFile + "'\"";
    RunAdbCommand(instanceId, copyCmd);
    std::string pullCmd = "pull " + tempSdFile + " \"" + tempRawFile + "\"";
    RunAdbCommand(instanceId, pullCmd);
    RunAdbCommand(instanceId, "shell rm " + tempSdFile);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    if (fs::exists(tempRawFile) && fs::file_size(tempRawFile) > 0) {
        std::ifstream inFile(tempRawFile, std::ios::binary);
        std::string rawData((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        std::string encryptedData = EncryptXORHex(rawData, "NXRTH_LOCAL_ACCOUNT_KEY"); // SUPER DUPER SECRET KEY

        std::ofstream outFile(pcFileName, std::ios::binary);
        outFile << encryptedData;
        outFile.close();

        fs::remove(tempRawFile);

        g_Bots[instanceId].accounts[slotIndex].hasFile = true;
        g_Bots[instanceId].accounts[slotIndex].fileName = pcFileName;
        AddLog(instanceId, Tr("Account Saved & Encrypted Successfully."), ImVec4(0, 1, 0, 1));
    }
    else {
        AddLog(instanceId, Tr("Save Failed. Check Settings."), ImVec4(1, 0, 0, 1));
    }
}

// =========================================================
// DECRYPT FUNCTION FOR ACCOUNTS N OTHER STUFF USED.
// =========================================================
std::string DecryptPureXORHex(const std::string& hexStr, const std::string& key) {
    std::string text;
    if (hexStr.length() % 2 != 0) return "";
    for (size_t i = 0; i < hexStr.length(); i += 2) {
        std::string byteString = hexStr.substr(i, 2);
        char byte = (char)strtol(byteString.c_str(), NULL, 16);
        text += (byte ^ key[(i / 2) % key.length()]);
    }
    return text;
}

// =========================================================
// ZOOM HACK VERIFICATION
// Checks if game_config.csv exists on the device. If missing
// (game update, data wipe, or first run), re-pushes it.
// Called once at bot startup and after Janitor emulator reboots.
// =========================================================
static void EnsureZoomHackPresent(int instanceId) {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    std::string exeDir = std::string(buffer).substr(0, pos);
    std::string zoomFile = exeDir + "\\injecthacks\\game_config.csv";

    if (!fs::exists(zoomFile)) {
        AddLog(instanceId, "Zoom check skipped: game_config.csv not found in injecthacks folder.", ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
        return;
    }

    // Check if the zoom file already exists on the device
    std::string checkResult = "C:\\Users\\Public\\zoom_check_" + std::to_string(instanceId) + ".txt";
    remove(checkResult.c_str());
    std::string checkCmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + std::string(g_Bots[instanceId].adbSerial) +
        " shell su -c 'ls " + ZOOM_DATA_PATH + "' > \"" + checkResult + "\" 2>&1\"";
    RunCmdHidden(checkCmd);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::ifstream checkFile(checkResult);
    std::string line;
    bool zoomExists = false;
    if (checkFile.is_open()) {
        std::getline(checkFile, line);
        checkFile.close();
        zoomExists = (line.find("No such file") == std::string::npos && line.find("game_config.csv") != std::string::npos);
    }
    remove(checkResult.c_str());

    if (zoomExists) {
        AddLog(instanceId, "Zoom hack verified: game_config.csv is present on device.", ImVec4(0.5f, 0.8f, 0.5f, 1.0f));
        return;
    }

    // Missing — re-inject
    AddLog(instanceId, "Zoom hack MISSING on device! Re-injecting game_config.csv...", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
    std::string dataDir = "/data/data/com.supercell.hayday/update/data/";
    std::string tempZoom = "/sdcard/temp_zoom_reinject.csv";

    RunAdbCommand(instanceId, "shell \"su -c 'mkdir -p " + dataDir + "'\"");
    RunAdbCommand(instanceId, "push \"" + zoomFile + "\" " + tempZoom);
    RunAdbCommand(instanceId, "shell \"su -c 'cp " + tempZoom + " " + ZOOM_DATA_PATH + "'\"");
    RunAdbCommand(instanceId, "shell \"su -c 'chmod 777 " + ZOOM_DATA_PATH + "'\"");
    RunAdbCommand(instanceId, "shell rm " + tempZoom);

    AddLog(instanceId, "Zoom hack re-injected successfully.", ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
}

// =========================================================
// ACCOUNT LOADER
// =========================================================
void LoadAccountFromSlot(int instanceId, int slotIndex) {
    // DOĞRUDAN SENİN EXTERN FONKSİYONUNU KULLANIYORUZ
    std::string folderPath = GetAppDataPath() + "\\Backups\\Instance_" + std::to_string(instanceId);
    std::string pcFileName = folderPath + "\\account_" + std::to_string(slotIndex + 1) + ".nxrth";

    if (!fs::exists(pcFileName)) {
        AddLog(instanceId, Tr("Error: Slot file empty or missing!"), ImVec4(1, 0, 0, 1));
        return;
    }
    AddLog(instanceId, Tr("Decrypting & Switching Account..."), ImVec4(1, 1, 0, 1));
    if (std::string(g_Bots[instanceId].adbSerial).empty()) {
        AddLog(instanceId, "Account load aborted: this instance has no ADB serial configured.", ImVec4(1, 0.35f, 0.35f, 1));
        return;
    }

    std::ifstream inFile(pcFileName, std::ios::binary);
    std::string encryptedData((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();

    // 1. DECRYPT ACCOUNT FILES ACCOUNT*.NXRTH'S.
    std::string rawData = DecryptXORHex(encryptedData, "NXRTH_LOCAL_ACCOUNT_KEY");

    if (rawData.find("<?xml") == std::string::npos) {
        rawData = DecryptPureXORHex(encryptedData, "NXRTH_LOCAL_ACCOUNT_KEY");
    }

    if (rawData.empty() || rawData.find("<?xml") == std::string::npos) {
        AddLog(instanceId, Tr("Error: File decryption failed! Corrupted data."), ImVec4(1, 0, 0, 1));
        return;
    }

    std::string tempRawFile = folderPath + "\\temp_decrypted.xml";
    std::ofstream outFile(tempRawFile, std::ios::binary);
    outFile << rawData;
    outFile.close();

    bool forceStopOk = RunAdbCommand(instanceId, "shell am force-stop com.supercell.hayday");
    std::string tempSdFile = "/sdcard/temp_restore_" + std::to_string(instanceId) + ".xml";

    std::string pushCmd = "push \"" + tempRawFile + "\" " + tempSdFile;
    bool pushOk = RunAdbCommand(instanceId, pushCmd);

    // RENAME THE ACTUAL FILE TO STORAGE_NEW.XML
    std::string moveCmd = "shell \"su -c 'cat " + tempSdFile + " > /data/data/com.supercell.hayday/shared_prefs/storage_new.xml'\"";
    bool moveOk = RunAdbCommand(instanceId, moveCmd);
    bool chmodOk = RunAdbCommand(instanceId, "shell \"su -c 'chmod 777 /data/data/com.supercell.hayday/shared_prefs/storage_new.xml'\"");
    RunAdbCommand(instanceId, "shell rm " + tempSdFile);

    fs::remove(tempRawFile);

    if (!forceStopOk || !pushOk || !moveOk || !chmodOk) {
        AddLog(instanceId, "Account load failed before launch. Check that the emulator is online, the ADB serial is correct, and root file access still works.", ImVec4(1, 0.25f, 0.25f, 1));
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    bool launchOk = RunAdbCommand(instanceId, "shell monkey -p com.supercell.hayday -c android.intent.category.LAUNCHER 1");
    if (!launchOk) {
        AddLog(instanceId, "Account data was restored, but Hay Day did not relaunch. Check that ADB still sees the instance.", ImVec4(1, 0.35f, 0.35f, 1));
        return;
    }
    AddLog(instanceId, Tr("Account Switched. Game Restarting."), ImVec4(0, 1, 0, 1));
}
// AUTO DETECT FOR TOUCH DEVICE (EVENT NUMBER) BECAUSE SOME PEOPLE CAN FORGET USING MANUALLY.
bool AutoDetectTouchDevice(int instanceId) {
    AddLog(instanceId, Tr("Detecting Input..."), ImVec4(1, 1, 0, 1));
    std::string tempFile = "C:\\Users\\Public\\devicelist_" + std::to_string(instanceId) + ".txt";
    remove(tempFile.c_str());
    std::string cmd = "cmd /c \"\"" + kAdbPath + "\" -s " + std::string(g_Bots[instanceId].adbSerial) + " shell getevent -pl > \"" + tempFile + "\"\"";
    RunCmdHidden(cmd);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::ifstream file(tempFile);
    if (!file.is_open()) {
        AddLog(instanceId, Tr("Error: Could not read list. Check ADB path."), ImVec4(1, 0, 0, 1));
        return false;
    }
    std::string line;
    std::string currentDevice = "";
    bool found = false;
    while (std::getline(file, line)) {
        if (line.find("add device") != std::string::npos && line.find("/dev/input/") != std::string::npos) {
            size_t startPos = line.find("/dev/input/");
            currentDevice = line.substr(startPos);
            currentDevice.erase(std::remove(currentDevice.begin(), currentDevice.end(), '\r'), currentDevice.end());
            currentDevice.erase(std::remove(currentDevice.begin(), currentDevice.end(), '\n'), currentDevice.end());
        }
        if (!currentDevice.empty()) {
            if (line.find("ABS_MT_POSITION_X") != std::string::npos || line.find("0035") != std::string::npos) {
                strcpy(g_Bots[instanceId].inputDevice, currentDevice.c_str());
                AddLog(instanceId, std::string(Tr("Input Found: ")) + std::string(g_Bots[instanceId].inputDevice), ImVec4(0, 1, 0, 1));
                found = true;
                break;
            }
        }
    }
    file.close();
    if (!found) {
        strcpy(g_Bots[instanceId].inputDevice, "/dev/input/event1");
        AddLog(instanceId, Tr("Failed. Retrying next time..."), ImVec4(1, 0.5f, 0, 1));
        return false;
    }
    return true; // RETURN TRUE IF SUCCESS
}

// ==============================================================================
// 
// 2-FINGER SWIPE 
// 
// ==============================================================================

void ExecuteDenseGridGesture(int instanceId, int startX, int startY, const std::vector<MatchResult>& fields) {
    if (fields.empty()) return;

    AddLog(instanceId, "[INFO] Executing Sweep...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));

    // 1. FIND FIELD'S BORDER SO THE BOT CAN DRAG IT TO THE CORNER OF THE FIELDS.
    int minX = 99999, maxX = 0, minY = 99999, maxY = 0;
    for (const auto& f : fields) {
        if (f.x < minX) minX = f.x;
        if (f.x > maxX) maxX = f.x;
        if (f.y < minY) minY = f.y;
        if (f.y > maxY) maxY = f.y;
    }


    int marginX = 80;
    int marginY = 80;

    int targetMinX = std::max(0, minX - marginX);
    int targetMaxX = std::min(1280, maxX + marginX);
    int targetMinY = std::max(0, minY - marginY);
    int targetMaxY = std::min(720, maxY + marginY);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != INVALID_SOCKET) {
        int port = 1111 + instanceId;
        sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        server.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == 0) {

            int p1_startX = startX;
            int p1_startY = startY;
            int p2_startX = startX + 5;
            int p2_startY = startY + 5;

            // PHASE 1: DRAG IT LIKE V SHAPE
            int p1_x1 = targetMinX; int p1_y1 = targetMaxY;
            int p2_x1 = targetMaxX; int p2_y1 = targetMaxY;

            // PHASE 2: LINEAR STRAIGHT TO THE TOP
            int p1_x2 = p1_x1; int p1_y2 = targetMinY;
            int p2_x2 = p2_x1; int p2_y2 = targetMinY;

            // PHASE 3: SWIPE BACK DOWN AGAIN JUST IN CASE.
            int p1_x3 = p1_x1; int p1_y3 = targetMaxY;
            int p2_x3 = p2_x1; int p2_y3 = targetMaxY;

            // PRESS WITH 2 FINGERS ON THE SCREEN
            std::string cmd = "d 0 " + std::to_string(p1_startX) + " " + std::to_string(p1_startY) + " 50\n" +
                "d 1 " + std::to_string(p2_startX) + " " + std::to_string(p2_startY) + " 50\nc\n";
            send(sock, cmd.c_str(), cmd.length(), 0);

            // WAIT FOR SEED MENU
            std::this_thread::sleep_for(std::chrono::milliseconds(350));

            auto interpolate = [&](int x0, int y0, int x1, int y1, int tX0, int tY0, int tX1, int tY1) {
                float dist0 = std::hypot(tX0 - x0, tY0 - y0);
                float dist1 = std::hypot(tX1 - x1, tY1 - y1);
                float maxDist = std::max(dist0, dist1);

                // THE LESSER THE SPEED, THE BETTER FOR THE BOT TO MISS EMPTY FIELDS.
                int steps = std::max(20, (int)(maxDist / 5.0f));

                for (int i = 1; i <= steps; ++i) {
                    float t = (float)i / steps;
                    int cx0 = x0 + (int)((tX0 - x0) * t);
                    int cy0 = y0 + (int)((tY0 - y0) * t);
                    int cx1 = x1 + (int)((tX1 - x1) * t);
                    int cy1 = y1 + (int)((tY1 - y1) * t);

                    std::string mCmd = "m 0 " + std::to_string(cx0) + " " + std::to_string(cy0) + " 50\n" +
                        "m 1 " + std::to_string(cx1) + " " + std::to_string(cy1) + " 50\nc\n";
                    send(sock, mCmd.c_str(), mCmd.length(), 0);

                   
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                };

            interpolate(p1_startX, p1_startY, p2_startX, p2_startY, p1_x1, p1_y1, p2_x1, p2_y1);
            interpolate(p1_x1, p1_y1, p2_x1, p2_y1, p1_x2, p1_y2, p2_x2, p2_y2);
            interpolate(p1_x2, p1_y2, p2_x2, p2_y2, p1_x3, p1_y3, p2_x3, p2_y3);


            std::string cmdUp0 = "u 0\nc\n";
            send(sock, cmdUp0.c_str(), cmdUp0.length(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // RELEASE FIRST FINGER

            std::string cmdUp1 = "u 1\nc\n";
			send(sock, cmdUp1.c_str(), cmdUp1.length(), 0); // RELEASE SECOND FINGER 

            // GIVE SOME TIME TO ANDROID BEFORE CLOSING TCP CONNECTION
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            AddLog(instanceId, "[SUCCESS] 2-Finger Smart Sweep complete!", ImVec4(0, 1, 0, 1));
        }
        else {
            AddLog(instanceId, "[ERROR] Minitouch connection failed!", ImVec4(1, 0, 0, 1));
        }
        closesocket(sock);
    }
    WSACleanup();
}


// FUNCTION NAME TELLS IT ALL, SCANS STUFF WITH COLOR. BECAUSE BOT CHANGING COLORS OF THE STUFF IN THE GAME
MatchResult FindGrownCropByColor(const cv::Mat& screen, int cropMode, int anchorX, int anchorY) {
    MatchResult result = { false, -1, -1, 0.0 };
    if (screen.empty()) return result;

    // ROI
    int topMargin = 70;
    int bottomMargin = 40;
    int sideMargin = 30;

    if (screen.rows <= topMargin + bottomMargin || screen.cols <= sideMargin * 2) return result;

    cv::Rect roiRect(sideMargin, topMargin, screen.cols - (sideMargin * 2), screen.rows - (topMargin + bottomMargin));
    cv::Mat searchArea = screen(roiRect);

    // 2. CHANGE COLOR SPACE
    cv::Mat hsv;
    cv::cvtColor(searchArea, hsv, cv::COLOR_BGR2HSV);

    // 3. COLOR FILTER
    cv::Scalar lowerBound, upperBound;
    if (cropMode == 0) { // WHEAT (Cyan)
        lowerBound = cv::Scalar(85, 200, 200); upperBound = cv::Scalar(95, 255, 255);
    }
    else if (cropMode == 1) { // CORN (Pure Blue)
        lowerBound = cv::Scalar(115, 200, 200); upperBound = cv::Scalar(125, 255, 255);
    }
    else if (cropMode == 2) { // CARROT (Hot Pink)
        lowerBound = cv::Scalar(160, 150, 200); upperBound = cv::Scalar(175, 255, 255);
    }
    else if (cropMode == 3) { // SOYBEAN (Mint Green)
        lowerBound = cv::Scalar(70, 200, 200); upperBound = cv::Scalar(80, 255, 255);
    }
    else if (cropMode == 4) { // SUGARCANE (Electric Purple)
        lowerBound = cv::Scalar(130, 200, 200); upperBound = cv::Scalar(140, 255, 255);
    }
    else {
        return result;
    }

    cv::Mat mask;
    cv::inRange(hsv, lowerBound, upperBound, mask);

    // =========================================================================
	// 4. CLEAN LITTLE DOTS TO AVOID FALSE DETECTIONS AND FILL SPACES IN BIGGER AREAS.
    // =========================================================================
    // A. DELETE DUST-LIKE TRASH (LIKE DOTS WITH 1-2 PIXEL BIG) USING (MORPH_OPEN)
    cv::Mat kernelOpen = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernelOpen);

    // B. FILL THE EMPTY SPACE IN FIELDS (MORPH_CLOSE)
    cv::Mat kernelClose = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 7));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernelClose);

	// 5. FIND CONTOURS
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double bestMetric = (anchorX != -1 && anchorY != -1) ? 9999999.0 : 0.0;
    cv::Point bestCenter(-1, -1);

    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);

        // AREA THRESHOLD
        if (area > 50) {
            cv::Moments m = cv::moments(contours[i]);
            if (m.m00 != 0) {
                int localX = (int)(m.m10 / m.m00);
                int localY = (int)(m.m01 / m.m00);

                // =====================================================================
                // DONUT HOLE PROTECTION
                // =====================================================================
                if (localX >= 0 && localX < mask.cols && localY >= 0 && localY < mask.rows) {
                    if (mask.at<uchar>(localY, localX) == 0) {

                        double minDist = 9999999.0;
                        int safeX = localX, safeY = localY;

                        for (int y = 0; y < mask.rows; y += 2) {
                            for (int x = 0; x < mask.cols; x += 2) {
                                if (mask.at<uchar>(y, x) > 0) {
                                    double dist = std::pow(x - localX, 2) + std::pow(y - localY, 2);
                                    if (dist < minDist) {
                                        minDist = dist;
                                        safeX = x;
                                        safeY = y;
                                    }
                                }
                            }
                        }
                        localX = safeX;
                        localY = safeY;
                    }
                }
                // =====================================================================

                int realX = localX + sideMargin;
                int realY = localY + topMargin;

                if (anchorX != -1 && anchorY != -1) {

                    double dist = std::sqrt(std::pow(realX - anchorX, 2) + std::pow(realY - anchorY, 2));
                    if (dist < bestMetric) {
                        bestMetric = dist;
                        bestCenter = cv::Point(realX, realY);
                    }
                }
                else {

                    if (area > bestMetric) {
                        bestMetric = area;
                        bestCenter = cv::Point(realX, realY);
                    }
                }
            }
        }
    }

    if (bestCenter.x != -1) {
        result.found = true;
        result.score = bestMetric;
        result.x = bestCenter.x;
        result.y = bestCenter.y;
    }

    return result;
}

// ==============================================================================
// EMPTY FIELD DETECTOR (MAGENTA - HSV)
// ==============================================================================
std::vector<MatchResult> FindEmptyFieldsByColor(const cv::Mat& screen) {
    std::vector<MatchResult> results;
    if (screen.empty()) return results;

    int topMargin = 70;
    int bottomMargin = 40;
    int sideMargin = 30;

    if (screen.rows <= topMargin + bottomMargin || screen.cols <= sideMargin * 2) return results;

    cv::Rect roiRect(sideMargin, topMargin, screen.cols - (sideMargin * 2), screen.rows - (topMargin + bottomMargin));
    cv::Mat searchArea = screen(roiRect);

    cv::Mat hsv, mask;
    cv::cvtColor(searchArea, hsv, cv::COLOR_BGR2HSV);

    // FIELD (Magenta)
    cv::Scalar lowerBound(145, 200, 200);
    cv::Scalar upperBound(155, 255, 255);

    cv::inRange(hsv, lowerBound, upperBound, mask);

	// Clean little pixels to avoid false detections and fill spaces in bigger areas.
    cv::Mat kernelOpen = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernelOpen);

    cv::Mat kernelClose = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 7));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernelClose);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);

        // ignore trash, fields are big
        if (area > 50) {
            cv::Moments M = cv::moments(contour);
            if (M.m00 > 0) {
                int cx = static_cast<int>(M.m10 / M.m00);
                int cy = static_cast<int>(M.m01 / M.m00);

                // DONUT HOLE PROTECTION
                if (cx >= 0 && cx < mask.cols && cy >= 0 && cy < mask.rows) {
                    if (mask.at<uchar>(cy, cx) == 0) {
                        double minDist = 9999999.0;
                        int safeX = cx, safeY = cy;

                        for (int y = 0; y < mask.rows; y += 2) {
                            for (int x = 0; x < mask.cols; x += 2) {
                                if (mask.at<uchar>(y, x) > 0) {
                                    double dist = std::pow(x - cx, 2) + std::pow(y - cy, 2);
                                    if (dist < minDist) {
                                        minDist = dist;
                                        safeX = x;
                                        safeY = y;
                                    }
                                }
                            }
                        }
                        cx = safeX;
                        cy = safeY;
                    }
                }

                MatchResult res;
                res.found = true;
                res.x = cx + sideMargin;
                res.y = cy + topMargin;
                res.score = area;
                results.push_back(res);
            }
        }
    }
    return results;
}
// sends a report of what changed in the barn items count.
void SendRotationReport(int instanceId) {
    if (!g_EnableBarnWebhook) return;
    BotInstance& bot = g_Bots[instanceId];

    std::string msg = "🔄 **" + std::string(Tr("ROTATION COMPLETE | INSTANCE ")) + std::to_string(instanceId + 1) + "** 🔄\n";
    msg += "--------------------------------------\n";

    InventoryData totalCurrent;
    InventoryData totalPrev;

    for (int i = 0; i < MAX_ACCOUNT_SLOTS; i++) {
        if (bot.accounts[i].hasFile) {
            totalCurrent.bolt += bot.accounts[i].currentInv.bolt;
            totalPrev.bolt += bot.accounts[i].previousInv.bolt;
            totalCurrent.tape += bot.accounts[i].currentInv.tape;
            totalPrev.tape += bot.accounts[i].previousInv.tape;
            totalCurrent.plank += bot.accounts[i].currentInv.plank;
            totalPrev.plank += bot.accounts[i].previousInv.plank;
            totalCurrent.nail += bot.accounts[i].currentInv.nail;
            totalPrev.nail += bot.accounts[i].previousInv.nail;
            totalCurrent.screw += bot.accounts[i].currentInv.screw;
            totalPrev.screw += bot.accounts[i].previousInv.screw;
            totalCurrent.panel += bot.accounts[i].currentInv.panel;
            totalPrev.panel += bot.accounts[i].previousInv.panel;
            totalCurrent.deed += bot.accounts[i].currentInv.deed;
            totalPrev.deed += bot.accounts[i].previousInv.deed;
            totalCurrent.mallet += bot.accounts[i].currentInv.mallet;
            totalPrev.mallet += bot.accounts[i].previousInv.mallet;
            totalCurrent.marker += bot.accounts[i].currentInv.marker;
            totalPrev.marker += bot.accounts[i].previousInv.marker;
            totalCurrent.map += bot.accounts[i].currentInv.map;
            totalPrev.map += bot.accounts[i].previousInv.map;
            totalCurrent.dynamite += bot.accounts[i].currentInv.dynamite;
            totalPrev.dynamite += bot.accounts[i].previousInv.dynamite;
            totalCurrent.tnt += bot.accounts[i].currentInv.tnt;
            totalPrev.tnt += bot.accounts[i].previousInv.tnt;
            totalCurrent.axe += bot.accounts[i].currentInv.axe;
            totalPrev.axe += bot.accounts[i].previousInv.axe;
            totalCurrent.saw += bot.accounts[i].currentInv.saw;
            totalPrev.saw += bot.accounts[i].previousInv.saw;
            totalCurrent.shovel += bot.accounts[i].currentInv.shovel;
            totalPrev.shovel += bot.accounts[i].previousInv.shovel;

            bot.accounts[i].previousInv = bot.accounts[i].currentInv;
        }
    }

    auto formatDelta = [](int current, int prev) -> std::string {
        int diff = current - prev;
        if (diff > 0) return " (+**" + std::to_string(diff) + "**)";
        if (diff < 0) return " (" + std::to_string(diff) + ")";
        return " (+0)";
        };

    msg += "💰 **" + std::string(Tr("BARN INVENTORY TOTALS")) + "**\n";
    msg += "<:Bolt:304466477527072768> Bolt: **" + std::to_string(totalCurrent.bolt) + "**" + formatDelta(totalCurrent.bolt, totalPrev.bolt) + " | ";
    msg += "<:Plank:304466477409370113> Plank: **" + std::to_string(totalCurrent.plank) + "**" + formatDelta(totalCurrent.plank, totalPrev.plank) + " | ";
    msg += "<:DuctTape:304466477564690432> Tape: **" + std::to_string(totalCurrent.tape) + "**" + formatDelta(totalCurrent.tape, totalPrev.tape) + "\n";
    msg += "<:Nail:304466477489324042> Nail: **" + std::to_string(totalCurrent.nail) + "**" + formatDelta(totalCurrent.nail, totalPrev.nail) + " | ";
    msg += "<:Screw:304466477438992394> Screw: **" + std::to_string(totalCurrent.screw) + "**" + formatDelta(totalCurrent.screw, totalPrev.screw) + " | ";
    msg += "<:WoodPanel:304466476977356802> Wood Panel: **" + std::to_string(totalCurrent.panel) + "**" + formatDelta(totalCurrent.panel, totalPrev.panel) + "\n";
    msg += "Deed: **" + std::to_string(totalCurrent.deed) + "**" + formatDelta(totalCurrent.deed, totalPrev.deed) + " | ";
    msg += "Mallet: **" + std::to_string(totalCurrent.mallet) + "**" + formatDelta(totalCurrent.mallet, totalPrev.mallet) + " | ";
    msg += "Marker: **" + std::to_string(totalCurrent.marker) + "**" + formatDelta(totalCurrent.marker, totalPrev.marker) + " | ";
    msg += "Map: **" + std::to_string(totalCurrent.map) + "**" + formatDelta(totalCurrent.map, totalPrev.map) + "\n";
    msg += "Dynamite: **" + std::to_string(totalCurrent.dynamite) + "**" + formatDelta(totalCurrent.dynamite, totalPrev.dynamite) + " | ";
    msg += "TNT: **" + std::to_string(totalCurrent.tnt) + "**" + formatDelta(totalCurrent.tnt, totalPrev.tnt) + " | ";
    msg += "Axe: **" + std::to_string(totalCurrent.axe) + "**" + formatDelta(totalCurrent.axe, totalPrev.axe) + " | ";
    msg += "Saw: **" + std::to_string(totalCurrent.saw) + "**" + formatDelta(totalCurrent.saw, totalPrev.saw) + " | ";
    msg += "Shovel: **" + std::to_string(totalCurrent.shovel) + "**" + formatDelta(totalCurrent.shovel, totalPrev.shovel) + "\n";

    std::thread([=]() { Discord::SendWebhookMessage(msg); }).detach();
    AddLog(instanceId, Tr("Rotation Delta Report Sent!"), ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
}

// sales cycle function.
void RunSalesCycle(int instanceId, int accountIndex, bool isEmergency) {
    BotInstance& bot = g_Bots[instanceId];
    AccountSlot& account = bot.accounts[accountIndex];
    AddLog(instanceId, Tr("Entering Sales Mode..."), ImVec4(0, 1, 1, 1));
    bool useSiloFullTimedSales = (!isEmergency && g_OnlySellIfSiloFull);
    std::string salesOpenDetail = "Opening shop for standard sales";
    if (isEmergency) salesOpenDetail = "Opening shop for emergency recovery";
    else if (useSiloFullTimedSales) salesOpenDetail = "Opening shop because silo full triggered the normal sale mode";
    SetSalesFlowState(instanceId, account, SalesFlowState::OpeningShop, salesOpenDetail, true);
    account.salesCycleCount++;

    int emergencyWheatStackLimit = (std::max)(0, (std::min)(20, account.emergencyWheatStackLimit));
    bool useEmergencyWheatSellLimit = isEmergency && bot.testCropMode == 0 && emergencyWheatStackLimit > 0;
    if (useEmergencyWheatSellLimit) {
        int remainingEmergencyWheatStacks = (std::max)(0, emergencyWheatStackLimit - account.emergencyWheatStacksSoldThisTurn);
        AddLog(instanceId, "Emergency Wheat Stack Limit active: this account can sell " + std::to_string(remainingEmergencyWheatStacks) + " more wheat stack(s) during silo-full recovery.", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
    }

    auto SmartSleep = [&](int ms) {
        int elapsed = 0;
        while (elapsed < ms) {
            if (!bot.isRunning) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            elapsed += 50;
        }
        return true;
        };

    const int shopSearchWaitMs = (std::max)(250, (std::min)(700, g_Intervals.shopEnterWait / 3));
    const int shopRetryWaitMs = (std::max)(350, (std::min)(900, g_Intervals.shopEnterWait / 2));

    MatchResult shopRes{};
    bool shopOpened = false;

    for (int tryCount = 1; tryCount <= 3; tryCount++) {
        if (!bot.isRunning) return;

        bool shopFound = false;
        bool alreadyInMarket = false;
        for (int k = 1; k <= 5; k++) {
            if (!bot.isRunning) return;
            cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            alreadyInMarket = IsMarketViewLikelyVisible(screen, bot.testCropMode);
            if (alreadyInMarket) {
                shopFound = true;
                shopOpened = true;
                break;
            }
            shopRes = FindShopIconPreferCache(screen, account);

            if (shopRes.found) {
                shopFound = true;
                break;
            }
            AddLog(instanceId, std::string(Tr("Searching Shop... ")) + std::to_string(k) + "/5", ImVec4(1, 1, 0, 1));
            if (!SmartSleep(shopSearchWaitMs)) return;
        }

        if (!shopFound) {
            AddLog(instanceId, std::string(Tr("Shop NOT found. Retrying... (")) + std::to_string(tryCount) + "/3)", ImVec4(1, 0.5f, 0, 1));
            if (!SmartSleep(shopRetryWaitMs)) return;
            continue;
        }

        if (shopOpened) {
            AddLog(instanceId, "Roadside shop already appears to be open. Continuing with crate scan.", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
            break;
        }

        AdbTap(instanceId, shopRes.x, shopRes.y);
        if (!SmartSleep(g_Intervals.shopEnterWait)) return;

        cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (IsMarketViewLikelyVisible(verifyScreen, bot.testCropMode)) {
            shopOpened = true;
            break;
        }
        if (!FindImage(verifyScreen, shop_templatePath, g_Thresholds.shopThreshold, false, 1.0f, false).found) {
            AddLog(instanceId, "Shop icon disappeared after tap. Assuming the roadside shop opened and continuing.", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
            shopOpened = true;
            break;
        }
        else {
            AddLog(instanceId, std::string(Tr("Shop click missed (Bug)! Retrying... (")) + std::to_string(tryCount) + "/3)", ImVec4(1, 0.5f, 0, 1));
        }
    }

    if (!shopOpened) {
        SetSalesFlowState(instanceId, account, SalesFlowState::Failed, "Failed to open the shop menu");
        AddLog(instanceId, Tr("Failed to open shop menu. Skipping sales."), ImVec4(1, 0.2f, 0.2f, 1));
        return;
    }

    bool doWebhook = (g_EnableBarnWebhook);
    bool webhookDoneThisCycle = false;

    int salesCount = 0;

    // Checks if it was in quarantine (shop full)
    bool wasInQuarantine = bot.accounts[accountIndex].isShopFullStuck;
    bool adPlacedThisCycle = false;

    while (bot.isRunning && salesCount < 10) {
        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);

        // =========================================================
		// 1. COLLECT SOLD CRATES (IF ANY)
        // =========================================================
        std::vector<MatchResult> soldCrates = FindAllImages(screen, soldcrate_templatePath, 0.80f);
        if (!soldCrates.empty()) {
            SetSalesFlowState(instanceId, account, SalesFlowState::CollectingSoldCrates, "Collecting sold crates before listing anything else");

            account.isShopFullStuck = false; // LIFT QUARANTINE.

            // COLLECT THE COINS AND GO BACK TO THE FIELDS          
            if (wasInQuarantine) {
                AddLog(instanceId, "Quarantine lifted via sold crates! Skipping coin collect to save time. Returning to farm.", ImVec4(0, 1, 0, 1));
                break; 
            }

          
            AddLog(instanceId, Tr("Collecting coins..."), ImVec4(0, 1, 0, 1));
            for (auto& crate : soldCrates) {
                if (!bot.isRunning) return;
                AdbTap(instanceId, crate.x, crate.y);
                if (!SmartSleep(300)) return;
            }
            if (!SmartSleep(g_Intervals.coinCollectWait)) return;
            continue;
        }

        if (useEmergencyWheatSellLimit) {
            SetSalesFlowState(instanceId, account, SalesFlowState::CheckingEmergencyLimit, "Checking remaining emergency wheat stack allowance");
            int remainingEmergencyWheatStacks = (std::max)(0, emergencyWheatStackLimit - account.emergencyWheatStacksSoldThisTurn);
            if (remainingEmergencyWheatStacks <= 0) {
                SetSalesFlowState(instanceId, account, SalesFlowState::Completed, "Emergency wheat reserve reached, leaving the shop");
                AddLog(instanceId, "Emergency Wheat Stack Limit reached for this account turn. Leaving the shop with wheat reserved.", ImVec4(1.0f, 1.0f, 0.2f, 1.0f));
                break;
            }
        }

        // =========================================================
		// 2. FIND EMPTY CRATES. IF NONE, CHECK FOR ADVERTISEMENT POSSIBILITY.
        // =========================================================
        SetSalesFlowState(instanceId, account, SalesFlowState::ScanningCrates, "Scanning the market for an empty crate");
        MatchResult emptyCrate = FindEmptyCratePreferCache(screen, account);
        if (!emptyCrate.found) {
            if (IsSalePanelVisible(screen)) {
                SetSalesFlowState(instanceId, account, SalesFlowState::HandlingOccupiedCrates, "Sale panel is still open, closing it before rescanning");
                AddLog(instanceId, "Already in sale panel. Closing one panel and rescanning crates.", ImVec4(1, 1, 0, 1));
                if (!EnsureSalePanelClosed(instanceId, bot.testCropMode, "crate rescan")) return;
                if (!SmartSleep(250)) return;
                continue;
            }
            AddLog(instanceId, Tr("Shop is full."), ImVec4(1, 0.8f, 0, 1));
            if (g_TransferRequest.isPending && g_TransferRequest.senderInstanceId == instanceId) {
                AddLog(instanceId, "HEIST ABORTED: Shop is completely full!", ImVec4(1, 0, 0, 1));
                g_TransferRequest.isPending = false;
            }

			// IF WE CAN PLACE AN ADVERTISEMENT, DO IT. OTHERWISE, MARK ACCOUNT AS QUARANTINED (SHOP FULL STUCK)
            if (adPlacedThisCycle) {
                AddLog(instanceId, "Ad already placed this cycle. Closing shop normally.", ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
				break; // PRESS CROSS TO EXIT SHOP
            }

            // ---SHOP FULL PROTOCOL
            MatchResult filledCrate = FindAnyFilledCrate(screen, bot.testCropMode);
            bool occupiedCrateOpened = false;
            if (filledCrate.found) {
                SetSalesFlowState(instanceId, account, SalesFlowState::HandlingOccupiedCrates, "All crates are occupied, opening one to try the ad flow");
                AddLog(instanceId, "All crates are occupied. Opening a non-empty crate to check advertisement flow...", ImVec4(1, 1, 0, 1));
                AdbTap(instanceId, filledCrate.x, filledCrate.y);
                if (!SmartSleep(1200)) return;
                occupiedCrateOpened = true;
            }
            else if (IsMarketViewLikelyVisible(screen, bot.testCropMode)) {
                SetSalesFlowState(instanceId, account, SalesFlowState::HandlingOccupiedCrates, "All crates appear occupied, probing fallback crate slots");
                AddLog(instanceId, "Filled-crate template missed, but the roadside shop is open with no empty crates. Probing fallback occupied-crate slots...", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
                occupiedCrateOpened = TryOpenOccupiedCrateFallbackGrid(instanceId, bot.testCropMode);
            }

            if (occupiedCrateOpened) {
                cv::Mat editScreen;
                MatchResult advNowRes{ false, 0, 0, 0.0 };
                MatchResult createAdRes{ false, 0, 0, 0.0 };
                bool usedAdvertiseCoordinateFallback = false;
                bool usedCreateAdCoordinateFallback = false;

                editScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                advNowRes = FindBestDialogTemplate(editScreen, { occupied_crate_advertise_templatePath, advertise_templatePath }, g_Thresholds.advertiseThreshold);

                if (!advNowRes.found) {
                    if (!IsOccupiedCrateEditPanelVisible(editScreen)) {
                        AddLog(instanceId, "Occupied crate panel did not open cleanly. Skipping this sales pass.", ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
                        if (!EnsureMarketClosed(instanceId, bot.testCropMode, "occupied crate panel missing")) return;
                        break;
                    }

                    SetSalesFlowState(instanceId, account, SalesFlowState::OpeningAdFlow, "Advertise template missed, using occupied-crate ad fallback coordinate");
                    AddLog(instanceId, "Advertise template missed. Trying occupied-crate ad fallback coordinate...", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
                    AdbTap(instanceId, g_CoordinateProfile.occupiedCrateAdOpenX, g_CoordinateProfile.occupiedCrateAdOpenY);
                    usedAdvertiseCoordinateFallback = true;
                    if (!SmartSleep(900)) return;
                    editScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                }
                else {
                    SetSalesFlowState(instanceId, account, SalesFlowState::OpeningAdFlow, "Advertise button found on the opened crate");
                    AddLog(instanceId, "Advertise button found. Opening the advertisement dialog...", ImVec4(0.9f, 0.9f, 0.3f, 1.0f));
                    AdbTap(instanceId, advNowRes.x, advNowRes.y);
                    if (!SmartSleep(900)) return;
                    editScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                }

                createAdRes = FindBestDialogTemplate(editScreen, { occupied_crate_create_ad_templatePath, create_ad_templatePath }, g_Thresholds.createAdThreshold);
                if (!createAdRes.found) {
                    SetSalesFlowState(instanceId, account, SalesFlowState::PublishingAd, "Create Ad template missed, using fallback confirm coordinate");
                    AddLog(instanceId, "Create Ad template missed. Trying direct confirm coordinate...", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
                    AdbTap(instanceId, g_CoordinateProfile.createAdConfirmX, g_CoordinateProfile.createAdConfirmY);
                    usedCreateAdCoordinateFallback = true;
                    if (!SmartSleep(1000)) return;
                    editScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                }
                else {
                    SetSalesFlowState(instanceId, account, SalesFlowState::PublishingAd, "Confirming the advertisement on the occupied crate");
                    AdbTap(instanceId, createAdRes.x, createAdRes.y);
                    if (!SmartSleep(1000)) return;
                    editScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                }

                if (!IsOccupiedCrateEditPanelVisible(editScreen)) {
                    adPlacedThisCycle = true;
                    AddLog(instanceId,
                        usedAdvertiseCoordinateFallback || usedCreateAdCoordinateFallback
                            ? "Advertisement published via direct-coordinate fallback!"
                            : "Advertisement published!",
                        ImVec4(0, 1, 0, 1));
                    cv::Mat crossScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                    MatchResult crossRes = FindImage(crossScreen, cross_templatePath, g_Thresholds.crossThreshold);
                    if (crossRes.found) AdbTap(instanceId, crossRes.x, crossRes.y);

                    bot.accounts[accountIndex].isShopFullStuck = false;
                    bot.accounts[accountIndex].lastAdTime = std::chrono::steady_clock::now();

                    SetSalesFlowState(instanceId, account, SalesFlowState::WaitingForAdSale, "Waiting for the advertisement to free up a crate");
                    AddLog(instanceId, "Waiting 60 seconds for crops to be sold...", ImVec4(1, 0.5f, 0, 1));
                    for (int w = 0; w < 60; w++) {
                        if (!bot.isRunning) return;
                        bot.statusText = "Waiting Sales: " + std::to_string(60 - w) + "s";
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                    continue;
                }

                AddLog(instanceId, "Create Ad confirm is not available even after fallback. Skipping this sales pass.", ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
                if (!EnsureSalePanelClosed(instanceId, bot.testCropMode, "create-ad unavailable")) return;
                break;
            }
            AddLog(instanceId, "Shop is full, but no occupied crate could be opened. Skipping sales for this pass.", ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
            break;
        }
        else {
            SetSalesFlowState(instanceId, account, SalesFlowState::SelectingEmptyCrate, "Empty crate found, opening the sale panel");
			// IF THERES EMPTY CRATE THAT MEANS NO LONGER NEED TO BE IN QUARANTINE, UNMARK IT.
            bot.accounts[accountIndex].isShopFullStuck = false;


            if (wasInQuarantine) {
                AddLog(instanceId, "Quarantine lifted via empty crate! Returning to farm.", ImVec4(0, 1, 0, 1));
                break;
            }
        }

		// CLICK SELLING MENU BY CLICKING THE EMPTY CRATE
        AdbTap(instanceId, emptyCrate.x, emptyCrate.y);
        if (!SmartSleep(g_Intervals.crateClickWait)) return;

        // =========================================================
        // 3. WEBHOOK & ITEM COUNT ROUTINE.
        // =========================================================
        if (!isEmergency && doWebhook && !webhookDoneThisCycle) {
            SetSalesFlowState(instanceId, account, SalesFlowState::RunningWebhook, "Running the webhook and transfer scan");
            AddLog(instanceId, Tr("Executing Webhook Routine (Checking for Transfer)..."), ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
            cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult barnRes = FindImage(screen, barn_market_templatePath, 0.80f);

            if (barnRes.found) {
                AdbTap(instanceId, barnRes.x, barnRes.y);
 
                if (!SmartSleep(2000)) return;

                cv::Mat fullScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                if (!fullScreen.empty()) {


                    std::string webhookImgPath = "C:\\Users\\Public\\webhook_capture_inst" + std::to_string(instanceId) + ".png";
                    cv::imwrite(webhookImgPath, fullScreen);

                    auto GetRealCount = [&](const std::string& tplPath) -> std::string {
                        // cropped yerine direkt fullScreen'den aratıyoruz
                        int count = ReadItemCountByAnchor(fullScreen, tplPath);
                        return std::to_string(count);
                        };

                    std::string boltCount = GetRealCount("templates\\bolt.png");
                    std::string tapeCount = GetRealCount("templates\\duct_tape.png");
                    std::string plankCount = GetRealCount("templates\\plank.png");
                    std::string nailCount = GetRealCount("templates\\nail.png");
                    std::string screwCount = GetRealCount("templates\\screw.png");
                    std::string panelCount = GetRealCount("templates\\wood_panel.png");
                    std::string dynamiteCount = GetRealCount("templates\\dynamite.png");
                    std::string tntCount = GetRealCount("templates\\TNT.png");
                    std::string axeCount = GetRealCount("templates\\axe.png");
                    std::string sawCount = GetRealCount("templates\\saw.png");
                    std::string shovelCount = GetRealCount("templates\\shovel.png");

                    InventoryData& inv = bot.accounts[accountIndex].currentInv;
                    try {
                        inv.bolt = std::stoi(boltCount); inv.tape = std::stoi(tapeCount);
                        inv.plank = std::stoi(plankCount); inv.nail = std::stoi(nailCount);
                        inv.screw = std::stoi(screwCount); inv.panel = std::stoi(panelCount);
                        inv.dynamite = std::stoi(dynamiteCount); inv.tnt = std::stoi(tntCount);
                        inv.axe = std::stoi(axeCount); inv.saw = std::stoi(sawCount); inv.shovel = std::stoi(shovelCount);
                        SaveInventoryData();
                    }
                    catch (...) {}


                    int cycleCount = bot.accounts[accountIndex].salesCycleCount;
                    int currentCoins = bot.accounts[accountIndex].coinAmount;


                    std::thread([instanceId, accountIndex, cycleCount, currentCoins, boltCount, plankCount, tapeCount, nailCount, screwCount, panelCount, dynamiteCount, tntCount, axeCount, sawCount, shovelCount, webhookImgPath]() {


                        std::string msg = "📊 **[NXRTH] INSTANCE " + std::to_string(instanceId + 1) + " | SLOT " + std::to_string(accountIndex + 1) + "**\n";
                        msg += "🔄 Sales Cycle: **" + std::to_string(cycleCount) + "**\n";
                        msg += "<:Coin:305966854608912386> Total Coins: **" + std::to_string(currentCoins) + "**\n";
                        msg += "--------------------------------------\n";
                        msg += "💰 **BARN INVENTORY**\n";
                        msg += "<:Bolt:304466477527072768> Bolt: **" + boltCount + "** | ";
                        msg += "<:Plank:304466477409370113> Plank: **" + plankCount + "** | ";
                        msg += "<:DuctTape:304466477564690432> Tape: **" + tapeCount + "**\n";
                        msg += "<:Nail:304466477489324042> Nail: **" + nailCount + "** | ";
                        msg += "<:Screw:304466477438992394> Screw: **" + screwCount + "** | ";
                        msg += "<:WoodPanel:304466476977356802> Wood Panel: **" + panelCount + "**\n";
                        msg += "Dynamite: **" + dynamiteCount + "** | TNT: **" + tntCount + "** | Axe: **" + axeCount + "**\n";
                        msg += "Saw: **" + sawCount + "** | Shovel: **" + shovelCount + "**\n";

                        if (g_EnableWebhookImage) Discord::SendWebhookImage(webhookImgPath, msg);
                        else Discord::SendWebhookMessage(msg);
                        }).detach();
                }

                screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                MatchResult siloRes = FindImage(screen, silo_market_templatePath, 0.80f);
                if (siloRes.found) { AdbTap(instanceId, siloRes.x, siloRes.y); SmartSleep(1000); }
            }
            webhookDoneThisCycle = true;


            if (g_Bots[5].isRunning && instanceId != 5) {
                InventoryData& chk = bot.accounts[accountIndex].currentInv;
                if (chk.bolt >= g_TransferThreshold || chk.tape >= g_TransferThreshold || chk.plank >= g_TransferThreshold || chk.nail >= g_TransferThreshold || chk.screw >= g_TransferThreshold || chk.panel >= g_TransferThreshold) {
                    AdbTap(instanceId, barnRes.x, barnRes.y);
                    SmartSleep(500);
                }
            }
        }


        if (!isEmergency && instanceId != 5 && !g_TransferRequest.isPending && g_Bots[5].isRunning) {
            InventoryData& inv = bot.accounts[accountIndex].currentInv;
            std::string triggeredItem = "";
            int triggeredAmount = 0;

            if (inv.bolt >= g_TransferThreshold) { triggeredItem = "bolt"; triggeredAmount = inv.bolt; }
            else if (inv.tape >= g_TransferThreshold) { triggeredItem = "duct_tape"; triggeredAmount = inv.tape; }
            else if (inv.plank >= g_TransferThreshold) { triggeredItem = "plank"; triggeredAmount = inv.plank; }
            else if (inv.nail >= g_TransferThreshold) { triggeredItem = "nail"; triggeredAmount = inv.nail; }
            else if (inv.screw >= g_TransferThreshold) { triggeredItem = "screw"; triggeredAmount = inv.screw; }
            else if (inv.panel >= g_TransferThreshold) { triggeredItem = "wood_panel"; triggeredAmount = inv.panel; }

            if (triggeredItem != "") {
                g_TransferRequest.isPending = true;
                g_TransferRequest.senderInstanceId = instanceId;
                g_TransferRequest.senderAccountId = accountIndex;
                g_TransferRequest.sellerFarmName = bot.accounts[accountIndex].farmName;
                g_TransferRequest.sellerTag = bot.accounts[accountIndex].playerTag;
                g_TransferRequest.itemType = triggeredItem;
                g_TransferRequest.itemAmount = triggeredAmount;
                g_TransferRequest.needFriendship = !bot.accounts[accountIndex].isFriendWithStorage;

                AddLog(instanceId, "RADIO: Transfer triggered! (" + std::to_string(triggeredAmount) + "x " + triggeredItem + " remaining)", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
            }
        }


        if (g_TransferRequest.isPending && g_TransferRequest.senderInstanceId == instanceId) {
            SetSalesFlowState(instanceId, account, SalesFlowState::WaitingForStorage, "Pausing sales while the storage transfer handshake completes");
            // --- 1. AŞAMA: ARKADAŞLIK EL SIKIŞMASI ---
            if (g_TransferRequest.needFriendship) {
                AddLog(instanceId, "HEIST: Not friends with storage. Initiating handshake...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));

                if (!SendFriendRequestToStorage(instanceId, g_StorageTag)) {
                    AddLog(instanceId, "HEIST FAILED: Could not send request.", ImVec4(1, 0, 0, 1));
                    g_TransferRequest.isPending = false; g_TransferRequest.needFriendship = false;
                    break;
                }

                g_TransferRequest.friendRequestSent = true;
                bot.statusText = "WAITING FRIEND ACCEPT";

                int waitAccept = 0;
                while (g_TransferRequest.needFriendship && waitAccept < 600) {
                    if (!bot.isRunning) return;
                    if (!g_TransferRequest.isPending) {
                        AddLog(instanceId, "HEIST ABORTED: Storage bot cancelled the operation!", ImVec4(1, 0, 0, 1));
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    waitAccept++;
                }

                if (!g_TransferRequest.needFriendship) {
                    AddLog(instanceId, "HEIST: Friendship accepted! Closing menus and re-entering shop...", ImVec4(0, 1, 0, 1));
                    bot.accounts[accountIndex].isFriendWithStorage = true;
                    SaveConfig();

                    // 1. Close all menus
                    ForceCloseAllMenus(instanceId);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));


                    AdbTap(instanceId, 400, 50);
                    std::this_thread::sleep_for(std::chrono::milliseconds(2500));


                    bool shopOpened = false;
                    for (int k = 0; k < 4; k++) {
                        cv::Mat scr = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);

                        MatchResult shopRe = FindImage(scr, shop_templatePath, 0.75f);

                        if (shopRe.found) {
                            AddLog(instanceId, "HEIST: Shop found, tapping...", ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                            AdbTap(instanceId, shopRe.x, shopRe.y);
                            std::this_thread::sleep_for(std::chrono::milliseconds(2500));


                            cv::Mat verifyScr = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                            MatchResult verifyCrate = FindImage(verifyScr, crate_templatePath, g_Thresholds.crateThreshold);
                            MatchResult verifyCross = FindImage(verifyScr, cross_templatePath, g_Thresholds.crossThreshold);

                            if (verifyCrate.found || verifyCross.found) {
                                AddLog(instanceId, "HEIST: Shop verified as open!", ImVec4(0, 1, 0, 1));
                                shopOpened = true;
                                break;
                            }
                            else {
                                AddLog(instanceId, "HEIST: Shop didn't open. False positive or lag. Retrying...", ImVec4(1, 0.5f, 0, 1));
                            }
                        }
                        else {
                            AddLog(instanceId, "HEIST: Shop not visible yet, waiting...", ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        }
                    }

                    if (!shopOpened) {
                        AddLog(instanceId, "HEIST ERROR: Could not verify shop is open after multiple tries!", ImVec4(1, 0, 0, 1));
                        g_TransferRequest.isPending = false;
                        break;
                    }

                    continue; 
                }
                else {
                    g_TransferRequest.isPending = false; break;
                }
            }

        
            g_TransferRequest.targetSlotX = emptyCrate.x;
            g_TransferRequest.targetSlotY = emptyCrate.y;

            bot.statusText = "WAITING STORAGE BOSS";
            int waitStorage = 0;
            while (!g_TransferRequest.storageReady && waitStorage < 600) {
                if (!bot.isRunning) return;
                if (!g_TransferRequest.isPending) {
                    AddLog(instanceId, "HEIST ABORTED: Storage bot failed to infiltrate!", ImVec4(1, 0, 0, 1));
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                waitStorage++;
            }

            if (g_TransferRequest.storageReady) {
                cv::Mat shopScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                std::string itemTemplate = "templates\\" + g_TransferRequest.itemType + ".png";
                MatchResult targetItemRes = FindImage(shopScreen, itemTemplate, 0.75f, false);

                if (targetItemRes.found) {
                    AdbTap(instanceId, targetItemRes.x, targetItemRes.y); SmartSleep(500);


                    for (int k = 0; k < 10; k++) { AdbTap(instanceId, 467, 173); SmartSleep(100); }

                    AdbTap(instanceId, 400, 240); SmartSleep(300); 

                    shopScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                    MatchResult saleBtn = FindImage(shopScreen, create_sale_templatePath, g_Thresholds.createSaleThreshold);
                    if (saleBtn.found) {
                        AdbTap(instanceId, saleBtn.x, saleBtn.y);
                        AddLog(instanceId, "HEIST: Item Listed! GO GO GO!", ImVec4(1, 0, 0, 1));
                        g_TransferRequest.itemListed = true;


                        int waitTransfer = 0;
                        while (!g_TransferRequest.transferComplete && waitTransfer < 200) {
                            if (!bot.isRunning) return;
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            waitTransfer++;
                        }

                        if (g_TransferRequest.transferComplete) {
                            AddLog(instanceId, "HEIST: Transfer complete. Collecting coins...", ImVec4(0, 1, 0, 1));
                            AdbTap(instanceId, g_TransferRequest.targetSlotX, g_TransferRequest.targetSlotY);
                            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

               
                            InventoryData& inv = bot.accounts[accountIndex].currentInv;
                            if (g_TransferRequest.itemType == "bolt") inv.bolt -= 10;
                            else if (g_TransferRequest.itemType == "duct_tape") inv.tape -= 10;
                            else if (g_TransferRequest.itemType == "plank") inv.plank -= 10;
                            else if (g_TransferRequest.itemType == "nail") inv.nail -= 10;
                            else if (g_TransferRequest.itemType == "screw") inv.screw -= 10;
                            else if (g_TransferRequest.itemType == "wood_panel") inv.panel -= 10;

                        
                            if (inv.bolt >= g_TransferThreshold || inv.tape >= g_TransferThreshold || inv.plank >= g_TransferThreshold ||
                                inv.nail >= g_TransferThreshold || inv.screw >= g_TransferThreshold || inv.panel >= g_TransferThreshold) {
                                g_TransferRequest.hasMoreItems = true;
                                AddLog(instanceId, "HEIST: I have more items! Telling Storage Bot to wait...", ImVec4(1, 1, 0, 1));
                            }
                            else {
                                g_TransferRequest.hasMoreItems = false;
                                AddLog(instanceId, "HEIST: All items transferred. Telling Storage Bot to go home.", ImVec4(0, 1, 0, 1));
                            }
                        }
                        else {
                            AddLog(instanceId, "HEIST ERROR: Storage bot didn't confirm receipt! (Timeout)", ImVec4(1, 0.5f, 0, 1));
                            g_TransferRequest.hasMoreItems = false; 
                        }

                       
                        g_TransferRequest.isPending = false; g_TransferRequest.storageReady = false;
                        g_TransferRequest.itemListed = false; g_TransferRequest.transferComplete = false;

                        salesCount++;
                        continue;
                    }
                }
            }
            g_TransferRequest.isPending = false;
            continue;
        }



        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        SetSalesFlowState(instanceId, account, SalesFlowState::SelectingProduct, "Selecting the crop from the sale panel");
        MatchResult productRes = FindSaleProductPreferCache(screen, bot.testCropMode, account);

        if (!productRes.found) {
            AddLog(instanceId, Tr("No crops to sell."), ImVec4(1, 0.5f, 0, 1));
            if (!EnsureSalePanelClosed(instanceId, bot.testCropMode, "no-product cleanup")) return;
            break; 
        }

        AdbTap(instanceId, productRes.x, productRes.y);
        if (!SmartSleep(g_Intervals.productSelectWait)) return;

        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        SetSalesFlowState(instanceId, account, SalesFlowState::AdjustingQuantity, "Adjusting the sale quantity");

        int quantityPlusX = g_CoordinateProfile.saleQuantityPlusX;
        int quantityPlusY = g_CoordinateProfile.saleQuantityPlusY;
        int quantityMinusX = g_CoordinateProfile.saleQuantityMinusX;
        int quantityMinusY = g_CoordinateProfile.saleQuantityMinusY;
        int quantityTapCount = 0;
        int quantityTapX = quantityPlusX;
        int quantityTapY = quantityPlusY;
        int resetMinusTapCount = 0;
        int saleStackMode = (std::max)(kSaleStackModeGameDefault, (std::min)(kSaleStackModePreserveOcr, account.saleStackMode));
        int fixedSaleStack = (std::max)(1, (std::min)(kMaxSaleStack, account.fixedSaleStack));
        bool isWheatSale = (bot.testCropMode == 0);
        WheatPreservePlan preservePlan = BuildWheatPreservePlan(screen, account, isWheatSale);
        account.preserveReadDetail = preservePlan.active
            ? ("Preserve OCR: count=" + std::to_string(preservePlan.currentCount) + " [" +
                (preservePlan.readMethod.empty() ? "unreadable" : preservePlan.readMethod) + " " +
                std::to_string(preservePlan.ocrScore) + " | digits=" + (preservePlan.digits.empty() ? "-" : preservePlan.digits) + "]")
            : (isWheatSale ? "Preserve OCR inactive for this sale." : "Preserve OCR only applies to wheat.");

        if (saleStackMode == kSaleStackModePreserveOcr) {
            if (preservePlan.active && preservePlan.ocrReadable) {
                if (preservePlan.skipSale) {
                    AddLog(instanceId, "Preserve OCR: current wheat is " + std::to_string(preservePlan.currentCount) + ", reserve is " + std::to_string(preservePlan.reserveCount) + ". Skipping this sale to keep the reserve intact.", ImVec4(0.9f, 0.9f, 0.3f, 1.0f));
                    if (!EnsureSalePanelClosed(instanceId, bot.testCropMode, "preserve reserve reached")) return;
                    break;
                }

                resetMinusTapCount = kMaxSaleStack;
                quantityTapCount = preservePlan.desiredStack - 1;
                AddLog(instanceId,
                    "Sale stack mode [Preserve OCR]: wheat=" + std::to_string(preservePlan.currentCount) +
                    ", reserve=" + std::to_string(preservePlan.reserveCount) +
                    ", panelStart=" + std::to_string(preservePlan.panelStart) +
                    ", target=" + std::to_string(preservePlan.desiredStack) +
                    ", method=" + preservePlan.readMethod +
                    ", digits=\"" + preservePlan.digits + "\" score=" + std::to_string(preservePlan.ocrScore) + ".",
                    ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
            }
            else if (preservePlan.active) {
                quantityTapCount = kMaxSaleStack;
                AddLog(instanceId, "Sale stack mode [Preserve OCR]: wheat count OCR was unreadable, so this listing will ignore the reserve and sell full stacks this pass.", ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
            }
            else if (!isWheatSale) {
                AddLog(instanceId, "Sale stack mode [Preserve OCR]: current crop is not wheat, so the listing will fall back to the game's default quantity.", ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
            }
        }

        MatchResult plusButton = FindImage(screen, plus_templatePath, std::max(0.65f, g_Thresholds.plusThreshold), false, 1.0f, false);
        if (plusButton.found &&
            std::abs(plusButton.x - g_CoordinateProfile.saleQuantityPlusX) <= 90 &&
            std::abs(plusButton.y - g_CoordinateProfile.saleQuantityPlusY) <= 70) {
            quantityPlusX = plusButton.x;
            quantityPlusY = plusButton.y;
            quantityMinusX = (std::max)(0, quantityPlusX - kSaleQuantityControlSpacingX);
            quantityMinusY = quantityPlusY;
            quantityTapX = quantityPlusX;
            quantityTapY = quantityPlusY;
        }
        else if (plusButton.found) {
            AddLog(instanceId,
                "Sale quantity plus template was ignored because it matched away from the configured quantity button. Using Sale Qty Plus coordinate instead.",
                ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        }

        if (useEmergencyWheatSellLimit) {
            int remainingEmergencyWheatStacks = (std::max)(0, emergencyWheatStackLimit - account.emergencyWheatStacksSoldThisTurn);
            quantityTapCount = kMaxSaleStack;
            AddLog(instanceId, "Emergency Wheat Stack Limit: forcing a full wheat listing. Remaining emergency wheat stack slots after this one: " + std::to_string((std::max)(0, remainingEmergencyWheatStacks - 1)) + ".", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
        }
        else if (saleStackMode == kSaleStackModePreserveOcr && preservePlan.active && preservePlan.ocrReadable) {
            // Preserve OCR already computed reset/tap counts above.
        }
        else if (saleStackMode == kSaleStackModeMax) {
            quantityTapCount = kMaxSaleStack;
            AddLog(instanceId, std::string("Sale stack mode [") + GetSaleStackModeName(saleStackMode) + "]: pushing quantity to the game maximum.", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
        }
        else if (saleStackMode == kSaleStackModeFixed) {
            resetMinusTapCount = kMaxSaleStack;
            quantityTapCount = fixedSaleStack - 1;
            AddLog(instanceId, std::string("Sale stack mode [") + GetSaleStackModeName(saleStackMode) + "]: resetting to 1 and targeting stack " + std::to_string(fixedSaleStack) + ".", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
        }

        if (resetMinusTapCount > 0) {
            AddLog(instanceId, "Resetting sale quantity at X " + std::to_string(quantityMinusX) + " / Y " + std::to_string(quantityMinusY) + " (" + std::to_string(resetMinusTapCount) + " tap(s)).", ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
            if (!AdbTapRepeatedSlow(instanceId, quantityMinusX, quantityMinusY, resetMinusTapCount)) return;
            if (!SmartSleep(120)) return;
        }

        if (quantityTapCount > 0) {
            AddLog(instanceId, "Increasing sale quantity at X " + std::to_string(quantityTapX) + " / Y " + std::to_string(quantityTapY) + " (" + std::to_string(quantityTapCount) + " tap(s)).", ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
            if (!AdbTapRepeatedSlow(instanceId, quantityTapX, quantityTapY, quantityTapCount)) return;
            if (!SmartSleep(120)) return;
        }

        SetSalesFlowState(instanceId, account, SalesFlowState::AdjustingPrice, account.sellAtMaxPrice ? "Applying max price" : "Applying configured price adjustments");
        // MAX PRICE BUTTON (DIRECT COORDINATE BY REQUEST)
        if (account.sellAtMaxPrice) {
            AddLog(instanceId, "Applying max price.", ImVec4(0, 1, 1, 1));
            AdbTap(instanceId, g_CoordinateProfile.saleMaxPriceX, g_CoordinateProfile.saleMaxPriceY);
            if (!SmartSleep(120)) return;
        }

        // Randomizer (Anti-Ban PRICE DROPPER) 
        if (!account.sellAtMaxPrice && g_Bots[instanceId].enableShopRandomizer) {
            int randomClicks = rand() % (g_Bots[instanceId].shopRandomizerMax + 1);
            if (randomClicks > 0) {
                AddLog(instanceId, std::string(Tr("Anti-Ban: Decreasing price ")) + std::to_string(randomClicks) + Tr(" times."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
                for (int m = 0; m < randomClicks; m++) {
                    if (!bot.isRunning) return;
                    AdbTap(instanceId, g_CoordinateProfile.salePriceMinusX, g_CoordinateProfile.salePriceMinusY);
                    if (!SmartSleep(100 + (rand() % 150))) return;
                }
            }
        }

        // PLACE AD
        auto now = std::chrono::steady_clock::now();
        auto elapsedMinutes = std::chrono::duration_cast<std::chrono::minutes>(now - account.lastAdTime).count();

        if (elapsedMinutes >= 5) {
            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult adRes = FindImage(screen, advertise_templatePath, g_Thresholds.advertiseThreshold);
            if (adRes.found) {
                AddLog(instanceId, Tr("Placing Advertisement..."), ImVec4(0, 1, 0, 1));
                AdbTap(instanceId, adRes.x, adRes.y);
                adPlacedThisCycle = true; // CONFIRM THAT AD WAS PLACED IN THIS CYCLE.
                if (!SmartSleep(300)) return;
                account.lastAdTime = now;
            }
        }

		SetSalesFlowState(instanceId, account, SalesFlowState::ConfirmingSale, "Confirming the listing");
		// PUT ON SALE BUTTON
        const int saleConfirmProbeWaitMs = (std::max)(220, (std::min)(450, g_Intervals.createSaleWait));
        auto saleListingConfirmed = [&]() -> bool {
            cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            return !IsSalePanelVisible(verifyScreen);
            };
        auto finalizeSaleSuccess = [&]() {
            AddLog(instanceId, Tr("Item Sold."), ImVec4(0, 1, 0, 1));
            salesCount++;
            bot.totalSales++;
            if (useEmergencyWheatSellLimit) {
                account.emergencyWheatStacksSoldThisTurn++;
            }
            };

        bool saleSubmitted = false;
        AdbTap(instanceId, g_CoordinateProfile.putOnSaleX, g_CoordinateProfile.putOnSaleY);
        if (!SmartSleep(saleConfirmProbeWaitMs)) return;
        if (saleListingConfirmed()) {
            saleSubmitted = true;
        }
        else {
            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult saleBtn = FindImage(screen, create_sale_templatePath, g_Thresholds.createSaleThreshold);
            if (saleBtn.found) {
                AddLog(instanceId, "Put on Sale coordinate missed. Using detected sale button...", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
                AdbTap(instanceId, saleBtn.x, saleBtn.y);
                if (!SmartSleep(saleConfirmProbeWaitMs)) return;
                saleSubmitted = saleListingConfirmed();
            }
            else {
				AddLog(instanceId, Tr("Put on Sale button template was not visible after the coordinate tap. Retrying custom x,y coordinates."), ImVec4(1, 0, 0, 1));
				AdbTap(instanceId, g_CoordinateProfile.putOnSaleX, g_CoordinateProfile.putOnSaleY);
                if (!SmartSleep(saleConfirmProbeWaitMs)) return;
                saleSubmitted = saleListingConfirmed();
            }
        }

        if (saleSubmitted) {
            finalizeSaleSuccess();
            if (!SmartSleep((std::max)(160, g_Intervals.createSaleWait / 2))) return;
        }
        else {
            AddLog(instanceId, "Put on Sale did not confirm. Leaving the sale panel logic to the normal close recovery.", ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
        }
    }
	SmartSleep((std::max)(350, (std::min)(700, g_Intervals.createSaleWait)));
    if (!EnsureMarketClosed(instanceId, bot.testCropMode, "sales exit")) {
        AddLog(instanceId, "Warning: market view may still be open after the sales exit retries.", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    }
    SetSalesFlowState(instanceId, account, SalesFlowState::Completed, salesCount > 0 ? "Sales cycle finished and the market was closed" : "Sales cycle finished without new listings");
}

static void RunAccountFarmCycle(int instanceId, int accountIndex) {
    BotInstance& bot = g_Bots[instanceId];
    AccountSlot& account = bot.accounts[accountIndex];
    account.cyclesSinceStart++;
    bool outOfSeeds = false;
    CropRuntimeInfo crop = GetCropRuntimeInfo(bot.testCropMode);

    SetSalesFlowState(instanceId, account, SalesFlowState::Idle, "Waiting for sales cycle", true);

    auto ShouldRunAutoTom = [&]() -> bool {
        if (!account.autoTomEnabled) return false;
        if ((account.tomRemainingHours * 60 + account.tomRemainingMinutes) <= 0) return false;
        if (account.tomItemName.empty()) return false;
        return true;
        };

    auto TryHarvestForCropMode = [&](int cropMode, const char* label) -> int {
        bot.statusText = Tr("Scanning for Grown Crops...");

        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (screen.empty()) return 0;

        std::vector<MatchResult> allGrown = FindGrownCandidatesForCrop(screen, cropMode);

        if (allGrown.empty()) {
            return 0;
        }

        MatchResult sickleRes;
        AddLog(instanceId, std::string(label) + ": grown crops detected. Verifying the harvest anchor before opening the sickle menu...", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        bool foundSickle = TryOpenSickleMenuFromCandidates(instanceId, allGrown, sickleRes, nullptr);

        if (foundSickle) {
            AddLog(instanceId, Tr("Sickle Found! Calculating harvest zone..."), ImVec4(0, 1, 0, 1));
            ExecuteDenseGridGesture(instanceId, sickleRes.x, sickleRes.y, allGrown);
            std::this_thread::sleep_for(std::chrono::milliseconds(g_Intervals.afterHarvestWait));

            bot.totalHarvest++;
            std::this_thread::sleep_for(std::chrono::milliseconds(g_Intervals.afterHarvestWait));

            cv::Mat siloScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult siloFullRes;
            IsSiloFullPopupVisible(siloScreen, &siloFullRes);
            if (siloFullRes.found) {
                AddLog(instanceId, Tr("SILO FULL DETECTED! Harvest interrupted."), ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                bool cleared = EnsureSiloFullPopupClosed(instanceId, "harvest recovery");
                if (cleared) AddLog(instanceId, "SILO FULL popup closed. Preparing emergency sales...", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
                else AddLog(instanceId, "SILO FULL popup is still visible after retrying the cross. Emergency sales will try once more before opening the shop.", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                return 2;
            }

            return 1;
        }

        cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult crossRes = FindBestCrossAny(verifyScreen);
        if (crossRes.found) {
            AdbTap(instanceId, crossRes.x, crossRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } else {
            DismissUnknownOverlay(instanceId, "harvest menu did not open");
        }
        AddLog(instanceId, "Harvest menu did not open from any grown-crop candidate. Skipping this harvest pass.", ImVec4(1.0f, 0.55f, 0.2f, 1.0f));
        return 0;
        };

    auto TryHarvest = [&]() -> int {
        if (account.cowFeedCropRecoveryPlanted) {
            AddLog(instanceId, "Cow Feed: harvesting soybean/corn recovery crop mix before normal planting.", ImVec4(0.3f, 0.9f, 1.0f, 1.0f));
            int soybeanStatus = TryHarvestForCropMode(kCowFeedSoybeanCropMode, "Cow Feed Soybean Recovery");
            if (soybeanStatus == 2) return 2;
            int cornStatus = TryHarvestForCropMode(kCowFeedCornCropMode, "Cow Feed Corn Recovery");
            if (cornStatus == 2) return 2;
            if (soybeanStatus == 1 || cornStatus == 1) {
                account.cowFeedCropRecoveryPlanted = false;
                SaveConfig();
                AddLog(instanceId, "Cow Feed: recovery crops harvested. Returning to the configured crop after this pass.", ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
                return 1;
            }
            return 0;
        }

        return TryHarvestForCropMode(crop.mode, crop.name);
        };

    auto TryAutoTom = [&]() -> bool {
        if (!ShouldRunAutoTom()) return false;

        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto formatTomDelay = [](long long seconds) -> std::string {
            seconds = (std::max)(0LL, seconds);
            long long hours = seconds / 3600;
            long long minutes = (seconds % 3600) / 60;
            if (hours > 0) return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
            return std::to_string((seconds + 59) / 60) + "m";
            };

        if (account.tomStartTime != 0) {
            long long contractSeconds =
                (static_cast<long long>(account.tomRemainingHours) * 3600LL) +
                (static_cast<long long>(account.tomRemainingMinutes) * 60LL);
            long long expireTime = account.tomStartTime + contractSeconds;
            if (now > expireTime) {
                account.autoTomEnabled = false;
                SaveConfig();
                AddLog(instanceId, Tr("Tom contract expired! Auto Tom disabled."), ImVec4(1, 0.2f, 0.2f, 1));
                return false;
            }
        }

        if (now < account.nextTomTime) {
            bot.statusText = std::string(Tr("Auto Tom waits ")) + formatTomDelay(account.nextTomTime - now);
            return false;
        }

        bot.statusText = Tr("Deploying Auto Tom...");
        AddLog(instanceId, Tr("Initiating Auto Tom sequence..."), ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
        AddLog(instanceId, std::string(Tr("Tom target search: ")) + account.tomItemName +
            ((account.tomCategory == 0) ? " (Barn)" : " (Silo)"), ImVec4(0.45f, 0.85f, 1.0f, 1.0f));

        bool menuOpened = false;
        int savedTomX = 0;
        int savedTomY = 0;

        for (int retry = 1; retry <= 3; retry++) {
            if (!bot.isRunning) return false;

            cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);

            MatchResult barnCheck = FindImage(screen, "templates\\tom_barn.png", 0.80f, false);
            MatchResult siloCheck = FindImage(screen, "templates\\tom_silo.png", 0.80f, false);

            if (barnCheck.found || siloCheck.found) {
                menuOpened = true;
                break;
            }

            MatchResult tomRes = FindImage(screen, "templates\\tom_crate.png", 0.75f, false);
            if (tomRes.found) {
                savedTomX = tomRes.x;
                savedTomY = tomRes.y;

                AdbTap(instanceId, tomRes.x, tomRes.y);
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));

                screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                barnCheck = FindImage(screen, "templates\\tom_barn.png", 0.80f, false);
                siloCheck = FindImage(screen, "templates\\tom_silo.png", 0.80f, false);

                if (barnCheck.found || siloCheck.found) {
                    menuOpened = true;
                    break;
                }
            }

            AddLog(instanceId, std::string(Tr("Tom menu not found. Retrying... (")) + std::to_string(retry) + "/3)", ImVec4(1, 0.5f, 0, 1));
            MatchResult crossRes = FindImage(screen, cross_templatePath, g_Thresholds.crossThreshold, false);
            if (crossRes.found) {
                AdbTap(instanceId, crossRes.x, crossRes.y);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }

        if (!menuOpened) {
            AddLog(instanceId, Tr("Tom menu failed to open after 3 retries! Aborting sequence."), ImVec4(1, 0.2f, 0.2f, 1));
            cv::Mat failScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult failCross = FindImage(failScreen, cross_templatePath, g_Thresholds.crossThreshold, false);
            if (failCross.found) {
                AddLog(instanceId, Tr("Closing stuck menu before returning to farm..."), ImVec4(1, 0.5f, 0, 1));
                AdbTap(instanceId, failCross.x, failCross.y);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
            return false;
        }

        std::string catTemplate = (account.tomCategory == 0) ? "templates\\tom_barn.png" : "templates\\tom_silo.png";
        cv::Mat finalScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult catRes = FindImage(finalScreen, catTemplate, 0.80f, false);
        if (catRes.found) {
            AdbTap(instanceId, catRes.x, catRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        int SEARCH_ICON_X = 150;
        int SEARCH_ICON_Y = 120;
        int TEXT_BOX_X = 250;
        int TEXT_BOX_Y = 175;
        int FIRST_ITEM_X = 196;
        int FIRST_ITEM_Y = 234;
        int YES_BUTTON_X = 436;
        int YES_BUTTON_Y = 291;

        AdbTap(instanceId, SEARCH_ICON_X, SEARCH_ICON_Y);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        AddLog(instanceId, Tr("Focusing on search text box..."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
        AdbTap(instanceId, TEXT_BOX_X, TEXT_BOX_Y);
        std::this_thread::sleep_for(std::chrono::milliseconds(800));

        RunAdbCommand(instanceId, "shell input text '" + account.tomItemName + "'");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        AdbTap(instanceId, FIRST_ITEM_X, FIRST_ITEM_Y);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        AdbTap(instanceId, YES_BUTTON_X, YES_BUTTON_Y);

        AddLog(instanceId, Tr("Tom deployed. Waiting 30s for him to return..."), ImVec4(1, 1, 0, 1));
        for (int w = 0; w < 30; w++) {
            if (!bot.isRunning) return false;
            bot.statusText = std::string(Tr("Tom returning... ")) + std::to_string(30 - w) + "s";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (savedTomX != 0 && savedTomY != 0) {
            AddLog(instanceId, Tr("Tom is back! Clicking saved Crate location..."), ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
            bot.statusText = Tr("Opening Tom Boxes...");
            AdbTap(instanceId, savedTomX, savedTomY);
            std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        }
        else {
            AddLog(instanceId, Tr("Saved location missing, searching crate again..."), ImVec4(1, 0.5f, 0, 1));
            cv::Mat retScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult retTom = FindImage(retScreen, "templates\\tom_crate.png", 0.75f, false);
            if (retTom.found) {
                AdbTap(instanceId, retTom.x, retTom.y);
                std::this_thread::sleep_for(std::chrono::milliseconds(2500));
            }
        }

        int BOX_RIGHT_X = 525;
        int BOX_RIGHT_Y = 310;
        int BOX_MID_X = 428;
        int BOX_MID_Y = 313;
        int BOX_LEFT_X = 332;
        int BOX_LEFT_Y = 316;

        AdbTap(instanceId, BOX_RIGHT_X, BOX_RIGHT_Y);
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult crossRes = FindImage(screen, cross_templatePath, g_Thresholds.crossThreshold, false);

        if (crossRes.found) {
            AddLog(instanceId, Tr("Not enough coins for Max Stack! Falling back to Mid..."), ImVec4(1, 0.5f, 0, 1));
            AdbTap(instanceId, crossRes.x, crossRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            AdbTap(instanceId, BOX_MID_X, BOX_MID_Y);
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));

            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult crossRes2 = FindImage(screen, cross_templatePath, g_Thresholds.crossThreshold, false);

            if (crossRes2.found) {
                AddLog(instanceId, Tr("Still poor! Falling back to Min Stack..."), ImVec4(1, 0.5f, 0, 1));
                AdbTap(instanceId, crossRes2.x, crossRes2.y);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));

                AdbTap(instanceId, BOX_LEFT_X, BOX_LEFT_Y);
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }
        }

        AddLog(instanceId, Tr("Purchase confirmed! Waiting 30s for Tom to deliver..."), ImVec4(1, 1, 0, 1));
        for (int w = 0; w < 30; w++) {
            if (!bot.isRunning) return false;
            bot.statusText = std::string(Tr("Tom delivering... ")) + std::to_string(30 - w) + "s";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (savedTomX != 0 && savedTomY != 0) {
            AddLog(instanceId, Tr("Tom delivered! Collecting items..."), ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
            bot.statusText = Tr("Collecting Items...");
            AdbTap(instanceId, savedTomX, savedTomY);
            std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        }
        else {
            AddLog(instanceId, Tr("Saved location missing, searching crate to collect..."), ImVec4(1, 0.5f, 0, 1));
            cv::Mat retScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult retTom = FindImage(retScreen, "templates\\tom_crate.png", 0.75f, false);
            if (retTom.found) {
                AdbTap(instanceId, retTom.x, retTom.y);
                std::this_thread::sleep_for(std::chrono::milliseconds(2500));
            }
        }

        if (account.tomStartTime == 0) {
            account.tomStartTime = now;
        }

        account.nextTomTime = now + 7260;
        SaveConfig();

        AddLog(instanceId, Tr("Auto Tom cycle complete! He will wake up in 2 hours."), ImVec4(0, 1, 0, 1));
        return true;
        };

    auto TryPlant = [&]() -> bool {
        bool atLeastOnePlantDone = false;
        int tempAnchorX = -1;
        int tempAnchorY = -1;

        std::string currentSeedTemplate = crop.seedTemplate;
        float currentSeedThresh = crop.seedThreshold;
        bool useCowFeedRecoveryPlant =
            account.autoCowFeedEnabled &&
            account.cowFeedCropRecoveryRequested &&
            !account.cowFeedCropRecoveryPlanted;

        auto plantTargetsWithCrop = [&](int cropMode, const std::vector<MatchResult>& targets, const char* phaseLabel) -> bool {
            if (targets.empty()) return false;
            CropRuntimeInfo plantCrop = GetCropRuntimeInfo(cropMode);
            tempAnchorX = targets[0].x;
            tempAnchorY = targets[0].y;

            AddLog(instanceId, std::string("Cow Feed: planting ") + plantCrop.name + " for " + phaseLabel + ".", ImVec4(0.2f, 0.9f, 0.6f, 1.0f));
            AdbTap(instanceId, tempAnchorX, tempAnchorY);
            std::this_thread::sleep_for(std::chrono::milliseconds(1200));

            float seedThreshold = (std::max)(0.65f, plantCrop.seedThreshold - 0.08f);
            MatchResult seedRes;
            bool seedFound = false;
            for (int seedTry = 1; seedTry <= 4; ++seedTry) {
                if (!bot.isRunning) return false;
                if (seedTry >= 3) {
                    AddLog(instanceId, "Cow Feed: seed tray not visible yet. Retapping a field to reopen it...", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                    AdbTap(instanceId, tempAnchorX, tempAnchorY);
                    std::this_thread::sleep_for(std::chrono::milliseconds(700));
                }
                cv::Mat seedScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                seedRes = FindImage(seedScreen, plantCrop.seedTemplate, seedThreshold, false);
                if (seedRes.found) {
                    seedFound = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(450 + (seedTry * 100)));
            }

            if (!seedFound) {
                AddLog(instanceId, std::string("Cow Feed: ") + plantCrop.name + " seed template not found for recovery planting.", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
                cv::Mat closeScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                MatchResult crossRes = FindImage(closeScreen, cross_templatePath, g_Thresholds.crossThreshold, false);
                if (crossRes.found) AdbTap(instanceId, crossRes.x, crossRes.y);
                return false;
            }

            ExecuteDenseGridGesture(instanceId, seedRes.x, seedRes.y, targets);
            std::this_thread::sleep_for(std::chrono::milliseconds(g_Intervals.afterPlantWait));

            cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult crossRes = FindImage(verifyScreen, cross_templatePath, g_Thresholds.crossThreshold, false);
            if (crossRes.found) {
                AddLog(instanceId, "Cow Feed: out of seeds while planting recovery crops. Closing popup.", ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                AdbTap(instanceId, crossRes.x, crossRes.y);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                outOfSeeds = true;
                return false;
            }

            return true;
            };

        for (int plantAttempt = 1; plantAttempt <= 3; plantAttempt++) {
            if (!bot.isRunning) return atLeastOnePlantDone;

            cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);

            MatchResult lvlRes = FindImage(screen, levelup_templatePath, g_Thresholds.levelUpThreshold, false);
            if (lvlRes.found) {
                AddLog(instanceId, Tr("Account Leveled Up! Claiming rewards..."), ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                account.level += 1;
                SaveConfig();
                AddLog(instanceId, std::string(Tr("Level updated to: ")) + std::to_string(account.level), ImVec4(0, 1, 0, 1));

                MatchResult contRes = FindImage(screen, levelup_continue_templatePath, g_Thresholds.levelUpThreshold, false);
                if (contRes.found) AdbTap(instanceId, contRes.x, contRes.y);
                else AdbTap(instanceId, screen.cols / 2, screen.rows - 50);

                std::this_thread::sleep_for(std::chrono::milliseconds(2500));
                plantAttempt--;
                continue;
            }

            bot.statusText = Tr("Scanning Fields...");
            bool isRecheck = (plantAttempt > 1);
            std::vector<MatchResult> allFields = FindEmptyFields(screen, isRecheck);

            if (!allFields.empty()) {
                if (useCowFeedRecoveryPlant) {
                    std::vector<MatchResult> soybeanFields;
                    std::vector<MatchResult> cornFields;
                    soybeanFields.reserve(allFields.size());
                    cornFields.reserve(allFields.size() / 3 + 1);
                    for (size_t fieldIndex = 0; fieldIndex < allFields.size(); ++fieldIndex) {
                        if ((fieldIndex % 3) == 2) cornFields.push_back(allFields[fieldIndex]);
                        else soybeanFields.push_back(allFields[fieldIndex]);
                    }
                    if (soybeanFields.empty() && !allFields.empty()) {
                        soybeanFields.push_back(allFields.front());
                    }

                    AddLog(instanceId,
                        "Cow Feed: resource recovery planting active. Planting soybean/corn at roughly 2:1.",
                        ImVec4(0.2f, 0.9f, 1.0f, 1.0f));
                    bool plantedSoybean = plantTargetsWithCrop(kCowFeedSoybeanCropMode, soybeanFields, "feed resource recovery");
                    bool plantedCorn = false;
                    if (!outOfSeeds) {
                        plantedCorn = plantTargetsWithCrop(kCowFeedCornCropMode, cornFields, "feed resource recovery");
                    }

                    if (plantedSoybean || plantedCorn) {
                        account.cowFeedCropRecoveryRequested = false;
                        account.cowFeedCropRecoveryPlanted = true;
                        SaveConfig();
                        AddLog(instanceId, "Cow Feed: recovery crops planted. Next wait uses the soybean timer before harvesting the mix.", ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
                        atLeastOnePlantDone = true;
                    }
                    return atLeastOnePlantDone;
                }

                tempAnchorX = allFields[0].x;
                tempAnchorY = allFields[0].y;

                AddLog(instanceId, std::string(Tr("Neon pink fields detected! Opening seed menu (Swipe ")) + std::to_string(plantAttempt) + "/3)...", ImVec4(1.0f, 0.0f, 0.6f, 1.0f));
                AdbTap(instanceId, tempAnchorX, tempAnchorY);
                std::this_thread::sleep_for(std::chrono::milliseconds(1200));
                float seedThreshold = (std::max)(0.65f, currentSeedThresh - 0.08f);
                MatchResult seedRes;
                bool seedFound = false;
                for (int seedTry = 1; seedTry <= 4; ++seedTry) {
                    if (!bot.isRunning) return atLeastOnePlantDone;
                    if (seedTry >= 3) {
                        AddLog(instanceId, "Seed menu not visible yet. Retapping a field to reopen the seed tray...", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                        AdbTap(instanceId, tempAnchorX, tempAnchorY);
                        std::this_thread::sleep_for(std::chrono::milliseconds(700));
                    }
                    screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                    seedRes = FindImage(screen, currentSeedTemplate, seedThreshold, false);
                    if (seedRes.found) {
                        seedFound = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(450 + (seedTry * 100)));
                }

                if (seedFound) {
                    AddLog(instanceId, Tr("Seed Found. Executing dense grid..."), ImVec4(0, 1, 0, 1));

                    ExecuteDenseGridGesture(instanceId, seedRes.x, seedRes.y, allFields);
                    std::this_thread::sleep_for(std::chrono::milliseconds(g_Intervals.afterPlantWait));

                    cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                    MatchResult crossRes = FindImage(verifyScreen, cross_templatePath, g_Thresholds.crossThreshold, false);

                    if (crossRes.found) {
                        AddLog(instanceId, Tr("OUT OF SEEDS! Diamond pop-up detected. Skipping plant phase."), ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                        AdbTap(instanceId, crossRes.x, crossRes.y);
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        outOfSeeds = true;
                        return false;
                    }

                    atLeastOnePlantDone = true;
                }
                else {
                    AddLog(instanceId, Tr("Seed menu NOT opened or seed missing. Breaking plant loop."), ImVec4(1, 0.5f, 0, 1));
                    MatchResult crossRes = FindImage(screen, cross_templatePath, g_Thresholds.crossThreshold, false);
                    if (crossRes.found) AdbTap(instanceId, crossRes.x, crossRes.y);
                    break;
                }
            }
            else {
                if (plantAttempt > 1) {
                    AddLog(instanceId, Tr("All visible fields are planted successfully."), ImVec4(0, 1, 0, 1));
                }
                break;
            }
        }

        if (atLeastOnePlantDone) {
            account.anchorX = tempAnchorX;
            account.anchorY = tempAnchorY;
            if (account.cowFeedCropRecoveryPlanted) {
                account.cowFeedCropRecoveryPlanted = false;
                SaveConfig();
                AddLog(instanceId, "Cow Feed: cleared stale recovery-crop state after normal planting.", ImVec4(0.8f, 0.85f, 0.3f, 1.0f));
            }
            AddLog(instanceId, Tr("Field position mapped and saved!"), ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
            return true;
        }

        return false;
        };

    FarmFlowState state = account.isShopFullStuck ? FarmFlowState::ClearingQuarantine :
        (ShouldRunAutoTom() ? FarmFlowState::CheckingAutoTom : FarmFlowState::Harvesting);
    int harvestStatus = 0;
    bool plantDone = false;

    while (bot.isRunning) {
        switch (state) {
        case FarmFlowState::ClearingQuarantine:
            SetFarmFlowState(instanceId, account, state, "Checking ad availability on a quarantined account", true);
            bot.statusText = "Checking Ad Availability...";
            AddLog(instanceId, "Account in quarantine. Checking shop immediately to clear status...", ImVec4(1, 0.5f, 0, 1));
            RunSalesCycle(instanceId, accountIndex, true);
            if (account.isShopFullStuck) {
                SetFarmFlowState(instanceId, account, FarmFlowState::WaitingForSiloFull, "Quarantine cooldown active, skipping account");
                AddLog(instanceId, "Still on Ad cooldown. Returning to quarantine...", ImVec4(1, 0.2f, 0.2f, 1));
                return;
            }
            AddLog(instanceId, "Quarantine lifted! Resuming normal farm operations.", ImVec4(0, 1, 0, 1));
            state = ShouldRunAutoTom() ? FarmFlowState::CheckingAutoTom : FarmFlowState::Harvesting;
            break;

        case FarmFlowState::CheckingAutoTom:
            if (!ShouldRunAutoTom()) {
                state = FarmFlowState::Harvesting;
                break;
            }
            SetFarmFlowState(instanceId, account, state, "Running scheduled Auto Tom checks");
            bot.statusText = Tr("Checking Auto Tom...");
            TryAutoTom();
            state = FarmFlowState::Harvesting;
            break;

        case FarmFlowState::Harvesting:
            SetFarmFlowState(instanceId, account, state, "Scanning for grown crops");
            bot.statusText = Tr("Checking Harvest...");
            harvestStatus = TryHarvest();
            state = (harvestStatus == 2) ? FarmFlowState::HandlingSiloFull : FarmFlowState::Planting;
            break;

        case FarmFlowState::HandlingSiloFull:
            if (g_OnlySellIfSiloFull) {
                SetFarmFlowState(instanceId, account, state, "Silo full reached, running the selected normal sale mode");
                AddLog(instanceId, "SILO FULL! Triggering the normal sale mode because 'Only sell if silo is full' is enabled.", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
            }
            else {
                SetFarmFlowState(instanceId, account, state, "Emergency protocol: plant, sell, harvest");
                AddLog(instanceId, Tr("SILO FULL! Executing Emergency Protocol (Plant -> Sell -> Harvest)..."), ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
            }

            bot.statusText = g_OnlySellIfSiloFull ? Tr("Preparing Silo-Full Sale...") : Tr("Emergency Plant...");
            TryPlant();

            if (!EnsureSiloFullPopupClosed(instanceId, "pre-sale silo-full cleanup")) {
                AddLog(instanceId, "Warning: silo-full popup still appears visible after the second close attempt. Continuing into emergency sales anyway.", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
            }

            bot.statusText = g_OnlySellIfSiloFull ? Tr("Silo-Full Sales...") : Tr("Emergency Sales...");
            RunSalesCycle(instanceId, accountIndex, !g_OnlySellIfSiloFull);

            AddLog(instanceId, Tr("Emergency sales pass finished. Resuming harvest after emergency actions..."), ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
            bot.statusText = Tr("Resuming Harvest...");
            harvestStatus = TryHarvest();
            state = FarmFlowState::Planting;
            break;

        case FarmFlowState::Planting:
            SetFarmFlowState(instanceId, account, state, outOfSeeds ? "Skipping planting because the account ran out of seeds" : "Checking empty fields and planting");
            bot.statusText = Tr("Checking Plant...");
            plantDone = false;
            if (!outOfSeeds) {
                plantDone = TryPlant();
            }
            state = (!plantDone && !outOfSeeds) ? FarmFlowState::RetryingRecovery :
                (ShouldRunCowFeed(account) ? FarmFlowState::HandlingCowFeed :
                (ShouldRunDairy(account) ? FarmFlowState::HandlingDairy : FarmFlowState::EvaluatingSales));
            break;

        case FarmFlowState::RetryingRecovery:
            SetFarmFlowState(instanceId, account, state, "Retrying harvest and plant because the last plant pass looked incomplete");
            AddLog(instanceId, Tr("Plant incomplete or failed. Suspecting false positive or stuck menu. Retrying..."), ImVec4(1, 0.5f, 0, 1));
            {
                cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                MatchResult crossRes = FindBestCrossAny(screen);
                if (crossRes.found) {
                    AdbTap(instanceId, crossRes.x, crossRes.y);
                } else {
                    // Last resort: dismiss whatever is blocking the farm view
                    DismissUnknownOverlay(instanceId, "RetryingRecovery stuck menu");
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            bot.statusText = Tr("Retrying Harvest...");
            harvestStatus = TryHarvest();

            if (harvestStatus == 2) {
                bot.statusText = g_OnlySellIfSiloFull ? Tr("Silo-Full Sales...") : Tr("Emergency Sales...");
                RunSalesCycle(instanceId, accountIndex, !g_OnlySellIfSiloFull);
                harvestStatus = TryHarvest();
            }

            bot.statusText = Tr("Retrying Plant...");
            if (!outOfSeeds) plantDone = TryPlant();

            if (!plantDone && harvestStatus == 0) {
                AddLog(instanceId, Tr("All retries failed. Attempting last-resort overlay dismissal before moving on."), ImVec4(1, 0.2f, 0.2f, 1));
                if (!DismissUnknownOverlay(instanceId, "all retries exhausted")) {
                    AddLog(instanceId, "Overlay dismissal failed. Reloading account to recover from stuck state.", ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                    LoadAccountFromSlot(instanceId, accountIndex);
                    AddLog(instanceId, std::string("Waiting ") + std::to_string(g_Intervals.gameLoadWait) + "s for game to reload...", ImVec4(1, 1, 0, 1));
                    for (int t = 0; t < g_Intervals.gameLoadWait; t++) {
                        if (!bot.isRunning) return;
                        bot.statusText = "Reloading: " + std::to_string(g_Intervals.gameLoadWait - t) + "s";
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                    return; // Abort this cycle safely
                }
            }
            state = ShouldRunCowFeed(account) ? FarmFlowState::HandlingCowFeed :
                (ShouldRunDairy(account) ? FarmFlowState::HandlingDairy : FarmFlowState::EvaluatingSales);
            break;

        case FarmFlowState::HandlingCowFeed:
            SetFarmFlowState(instanceId, account, state, "Creating cow feed and feeding the cow pasture");
            bot.statusText = "Cow Feed";
            {
                CowFeedRoutineResult cowFeedResult = RunCowFeedRoutine(instanceId, accountIndex, false);
                if (cowFeedResult == CowFeedRoutineResult::Completed) {
                    account.lastCowFeedRun = std::chrono::steady_clock::now();
                }
                else if (cowFeedResult == CowFeedRoutineResult::NotEnoughResources) {
                    account.cowFeedCropRecoveryRequested = true;
                    SaveConfig();
                    AddLog(instanceId, "Cow Feed: resource shortage recorded. The next empty-field planting pass will use soybean/corn recovery crops.", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                }
            }
            state = ShouldRunDairy(account) ? FarmFlowState::HandlingDairy : FarmFlowState::EvaluatingSales;
            break;

        case FarmFlowState::HandlingDairy:
            SetFarmFlowState(instanceId, account, state, "Creating dairy product from the configured dairy slot");
            bot.statusText = "Dairy";
            if (RunDairyRoutine(instanceId, accountIndex, false)) {
                account.lastDairyRun = std::chrono::steady_clock::now();
            }
            state = FarmFlowState::EvaluatingSales;
            break;

        case FarmFlowState::EvaluatingSales:
            SetFarmFlowState(instanceId, account, state, "Deciding whether this cycle should sell");
            account.lastPlantTime = std::chrono::steady_clock::now();
            account.isFirstRun = false;

            if (g_OnlySellIfSiloFull) {
                AddLog(instanceId, "Normal sales skipped because 'Only sell if silo is full' is enabled.", ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
                bot.statusText = Tr("Waiting For Silo Full");
                state = FarmFlowState::WaitingForSiloFull;
            }
            else if (bot.enableRandomSaleCycle) {
                if (account.targetCyclesBeforeSale <= 0) {
                    account.targetCyclesBeforeSale = (rand() % 3) + 1;
                }

                account.currentCyclesWithoutSale++;

                if (account.currentCyclesWithoutSale >= account.targetCyclesBeforeSale) {
                    AddLog(instanceId, std::string(Tr("Random Sales Triggered! (")) + std::to_string(account.currentCyclesWithoutSale) + "/" + std::to_string(account.targetCyclesBeforeSale) + ")", ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
                    state = FarmFlowState::CheckingSales;
                }
                else {
                    AddLog(instanceId, std::string(Tr("Skipping Sales to mimic human behavior... (")) + std::to_string(account.currentCyclesWithoutSale) + "/" + std::to_string(account.targetCyclesBeforeSale) + ")", ImVec4(0.5f, 0.7f, 0.5f, 1.0f));
                    bot.statusText = Tr("Skipped Sales (Random)");
                    state = FarmFlowState::CycleComplete;
                }
            }
            else {
                state = FarmFlowState::CheckingSales;
            }
            break;

        case FarmFlowState::WaitingForSiloFull:
            SetFarmFlowState(instanceId, account, state, "Regular sales are disabled until silo full");
            state = FarmFlowState::CycleComplete;
            break;

        case FarmFlowState::CheckingSales:
            SetFarmFlowState(instanceId, account, state, "Running the normal sales cycle");
            bot.statusText = Tr("Checking Sales...");
            RunSalesCycle(instanceId, accountIndex, false);
            if (bot.enableRandomSaleCycle) {
                account.currentCyclesWithoutSale = 0;
                account.targetCyclesBeforeSale = (rand() % 3) + 1;
            }
            state = FarmFlowState::CycleComplete;
            break;

        case FarmFlowState::CycleComplete:
            SetFarmFlowState(instanceId, account, state, "Account cycle complete");
            SetSalesFlowState(instanceId, account, SalesFlowState::Idle, "Waiting for next sales cycle");
            AddLog(instanceId, Tr("Account Cycle Done. Moving to next..."), ImVec4(0.5f, 0.5f, 0.5f, 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(g_Intervals.nextAccountWait));
            return;

        case FarmFlowState::Idle:
        case FarmFlowState::Error:
        default:
            SetFarmFlowState(instanceId, account, FarmFlowState::Error, "Unexpected farm state, ending cycle");
            return;
        }
    }

    SetFarmFlowState(instanceId, account, FarmFlowState::Idle, "Bot stopped");
    SetSalesFlowState(instanceId, account, SalesFlowState::Idle, "Bot stopped");
}
// MAIN BOT FUNCTION. THIS FUNCTION RUNS THE ENTIRE BOT LOGIC FOR ONE INSTANCE IN A LOOP UNTIL THE BOT IS STOPPED.
void RunPremiumBot(int instanceId) {
    BotInstance& bot = g_Bots[instanceId];
    // --- MEMUC STRICT INSPECTOR ---
    //AddLog(instanceId, "[INSPECTOR] Checking MEmu engine configuration...");

    //std::string width = GetMEmuConfig(instanceId, "resolution_width");
    //std::string height = GetMEmuConfig(instanceId, "resolution_height");
    //std::string dpi = GetMEmuConfig(instanceId, "vbox_dpi");

	// STOP THE BOT IF THE RESOLUTION OR DPI SETTINGS ARE NOT CORRECT TO AVOID UNEXPECTED BEHAVIOR AND FALSE DETECTIONS.
    //if (width != "640" || height != "480" || dpi != "100") {
        //AddLog(instanceId, "[FATAL ERROR] Emulator Settings are WRONG!");
        //AddLog(instanceId, ("-> Current: " + width + "x" + height + " | DPI: " + dpi).c_str());
        //AddLog(instanceId, "-> REQUIRED: 640x480 | DPI: 100");
        //AddLog(instanceId, "Please change settings in MEmu, restart emulator and try again.");
       // bot.isRunning = false;
      //  return;
    //}
    //AddLog(instanceId, "[INSPECTOR] Settings verified (640x480 | 100 DPI)."); 
    AddLog(instanceId, Tr("Bot Started."), ImVec4(0, 1, 0, 1));
    if (strcmp(bot.inputDevice, "/dev/input/event1") == 0) {
        if (AutoDetectTouchDevice(instanceId)) {
            SaveConfig();
        }
    }
    AddLog(instanceId, "Waking up Minitouch agent and opening ports...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
    StartMinitouchStealth(instanceId);

    // WAITING 2 SECONDS FOR THE MINITOUCH TO WAKE UP PROPERLY.
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // ONE-TIME: Verify zoom hack is present on the device (auto-fix if a game update wiped it)
    EnsureZoomHackPresent(instanceId);
    while (bot.isRunning) {
        EmulatorCrashWatchdog(instanceId);
        auto multiModeSkipMask = ParseMultiModeSkipMask(bot.multiModeSkipSlots);

        // --- Pair Rotation: dynamically choose which accounts are active ---
        if (bot.enablePairRotation && bot.useMultiMode) {
            // Collect saved + not-manually-skipped account indices
            std::vector<int> eligibleSlots;
            for (int k = 0; k < MAX_ACCOUNT_SLOTS; k++) {
                if (bot.accounts[k].hasFile && !IsMultiModeSlotSkipped(multiModeSkipMask, k))
                    eligibleSlots.push_back(k);
            }
            int groupSize = (std::max)(1, (std::min)((int)eligibleSlots.size(), bot.pairGroupSize));

            if ((int)eligibleSlots.size() > groupSize) {
                // Check if rotation timer expired
                auto now = std::chrono::steady_clock::now();
                if (bot.pairRotationStartedAt.time_since_epoch().count() == 0) {
                    bot.pairRotationStartedAt = now;
                }
                int elapsedMin = static_cast<int>(std::chrono::duration_cast<std::chrono::minutes>(now - bot.pairRotationStartedAt).count());
                if (elapsedMin >= bot.pairRotationMinutes) {
                    bot.currentPairOffset = (bot.currentPairOffset + groupSize) % (int)eligibleSlots.size();
                    bot.pairRotationStartedAt = now;
                    SaveConfig();
                    AddLog(instanceId, "Pair rotation: advancing to group offset " + std::to_string(bot.currentPairOffset) +
                        " (slots: " + [&]() {
                            std::string s;
                            for (int g = 0; g < groupSize; g++) {
                                int idx = (bot.currentPairOffset + g) % (int)eligibleSlots.size();
                                if (g > 0) s += ",";
                                s += std::to_string(eligibleSlots[idx] + 1);
                            }
                            return s;
                        }() + ")", ImVec4(0.2f, 1.0f, 0.6f, 1.0f));
                }

                // Build a new skip mask that only allows the current group
                std::array<bool, MAX_ACCOUNT_SLOTS> pairMask;
                pairMask.fill(true); // skip everything
                for (int g = 0; g < groupSize; g++) {
                    int idx = (bot.currentPairOffset + g) % (int)eligibleSlots.size();
                    pairMask[eligibleSlots[idx]] = false; // un-skip this slot
                }
                multiModeSkipMask = pairMask;
            }
        }

        int savedAccountCount = 0;
        int activeAccountCount = 0;
        for (int k = 0; k < MAX_ACCOUNT_SLOTS; k++) {
            if (!bot.accounts[k].hasFile) continue;
            savedAccountCount++;
            if (!bot.useMultiMode || !IsMultiModeSlotSkipped(multiModeSkipMask, k)) activeAccountCount++;
        }

        if (bot.useMultiMode && activeAccountCount == 0) {
            if (bot.statusText != "No Multi Slots Selected") {
                AddLog(instanceId, "Multi Mode is enabled, but every saved slot is excluded by the skip list. Nothing will run until you clear at least one slot.", ImVec4(1.0f, 0.55f, 0.2f, 1.0f));
            }
            bot.statusText = "No Multi Slots Selected";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        bool runLimitReplacementMode =
            bot.enableAccountRunLimit &&
            bot.replaceAccountDuringRest &&
            savedAccountCount > 1;
        bool isSingleAccountMode = (bot.useSingleMode || (activeAccountCount <= 1)) && !runLimitReplacementMode;

        if (isSingleAccountMode) {
            bot.statusText = Tr("Single Account Mode");
        }
        auto waitWithStatus = [&](int waitSeconds, const std::string& label) {
            waitSeconds = (std::max)(0, waitSeconds);
            for (int w = 0; w < waitSeconds; ++w) {
                if (!bot.isRunning) break;
                int remaining = waitSeconds - w;
                bot.statusText = label + ": " + std::to_string(remaining) + "s";
                if (w > 0 && (w % bot.reviveCheckInterval == 0)) {
                    HandleReviveHeartbeat(instanceId);
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        };
        bool processedOneSingleAccount = false;
        int shortestRestSeconds = INT_MAX;
        for (int i = 0; i < MAX_ACCOUNT_SLOTS; i++) {
            if (!bot.isRunning) break;
            if (isSingleAccountMode && processedOneSingleAccount) {
                break;
            }
            if (savedAccountCount == 0) {
                if (i > 0) break;
            }
            else {
                if (!bot.accounts[i].hasFile) continue;
                if (bot.useSingleMode && i != bot.currentAccountIndex) continue;
                if (bot.useMultiMode && IsMultiModeSlotSkipped(multiModeSkipMask, i)) continue;
            }

            if (bot.enableAccountRunLimit) {
                bot.accountRunMinutes = (std::max)(1, (std::min)(1440, bot.accountRunMinutes));
                bot.accountRestMinutes = (std::max)(1, (std::min)(1440, bot.accountRestMinutes));

                AccountSlot& runAccount = bot.accounts[i];
                auto now = std::chrono::steady_clock::now();
                if (runAccount.restUntil.time_since_epoch().count() != 0 && runAccount.restUntil > now) {
                    int restSeconds = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(runAccount.restUntil - now).count());
                    if (isSingleAccountMode && !bot.replaceAccountDuringRest) {
                        AddLog(instanceId,
                            "Account run limit: staying AFK on slot " + std::to_string(i + 1) + " for " + std::to_string((restSeconds + 59) / 60) + " minute(s).",
                            ImVec4(0.8f, 0.85f, 0.3f, 1.0f));
                        waitWithStatus(restSeconds, "AFK Rest Slot " + std::to_string(i + 1));
                        runAccount.restUntil = std::chrono::steady_clock::time_point{};
                        processedOneSingleAccount = true;
                    }
                    else {
                        shortestRestSeconds = (std::min)(shortestRestSeconds, restSeconds);
                        AddLog(instanceId,
                            "Account run limit: slot " + std::to_string(i + 1) + " is resting for " + std::to_string((restSeconds + 59) / 60) + " minute(s).",
                            ImVec4(0.8f, 0.85f, 0.3f, 1.0f));
                    }
                    continue;
                }

                if (runAccount.activeSessionStartedAt.time_since_epoch().count() == 0) {
                    runAccount.activeSessionStartedAt = now;
                }
                int activeMinutes = static_cast<int>(std::chrono::duration_cast<std::chrono::minutes>(now - runAccount.activeSessionStartedAt).count());
                if (activeMinutes >= bot.accountRunMinutes) {
                    runAccount.activeSessionStartedAt = std::chrono::steady_clock::time_point{};
                    runAccount.restUntil = now + std::chrono::minutes(bot.accountRestMinutes);
                    int restSeconds = bot.accountRestMinutes * 60;
                    AddLog(instanceId,
                        "Account run limit: slot " + std::to_string(i + 1) + " reached " + std::to_string(bot.accountRunMinutes) +
                        " minute(s). Resting for " + std::to_string(bot.accountRestMinutes) + " minute(s).",
                        ImVec4(0.9f, 0.75f, 0.25f, 1.0f));
                    if (isSingleAccountMode && !bot.replaceAccountDuringRest) {
                        waitWithStatus(restSeconds, "AFK Rest Slot " + std::to_string(i + 1));
                        runAccount.restUntil = std::chrono::steady_clock::time_point{};
                        processedOneSingleAccount = true;
                    }
                    else {
                        shortestRestSeconds = (std::min)(shortestRestSeconds, restSeconds);
                    }
                    continue;
                }
            }

            bot.currentAccountIndex = i;
            ApplyFarmConfigForSlot(instanceId, i);
            bot.accounts[i].emergencyWheatStacksSoldThisTurn = 0;
            processedOneSingleAccount = true; 
            // =========================================================================
            // 1. SHOP FULL (TIME) CONTROL
            // =========================================================================
            if (bot.accounts[i].isShopFullStuck) {
                auto now = std::chrono::steady_clock::now();
                auto elapsedMins = std::chrono::duration_cast<std::chrono::minutes>(now - bot.accounts[i].lastShopCheckTime).count();

                if (elapsedMins < 2) { 
                    if (isSingleAccountMode) {
                        int waitSecs = 120 - std::chrono::duration_cast<std::chrono::seconds>(now - bot.accounts[i].lastShopCheckTime).count();
                        AddLog(instanceId, "Single Account Quarantine: Waiting " + std::to_string(waitSecs) + "s for Ad Cooldown...", ImVec4(1, 0.5f, 0, 1));
                        for (int w = 0; w < waitSecs; w++) {
                            if (!bot.isRunning) break;
                            bot.statusText = "Ad Cooldown: " + std::to_string(waitSecs - w) + "s";
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }
                    }
                    else {
                        AddLog(instanceId, "Account is in Shop Full Quarantine. Skipping to next account...", ImVec4(1, 0.2f, 0.2f, 1));
                        continue; 
                    }
                }
            }
            int batchCycles = (std::max)(1, bot.cyclesPerAccount);
            for (int batchIdx = 0; batchIdx < batchCycles; ++batchIdx) {
                if (!bot.isRunning) break;

                int currentCropTime = GetAccountCropWaitSeconds(bot.accounts[i], bot.testCropMode);

            if (!bot.accounts[i].isFirstRun) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - bot.accounts[i].lastPlantTime).count();
                int remaining = currentCropTime - (int)elapsed;

                if (remaining > 0) {
                    std::string batchLabel = (batchCycles > 1) ? " [" + std::to_string(batchIdx + 1) + "/" + std::to_string(batchCycles) + "]" : "";
                    AddLog(instanceId, std::string(Tr("Waiting for growth (")) + std::to_string(remaining) + Tr("s)...") + batchLabel, ImVec4(1, 1, 0, 1));
                    for (int t = 0; t < remaining; t++) {
                        if (!bot.isRunning) return;
                        bot.statusText = std::string(Tr("Waiting Slot ")) + std::to_string(i + 1) + ": " + std::to_string(remaining - t) + "s";

                        if (t > 0 && (t % bot.reviveCheckInterval == 0)) {
                            HandleReviveHeartbeat(instanceId);
                        }
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
            }

                // Only load the account on the FIRST batch cycle
                if (batchIdx == 0 && !isSingleAccountMode) {
                bot.statusText = std::string(Tr("Loading Slot ")) + std::to_string(i + 1);
                LoadAccountFromSlot(instanceId, i);

                AddLog(instanceId, std::string(Tr("Waiting ")) + std::to_string(g_Intervals.gameLoadWait) + Tr("s for game logic..."), ImVec4(1, 1, 0, 1));
                for (int w = 0; w < g_Intervals.gameLoadWait; w++) {
                    if (!bot.isRunning) break;
                    bot.statusText = std::string(Tr("Loading Game... ")) + std::to_string(g_Intervals.gameLoadWait - w) + "s";
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (!bot.isRunning) break;

                bot.statusText = Tr("Game Ready.");
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            else {
                bot.statusText = Tr("Single Account Mode Active.");
                if (bot.accounts[i].isFirstRun) {
                    AddLog(instanceId, Tr("Single mode auto-detected. Skipping game restart."), ImVec4(0, 1, 1, 1));
                }
            }
            bot.statusText = Tr("Checking Game Load...");
            bool gameLoaded = false;

            // =========================================================================
     //      SKIP MAILBOX SCAN IN SINGLE MODE COMPLETELY
            // =========================================================================
                if (isSingleAccountMode || batchIdx > 0) {
                    gameLoaded = true; // Game already open from previous batch cycle or single mode.
                    if (bot.accounts[i].isFirstRun && batchIdx == 0) {
                        AddLog(instanceId, "Single Account Mode: Skipping Mailbox scan, starting immediately!", ImVec4(0, 1, 0, 1));
                    }
                }
            else {
                // ONLY WAIT IN MULTI ACCOUNT MODE.
                bot.statusText = Tr("Checking Game Load...");
                MatchResult mailRes;
                bool loadingVisibleAtTimeout = false;
                for (int k = 0; k < 45; k++) {
                    if (!bot.isRunning) break;
                    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                    mailRes = FindImage(screen, mailbox_templatePath, g_Thresholds.mailboxThreshold, false);

                    if (mailRes.found) {
                        gameLoaded = true;
                        if (bot.accounts[i].isFirstRun) {
                            AddLog(instanceId, Tr("Game Loaded! Mailbox found."), ImVec4(0, 1, 0, 1));
                        }
                        break;
                    }
                    if (k == 44) {
                        MatchResult loadingRes = FindImage(screen, loading_templatePath, g_Thresholds.loadingThreshold, false, 1.0f, false);
                        loadingVisibleAtTimeout = loadingRes.found;
                    }
                    bot.statusText = std::string(Tr("Waiting for Game... ")) + std::to_string(k) + "/45";
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (!gameLoaded && bot.isRunning && loadingVisibleAtTimeout) {
                    AddLog(instanceId, "Mailbox timeout reached, but loading template is still visible. Waiting 3s more...", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                    for (int extra = 0; extra < 3; ++extra) {
                        if (!bot.isRunning) break;
                        bot.statusText = "Loading visible: +" + std::to_string(extra + 1) + "/3s";
                        std::this_thread::sleep_for(std::chrono::seconds(1));

                        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                        mailRes = FindImage(screen, mailbox_templatePath, g_Thresholds.mailboxThreshold, false);
                        if (mailRes.found) {
                            gameLoaded = true;
                            AddLog(instanceId, "Game Loaded! Mailbox found during loading grace period.", ImVec4(0, 1, 0, 1));
                            break;
                        }
                    }
                }
            }

            if (!gameLoaded) {
                AddLog(instanceId, Tr("Timeout (Mailbox not found). Skipping account."), ImVec4(1, 0.4f, 0.4f, 1));
                continue;
            }
                if (batchIdx == 0 && !isSingleAccountMode && g_Intervals.autoZoomOutAfterMailbox) {
                if (!WaitForBotActionWindow(instanceId, (std::max)(0, g_Intervals.mailboxZoomDelayMs))) {
                    break;
                }
                if (!ZoomOutFarmView(instanceId, "mailbox detection")) {
                    AddLog(instanceId, "Mailbox zoom normalization was skipped or failed. Continuing with the current view.", ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
                }
            }
            cv::Mat curScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);

            // ACCOUNT INFORMATION READING GOES HERE.
                if (batchIdx == 0 && !curScreen.empty() && curScreen.cols > 600 && curScreen.rows > 100) {
                GameNumberDebugInfo coinInfo = ReadGameNumberDebug(curScreen, cv::Rect(507, 2, 72, 21), "coin");
                GameNumberDebugInfo diaInfo = ReadGameNumberDebug(curScreen, cv::Rect(547, 22, 34, 21), "dia");
                bot.accounts[i].coinAmount = coinInfo.count;
                bot.accounts[i].diamondAmount = diaInfo.count;

                if (bot.accounts[i].playerTag == "Unknown" || bot.accounts[i].playerTag == "NO_TAG") {
                    bot.statusText = Tr("Extracting Profile Data...");
                    AddLog(instanceId, Tr("Reading Account Profile Data..."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));

                    GameNumberDebugInfo levelInfo = ReadGameNumberDebug(curScreen, cv::Rect(16, 8, 22, 20), "lvl");
                    bot.accounts[i].level = levelInfo.count;
                    bot.accounts[i].hudReadDetail = BuildHudReadDetail(coinInfo, diaInfo, levelInfo);

                    AdbTap(instanceId, g_CoordinateProfile.profileOpenX, g_CoordinateProfile.profileOpenY);
                    std::this_thread::sleep_for(std::chrono::milliseconds(800)); 
                    cv::Mat profileScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);

                    
                    if (!profileScreen.empty() && profileScreen.cols > 450 && profileScreen.rows > 150) {
                        bot.accounts[i].farmName = ReadFarmName(profileScreen, cv::Rect(203, 63, 220, 29));

                        AdbTap(instanceId, g_CoordinateProfile.profileCopyTagX, g_CoordinateProfile.profileCopyTagY);
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        bot.accounts[i].playerTag = GetClipboardText();
                        SaveConfig();
                    }
                    else {
                        AddLog(instanceId, "Warning: Profile screen not loaded properly, skipping tag extraction.", ImVec4(1, 0.5f, 0, 1));
                    }

                    AdbTap(instanceId, g_CoordinateProfile.profileCloseX, g_CoordinateProfile.profileCloseY);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                else {
                    GameNumberDebugInfo levelInfo = ReadGameNumberDebug(curScreen, cv::Rect(16, 8, 22, 20), "lvl");
                    if (levelInfo.count > 0) bot.accounts[i].level = levelInfo.count;
                    bot.accounts[i].hudReadDetail = BuildHudReadDetail(coinInfo, diaInfo, levelInfo);
                }
            }

                if (batchCycles > 1) {
                    AddLog(instanceId, "Batch cycle " + std::to_string(batchIdx + 1) + "/" + std::to_string(batchCycles) + " on slot " + std::to_string(i + 1), ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                }
                RunAccountFarmCycle(instanceId, i);
            } // end batch loop
            continue;
        }
        if (bot.isRunning && bot.enableAccountRunLimit && !processedOneSingleAccount && shortestRestSeconds != INT_MAX) {
            AddLog(instanceId,
                "Account run limit: all eligible slots are resting. Waiting for the next slot to become available.",
                ImVec4(0.8f, 0.85f, 0.3f, 1.0f));
            waitWithStatus(shortestRestSeconds, "All Slots Resting");
        }
        if (!bot.isRunning) break;
        SendRotationReport(instanceId);
        if (bot.enableJanitor) {
            bot.currentJanitorCycles++;

            if (bot.currentJanitorCycles >= bot.janitorLimit) {
                bot.statusText = "JANITOR: DEEP CLEANING RAM...";
                AddLog(instanceId, "[JANITOR] Cycle limit (" + std::to_string(bot.janitorLimit) + ") reached! Halting operations for Deep Clean...", ImVec4(0, 1, 1, 1));

                // CLOSE THE GAME 
                RunAdbCommand(instanceId, "shell am force-stop com.supercell.hayday");
                std::this_thread::sleep_for(std::chrono::seconds(2));

                // 2. CLOSE EMULATOR
                AddLog(instanceId, "[JANITOR] Shutting down emulator to flush Memory Leaks...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));

                // --- LDPLAYER & MEMU CHECK ---
                if (bot.emulatorType == 1) { // LDPLAYER
                    RunCmdHidden("cmd.exe /c \"\"" + kLDConsolePath + "\" quit --index " + std::to_string(bot.emuIndex) + "\"");
                }
                else { // MEMU
                    RunCmdHidden("cmd.exe /c \"\"" + kMEmuConsolePath + "\" stop " + bot.vmName + "\"");
                }

				// 10 SECONDS WAIT FOR EMULATOR TO PROPERLY SHUT DOWN AND FLUSH RAM (BECAUSE JUST KILLING THE PROCESS DOESNT FLUSH RAM, YOU HAVE TO PROPERLY SHUT IT DOWN)
                bot.statusText = "JANITOR: FLUSHING WINDOWS RAM...";
                std::this_thread::sleep_for(std::chrono::seconds(10));

                // 3. FRESH START THE EMULATOR
                AddLog(instanceId, "[JANITOR] Booting up a fresh emulator instance...", ImVec4(0, 1, 0, 1));

                // --- LDPLAYER & MEMU CONTROL ---
                if (bot.emulatorType == 1) { // LDPLAYER
                    RunCmdHidden("cmd.exe /c \"\"" + kLDConsolePath + "\" launch --index " + std::to_string(bot.emuIndex) + "\"");
                }
                else { // MEMU
                    RunCmdHidden("cmd.exe /c \"\"" + kMEmuConsolePath + "\" start " + bot.vmName + "\"");
                }
                //WAIT OR ANDROID TO WAKE UP PROPERLY.
                bot.statusText = "JANITOR: BOOTING ANDROID OS...";
                for (int w = 0; w < 35; w++) {
                    if (!bot.isRunning) break;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                // 4. START MINITOUCH BECAUSE WE RESTARTED THE AGENT AND NEED TO WAKE UP AGAIN.
                if (bot.isRunning) {
                    bot.statusText = "JANITOR: INJECTING AGENTS...";
                    AddLog(instanceId, "[JANITOR] Re-injecting Minitouch input agent...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
                    StartMinitouchStealth(instanceId);
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }

                // 5. VERIFY ZOOM HACK AFTER EMULATOR REBOOT
                if (bot.isRunning) {
                    bot.statusText = "JANITOR: VERIFYING ZOOM HACK...";
                    EnsureZoomHackPresent(instanceId);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                bot.currentJanitorCycles = 0; //CLEANING IS DONE,RESET TTIMER.
                AddLog(instanceId, "[JANITOR] System completely refreshed! Resuming normal operations.", ImVec4(0, 1, 0, 1));
            }
            else {
                AddLog(instanceId, "[JANITOR] Cycles until next deep clean: " + std::to_string(bot.janitorLimit - bot.currentJanitorCycles), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            }
        }
        if (!isSingleAccountMode) {
            bot.statusText = Tr("Cycle End Cleanup...");
            AddLog(instanceId, Tr("Performing deep system cleanup to prevent Game Not Responding Error..."), ImVec4(0.8f, 0.4f, 1.0f, 1.0f));

            RunAdbCommand(instanceId, "shell am force-stop com.supercell.hayday");
            RunAdbCommand(instanceId, "shell am kill-all");
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        else {
            bot.statusText = Tr("Cycle Done");
            AddLog(instanceId, Tr("Single Account Cycle Done. Waiting for crops..."), ImVec4(0.5f, 0.5f, 0.5f, 1));
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }

    }
}


// ==============================================================================
// FUZZY MATCHER FOR TESSERACT OCR TO READ ITEMS COUNTS IN BARN (FUZZY MATCH)
// ==============================================================================
float GetSimilarityScore(const std::string& s1, const std::string& s2) {
    if (s1.empty() || s2.empty()) return 0.0f;

    std::string str1 = s1; std::string str2 = s2;

	// 1. TRANSFORM TO LOWERCASE
    std::transform(str1.begin(), str1.end(), str1.begin(), ::tolower);
    std::transform(str2.begin(), str2.end(), str2.begin(), ::tolower);

    // =========================================================
	// 2. OCR CHARACTER NORMALIZATION (TO HANDLE COMMON OCR MISTAKES) 
    // =========================================================
    auto NormalizeOCR = [](std::string& s) {
        for (char& c : s) {
            if (c == '0') c = 'o';                               // 0 AND o, O
            else if (c == '1' || c == 'l' || c == '!' || c == '|') c = 'i'; // 1, L, i, !, |
            else if (c == '5') c = 's';                          // 5 AND s, S
            else if (c == '2') c = 'z';                          // 2 AND z, Z
            else if (c == '8') c = 'b';                          // 8 AND b, B
            else if (c == 'q') c = 'g';                          // q AND g
        }
        };


    NormalizeOCR(str1);
    NormalizeOCR(str2);

	// 3. CLASSIC FUZZY MATCHING USING LEVENSHTEIN DISTANCE
    int len1 = (int)str1.size();
    int len2 = (int)str2.size();
    std::vector<std::vector<int>> d(len1 + 1, std::vector<int>(len2 + 1));

    for (int i = 0; i <= len1; ++i) d[i][0] = i;
    for (int i = 0; i <= len2; ++i) d[0][i] = i;

    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            int cost = (str1[i - 1] == str2[j - 1]) ? 0 : 1;
            int a = d[i - 1][j] + 1;
            int b = d[i][j - 1] + 1;
            int c = d[i - 1][j - 1] + cost;
            d[i][j] = std::min(a, std::min(b, c));
        }
    }

    int maxLen = std::max(len1, len2);
    int distance = d[len1][len2];
    return (1.0f - ((float)distance / (float)maxLen)) * 100.0f;
}

// ==============================================================================
//RADAR AND CLICK STUFF AND BTW THIS IS NOT DONE SO I LITERALLY QUITTED MID-DEVELOPING SO IF YOUR BUILDING YOUR OWN BOT JUST DELETE THESE AND FORGET ABOUT IT
// HASTA HISLER KOPTU İPLER HEMDE KENDİLİĞİNDEN, BİLMEM NASIL İLERLEDİ BU, ACI KEDER DERKEN STRES OLDUM KENDİLİĞİMDEN VE HİÇ KİMSEYE İYİ GELMEDİ BU.
// ==============================================================================

bool NXRTH_Radar(int instanceId, std::string targetName, int mode, bool skipMenuOpen) {
    BotInstance& bot = g_Bots[instanceId];

    cv::Rect roi;
    if (mode == 0) roi = cv::Rect(171, 131, 180, 265);
    else roi = cv::Rect(170, 251, 194, 140);

    int whiteMin = 200;
    AddLog(instanceId, "RADAR: Initializing System...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));


    if (!skipMenuOpen) {
        ForceCloseAllMenus(instanceId);
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        cv::Mat scr = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult fRes = FindImage(scr, "templates\\friends.png", 0.65f, false, 1.0f, false);
        if (fRes.found) {
            AdbTap(instanceId, fRes.x, fRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        scr = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult bookRes = FindImage(scr, "templates\\friends_book.png", 0.70f, false, 1.0f, false);
        if (bookRes.found) {
            AdbTap(instanceId, bookRes.x, bookRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        }
    }
    else {
        AddLog(instanceId, "RADAR: Skipping menu open, already inside Friend Book.", ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }

    if (mode == 0) {
        AddLog(instanceId, "RADAR: Mode 0 Active. Switching to 'In-game friends' tab...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
        AdbTap(instanceId, 280, 117);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        AdbTap(instanceId, 280, 117); 
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
    else {
        AddLog(instanceId, "RADAR: Mode 1 Active. Scanning 'Requests' tab...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
    }

    AddLog(instanceId, "RADAR: Scanning for -> [" + targetName + "]...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));

    for (int scanTry = 1; scanTry <= 10; scanTry++) {
        if (!bot.isRunning) return false;

        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (screen.empty() || roi.x + roi.width > screen.cols || roi.y + roi.height > screen.rows) continue;

        cv::Mat crop = screen(roi);
        cv::Mat resizedImage, bw;

        cv::resize(crop, resizedImage, cv::Size(), 3.0, 3.0, cv::INTER_CUBIC);
        cv::inRange(resizedImage, cv::Scalar(whiteMin, whiteMin, whiteMin), cv::Scalar(255, 255, 255), bw);
        cv::bitwise_not(bw, bw);
        cv::copyMakeBorder(bw, bw, 10, 10, 10, 10, cv::BORDER_CONSTANT, cv::Scalar(255));

        char exePathBuf[MAX_PATH];
        GetModuleFileNameA(NULL, exePathBuf, MAX_PATH);
        std::string exePath = std::string(exePathBuf);
        std::string tessDataPath = exePath.substr(0, exePath.find_last_of("\\/")) + "\\tessdata";

        tesseract::TessBaseAPI* api = new tesseract::TessBaseAPI();
        if (api->Init(tessDataPath.c_str(), "eng")) {
            delete api; return false;
        }

        api->SetVariable("tessedit_char_whitelist", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ");
        api->SetPageSegMode(tesseract::PSM_SPARSE_TEXT);
        api->SetImage((uchar*)bw.data, bw.cols, bw.rows, 1, bw.step);
        api->Recognize(0);

        tesseract::ResultIterator* ri = api->GetIterator();
        tesseract::PageIteratorLevel level = tesseract::RIL_TEXTLINE;

        bool found = false;

        if (ri != 0) {
            do {
                char* word = ri->GetUTF8Text(level);
                if (word != nullptr) {
                    std::string readName(word);
                    while (!readName.empty() && (readName.back() == '\r' || readName.back() == '\n' || readName.back() == ' ')) readName.pop_back();

                    int x1, y1, x2, y2;
                    ri->BoundingBox(level, &x1, &y1, &x2, &y2);

                    float score = GetSimilarityScore(readName, targetName);

                    
                    if (score >= 60.0f) {
                        int realX1 = x1 / 3;
                        int realY1 = y1 / 3;
                        int realX2 = x2 / 3;
                        int realY2 = y2 / 3;

                        int clickX = roi.x + realX1 + ((realX2 - realX1) / 2);
                        int clickY = roi.y + realY1 + ((realY2 - realY1) / 2);

                        if (mode == 0) {
							// DOUBLE CLICK TO VISIT FARM.
                            AddLog(instanceId, "RADAR: Target found! Double clicking: [" + readName + "]", ImVec4(0, 1, 0, 1));
                            AdbTap(instanceId, clickX, clickY);
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            AdbTap(instanceId, clickX, clickY);
                            found = true;
                        }
                        else if (mode == 1) {
                            // ACCEPT FRIEND REQUEST
                            AddLog(instanceId, "RADAR: Request found! Scanning for green tick on the same row...", ImVec4(0.8f, 0.8f, 0.2f, 1.0f));

                            std::vector<MatchResult> greenTicks = FindAllImages(screen, "templates\\acceptfq.png", 0.70f, 10, false);
                            bool tickFound = false;

                            for (auto& tick : greenTicks) {
                                if (std::abs(tick.y - clickY) <= 40) {
                                    AddLog(instanceId, "RADAR: Green tick matched with name! Accepting...", ImVec4(0, 1, 0, 1));
                                    AdbTap(instanceId, tick.x, tick.y);
									std::this_thread::sleep_for(std::chrono::milliseconds(100));
									AdbTap(instanceId, tick.x, tick.y); // ACCEPT BY DOUBLE CLICKING GREEN TICK.
                                    tickFound = true;
                                    break;
                                }
                            }

                            if (!tickFound) {
                                AddLog(instanceId, "RADAR ERROR: Name found but acceptfq.png missing on that row!", ImVec4(1, 0.3f, 0.3f, 1.0f));
                                AdbTap(instanceId, clickX + 230, clickY); // Fallback: CLICK TO THE POSITION.
                            }
                            found = true;
                        }

                        delete[] word;
                        break;
                    }
                    delete[] word;
                }
            } while (ri->Next(level));
        }
        delete ri;
        api->ClearAdaptiveClassifier();
        api->End();
        delete api;

        if (found) {
            std::this_thread::sleep_for(std::chrono::seconds(2)); 
            return true;
        }

        AddLog(instanceId, "RADAR: Target not found here. Scrolling 50 pixels down...", ImVec4(1, 1, 0, 1));
        RunAdbCommand(instanceId, "shell input swipe 300 350 300 300 1000");
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }

    AddLog(instanceId, "RADAR ERROR: Target [" + targetName + "] not found!", ImVec4(1, 0.2f, 0.2f, 1));
    return false;
}

void RunStorageMaster(int instanceId) {
    BotInstance& bot = g_Bots[instanceId];
    AddLog(instanceId, "Storage Master is online. Waiting for signals...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
    if (strcmp(bot.inputDevice, "/dev/input/event1") == 0) {
        if (AutoDetectTouchDevice(instanceId)) {
            SaveConfig();
        }
    }
    bool friendMenuOpen = false;

    while (bot.isRunning) {
        EmulatorCrashWatchdog(instanceId);
        if (g_TransferRequest.isPending && !g_TransferRequest.storageReady) {


            if (g_TransferRequest.needFriendship && g_TransferRequest.friendRequestSent) {
                bot.statusText = "ACCEPTING FRIEND REQUEST";
                AddLog(instanceId, "Friend request signal received. Accepting...", ImVec4(0.8f, 0.8f, 0.2f, 1.0f));

                if (NXRTH_Radar(instanceId, g_TransferRequest.sellerFarmName, 1, false)) {
                    AddLog(instanceId, "Friend request accepted! Moving directly to infiltration...", ImVec4(0, 1, 0, 1));

                
                    g_TransferRequest.needFriendship = false;
                    friendMenuOpen = true;

                    std::this_thread::sleep_for(std::chrono::seconds(4)); 
                }
                else {
                    ForceCloseAllMenus(instanceId);
                    g_TransferRequest.isPending = false;
                    friendMenuOpen = false;
                }
                continue;
            }

           
            if (!g_TransferRequest.needFriendship) {
                AddLog(instanceId, "SIGNAL RECEIVED! Target: [" + g_TransferRequest.sellerFarmName + "]", ImVec4(1, 1, 0, 1));
                bot.statusText = "INFILTRATING: " + g_TransferRequest.sellerFarmName;

                if (NXRTH_Radar(instanceId, g_TransferRequest.sellerFarmName, 0, friendMenuOpen)) {
                    friendMenuOpen = false; 
                    std::this_thread::sleep_for(std::chrono::seconds(4)); 

              
                    bool shopVerified = false;
                    for (int retry = 0; retry < 3; retry++) {
                        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                        MatchResult shopRes = FindImage(screen, shop_templatePath, g_Thresholds.shopThreshold);

                        if (shopRes.found) {
                            AdbTap(instanceId, shopRes.x, shopRes.y);
                            std::this_thread::sleep_for(std::chrono::milliseconds(2500)); 

                            
                            cv::Mat verifyScr = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                            if (FindImage(verifyScr, cross_templatePath, g_Thresholds.crossThreshold, false, 1.0f, false).found ||
                                FindImage(verifyScr, crate_templatePath, g_Thresholds.crateThreshold, false, 1.0f, false).found) {
                                shopVerified = true;
                                break;
                            }
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    }

                    if (shopVerified) {
                        AddLog(instanceId, "Entered the shop. Initiating Heist Session...", ImVec4(0, 1, 1, 1));

                    
                        
                        while (bot.isRunning && g_TransferRequest.isPending) {
                            g_TransferRequest.storageReady = true; 

                            
                            int waitTimeout = 0;
                            while (!g_TransferRequest.itemListed && waitTimeout < 400) {
                                if (!bot.isRunning || !g_TransferRequest.isPending) break;
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                waitTimeout++;
                            }

                            if (g_TransferRequest.itemListed) {
                                AddLog(instanceId, "ITEM LISTED! Waiting 800ms for network sync...", ImVec4(1, 1, 0, 1));
                                std::this_thread::sleep_for(std::chrono::milliseconds(800));

                                int visitorX = g_TransferRequest.targetSlotX + 30; 
                                int visitorY = g_TransferRequest.targetSlotY;

                                AddLog(instanceId, "TARGET LOCKED! Applying +30 X Offset & Executing Snipe...", ImVec4(1, 0, 0, 1));

                                for (int spam = 0; spam < 15; spam++) {
                                    AdbTap(instanceId, visitorX, visitorY);
                                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                }

                                AddLog(instanceId, "Heist Successful! Item secured.", ImVec4(0, 1, 0, 1));
                            }
                            else {
                                AddLog(instanceId, "Heist Failed! Seller didn't list item (Timeout).", ImVec4(1, 0.5f, 0, 1));
                            }

                            g_TransferRequest.transferComplete = true;

                           
                            int waitAcknowledge = 0;
                            while (g_TransferRequest.isPending && waitAcknowledge < 50) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                waitAcknowledge++;
                            }

                           
                            if (g_TransferRequest.hasMoreItems) {
                                AddLog(instanceId, "SIGNAL RECEIVED: Farm bot has more items! Waiting in shop...", ImVec4(1, 1, 0, 1));

                                int waitNextItem = 0;
                                while (!g_TransferRequest.isPending && waitNextItem < 450) {
                                    if (!bot.isRunning) break;
                                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                    waitNextItem++;
                                }

                                if (g_TransferRequest.isPending) {
                                    continue; 
                                }
                                else {
                                    AddLog(instanceId, "ERROR: Waited 45s but Farm bot got stuck. Going home.", ImVec4(1, 0, 0, 1));
                                    break;
                                }
                            }
                            else {
                                AddLog(instanceId, "No more items to transfer. Finishing heist session.", ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                                break; 
                            }
                        } 

                        ForceCloseAllMenus(instanceId);
                        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

                        cv::Mat homeScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                        MatchResult homeRes = FindImage(homeScreen, "templates\\home.png", 0.70f);

                        if (homeRes.found) {
                            AdbTap(instanceId, homeRes.x, homeRes.y);
                            AddLog(instanceId, "Returning to Home Base...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
                        }
                        else {
                            AddLog(instanceId, "WARNING: home.png not found! Using fallback coordinate.", ImVec4(1, 0.5f, 0.0f, 1.0f));
                            AdbTap(instanceId, 30, 450);
                        }

                        bot.statusText = "LISTENING FOR SIGNALS";
                    }
                    else {
                        AddLog(instanceId, "ERROR: Shop not found or didn't open on target farm!", ImVec4(1, 0, 0, 1));
                        g_TransferRequest.isPending = false;
                    }
                }
                else {
                    AddLog(instanceId, "ERROR: Target not found on Radar!", ImVec4(1, 0, 0, 1));
                    g_TransferRequest.isPending = false;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

bool SendFriendRequestToStorage(int instanceId, std::string targetTag) {
    AddLog(instanceId, "Sending friend request to Storage (" + targetTag + ")...", ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
    ForceCloseAllMenus(instanceId); 

  
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    BotInstance& bot = g_Bots[instanceId];
    bool friendsOpened = false;

    
    for (int k = 0; k < 3; k++) {
        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult friendsRes = FindImage(screen, "templates\\friends.png", 0.65f, false, 1.0f, false);

        if (friendsRes.found) {
            AdbTap(instanceId, friendsRes.x, friendsRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            friendsOpened = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!friendsOpened) {
        AddLog(instanceId, "ERROR: friends.png NOT FOUND on screen!", ImVec4(1, 0, 0, 1));
        return false;
    }


    bool bookOpened = false;
    for (int k = 0; k < 3; k++) {
        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult bookRes = FindImage(screen, "templates\\friends_book.png", 0.70f, false);

        if (bookRes.found) {
            AdbTap(instanceId, bookRes.x, bookRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            bookOpened = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!bookOpened) return false;

    AdbTap(instanceId, 210, 170); 
	RunAdbCommand(instanceId, "shell input keyevent 67");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    RunAdbCommand(instanceId, "shell input text '" + targetTag + "'");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));


	AdbTap(instanceId, 445,167 ); 
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    MatchResult addRes = FindImage(screen, "templates\\addfriend.png", 0.75f, false);
    if (addRes.found) {
        AdbTap(instanceId, addRes.x, addRes.y);
        AddLog(instanceId, "Friend request sent successfully!", ImVec4(0, 1, 0, 1));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    ForceCloseAllMenus(instanceId); 
    return true;
}

extern int g_TargetTabToSelect;


std::string ExecuteMEmuCommandHidden(std::string args) {
    std::string memucPath = kAdbPath;
    size_t lastSlash = memucPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        memucPath = memucPath.substr(0, lastSlash + 1) + "memuc.exe";
    }

    std::string cmd = "cmd.exe /c \"\"" + memucPath + "\" " + args + "\"";
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hRead = NULL;
    HANDLE hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return "";
    }

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 45000); 
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    
    CloseHandle(hWrite);

    std::string result = "";
    DWORD read;
    char buffer[256];

 
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &read, NULL) && read != 0) {
        buffer[read] = '\0';
        result += buffer;
    }
    CloseHandle(hRead);

    return result;
}

std::vector<int> GetExistingMEmuInstances() {
    std::vector<int> instances;
    std::string listOut = ExecuteMEmuCommandHidden("list");
    std::stringstream ss(listOut);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty() || line.find("Step:") != std::string::npos) continue; 
        size_t commaPos = line.find(',');
        if (commaPos != std::string::npos) {
            try {
             
                int index = std::stoi(line.substr(0, commaPos));
                instances.push_back(index);
            }
            catch (...) {}
        }
    }
    return instances;
}
// ========================================================================
// THE EMULATOR FACTORY: 1-CLICK MEMU CREATOR
// ========================================================================
void RunEmulatorFactory(int requestedInstance) {
    AddLog(requestedInstance, "Forging a new MEmu instance in the background... Please wait.", ImVec4(1, 1, 0, 1));

 
    std::string createOut = ExecuteMEmuCommandHidden("create");

  
    std::string lowerOut = createOut;
    for (char& c : lowerOut) c = tolower(c);

  
    int actualVm = -1;
    size_t idxPos = lowerOut.find("index:");
    if (idxPos != std::string::npos) {
        try { actualVm = std::stoi(lowerOut.substr(idxPos + 6)); }
        catch (...) {}
    }

    if (actualVm == -1) {
        std::string errMsg = "Failed to parse VM index!\n\n[MEMU RAW OUTPUT]:\n" + createOut;
        MessageBoxA(NULL, errMsg.c_str(), "NXRTH - Fatal Error", MB_ICONERROR | MB_TOPMOST);
        g_Bots[requestedInstance].isCreatingEmulator = false;
        return;
    }

    int targetInstance = actualVm;
    bool didShift = false;

    if (targetInstance >= 6) {
        std::string limitMsg = "You created VM " + std::to_string(actualVm) + ", but NXRTH supports max 6 instances!";
        MessageBoxA(NULL, limitMsg.c_str(), "NXRTH - Limit Reached", MB_ICONWARNING | MB_TOPMOST);
        g_Bots[requestedInstance].isCreatingEmulator = false;
        return;
    }

    if (actualVm != requestedInstance) {
        didShift = true;
        g_TargetTabToSelect = targetInstance; 
    }

 
    AddLog(targetInstance, "VM created. Waiting for MEmu to unlock config files...", ImVec4(1, 1, 0, 1));
    std::this_thread::sleep_for(std::chrono::seconds(2));

    AddLog(targetInstance, "Applying NXRTH strict configurations (640x480, DPI 100, Root, DirectX)...", ImVec4(1, 1, 0, 1));
    std::string vmStr = std::to_string(actualVm);

    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " is_customed_resolution 1");
    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " resolution_width 640");
    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " resolution_height 480");
    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " vbox_dpi 100");
    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " root 1");
    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " vbox_graph_mode 1"); // 1 = DirectX, 0 = OpenGL


    int calculatedPort = 21503 + (actualVm * 10);
    strncpy(g_Bots[targetInstance].adbSerial, ("127.0.0.1:" + std::to_string(calculatedPort)).c_str(), 64);
    std::string vmNameStr = (actualVm == 0) ? "MEmu" : "MEmu_" + std::to_string(actualVm);
    strncpy(g_Bots[targetInstance].vmName, vmNameStr.c_str(), 64);


    AddLog(targetInstance, " Starting Emulator Engine...", ImVec4(0, 1, 0, 1));
    ExecuteMEmuCommandHidden("start -i " + vmStr);


    if (didShift) {
        std::string msg = " SMART SHIFT ALERT\n\nYou clicked 'Create' on Instance " + std::to_string(requestedInstance + 1) +
            ", but MEmu actually created VM " + std::to_string(actualVm) +
            ".\n\nDon't worry! NXRTH automatically linked it and moved you to the correct tab.";
        MessageBoxA(NULL, msg.c_str(), "NXRTH - Auto Linker", MB_ICONINFORMATION | MB_TOPMOST);
    }
    else {
        std::string msg = " MEmu created successfully!\n\nTarget: VM " + std::to_string(actualVm) +
            "\nConfig: 640x480, 100 DPI, Root, DirectX.\n\nThe emulator is booting up now.";
        MessageBoxA(NULL, msg.c_str(), "NXRTH - Emulator Factory", MB_ICONINFORMATION | MB_TOPMOST);
    }

    g_Bots[requestedInstance].isCreatingEmulator = false;
}
