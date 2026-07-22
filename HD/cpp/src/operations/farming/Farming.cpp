#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "Farming.h"

#include "cpp/src/infra/MinitouchClient.h"
#include "cpp/src/infra/NativeCapture.h"
#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>

extern void AddLog(int instanceId, std::string message, ImVec4 color);
extern BotInstance g_Bots[6];
extern std::string kAdbPath;

void ExecuteDenseGridGesture(int instanceId, int startX, int startY, const std::vector<MatchResult>& fields) {
    if (fields.empty()) return;

    AddLog(instanceId, "[INFO] Executing classic V-shape sweep...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));

    int minX = 99999, maxX = 0, minY = 99999, maxY = 0;
    for (const auto& f : fields) {
        if (f.x < minX) minX = f.x;
        if (f.x > maxX) maxX = f.x;
        if (f.y < minY) minY = f.y;
        if (f.y > maxY) maxY = f.y;
    }

    int screenWidth = 640;
    int screenHeight = 480;
    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, g_Bots[instanceId].adbSerial);
    if (!screen.empty()) {
        screenWidth = screen.cols;
        screenHeight = screen.rows;
    }
    // Margins relative to screen dimensions (~12.5% width, ~16.7% height = ~80px on 640x480).
    int marginX = (int)(screenWidth * 0.125f);
    int marginY = (int)(screenHeight * 0.167f);

    int targetMinX = std::max(0, minX - marginX);
    int targetMaxX = std::min(screenWidth - 1, maxX + marginX);
    int targetMinY = std::max(0, minY - marginY);
    int targetMaxY = std::min(screenHeight - 1, maxY + marginY);
    startX = (std::max)(0, (std::min)(screenWidth - 1, startX));
    startY = (std::max)(0, (std::min)(screenHeight - 1, startY));

    int p1_startX = startX;
    int p1_startY = startY;
    int p2_startX = (std::min)(screenWidth - 1, startX + 5);
    int p2_startY = (std::min)(screenHeight - 1, startY + 5);
    int p1_x1 = targetMinX; int p1_y1 = targetMaxY;
    int p2_x1 = targetMaxX; int p2_y1 = targetMaxY;
    int p1_x2 = p1_x1; int p1_y2 = targetMinY;
    int p2_x2 = p2_x1; int p2_y2 = targetMinY;
    int p1_x3 = p1_x1; int p1_y3 = targetMaxY;
    int p2_x3 = p2_x1; int p2_y3 = targetMaxY;

    std::vector<POINT> aWaypoints = {{p1_x1, p1_y1}, {p1_x2, p1_y2}, {p1_x3, p1_y3}};
    std::vector<POINT> bWaypoints = {{p2_x1, p2_y1}, {p2_x2, p2_y2}, {p2_x3, p2_y3}};
    if (Minitouch::TwoFingerSweep(instanceId, {p1_startX, p1_startY}, {p2_startX, p2_startY},
                                   aWaypoints, bWaypoints)) {
        AddLog(instanceId, "[SUCCESS] 2-Finger Smart Sweep complete!", ImVec4(0, 1, 0, 1));
    } else {
        AddLog(instanceId, "[ERROR] Minitouch connection failed!", ImVec4(1, 0, 0, 1));
    }
}

