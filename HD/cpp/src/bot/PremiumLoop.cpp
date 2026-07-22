#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "cpp/src/bot/RuntimeDeps.h"
#include "cpp/src/bot/PremiumLoop.h"
#include "cpp/src/bot/AccountFiles.h"
#include "cpp/src/bot/DataSync.h"
#include "cpp/src/bot/EmulatorControl.h"
#include "cpp/src/bot/FlowLogging.h"
#include "cpp/src/bot/ScreenRecovery.h"
#include "cpp/src/infra/Adb.h"
#include "cpp/src/infra/ProcessRunner.h"

namespace fs = std::filesystem;

static bool IsHayDayProcessRunning(int instanceId) {
    BotInstance& bot = g_Bots[instanceId];
    std::string tempFile = "C:\\Users\\Public\\hayday_pid_gate_" + std::to_string(instanceId) + ".txt";
    std::remove(tempFile.c_str());

    std::string cmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + std::string(bot.adbSerial) +
        " shell pidof com.supercell.hayday > \"" + tempFile + "\"\"";
    RunCmdHidden(cmd);

    std::ifstream file(tempFile);
    std::string pid;
    if (file.is_open()) {
        std::getline(file, pid);
        file.close();
    }
    std::remove(tempFile.c_str());
    pid.erase(std::remove_if(pid.begin(), pid.end(), [](unsigned char ch) { return std::isspace(ch) != 0; }), pid.end());
    return pid.length() >= 2;
}

