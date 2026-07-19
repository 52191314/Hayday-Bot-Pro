#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <string>
#include <vector>
#include <chrono> // Zaman ayarları için eklendi
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <windows.h>
//-------------------------

static constexpr int MAX_ACCOUNT_SLOTS = 25;

//------------------------- 
// --- PREMIUM GUI'DEN TAŞINAN BOT YAPILARI ---

struct InventoryData {
    int wheat = 0;
    int bolt = 0, tape = 0, plank = 0;
    int nail = 0, screw = 0, panel = 0;
    int deed = 0, mallet = 0, marker = 0, map = 0;
    int dynamite = 0, tnt = 0, axe = 0, saw = 0, shovel = 0;
};

enum class FarmFlowState {
    Idle,
    ClearingQuarantine,
    CheckingAutoTom,
    Harvesting,
    HandlingSiloFull,
    Planting,
    RetryingRecovery,
    HandlingCowFeed,
    HandlingDairy,
    EvaluatingSales,
    WaitingForSiloFull,
    CheckingSales,
    CycleComplete,
    Error
};

enum class SalesFlowState {
    Idle,
    OpeningShop,
    CollectingSoldCrates,
    CheckingEmergencyLimit,
    ScanningCrates,
    HandlingOccupiedCrates,
    OpeningAdFlow,
    PublishingAd,
    WaitingForAdSale,
    SelectingEmptyCrate,
    RunningWebhook,
    WaitingForStorage,
    SelectingProduct,
    AdjustingQuantity,
    AdjustingPrice,
    ConfirmingSale,
    Quarantined,
    Completed,
    Failed
};

inline const char* ToString(FarmFlowState state) {
    switch (state) {
    case FarmFlowState::Idle: return "Idle";
    case FarmFlowState::ClearingQuarantine: return "Clearing Quarantine";
    case FarmFlowState::CheckingAutoTom: return "Checking Auto Tom";
    case FarmFlowState::Harvesting: return "Harvesting";
    case FarmFlowState::HandlingSiloFull: return "Handling Silo Full";
    case FarmFlowState::Planting: return "Planting";
    case FarmFlowState::RetryingRecovery: return "Retrying Recovery";
    case FarmFlowState::HandlingCowFeed: return "Handling Cow Feed";
    case FarmFlowState::HandlingDairy: return "Handling Dairy";
    case FarmFlowState::EvaluatingSales: return "Evaluating Sales";
    case FarmFlowState::WaitingForSiloFull: return "Waiting For Silo Full";
    case FarmFlowState::CheckingSales: return "Checking Sales";
    case FarmFlowState::CycleComplete: return "Cycle Complete";
    case FarmFlowState::Error: return "Error";
    default: return "Unknown";
    }
}

inline const char* ToString(SalesFlowState state) {
    switch (state) {
    case SalesFlowState::Idle: return "Idle";
    case SalesFlowState::OpeningShop: return "Opening Shop";
    case SalesFlowState::CollectingSoldCrates: return "Collecting Sold Crates";
    case SalesFlowState::CheckingEmergencyLimit: return "Checking Emergency Limit";
    case SalesFlowState::ScanningCrates: return "Scanning Crates";
    case SalesFlowState::HandlingOccupiedCrates: return "Handling Occupied Crates";
    case SalesFlowState::OpeningAdFlow: return "Opening Ad Flow";
    case SalesFlowState::PublishingAd: return "Publishing Ad";
    case SalesFlowState::WaitingForAdSale: return "Waiting For Ad Sale";
    case SalesFlowState::SelectingEmptyCrate: return "Selecting Empty Crate";
    case SalesFlowState::RunningWebhook: return "Running Webhook";
    case SalesFlowState::WaitingForStorage: return "Waiting For Storage";
    case SalesFlowState::SelectingProduct: return "Selecting Product";
    case SalesFlowState::AdjustingQuantity: return "Adjusting Quantity";
    case SalesFlowState::AdjustingPrice: return "Adjusting Price";
    case SalesFlowState::ConfirmingSale: return "Confirming Sale";
    case SalesFlowState::Quarantined: return "Quarantined";
    case SalesFlowState::Completed: return "Completed";
    case SalesFlowState::Failed: return "Failed";
    default: return "Unknown";
    }
}

