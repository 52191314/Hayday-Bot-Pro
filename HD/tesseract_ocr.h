#pragma once
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

extern float g_OcrAnchorThreshold;
extern int g_WheatPreserveScanMode; // 0 = template anchor, 1 = direct coordinate, 2 = calibrated ROI
extern int g_WheatPreserveCoordX;
extern int g_WheatPreserveCoordY;
extern int g_WheatPreserveRoiLeft;
extern int g_WheatPreserveRoiTop;
extern int g_WheatPreserveRoiRight;
extern int g_WheatPreserveRoiBottom;

struct ItemCountDebugInfo {
    int count = 0;
    bool anchorFound = false;
    bool roiValid = false;
    bool usedFocusedCrop = false;
    int ocrScore = 0;
    std::string digits;
    std::string readMethod = "unreadable";
    cv::Rect roi;
    cv::Mat rawCrop;
    cv::Mat finalCrop;
};

struct GameNumberDebugInfo {
    int count = 0;
    int score = 0;
    std::string digits;
    std::string readMethod = "unreadable";
};

ItemCountDebugInfo ReadItemCountByAnchorDebug(cv::Mat screen, const std::string& templatePath, bool forceCalibratedRoi = false);
int ReadItemCountByAnchor(cv::Mat screen, const std::string& templatePath, bool forceCalibratedRoi = false);

GameNumberDebugInfo ReadGameNumberDebug(cv::Mat screen, cv::Rect roi, std::string debugName);
int ReadGameNumber(cv::Mat screen, cv::Rect roi, std::string debugName);
std::string ReadFarmName(cv::Mat screen, cv::Rect roi);
