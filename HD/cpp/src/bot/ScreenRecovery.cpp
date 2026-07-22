#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include "cpp/src/bot/RuntimeDeps.h"
#include "cpp/src/bot/ScreenRecovery.h"
#include "cpp/src/bot/AccountFiles.h"
#include "cpp/src/bot/FlowLogging.h"
#include "cpp/src/infra/MinitouchClient.h"

namespace fs = std::filesystem;

namespace {
struct CloseCrossCandidate {
    MatchResult match{ false, 0, 0, 0.0 };
    const char* source = "";
};

double DiagonalXSupportScore(const cv::Mat& mask) {
    if (mask.empty()) return 0.0;

    const int width = mask.cols;
    const int height = mask.rows;
    const int totalPixels = cv::countNonZero(mask);
    if (totalPixels < 8) return 0.0;

    const double tolerance = 0.18;
    int diagA = 0;
    int diagB = 0;
    int centerBand = 0;

    for (int y = 0; y < height; ++y) {
        const uchar* row = mask.ptr<uchar>(y);
        const double ny = height > 1 ? static_cast<double>(y) / (height - 1) : 0.5;
        for (int x = 0; x < width; ++x) {
            if (!row[x]) continue;
            const double nx = width > 1 ? static_cast<double>(x) / (width - 1) : 0.5;
            if (std::abs(nx - ny) <= tolerance) ++diagA;
            if (std::abs((nx + ny) - 1.0) <= tolerance) ++diagB;
            if (std::abs(nx - 0.5) <= 0.25 && std::abs(ny - 0.5) <= 0.25) ++centerBand;
        }
    }

    const double minDiag = static_cast<double>((std::min)(diagA, diagB));
    const double diagBalance = minDiag / (std::max)(1.0, static_cast<double>((std::max)(diagA, diagB)));
    const double diagCoverage = (diagA + diagB) / static_cast<double>(totalPixels);
    const double centerCoverage = centerBand / static_cast<double>(totalPixels);
    return diagBalance * 0.45 + (std::min)(1.0, diagCoverage) * 0.35 + centerCoverage * 0.20;
}

cv::Mat WhiteCrossMask(const cv::Mat& patchBgr) {
    cv::Mat hsv;
    cv::cvtColor(patchBgr, hsv, cv::COLOR_BGR2HSV);
    cv::Mat whiteMask;
    cv::inRange(hsv, cv::Scalar(0, 0, 145), cv::Scalar(179, 95, 255), whiteMask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
    cv::morphologyEx(whiteMask, whiteMask, cv::MORPH_OPEN, kernel);
    return whiteMask;
}

void ConsiderCloseCandidate(CloseCrossCandidate& best, const cv::Rect& rect, double quality, const char* source, const cv::Size& screenSize) {
    if (quality <= 0.0) return;

    const int centerX = rect.x + rect.width / 2;
    const int centerY = rect.y + rect.height / 2;
    const double rightBias = static_cast<double>(centerX) / (std::max)(1, screenSize.width);
    const double topBias = 1.0 - static_cast<double>(centerY) / (std::max)(1, screenSize.height);
    const double score = quality + rightBias * 0.10 + topBias * 0.04;

    if (!best.match.found || score > best.match.score) {
        best.match.found = true;
        best.match.x = centerX;
        best.match.y = centerY;
        best.match.score = score;
        best.source = source;
    }
}

CloseCrossCandidate FindLikelyPopupCloseCrossByVision(const cv::Mat& screen) {
    CloseCrossCandidate best;
    if (screen.empty() || screen.cols < 120 || screen.rows < 120) return best;

    const int minSize = (std::max)(12, screen.cols / 110);
    const int maxSize = (std::min)(96, screen.cols / 10);
    const cv::Rect searchRoi(
        screen.cols / 3,
        0,
        screen.cols - screen.cols / 3,
        (std::min)(screen.rows, static_cast<int>(screen.rows * 0.72)));
    cv::Mat roi = screen(searchRoi);

    cv::Mat hsv;
    cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);

    // Hay Day purchase/offers usually use a red circular close button with a white X.
    cv::Mat redA, redB, redMask;
    cv::inRange(hsv, cv::Scalar(0, 80, 95), cv::Scalar(12, 255, 255), redA);
    cv::inRange(hsv, cv::Scalar(168, 80, 95), cv::Scalar(179, 255, 255), redB);
    cv::bitwise_or(redA, redB, redMask);
    cv::morphologyEx(redMask, redMask, cv::MORPH_CLOSE, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5)));

    std::vector<std::vector<cv::Point>> redContours;
    cv::findContours(redMask, redContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    for (const auto& contour : redContours) {
        cv::Rect r = cv::boundingRect(contour);
        if (r.width < minSize || r.height < minSize || r.width > maxSize || r.height > maxSize) continue;
        const double aspect = static_cast<double>(r.width) / (std::max)(1, r.height);
        if (aspect < 0.60 || aspect > 1.65) continue;

        const double area = cv::contourArea(contour);
        const double fill = area / (std::max)(1, r.width * r.height);
        if (fill < 0.55) continue; // Must be circular/solid, rejecting irregular red blobs like building roofs

        const int expandedX = (std::max)(0, r.x - r.width / 5);
        const int expandedY = (std::max)(0, r.y - r.height / 5);
        cv::Rect expanded(
            expandedX,
            expandedY,
            (std::min)(roi.cols - expandedX, r.width + (2 * r.width / 5)),
            (std::min)(roi.rows - expandedY, r.height + (2 * r.height / 5)));
        if (expanded.width <= 0 || expanded.height <= 0) continue;

        cv::Mat whiteMask = WhiteCrossMask(roi(expanded));
        double xSupport = DiagonalXSupportScore(whiteMask);
        if (xSupport < 0.34) continue;

        cv::Rect screenRect(expanded.x + searchRoi.x, expanded.y + searchRoi.y, expanded.width, expanded.height);
        ConsiderCloseCandidate(best, screenRect, 0.72 + xSupport * 0.25 + fill * 0.08, "vision_red_close_x", screen.size());
    }

    // The "plain light X" check has been removed as it dangerously misidentifies 
    // the white spinning diagonal blades of the Feed Mill as a close button.

    return best;
}
}

