#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "cpp/src/bot/RuntimeDeps.h"
#include "cpp/src/operations/storage/Storage.h"
#include "cpp/src/bot/EmulatorControl.h"
#include "cpp/src/bot/ScreenRecovery.h"

namespace fs = std::filesystem;

int g_TransferThreshold = 10;
TransferRequest g_TransferRequest;

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
// HASTA HISLER KOPTU Ä°PLER HEMDE KENDÄ°LÄ°ÄžÄ°NDEN, BÄ°LMEM NASIL Ä°LERLEDÄ° BU, ACI KEDER DERKEN STRES OLDUM KENDÄ°LÄ°ÄžÄ°MDEN VE HÄ°Ã‡ KÄ°MSEYE Ä°YÄ° GELMEDÄ° BU.
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
        if (screen.empty() || roi.x + roi.width > screen.cols || roi.y + roi.height > screen.rows) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }

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
        api->SetImage((uchar*)bw.data, bw.cols, bw.rows, 1, static_cast<int>(bw.step));
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
                            AdbTap(instanceId, g_CoordinateProfile.homeX, g_CoordinateProfile.homeY);
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
