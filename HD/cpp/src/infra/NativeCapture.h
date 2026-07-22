#pragma once

#include <opencv2/opencv.hpp>
#include <string>

cv::Mat CaptureInstanceScreen(int instanceId, const std::string& adbPath, const std::string& serial);

