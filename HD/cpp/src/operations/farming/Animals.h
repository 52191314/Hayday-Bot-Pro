#pragma once

#include "bot_logic.h"

enum class CowFeedRoutineResult {
    Completed,
    NotEnoughResources,
    SkippedMissingTemplate,
    Failed
};

inline constexpr int kCowFeedCornCropMode = 1;
inline constexpr int kCowFeedSoybeanCropMode = 3;

const char* ToString(CowFeedRoutineResult result);
bool ShouldRunCowFeed(const AccountSlot& account);
int GetAccountCropWaitSeconds(const AccountSlot& account, int configuredCropMode);
int GetDairyCooldownMinutes(int dairyProductMode);
const char* GetDairyProductName(int dairyProductMode);
bool ShouldRunDairy(const AccountSlot& account);
CowFeedRoutineResult RunCowFeedRoutine(int instanceId, int accountIndex, bool isTestRun);
bool RunDairyRoutine(int instanceId, int accountIndex, bool isTestRun);
