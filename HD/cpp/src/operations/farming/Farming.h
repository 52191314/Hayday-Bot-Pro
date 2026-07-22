#pragma once

#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "bot_logic.h"

struct CropRuntimeInfo {
    int mode = 0;
    const char* name = "Wheat";
    int growSeconds = 120;
    std::string seedTemplate;
    float seedThreshold = 0.0f;
    std::string productTemplate;
    float productThreshold = 0.0f;
    std::string grownTemplate;
    float grownThreshold = 0.0f;
};

CropRuntimeInfo GetCropRuntimeInfo(int cropMode);
std::vector<MatchResult> FindGrownCandidatesForCrop(const cv::Mat& screen, int cropMode);
MatchResult FindGrownCropByColor(const cv::Mat& screen, int cropMode, int anchorX, int anchorY);
std::vector<MatchResult> FindEmptyFieldsByColor(const cv::Mat& screen);
std::vector<MatchResult> FindGrowingFieldsByColor(const cv::Mat& screen);

void ExecuteDenseGridGesture(int instanceId, int startX, int startY, const std::vector<MatchResult>& fields);