MatchResult FindBestCrossAny(const cv::Mat& screen) {
    MatchResult best{false,0,0,0.0};
    auto consider = [&](const std::string& path, float thr) {
        MatchResult r = FindImage(screen, path, thr, false, 1.0f, false);
        if (r.found && (!best.found || r.score > best.score)) best = r;
    };
    consider(cross_templatePath, std::max(0.65f, g_Thresholds.crossThreshold));
    consider(market_close_crosstemplatePath, std::max(0.65f, g_Thresholds.marketCloseCrossThreshold));
    consider(silo_full_cross_templatePath, std::max(0.65f, g_Thresholds.siloFullCrossThreshold));
    consider(farm_pack_cross_templatePath, std::max(0.65f, g_Thresholds.crossThreshold));
    consider(event_board_cross_templatePath, std::max(0.65f, g_Thresholds.crossThreshold));
    CloseCrossCandidate vision = FindLikelyPopupCloseCrossByVision(screen);
    if (vision.match.found && (!best.found || vision.match.score > best.score)) best = vision.match;
    return best;
}

// LAST-RESORT RECOVERY: Dismiss unknown overlays/popups by finding visible close crosses.
// Strategy: scan all known cross templates, dedupe nearby hits, click all detected crosses
// in multiple passes so stacked popups can be cleared in one recovery call.
// Returns true if any close action was taken.
bool DismissUnknownOverlay(int instanceId, const char* reason) {
    BotInstance& bot = g_Bots[instanceId];
    bool dismissedAny = false;
    constexpr int kOverlayDismissPasses = 3;
    constexpr int kCrossDedupDistance = 28;
    constexpr int kMaxCrossTapsPerPass = 6;

    struct CrossHit {
        MatchResult match;
        const char* source = "";
    };

    auto addCrossHits = [&](std::vector<CrossHit>& out, const cv::Mat& screen, const std::string& path, float threshold, const char* source) {
        std::vector<MatchResult> hits = FindAllImages(screen, path, threshold, 24, false, false);
        const int dedupSq = kCrossDedupDistance * kCrossDedupDistance;
        for (const auto& hit : hits) {
            if (!hit.found) continue;
            bool merged = false;
            for (auto& existing : out) {
                int dx = existing.match.x - hit.x;
                int dy = existing.match.y - hit.y;
                if (dx * dx + dy * dy <= dedupSq) {
                    if (hit.score > existing.match.score) {
                        existing.match = hit;
                        existing.source = source;
                    }
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                out.push_back(CrossHit{ hit, source });
            }
        }
    };

    for (int pass = 1; pass <= kOverlayDismissPasses; ++pass) {
        if (!bot.isRunning) break;
        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (screen.empty()) break;

        std::vector<CrossHit> crossHits;
        addCrossHits(crossHits, screen, cross_templatePath, std::max(0.65f, g_Thresholds.crossThreshold), "generic_cross");
        addCrossHits(crossHits, screen, market_close_crosstemplatePath, std::max(0.65f, g_Thresholds.marketCloseCrossThreshold), "market_cross");
        addCrossHits(crossHits, screen, silo_full_cross_templatePath, std::max(0.65f, g_Thresholds.siloFullCrossThreshold), "silo_cross");
        addCrossHits(crossHits, screen, farm_pack_cross_templatePath, std::max(0.65f, g_Thresholds.crossThreshold), "farm_pack_cross");
        addCrossHits(crossHits, screen, event_board_cross_templatePath, std::max(0.65f, g_Thresholds.crossThreshold), "event_board_cross");
        CloseCrossCandidate visionClose = FindLikelyPopupCloseCrossByVision(screen);
        if (visionClose.match.found) {
            bool duplicate = false;
            const int dedupSq = kCrossDedupDistance * kCrossDedupDistance;
            for (const auto& existing : crossHits) {
                int dx = existing.match.x - visionClose.match.x;
                int dy = existing.match.y - visionClose.match.y;
                if (dx * dx + dy * dy <= dedupSq) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) crossHits.push_back(CrossHit{ visionClose.match, visionClose.source });
        }

        if (crossHits.empty()) break;

        std::sort(crossHits.begin(), crossHits.end(), [](const CrossHit& a, const CrossHit& b) {
            if (a.match.y != b.match.y) return a.match.y < b.match.y;
            if (a.match.x != b.match.x) return a.match.x > b.match.x;
            return a.match.score > b.match.score;
        });

        int tapCount = (std::min)(static_cast<int>(crossHits.size()), kMaxCrossTapsPerPass);
        AddRecoveryLog(instanceId, 2, "RECOVERY_CROSS_BATCH",
            std::string("reason=") + reason + " | pass=" + std::to_string(pass) +
            " | hits=" + std::to_string(crossHits.size()) +
            " | taps=" + std::to_string(tapCount),
            ImVec4(1.0f, 0.85f, 0.2f, 1.0f));

        for (int i = 0; i < tapCount; ++i) {
            const auto& hit = crossHits[i];
            AddRecoveryLog(instanceId, 1, "RECOVERY_CROSS_TAP",
                std::string("reason=") + reason +
                " | pass=" + std::to_string(pass) +
                " | idx=" + std::to_string(i + 1) + "/" + std::to_string(tapCount) +
                " | src=" + hit.source +
                " | x=" + std::to_string(hit.match.x) +
                " y=" + std::to_string(hit.match.y) +
                " score=" + std::to_string(hit.match.score).substr(0, 6),
                ImVec4(0.35f, 0.9f, 1.0f, 1.0f));
            AdbTap(instanceId, hit.match.x, hit.match.y);
            dismissedAny = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(280));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(520));
    }

    if (!dismissedAny) {
        AddRecoveryLog(instanceId, 3, "RECOVERY_CROSS_MISSING", std::string("reason=") + reason + " | action=overlay_dismissal success=0", ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    }
    return dismissedAny;
}

bool DismissPurchasePopupIfVisible(int instanceId, int accountIndex, const char* contextLabel, int maxPasses, bool ignoreBotState) {
    BotInstance& bot = g_Bots[instanceId];
    bool dismissedAny = false;

    for (int pass = 1; pass <= maxPasses; ++pass) {
        if (!ignoreBotState && !bot.isRunning) return dismissedAny;

        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (screen.empty()) return dismissedAny;

        MatchResult closeRes = FindLikelyPopupCloseCrossByVision(screen).match;
        if (!closeRes.found) return dismissedAny;

        AddRecoveryLog(instanceId, 1, "PURCHASE_POPUP_DETECTED",
            std::string(contextLabel) + " | pass=" + std::to_string(pass) +
            " | x=" + std::to_string(closeRes.x) +
            " y=" + std::to_string(closeRes.y) +
            " score=" + std::to_string(closeRes.score).substr(0, 6) + " | action=reloading",
            ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            
        if (accountIndex >= 0) {
            ReloadAccountForRecovery(instanceId, accountIndex, "purchase popup detected", ignoreBotState);
        } else {
            RunAdbCommand(instanceId, "shell am force-stop com.supercell.hayday");
        }
        
        dismissedAny = true;
        break; // We already reloaded the game, no need to keep looping
    }
    return dismissedAny;
}

static MatchResult FindPopupLabel(const cv::Mat& screen, const std::string& templatePath) {
    MatchResult best{ false, 0, 0, 0.0 };
    auto consider = [&](const MatchResult& candidate) {
        if (candidate.found && (!best.found || candidate.score > best.score)) best = candidate;
    };
    consider(FindImage(screen, templatePath, 0.76f, false, 1.0f, false, false));
    consider(FindImage(screen, templatePath, 0.70f, true, 1.0f, false, false));
    return best;
}

static MatchResult FindPopupCross(const cv::Mat& screen, const std::string& crossTemplatePath) {
    MatchResult best{ false, 0, 0, 0.0 };
    auto consider = [&](const MatchResult& candidate) {
        if (candidate.found && (!best.found || candidate.score > best.score)) best = candidate;
    };
    consider(FindImage(screen, crossTemplatePath, std::max(0.65f, g_Thresholds.crossThreshold), false, 1.0f, false, false));
    consider(FindImage(screen, crossTemplatePath, 0.62f, true, 1.0f, false, false));
    if (!best.found) best = FindBestCrossAny(screen);
    return best;
}

bool DismissStartupPopupsIfVisible(int instanceId, const char* contextLabel, int maxPasses, bool ignoreBotState) {
    BotInstance& bot = g_Bots[instanceId];
    bool dismissedAny = false;

    for (int pass = 1; pass <= maxPasses; ++pass) {
        if (!ignoreBotState && !bot.isRunning) return dismissedAny;

        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (screen.empty()) return dismissedAny;

        MatchResult farmPack = FindPopupLabel(screen, farm_pack_templatePath);
        MatchResult eventBoard = FindPopupLabel(screen, event_board_templatePath);

        const bool closeFarmPack = farmPack.found && (!eventBoard.found || farmPack.score >= eventBoard.score);
        const bool closeEventBoard = eventBoard.found && !closeFarmPack;
        if (!closeFarmPack && !closeEventBoard) break;

        const char* popupName = closeFarmPack ? "Farm Pack" : "Event Board";
        const std::string& crossPath = closeFarmPack ? farm_pack_cross_templatePath : event_board_cross_templatePath;
        MatchResult closeRes = FindPopupCross(screen, crossPath);
        if (!closeRes.found) {
            AddRecoveryLog(instanceId, 2, "STARTUP_POPUP_CLOSE_MISSING",
                std::string(contextLabel) + " | popup=" + popupName + " | pass=" + std::to_string(pass) + " | action=no_cross_found",
                ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
            break;
        }

        AddRecoveryLog(instanceId, 1, "STARTUP_POPUP_CLOSED",
            std::string(contextLabel) + " | popup=" + popupName + " | pass=" + std::to_string(pass) +
            " | x=" + std::to_string(closeRes.x) + " y=" + std::to_string(closeRes.y),
            ImVec4(0.35f, 0.9f, 1.0f, 1.0f));
        AdbTap(instanceId, closeRes.x, closeRes.y);
        dismissedAny = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
    }

    return dismissedAny;
}

bool IsNotRespondingDialogVisible(const cv::Mat& screen, MatchResult* outDialog) {
    MatchResult dialog = FindPopupLabel(screen, not_responding_templatePath);
    if (outDialog) *outDialog = dialog;
    return dialog.found;
}

bool IsSiloFullPopupVisible(const cv::Mat& screen, MatchResult* outPopup) {
    MatchResult popup = FindImage(screen, silo_full_templatePath, 0.75f, false, 1.0f, false);
    if (outPopup) *outPopup = popup;
    return popup.found;
}

bool EnsureSiloFullPopupClosed(int instanceId, const char* contextLabel, int maxAttempts) {
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

bool WaitForBotActionWindow(int instanceId, int totalMs, bool ignoreBotState) {
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

bool ZoomOutFarmView(int instanceId, const char* reason, int repeatCount, bool ignoreBotState) {
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
    int width = screen.empty() ? 640 : screen.cols;
    int height = screen.empty() ? 480 : screen.rows;

    auto clampCoord = [](int value, int low, int high) {
        return (std::max)(low, (std::min)(high, value));
    };

    int centerX = width / 2;
    int centerY = height / 2;
    // Match the Project_Wheat gesture that works reliably through MEmu/minitouch.
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
        if (!Minitouch::TwoFingerPinch(instanceId, fingerAStart, fingerAEnd, fingerBStart, fingerBEnd)) {
            AddLog(instanceId, "Farm zoom normalization failed. Minitouch pinch could not be sent.", ImVec4(1.0f, 0.4f, 0.3f, 1.0f));
            return false;
        }
        if (!WaitForBotActionWindow(instanceId, 350, ignoreBotState)) return false;
    }

    return true;
}

struct FarmZoomSignals {
    bool mailboxVisible = false;
    bool shopVisible = false;
    bool fieldsVisible = false;
    int emptyFieldCount = 0;
    int grownCropCount = 0;
    int score = 0;
};

static FarmZoomSignals ReadFarmZoomSignals(const cv::Mat& screen, int cropMode) {
    FarmZoomSignals signals;
    if (screen.empty()) return signals;

    MatchResult mailboxRes = FindImage(screen, mailbox_templatePath, (std::max)(0.68f, g_Thresholds.mailboxThreshold - 0.07f), false, 1.0f, false, false);
    signals.mailboxVisible = mailboxRes.found;

    MatchResult shopRes = FindImage(screen, shop_templatePath, (std::max)(0.68f, g_Thresholds.shopThreshold - 0.10f), false, 1.0f, false, false);
    signals.shopVisible = shopRes.found;

    std::vector<MatchResult> emptyFields = FindEmptyFields(screen, true);
    signals.emptyFieldCount = static_cast<int>(emptyFields.size());

    std::vector<MatchResult> grownCrops = FindGrownCrops(screen, cropMode);
    signals.grownCropCount = static_cast<int>(grownCrops.size());
    signals.fieldsVisible = signals.emptyFieldCount >= 4 || signals.grownCropCount >= 4;

    signals.score = (signals.mailboxVisible ? 1 : 0) + (signals.shopVisible ? 1 : 0) + (signals.fieldsVisible ? 1 : 0);
    return signals;
}

static bool FarmZoomHasEnoughAnchors(const FarmZoomSignals& signals) {
    return signals.score >= 2;
}

static bool ShopAndMailboxBothMissing(const FarmZoomSignals& signals) {
    return !signals.shopVisible && !signals.mailboxVisible;
}

static std::string DescribeFarmZoomSignals(const FarmZoomSignals& signals) {
    std::ostringstream oss;
    oss << "shop=" << (signals.shopVisible ? "1" : "0")
        << " mailbox=" << (signals.mailboxVisible ? "1" : "0")
        << " fields=" << (signals.fieldsVisible ? "1" : "0")
        << " emptyFields=" << signals.emptyFieldCount
        << " grownCrops=" << signals.grownCropCount
        << " score=" << signals.score << "/3";
    return oss.str();
}

static bool IsFarmViewLikelyZoomedIn(const cv::Mat& screen, int cropMode, FarmZoomSignals* outSignals = nullptr) {
    if (screen.empty()) return false;
    if (IsSiloFullPopupVisible(screen, nullptr)) return false;
    if (IsMarketViewLikelyVisible(screen, cropMode)) return false;
    MatchResult loadingRes = FindImage(screen, loading_templatePath, 0.75f, false, 1.0f, false, false);
    if (loadingRes.found) return false;

    FarmZoomSignals signals = ReadFarmZoomSignals(screen, cropMode);
    if (outSignals) *outSignals = signals;
    return !FarmZoomHasEnoughAnchors(signals);
}

FarmViewRecoveryResult NormalizeFarmZoomIfAnchorHeuristicMatches(int instanceId, int accountIndex, int cropMode, const char* reason, bool allowReload, bool ignoreBotState) {
    if (!g_Intervals.autoRecoverZoomAfterFieldSweep) return FarmViewRecoveryResult::Stable;

    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
    FarmZoomSignals signals;
    if (!IsFarmViewLikelyZoomedIn(screen, cropMode, &signals)) return FarmViewRecoveryResult::Stable;

    AddLog(instanceId, std::string("Farm zoom anchor heuristic missing enough signals (") + reason + "): " + DescribeFarmZoomSignals(signals) + ". Normalizing zoom...", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    if (!ZoomOutFarmView(instanceId, reason, 1, ignoreBotState)) {
        if (allowReload && accountIndex >= 0) return ReloadAccountForRecovery(instanceId, accountIndex, reason, ignoreBotState);
        return FarmViewRecoveryResult::Failed;
    }
    if (!WaitForBotActionWindow(instanceId, 500, ignoreBotState)) return FarmViewRecoveryResult::Failed;

    cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
    FarmZoomSignals verifySignals = ReadFarmZoomSignals(verifyScreen, cropMode);
    if (ShopAndMailboxBothMissing(verifySignals)) {
        AddLog(instanceId, std::string("Shop and mailbox are still missing after zoom recovery (") + reason + "): " + DescribeFarmZoomSignals(verifySignals) + ".", ImVec4(1.0f, 0.35f, 0.25f, 1.0f));
        if (allowReload && accountIndex >= 0) return ReloadAccountForRecovery(instanceId, accountIndex, "farm anchors missing after zoom recovery", ignoreBotState);
        return FarmViewRecoveryResult::Failed;
    }

    return FarmViewRecoveryResult::Recovered;
}

FarmViewRecoveryResult ReloadAccountForRecovery(int instanceId, int accountIndex, const char* reason, bool ignoreBotState) {
    BotInstance& bot = g_Bots[instanceId];
    AddLog(instanceId, std::string("Reloading account to recover from ") + reason + ".", ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    LoadAccountFromSlot(instanceId, accountIndex);
    AddLog(instanceId, std::string("Waiting ") + std::to_string(g_Intervals.gameLoadWait) + "s for game to reload...", ImVec4(1, 1, 0, 1));
    for (int t = 0; t < g_Intervals.gameLoadWait; t++) {
        if (!ignoreBotState && !bot.isRunning) return FarmViewRecoveryResult::Failed;
        bot.statusText = "Reloading: " + std::to_string(g_Intervals.gameLoadWait - t) + "s";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return FarmViewRecoveryResult::Reloaded;
}

bool ReloadIfGameNotResponding(int instanceId, int accountIndex, const char* contextLabel, bool ignoreBotState) {
    BotInstance& bot = g_Bots[instanceId];
    if (!ignoreBotState && !bot.isRunning) return false;

    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    MatchResult dialog;
    if (!IsNotRespondingDialogVisible(screen, &dialog)) return false;

    AddRecoveryLog(instanceId, 3, "NOT_RESPONDING_RELOAD",
        std::string(contextLabel) + " | dialog_score=" + std::to_string(dialog.score) + " | action=reload_account",
        ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
    FarmViewRecoveryResult reloadResult = ReloadAccountForRecovery(instanceId, accountIndex, "Hay Day not responding dialog", ignoreBotState);
    if (reloadResult == FarmViewRecoveryResult::Reloaded) {
        DismissStartupPopupsIfVisible(instanceId, "post not-responding reload", 4, ignoreBotState);
    }
    return true;
}

FarmViewRecoveryResult RecoverFarmViewAfterDenseSweep(int instanceId, int accountIndex, int cropMode, const char* contextLabel, bool allowReload, bool ignoreBotState) {
    if (!g_Intervals.autoRecoverZoomAfterFieldSweep) return FarmViewRecoveryResult::Stable;

    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
    FarmZoomSignals signals;
    if (!IsFarmViewLikelyZoomedIn(screen, cropMode, &signals)) return FarmViewRecoveryResult::Stable;

    AddLog(instanceId, std::string("Farm zoom check triggered after ") + contextLabel + ": " + DescribeFarmZoomSignals(signals) + ". Attempting zoom recovery...", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    if (!ZoomOutFarmView(instanceId, contextLabel, 1, ignoreBotState)) {
        if (allowReload && accountIndex >= 0) return ReloadAccountForRecovery(instanceId, accountIndex, contextLabel, ignoreBotState);
        return FarmViewRecoveryResult::Failed;
    }
    if (!WaitForBotActionWindow(instanceId, 500, ignoreBotState)) return FarmViewRecoveryResult::Failed;

    cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
    FarmZoomSignals verifySignals;
    if (!IsFarmViewLikelyZoomedIn(verifyScreen, cropMode, &verifySignals)) {
        AddLog(instanceId, std::string("Farm zoom recovered after ") + contextLabel + ".", ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
        return FarmViewRecoveryResult::Recovered;
    }

    if (ShopAndMailboxBothMissing(verifySignals)) {
        AddLog(instanceId, std::string("Shop and mailbox are still missing after zoom recovery (") + contextLabel + "): " + DescribeFarmZoomSignals(verifySignals) + ".", ImVec4(1.0f, 0.35f, 0.25f, 1.0f));
        if (allowReload && accountIndex >= 0) return ReloadAccountForRecovery(instanceId, accountIndex, "farm anchors missing after zoom recovery", ignoreBotState);
        return FarmViewRecoveryResult::Failed;
    }

    AddLog(instanceId, std::string("Farm zoom still looks wrong after recovery (") + contextLabel + "): " + DescribeFarmZoomSignals(verifySignals) + ".", ImVec4(1.0f, 0.45f, 0.2f, 1.0f));
    if (allowReload && accountIndex >= 0) return ReloadAccountForRecovery(instanceId, accountIndex, contextLabel, ignoreBotState);
    return FarmViewRecoveryResult::Failed;
}

bool TryOpenSickleMenuFromCandidates(int instanceId, const std::vector<MatchResult>& grownCandidates, MatchResult& sickleRes, MatchResult* chosenAnchor) {
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