struct AccountSlot {
    std::string name = "Empty Slot";
    std::string note;
    bool hasFile = false;
    std::string fileName = "";
    std::chrono::steady_clock::time_point lastPlantTime;
    std::chrono::steady_clock::time_point lastAdTime;
    std::chrono::steady_clock::time_point lastCowFeedRun;
    std::chrono::steady_clock::time_point lastDairyRun;
    std::chrono::steady_clock::time_point activeSessionStartedAt;
    std::chrono::steady_clock::time_point restUntil;
    int salesCycleCount = 0;
    int cyclesSinceStart = 0; // runtime-only: farm cycles completed since bot started
    bool isFirstRun = true;

    // --- NXRTH HAFIZA DEPOSU ---
    InventoryData currentInv;
    InventoryData previousInv;

    // YENİ EKLENEN: ONAYLANMIŞ ÇIPA (ANCHOR) KOORDİNATLARI
    int anchorX = -1;
    int anchorY = -1;
    std::string playerTag = "Unknown";
    std::string farmName = "Unknown";
    int coinAmount = 0;
    int diamondAmount = 0;
    int level= 0;
    // Tom
    bool autoTomEnabled = false;
    int tomRemainingHours = 0;
    int tomRemainingMinutes = 0;
    long long tomStartTime = 0; // İlk tıklamanın zaman damgası (Saniye)
    long long nextTomTime = 0;  // Bir sonraki uyanış (Saniye)
    int tomCategory = 0;        // 0 = Barn, 1 = Silo
    std::string tomItemName = "";
    int targetCyclesBeforeSale = 0;
    int currentCyclesWithoutSale = 0;
    std::string farmConfigName = "default";
    bool sellAtMaxPrice = true;
    int saleStackMode = 0; // 0 = game default, 1 = max stack, 2 = fixed stack, 3 = preserve by OCR
    int fixedSaleStack = 10;
    int keepWheatReserve = 0; // minimum wheat to keep when using Preserve OCR mode
    int emergencyWheatStackLimit = 0; // 0 = unlimited, otherwise max wheat listings per emergency turn
    int emergencyWheatStacksSoldThisTurn = 0; // runtime-only counter, reset each account turn
    bool autoCowFeedEnabled = false;
    bool cowFeedUseFourSlots = false;
    bool cowFeedCropRecoveryRequested = false;
    bool cowFeedCropRecoveryPlanted = false;
    bool autoDairyEnabled = false;
    int dairyProductMode = 0; // 0 = cream, 1 = butter, 2 = cheese
    int dairyQueueCount = 1;
    bool isFriendWithStorage = false; // Depo ile arkadaş mı?
    bool isShopFullStuck = false; // Dükkan tıkandı mı?
    std::chrono::steady_clock::time_point lastShopCheckTime;
    FarmFlowState farmFlowState = FarmFlowState::Idle;
    SalesFlowState salesFlowState = SalesFlowState::Idle;
    std::chrono::steady_clock::time_point farmFlowStateStartedAt;
    std::chrono::steady_clock::time_point salesFlowStateStartedAt;
    std::string farmFlowDetail = "Idle";
    std::string salesFlowDetail = "Idle";
    std::string hudReadDetail = "HUD not read yet";
    std::string preserveReadDetail = "Preserve OCR not used yet";
    int cachedShopX = -1;
    int cachedShopY = -1;
    int cachedEmptyCrateX = -1;
    int cachedEmptyCrateY = -1;
    int cachedSaleProductX = -1;
    int cachedSaleProductY = -1;

};