extern TemplateThresholds g_Thresholds;
extern std::string w_templatePath;
extern std::string wheatshop_templatePath;
extern std::string g_templatePath;
extern std::string c_templatePath;
extern std::string cornshop_templatePath;
extern std::string gc_templatePath;
extern std::string carrot_templatePath;
extern std::string carrot_shop_templatePath;
extern std::string grown_carrot_templatePath;
extern std::string soybean_templatePath;
extern std::string soybean_shop_templatePath;
extern std::string grown_soybean_templatePath;
extern std::string sugarcane_templatePath;
extern std::string sugarcane_shop_templatePath;
extern std::string grown_sugarcane_templatePath;
CropRuntimeInfo GetCropRuntimeInfo(int cropMode) {
    CropRuntimeInfo crop;
    crop.mode = cropMode;
    crop.seedTemplate = w_templatePath;
    crop.seedThreshold = g_Thresholds.wheatThreshold;
    crop.productTemplate = wheatshop_templatePath;
    crop.productThreshold = g_Thresholds.wheatShopThreshold;
    crop.grownTemplate = g_templatePath;
    crop.grownThreshold = g_Thresholds.grownThreshold;

    if (cropMode == 1) {
        crop.name = "Corn";
        crop.growSeconds = 300;
        crop.seedTemplate = c_templatePath;
        crop.seedThreshold = g_Thresholds.cornThreshold;
        crop.productTemplate = cornshop_templatePath;
        crop.productThreshold = g_Thresholds.cornShopThreshold;
        crop.grownTemplate = gc_templatePath;
        crop.grownThreshold = g_Thresholds.grownCornThreshold;
    }
    else if (cropMode == 2) {
        crop.name = "Carrot";
        crop.growSeconds = 600;
        crop.seedTemplate = carrot_templatePath;
        crop.seedThreshold = g_Thresholds.carrotThreshold;
        crop.productTemplate = carrot_shop_templatePath;
        crop.productThreshold = g_Thresholds.carrotShopThreshold;
        crop.grownTemplate = grown_carrot_templatePath;
        crop.grownThreshold = g_Thresholds.grownCarrotThreshold;
    }
    else if (cropMode == 3) {
        crop.name = "Soybean";
        crop.growSeconds = 1200;
        crop.seedTemplate = soybean_templatePath;
        crop.seedThreshold = g_Thresholds.soybeanThreshold;
        crop.productTemplate = soybean_shop_templatePath;
        crop.productThreshold = g_Thresholds.soybeanShopThreshold;
        crop.grownTemplate = grown_soybean_templatePath;
        crop.grownThreshold = g_Thresholds.grownSoybeanThreshold;
    }
    else if (cropMode == 4) {
        crop.name = "Sugarcane";
        crop.growSeconds = 1800;
        crop.seedTemplate = sugarcane_templatePath;
        crop.seedThreshold = g_Thresholds.sugarcaneThreshold;
        crop.productTemplate = sugarcane_shop_templatePath;
        crop.productThreshold = g_Thresholds.sugarcaneShopThreshold;
        crop.grownTemplate = grown_sugarcane_templatePath;
        crop.grownThreshold = g_Thresholds.grownSugarcaneThreshold;
    }

    return crop;
}

std::vector<MatchResult> FindGrownCandidatesForCrop(const cv::Mat& screen, int cropMode) {
    if (screen.empty()) return {};
    CropRuntimeInfo crop = GetCropRuntimeInfo(cropMode);
    std::vector<MatchResult> templateHits;
    if (!crop.grownTemplate.empty()) {
        // Prefer template anchors first so sickle verification taps reliable points before HSV/contour guesses.
        templateHits = FindAllImages(screen, crop.grownTemplate, crop.grownThreshold, 20, false, false);
        if (templateHits.empty()) {
            float relaxedThreshold = (std::max)(0.60f, crop.grownThreshold - 0.08f);
            if (relaxedThreshold < crop.grownThreshold) {
                templateHits = FindAllImages(screen, crop.grownTemplate, relaxedThreshold, 20, false, false);
            }
        }
    }

    std::vector<MatchResult> colorHits = FindGrownCrops(screen, cropMode);
    if (colorHits.empty() && templateHits.empty()) return {};

    // Keep color matches (centroids) first, then append non-duplicate template matches as extra coverage.
    std::vector<MatchResult> merged = colorHits;
    const int mergeDistanceSq = 18 * 18;
    for (const auto& templateHit : templateHits) {
        bool duplicate = false;
        for (const auto& existing : merged) {
            int dx = templateHit.x - existing.x;
            int dy = templateHit.y - existing.y;
            if (dx * dx + dy * dy <= mergeDistanceSq) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) merged.push_back(templateHit);
    }
    return merged;
}


