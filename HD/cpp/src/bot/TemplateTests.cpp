#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "cpp/src/bot/RuntimeDeps.h"
#include "cpp/src/bot/TemplateTests.h"
#include "cpp/src/bot/TestControl.h"
#include "cpp/src/bot/DataSync.h"
#include "cpp/src/bot/ScreenRecovery.h"

namespace fs = std::filesystem;

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
            if (IsSaleListingControlsVisible(saleScreen)) return true;
            AccountSlot verifyAccount;
            if (!FindSaleProductPreferCache(saleScreen, bot.testCropMode, verifyAccount).found) {
                AddLog(instanceId, "TEST SALE PANEL: listing controls were not detected, but the crop picker closed after selection. Continuing.", ImVec4(1.0f, 0.75f, 0.25f, 1.0f));
                return true;
            }
            return false;
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
            MatchResult maxPriceRes = FindImage(screen, max_price_templatePath, g_Thresholds.maxPriceThreshold, true);
            if (maxPriceRes.found) {
                AdbTap(instanceId, maxPriceRes.x, maxPriceRes.y);
            } else {
                AdbTap(instanceId, g_CoordinateProfile.saleMaxPriceX, g_CoordinateProfile.saleMaxPriceY);
            }
            if (!testSleep(250)) return;

            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult adRes = fullFind(screen, advertise_templatePath, g_Thresholds.advertiseThreshold, false);
            if (adRes.found) {
                AddLog(instanceId, "TEST SELL: placing advertisement.", ImVec4(0, 1, 1, 1));
                AdbTap(instanceId, adRes.x, adRes.y);
                if (!testSleep(300)) return;
            }

            screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
            MatchResult saleBtn = FindCreateSaleButtonRelaxed(screen);
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