struct CoordinateProfile {
    int profileOpenX = 25;
    int profileOpenY = 18;
    int profileCopyTagX = 482;
    int profileCopyTagY = 112;
    int profileCloseX = 595;
    int profileCloseY = 45;
    int salePanelCloseX = 545;
    int salePanelCloseY = 100;
    int marketCloseX = 480;
    int marketCloseY = 130;
    int marketCloseSecondX = 473;
    int marketCloseSecondY = 131;
    int occupiedCrateAdOpenX = 354;
    int occupiedCrateAdOpenY = 239;
    int createAdConfirmX = 313;
    int createAdConfirmY = 294;
    int saleQuantityPlusX = 524;
    int saleQuantityPlusY = 161;
    int saleQuantityMinusX = 411;
    int saleQuantityMinusY = 161;
    int saleMaxPriceX = 491;
    int saleMaxPriceY = 247;
    int salePriceMinusX = 411;
    int salePriceMinusY = 218;
    int putOnSaleX = 464;
    int putOnSaleY = 384;
};
struct BotInstance {
    int id;
    bool isActive = false;
    std::atomic<bool> isRunning{false};
    
    int emulatorType = 0; // 0 = MEmu, 1 = LDPlayer, 2 = BlueStacks (Gelecek 
    int emuIndex = 0;
    
    char adbSerial[64] = "";
    char inputDevice[64] = "/dev/input/event1";
    char vmName[64] = "";

    std::string statusText = "IDLE";

    AccountSlot accounts[MAX_ACCOUNT_SLOTS];
    int currentAccountIndex = 0;

    int testCropMode = 0;
    int totalHarvest = 0;
    int totalSales = 0;
    bool enableShopRandomizer = true;
    int shopRandomizerMax = 8;
    bool enableRandomSaleCycle = false;
    std::string multiModeSkipSlots;

    bool isCreatingEmulator = false;
    bool useSingleMode = true;
    bool useMultiMode = false;
    bool useReviveMode = false;
    char reviveTemplatePath[MAX_PATH] = ""; 
    int reviveCheckInterval = 180; 
    int reviveFailCounter = 0;      
    bool enableAccountRunLimit = false;
    int accountRunMinutes = 60;
    int accountRestMinutes = 10;
    bool replaceAccountDuringRest = false;
    int cyclesPerAccount = 1; // How many farm cycles to stay on one account before switching
    bool enablePairRotation = false;
    int pairGroupSize = 2;           // How many accounts active at a time
    int pairRotationMinutes = 120;   // Rotate to next group after this many minutes
    int currentPairOffset = 0;       // Runtime: which group is currently active (persisted)
    std::chrono::steady_clock::time_point pairRotationStartedAt; // Runtime: when current group started
    
    bool enableJanitor = false;       
    int janitorLimit = 15;          
    int currentJanitorCycles = 0;
};

inline void ApplyMultiModeSkipToken(const std::string& token, std::array<bool, MAX_ACCOUNT_SLOTS>& skipMask) {
    if (token.empty()) return;

    auto markSlot = [&](int oneBasedSlot) {
        if (oneBasedSlot >= 1 && oneBasedSlot <= MAX_ACCOUNT_SLOTS) skipMask[oneBasedSlot - 1] = true;
    };

    size_t dashPos = token.find('-');
    try {
        if (dashPos == std::string::npos) {
            markSlot(std::stoi(token));
            return;
        }

        int startSlot = std::stoi(token.substr(0, dashPos));
        int endSlot = std::stoi(token.substr(dashPos + 1));
        if (startSlot > endSlot) std::swap(startSlot, endSlot);
        startSlot = (std::max)(1, startSlot);
        endSlot = (std::min)(15, endSlot);
        for (int slot = startSlot; slot <= endSlot; ++slot) markSlot(slot);
    }
    catch (...) {
    }
}

inline std::array<bool, MAX_ACCOUNT_SLOTS> ParseMultiModeSkipMask(const std::string& value) {
    std::array<bool, MAX_ACCOUNT_SLOTS> skipMask{};
    std::string token;
    auto flushToken = [&]() {
        ApplyMultiModeSkipToken(token, skipMask);
        token.clear();
    };

    for (char ch : value) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isdigit(uch) || ch == '-') {
            token += ch;
        }
        else {
            flushToken();
        }
    }
    flushToken();
    return skipMask;
}

