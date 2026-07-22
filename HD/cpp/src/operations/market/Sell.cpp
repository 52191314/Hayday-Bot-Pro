#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#include "Sell.h"

#include "BotEngine.h"
#include "Discord.h"
#include "Language.h"
#include "State.h"
#include "cpp/src/bot/ScreenRecovery.h"
#include "cpp/src/infra/ActionDebugLog.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <thread>
#include <windows.h>

namespace fs = std::filesystem;

extern TemplateThresholds g_Thresholds;
extern IntervalSettings g_Intervals;
extern BotInstance g_Bots[6];
extern CoordinateProfile g_CoordinateProfile;

extern std::string kAdbPath;
extern bool g_EnableBarnWebhook;
extern bool g_OnlySellIfSiloFull;
extern bool g_EnableWebhookImage;

extern std::string shop_templatePath;
extern std::string wheatshop_templatePath;
extern std::string soldcrate_templatePath;
extern std::string crate_templatePath;
extern std::string plus_templatePath;
extern std::string cross_templatePath;
extern std::string market_close_crosstemplatePath;
extern std::string advertise_templatePath;
extern std::string occupied_crate_advertise_templatePath;
extern std::string create_sale_templatePath;
extern std::string max_price_templatePath;
extern std::string create_ad_templatePath;
extern std::string occupied_crate_create_ad_templatePath;
extern std::string barn_market_templatePath;
extern std::string silo_market_templatePath;
extern std::string bolt_material_templatePath;
extern std::string tape_material_templatePath;
extern std::string plank_material_templatePath;
extern std::string nail_material_templatePath;
extern std::string screw_material_templatePath;
extern std::string panel_material_templatePath;
extern std::string dynamite_templatePath;
extern std::string tnt_templatePath;
extern std::string axe_templatePath;
extern std::string saw_templatePath;
extern std::string shovel_templatePath;

extern void AddLog(int instanceId, std::string message, ImVec4 color);
extern void SaveConfig();
extern void SaveInventoryData();
extern std::string GetAppDataPath();

void AddSaleLog(int instanceId, int level, const std::string& code, const std::string& message, ImVec4 color);
void AddRecoveryLog(int instanceId, int level, const std::string& code, const std::string& message, ImVec4 color);
void LogSaleAction(int instanceId, const std::string& code, const std::string& action, const std::string& detail = "");
void LogSaleResult(int instanceId, const std::string& code, const std::string& action, bool success, const std::string& detail = "", int failureLevel = 2);
void SetSalesFlowState(int instanceId, AccountSlot& account, SalesFlowState state, const std::string& detail = "", bool forceLog = false);
MatchResult FindBestCrossAny(const cv::Mat& screen);

static constexpr int kSaleQuantityControlSpacingX = 113;
static constexpr int kMaxSaleQuantityPlusTaps = 3;
static constexpr int kRetryOpenShop = 3;
static constexpr int kRetryShopIconSearch = 5;
static constexpr int kRetryCloseSalePanel = 2;
static constexpr int kRetryCloseMarket = 3;
static constexpr int kRetryNoProductCloseStack = 3;
static constexpr int kMaxCrateViewMismatchBeforeReload = 3;

static POINT GetOccupiedCreateAdFallbackPoint(const cv::Mat& screen) {
    int width = screen.empty() ? 640 : screen.cols;
    int height = screen.empty() ? 480 : screen.rows;
    return POINT{
        static_cast<LONG>(width * 0.49),
        static_cast<LONG>(height * 0.605)
    };
}

static std::string FormatProbeScore(double score) {
    return FormatMarketScore(score);
}

const char* GetSaleStackModeName(int mode) {
    switch (mode) {
    case kSaleStackModeGameDefault: return "Game Default";
    case kSaleStackModeMax: return "Max Stack";
    case kSaleStackModeFixed: return "Fixed Stack";
    case kSaleStackModePreserveOcr: return "Preserve OCR";
    default: return "Unknown";
    }
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
        if (IsOccupiedCrateEditPanelVisible(verifyScreen) || IsSalePanelVisibleForCrop(verifyScreen, cropMode)) return true;
        if (!IsMarketViewLikelyVisible(verifyScreen, cropMode)) {
            AddSaleLog(instanceId, 2, "SALE_OCCUPIED_FALLBACK_LEFT_MARKET",
                "Fallback occupied-crate probe left the shop view instead of opening an ad/edit panel. Aborting occupied-crate fallback. signals=" + DescribeShopSignals(verifyScreen, cropMode),
                ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
            return false;
        }
    }
    return false;
}

static std::string SaveUiEvidenceArtifact(int instanceId, const std::string& code, const cv::Mat& screen, const std::string& detail);

static bool TryDismissShopOpenBlocker(int instanceId, AccountSlot& account, int cropMode, const char* contextLabel) {
    BotInstance& bot = g_Bots[instanceId];
    if (!bot.isRunning) return false;

    cv::Mat before = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    if (before.empty()) return false;
    if (IsRoadsideShopCrateViewVisible(before, cropMode)) return false;

    MatchResult shopIcon = FindShopIconPreferCache(before, account);
    if (!shopIcon.found) {
        shopIcon = FindImage(before, shop_templatePath, (std::max)(0.68f, g_Thresholds.shopThreshold - 0.10f), true, 1.0f, false, false);
    }
    if (!shopIcon.found) return false;

    bool dismissedKnown = DismissStartupPopupsIfVisible(instanceId, contextLabel, 1);
    bool dismissedUnknown = false;
    if (!dismissedKnown) {
        dismissedUnknown = DismissUnknownOverlay(instanceId, contextLabel);
    }
    if (!dismissedKnown && !dismissedUnknown) return false;

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    cv::Mat after = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    if (after.empty()) return true;

    bool marketVisibleAfter = IsRoadsideShopCrateViewVisible(after, cropMode);
    bool shopIconAfter = FindShopIconPreferCache(after, account).found;
    std::string mode = dismissedKnown ? "known_popup" : "unknown_overlay";
    AddSaleLog(instanceId, 2, "SALE_SHOP_BLOCKER_DISMISSED",
        std::string("context=") + contextLabel +
        " mode=" + mode +
        " market_after=" + std::to_string(marketVisibleAfter ? 1 : 0) +
        " shop_icon_after=" + std::to_string(shopIconAfter ? 1 : 0) +
        " signals_after=" + DescribeShopSignals(after, cropMode, &account),
        ImVec4(1.0f, 0.85f, 0.25f, 1.0f));
    return true;
}