MatchResult FindGrownCropByColor(const cv::Mat& screen, int cropMode, int anchorX, int anchorY) {
    MatchResult result = { false, -1, -1, 0.0 };
    if (screen.empty()) return result;

    // ROI
    int topMargin = 70;
    int bottomMargin = 40;
    int sideMargin = 30;

    if (screen.rows <= topMargin + bottomMargin || screen.cols <= sideMargin * 2) return result;

    cv::Rect roiRect(sideMargin, topMargin, screen.cols - (sideMargin * 2), screen.rows - (topMargin + bottomMargin));
    cv::Mat searchArea = screen(roiRect);

    // 2. CHANGE COLOR SPACE
    cv::Mat hsv;
    cv::cvtColor(searchArea, hsv, cv::COLOR_BGR2HSV);

    CropStateHsvRange grownRange;
    if (!GetCropGrownHsvRange(cropMode, grownRange)) return result;
    cv::Scalar lowerBound = grownRange.lower;
    cv::Scalar upperBound = grownRange.upper;

    cv::Mat mask;
    cv::inRange(hsv, lowerBound, upperBound, mask);

    // =========================================================================
	// 4. CLEAN LITTLE DOTS TO AVOID FALSE DETECTIONS AND FILL SPACES IN BIGGER AREAS.
    // =========================================================================
    // A. DELETE DUST-LIKE TRASH (LIKE DOTS WITH 1-2 PIXEL BIG) USING (MORPH_OPEN)
    cv::Mat kernelOpen = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernelOpen);

    // B. FILL THE EMPTY SPACE IN FIELDS (MORPH_CLOSE)
    cv::Mat kernelClose = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 7));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernelClose);

	// 5. FIND CONTOURS
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double bestMetric = (anchorX != -1 && anchorY != -1) ? 9999999.0 : 0.0;
    cv::Point bestCenter(-1, -1);

    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);

        // AREA THRESHOLD
        if (area > 50) {
            cv::Moments m = cv::moments(contours[i]);
            if (m.m00 != 0) {
                int localX = (int)(m.m10 / m.m00);
                int localY = (int)(m.m01 / m.m00);

                // =====================================================================
                // DONUT HOLE PROTECTION
                // =====================================================================
                if (localX >= 0 && localX < mask.cols && localY >= 0 && localY < mask.rows) {
                    if (mask.at<uchar>(localY, localX) == 0) {

                        double minDist = 9999999.0;
                        int safeX = localX, safeY = localY;

                        for (int y = 0; y < mask.rows; y += 2) {
                            for (int x = 0; x < mask.cols; x += 2) {
                                if (mask.at<uchar>(y, x) > 0) {
                                    double dist = std::pow(x - localX, 2) + std::pow(y - localY, 2);
                                    if (dist < minDist) {
                                        minDist = dist;
                                        safeX = x;
                                        safeY = y;
                                    }
                                }
                            }
                        }
                        localX = safeX;
                        localY = safeY;
                    }
                }
                // =====================================================================

                int realX = localX + sideMargin;
                int realY = localY + topMargin;

                if (anchorX != -1 && anchorY != -1) {

                    double dist = std::sqrt(std::pow(realX - anchorX, 2) + std::pow(realY - anchorY, 2));
                    if (dist < bestMetric) {
                        bestMetric = dist;
                        bestCenter = cv::Point(realX, realY);
                    }
                }
                else {

                    if (area > bestMetric) {
                        bestMetric = area;
                        bestCenter = cv::Point(realX, realY);
                    }
                }
            }
        }
    }

    if (bestCenter.x != -1) {
        result.found = true;
        result.score = bestMetric;
        result.x = bestCenter.x;
        result.y = bestCenter.y;
    }

    return result;
}

