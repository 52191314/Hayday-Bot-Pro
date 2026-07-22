#pragma once

#include "bot_logic.h"

#include <initializer_list>
#include <opencv2/opencv.hpp>
#include <string>

struct MarketSignalSnapshot {
    bool farm = false;
    bool shop = false;
    bool product = false;
    bool ad = false;
    bool emptyCrate = false;
    bool soldCrate = false;
    bool filledCrate = false;
    bool shopIcon = false;
};

std::string FormatMarketScore(double score);

std::string GetFilledCrateTemplateForCrop(int cropMode);
MatchResult FindAnyFilledCrate(const cv::Mat& screen, int cropMode, float threshold = 0.75f);

MarketSignalSnapshot ProbeMarketSignals(const cv::Mat& screen, int cropMode, AccountSlot* account = nullptr);
std::string DescribeMarketSignalSnapshot(const MarketSignalSnapshot& snapshot);
std::string DescribeMarketSignalSnapshot(const cv::Mat& screen, int cropMode, AccountSlot* account = nullptr);
std::string DescribeShopSignals(const cv::Mat& screen, int cropMode, AccountSlot* account = nullptr);
bool IsRoadsideShopBannerVisible(const cv::Mat& screen);
bool IsRoadsideShopCrateViewVisible(const cv::Mat& screen, int cropMode);
bool IsBarnSalePanelVisible(const cv::Mat& screen);

cv::Rect BuildSaleDialogSearchRect(const cv::Mat& screen);
MatchResult FindTemplateInRect(const cv::Mat& screen, const cv::Rect& rect, const std::string& templatePath, float threshold, bool grayscale, bool allowFallback = true);
MatchResult FindDialogButtonTemplate(const cv::Mat& screen, const std::string& templatePath, float threshold, bool allowFallback = true);
MatchResult FindBestDialogTemplate(const cv::Mat& screen, const std::initializer_list<std::string>& templatePaths, float threshold, bool allowFallback = true);

std::string GetSaleProductTemplateForCrop(int cropMode);
float GetSaleProductThresholdForCrop(int cropMode);
MatchResult FindShopIconPreferCache(const cv::Mat& screen, AccountSlot& account);
MatchResult FindEmptyCratePreferCache(const cv::Mat& screen, AccountSlot& account);
MatchResult FindEmptyCrateWithFallback(const cv::Mat& screen, AccountSlot& account, bool* usedRelaxedThreshold = nullptr);
MatchResult FindSaleProductPreferCache(const cv::Mat& screen, int cropMode, AccountSlot& account, std::string* matchSource = nullptr);

bool IsSaleProductPickerVisible(const cv::Mat& screen, int cropMode);
MatchResult FindCreateSaleButtonRelaxed(const cv::Mat& screen);
bool IsSaleListingControlsVisible(const cv::Mat& screen);
bool IsSalePanelVisible(const cv::Mat& screen);
bool IsSalePanelVisibleForCrop(const cv::Mat& screen, int cropMode);
bool IsOccupiedCrateEditPanelVisible(const cv::Mat& screen);
bool IsMarketViewVisible(const cv::Mat& screen, int cropMode);
bool IsMarketViewLikelyVisible(const cv::Mat& screen, int cropMode);