inline bool IsMultiModeSlotSkipped(const std::array<bool, MAX_ACCOUNT_SLOTS>& skipMask, int slotIndex) {
    return slotIndex >= 0 && slotIndex < static_cast<int>(skipMask.size()) && skipMask[slotIndex];
}

inline std::string GenerateMultiModeSkipString(const std::array<bool, MAX_ACCOUNT_SLOTS>& skipMask) {
    std::string result = "";
    for (int i = 0; i < MAX_ACCOUNT_SLOTS; i++) {
        if (skipMask[i]) {
            if (!result.empty()) result += ", ";
            result += std::to_string(i + 1);
        }
    }
    return result;
}

struct TemplateThresholds {
    float soilThreshold = 0.80f;
    float grownThreshold = 0.7f;
    float wheatThreshold = 0.80f;
    float sickleThreshold = 0.90f;
    float shopThreshold = 0.80f;
    float wheatShopThreshold = 0.80f;
    float feedMillThreshold = 0.72f;
    float cowFeedThreshold = 0.72f;
    float dairyThreshold = 0.72f;
    float creamThreshold = 0.72f;
    float notEnoughThreshold = 0.75f;
    float feedMillCrossThreshold = 0.72f;
    float cowPastureThreshold = 0.72f;
    float cornShopThreshold = 0.80f;
    float carrotShopThreshold = 0.80f;
    float soybeanShopThreshold = 0.80f;
    float sugarcaneShopThreshold = 0.80f;
    float barnTabThreshold = 0.80f;
    float siloTabThreshold = 0.80f;
    float materialTemplateThreshold = 0.80f;
    float crateThreshold = 0.80f;
    float arrowsThreshold = 0.85f;
    float plusThreshold = 0.80f;
    float crossThreshold = 0.80f;
    float advertiseThreshold = 0.72f;
    float createAdThreshold = 0.68f;
    float createSaleThreshold = 0.80f;
    float mailboxThreshold = 0.75f;
    float loadingThreshold = 0.75f;
    float cornThreshold = 0.70f;
    float grownCornThreshold = 0.70f;
    float levelUpThreshold = 0.75f;


    float carrotThreshold = 0.75f;
    float grownCarrotThreshold = 0.70f;
    float soybeanThreshold = 0.75f;
    float grownSoybeanThreshold = 0.70f;
    float sugarcaneThreshold = 0.75f;
    float grownSugarcaneThreshold = 0.70f;
	float marketCloseCrossThreshold = 0.70f;
	float siloFullCrossThreshold = 0.70f;
};

struct IntervalSettings {
    int gameLoadWait = 22;
    int afterHarvestWait = 2800;
    int afterPlantWait = 2600;
    int shopEnterWait = 2000;
    int crateClickWait = 800;
    int nextAccountWait = 2000;
    int coinCollectWait = 700;
    int productSelectWait = 500;
    int createSaleWait = 1000;
    bool autoZoomOutAfterMailbox = false;
    int mailboxZoomDelayMs = 5000;

};




extern BotInstance g_Bots[6];
extern CoordinateProfile g_CoordinateProfile;


struct MatchResult {
    bool found;
    int x;
    int y;
    double score;
};

cv::Mat CaptureInstanceScreen(int instanceId, const std::string& adbPath, const std::string& serial);
std::string ResolveTemplatePath(const std::string& templatePath);
MatchResult FindImage(const cv::Mat& screen, const std::string& templatePath, float threshold = 0.70f, bool useGrayscale = false, float roiPercent = 1.0f, bool useMargins = true);
std::vector<MatchResult> FindAllImages(const cv::Mat& screen, const std::string& templatePath, float threshold = 0.80f, int minDist = 10, bool useMargins = true);
std::vector<MatchResult> FindGrownCrops(const cv::Mat& screen, int cropMode);
std::vector<MatchResult> FindEmptyFields(const cv::Mat& screen, bool isRecheck = false);


std::string EncryptXORHex(const std::string& text, const std::string& key = "ISpentDaysForThisApp");
std::string DecryptXORHex(const std::string& hexStr, const std::string& key = "ISpentDaysForThisApp");
