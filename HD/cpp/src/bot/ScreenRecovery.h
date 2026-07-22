#pragma once

#include "bot_logic.h"
#include <opencv2/opencv.hpp>
#include <vector>

MatchResult FindBestCrossAny(const cv::Mat& screen);
bool DismissUnknownOverlay(int instanceId, const char* reason);
bool DismissPurchasePopupIfVisible(int instanceId, int accountIndex, const char* contextLabel, int maxPasses = 2, bool ignoreBotState = false);
bool DismissStartupPopupsIfVisible(int instanceId, const char* contextLabel, int maxPasses = 4, bool ignoreBotState = false);
bool IsNotRespondingDialogVisible(const cv::Mat& screen, MatchResult* outDialog = nullptr);
bool ReloadIfGameNotResponding(int instanceId, int accountIndex, const char* contextLabel, bool ignoreBotState = false);
bool IsSiloFullPopupVisible(const cv::Mat& screen, MatchResult* outPopup = nullptr);
bool EnsureSiloFullPopupClosed(int instanceId, const char* contextLabel, int maxAttempts = 2);
bool WaitForBotActionWindow(int instanceId, int totalMs, bool ignoreBotState = false);
bool ZoomOutFarmView(int instanceId, const char* reason, int repeatCount = 2, bool ignoreBotState = false);

enum class FarmViewRecoveryResult {
    Stable,
    Recovered,
    Reloaded,
    Failed
};

FarmViewRecoveryResult NormalizeFarmZoomIfAnchorHeuristicMatches(int instanceId, int accountIndex, int cropMode, const char* reason, bool allowReload = true, bool ignoreBotState = false);
FarmViewRecoveryResult ReloadAccountForRecovery(int instanceId, int accountIndex, const char* reason, bool ignoreBotState = false);
FarmViewRecoveryResult RecoverFarmViewAfterDenseSweep(int instanceId, int accountIndex, int cropMode, const char* contextLabel, bool allowReload, bool ignoreBotState = false);
bool TryOpenSickleMenuFromCandidates(int instanceId, const std::vector<MatchResult>& grownCandidates, MatchResult& sickleRes, MatchResult* chosenAnchor = nullptr);
void ForceCloseAllMenus(int instanceId);