// ==============================================================================
// EMPTY FIELD DETECTOR (MAGENTA - HSV)
// ==============================================================================
std::vector<MatchResult> FindEmptyFieldsByColor(const cv::Mat& screen) {
    std::vector<MatchResult> results;
    if (screen.empty()) return results;

    int topMargin = 70;
    int bottomMargin = 40;
    int sideMargin = 30;

    if (screen.rows <= topMargin + bottomMargin || screen.cols <= sideMargin * 2) return results;

    cv::Rect roiRect(sideMargin, topMargin, screen.cols - (sideMargin * 2), screen.rows - (topMargin + bottomMargin));
    cv::Mat searchArea = screen(roiRect);

    cv::Mat hsv, mask;
    cv::cvtColor(searchArea, hsv, cv::COLOR_BGR2HSV);

    const CropStateHsvRange& emptyRange = GetCropStateHsvRange(CropVisualState::Empty);
    cv::Scalar lowerBound = emptyRange.lower;
    cv::Scalar upperBound = emptyRange.upper;

    cv::inRange(hsv, lowerBound, upperBound, mask);

	// Clean little pixels to avoid false detections and fill spaces in bigger areas.
    cv::Mat kernelOpen = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernelOpen);

    cv::Mat kernelClose = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 7));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernelClose);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);

        // ignore trash, fields are big
        if (area > 50) {
            cv::Moments M = cv::moments(contour);
            if (M.m00 > 0) {
                int cx = static_cast<int>(M.m10 / M.m00);
                int cy = static_cast<int>(M.m01 / M.m00);

                // DONUT HOLE PROTECTION
                if (cx >= 0 && cx < mask.cols && cy >= 0 && cy < mask.rows) {
                    if (mask.at<uchar>(cy, cx) == 0) {
                        double minDist = 9999999.0;
                        int safeX = cx, safeY = cy;

                        for (int y = 0; y < mask.rows; y += 2) {
                            for (int x = 0; x < mask.cols; x += 2) {
                                if (mask.at<uchar>(y, x) > 0) {
                                    double dist = std::pow(x - cx, 2) + std::pow(y - cy, 2);
                                    if (dist < minDist) {
                                        minDist = dist;
                                        safeX = x;
                                        safeY = y;
                                    }
                                }
                            }
                        }
                        cx = safeX;
                        cy = safeY;
                    }
                }

                MatchResult res;
                res.found = true;
                res.x = cx + sideMargin;
                res.y = cy + topMargin;
                res.score = area;
                results.push_back(res);
            }
        }
    }
    return results;
}

// ==============================================================================
// GROWING FIELD DETECTOR (ORANGE/YELLOW - HSV)
// ==============================================================================
std::vector<MatchResult> FindGrowingFieldsByColor(const cv::Mat& screen) {
    std::vector<MatchResult> results;
    if (screen.empty()) return results;

    int topMargin = 70;
    int bottomMargin = 40;
    int sideMargin = 30;

    if (screen.rows <= topMargin + bottomMargin || screen.cols <= sideMargin * 2) return results;

    cv::Rect roiRect(sideMargin, topMargin, screen.cols - (sideMargin * 2), screen.rows - (topMargin + bottomMargin));
    cv::Mat searchArea = screen(roiRect);

    cv::Mat hsv, mask;
    cv::cvtColor(searchArea, hsv, cv::COLOR_BGR2HSV);

    const CropStateHsvRange& growingRange = GetCropStateHsvRange(CropVisualState::Growing);
    cv::inRange(hsv, growingRange.lower, growingRange.upper, mask);

    cv::Mat kernelOpen = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernelOpen);

    cv::Mat kernelClose = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 7));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernelClose);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        if (area <= 50) continue;

        cv::Moments M = cv::moments(contour);
        if (M.m00 <= 0) continue;

        int cx = static_cast<int>(M.m10 / M.m00);
        int cy = static_cast<int>(M.m01 / M.m00);

        if (cx >= 0 && cx < mask.cols && cy >= 0 && cy < mask.rows && mask.at<uchar>(cy, cx) == 0) {
            double minDist = 9999999.0;
            int safeX = cx;
            int safeY = cy;

            for (int y = 0; y < mask.rows; y += 2) {
                for (int x = 0; x < mask.cols; x += 2) {
                    if (mask.at<uchar>(y, x) == 0) continue;
                    double dist = std::pow(x - cx, 2) + std::pow(y - cy, 2);
                    if (dist < minDist) {
                        minDist = dist;
                        safeX = x;
                        safeY = y;
                    }
                }
            }
            cx = safeX;
            cy = safeY;
        }

        MatchResult res;
        res.found = true;
        res.x = cx + sideMargin;
        res.y = cy + topMargin;
        res.score = area;
        results.push_back(res);
    }

    return results;
}