static bool LaunchHayDayAndWaitForPid(int instanceId, const char* reason, int maxWaitSeconds = 45) {
    if (IsHayDayProcessRunning(instanceId)) return true;

    AddLog(instanceId,
        std::string("Hay Day is not running (") + reason + "). Launching it before bot actions...",
        ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
    RunAdbCommand(instanceId, "shell monkey -p com.supercell.hayday -c android.intent.category.LAUNCHER 1");

    BotInstance& bot = g_Bots[instanceId];
    for (int wait = 0; wait < maxWaitSeconds; ++wait) {
        if (!bot.isRunning) return false;
        if (IsHayDayProcessRunning(instanceId)) {
            AddLog(instanceId, "Hay Day process detected. Waiting for the game screen to settle...", ImVec4(0.3f, 1.0f, 0.4f, 1.0f));
            std::this_thread::sleep_for(std::chrono::seconds(3));
            return true;
        }
        bot.statusText = "Launching Hay Day: " + std::to_string(wait + 1) + "/" + std::to_string(maxWaitSeconds);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    AddLog(instanceId, "Hay Day launch timed out; skipping bot actions for this pass.", ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
    return false;
}

static std::string BuildRunnableSlotSummary(const BotInstance& bot, const std::array<bool, MAX_ACCOUNT_SLOTS>& skipMask) {
    std::string summary;
    int count = 0;
    for (int slot = 0; slot < MAX_ACCOUNT_SLOTS; ++slot) {
        if (!bot.accounts[slot].hasFile || IsMultiModeSlotSkipped(skipMask, slot)) continue;
        if (count++ > 0) summary += ", ";
        summary += std::to_string(slot + 1);
    }
    return count > 0 ? summary : "none";
}

static void RunAccountFarmCycle(int instanceId, int accountIndex) {
    BotInstance& bot = g_Bots[instanceId];
    AccountSlot& account = bot.accounts[accountIndex];
    account.cyclesSinceStart++;
    bool outOfSeeds = false;
    bool reloadedForRecovery = false;
    bool fieldsStillGrowing = false;
    bool plantFailedThisCycle = false;
    CropRuntimeInfo crop = GetCropRuntimeInfo(bot.testCropMode);

    constexpr int kHarvestNone = 0;
    constexpr int kHarvestDone = 1;
    constexpr int kHarvestSiloFull = 2;
    constexpr int kHarvestGrowing = 3;

    SetSalesFlowState(instanceId, account, SalesFlowState::Idle, "Waiting for sales cycle", true);
    DismissPurchasePopupIfVisible(instanceId, accountIndex, "farm cycle preflight offer cleanup", 1);

    auto markRecoveryReloadIfNeeded = [&](FarmViewRecoveryResult result) -> bool {
        if (result == FarmViewRecoveryResult::Reloaded) {
            reloadedForRecovery = true;
            return false;
        }
        return result == FarmViewRecoveryResult::Stable || result == FarmViewRecoveryResult::Recovered;
    };

    auto ShouldRunAutoTom = [&]() -> bool {
        if (!account.autoTomEnabled) return false;
        if ((account.tomRemainingHours * 60 + account.tomRemainingMinutes) <= 0) return false;
        if (account.tomItemName.empty()) return false;
        return true;
        };

    auto TryHarvestForCropMode = [&](int cropMode, const char* label) -> int {
        bot.statusText = Tr("Scanning for Grown Crops...");
        DismissPurchasePopupIfVisible(instanceId, accountIndex, "pre-harvest offer cleanup", 1);

        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
        if (screen.empty()) return kHarvestNone;

        std::vector<MatchResult> allGrown = FindGrownCandidatesForCrop(screen, cropMode);
        std::vector<MatchResult> growingFields = FindGrowingFieldsByColor(screen);
        if (!growingFields.empty()) {
            fieldsStillGrowing = true;
        }

        if (allGrown.empty()) {
            if (!growingFields.empty()) {
                std::string waitHint;
                if (account.lastPlantTime.time_since_epoch().count() != 0) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - account.lastPlantTime).count();
                    int remaining = GetAccountCropWaitSeconds(account, cropMode) - static_cast<int>(elapsed);
                    if (remaining > 0) {
                        waitHint = " About " + std::to_string(remaining) + "s remain on the crop timer.";
                    }
                }
                AddLog(instanceId,
                    std::string(label) + ": no grown crops yet; " + std::to_string(growingFields.size()) +
                    " growing field marker(s) are still orange/yellow." + waitHint,
                    ImVec4(1.0f, 0.78f, 0.2f, 1.0f));
                return kHarvestGrowing;
            }
            return kHarvestNone;
        }

        if (!growingFields.empty()) {
            AddLog(instanceId,
                std::string(label) + ": " + std::to_string(allGrown.size()) +
                " grown marker(s) found while " + std::to_string(growingFields.size()) +
                " field marker(s) are still growing. Harvesting only grown-color targets.",
                ImVec4(0.8f, 0.9f, 0.25f, 1.0f));
        }

        MatchResult sickleRes;
        AddLog(instanceId, std::string(label) + ": grown crops detected. Verifying the harvest anchor before opening the sickle menu...", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        bool foundSickle = TryOpenSickleMenuFromCandidates(instanceId, allGrown, sickleRes, nullptr);

        if (foundSickle) {
            AddLog(instanceId, Tr("Sickle Found! Calculating harvest zone..."), ImVec4(0, 1, 0, 1));
            ExecuteDenseGridGesture(instanceId, sickleRes.x, sickleRes.y, allGrown);
            std::this_thread::sleep_for(std::chrono::milliseconds(g_Intervals.afterHarvestWait));
            if (!markRecoveryReloadIfNeeded(RecoverFarmViewAfterDenseSweep(instanceId, accountIndex, cropMode, "harvest sweep", true))) return kHarvestNone;

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
                return kHarvestSiloFull;
            }

            return kHarvestDone;
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
        return fieldsStillGrowing ? kHarvestGrowing : kHarvestNone;
        };

    auto TryHarvest = [&]() -> int {
        if (account.cowFeedCropRecoveryPlanted) {
            AddLog(instanceId, "Cow Feed: harvesting soybean/corn recovery crop mix before normal planting.", ImVec4(0.3f, 0.9f, 1.0f, 1.0f));
            int soybeanStatus = TryHarvestForCropMode(kCowFeedSoybeanCropMode, "Cow Feed Soybean Recovery");
            if (soybeanStatus == kHarvestSiloFull || soybeanStatus == kHarvestGrowing) return soybeanStatus;
            int cornStatus = TryHarvestForCropMode(kCowFeedCornCropMode, "Cow Feed Corn Recovery");
            if (cornStatus == kHarvestSiloFull || cornStatus == kHarvestGrowing) return cornStatus;
            if (soybeanStatus == kHarvestDone || cornStatus == kHarvestDone) {
                account.cowFeedCropRecoveryPlanted = false;
                SaveConfig();
                AddLog(instanceId, "Cow Feed: recovery crops harvested. Returning to the configured crop after this pass.", ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
                return kHarvestDone;
            }
            return kHarvestNone;
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
        bool sawEmptyFields = false;
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
                plantFailedThisCycle = true;
                return false;
            }

            ExecuteDenseGridGesture(instanceId, seedRes.x, seedRes.y, targets);
            std::this_thread::sleep_for(std::chrono::milliseconds(g_Intervals.afterPlantWait));
            if (!markRecoveryReloadIfNeeded(RecoverFarmViewAfterDenseSweep(instanceId, accountIndex, cropMode, phaseLabel, true))) return false;

            cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult crossRes = FindImage(verifyScreen, cross_templatePath, g_Thresholds.crossThreshold, false);
            if (crossRes.found) {
                AddLog(instanceId, "Cow Feed: out of seeds while planting recovery crops. Closing popup.", ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                AdbTap(instanceId, crossRes.x, crossRes.y);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                outOfSeeds = true;
                plantFailedThisCycle = true;
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
                sawEmptyFields = true;
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
                    else {
                        plantFailedThisCycle = true;
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
                    if (!markRecoveryReloadIfNeeded(RecoverFarmViewAfterDenseSweep(instanceId, accountIndex, crop.mode, "plant sweep", true))) return false;

                    cv::Mat verifyScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                    MatchResult crossRes = FindImage(verifyScreen, cross_templatePath, g_Thresholds.crossThreshold, false);

                    if (crossRes.found) {
                        AddLog(instanceId, Tr("OUT OF SEEDS! Diamond pop-up detected. Skipping plant phase."), ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                        AdbTap(instanceId, crossRes.x, crossRes.y);
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        outOfSeeds = true;
                        plantFailedThisCycle = true;
                        return false;
                    }

                    atLeastOnePlantDone = true;
                }
                else {
                    AddLog(instanceId, Tr("Seed menu NOT opened or seed missing. Breaking plant loop."), ImVec4(1, 0.5f, 0, 1));
                    MatchResult crossRes = FindImage(screen, cross_templatePath, g_Thresholds.crossThreshold, false);
                    if (crossRes.found) AdbTap(instanceId, crossRes.x, crossRes.y);
                    plantFailedThisCycle = true;
                    break;
                }
            }
            else {
                std::vector<MatchResult> growingFields = FindGrowingFieldsByColor(screen);
                if (!growingFields.empty()) {
                    fieldsStillGrowing = true;
                    AddLog(instanceId,
                        "No empty pink fields detected; " + std::to_string(growingFields.size()) +
                        " field marker(s) are still growing. Plant pass skipped without retrying.",
                        ImVec4(1.0f, 0.78f, 0.2f, 1.0f));
                    break;
                }

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

        if (sawEmptyFields) {
            plantFailedThisCycle = true;
        }
        return false;
        };

    auto NextAfterCropWork = [&]() -> FarmFlowState {
        return ShouldRunCowFeed(account) ? FarmFlowState::HandlingCowFeed :
            (ShouldRunDairy(account) ? FarmFlowState::HandlingDairy : FarmFlowState::EvaluatingSales);
        };

    FarmFlowState state = account.isShopFullStuck ? FarmFlowState::ClearingQuarantine :
        (ShouldRunAutoTom() ? FarmFlowState::CheckingAutoTom : FarmFlowState::Harvesting);
    int harvestStatus = 0;
    bool plantDone = false;

    while (bot.isRunning) {
        if (ReloadIfGameNotResponding(instanceId, accountIndex, "farm flow loop")) {
            return;
        }

        switch (state) {
        case FarmFlowState::ClearingQuarantine:
            SetFarmFlowState(instanceId, account, state, "Checking ad availability on a quarantined account", true);
            bot.statusText = "Checking Ad Availability...";
            AddLog(instanceId, "Account in quarantine. Checking shop immediately to clear status...", ImVec4(1, 0.5f, 0, 1));
            RunMarketSalesCycle(instanceId, accountIndex, true);
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
            fieldsStillGrowing = false;
            harvestStatus = TryHarvest();
            if (reloadedForRecovery) return;
            state = (harvestStatus == kHarvestSiloFull) ? FarmFlowState::HandlingSiloFull :
                (harvestStatus == kHarvestGrowing ? NextAfterCropWork() : FarmFlowState::Planting);
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
            if (reloadedForRecovery) return;

            if (!EnsureSiloFullPopupClosed(instanceId, "pre-sale silo-full cleanup")) {
                AddLog(instanceId, "Warning: silo-full popup still appears visible after the second close attempt. Continuing into emergency sales anyway.", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
            }

            bot.statusText = g_OnlySellIfSiloFull ? Tr("Silo-Full Sales...") : Tr("Emergency Sales...");
            RunMarketSalesCycle(instanceId, accountIndex, !g_OnlySellIfSiloFull);

            AddLog(instanceId, Tr("Emergency sales pass finished. Resuming harvest after emergency actions..."), ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
            bot.statusText = Tr("Resuming Harvest...");
            fieldsStillGrowing = false;
            harvestStatus = TryHarvest();
            if (reloadedForRecovery) return;
            state = (harvestStatus == kHarvestGrowing) ? NextAfterCropWork() : FarmFlowState::Planting;
            break;

        case FarmFlowState::Planting:
            SetFarmFlowState(instanceId, account, state, outOfSeeds ? "Skipping planting because the account ran out of seeds" : "Checking empty fields and planting");
            bot.statusText = Tr("Checking Plant...");
            plantDone = false;
            plantFailedThisCycle = false;
            if (!outOfSeeds) {
                plantDone = TryPlant();
                if (reloadedForRecovery) return;
            }
            state = (!plantDone && !outOfSeeds && !fieldsStillGrowing) ? FarmFlowState::RetryingRecovery : NextAfterCropWork();
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
            fieldsStillGrowing = false;
            harvestStatus = TryHarvest();
            if (reloadedForRecovery) return;

            if (harvestStatus == kHarvestGrowing) {
                state = NextAfterCropWork();
                break;
            }

            if (harvestStatus == kHarvestSiloFull) {
                bot.statusText = g_OnlySellIfSiloFull ? Tr("Silo-Full Sales...") : Tr("Emergency Sales...");
                RunMarketSalesCycle(instanceId, accountIndex, !g_OnlySellIfSiloFull);
                fieldsStillGrowing = false;
                harvestStatus = TryHarvest();
                if (reloadedForRecovery) return;
                if (harvestStatus == kHarvestGrowing) {
                    state = NextAfterCropWork();
                    break;
                }
            }

            bot.statusText = Tr("Retrying Plant...");
            if (!outOfSeeds) {
                plantDone = TryPlant();
                if (reloadedForRecovery) return;
            }

            if (!plantDone && harvestStatus == kHarvestNone && !fieldsStillGrowing) {
                AddLog(instanceId, Tr("All retries failed. Attempting last-resort overlay dismissal before moving on."), ImVec4(1, 0.2f, 0.2f, 1));
                if (!DismissUnknownOverlay(instanceId, "all retries exhausted")) {
                    ReloadAccountForRecovery(instanceId, accountIndex, "stuck overlay recovery");
                    return; // Abort this cycle safely
                }
            }
            state = NextAfterCropWork();
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
            if (plantDone) {
                account.lastPlantTime = std::chrono::steady_clock::now();
            }
            else if (fieldsStillGrowing) {
                if (account.lastPlantTime.time_since_epoch().count() == 0) {
                    account.lastPlantTime = std::chrono::steady_clock::now();
                    AddLog(instanceId, "Growing fields were detected without a saved crop timer; starting the wait timer from this scan.", ImVec4(0.8f, 0.9f, 0.25f, 1.0f));
                }
                else {
                    AddLog(instanceId, "Crop timer preserved because fields are still growing; next wait uses the original plant time.", ImVec4(0.8f, 0.9f, 0.25f, 1.0f));
                }
            }
            else if (outOfSeeds || plantFailedThisCycle) {
                AddLog(instanceId, "Crop timer not reset because planting did not complete successfully.", ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
            }
            else {
                account.lastPlantTime = std::chrono::steady_clock::now();
            }
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
            RunMarketSalesCycle(instanceId, accountIndex, false);
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
    AddLog(instanceId, "[INSPECTOR] Checking MEmu engine configuration...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));

    std::string width = GetMEmuConfig(instanceId, "resolution_width");
    std::string height = GetMEmuConfig(instanceId, "resolution_height");
    std::string dpi = GetMEmuConfig(instanceId, "vbox_dpi");

    // WARN (not fatal) if settings don't match 640x480 / 100 DPI.
    // Wrong resolution causes all coordinates, templates, and ROIs to be off.
    if (width != "640" || height != "480" || dpi != "100") {
        AddLog(instanceId, "[WARNING] Emulator Settings are WRONG! Expected 640x480 @ 100 DPI. Bot may mis-click or fail.",
            ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        AddLog(instanceId, ("-> Current: " + width + "x" + height + " | DPI: " + dpi).c_str(),
            ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        AddLog(instanceId, "Please change settings in MEmu, restart the emulator and try again.",
            ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        // Do NOT stop the bot — warn and continue so the user sees the issue at runtime.
    } else {
        AddLog(instanceId, "[INSPECTOR] Settings verified (640x480 | 100 DPI).", ImVec4(0, 1, 0, 1));
    }
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
    std::string lastRunnableSlotSummary;
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

        std::string runnableSlotSummary = BuildRunnableSlotSummary(bot, multiModeSkipMask);
        if (bot.useMultiMode && runnableSlotSummary != lastRunnableSlotSummary) {
            AddLog(instanceId,
                "Account rotation effective run slots: " + runnableSlotSummary +
                (bot.enablePairRotation ? " (pair rotation applied if active)" : ""),
                ImVec4(0.45f, 0.85f, 1.0f, 1.0f));
            lastRunnableSlotSummary = runnableSlotSummary;
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
            if (bot.useMultiMode) {
                AddLog(instanceId, "Account rotation selected slot " + std::to_string(i + 1) + " for this pass.", ImVec4(0.45f, 0.85f, 1.0f, 1.0f));
            }
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
                        int elapsedSecs = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - bot.accounts[i].lastShopCheckTime).count());
                        int waitSecs = (std::max)(0, 120 - elapsedSecs);
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

                bool singleModeNeedsLoadScan = false;
                if (batchIdx == 0 && isSingleAccountMode && !IsHayDayProcessRunning(instanceId)) {
                    singleModeNeedsLoadScan = true;
                    if (!LaunchHayDayAndWaitForPid(instanceId, "single account startup gate")) {
                        continue;
                    }
                }

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
                        if (t > 0 && (t % 10 == 0) && ReloadIfGameNotResponding(instanceId, i, "growth wait")) {
                            break;
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
                    AddLog(instanceId,
                        singleModeNeedsLoadScan ? "Single mode launched Hay Day. Running the game-load checks before farming." : Tr("Single mode auto-detected. Hay Day is already running."),
                        ImVec4(0, 1, 1, 1));
                }
            }
            bot.statusText = Tr("Checking Game Load...");
            bool gameLoaded = false;

            // =========================================================================
     //      SKIP MAILBOX SCAN IN SINGLE MODE COMPLETELY
            // =========================================================================
                if ((isSingleAccountMode && !singleModeNeedsLoadScan) || batchIdx > 0) {
                    gameLoaded = true; // Game already open from previous batch cycle or single mode.
                    if (bot.accounts[i].isFirstRun && batchIdx == 0) {
                        AddLog(instanceId, "Single Account Mode: Skipping Mailbox scan, starting immediately!", ImVec4(0, 1, 0, 1));
                    }
                }
            else {
                // ONLY WAIT IN MULTI ACCOUNT MODE.
                bot.statusText = Tr("Checking Game Load...");
                MatchResult mailRes;
                bool loadingStillVisible = true;
                int loadingGoneCount = 0;
                auto tryClosePostLoadPopup = [&](const cv::Mat& screen, const char* phaseLabel) -> bool {
                    MatchResult closeCross = FindBestCrossAny(screen);
                    if (closeCross.found) {
                        AddLog(instanceId,
                            std::string("Mailbox not found (") + phaseLabel + "). Closing visible popup/offer cross...",
                            ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
                        AdbTap(instanceId, closeCross.x, closeCross.y);
                        std::this_thread::sleep_for(std::chrono::milliseconds(700));
                        return true;
                    }

                    return false;
                };
                const int kGameLoadMaxWait = 75; // wait up to 75 seconds
                for (int k = 0; k < kGameLoadMaxWait; k++) {
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

                    // If mailbox is missing right after account load, clear known popups
                    // (event board first, then generic cross) and continue scanning.
                    if (k >= 2 && (k % 3 == 0)) {
                        if (tryClosePostLoadPopup(screen, "load wait")) {
                            continue;
                        }
                    }

                    // Secondary check: if the loading overlay is gone and the
                    // screen has stabilized (contains visible content), treat
                    // the game as loaded even without the mailbox detection.
                    MatchResult loadingRes = FindImage(screen, loading_templatePath, g_Thresholds.loadingThreshold, false, 1.0f, false);
                    if (!loadingRes.found) {
                        loadingGoneCount++;
                        // require 3 consecutive frames without "loading" overlay
                        if (loadingGoneCount >= 3) {
                            // Check that the screen isn't blank (e.g. still
                            // transitioning from the SC/SCW splash)
                            cv::Scalar meanBrightness = cv::mean(screen);
                            double avgBrightness = (meanBrightness[0] + meanBrightness[1] + meanBrightness[2]) / 3.0;
                            if (avgBrightness > 20.0) {
                                gameLoaded = true;
                                AddLog(instanceId, "Game Loaded! Loading overlay gone, screen has stabilized.", ImVec4(0, 1, 0, 1));
                                break;
                            }
                        }
                    } else {
                        loadingGoneCount = 0; // loading still visible, reset
                    }

                    // Emergency: if at 60+ seconds and loading is gone, force-continue
                    if (k >= 60 && loadingGoneCount >= 1 && !loadingRes.found) {
                        cv::Scalar meanBrightness = cv::mean(screen);
                        double avgBrightness = (meanBrightness[0] + meanBrightness[1] + meanBrightness[2]) / 3.0;
                        if (avgBrightness > 20.0) {
                            gameLoaded = true;
                            AddLog(instanceId, "Game loaded after long wait. Loading overlay gone, proceeding.", ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
                            break;
                        }
                    }

                    if (k == kGameLoadMaxWait - 1) {
                        loadingStillVisible = loadingRes.found;
                    }
                    bot.statusText = std::string(Tr("Waiting for Game... ")) + std::to_string(k + 1) + "/" + std::to_string(kGameLoadMaxWait);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (!gameLoaded && bot.isRunning && loadingStillVisible) {
                    AddLog(instanceId, "Mailbox timeout reached, but loading template is still visible. Waiting 10s more...", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                    for (int extra = 0; extra < 10; ++extra) {
                        if (!bot.isRunning) break;
                        bot.statusText = "Loading visible: +" + std::to_string(extra + 1) + "/10s";
                        std::this_thread::sleep_for(std::chrono::seconds(1));

                        cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
                        mailRes = FindImage(screen, mailbox_templatePath, g_Thresholds.mailboxThreshold, false);
                        if (mailRes.found) {
                            gameLoaded = true;
                            AddLog(instanceId, "Game Loaded! Mailbox found during loading grace period.", ImVec4(0, 1, 0, 1));
                            break;
                        }
                        if (extra % 2 == 0) {
                            if (tryClosePostLoadPopup(screen, "loading grace")) {
                                continue;
                            }
                        }
                        // Also fall back on loading-gone detection
                        MatchResult loadingRes = FindImage(screen, loading_templatePath, g_Thresholds.loadingThreshold, false, 1.0f, false);
                        if (!loadingRes.found) {
                            cv::Scalar meanBrightness = cv::mean(screen);
                            double avgBrightness = (meanBrightness[0] + meanBrightness[1] + meanBrightness[2]) / 3.0;
                            if (avgBrightness > 20.0) {
                                gameLoaded = true;
                                AddLog(instanceId, "Game Loaded! Loading cleared during grace period.", ImVec4(0, 1, 0, 1));
                                break;
                            }
                        }
                    }
                }
            }

            if (!gameLoaded) {
                AddLog(instanceId, Tr("Timeout (Mailbox not found). Skipping account."), ImVec4(1, 0.4f, 0.4f, 1));
                continue;
            }
            DismissPurchasePopupIfVisible(instanceId, i, "post-load offer cleanup", 3);
            DismissStartupPopupsIfVisible(instanceId, "post-load popup cleanup", 5);
            if (ReloadIfGameNotResponding(instanceId, i, "post-load ready check")) {
                DismissPurchasePopupIfVisible(instanceId, i, "post-reload offer cleanup", 3);
                DismissStartupPopupsIfVisible(instanceId, "post-reload popup cleanup", 5);
            }
            // game_config.csv still widens the allowed camera range, but newer
            // Hay Day builds can restore account camera state after login.
            // Run an optional startup zoom pass before any farming/sale actions.
            bool normalizedByAnchorHeuristic = false;
            if (batchIdx == 0 && g_Intervals.autoZoomOutAfterMailbox && !normalizedByAnchorHeuristic) {
                if (!WaitForBotActionWindow(instanceId, (std::max)(0, g_Intervals.mailboxZoomDelayMs))) {
                    break;
                }
                if (!ZoomOutFarmView(instanceId, "startup mailbox detection", 3)) {
                    AddLog(instanceId, "Startup zoom normalization was skipped or failed. Continuing with the current view.", ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
                }
            }
            cv::Mat curScreen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);

            // ACCOUNT INFORMATION READING GOES HERE.
                if (batchIdx == 0 && !curScreen.empty() && curScreen.cols > 600 && curScreen.rows > 100) {
                GameNumberDebugInfo coinInfo = ReadGameNumberDebug(curScreen, cv::Rect(507, 2, 72, 21), "coin");
                GameNumberDebugInfo diaInfo = ReadGameNumberDebug(curScreen, cv::Rect(547, 22, 34, 21), "dia");
                bot.accounts[i].coinAmount = coinInfo.count;
                bot.accounts[i].diamondAmount = diaInfo.count;

                bool profileTagMissing = bot.accounts[i].playerTag == "Unknown" || bot.accounts[i].playerTag == "NO_TAG";
                bool needsProfileForStorageTransfer = profileTagMissing && instanceId != 5 && g_Bots[5].isRunning;
                if (needsProfileForStorageTransfer) {
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

                // Save the account state after farming, before rotating to the next slot.
                // Only in multi-mode to mirror the load pattern (LoadAccountFromSlot above).
                if (!isSingleAccountMode) {
                    SaveAccountToSlot(instanceId, i);
                }
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