static bool EnsureSalePanelClosed(int instanceId, int cropMode, const char* contextLabel, int maxAttempts = kRetryCloseSalePanel, bool forceFirstClose = false) {
    BotInstance& bot = g_Bots[instanceId];
    const int coordinateSettleMs = 420;
    const int templateSettleMs = 520;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (!bot.isRunning) return false;
        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (!forceFirstClose && !IsSalePanelVisibleForCrop(screen, cropMode)) return true;
        forceFirstClose = false;

        if (attempt > 1) {
            AddRecoveryLog(instanceId, 2, "SALE_PANEL_CLOSE_RETRY", std::string("context=") + contextLabel + " attempt=" + std::to_string(attempt) + "/" + std::to_string(maxAttempts), ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
        }
        else {
            LogSaleAction(instanceId, "SALE_PANEL_CLOSE_ACTION", "close_sale_panel", std::string("context=") + contextLabel);
        }

        AdbTap(instanceId, g_CoordinateProfile.salePanelCloseX, g_CoordinateProfile.salePanelCloseY);
        std::this_thread::sleep_for(std::chrono::milliseconds(coordinateSettleMs));

        cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (!IsSalePanelVisibleForCrop(verifyScreen, cropMode)) {
            LogSaleResult(instanceId, "SALE_PANEL_CLOSE_RESULT", "close_sale_panel", true, std::string("context=") + contextLabel + " attempt=" + std::to_string(attempt), 1);
            return true;
        }

        MatchResult closeRes = FindBestCrossAny(verifyScreen);
        if (closeRes.found) {
            AddRecoveryLog(instanceId, 2, "SALE_PANEL_CLOSE_FALLBACK", std::string("context=") + contextLabel + " | coordinate missed, detected cross x=" + std::to_string(closeRes.x) + " y=" + std::to_string(closeRes.y), ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
            AdbTap(instanceId, closeRes.x, closeRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(templateSettleMs));
        }
    }
    cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    bool closed = !IsSalePanelVisibleForCrop(verifyScreen, cropMode);
    if (!closed) {
        std::string evidence = SaveUiEvidenceArtifact(instanceId, "PANEL_STUCK_PRODUCT", verifyScreen, std::string("context=") + contextLabel);
        AddRecoveryLog(instanceId, 3, "PANEL_STUCK_PRODUCT", std::string("context=") + contextLabel + " duration_attempts=" + std::to_string(maxAttempts) + " evidence=" + evidence, ImVec4(1.0f, 0.25f, 0.2f, 1.0f));
    }
    LogSaleResult(instanceId, "SALE_PANEL_CLOSE_RESULT", "close_sale_panel", closed, std::string("context=") + contextLabel, closed ? 1 : 3);
    return closed;
}

static bool EnsureMarketClosed(int instanceId, int cropMode, const char* contextLabel, int maxAttempts = kRetryCloseMarket) {
    BotInstance& bot = g_Bots[instanceId];
    const int coordinateSettleMs = 450;
    const int templateSettleMs = 560;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (!bot.isRunning) return false;
        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (!IsMarketViewVisible(screen, cropMode)) return true;

        if (attempt > 1) {
            AddRecoveryLog(instanceId, 2, "MARKET_CLOSE_RETRY", std::string("context=") + contextLabel + " attempt=" + std::to_string(attempt) + "/" + std::to_string(maxAttempts), ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
        }
        else {
            LogSaleAction(instanceId, "MARKET_CLOSE_ACTION", "close_market", std::string("context=") + contextLabel);
        }

        AdbTap(instanceId, g_CoordinateProfile.marketCloseX, g_CoordinateProfile.marketCloseY);
        std::this_thread::sleep_for(std::chrono::milliseconds(coordinateSettleMs));

        cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (!IsMarketViewVisible(verifyScreen, cropMode)) {
            LogSaleResult(instanceId, "MARKET_CLOSE_RESULT", "close_market", true, std::string("context=") + contextLabel + " attempt=" + std::to_string(attempt), 1);
            return true;
        }

        AdbTap(instanceId, g_CoordinateProfile.marketCloseSecondX, g_CoordinateProfile.marketCloseSecondY);
        std::this_thread::sleep_for(std::chrono::milliseconds(coordinateSettleMs));

        verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (!IsMarketViewVisible(verifyScreen, cropMode)) {
            LogSaleResult(instanceId, "MARKET_CLOSE_RESULT", "close_market", true, std::string("context=") + contextLabel + " attempt=" + std::to_string(attempt) + " second_coordinate=1", 1);
            return true;
        }

        MatchResult closeRes = FindImage(verifyScreen, market_close_crosstemplatePath, std::max(0.65f, g_Thresholds.marketCloseCrossThreshold), false, 1.0f, false);
        if (!closeRes.found) closeRes = FindBestCrossAny(verifyScreen);
        if (closeRes.found) {
            AddRecoveryLog(instanceId, 2, "MARKET_CLOSE_FALLBACK", std::string("context=") + contextLabel + " | coordinate missed, detected cross x=" + std::to_string(closeRes.x) + " y=" + std::to_string(closeRes.y), ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
            AdbTap(instanceId, closeRes.x, closeRes.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(templateSettleMs));
        }
    }
    cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    bool closed = !IsMarketViewVisible(verifyScreen, cropMode);
    if (!closed) {
        std::string evidence = SaveUiEvidenceArtifact(instanceId, "PANEL_STUCK_MARKET", verifyScreen, std::string("context=") + contextLabel + " signals=" + DescribeShopSignals(verifyScreen, cropMode));
        AddRecoveryLog(instanceId, 3, "PANEL_STUCK_MARKET", std::string("context=") + contextLabel + " duration_attempts=" + std::to_string(maxAttempts) + " evidence=" + evidence, ImVec4(1.0f, 0.25f, 0.2f, 1.0f));
    }
    LogSaleResult(instanceId, "MARKET_CLOSE_RESULT", "close_market", closed, std::string("context=") + contextLabel, closed ? 1 : 3);
    return closed;
}

static bool ClosePanelStackToFarm(int instanceId, int cropMode, const char* contextLabel, int maxAttempts = kRetryNoProductCloseStack) {
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
    AddRecoveryLog(instanceId, 1, "PANEL_STACK_CLOSE_START",
        std::string("context=") + contextLabel + " snapshot=" + DescribeMarketSignalSnapshot(screen, cropMode, nullptr),
        ImVec4(0.65f, 0.85f, 1.0f, 1.0f));

    if (IsBarnSalePanelVisible(screen) || IsSalePanelVisibleForCrop(screen, cropMode)) {
        if (!EnsureSalePanelClosed(instanceId, cropMode, contextLabel, maxAttempts, true)) return false;
    }
    if (!EnsureMarketClosed(instanceId, cropMode, contextLabel, maxAttempts)) return false;

    screen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
    AddRecoveryLog(instanceId, 1, "PANEL_STACK_CLOSE_DONE",
        std::string("context=") + contextLabel + " snapshot=" + DescribeMarketSignalSnapshot(screen, cropMode, nullptr),
        ImVec4(0.45f, 0.95f, 0.55f, 1.0f));
    return true;
}

static std::string SanitizeDebugLabel(std::string text) {
    for (char& c : text) {
        if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
    }
    return text;
}

std::string SavePreserveDebugArtifacts(int instanceId, const std::string& reasonTag, const ItemCountDebugInfo& info, const cv::Mat& fullScreen, bool ok) {
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

static std::string SaveUiEvidenceArtifact(int instanceId, const std::string& code, const cv::Mat& screen, const std::string& detail) {
    std::string debugDir = GetAppDataPath() + "\\ui_debug";
    std::error_code ec;
    fs::create_directories(debugDir, ec);

    auto now = std::chrono::system_clock::now().time_since_epoch();
    long long stamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::string baseName = "inst" + std::to_string(instanceId + 1) + "_" + std::to_string(stamp) + "_" + SanitizeDebugLabel(code);
    std::string basePath = debugDir + "\\" + baseName;

    if (!screen.empty()) {
        cv::imwrite(basePath + "_screen.png", screen);
        cv::Rect dialogRect = BuildSaleDialogSearchRect(screen);
        if (dialogRect.width > 0 && dialogRect.height > 0) {
            cv::imwrite(basePath + "_dialog_roi.png", screen(dialogRect));
        }
        cv::Rect marketRect(0, 0, std::max(1, static_cast<int>(screen.cols * 0.62)), screen.rows);
        marketRect &= cv::Rect(0, 0, screen.cols, screen.rows);
        if (marketRect.width > 0 && marketRect.height > 0) {
            cv::imwrite(basePath + "_market_roi.png", screen(marketRect));
        }
    }

    std::ofstream meta(basePath + "_meta.txt");
    if (meta.is_open()) {
        meta << "code=" << code << "\n";
        meta << "detail=" << detail << "\n";
        meta << "screen_empty=" << (screen.empty() ? 1 : 0) << "\n";
        if (!screen.empty()) {
            meta << "screen=" << screen.cols << "x" << screen.rows << "\n";
            meta << "snapshot=" << DescribeMarketSignalSnapshot(screen, 0, nullptr) << "\n";
        }
    }

    return basePath;
}

enum class SalesLoopDirective {
    None,
    Continue,
    Break,
    Abort
};

struct SalesCycleStats {
    int listed = 0;
    int ads = 0;
    int soldCollected = 0;
    int emptyCratesSeen = 0;
    int crateScans = 0;
    int shopOpenAttempts = 0;
    int shopOpenRetries = 0;
    int panelCloseAttempts = 0;
    bool occupiedFlowUsed = false;
    bool noProductSeen = false;
    bool forcedReopenUsed = false;
    std::string exitReason = "running";
};

static std::string BuildSalesSummaryDetail(const SalesCycleStats& stats) {
    return "listed=" + std::to_string(stats.listed) +
        " ads=" + std::to_string(stats.ads) +
        " sold_collected=" + std::to_string(stats.soldCollected) +
        " empty_seen=" + std::to_string(stats.emptyCratesSeen) +
        " crate_scans=" + std::to_string(stats.crateScans) +
        " shop_attempts=" + std::to_string(stats.shopOpenAttempts) +
        " shop_retries=" + std::to_string(stats.shopOpenRetries) +
        " occupied_flow=" + std::string(stats.occupiedFlowUsed ? "1" : "0") +
        " forced_reopen=" + std::string(stats.forcedReopenUsed ? "1" : "0") +
        " no_product=" + std::string(stats.noProductSeen ? "1" : "0") +
        " exit=" + stats.exitReason;
}

struct SalesCycleSummaryGuard {
    int instanceId = -1;
    SalesCycleStats& stats;
    bool active = true;

    ~SalesCycleSummaryGuard() {
        if (!active) return;
        if (stats.exitReason == "running") stats.exitReason = "scope_exit";
        AddSaleLog(instanceId, stats.exitReason == "normal" ? 1 : 2, "SALE_SUMMARY", BuildSalesSummaryDetail(stats), ImVec4(0.65f, 0.9f, 1.0f, 1.0f));
    }
};

static long long SecondsUntilAdReady(const AccountSlot& account) {
    auto now = std::chrono::steady_clock::now();
    auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - account.lastAdTime).count();
    return (std::max)(0LL, 300LL - elapsedSeconds);
}

static bool IsAdCooldownReady(const AccountSlot& account) {
    return SecondsUntilAdReady(account) == 0;
}

static bool OpenShopForSales(int instanceId, AccountSlot& account, int cropMode, const std::function<bool(int)>& smartSleep, SalesCycleStats* stats = nullptr) {
    BotInstance& bot = g_Bots[instanceId];
    const int shopSearchWaitMs = (std::max)(250, (std::min)(700, g_Intervals.shopEnterWait / 3));
    const int shopRetryWaitMs = (std::max)(350, (std::min)(900, g_Intervals.shopEnterWait / 2));

    MatchResult shopRes{};
    bool shopOpened = false;

    for (int tryCount = 1; tryCount <= kRetryOpenShop; tryCount++) {
        if (!bot.isRunning) return false;
        if (stats) {
            stats->shopOpenAttempts++;
            if (tryCount > 1) stats->shopOpenRetries++;
        }
        LogSaleAction(instanceId, "SALE_OPEN_SHOP_ACTION", "open_shop", "attempt=" + std::to_string(tryCount) + "/" + std::to_string(kRetryOpenShop));

        bool shopFound = false;
        bool alreadyInMarket = false;
        for (int k = 1; k <= kRetryShopIconSearch; k++) {
            if (!bot.isRunning) return false;
            cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            alreadyInMarket = IsRoadsideShopCrateViewVisible(screen, cropMode);
            if (alreadyInMarket) {
                std::string signals = DescribeShopSignals(screen, cropMode, &account);
                AddSaleLog(instanceId, 1, "SALE_SHOP_ALREADY_OPEN", "Shop-open evidence detected before tapping shop. signals=" + signals, ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
                LogSaleResult(instanceId, "SALE_OPEN_SHOP_RESULT", "open_shop", true, "source=precheck signals=" + signals);
                shopFound = true;
                shopOpened = true;
                break;
            }
            if (IsBarnSalePanelVisible(screen)) {
                AddSaleLog(instanceId, 2, "SALE_PRODUCT_PANEL_BEFORE_SHOP", "Sale/product panel is visible before Roadside_Shop was verified. Closing it and rechecking the shop. signals=" + DescribeShopSignals(screen, cropMode, &account), ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                if (!EnsureSalePanelClosed(instanceId, cropMode, "shop-open precheck", 2, true)) return false;
                if (!smartSleep(250)) return false;
                continue;
            }
            shopRes = FindShopIconPreferCache(screen, account);
            if (shopRes.found) {
                shopFound = true;
                break;
            }
            AddSaleLog(instanceId, 1, "SALE_SHOP_SEARCHING", std::string(Tr("Searching Shop... ")) + std::to_string(k) + "/" + std::to_string(kRetryShopIconSearch), ImVec4(1, 1, 0, 1));
            if (!smartSleep(shopSearchWaitMs)) return false;
        }

        if (!shopFound) {
            if (TryDismissShopOpenBlocker(instanceId, account, cropMode, "shop icon search failed")) {
                if (!smartSleep(220)) return false;
                continue;
            }
            AddSaleLog(instanceId, 2, "SALE_SHOP_ICON_NOT_FOUND", std::string(Tr("Shop NOT found. Retrying... (")) + std::to_string(tryCount) + "/" + std::to_string(kRetryOpenShop) + ")", ImVec4(1, 0.5f, 0, 1));
            if (!smartSleep(shopRetryWaitMs)) return false;
            continue;
        }

        if (shopOpened) {
            AddSaleLog(instanceId, 1, "SALE_SHOP_VERIFIED", "Roadside shop already appears to be open. Continuing with crate scan.", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
            return true;
        }

        AdbTap(instanceId, shopRes.x, shopRes.y);
        if (!smartSleep(g_Intervals.shopEnterWait)) return false;

        cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (IsRoadsideShopCrateViewVisible(verifyScreen, cropMode)) {
            std::string signals = DescribeShopSignals(verifyScreen, cropMode, &account);
            AddSaleLog(instanceId, 1, "SALE_SHOP_VERIFIED", "Shop opened. Scanning crates. signals=" + signals, ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
            LogSaleResult(instanceId, "SALE_OPEN_SHOP_RESULT", "open_shop", true, "source=tap signals=" + signals);
            return true;
        }
        if (IsBarnSalePanelVisible(verifyScreen)) {
            AddSaleLog(instanceId, 2, "SALE_SHOP_TAP_PRODUCT_PANEL", "Shop tap led to the product panel, but Roadside_Shop was not verified yet. Closing the panel and retrying shop confirmation. signals=" + DescribeShopSignals(verifyScreen, cropMode, &account), ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
            if (!EnsureSalePanelClosed(instanceId, cropMode, "shop-open verify recovery", 2, true)) return false;
            if (!smartSleep(250)) return false;
            cv::Mat recoveredScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            if (IsRoadsideShopCrateViewVisible(recoveredScreen, cropMode)) {
                std::string signals = DescribeShopSignals(recoveredScreen, cropMode, &account);
                AddSaleLog(instanceId, 1, "SALE_SHOP_VERIFIED_AFTER_PANEL_CLOSE", "Roadside shop became visible after closing the product panel. signals=" + signals, ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
                LogSaleResult(instanceId, "SALE_OPEN_SHOP_RESULT", "open_shop", true, "source=product_panel_recovery signals=" + signals);
                return true;
            }
        }

        MatchResult refreshedShopRes = FindShopIconPreferCache(verifyScreen, account);
        if (refreshedShopRes.found) {
            AddSaleLog(instanceId, 2, "SALE_SHOP_FARM_RETAP", "Shop verification failed but shop.png is still visible on farm. Retapping fresh shop target x=" + std::to_string(refreshedShopRes.x) + " y=" + std::to_string(refreshedShopRes.y) + " score=" + FormatProbeScore(refreshedShopRes.score) + " signals=" + DescribeShopSignals(verifyScreen, cropMode, &account), ImVec4(1.0f, 0.8f, 0.25f, 1.0f));
            AdbTap(instanceId, refreshedShopRes.x, refreshedShopRes.y);
            if (!smartSleep(g_Intervals.shopEnterWait)) return false;

            cv::Mat retapVerifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            if (IsRoadsideShopCrateViewVisible(retapVerifyScreen, cropMode)) {
                std::string signals = DescribeShopSignals(retapVerifyScreen, cropMode, &account);
                AddSaleLog(instanceId, 1, "SALE_SHOP_VERIFIED_AFTER_RETAP", "Shop opened after fresh farm-view retap. signals=" + signals, ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
                LogSaleResult(instanceId, "SALE_OPEN_SHOP_RESULT", "open_shop", true, "source=farm_retap signals=" + signals);
                return true;
            }
            if (IsBarnSalePanelVisible(retapVerifyScreen)) {
                AddSaleLog(instanceId, 2, "SALE_SHOP_RETAP_PRODUCT_PANEL", "Fresh shop retap led to product panel. Closing it and retrying normal shop open. signals=" + DescribeShopSignals(retapVerifyScreen, cropMode, &account), ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                if (!EnsureSalePanelClosed(instanceId, cropMode, "shop-open retap recovery", 2, true)) return false;
                if (!smartSleep(250)) return false;
            }
            else {
                AddSaleLog(instanceId, 2, "SALE_SHOP_RETAP_UNVERIFIED", "Fresh shop retap did not verify Roadside_Shop yet. Retrying outer open loop. signals=" + DescribeShopSignals(retapVerifyScreen, cropMode, &account), ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                if (TryDismissShopOpenBlocker(instanceId, account, cropMode, "shop retap unverified")) {
                    if (!smartSleep(220)) return false;
                } else {
                    if (!smartSleep(220)) return false;
                }
            }
            continue;
        }

        if (!FindImage(verifyScreen, shop_templatePath, g_Thresholds.shopThreshold, false, 1.0f, false).found) {
            AddSaleLog(instanceId, 2, "SALE_SHOP_VERIFY_FAILED_ICON_GONE", "Shop icon disappeared after tap, but market UI was not verified. Retrying shop open. signals=" + DescribeShopSignals(verifyScreen, cropMode, &account), ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
            if (TryDismissShopOpenBlocker(instanceId, account, cropMode, "shop verify failed icon gone")) {
                if (!smartSleep(220)) return false;
                continue;
            }
            if (!smartSleep(shopRetryWaitMs)) return false;
        }
        else {
            AddSaleLog(instanceId, 2, "SALE_SHOP_CLICK_MISSED", std::string(Tr("Shop click missed (Bug)! Retrying... (")) + std::to_string(tryCount) + "/" + std::to_string(kRetryOpenShop) + ") | signals=" + DescribeShopSignals(verifyScreen, cropMode, &account), ImVec4(1, 0.5f, 0, 1));
            if (TryDismissShopOpenBlocker(instanceId, account, cropMode, "shop click missed")) {
                if (!smartSleep(220)) return false;
                continue;
            }
        }
    }

    cv::Mat failScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    std::string signals = DescribeShopSignals(failScreen, cropMode, &account);
    std::string evidence = SaveUiEvidenceArtifact(instanceId, "SALE_SHOP_VERIFY_FAILED", failScreen, "signals=" + signals);
    AddSaleLog(instanceId, 3, "SALE_SHOP_VERIFY_FAILED", "Unable to verify Roadside_Shop/crate view after retries. signals=" + signals + " evidence=" + evidence, ImVec4(1.0f, 0.25f, 0.2f, 1.0f));
    LogSaleResult(instanceId, "SALE_OPEN_SHOP_RESULT", "open_shop", false, "reason=verification_failed signals=" + signals + " evidence=" + evidence, 3);
    return false;
}

static SalesLoopDirective HandleNoCollectableCrateBreak(int instanceId,
    AccountSlot& account,
    int cropMode,
    const std::function<bool(int)>& smartSleep,
    bool& forcedShopReopenPassUsed,
    bool& noCollectableCrateConfirmedThisCycle,
    bool noCropConfirmedThisCycle,
    int salesCount,
    SalesCycleStats& stats) {
    if (!forcedShopReopenPassUsed) {
        forcedShopReopenPassUsed = true;
        stats.forcedReopenUsed = true;
        AddSaleLog(instanceId, 2, "SALE_NO_COLLECTABLE_RESCAN", "No collectable crate found. Forcing one shop reopen + rescan pass before ending sales cycle.", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
        if (!EnsureMarketClosed(instanceId, cropMode, "forced sales rescan")) return SalesLoopDirective::Abort;
        if (!smartSleep(350)) return SalesLoopDirective::Abort;
        if (!OpenShopForSales(instanceId, account, cropMode, smartSleep, &stats)) {
            AddSaleLog(instanceId, 3, "SALE_FORCED_REOPEN_FAILED", "Forced shop reopen failed. Ending this sales pass so the bot can move on cleanly.", ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
            return SalesLoopDirective::Break;
        }
        noCollectableCrateConfirmedThisCycle = false;
        return SalesLoopDirective::Continue;
    }
    if (noCropConfirmedThisCycle || salesCount > 0) return SalesLoopDirective::Break;
    if (noCollectableCrateConfirmedThisCycle) {
        AddSaleLog(instanceId, 2, "SALE_NO_COLLECTABLE_ENDING",
            "No collectable crate remains after forced rescan and crop status is still uncertain. Ending sales pass to avoid an infinite shop loop.",
            ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
        return SalesLoopDirective::Break;
    }
    noCollectableCrateConfirmedThisCycle = true;
    AddSaleLog(instanceId, 2, "SALE_NO_COLLECTABLE_UNCONFIRMED_CROP", "Collectable crates were not confirmed, but crop availability was not confirmed yet. Keeping cycle alive for one more check.", ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
    return SalesLoopDirective::Continue;
}

static SalesLoopDirective CollectSoldCratesIfAny(int instanceId, AccountSlot& account, bool wasInQuarantine, const std::function<bool(int)>& smartSleep, SalesCycleStats& stats) {
    BotInstance& bot = g_Bots[instanceId];
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    std::vector<MatchResult> soldCrates = FindAllImages(screen, soldcrate_templatePath, 0.80f);
    if (soldCrates.empty()) return SalesLoopDirective::None;
    stats.soldCollected += static_cast<int>(soldCrates.size());

    SetSalesFlowState(instanceId, account, SalesFlowState::CollectingSoldCrates, "Collecting sold crates before listing anything else");
    account.isShopFullStuck = false;

    if (wasInQuarantine) {
        AddSaleLog(instanceId, 1, "SALE_QUARANTINE_LIFTED_SOLD_CRATE", "Quarantine lifted via sold crates. Skipping coin collect to save time. Returning to farm.", ImVec4(0, 1, 0, 1));
        return SalesLoopDirective::Break;
    }

    AddSaleLog(instanceId, 1, "SALE_COLLECTING_SOLD_CRATES", std::string(Tr("Collecting coins...")) + " count=" + std::to_string(soldCrates.size()), ImVec4(0, 1, 0, 1));
    for (const auto& crate : soldCrates) {
        if (!bot.isRunning) return SalesLoopDirective::Abort;
        AdbTap(instanceId, crate.x, crate.y);
        if (!smartSleep(300)) return SalesLoopDirective::Abort;
    }
    if (!smartSleep(g_Intervals.coinCollectWait)) return SalesLoopDirective::Abort;
    return SalesLoopDirective::Continue;
}

static SalesLoopDirective ExitSalesPassUntilAdReady(int instanceId, AccountSlot& account, SalesCycleStats& stats) {
    long long remainingSeconds = SecondsUntilAdReady(account);
    account.isShopFullStuck = true;
    account.lastShopCheckTime = std::chrono::steady_clock::now();
    SetSalesFlowState(instanceId, account, SalesFlowState::Quarantined, "Shop full and advertisement is not ready; leaving sales");
    stats.exitReason = "ad_cooldown_no_empty_crate";
    AddSaleLog(instanceId, 2, "SALE_AD_COOLDOWN_EXIT",
        "No empty/sold crate is available and the advertisement cooldown has " + std::to_string(remainingSeconds) +
        "s remaining. Marking the account as shop-full quarantined so farming or account switching can continue.",
        ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    return SalesLoopDirective::Break;
}

static bool SubmitSaleListing(int instanceId, int cropMode, AccountSlot& account, const std::function<bool(int)>& smartSleep, bool& saleSubmitted) {
    BotInstance& bot = g_Bots[instanceId];
    const int saleConfirmProbeWaitMs = (std::max)(220, (std::min)(450, g_Intervals.createSaleWait));
    auto saleListingConfirmed = [&]() -> bool {
        cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        return !IsSalePanelVisibleForCrop(verifyScreen, cropMode) &&
            IsRoadsideShopCrateViewVisible(verifyScreen, cropMode);
    };

    saleSubmitted = false;
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    bool listingControlsVisible = IsSaleListingControlsVisible(screen);
    if (!listingControlsVisible) {
        if (IsSaleProductPickerVisible(screen, cropMode)) {
            std::string evidence = SaveUiEvidenceArtifact(instanceId, "SALE_CONFIRM_BLOCKED_PRODUCT_PICKER", screen, "signals=" + DescribeShopSignals(screen, cropMode, &account));
            AddSaleLog(instanceId, 3, "SALE_CONFIRM_BLOCKED_PRODUCT_PICKER", "Still on the crop picker before sale action. Aborting sale step. evidence=" + evidence, ImVec4(1, 0, 0, 1));
            account.cachedSaleProductX = -1;
            account.cachedSaleProductY = -1;
            EnsureMarketClosed(instanceId, cropMode, "sale safety abort");
            return false;
        }
        AddSaleLog(instanceId, 2, "SALE_LISTING_CONTROLS_MISSING", "Sale listing controls were not detected before sale action. Using configured Put on Sale coordinate fallback.", ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
    }

    LogSaleAction(instanceId, "SALE_CONFIRM_ACTION", "put_on_sale", "source=configured_coordinate");
    AdbTap(instanceId, g_CoordinateProfile.putOnSaleX, g_CoordinateProfile.putOnSaleY);
    if (!smartSleep(saleConfirmProbeWaitMs)) return false;
    if (saleListingConfirmed()) {
        saleSubmitted = true;
        LogSaleResult(instanceId, "SALE_CONFIRM_RESULT", "put_on_sale", true, "source=configured_coordinate");
        return true;
    }

    screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    MatchResult saleBtn = FindCreateSaleButtonRelaxed(screen);
    if (saleBtn.found) {
        AddSaleLog(instanceId, 2, "SALE_CONFIRM_COORDINATE_MISSED", "Put on Sale coordinate missed. Using detected sale button at x=" + std::to_string(saleBtn.x) + " y=" + std::to_string(saleBtn.y), ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
        LogSaleAction(instanceId, "SALE_CONFIRM_ACTION", "put_on_sale", "source=detected_template x=" + std::to_string(saleBtn.x) + " y=" + std::to_string(saleBtn.y));
        AdbTap(instanceId, saleBtn.x, saleBtn.y);
        if (!smartSleep(saleConfirmProbeWaitMs)) return false;
        saleSubmitted = saleListingConfirmed();
        LogSaleResult(instanceId, "SALE_CONFIRM_RESULT", "put_on_sale", saleSubmitted, "source=detected_template", saleSubmitted ? 1 : 2);
        return true;
    }

    AddSaleLog(instanceId, 2, "SALE_CONFIRM_TEMPLATE_MISSING", Tr("Put on Sale button template was not visible after the coordinate tap. Retrying custom x,y coordinates."), ImVec4(1, 0, 0, 1));
    LogSaleAction(instanceId, "SALE_CONFIRM_ACTION", "put_on_sale", "source=configured_coordinate_retry");
    AdbTap(instanceId, g_CoordinateProfile.putOnSaleX, g_CoordinateProfile.putOnSaleY);
    if (!smartSleep(saleConfirmProbeWaitMs)) return false;
    saleSubmitted = saleListingConfirmed();
    LogSaleResult(instanceId, "SALE_CONFIRM_RESULT", "put_on_sale", saleSubmitted, "source=configured_coordinate_retry", saleSubmitted ? 1 : 2);
    return true;
}

static SalesLoopDirective OpenSalePanelFromEmptyCrate(int instanceId, int cropMode, AccountSlot& account, const MatchResult& emptyCrate, const std::function<bool(int)>& smartSleep) {
    BotInstance& bot = g_Bots[instanceId];
    LogSaleAction(instanceId, "SALE_EMPTY_CRATE_OPEN_ACTION", "open_empty_crate", "x=" + std::to_string(emptyCrate.x) + " y=" + std::to_string(emptyCrate.y) + " score=" + FormatProbeScore(emptyCrate.score));
    AdbTap(instanceId, emptyCrate.x, emptyCrate.y);
    if (!smartSleep(g_Intervals.crateClickWait)) return SalesLoopDirective::Abort;

    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    if (!IsSalePanelVisibleForCrop(screen, cropMode)) {
        std::string signals = DescribeShopSignals(screen, cropMode, &account);
        std::string evidence = SaveUiEvidenceArtifact(instanceId, "SALE_PANEL_OPEN_FAILED", screen, "empty_crate_x=" + std::to_string(emptyCrate.x) + " empty_crate_y=" + std::to_string(emptyCrate.y) + " signals=" + signals);
        AddSaleLog(instanceId, 3, "SALE_PANEL_OPEN_FAILED", "Empty crate was tapped, but sale panel did not open. Clearing cached crate coordinate. signals=" + signals + " evidence=" + evidence, ImVec4(1, 0.4f, 0.2f, 1));
        LogSaleResult(instanceId, "SALE_EMPTY_CRATE_OPEN_RESULT", "open_empty_crate", false, "reason=sale_panel_not_visible evidence=" + evidence, 3);
        account.cachedEmptyCrateX = -1;
        account.cachedEmptyCrateY = -1;
        return SalesLoopDirective::Continue;
    }

    LogSaleResult(instanceId, "SALE_EMPTY_CRATE_OPEN_RESULT", "open_empty_crate", true, "sale_panel_visible=1");
    return SalesLoopDirective::None;
}

static SalesLoopDirective RunOccupiedCrateAdFlow(int instanceId, int accountIndex, AccountSlot& account, int cropMode, bool& adPlacedThisCycle, const std::function<bool(int)>& smartSleep, SalesCycleStats& stats) {
    BotInstance& bot = g_Bots[instanceId];
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    stats.occupiedFlowUsed = true;

    MatchResult filledCrate = FindAnyFilledCrate(screen, cropMode);
    bool occupiedCrateOpened = false;
    if (filledCrate.found) {
        SetSalesFlowState(instanceId, account, SalesFlowState::HandlingOccupiedCrates, "All crates are occupied, opening one to try the ad flow");
        AddSaleLog(instanceId, 1, "SALE_OCCUPIED_FLOW_OPEN_FILLED", "All crates are occupied. Opening a non-empty crate to check advertisement flow. x=" + std::to_string(filledCrate.x) + " y=" + std::to_string(filledCrate.y) + " score=" + FormatProbeScore(filledCrate.score), ImVec4(1, 1, 0, 1));
        LogSaleAction(instanceId, "SALE_OCCUPIED_CRATE_OPEN_ACTION", "open_occupied_crate", "source=template x=" + std::to_string(filledCrate.x) + " y=" + std::to_string(filledCrate.y));
        AdbTap(instanceId, filledCrate.x, filledCrate.y);
        if (!smartSleep(1200)) return SalesLoopDirective::Abort;
        occupiedCrateOpened = true;
    }
    else if (IsRoadsideShopCrateViewVisible(screen, cropMode)) {
        SetSalesFlowState(instanceId, account, SalesFlowState::HandlingOccupiedCrates, "All crates appear occupied, probing fallback crate slots");
        AddSaleLog(instanceId, 2, "SALE_OCCUPIED_FLOW_TEMPLATE_MISSED", "Filled-crate template missed, but the roadside shop is open with no empty crates. Probing fallback occupied-crate slots. signals=" + DescribeShopSignals(screen, cropMode, &account), ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
        LogSaleAction(instanceId, "SALE_OCCUPIED_CRATE_OPEN_ACTION", "open_occupied_crate", "source=fallback_grid");
        occupiedCrateOpened = TryOpenOccupiedCrateFallbackGrid(instanceId, cropMode);
    }

    if (!occupiedCrateOpened) {
        AddSaleLog(instanceId, 3, "SALE_OCCUPIED_CRATE_OPEN_FAILED", "Shop is full, but no occupied crate could be opened. Skipping sales for this pass.", ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
        LogSaleResult(instanceId, "SALE_OCCUPIED_CRATE_OPEN_RESULT", "open_occupied_crate", false, "reason=no_occupied_crate_opened", 3);
        return SalesLoopDirective::Break;
    }
    LogSaleResult(instanceId, "SALE_OCCUPIED_CRATE_OPEN_RESULT", "open_occupied_crate", true);

    cv::Mat editScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    MatchResult advNowRes = FindBestDialogTemplate(editScreen, { occupied_crate_advertise_templatePath, advertise_templatePath }, g_Thresholds.advertiseThreshold, false);
    bool usedAdvertiseCoordinateFallback = false;
    bool usedCreateAdCoordinateFallback = false;

    if (!advNowRes.found) {
        if (!IsOccupiedCrateEditPanelVisible(editScreen)) {
            std::string evidence = SaveUiEvidenceArtifact(instanceId, "SALE_OCCUPIED_EDIT_PANEL_MISSING", editScreen, "signals=" + DescribeShopSignals(editScreen, cropMode, &account));
            AddSaleLog(instanceId, 3, "SALE_OCCUPIED_EDIT_PANEL_MISSING", "Occupied crate panel did not open cleanly. Skipping this sales pass. evidence=" + evidence, ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
            if (!EnsureMarketClosed(instanceId, cropMode, "occupied crate panel missing")) return SalesLoopDirective::Abort;
            return SalesLoopDirective::Break;
        }

        SetSalesFlowState(instanceId, account, SalesFlowState::OpeningAdFlow, "Occupied-crate advertise template missed, using fallback coordinate");
        AddSaleLog(instanceId, 2, "SALE_OCCUPIED_AD_TEMPLATE_MISSED", "Occupied-crate advertise template missed. Trying occupied-crate ad fallback coordinate.", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
        LogSaleAction(instanceId, "SALE_OCCUPIED_AD_OPEN_ACTION", "open_occupied_ad", "source=configured_coordinate");
        AdbTap(instanceId, g_CoordinateProfile.occupiedCrateAdOpenX, g_CoordinateProfile.occupiedCrateAdOpenY);
        usedAdvertiseCoordinateFallback = true;
        if (!smartSleep(900)) return SalesLoopDirective::Abort;
        editScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    }
    else {
        SetSalesFlowState(instanceId, account, SalesFlowState::OpeningAdFlow, "Occupied-crate advertise button found on the opened crate");
        AddSaleLog(instanceId, 1, "SALE_OCCUPIED_AD_TEMPLATE_FOUND", "Occupied-crate advertise button found. Opening the advertisement dialog. x=" + std::to_string(advNowRes.x) + " y=" + std::to_string(advNowRes.y) + " score=" + FormatProbeScore(advNowRes.score), ImVec4(0.9f, 0.9f, 0.3f, 1.0f));
        LogSaleAction(instanceId, "SALE_OCCUPIED_AD_OPEN_ACTION", "open_occupied_ad", "source=template x=" + std::to_string(advNowRes.x) + " y=" + std::to_string(advNowRes.y));
        AdbTap(instanceId, advNowRes.x, advNowRes.y);
        if (!smartSleep(900)) return SalesLoopDirective::Abort;
        editScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    }

    MatchResult createAdRes = FindBestDialogTemplate(editScreen, { occupied_crate_create_ad_templatePath, create_ad_templatePath }, g_Thresholds.createAdThreshold, false);
    if (!createAdRes.found) {
        SetSalesFlowState(instanceId, account, SalesFlowState::PublishingAd, "Occupied-crate Create Ad template missed, using fallback confirm coordinate");
        POINT fallbackPoint = GetOccupiedCreateAdFallbackPoint(editScreen);
        AddSaleLog(instanceId, 2, "SALE_OCCUPIED_CREATE_AD_TEMPLATE_MISSED",
            "Occupied-crate Create Ad template missed. Trying occupied-dialog fallback coordinate x=" +
            std::to_string(fallbackPoint.x) + " y=" + std::to_string(fallbackPoint.y) + ".",
            ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
        LogSaleAction(instanceId, "SALE_OCCUPIED_CREATE_AD_ACTION", "create_occupied_ad",
            "source=occupied_dialog_coordinate x=" + std::to_string(fallbackPoint.x) + " y=" + std::to_string(fallbackPoint.y));
        AdbTap(instanceId, fallbackPoint.x, fallbackPoint.y);
        usedCreateAdCoordinateFallback = true;
        if (!smartSleep(1400)) return SalesLoopDirective::Abort;
        editScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    }
    else {
        SetSalesFlowState(instanceId, account, SalesFlowState::PublishingAd, "Confirming the advertisement on the occupied crate");
        AddSaleLog(instanceId, 1, "SALE_OCCUPIED_CREATE_AD_TEMPLATE_FOUND", "Occupied-crate Create Ad button found. Publishing advertisement. x=" + std::to_string(createAdRes.x) + " y=" + std::to_string(createAdRes.y) + " score=" + FormatProbeScore(createAdRes.score), ImVec4(0.9f, 0.9f, 0.3f, 1.0f));
        LogSaleAction(instanceId, "SALE_OCCUPIED_CREATE_AD_ACTION", "create_occupied_ad", "source=template x=" + std::to_string(createAdRes.x) + " y=" + std::to_string(createAdRes.y));
        AdbTap(instanceId, createAdRes.x, createAdRes.y);
        if (!smartSleep(1000)) return SalesLoopDirective::Abort;
        editScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    }

    if (!IsOccupiedCrateEditPanelVisible(editScreen)) {
        adPlacedThisCycle = true;
        stats.ads++;
        AddSaleLog(instanceId, 1, "SALE_OCCUPIED_AD_PUBLISHED",
            usedAdvertiseCoordinateFallback || usedCreateAdCoordinateFallback
            ? "Advertisement published via direct-coordinate fallback!"
            : "Advertisement published!",
            ImVec4(0, 1, 0, 1));
        LogSaleResult(instanceId, "SALE_OCCUPIED_CREATE_AD_RESULT", "create_occupied_ad", true, usedAdvertiseCoordinateFallback || usedCreateAdCoordinateFallback ? "source=fallback" : "source=template");
        cv::Mat crossScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult crossRes = FindImage(crossScreen, cross_templatePath, g_Thresholds.crossThreshold);
        if (crossRes.found) AdbTap(instanceId, crossRes.x, crossRes.y);

        bot.accounts[accountIndex].isShopFullStuck = false;
        bot.accounts[accountIndex].lastAdTime = std::chrono::steady_clock::now();
        stats.exitReason = "occupied_ad_published";
        SetSalesFlowState(instanceId, account, SalesFlowState::Completed, "Advertisement published; leaving sales so the bot can continue");
        AddSaleLog(instanceId, 1, "SALE_OCCUPIED_AD_EXIT",
            "Advertisement was published on an occupied crate. Not waiting inside the sales cycle; moving to the next bot action.",
            ImVec4(0.45f, 1.0f, 0.45f, 1.0f));
        return SalesLoopDirective::Break;
    }

    std::string evidence = SaveUiEvidenceArtifact(instanceId, "SALE_OCCUPIED_CREATE_AD_FAILED", editScreen, "occupied edit panel still visible");
    AddSaleLog(instanceId, 3, "SALE_OCCUPIED_CREATE_AD_FAILED", "Create Ad confirm is not available even after fallback. Skipping this sales pass. evidence=" + evidence, ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
    LogSaleResult(instanceId, "SALE_OCCUPIED_CREATE_AD_RESULT", "create_occupied_ad", false, "reason=confirm_unavailable evidence=" + evidence, 3);
    if (!EnsureSalePanelClosed(instanceId, cropMode, "create-ad unavailable")) return SalesLoopDirective::Abort;
    return SalesLoopDirective::Break;
}

static SalesLoopDirective RunSalesWebhookRoutineIfNeeded(int instanceId,
    int accountIndex,
    AccountSlot& account,
    bool isEmergency,
    bool& webhookDoneThisCycle,
    const std::function<bool(int)>& smartSleep) {
    if (isEmergency || !g_EnableBarnWebhook || webhookDoneThisCycle) return SalesLoopDirective::None;

    BotInstance& bot = g_Bots[instanceId];
    SetSalesFlowState(instanceId, account, SalesFlowState::RunningWebhook, "Running the webhook and transfer scan");
    AddSaleLog(instanceId, 1, "SALE_WEBHOOK_RUNNING", Tr("Executing Webhook Routine (Checking for Transfer)..."), ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    MatchResult barnRes = FindImage(screen, barn_market_templatePath, 0.80f);

    if (barnRes.found) {
        AdbTap(instanceId, barnRes.x, barnRes.y);
        if (!smartSleep(2000)) return SalesLoopDirective::Abort;

        cv::Mat fullScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (!fullScreen.empty()) {
            std::string webhookImgPath = "C:\\Users\\Public\\webhook_capture_inst" + std::to_string(instanceId) + ".png";
            cv::imwrite(webhookImgPath, fullScreen);

            auto GetRealCount = [&](const std::string& tplPath) -> std::string {
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
                std::string msg = "Ã°Å¸â€œÅ  **[Project Wheat] INSTANCE " + std::to_string(instanceId + 1) + " | SLOT " + std::to_string(accountIndex + 1) + "**\n";
                msg += "Ã°Å¸â€â€ž Sales Cycle: **" + std::to_string(cycleCount) + "**\n";
                msg += "<:Coin:305966854608912386> Total Coins: **" + std::to_string(currentCoins) + "**\n";
                msg += "--------------------------------------\n";
                msg += "Ã°Å¸â€™Â° **BARN INVENTORY**\n";
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
        if (siloRes.found) {
            AdbTap(instanceId, siloRes.x, siloRes.y);
            if (!smartSleep(1000)) return SalesLoopDirective::Abort;
        }
    }

    webhookDoneThisCycle = true;

    if (barnRes.found && g_Bots[5].isRunning && instanceId != 5) {
        InventoryData& chk = bot.accounts[accountIndex].currentInv;
        if (chk.bolt >= g_TransferThreshold || chk.tape >= g_TransferThreshold || chk.plank >= g_TransferThreshold || chk.nail >= g_TransferThreshold || chk.screw >= g_TransferThreshold || chk.panel >= g_TransferThreshold) {
            AdbTap(instanceId, barnRes.x, barnRes.y);
            if (!smartSleep(500)) return SalesLoopDirective::Abort;
        }
    }

    return SalesLoopDirective::None;
}

static void RecordSuccessfulSale(BotInstance& bot,
    AccountSlot& account,
    int instanceId,
    bool useEmergencyWheatSellLimit,
    int& salesCount,
    bool& noCropConfirmedThisCycle,
    bool& noCollectableCrateConfirmedThisCycle,
    SalesCycleStats& stats) {
    AddSaleLog(instanceId, 1, "SALE_LISTING_CONFIRMED", Tr("Item Sold."), ImVec4(0, 1, 0, 1));
    salesCount++;
    stats.listed++;
    bot.totalSales++;
    noCropConfirmedThisCycle = false;
    noCollectableCrateConfirmedThisCycle = false;
    if (useEmergencyWheatSellLimit) {
        account.emergencyWheatStacksSoldThisTurn++;
    }
}

static bool FinalizeSalesCycleExit(int instanceId, int cropMode, const std::function<bool(int)>& smartSleep) {
    if (!smartSleep((std::max)(350, (std::min)(700, g_Intervals.createSaleWait)))) return false;
    if (!ClosePanelStackToFarm(instanceId, cropMode, "sales exit")) {
        AddRecoveryLog(instanceId, 2, "SALE_EXIT_MARKET_STILL_OPEN", "Market view may still be open after the sales exit retries.", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
        if (!EnsureSalePanelClosed(instanceId, cropMode, "sales exit forced panel cleanup", 3, true)) {
            AddRecoveryLog(instanceId, 3, "SALE_EXIT_PANEL_CLEANUP_FAILED", "Sales exit forced panel cleanup failed. Keeping recovery visible in logs.", ImVec4(1.0f, 0.25f, 0.2f, 1.0f));
        }
        if (!EnsureMarketClosed(instanceId, cropMode, "sales exit retry after forced panel cleanup")) {
            AddRecoveryLog(instanceId, 3, "SALE_EXIT_OVERLAY_STILL_OPEN", "Sales overlay may still be open after forced cleanup.", ImVec4(1.0f, 0.25f, 0.2f, 1.0f));
        }
    }
    return true;
}

static SalesLoopDirective ResolveCrateAction(int instanceId, int accountIndex, AccountSlot& account, int cropMode, bool wasInQuarantine, bool& adPlacedThisCycle, const std::function<bool(int)>& smartSleep, MatchResult& emptyCrateOut, int& crateViewMismatchStreak, SalesCycleStats& stats) {
    BotInstance& bot = g_Bots[instanceId];
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);

    SetSalesFlowState(instanceId, account, SalesFlowState::ScanningCrates, "Scanning the market for an empty crate");
    stats.crateScans++;
    AddSaleLog(instanceId, 1, "SALE_CRATE_SCAN", "Scanning crates. signals=" + DescribeShopSignals(screen, cropMode, &account), ImVec4(0.65f, 0.85f, 1.0f, 1.0f));
    bool usedRelaxedCrateThreshold = false;
    MatchResult emptyCrate = FindEmptyCrateWithFallback(screen, account, &usedRelaxedCrateThreshold);
    if (!emptyCrate.found && IsRoadsideShopCrateViewVisible(screen, cropMode)) {
        AddSaleLog(instanceId, 2, "SALE_EMPTY_CRATE_FIRST_SCAN_MISS", "Empty crate not detected on first scan. Running one fallback rescan pass. signals=" + DescribeShopSignals(screen, cropMode, &account), ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
        if (!smartSleep(220)) return SalesLoopDirective::Abort;
        cv::Mat rescanScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        stats.crateScans++;
        usedRelaxedCrateThreshold = false;
        emptyCrate = FindEmptyCrateWithFallback(rescanScreen, account, &usedRelaxedCrateThreshold);
    }
    if (emptyCrate.found && usedRelaxedCrateThreshold) {
        AddSaleLog(instanceId, 1, "SALE_EMPTY_CRATE_RELAXED_HIT", "Empty crate detected via relaxed threshold fallback. x=" + std::to_string(emptyCrate.x) + " y=" + std::to_string(emptyCrate.y) + " score=" + FormatProbeScore(emptyCrate.score), ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
    }
    if (!emptyCrate.found) {
        if (IsSalePanelVisibleForCrop(screen, cropMode)) {
            SetSalesFlowState(instanceId, account, SalesFlowState::HandlingOccupiedCrates, "Sale panel is still open, closing it before rescanning");
            AddSaleLog(instanceId, 2, "SALE_PANEL_OPEN_DURING_CRATE_SCAN", "Already in sale panel. Closing one panel and rescanning crates.", ImVec4(1, 1, 0, 1));
            if (!EnsureSalePanelClosed(instanceId, cropMode, "crate rescan")) return SalesLoopDirective::Abort;
            if (!smartSleep(250)) return SalesLoopDirective::Abort;
            return SalesLoopDirective::Continue;
        }
        if (!IsRoadsideShopCrateViewVisible(screen, cropMode)) {
            crateViewMismatchStreak++;
            SetSalesFlowState(instanceId, account, SalesFlowState::OpeningShop, "Crate scan lost Roadside_Shop verification, reopening the shop");
            AddSaleLog(instanceId, 2, "SALE_CRATE_VIEW_MISMATCH", "Crate scan did not verify Roadside_Shop/crate view. Reopening the shop instead of blind close taps. mismatch_streak=" + std::to_string(crateViewMismatchStreak) + " signals=" + DescribeShopSignals(screen, cropMode, &account), ImVec4(1.0f, 0.75f, 0.2f, 1.0f));

            if (crateViewMismatchStreak >= (kMaxCrateViewMismatchBeforeReload - 1)) {
                AddSaleLog(instanceId, 2, "SALE_CRATE_VIEW_ZOOM_RECOVERY_START", "Crate-view mismatch persists. Leaving market and forcing farm zoom recovery before next shop open.", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                if (!ClosePanelStackToFarm(instanceId, cropMode, "sale crate-view mismatch recovery")) return SalesLoopDirective::Abort;
                if (!smartSleep(320)) return SalesLoopDirective::Abort;
                if (!ZoomOutFarmView(instanceId, "sale crate-view mismatch recovery", 1)) {
                    AddSaleLog(instanceId, 2, "SALE_CRATE_VIEW_ZOOM_RECOVERY_FAILED", "Forced farm zoom recovery failed before shop reopen. Continuing to reopen logic.", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
                }
                else if (!WaitForBotActionWindow(instanceId, 500)) {
                    return SalesLoopDirective::Abort;
                }
            }

            if (!OpenShopForSales(instanceId, account, cropMode, smartSleep, &stats)) {
                if (crateViewMismatchStreak >= kMaxCrateViewMismatchBeforeReload) {
                    AddSaleLog(instanceId, 2, "SALE_CRATE_VIEW_PERSISTENT_RELOAD", "Crate-view mismatch persisted after multiple retries (likely desync/slow network). Reloading the current account to realign UI state.", ImVec4(1.0f, 0.65f, 0.2f, 1.0f));
                    FarmViewRecoveryResult reloadResult = ReloadAccountForRecovery(instanceId, accountIndex, "persistent sale crate-view mismatch");
                    if (reloadResult == FarmViewRecoveryResult::Reloaded) {
                        stats.exitReason = "crate_view_reloaded_after_persistent_mismatch";
                        return SalesLoopDirective::Abort;
                    }
                }
                AddSaleLog(instanceId, 3, "SALE_CRATE_VIEW_REVERIFY_FAILED", "Roadside shop could not be re-verified after the crate-view mismatch. Ending this sales pass so the bot can switch accounts.", ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
                return SalesLoopDirective::Break;
            }
            crateViewMismatchStreak = 0;
            if (!smartSleep(250)) return SalesLoopDirective::Abort;
            return SalesLoopDirective::Continue;
        }

        AddSaleLog(instanceId, 2, "SALE_SHOP_FULL", Tr("Shop is full."), ImVec4(1, 0.8f, 0, 1));
        AddSaleLog(instanceId, 2, "SALE_NO_EMPTY_CRATE_OCCUPIED_FLOW", "No empty crate found. Opening a filled crate for occupied-crate ad flow. signals=" + DescribeShopSignals(screen, cropMode, &account), ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
        std::string evidence = SaveUiEvidenceArtifact(instanceId, "SALE_SHOP_FULL", screen,
            "snapshot=" + DescribeMarketSignalSnapshot(screen, cropMode, &account) + " signals=" + DescribeShopSignals(screen, cropMode, &account));
        AddSaleLog(instanceId, 1, "SALE_SHOP_FULL_EVIDENCE", "Saved shop-full debug capture. evidence=" + evidence, ImVec4(0.65f, 0.85f, 1.0f, 1.0f));
        if (g_TransferRequest.isPending && g_TransferRequest.senderInstanceId == instanceId) {
            AddLog(instanceId, "HEIST ABORTED: Shop is completely full!", ImVec4(1, 0, 0, 1));
            g_TransferRequest.isPending = false;
        }

        if (!IsAdCooldownReady(account)) {
            return ExitSalesPassUntilAdReady(instanceId, account, stats);
        }

        if (adPlacedThisCycle) {
            AddSaleLog(instanceId, 1, "SALE_AD_COOLDOWN_READY",
                "Advertisement was already used earlier in this sales pass, but the cooldown is ready again. Trying occupied-crate ad flow instead of waiting.",
                ImVec4(0.65f, 0.9f, 1.0f, 1.0f));
        }

        return RunOccupiedCrateAdFlow(instanceId, accountIndex, account, cropMode, adPlacedThisCycle, smartSleep, stats);
    }

    SetSalesFlowState(instanceId, account, SalesFlowState::SelectingEmptyCrate, "Empty crate found, opening the sale panel");
    crateViewMismatchStreak = 0;
    stats.emptyCratesSeen++;
    AddSaleLog(instanceId, 1, "SALE_EMPTY_CRATE_FOUND", "Empty crate found. Continuing normal sale flow. x=" + std::to_string(emptyCrate.x) + " y=" + std::to_string(emptyCrate.y) + " score=" + FormatProbeScore(emptyCrate.score), ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
    bot.accounts[accountIndex].isShopFullStuck = false;
    if (wasInQuarantine) {
        AddSaleLog(instanceId, 1, "SALE_QUARANTINE_LIFTED_EMPTY_CRATE", "Quarantine lifted via empty crate. Returning to farm.", ImVec4(0, 1, 0, 1));
        return SalesLoopDirective::Break;
    }

    emptyCrateOut = emptyCrate;
    return OpenSalePanelFromEmptyCrate(instanceId, cropMode, account, emptyCrate, smartSleep);
}

static SalesLoopDirective HandleTransferFlowIfNeeded(int instanceId, int accountIndex, AccountSlot& account, const MatchResult& emptyCrate, bool isEmergency, const std::function<bool(int)>& smartSleep, int& salesCount) {
    BotInstance& bot = g_Bots[instanceId];

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

    if (!(g_TransferRequest.isPending && g_TransferRequest.senderInstanceId == instanceId)) return SalesLoopDirective::None;

    SetSalesFlowState(instanceId, account, SalesFlowState::WaitingForStorage, "Pausing sales while the storage transfer handshake completes");
    if (g_TransferRequest.needFriendship) {
        AddLog(instanceId, "HEIST: Not friends with storage. Initiating handshake...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));

        if (!SendFriendRequestToStorage(instanceId, g_StorageTag)) {
            AddLog(instanceId, "HEIST FAILED: Could not send request.", ImVec4(1, 0, 0, 1));
            g_TransferRequest.isPending = false; g_TransferRequest.needFriendship = false;
            return SalesLoopDirective::Break;
        }

        g_TransferRequest.friendRequestSent = true;
        bot.statusText = "WAITING FRIEND ACCEPT";
        int waitAccept = 0;
        while (g_TransferRequest.needFriendship && waitAccept < 600) {
            if (!bot.isRunning) return SalesLoopDirective::Abort;
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
                    bool marketVisible = IsMarketViewVisible(verifyScr, bot.testCropMode);
                    if (verifyCrate.found || marketVisible) {
                        AddLog(instanceId, "HEIST: Shop verified as open!", ImVec4(0, 1, 0, 1));
                        shopOpened = true;
                        break;
                    }
                    AddLog(instanceId, "HEIST: Shop didn't open. False positive or lag. Retrying...", ImVec4(1, 0.5f, 0, 1));
                }
                else {
                    AddLog(instanceId, "HEIST: Shop not visible yet, waiting...", ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
            }

            if (!shopOpened) {
                AddLog(instanceId, "HEIST ERROR: Could not verify shop is open after multiple tries!", ImVec4(1, 0, 0, 1));
                g_TransferRequest.isPending = false;
                return SalesLoopDirective::Break;
            }

            return SalesLoopDirective::Continue;
        }

        g_TransferRequest.isPending = false;
        return SalesLoopDirective::Break;
    }

    g_TransferRequest.targetSlotX = emptyCrate.x;
    g_TransferRequest.targetSlotY = emptyCrate.y;
    bot.statusText = "WAITING STORAGE BOSS";
    int waitStorage = 0;
    while (!g_TransferRequest.storageReady && waitStorage < 600) {
        if (!bot.isRunning) return SalesLoopDirective::Abort;
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
            AdbTap(instanceId, targetItemRes.x, targetItemRes.y); if (!smartSleep(500)) return SalesLoopDirective::Abort;
            for (int k = 0; k < 10; k++) { AdbTap(instanceId, 467, 173); if (!smartSleep(100)) return SalesLoopDirective::Abort; }
            AdbTap(instanceId, 400, 240); if (!smartSleep(300)) return SalesLoopDirective::Abort;

            shopScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult saleBtn = FindImage(shopScreen, create_sale_templatePath, g_Thresholds.createSaleThreshold);
            if (saleBtn.found) {
                AdbTap(instanceId, saleBtn.x, saleBtn.y);
                AddLog(instanceId, "HEIST: Item Listed! GO GO GO!", ImVec4(1, 0, 0, 1));
                g_TransferRequest.itemListed = true;

                int waitTransfer = 0;
                while (!g_TransferRequest.transferComplete && waitTransfer < 200) {
                    if (!bot.isRunning) return SalesLoopDirective::Abort;
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
                return SalesLoopDirective::Continue;
            }
        }
    }

    g_TransferRequest.isPending = false;
    return SalesLoopDirective::Continue;
}

WheatPreservePlan BuildWheatPreservePlan(const cv::Mat& salePanelScreen, const AccountSlot& account, bool isWheatSale) {
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


static bool AdbTapRepeatedSlow(int instanceId, int x, int y, int count, int delayMs = 85) {
    if (count <= 0) return true;
    count = (std::min)(count, 30);
    delayMs = (std::max)(25, delayMs);
    AppendActionDebugLog(instanceId, "TAP", "AdbTapRepeatedSlow", "x=" + std::to_string(x) + ",y=" + std::to_string(y) + ",count=" + std::to_string(count) + ",delayMs=" + std::to_string(delayMs));
    for (int i = 0; i < count; ++i) {
        if (!g_Bots[instanceId].isRunning) return false;
        AdbTap(instanceId, x, y);
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    return true;
}

// FUNCTION USED IN AUTO ITEM TRANSFER, THIS FUNCTION CLOSES ALL THE MENUS BY SEARCHING CROSS AND TAP ON IT OVER AND OVER UNTIL THERES NO CROSS.

static SalesLoopDirective ConfigureSaleListing(int instanceId, AccountSlot& account, int cropMode, bool useEmergencyWheatSellLimit, int emergencyWheatStackLimit, bool& adPlacedThisCycle, const std::function<bool(int)>& smartSleep, bool& noCropDetectedOut, SalesCycleStats& stats) {
    BotInstance& bot = g_Bots[instanceId];
    noCropDetectedOut = false;
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    SetSalesFlowState(instanceId, account, SalesFlowState::SelectingProduct, "Selecting the crop from the sale panel");
    bool listingControlsAlreadyVisible = IsSaleListingControlsVisible(screen);
    std::string productMatchSource;
    MatchResult productRes = FindSaleProductPreferCache(screen, cropMode, account, &productMatchSource);
    if (!productRes.found) {
        stats.noProductSeen = true;
        AddSaleLog(instanceId, 2, "SALE_PRODUCT_NOT_FOUND", listingControlsAlreadyVisible
            ? "Sale controls are visible, but the configured crop was not found. Closing the panel instead of tapping quantity/price blindly."
            : Tr("No crops to sell."),
            ImVec4(1, 0.5f, 0, 1));
        noCropDetectedOut = true;
        account.cachedSaleProductX = -1;
        account.cachedSaleProductY = -1;
        std::string evidence = SaveUiEvidenceArtifact(instanceId, "SALE_PRODUCT_NOT_FOUND", screen,
            "listing_controls=" + std::string(listingControlsAlreadyVisible ? "1" : "0") +
            " snapshot=" + DescribeMarketSignalSnapshot(screen, cropMode, &account));
        AddSaleLog(instanceId, 2, "SALE_PRODUCT_NOT_FOUND_EVIDENCE", "Saved no-product debug capture. evidence=" + evidence, ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
        if (!ClosePanelStackToFarm(instanceId, cropMode, "no-product cleanup", kRetryNoProductCloseStack)) return SalesLoopDirective::Abort;
        return SalesLoopDirective::Break;
    }
    else {
        if (productMatchSource == "relaxed product" || productMatchSource == "seed fallback") {
            AddSaleLog(instanceId, 1, "SALE_PRODUCT_FALLBACK_HIT", "Sale product found via " + productMatchSource + ". Selecting it now.", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
        }
        LogSaleAction(instanceId, "SALE_SELECT_PRODUCT_ACTION", "select_product", "source=" + productMatchSource + " x=" + std::to_string(productRes.x) + " y=" + std::to_string(productRes.y) + " score=" + FormatProbeScore(productRes.score));
        AdbTap(instanceId, productRes.x, productRes.y);
        if (!smartSleep(g_Intervals.productSelectWait)) return SalesLoopDirective::Abort;
    }

    screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    if (!IsSaleListingControlsVisible(screen) && productRes.found) {
        std::string evidence = SaveUiEvidenceArtifact(instanceId, "SALE_LISTING_CONTROLS_AFTER_PRODUCT_MISSING", screen,
            "source=" + productMatchSource + " snapshot=" + DescribeMarketSignalSnapshot(screen, cropMode, &account));
        AddSaleLog(instanceId, 3, "SALE_LISTING_CONTROLS_AFTER_PRODUCT_MISSING", "Crop was tapped, but listing controls were not verified. Closing the panel instead of using quantity/price coordinate fallbacks. evidence=" + evidence, ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
        account.cachedSaleProductX = -1;
        account.cachedSaleProductY = -1;
        if (!ClosePanelStackToFarm(instanceId, cropMode, "product selection did not open listing controls", kRetryNoProductCloseStack)) return SalesLoopDirective::Abort;
        return SalesLoopDirective::Break;
    }
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
    bool isWheatSale = (cropMode == 0);
    WheatPreservePlan preservePlan = BuildWheatPreservePlan(screen, account, isWheatSale);
    account.preserveReadDetail = preservePlan.active
        ? ("Preserve OCR: count=" + std::to_string(preservePlan.currentCount) + " [" +
            (preservePlan.readMethod.empty() ? "unreadable" : preservePlan.readMethod) + " " +
            std::to_string(preservePlan.ocrScore) + " | digits=" + (preservePlan.digits.empty() ? "-" : preservePlan.digits) + "]")
        : (isWheatSale ? "Preserve OCR inactive for this sale." : "Preserve OCR only applies to wheat.");

    if (saleStackMode == kSaleStackModePreserveOcr) {
        if (preservePlan.active && preservePlan.ocrReadable) {
            if (preservePlan.skipSale) {
                AddSaleLog(instanceId, 1, "SALE_PRESERVE_RESERVE_REACHED", "Preserve OCR: current wheat is " + std::to_string(preservePlan.currentCount) + ", reserve is " + std::to_string(preservePlan.reserveCount) + ". Skipping this sale to keep the reserve intact.", ImVec4(0.9f, 0.9f, 0.3f, 1.0f));
                if (!EnsureSalePanelClosed(instanceId, cropMode, "preserve reserve reached")) return SalesLoopDirective::Abort;
                return SalesLoopDirective::Break;
            }

            resetMinusTapCount = kMaxSaleStack;
            quantityTapCount = preservePlan.desiredStack - 1;
            AddSaleLog(instanceId, 1, "SALE_PRESERVE_PLAN",
                "Sale stack mode [Preserve OCR]: wheat=" + std::to_string(preservePlan.currentCount) +
                ", reserve=" + std::to_string(preservePlan.reserveCount) +
                ", panelStart=" + std::to_string(preservePlan.panelStart) +
                ", target=" + std::to_string(preservePlan.desiredStack) +
                ", method=" + preservePlan.readMethod +
                ", digits=\"" + preservePlan.digits + "\" score=" + std::to_string(preservePlan.ocrScore) + ".",
                ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
        }
        else if (preservePlan.active) {
            AddSaleLog(instanceId, 3, "SALE_PRESERVE_OCR_UNREADABLE", "Sale stack mode [Preserve OCR]: wheat count OCR was unreadable. Skipping this listing so the configured reserve is not violated.", ImVec4(1.0f, 0.55f, 0.2f, 1.0f));
            if (!EnsureSalePanelClosed(instanceId, cropMode, "preserve ocr unreadable")) return SalesLoopDirective::Abort;
            return SalesLoopDirective::Break;
        }
        else if (!isWheatSale) {
            AddSaleLog(instanceId, 2, "SALE_PRESERVE_NON_WHEAT", "Sale stack mode [Preserve OCR]: current crop is not wheat, so the listing will fall back to the game's default quantity.", ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
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
        AddSaleLog(instanceId, 2, "SALE_QUANTITY_PLUS_IGNORED",
            "Sale quantity plus template was ignored because it matched away from the configured quantity button. Using Sale Qty Plus coordinate instead.",
            ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
    }

    if (useEmergencyWheatSellLimit) {
        int remainingEmergencyWheatStacks = (std::max)(0, emergencyWheatStackLimit - account.emergencyWheatStacksSoldThisTurn);
        quantityTapCount = kMaxSaleQuantityPlusTaps;
        AddSaleLog(instanceId, 1, "SALE_EMERGENCY_FULL_STACK", "Emergency Wheat Stack Limit: using three plus taps for a full wheat listing. Remaining emergency wheat stack slots after this one: " + std::to_string((std::max)(0, remainingEmergencyWheatStacks - 1)) + ".", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
    }
    else if (saleStackMode == kSaleStackModePreserveOcr && preservePlan.active && preservePlan.ocrReadable) {
    }
    else if (saleStackMode == kSaleStackModeMax) {
        quantityTapCount = kMaxSaleQuantityPlusTaps;
        AddSaleLog(instanceId, 1, "SALE_STACK_MODE_MAX", std::string("Sale stack mode [") + GetSaleStackModeName(saleStackMode) + "]: using three plus taps for the game maximum.", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
    }
    else if (saleStackMode == kSaleStackModeFixed) {
        resetMinusTapCount = kMaxSaleStack;
        quantityTapCount = fixedSaleStack - 1;
        AddSaleLog(instanceId, 1, "SALE_STACK_MODE_FIXED", std::string("Sale stack mode [") + GetSaleStackModeName(saleStackMode) + "]: resetting to 1 and targeting stack " + std::to_string(fixedSaleStack) + ".", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
    }

    if (resetMinusTapCount > 0) {
        LogSaleAction(instanceId, "SALE_QUANTITY_RESET_ACTION", "reset_quantity", "x=" + std::to_string(quantityMinusX) + " y=" + std::to_string(quantityMinusY) + " taps=" + std::to_string(resetMinusTapCount));
        if (!AdbTapRepeatedSlow(instanceId, quantityMinusX, quantityMinusY, resetMinusTapCount)) return SalesLoopDirective::Abort;
        if (!smartSleep(120)) return SalesLoopDirective::Abort;
    }

    if (quantityTapCount > 0) {
        LogSaleAction(instanceId, "SALE_QUANTITY_INCREASE_ACTION", "increase_quantity", "x=" + std::to_string(quantityTapX) + " y=" + std::to_string(quantityTapY) + " taps=" + std::to_string(quantityTapCount));
        if (!AdbTapRepeatedSlow(instanceId, quantityTapX, quantityTapY, quantityTapCount)) return SalesLoopDirective::Abort;
        if (!smartSleep(120)) return SalesLoopDirective::Abort;
    }

    SetSalesFlowState(instanceId, account, SalesFlowState::AdjustingPrice, account.sellAtMaxPrice ? "Applying max price" : "Applying configured price adjustments");
    if (account.sellAtMaxPrice) {
        AddSaleLog(instanceId, 1, "SALE_PRICE_MAX", "Applying max price.", ImVec4(0, 1, 1, 1));
        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult maxPriceRes = FindImage(screen, max_price_templatePath, g_Thresholds.maxPriceThreshold, true);
        LogSaleAction(instanceId, "SALE_PRICE_ACTION", "apply_max_price", maxPriceRes.found ? ("source=template x=" + std::to_string(maxPriceRes.x) + " y=" + std::to_string(maxPriceRes.y) + " score=" + FormatProbeScore(maxPriceRes.score)) : "source=configured_coordinate");
        if (maxPriceRes.found) AdbTap(instanceId, maxPriceRes.x, maxPriceRes.y);
        else AdbTap(instanceId, g_CoordinateProfile.saleMaxPriceX, g_CoordinateProfile.saleMaxPriceY);
        if (!smartSleep(120)) return SalesLoopDirective::Abort;
    }

    if (!account.sellAtMaxPrice && g_Bots[instanceId].enableShopRandomizer) {
        int randomClicks = rand() % (g_Bots[instanceId].shopRandomizerMax + 1);
        if (randomClicks > 0) {
            AddSaleLog(instanceId, 1, "SALE_PRICE_RANDOMIZER", std::string(Tr("Anti-Ban: Decreasing price ")) + std::to_string(randomClicks) + Tr(" times."), ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
            for (int m = 0; m < randomClicks; m++) {
                if (!bot.isRunning) return SalesLoopDirective::Abort;
                AdbTap(instanceId, g_CoordinateProfile.salePriceMinusX, g_CoordinateProfile.salePriceMinusY);
                if (!smartSleep(100 + (rand() % 150))) return SalesLoopDirective::Abort;
            }
        }
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsedMinutes = std::chrono::duration_cast<std::chrono::minutes>(now - account.lastAdTime).count();
    if (elapsedMinutes >= 5) {
        screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        MatchResult adRes = FindImage(screen, advertise_templatePath, g_Thresholds.advertiseThreshold);
        if (adRes.found) {
            AddSaleLog(instanceId, 1, "SALE_AD_PLACING", Tr("Placing Advertisement..."), ImVec4(0, 1, 0, 1));
            LogSaleAction(instanceId, "SALE_AD_ACTION", "place_ad", "source=template x=" + std::to_string(adRes.x) + " y=" + std::to_string(adRes.y) + " score=" + FormatProbeScore(adRes.score));
            AdbTap(instanceId, adRes.x, adRes.y);
            adPlacedThisCycle = true;
            stats.ads++;
            if (!smartSleep(300)) return SalesLoopDirective::Abort;
            account.lastAdTime = now;
        }
    }

    return SalesLoopDirective::None;
}

// sales cycle function.
void RunSalesCycle(int instanceId, int accountIndex, bool isEmergency) {
    BotInstance& bot = g_Bots[instanceId];
    AccountSlot& account = bot.accounts[accountIndex];
    AddSaleLog(instanceId, 1, "SALE_MODE_ENTER", Tr("Entering Sales Mode..."), ImVec4(0, 1, 1, 1));
    bool useSiloFullTimedSales = (!isEmergency && g_OnlySellIfSiloFull);
    std::string salesOpenDetail = "Opening shop for standard sales";
    if (isEmergency) salesOpenDetail = "Opening shop for emergency recovery";
    else if (useSiloFullTimedSales) salesOpenDetail = "Opening shop because silo full triggered the normal sale mode";
    SetSalesFlowState(instanceId, account, SalesFlowState::OpeningShop, salesOpenDetail, true);
    account.salesCycleCount++;
    SalesCycleStats stats;
    SalesCycleSummaryGuard summaryGuard{ instanceId, stats };

    int emergencyWheatStackLimit = (std::max)(0, (std::min)(20, account.emergencyWheatStackLimit));
    bool useEmergencyWheatSellLimit = isEmergency && bot.testCropMode == 0 && emergencyWheatStackLimit > 0;
    if (useEmergencyWheatSellLimit) {
        int remainingEmergencyWheatStacks = (std::max)(0, emergencyWheatStackLimit - account.emergencyWheatStacksSoldThisTurn);
        AddSaleLog(instanceId, 1, "SALE_EMERGENCY_LIMIT_ACTIVE", "Emergency Wheat Stack Limit active: this account can sell " + std::to_string(remainingEmergencyWheatStacks) + " more wheat stack(s) during silo-full recovery.", ImVec4(0.8f, 0.9f, 0.4f, 1.0f));
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

    if (!OpenShopForSales(instanceId, account, bot.testCropMode, SmartSleep, &stats)) {
        SetSalesFlowState(instanceId, account, SalesFlowState::Failed, "Failed to open the shop menu");
        stats.exitReason = "shop_verify_failed";
        AddSaleLog(instanceId, 3, "SALE_OPEN_SHOP_FAILED", "Failed to verify Roadside_Shop/crate view. Skipping this sales pass so the bot can switch accounts cleanly.", ImVec4(1, 0.2f, 0.2f, 1));
        return;
    }

    bool webhookDoneThisCycle = false;

    int salesCount = 0;
    bool noCropConfirmedThisCycle = false;
    bool noCollectableCrateConfirmedThisCycle = false;
    bool forcedShopReopenPassUsed = false;
    int crateViewMismatchStreak = 0;

    // Checks if it was in quarantine (shop full)
    bool wasInQuarantine = bot.accounts[accountIndex].isShopFullStuck;
    bool adPlacedThisCycle = false;

    while (bot.isRunning && salesCount < 10) {
        SalesLoopDirective soldCrateDirective = CollectSoldCratesIfAny(instanceId, account, wasInQuarantine, SmartSleep, stats);
        if (soldCrateDirective == SalesLoopDirective::Abort) {
            stats.exitReason = "sold_crate_collect_abort";
            return;
        }
        if (soldCrateDirective == SalesLoopDirective::Break) {
            stats.exitReason = wasInQuarantine ? "quarantine_lifted_sold_crate" : "sold_crate_break";
            break;
        }
        if (soldCrateDirective == SalesLoopDirective::Continue) continue;

        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        AddSaleLog(instanceId, 1, "SALE_STATE_SNAPSHOT",
            "loop=" + std::to_string(salesCount + 1) + " " + DescribeMarketSignalSnapshot(screen, bot.testCropMode, &account),
            ImVec4(0.55f, 0.78f, 1.0f, 1.0f));

        if (useEmergencyWheatSellLimit) {
            SetSalesFlowState(instanceId, account, SalesFlowState::CheckingEmergencyLimit, "Checking remaining emergency wheat stack allowance");
            int remainingEmergencyWheatStacks = (std::max)(0, emergencyWheatStackLimit - account.emergencyWheatStacksSoldThisTurn);
            if (remainingEmergencyWheatStacks <= 0) {
                SetSalesFlowState(instanceId, account, SalesFlowState::Completed, "Emergency wheat reserve reached, leaving the shop");
                stats.exitReason = "emergency_limit_reached";
                AddSaleLog(instanceId, 1, "SALE_EMERGENCY_LIMIT_REACHED", "Emergency Wheat Stack Limit reached for this account turn. Leaving the shop with wheat reserved.", ImVec4(1.0f, 1.0f, 0.2f, 1.0f));
                break;
            }
        }

        MatchResult emptyCrate{};
        SalesLoopDirective crateDirective = ResolveCrateAction(instanceId, accountIndex, account, bot.testCropMode, wasInQuarantine, adPlacedThisCycle, SmartSleep, emptyCrate, crateViewMismatchStreak, stats);
        if (crateDirective == SalesLoopDirective::Abort) {
            stats.exitReason = "crate_action_abort";
            return;
        }
        if (crateDirective == SalesLoopDirective::Break) {
            if (stats.exitReason == "ad_cooldown_no_empty_crate" || stats.exitReason == "occupied_ad_published") {
                break;
            }
            SalesLoopDirective noCrateDirective = HandleNoCollectableCrateBreak(instanceId, account, bot.testCropMode, SmartSleep, forcedShopReopenPassUsed, noCollectableCrateConfirmedThisCycle, noCropConfirmedThisCycle, salesCount, stats);
            if (noCrateDirective == SalesLoopDirective::Abort) {
                stats.exitReason = "no_collectable_recovery_abort";
                return;
            }
            if (noCrateDirective == SalesLoopDirective::Continue) continue;
            if (stats.exitReason == "running") stats.exitReason = "no_collectable_crate";
            break;
        }
        if (crateDirective == SalesLoopDirective::Continue) continue;

        // =========================================================
        // 3. WEBHOOK & ITEM COUNT ROUTINE.
        // =========================================================
        SalesLoopDirective webhookDirective = RunSalesWebhookRoutineIfNeeded(instanceId, accountIndex, account, isEmergency, webhookDoneThisCycle, SmartSleep);
        if (webhookDirective == SalesLoopDirective::Abort) {
            stats.exitReason = "webhook_abort";
            return;
        }
        if (false) {
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
                        // cropped yerine direkt fullScreen'den aratÄ±yoruz
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


                        std::string msg = "ðŸ“Š **[Project Wheat] INSTANCE " + std::to_string(instanceId + 1) + " | SLOT " + std::to_string(accountIndex + 1) + "**\n";
                        msg += "ðŸ”„ Sales Cycle: **" + std::to_string(cycleCount) + "**\n";
                        msg += "<:Coin:305966854608912386> Total Coins: **" + std::to_string(currentCoins) + "**\n";
                        msg += "--------------------------------------\n";
                        msg += "ðŸ’° **BARN INVENTORY**\n";
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


        SalesLoopDirective transferDirective = HandleTransferFlowIfNeeded(instanceId, accountIndex, account, emptyCrate, isEmergency, SmartSleep, salesCount);
        if (transferDirective == SalesLoopDirective::Abort) {
            stats.exitReason = "transfer_abort";
            return;
        }
        if (transferDirective == SalesLoopDirective::Break) {
            stats.exitReason = "transfer_break";
            break;
        }
        if (transferDirective == SalesLoopDirective::Continue) continue;

        bool noCropDetectedThisPass = false;
        SalesLoopDirective configureDirective = ConfigureSaleListing(instanceId, account, bot.testCropMode, useEmergencyWheatSellLimit, emergencyWheatStackLimit, adPlacedThisCycle, SmartSleep, noCropDetectedThisPass, stats);
        if (configureDirective == SalesLoopDirective::Abort) {
            stats.exitReason = "configure_sale_abort";
            return;
        }
        if (configureDirective == SalesLoopDirective::Break) {
            if (noCropDetectedThisPass) {
                noCropConfirmedThisCycle = true;
                stats.exitReason = "no_product";
                AddSaleLog(instanceId, 2, "SALE_NO_PRODUCT_ENDING", "No crop detected in sale panel. Product and shop panels were closed; ending this sales cycle.", ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
                break;
            }
            if (stats.exitReason == "running") stats.exitReason = "configure_sale_break";
            break;
        }


		SetSalesFlowState(instanceId, account, SalesFlowState::ConfirmingSale, "Confirming the listing");

        bool saleSubmitted = false;
        if (!SubmitSaleListing(instanceId, bot.testCropMode, account, SmartSleep, saleSubmitted)) {
            stats.exitReason = "submit_sale_abort";
            return;
        }

        if (saleSubmitted) {
            RecordSuccessfulSale(bot, account, instanceId, useEmergencyWheatSellLimit, salesCount, noCropConfirmedThisCycle, noCollectableCrateConfirmedThisCycle, stats);
            if (!SmartSleep((std::max)(160, g_Intervals.createSaleWait / 2))) {
                stats.exitReason = "post_submit_wait_abort";
                return;
            }
        }
        else {
            AddSaleLog(instanceId, 3, "SALE_CONFIRM_NOT_SUBMITTED", "Put on Sale did not confirm. Leaving the sale panel logic to the normal close recovery.", ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
        }
    }
    if (!bot.isRunning && stats.exitReason == "running") stats.exitReason = "bot_stopped";
    if (!FinalizeSalesCycleExit(instanceId, bot.testCropMode, SmartSleep)) {
        stats.exitReason = "exit_cleanup_abort";
        return;
    }
    if (stats.exitReason == "running") stats.exitReason = salesCount > 0 ? "normal" : "no_listing";
    SetSalesFlowState(instanceId, account, account.isShopFullStuck ? SalesFlowState::Quarantined : SalesFlowState::Completed,
        account.isShopFullStuck ? "Shop remains full and advertisement is cooling down" :
        (salesCount > 0 ? "Sales cycle finished and the market was closed" : "Sales cycle finished without new listings"));
}

void RunMarketSalesCycle(int instanceId, int accountIndex, bool isEmergency) {
    RunSalesCycle(instanceId, accountIndex, isEmergency);
}
