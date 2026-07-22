#pragma once

#include "bot_logic.h"
#include "tesseract_ocr.h"

#include <opencv2/opencv.hpp>
#include <string>

inline constexpr int kMaxSaleStack = 10;
inline constexpr int kSaleStackModeGameDefault = 0;
inline constexpr int kSaleStackModeMax = 1;
inline constexpr int kSaleStackModeFixed = 2;
inline constexpr int kSaleStackModePreserveOcr = 3;

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

const char* GetSaleStackModeName(int mode);
WheatPreservePlan BuildWheatPreservePlan(const cv::Mat& salePanelScreen, const AccountSlot& account, bool isWheatSale);
std::string SavePreserveDebugArtifacts(int instanceId, const std::string& reasonTag, const ItemCountDebugInfo& info, const cv::Mat& panelScreen, bool success);

void RunMarketSalesCycle(int instanceId, int accountIndex, bool isEmergency);
