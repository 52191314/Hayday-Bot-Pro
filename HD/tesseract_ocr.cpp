#include "tesseract_ocr.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <sstream>
#define NOMINMAX
#include <windows.h>
#include <mutex> 
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

#pragma comment(lib, "tesseract55.lib")
#pragma comment(lib, "leptonica-1.85.0.lib")

// =========================================================================
// GLOBAL TESSERACT ENGINE
// =========================================================================
tesseract::TessBaseAPI* g_tessApi = nullptr;
std::mutex g_ocrMutex;
float g_OcrAnchorThreshold = 0.70f;
int g_WheatPreserveScanMode = 0;
int g_WheatPreserveCoordX = 201;
int g_WheatPreserveCoordY = 165;
int g_WheatPreserveRoiLeft = 181;
int g_WheatPreserveRoiTop = 151;
int g_WheatPreserveRoiRight = 255;
int g_WheatPreserveRoiBottom = 185;

static bool TryReadHudNumberByTemplates(const cv::Mat& crop, int& outCount, int& outScore);
std::string ResolveTemplatePath(const std::string& templatePath);

void InitGlobalTesseract() {
    if (g_tessApi != nullptr) return;

    char exePathBuf[MAX_PATH];
    GetModuleFileNameA(NULL, exePathBuf, MAX_PATH);
    std::string exePath = std::string(exePathBuf);
    std::string tessDataPath = exePath.substr(0, exePath.find_last_of("\\/")) + "\\tessdata";

    g_tessApi = new tesseract::TessBaseAPI();
    if (g_tessApi->Init(tessDataPath.c_str(), "eng")) {
        delete g_tessApi;
        g_tessApi = nullptr;
    }
}

// =========================================================================
//  TRASH CLEANER
// =========================================================================
std::string AdvancedOCRRead(cv::Mat crop, int threshVal, int invertColor, int noiseArea, const char* whitelist, tesseract::PageSegMode psm) {
    if (crop.empty() || g_tessApi == nullptr) return "";

    std::lock_guard<std::mutex> lock(g_ocrMutex);

    cv::Mat processed;
    cv::cvtColor(crop, processed, cv::COLOR_BGR2GRAY);
    cv::resize(processed, processed, cv::Size(), 2.0, 2.0, cv::INTER_CUBIC);

    if (invertColor == 0) {
        cv::threshold(processed, processed, threshVal, 255, cv::THRESH_BINARY);
    }
    else {
        cv::threshold(processed, processed, threshVal, 255, cv::THRESH_BINARY_INV);
    }

    if (noiseArea > 0) {
        cv::Mat invertedForNoise;
        cv::bitwise_not(processed, invertedForNoise);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(invertedForNoise, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

        for (size_t i = 0; i < contours.size(); i++) {
            if (cv::contourArea(contours[i]) < noiseArea) {
                cv::drawContours(invertedForNoise, contours, (int)i, cv::Scalar(0), cv::FILLED);
            }
        }
        cv::bitwise_not(invertedForNoise, processed);
    }

    g_tessApi->SetVariable("tessedit_char_whitelist", whitelist);
    g_tessApi->SetPageSegMode(psm);
    g_tessApi->SetImage((uchar*)processed.data, processed.cols, processed.rows, 1, processed.step);

    char* outText = g_tessApi->GetUTF8Text();
    std::string result(outText ? outText : "");
    delete[] outText;
    g_tessApi->ClearAdaptiveClassifier();

    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
    result.erase(std::remove(result.begin(), result.end(), ' '), result.end());

    return result;
}

// =========================================================================
// 1. COIN AND DIAMOND COUNT READER
// =========================================================================
GameNumberDebugInfo ReadGameNumberDebug(cv::Mat screen, cv::Rect roi, std::string debugName) {
    GameNumberDebugInfo info;
    if (screen.empty() || roi.x < 0 || roi.y < 0 || roi.width <= 0 || roi.height <= 0 ||
        roi.x + roi.width > screen.cols || roi.y + roi.height > screen.rows) {
        return info;
    }

    cv::Mat crop = screen(roi);
    int templateCount = 0;
    int templateScore = 0;
    bool isHudCounter = (debugName == "coin" || debugName == "dia");
    if (isHudCounter && TryReadHudNumberByTemplates(crop, templateCount, templateScore) && templateScore >= 58) {
        info.count = templateCount;
        info.score = templateScore;
        info.digits = std::to_string(templateCount);
        info.readMethod = "template";
        return info;
    }

    cv::Mat resizedImage, bw;
    cv::resize(crop, resizedImage, cv::Size(), 3.1, 3.1, cv::INTER_CUBIC);
    cv::inRange(resizedImage, cv::Scalar(132, 132, 132), cv::Scalar(255, 255, 255), bw);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(bw.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        cv::Rect box = cv::boundingRect(contours[i]);
        if (area < 80 || box.height < 10) {
            cv::drawContours(bw, contours, static_cast<int>(i), cv::Scalar(0), cv::FILLED);
        }
    }

    cv::bitwise_not(bw, bw);
    cv::copyMakeBorder(bw, bw, 10, 10, 10, 10, cv::BORDER_CONSTANT, cv::Scalar(255));
    cv::imwrite("C:\\Users\\Public\\debug_" + debugName + ".png", bw);

    if (g_tessApi == nullptr) InitGlobalTesseract();
    std::lock_guard<std::mutex> lock(g_ocrMutex);

    g_tessApi->SetVariable("tessedit_char_whitelist", "0123456789Il|i!OoSsB/Zz");
    g_tessApi->SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);
    g_tessApi->SetImage((uchar*)bw.data, bw.cols, bw.rows, 1, bw.step);

    char* outText = g_tessApi->GetUTF8Text();
    std::string finalResult(outText ? outText : "");
    int confidence = g_tessApi->MeanTextConf();
    delete[] outText;
    g_tessApi->ClearAdaptiveClassifier();

    for (char& c : finalResult) {
        if (c == 'I' || c == 'l' || c == '|' || c == 'i' || c == '!') c = '1';
        if (c == 'O' || c == 'o') c = '0';
        if (c == 'S' || c == 's') c = '5';
        if (c == 'B') c = '8';
        if (c == '/') c = '7';
        if (c == 'Z' || c == 'z') c = '2';
    }

    for (char c : finalResult) {
        if (isdigit(static_cast<unsigned char>(c))) info.digits += c;
    }

    info.score = (std::max)(0, confidence);
    info.readMethod = "tesseract";
    if (info.digits.empty()) {
        info.readMethod = "unreadable";
        info.score = 0;
        return info;
    }

    try {
        info.count = std::stoi(info.digits);
    }
    catch (...) {
        info.count = 0;
        info.score = 0;
        info.readMethod = "unreadable";
    }
    return info;
}

int ReadGameNumber(cv::Mat screen, cv::Rect roi, std::string debugName) {
    return ReadGameNumberDebug(screen, roi, std::move(debugName)).count;
}

// =========================================================================
// 2. FARM name reader
// =========================================================================
std::string ReadFarmName(cv::Mat screen, cv::Rect roi) {
    if (screen.empty() || roi.x < 0 || roi.y < 0 || roi.width <= 0 || roi.height <= 0 ||
        roi.x + roi.width > screen.cols || roi.y + roi.height > screen.rows) return "Unknown";

    if (g_tessApi == nullptr) InitGlobalTesseract();
    cv::Mat crop = screen(roi);

    std::string finalResult = AdvancedOCRRead(crop, 200, 1, 0,
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ", tesseract::PSM_SINGLE_BLOCK);

    return finalResult.empty() ? "Unknown" : finalResult;
}


static int OCRDigitsBestEffort(const cv::Mat& crop) {
    if (crop.empty() || g_tessApi == nullptr) return 0;
    std::lock_guard<std::mutex> lock(g_ocrMutex);

    std::vector<cv::Mat> variants;
    cv::Mat baseGray;
    if (crop.channels() == 3) {
        cv::cvtColor(crop, baseGray, cv::COLOR_BGR2GRAY);
    } else {
        baseGray = crop.clone();
    }

    cv::Mat upGray;
    cv::resize(baseGray, upGray, cv::Size(), 4.0, 4.0, cv::INTER_CUBIC);
    variants.push_back(upGray);

    cv::Mat otsu;
    cv::threshold(upGray, otsu, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    variants.push_back(otsu);

    cv::Mat adap;
    cv::adaptiveThreshold(upGray, adap, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 31, 7);
    variants.push_back(adap);

    cv::Mat inv;
    cv::bitwise_not(otsu, inv);
    variants.push_back(inv);

    if (crop.channels() == 3) {
        cv::Mat hsv;
        cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);
        cv::Mat whiteMask;
        cv::inRange(hsv, cv::Scalar(0, 0, 150), cv::Scalar(180, 95, 255), whiteMask);
        cv::Mat whiteUp;
        cv::resize(whiteMask, whiteUp, cv::Size(), 5.0, 5.0, cv::INTER_NEAREST);
        variants.push_back(whiteUp);

        cv::Mat whiteInv;
        cv::bitwise_not(whiteUp, whiteInv);
        variants.push_back(whiteInv);
    }

    std::string bestDigits;
    int bestScore = (std::numeric_limits<int>::min)();
    for (auto &v : variants) {
        cv::Mat padded;
        cv::copyMakeBorder(v, padded, 10, 10, 10, 10, cv::BORDER_CONSTANT, cv::Scalar(255));
        g_tessApi->SetVariable("tessedit_char_whitelist", "0123456789");
        g_tessApi->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
        g_tessApi->SetImage((uchar*)padded.data, padded.cols, padded.rows, 1, padded.step);
        char* outText = g_tessApi->GetUTF8Text();
        std::string raw(outText ? outText : "");
        int confidence = g_tessApi->MeanTextConf();
        delete[] outText;
        g_tessApi->ClearAdaptiveClassifier();
        std::string digits;
        for (char c : raw) if (isdigit((unsigned char)c)) digits += c;
        if (digits.empty()) continue;

        int score = confidence + static_cast<int>(digits.size()) * 10;
        if (digits.size() > 4) score -= static_cast<int>(digits.size() - 4) * 25;
        if (score > bestScore || (score == bestScore && digits.size() < bestDigits.size())) {
            bestScore = score;
            bestDigits = digits;
        }
    }
    if (bestDigits.empty()) return 0;
    try { return std::stoi(bestDigits); } catch (...) { return 0; }
}

struct OCRDigitsResult {
    int count = 0;
    int score = 0;
    std::string digits;
    std::string method;
};

static OCRDigitsResult OCRDigitsBestEffortDetailed(const cv::Mat& crop) {
    OCRDigitsResult result;
    if (crop.empty() || g_tessApi == nullptr) return result;
    std::lock_guard<std::mutex> lock(g_ocrMutex);

    std::vector<cv::Mat> variants;
    cv::Mat baseGray;
    if (crop.channels() == 3) {
        cv::cvtColor(crop, baseGray, cv::COLOR_BGR2GRAY);
    }
    else {
        baseGray = crop.clone();
    }

    cv::Mat upGray;
    cv::resize(baseGray, upGray, cv::Size(), 4.0, 4.0, cv::INTER_CUBIC);
    variants.push_back(upGray);

    cv::Mat otsu;
    cv::threshold(upGray, otsu, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    variants.push_back(otsu);

    cv::Mat adap;
    cv::adaptiveThreshold(upGray, adap, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 31, 7);
    variants.push_back(adap);

    cv::Mat inv;
    cv::bitwise_not(otsu, inv);
    variants.push_back(inv);

    if (crop.channels() == 3) {
        cv::Mat hsv;
        cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);
        cv::Mat whiteMask;
        cv::inRange(hsv, cv::Scalar(0, 0, 150), cv::Scalar(180, 95, 255), whiteMask);
        cv::Mat whiteUp;
        cv::resize(whiteMask, whiteUp, cv::Size(), 5.0, 5.0, cv::INTER_NEAREST);
        variants.push_back(whiteUp);

        cv::Mat whiteInv;
        cv::bitwise_not(whiteUp, whiteInv);
        variants.push_back(whiteInv);
    }

    int bestScore = (std::numeric_limits<int>::min)();
    std::string bestDigits;
    for (auto& v : variants) {
        cv::Mat padded;
        cv::copyMakeBorder(v, padded, 10, 10, 10, 10, cv::BORDER_CONSTANT, cv::Scalar(255));
        g_tessApi->SetVariable("tessedit_char_whitelist", "0123456789");
        g_tessApi->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
        g_tessApi->SetImage((uchar*)padded.data, padded.cols, padded.rows, 1, padded.step);
        char* outText = g_tessApi->GetUTF8Text();
        std::string raw(outText ? outText : "");
        int confidence = g_tessApi->MeanTextConf();
        delete[] outText;
        g_tessApi->ClearAdaptiveClassifier();
        std::string digits;
        for (char c : raw) if (isdigit((unsigned char)c)) digits += c;
        if (digits.empty()) continue;

        int score = confidence + static_cast<int>(digits.size()) * 10;
        if (digits.size() > 4) score -= static_cast<int>(digits.size() - 4) * 25;
        if (score > bestScore || (score == bestScore && digits.size() < bestDigits.size())) {
            bestScore = score;
            bestDigits = digits;
        }
    }

    result.score = bestScore == (std::numeric_limits<int>::min)() ? 0 : bestScore;
    result.digits = bestDigits;
    result.method = bestDigits.empty() ? "unreadable" : "tesseract";
    if (!bestDigits.empty()) {
        try { result.count = std::stoi(bestDigits); } catch (...) { result.count = 0; }
    }
    return result;
}

static cv::Rect ClampRectToImage(const cv::Rect& rect, const cv::Size& size) {
    int x = (std::max)(0, rect.x);
    int y = (std::max)(0, rect.y);
    int right = (std::min)(size.width, rect.x + rect.width);
    int bottom = (std::min)(size.height, rect.y + rect.height);
    return cv::Rect(x, y, (std::max)(0, right - x), (std::max)(0, bottom - y));
}

static cv::Mat PrepareDigitForTesseract(const cv::Mat& digitMask) {
    cv::Mat gray;
    if (digitMask.channels() == 3) {
        cv::cvtColor(digitMask, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = digitMask.clone();
    }

    cv::Mat resized;
    cv::resize(gray, resized, cv::Size(), 4.0, 4.0, cv::INTER_NEAREST);

    cv::Mat binary;
    cv::threshold(resized, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::Mat tesseractReady;
    cv::bitwise_not(binary, tesseractReady);
    cv::copyMakeBorder(tesseractReady, tesseractReady, 12, 12, 12, 12, cv::BORDER_CONSTANT, cv::Scalar(255));
    return tesseractReady;
}

static cv::Mat ExtractDominantSingleDigitMask(const cv::Mat& digitMask) {
    if (digitMask.empty()) return cv::Mat();

    cv::Mat gray;
    if (digitMask.channels() == 3) {
        cv::cvtColor(digitMask, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = digitMask.clone();
    }

    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::Mat labels, stats, centroids;
    int componentCount = cv::connectedComponentsWithStats(binary, labels, stats, centroids, 8, CV_32S);
    if (componentCount <= 1) {
        std::vector<cv::Point> nonZero;
        cv::findNonZero(binary, nonZero);
        if (nonZero.empty()) return cv::Mat();
        return binary(cv::boundingRect(nonZero)).clone();
    }

    int bestLabel = -1;
    double bestScore = -1.0;
    for (int label = 1; label < componentCount; ++label) {
        int x = stats.at<int>(label, cv::CC_STAT_LEFT);
        int y = stats.at<int>(label, cv::CC_STAT_TOP);
        int w = stats.at<int>(label, cv::CC_STAT_WIDTH);
        int h = stats.at<int>(label, cv::CC_STAT_HEIGHT);
        int area = stats.at<int>(label, cv::CC_STAT_AREA);
        if (area <= 0 || w <= 0 || h <= 0) continue;

        double widthPenalty = static_cast<double>((std::max)(0, w - (binary.cols * 3) / 4)) * 0.8;
        double centerX = x + (w * 0.5);
        double horizontalBias = 1.0 - std::abs(centerX - binary.cols * 0.58) / (std::max)(1.0, binary.cols * 0.58);
        double centerY = y + (h * 0.5);
        double bottomBias = 1.0 - std::abs(centerY - binary.rows * 0.60) / (std::max)(1.0, binary.rows * 0.60);
        double edgePenalty = (x <= 1 ? 24.0 : 0.0) + ((x + w) >= (binary.cols - 1) ? 10.0 : 0.0);
        double score = area + h * 4.0 + w * 1.5 + bottomBias * 32.0 + horizontalBias * 42.0 - widthPenalty - edgePenalty;
        if (score > bestScore) {
            bestScore = score;
            bestLabel = label;
        }
    }

    if (bestLabel == -1) return cv::Mat();

    cv::Rect primaryRect(
        stats.at<int>(bestLabel, cv::CC_STAT_LEFT),
        stats.at<int>(bestLabel, cv::CC_STAT_TOP),
        stats.at<int>(bestLabel, cv::CC_STAT_WIDTH),
        stats.at<int>(bestLabel, cv::CC_STAT_HEIGHT));
    int primaryArea = stats.at<int>(bestLabel, cv::CC_STAT_AREA);

    cv::Mat filtered = cv::Mat::zeros(binary.size(), CV_8U);
    for (int label = 1; label < componentCount; ++label) {
        int x = stats.at<int>(label, cv::CC_STAT_LEFT);
        int y = stats.at<int>(label, cv::CC_STAT_TOP);
        int w = stats.at<int>(label, cv::CC_STAT_WIDTH);
        int h = stats.at<int>(label, cv::CC_STAT_HEIGHT);
        int area = stats.at<int>(label, cv::CC_STAT_AREA);
        if (area <= 0 || w <= 0 || h <= 0) continue;

        bool keep = (label == bestLabel);
        if (!keep) {
            cv::Rect rect(x, y, w, h);
            int gapLeft = primaryRect.x - (rect.x + rect.width);
            int gapRight = rect.x - (primaryRect.x + primaryRect.width);
            int horizontalGap = (std::max)(0, (std::max)(gapLeft, gapRight));
            int overlapTop = (std::max)(primaryRect.y, rect.y);
            int overlapBottom = (std::min)(primaryRect.y + primaryRect.height, rect.y + rect.height);
            int verticalOverlap = (std::max)(0, overlapBottom - overlapTop);
            int gapAbove = primaryRect.y - (rect.y + rect.height);
            int gapBelow = rect.y - (primaryRect.y + primaryRect.height);
            int verticalGap = (std::max)(0, (std::max)(gapAbove, gapBelow));

            bool areaOk = area >= (std::max)(10, primaryArea / 10);
            bool horizontallyNear = horizontalGap <= (std::max)(3, primaryRect.width / 3);
            bool verticallyRelated = verticalOverlap >= (std::max)(2, (std::min)(primaryRect.height, rect.height) / 4) ||
                verticalGap <= (std::max)(3, primaryRect.height / 6);
            bool notLooseEdgeNoise = !(x <= 1 && horizontalGap > 1 && area < primaryArea / 3);
            keep = areaOk && horizontallyNear && verticallyRelated && notLooseEdgeNoise;
        }

        if (keep) filtered.setTo(255, labels == label);
    }

    std::vector<cv::Point> nonZero;
    cv::findNonZero(filtered, nonZero);
    if (nonZero.empty()) return cv::Mat();
    return filtered(cv::boundingRect(nonZero)).clone();
}

static cv::Mat NormalizeBinaryDigitMask(const cv::Mat& digitMask, const cv::Size& targetSize = cv::Size(30, 48)) {
    cv::Mat cropped = ExtractDominantSingleDigitMask(digitMask);
    if (cropped.empty()) return cv::Mat();

    cv::Mat canvas = cv::Mat::zeros(targetSize, CV_8U);
    int availableW = targetSize.width - 4;
    int availableH = targetSize.height - 4;
    if (availableW <= 0 || availableH <= 0) return cv::Mat();

    double scaleX = static_cast<double>(availableW) / (std::max)(1, cropped.cols);
    double scaleY = static_cast<double>(availableH) / (std::max)(1, cropped.rows);
    double scale = (std::min)(scaleX, scaleY);

    cv::Mat resized;
    cv::resize(cropped, resized, cv::Size(), scale, scale, cv::INTER_NEAREST);
    int dstX = (targetSize.width - resized.cols) / 2;
    int dstY = (targetSize.height - resized.rows) / 2;
    cv::Rect dstRect(dstX, dstY, resized.cols, resized.rows);
    resized.copyTo(canvas(dstRect));
    return canvas;
}

static bool BuildWheatPreserveDigitMask(const cv::Mat& crop, cv::Mat& outMask, cv::Rect& outBounds);
static std::vector<cv::Rect> SegmentWheatDigitRects(const cv::Mat& digitMask);

struct HayDayDigitTemplateMatch {
    int digit = -1;
    int score = 0;
    double similarity = 0.0;
};

static HayDayDigitTemplateMatch MatchHayDayDigitTemplate(const cv::Mat& digitMask);

static double RegionFillRatio(const cv::Mat& mask, double fx0, double fy0, double fx1, double fy1) {
    if (mask.empty()) return 0.0;
    int x0 = (std::max)(0, (std::min)(mask.cols - 1, static_cast<int>(std::floor(mask.cols * fx0))));
    int y0 = (std::max)(0, (std::min)(mask.rows - 1, static_cast<int>(std::floor(mask.rows * fy0))));
    int x1 = (std::max)(x0 + 1, (std::min)(mask.cols, static_cast<int>(std::ceil(mask.cols * fx1))));
    int y1 = (std::max)(y0 + 1, (std::min)(mask.rows, static_cast<int>(std::ceil(mask.rows * fy1))));
    cv::Rect roi(x0, y0, x1 - x0, y1 - y0);
    return static_cast<double>(cv::countNonZero(mask(roi))) / static_cast<double>(roi.area());
}

static double HorizontalCenterOfMass(const cv::Mat& mask, double fy0, double fy1) {
    if (mask.empty()) return 0.5;

    int y0 = (std::max)(0, (std::min)(mask.rows - 1, static_cast<int>(std::floor(mask.rows * fy0))));
    int y1 = (std::max)(y0 + 1, (std::min)(mask.rows, static_cast<int>(std::ceil(mask.rows * fy1))));
    cv::Mat roi = mask.rowRange(y0, y1);

    double weightedX = 0.0;
    double total = 0.0;
    for (int y = 0; y < roi.rows; ++y) {
        const uchar* row = roi.ptr<uchar>(y);
        for (int x = 0; x < roi.cols; ++x) {
            if (row[x] > 0) {
                weightedX += static_cast<double>(x);
                total += 1.0;
            }
        }
    }

    if (total <= 0.0) return 0.5;
    return weightedX / total / (std::max)(1, mask.cols - 1);
}

struct ShapeDigitGuess {
    int digit = -1;
    int certainty = 0;
};

static ShapeDigitGuess GuessHayDayDigitByShape(const cv::Mat& digitMask) {
    ShapeDigitGuess guess;
    cv::Mat normalized = NormalizeBinaryDigitMask(digitMask);
    if (normalized.empty()) return guess;

    std::vector<cv::Point> nonZero;
    cv::findNonZero(normalized, nonZero);
    if (nonZero.empty()) return guess;

    cv::Rect bbox = cv::boundingRect(nonZero);
    double aspect = static_cast<double>(bbox.width) / static_cast<double>((std::max)(1, bbox.height));
    double left = RegionFillRatio(normalized, 0.0, 0.0, 0.33, 1.0);
    double center = RegionFillRatio(normalized, 0.33, 0.0, 0.66, 1.0);
    double right = RegionFillRatio(normalized, 0.66, 0.0, 1.0, 1.0);
    double upperLeft = RegionFillRatio(normalized, 0.0, 0.0, 0.33, 0.33);
    double midLeft = RegionFillRatio(normalized, 0.0, 0.33, 0.33, 0.66);
    double lowerLeft = RegionFillRatio(normalized, 0.0, 0.66, 0.33, 1.0);
    double upperRight = RegionFillRatio(normalized, 0.66, 0.0, 1.0, 0.33);
    double midRight = RegionFillRatio(normalized, 0.66, 0.33, 1.0, 0.66);
    double lowerRight = RegionFillRatio(normalized, 0.66, 0.66, 1.0, 1.0);
    double upperCenter = RegionFillRatio(normalized, 0.33, 0.0, 0.66, 0.33);
    double lowerCenter = RegionFillRatio(normalized, 0.33, 0.66, 0.66, 1.0);
    double bottomLeft = RegionFillRatio(normalized, 0.0, 0.78, 0.33, 1.0);
    double top = RegionFillRatio(normalized, 0.0, 0.0, 1.0, 0.26);
    double middle = RegionFillRatio(normalized, 0.0, 0.37, 1.0, 0.63);
    double bottom = RegionFillRatio(normalized, 0.0, 0.74, 1.0, 1.0);
    double topCenterX = HorizontalCenterOfMass(normalized, 0.0, 0.33);
    double midCenterX = HorizontalCenterOfMass(normalized, 0.33, 0.66);
    double bottomCenterX = HorizontalCenterOfMass(normalized, 0.66, 1.0);

    double leftLean = (std::max)(0.0, topCenterX - bottomCenterX);
    double rightLean = (std::max)(0.0, bottomCenterX - topCenterX);

    double score1 =
        (std::max)(0.0, 0.52 - aspect) * 4.0 +
        (std::max)(0.0, 0.16 - left) * 4.5 +
        (std::max)(0.0, 0.14 - lowerLeft) * 4.0 +
        (std::max)(0.0, 0.12 - bottomLeft) * 4.0 +
        (std::max)(0.0, 0.32 - top) * 1.4 +
        (std::max)(0.0, 0.22 - lowerCenter) * 1.0;

    double score2 =
        top * 1.3 +
        bottom * 1.9 +
        lowerLeft * 3.2 +
        bottomLeft * 3.0 +
        midLeft * 1.7 +
        lowerCenter * 1.3 +
        (std::max)(0.0, 0.58 - bottomCenterX) * 2.8 +
        leftLean * 1.4 -
        lowerRight * 0.3;

    double score3 =
        top * 1.1 +
        middle * 1.8 +
        bottom * 1.2 +
        right * 2.8 +
        upperRight * 1.1 +
        midRight * 1.4 +
        lowerRight * 1.5 +
        (std::max)(0.0, 0.14 - lowerLeft) * 4.0 +
        (std::max)(0.0, 0.16 - bottomLeft) * 2.0 +
        (std::max)(0.0, 0.55 - upperLeft) * 0.2 +
        (std::max)(0.0, bottomCenterX - 0.54) * 2.4 -
        upperLeft * 0.9;

    double score7 =
        top * 2.3 +
        upperRight * 1.9 +
        midRight * 1.8 +
        lowerRight * 1.0 +
        right * 1.5 +
        upperCenter * 0.6 +
        (std::max)(0.0, aspect - 0.38) * 2.0 +
        (std::max)(0.0, bottomCenterX - 0.60) * 3.0 +
        (std::max)(0.0, 0.14 - lowerLeft) * 3.6 +
        (std::max)(0.0, 0.12 - bottomLeft) * 3.6 -
        bottom * 0.5;

    struct CandidateScore { int digit; double score; };
    CandidateScore scores[] = {
        {1, score1},
        {2, score2},
        {3, score3},
        {7, score7},
    };

    int bestDigit = -1;
    double bestScore = -1.0;
    double secondScore = -1.0;
    for (const auto& candidate : scores) {
        if (candidate.score > bestScore) {
            secondScore = bestScore;
            bestScore = candidate.score;
            bestDigit = candidate.digit;
        }
        else if (candidate.score > secondScore) {
            secondScore = candidate.score;
        }
    }

    if (bestDigit == -1) return guess;

    double separation = bestScore - secondScore;
    int certainty = static_cast<int>(bestScore * 12.0 + separation * 30.0);
    if (bestDigit == 1 && aspect < 0.44 && lowerLeft < 0.10 && bottomLeft < 0.09) certainty += 18;
    if (bestDigit == 7 && top > 0.24 && lowerLeft < 0.10 && bottomLeft < 0.09) certainty += 18;
    if (bestDigit == 2 && lowerLeft > 0.12 && bottomCenterX < 0.56) certainty += 14;
    if (bestDigit == 3 && right > 0.18 && lowerLeft < 0.10) certainty += 14;

    guess.digit = bestDigit;
    guess.certainty = (std::max)(0, (std::min)(95, certainty));
    return guess;
}

static OCRDigitsResult OCRSingleDigitBestEffort(const cv::Mat& digitMask) {
    OCRDigitsResult result;
    if (digitMask.empty()) return result;
    cv::Mat cleanedMask = ExtractDominantSingleDigitMask(digitMask);
    if (cleanedMask.empty()) cleanedMask = digitMask.clone();

    HayDayDigitTemplateMatch templateMatch = MatchHayDayDigitTemplate(cleanedMask);
    if (templateMatch.digit != -1 && templateMatch.score >= 68) {
        result.digits = std::string(1, static_cast<char>('0' + templateMatch.digit));
        result.count = templateMatch.digit;
        result.score = templateMatch.score;
        result.method = "template";
        return result;
    }

    ShapeDigitGuess shapeGuess = GuessHayDayDigitByShape(cleanedMask);
    if (shapeGuess.digit != -1 && shapeGuess.certainty >= 58) {
        result.digits = std::string(1, static_cast<char>('0' + shapeGuess.digit));
        result.count = shapeGuess.digit;
        result.score = shapeGuess.certainty;
        result.method = "shape";
        return result;
    }

    if (g_tessApi != nullptr) {
        std::lock_guard<std::mutex> lock(g_ocrMutex);

        std::vector<cv::Mat> variants;
        cv::Mat baseGray;
        if (cleanedMask.channels() == 3) {
            cv::cvtColor(cleanedMask, baseGray, cv::COLOR_BGR2GRAY);
        }
        else {
            baseGray = cleanedMask.clone();
        }
        variants.push_back(baseGray);

        cv::Mat dilated;
        cv::dilate(baseGray, dilated, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2)));
        variants.push_back(dilated);

        cv::Mat eroded;
        cv::erode(baseGray, eroded, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2)));
        variants.push_back(eroded);

        int bestScore = (std::numeric_limits<int>::min)();
        std::string bestDigit;
        for (const auto& variant : variants) {
            cv::Mat prepared = PrepareDigitForTesseract(variant);
            g_tessApi->SetVariable("tessedit_char_whitelist", "0123456789");
            g_tessApi->SetPageSegMode(tesseract::PSM_SINGLE_CHAR);
            g_tessApi->SetImage((uchar*)prepared.data, prepared.cols, prepared.rows, 1, prepared.step);

            char* outText = g_tessApi->GetUTF8Text();
            std::string raw(outText ? outText : "");
            int confidence = g_tessApi->MeanTextConf();
            delete[] outText;
            g_tessApi->ClearAdaptiveClassifier();

            std::string digits;
            for (char c : raw) {
                if (isdigit((unsigned char)c)) digits += c;
            }
            if (digits.empty()) continue;

            std::string candidate(1, digits[0]);
            int score = confidence - static_cast<int>((std::max)(0, static_cast<int>(digits.size()) - 1)) * 35;
            score = (std::max)(0, score);
            if (score > bestScore) {
                bestScore = score;
                bestDigit = candidate;
            }
        }

        if (!bestDigit.empty()) {
            result.digits = bestDigit;
            result.count = bestDigit[0] - '0';
            result.score = bestScore == (std::numeric_limits<int>::min)() ? 0 : bestScore;
            result.method = "tesseract";
        }
    }

    if (templateMatch.digit != -1) {
        bool noDigitYet = result.digits.empty();
        bool lowConfidenceDigit = !result.digits.empty() && result.score < templateMatch.score - 6;
        bool ambiguousDigit = !result.digits.empty() &&
            (result.digits[0] == '1' || result.digits[0] == '2' || result.digits[0] == '3' ||
                result.digits[0] == '4' || result.digits[0] == '5' || result.digits[0] == '7');

        if (noDigitYet || (ambiguousDigit && lowConfidenceDigit && templateMatch.score >= 54)) {
            result.digits = std::string(1, static_cast<char>('0' + templateMatch.digit));
            result.count = templateMatch.digit;
            result.score = templateMatch.score;
            result.method = "template";
        }
        else if (!result.digits.empty() && result.digits[0] == static_cast<char>('0' + templateMatch.digit)) {
            result.score = (std::max)(result.score, templateMatch.score);
            result.method = "template";
        }
    }

    if (shapeGuess.digit != -1) {
        bool noTesseractDigit = result.digits.empty();
        bool sameDigit = !result.digits.empty() && result.digits[0] == static_cast<char>('0' + shapeGuess.digit);
        bool tesseractAmbiguous = !result.digits.empty() &&
            (result.digits[0] == '1' || result.digits[0] == '2' || result.digits[0] == '3' || result.digits[0] == '7');

        if (noTesseractDigit || (tesseractAmbiguous && result.score < shapeGuess.certainty - 8 && shapeGuess.certainty >= 46)) {
            result.digits = std::string(1, static_cast<char>('0' + shapeGuess.digit));
            result.count = shapeGuess.digit;
            result.score = (std::max)(result.score, shapeGuess.certainty);
            result.method = "shape";
        }
        else if (sameDigit) {
            result.score = (std::max)(result.score, shapeGuess.certainty);
            result.method = "shape";
        }
    }

    if (result.method.empty()) result.method = "unreadable";
    return result;
}

static bool BuildWheatPreserveDigitMask(const cv::Mat& crop, cv::Mat& outMask, cv::Rect& outBounds) {
    outMask.release();
    outBounds = cv::Rect();
    if (crop.empty() || crop.channels() != 3) return false;

    cv::Mat upscaled;
    cv::resize(crop, upscaled, cv::Size(), 5.0, 5.0, cv::INTER_CUBIC);

    cv::Mat hsv;
    cv::cvtColor(upscaled, hsv, cv::COLOR_BGR2HSV);

    cv::Mat whiteMask;
    cv::inRange(hsv, cv::Scalar(0, 0, 170), cv::Scalar(180, 110, 255), whiteMask);

    cv::Mat gray;
    cv::cvtColor(upscaled, gray, cv::COLOR_BGR2GRAY);
    cv::Mat brightMask;
    cv::threshold(gray, brightMask, 172, 255, cv::THRESH_BINARY);
    cv::bitwise_and(whiteMask, brightMask, whiteMask);

    cv::Mat yellowMask;
    cv::inRange(hsv, cv::Scalar(12, 70, 90), cv::Scalar(50, 255, 255), yellowMask);
    cv::morphologyEx(yellowMask, yellowMask, cv::MORPH_CLOSE, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3)));

    cv::Mat nonYellow;
    cv::bitwise_not(yellowMask, nonYellow);

    cv::Mat digitMask;
    cv::bitwise_and(whiteMask, nonYellow, digitMask);
    // Keep strokes vertically coherent without welding adjacent digits together.
    cv::morphologyEx(digitMask, digitMask, cv::MORPH_CLOSE, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1, 3)));
    cv::morphologyEx(digitMask, digitMask, cv::MORPH_OPEN, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2)));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(digitMask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    int minArea = (std::max)(18, (digitMask.cols * digitMask.rows) / 700);
    for (size_t i = 0; i < contours.size(); ++i) {
        if (cv::contourArea(contours[i]) < minArea) {
            cv::drawContours(digitMask, contours, static_cast<int>(i), cv::Scalar(0), cv::FILLED);
        }
    }

    cv::Mat labels, stats, centroids;
    int componentCount = cv::connectedComponentsWithStats(digitMask, labels, stats, centroids, 8, CV_32S);
    if (componentCount > 1) {
        cv::Mat filtered = cv::Mat::zeros(digitMask.size(), CV_8U);
        int minKeepHeight = (std::max)(12, digitMask.rows / 2);
        int minKeepBottom = (digitMask.rows * 11) / 20;

        for (int label = 1; label < componentCount; ++label) {
            int x = stats.at<int>(label, cv::CC_STAT_LEFT);
            int y = stats.at<int>(label, cv::CC_STAT_TOP);
            int w = stats.at<int>(label, cv::CC_STAT_WIDTH);
            int h = stats.at<int>(label, cv::CC_STAT_HEIGHT);
            int area = stats.at<int>(label, cv::CC_STAT_AREA);
            if (area < minArea) continue;
            if (h < minKeepHeight) continue;
            if (y + h < minKeepBottom) continue;
            if (w <= 1) continue;
            filtered.setTo(255, labels == label);
        }

        if (cv::countNonZero(filtered) > 0) {
            digitMask = filtered;
        }
    }

    std::vector<cv::Point> nonZero;
    cv::findNonZero(digitMask, nonZero);
    if (nonZero.empty()) return false;

    outBounds = ClampRectToImage(cv::boundingRect(nonZero), digitMask.size());
    if (outBounds.width <= 0 || outBounds.height <= 0) return false;
    outMask = digitMask(outBounds).clone();
    return !outMask.empty();
}

static void AppendDigitRect(std::vector<cv::Rect>& outRects, const cv::Rect& rect, const cv::Mat& digitMask) {
    cv::Rect expanded = ClampRectToImage(cv::Rect(rect.x - 2, rect.y - 2, rect.width + 4, rect.height + 4), digitMask.size());
    if (expanded.width <= 0 || expanded.height <= 0) return;
    if (expanded.height < (std::max)(8, digitMask.rows / 3)) return;
    outRects.push_back(expanded);
}

static std::vector<cv::Rect> SegmentWheatDigitRects(const cv::Mat& digitMask) {
    std::vector<cv::Rect> digitRects;
    if (digitMask.empty()) return digitRects;

    std::vector<int> columnCounts(digitMask.cols, 0);
    int minForegroundRows = (std::max)(2, digitMask.rows / 12);
    for (int x = 0; x < digitMask.cols; ++x) {
        columnCounts[x] = cv::countNonZero(digitMask.col(x));
    }

    std::vector<cv::Rect> spans;
    bool inSpan = false;
    int startX = 0;
    for (int x = 0; x < digitMask.cols; ++x) {
        bool active = columnCounts[x] >= minForegroundRows;
        if (active && !inSpan) {
            startX = x;
            inSpan = true;
        }
        else if (!active && inSpan) {
            spans.emplace_back(startX, 0, x - startX, digitMask.rows);
            inSpan = false;
        }
    }
    if (inSpan) spans.emplace_back(startX, 0, digitMask.cols - startX, digitMask.rows);

    if (spans.empty()) return digitRects;

    std::vector<cv::Rect> merged;
    int maxGap = 1;
    for (const auto& span : spans) {
        if (!merged.empty()) {
            cv::Rect& last = merged.back();
            int gap = span.x - (last.x + last.width);
            if (gap <= maxGap && (span.width < (std::max)(6, digitMask.cols / 20) || last.width < (std::max)(6, digitMask.cols / 20))) {
                int right = span.x + span.width;
                last.width = right - last.x;
                continue;
            }
        }
        merged.push_back(span);
    }

    for (const auto& span : merged) {
        if (span.width < (std::max)(4, digitMask.cols / 80)) continue;

        cv::Mat spanMask = digitMask(span);
        std::vector<int> rowCounts(spanMask.rows, 0);
        int minForegroundCols = (std::max)(2, spanMask.cols / 8);
        int top = -1;
        int bottom = -1;
        for (int y = 0; y < spanMask.rows; ++y) {
            rowCounts[y] = cv::countNonZero(spanMask.row(y));
            if (rowCounts[y] >= minForegroundCols) {
                if (top == -1) top = y;
                bottom = y;
            }
        }
        if (top == -1 || bottom <= top) continue;

        cv::Rect digitRect(span.x, top, span.width, bottom - top + 1);
        int splitThreshold = static_cast<int>(digitRect.height * 0.72f);
        if (digitRect.width > splitThreshold) {
            std::vector<int> localCounts(digitRect.width, 0);
            for (int x = 0; x < digitRect.width; ++x) {
                localCounts[x] = cv::countNonZero(digitMask.col(digitRect.x + x).rowRange(digitRect.y, digitRect.y + digitRect.height));
            }

            int searchStart = (std::max)(1, digitRect.width / 4);
            int searchEnd = (std::min)(digitRect.width - 1, (digitRect.width * 3) / 4);
            int valleyIndex = -1;
            int valleyCount = (std::numeric_limits<int>::max)();
            for (int x = searchStart; x < searchEnd; ++x) {
                if (localCounts[x] < valleyCount) {
                    valleyCount = localCounts[x];
                    valleyIndex = x;
                }
            }

            int localPeak = 0;
            for (int value : localCounts) localPeak = (std::max)(localPeak, value);
            if (valleyIndex > 1 && valleyIndex < digitRect.width - 2 && valleyCount <= (std::max)(1, localPeak / 3)) {
                cv::Rect leftRect(digitRect.x, digitRect.y, valleyIndex, digitRect.height);
                cv::Rect rightRect(digitRect.x + valleyIndex, digitRect.y, digitRect.width - valleyIndex, digitRect.height);
                AppendDigitRect(digitRects, leftRect, digitMask);
                AppendDigitRect(digitRects, rightRect, digitMask);
                continue;
            }
        }

        AppendDigitRect(digitRects, digitRect, digitMask);
    }

    std::sort(digitRects.begin(), digitRects.end(), [](const cv::Rect& a, const cv::Rect& b) {
        return a.x < b.x;
        });
    return digitRects;
}

struct HayDayDigitTemplateData {
    cv::Mat mask;
    std::string sourceName;
    double sourceScore = 0.0;
};

static std::once_flag g_hayDayDigitTemplateInitFlag;
static std::array<HayDayDigitTemplateData, 10> g_hayDayDigitTemplates;
static bool g_hayDayDigitTemplatesLoaded = false;

static std::string GetExecutableDirectory() {
    char exePathBuf[MAX_PATH];
    GetModuleFileNameA(NULL, exePathBuf, MAX_PATH);
    std::string exePath(exePathBuf);
    size_t slash = exePath.find_last_of("\\/");
    if (slash == std::string::npos) return ".";
    return exePath.substr(0, slash);
}

static double CompareNormalizedDigitMasks(const cv::Mat& lhs, const cv::Mat& rhs) {
    if (lhs.empty() || rhs.empty() || lhs.size() != rhs.size()) return 0.0;

    cv::Mat lhsBinary;
    cv::threshold(lhs, lhsBinary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    cv::Mat rhsBinary;
    cv::threshold(rhs, rhsBinary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::Mat overlap;
    cv::bitwise_and(lhsBinary, rhsBinary, overlap);
    cv::Mat combined;
    cv::bitwise_or(lhsBinary, rhsBinary, combined);

    int overlapCount = cv::countNonZero(overlap);
    int combinedCount = cv::countNonZero(combined);
    if (combinedCount <= 0) return 0.0;

    cv::Mat diff;
    cv::bitwise_xor(lhsBinary, rhsBinary, diff);
    int mismatchCount = cv::countNonZero(diff);
    double jaccard = static_cast<double>(overlapCount) / static_cast<double>(combinedCount);
    double pixelAgreement = 1.0 - static_cast<double>(mismatchCount) / static_cast<double>(lhsBinary.total());
    return jaccard * 0.82 + pixelAgreement * 0.18;
}

static cv::Rect GetHayDayPriceTrainingRoi(const cv::Mat& screen) {
    if (screen.empty()) return cv::Rect();
    cv::Rect roi(410, 198, 92, 46);
    return ClampRectToImage(roi, screen.size());
}

static cv::Rect GetHudTrainingRoi(const cv::Mat& screen, bool diamonds) {
    if (screen.empty()) return cv::Rect();
    cv::Rect roi = diamonds ? cv::Rect(547, 22, 34, 21) : cv::Rect(507, 2, 72, 21);
    return ClampRectToImage(roi, screen.size());
}

static bool BuildHudNumberDigitMask(const cv::Mat& crop, cv::Mat& outMask, cv::Rect& outBounds) {
    outMask.release();
    outBounds = cv::Rect();
    if (crop.empty() || crop.channels() != 3) return false;

    cv::Mat upscaled;
    cv::resize(crop, upscaled, cv::Size(), 4.0, 4.0, cv::INTER_CUBIC);

    cv::Mat hsv;
    cv::cvtColor(upscaled, hsv, cv::COLOR_BGR2HSV);

    cv::Mat whiteMask;
    cv::inRange(hsv, cv::Scalar(0, 0, 150), cv::Scalar(180, 120, 255), whiteMask);

    cv::Mat gray;
    cv::cvtColor(upscaled, gray, cv::COLOR_BGR2GRAY);
    cv::Mat brightMask;
    cv::threshold(gray, brightMask, 145, 255, cv::THRESH_BINARY);
    cv::bitwise_and(whiteMask, brightMask, whiteMask);

    cv::morphologyEx(whiteMask, whiteMask, cv::MORPH_CLOSE, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 3)));
    cv::morphologyEx(whiteMask, whiteMask, cv::MORPH_OPEN, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2)));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(whiteMask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    int minArea = (std::max)(18, (whiteMask.cols * whiteMask.rows) / 900);
    int minHeight = (std::max)(10, whiteMask.rows / 3);
    for (size_t i = 0; i < contours.size(); ++i) {
        double area = cv::contourArea(contours[i]);
        cv::Rect box = cv::boundingRect(contours[i]);
        if (area < minArea || box.height < minHeight || box.width <= 1) {
            cv::drawContours(whiteMask, contours, static_cast<int>(i), cv::Scalar(0), cv::FILLED);
        }
    }

    std::vector<cv::Point> nonZero;
    cv::findNonZero(whiteMask, nonZero);
    if (nonZero.empty()) return false;

    outBounds = ClampRectToImage(cv::boundingRect(nonZero), whiteMask.size());
    if (outBounds.width <= 0 || outBounds.height <= 0) return false;
    outMask = whiteMask(outBounds).clone();
    return !outMask.empty();
}

static bool LoadHayDayDigitTemplatesFromScreenshots() {
    struct DigitScreenshotSource {
        const char* fileName;
        const char* digits;
        int digitIndex;
    };

    static const DigitScreenshotSource sources[] = {
        {"templates\\digit_1_sale.png", "1", 0},
        {"templates\\digit_2_sale.png", "2", 0},
        {"templates\\digit_3_sale.png", "3", 0},
        {"templates\\digit_4_sale.png", "4", 0},
        {"templates\\digit_5_sale.png", "5", 0},
        {"templates\\digit_6_sale.png", "6", 0},
        {"templates\\digit_7_sale.png", "7", 0},
        {"templates\\digit_8_sale.png", "8", 0},
        {"templates\\digit_9_sale.png", "9", 0},
        {"templates\\digit_0_sale.png", "10", 1},
    };

    for (auto& entry : g_hayDayDigitTemplates) {
        entry.mask.release();
        entry.sourceName.clear();
        entry.sourceScore = 0.0;
    }

    for (const auto& source : sources) {
        cv::Mat screen = cv::imread(ResolveTemplatePath(source.fileName));
        if (screen.empty()) continue;

        cv::Rect trainingRoi = GetHayDayPriceTrainingRoi(screen);
        if (trainingRoi.width <= 0 || trainingRoi.height <= 0) continue;
        cv::Mat crop = screen(trainingRoi).clone();

        cv::Mat digitMask;
        cv::Rect focusedBounds;
        if (!BuildWheatPreserveDigitMask(crop, digitMask, focusedBounds)) continue;

        std::vector<cv::Rect> digitRects = SegmentWheatDigitRects(digitMask);
        size_t expectedDigits = std::strlen(source.digits);
        if (digitRects.size() != expectedDigits) continue;
        if (source.digitIndex < 0 || source.digitIndex >= static_cast<int>(digitRects.size())) continue;

        int digit = source.digits[source.digitIndex] - '0';
        if (digit < 0 || digit > 9) continue;

        cv::Mat normalized = NormalizeBinaryDigitMask(digitMask(digitRects[source.digitIndex]));
        if (normalized.empty()) continue;

        double quality = static_cast<double>(cv::countNonZero(normalized));
        if (g_hayDayDigitTemplates[digit].mask.empty() || quality > g_hayDayDigitTemplates[digit].sourceScore) {
            g_hayDayDigitTemplates[digit].mask = normalized;
            g_hayDayDigitTemplates[digit].sourceName = source.fileName;
            g_hayDayDigitTemplates[digit].sourceScore = quality;
        }
    }

    for (int digit = 0; digit <= 9; ++digit) {
        if (g_hayDayDigitTemplates[digit].mask.empty()) {
            return false;
        }
    }
    return true;
}

static void EnsureHayDayDigitTemplatesLoaded() {
    std::call_once(g_hayDayDigitTemplateInitFlag, []() {
        g_hayDayDigitTemplatesLoaded = LoadHayDayDigitTemplatesFromScreenshots();
        });
}

static HayDayDigitTemplateMatch MatchHayDayDigitTemplate(const cv::Mat& digitMask) {
    HayDayDigitTemplateMatch match;
    EnsureHayDayDigitTemplatesLoaded();
    if (!g_hayDayDigitTemplatesLoaded) return match;

    cv::Mat normalized = NormalizeBinaryDigitMask(digitMask);
    if (normalized.empty()) return match;

    double bestSimilarity = 0.0;
    double secondSimilarity = 0.0;
    int bestDigit = -1;
    for (int digit = 0; digit <= 9; ++digit) {
        if (g_hayDayDigitTemplates[digit].mask.empty()) continue;
        double similarity = CompareNormalizedDigitMasks(normalized, g_hayDayDigitTemplates[digit].mask);
        if (similarity > bestSimilarity) {
            secondSimilarity = bestSimilarity;
            bestSimilarity = similarity;
            bestDigit = digit;
        }
        else if (similarity > secondSimilarity) {
            secondSimilarity = similarity;
        }
    }

    if (bestDigit == -1) return match;

    double separation = bestSimilarity - secondSimilarity;
    int score = static_cast<int>(bestSimilarity * 100.0 + separation * 120.0);
    match.digit = bestDigit;
    match.similarity = bestSimilarity;
    match.score = (std::max)(0, (std::min)(99, score));
    return match;
}

struct HudDigitTemplateData {
    cv::Mat mask;
    std::string sourceName;
    double sourceScore = 0.0;
};

static std::once_flag g_hudDigitTemplateInitFlag;
static std::array<HudDigitTemplateData, 10> g_hudDigitTemplates;
static bool g_hudDigitTemplatesLoaded = false;

static bool LoadHudDigitTemplatesFromScreenshots() {
    struct HudScreenshotSource {
        const char* fileName;
        bool diamonds;
        const char* digits;
        int digitIndex;
    };

    static const HudScreenshotSource sources[] = {
        {"templates\\digit_0_hud.png", false, "5690", 3},
        {"templates\\digit_1_hud.png", true, "12", 0},
        {"templates\\digit_2_hud.png", true, "12", 1},
        {"templates\\digit_3_hud.png", false, "5683", 3},
        {"templates\\digit_4_hud.png", true, "24", 1},
        {"templates\\digit_5_hud.png", false, "5683", 0},
        {"templates\\digit_6_hud.png", false, "5683", 1},
        {"templates\\digit_7_hud.png", false, "5687", 3},
        {"templates\\digit_8_hud.png", false, "5683", 2},
        {"templates\\digit_9_hud.png", false, "5690", 2},
    };

    for (auto& entry : g_hudDigitTemplates) {
        entry.mask.release();
        entry.sourceName.clear();
        entry.sourceScore = 0.0;
    }

    auto trainFromCrop = [&](const cv::Mat& crop, const char* expectedDigits, const std::string& sourceName, int digitIndex) {
        cv::Mat digitMask;
        cv::Rect focusedBounds;
        if (!BuildHudNumberDigitMask(crop, digitMask, focusedBounds)) return;

        std::vector<cv::Rect> digitRects = SegmentWheatDigitRects(digitMask);
        size_t expectedDigitCount = std::strlen(expectedDigits);
        if (digitRects.size() != expectedDigitCount) return;
        if (digitIndex < 0 || digitIndex >= static_cast<int>(digitRects.size())) return;

        int digit = expectedDigits[digitIndex] - '0';
        if (digit < 0 || digit > 9) return;

        cv::Mat normalized = NormalizeBinaryDigitMask(digitMask(digitRects[digitIndex]), cv::Size(24, 36));
        if (normalized.empty()) return;

        double quality = static_cast<double>(cv::countNonZero(normalized));
        if (g_hudDigitTemplates[digit].mask.empty() || quality > g_hudDigitTemplates[digit].sourceScore) {
            g_hudDigitTemplates[digit].mask = normalized;
            g_hudDigitTemplates[digit].sourceName = sourceName;
            g_hudDigitTemplates[digit].sourceScore = quality;
        }
    };

    for (const auto& source : sources) {
        cv::Mat screen = cv::imread(ResolveTemplatePath(source.fileName));
        if (screen.empty()) continue;

        cv::Rect roi = GetHudTrainingRoi(screen, source.diamonds);
        if (roi.width > 0 && roi.height > 0) {
            trainFromCrop(
                screen(roi).clone(),
                source.digits,
                std::string(source.fileName) + (source.diamonds ? ":dia" : ":coin"),
                source.digitIndex);
        }
    }

    for (int digit = 0; digit <= 9; ++digit) {
        if (g_hudDigitTemplates[digit].mask.empty()) return false;
    }
    return true;
}

static void EnsureHudDigitTemplatesLoaded() {
    std::call_once(g_hudDigitTemplateInitFlag, []() {
        g_hudDigitTemplatesLoaded = LoadHudDigitTemplatesFromScreenshots();
        });
}

static HayDayDigitTemplateMatch MatchHudDigitTemplate(const cv::Mat& digitMask) {
    HayDayDigitTemplateMatch match;
    EnsureHudDigitTemplatesLoaded();
    if (!g_hudDigitTemplatesLoaded) return match;

    cv::Mat normalized = NormalizeBinaryDigitMask(digitMask, cv::Size(24, 36));
    if (normalized.empty()) return match;

    double bestSimilarity = 0.0;
    double secondSimilarity = 0.0;
    int bestDigit = -1;
    for (int digit = 0; digit <= 9; ++digit) {
        if (g_hudDigitTemplates[digit].mask.empty()) continue;
        double similarity = CompareNormalizedDigitMasks(normalized, g_hudDigitTemplates[digit].mask);
        if (similarity > bestSimilarity) {
            secondSimilarity = bestSimilarity;
            bestSimilarity = similarity;
            bestDigit = digit;
        }
        else if (similarity > secondSimilarity) {
            secondSimilarity = similarity;
        }
    }

    if (bestDigit == -1) return match;

    double separation = bestSimilarity - secondSimilarity;
    int score = static_cast<int>(bestSimilarity * 100.0 + separation * 120.0);
    match.digit = bestDigit;
    match.similarity = bestSimilarity;
    match.score = (std::max)(0, (std::min)(99, score));
    return match;
}

static bool TryReadHudNumberByTemplates(const cv::Mat& crop, int& outCount, int& outScore) {
    outCount = 0;
    outScore = 0;
    if (crop.empty()) return false;

    cv::Mat digitMask;
    cv::Rect focusedBounds;
    if (!BuildHudNumberDigitMask(crop, digitMask, focusedBounds)) return false;

    std::vector<cv::Rect> digitRects = SegmentWheatDigitRects(digitMask);
    if (digitRects.empty() || digitRects.size() > 6) return false;

    std::string digits;
    int totalScore = 0;
    for (const auto& rect : digitRects) {
        HayDayDigitTemplateMatch match = MatchHudDigitTemplate(digitMask(rect));
        if (match.digit < 0 || match.score < 52) return false;
        digits.push_back(static_cast<char>('0' + match.digit));
        totalScore += match.score;
    }

    if (digits.empty()) return false;

    try {
        outCount = std::stoi(digits);
        outScore = totalScore / static_cast<int>(digitRects.size());
        return true;
    }
    catch (...) {
        outCount = 0;
        outScore = 0;
        return false;
    }
}

static OCRDigitsResult OCRWheatDigitsBySegmentation(const cv::Mat& crop, cv::Mat& outDebugMask, bool& outFocusedCrop) {
    OCRDigitsResult result;
    outDebugMask.release();
    outFocusedCrop = false;
    if (crop.empty() || g_tessApi == nullptr) return result;

    cv::Mat digitMask;
    cv::Rect focusedBounds;
    if (!BuildWheatPreserveDigitMask(crop, digitMask, focusedBounds)) return result;

    outFocusedCrop = focusedBounds.width < crop.cols * 5 || focusedBounds.height < crop.rows * 5;
    std::vector<cv::Rect> digitRects = SegmentWheatDigitRects(digitMask);

    cv::cvtColor(digitMask, outDebugMask, cv::COLOR_GRAY2BGR);
    for (const auto& rect : digitRects) {
        cv::rectangle(outDebugMask, rect, cv::Scalar(0, 255, 0), 1);
    }

    if (digitRects.empty() || digitRects.size() > 3) return result;
    if (digitRects.size() == 1 && digitRects[0].width > static_cast<int>(digitRects[0].height * 0.72f)) {
        return result;
    }

    std::string digits;
    int totalScore = 0;
    std::string combinedMethod;
    for (const auto& rect : digitRects) {
        OCRDigitsResult digit = OCRSingleDigitBestEffort(digitMask(rect));
        if (digit.digits.empty()) {
            result.score = 0;
            result.digits = digits;
            result.method = "unreadable";
            return result;
        }
        digits += digit.digits;
        totalScore += digit.score;
        if (combinedMethod.empty()) combinedMethod = digit.method;
        else if (combinedMethod != digit.method) combinedMethod = "mixed";
    }

    if (digits.empty() || digits.size() > 3) return result;

    result.digits = digits;
    result.score = totalScore / static_cast<int>(digitRects.size());
    result.method = combinedMethod.empty() ? "segmented" : std::string("segmented/") + combinedMethod;
    try {
        result.count = std::stoi(digits);
    }
    catch (...) {
        result.count = 0;
        result.score = 0;
        result.method = "unreadable";
    }
    return result;
}

static bool TryBuildItemCountRect(const cv::Mat& screen, const std::string& templatePath, cv::Rect& outRect, bool& outAnchorFound, bool forceCalibratedRoi) {
    if (screen.empty()) return false;
    cv::Rect numRect;
    bool specialWheatShop = (templatePath.find("wheat_shop") != std::string::npos);
    int preserveScanMode = (specialWheatShop && forceCalibratedRoi) ? 2 : g_WheatPreserveScanMode;
    outAnchorFound = false;

    if (specialWheatShop && preserveScanMode == 1) {
        outAnchorFound = true;
        int x = g_WheatPreserveCoordX - 20;
        int y = g_WheatPreserveCoordY - 14;
        int w = 74;
        int h = 34;
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x + w > screen.cols) w = screen.cols - x;
        if (y + h > screen.rows) h = screen.rows - y;
        if (w <= 0 || h <= 0) return false;
        numRect = cv::Rect(x, y, w, h);
    } else if (specialWheatShop && preserveScanMode == 2) {
        outAnchorFound = true;
        int left = (std::min)(g_WheatPreserveRoiLeft, g_WheatPreserveRoiRight);
        int top = (std::min)(g_WheatPreserveRoiTop, g_WheatPreserveRoiBottom);
        int right = (std::max)(g_WheatPreserveRoiLeft, g_WheatPreserveRoiRight);
        int bottom = (std::max)(g_WheatPreserveRoiTop, g_WheatPreserveRoiBottom);
        int x = left;
        int y = top;
        int w = right - left;
        int h = bottom - top;
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x + w > screen.cols) w = screen.cols - x;
        if (y + h > screen.rows) h = screen.rows - y;
        if (w <= 0 || h <= 0) return false;
        numRect = cv::Rect(x, y, w, h);
    } else {
        cv::Mat templ = cv::imread(ResolveTemplatePath(templatePath));
        if (templ.empty()) return false;

        cv::Mat matchResult;
        cv::matchTemplate(screen, templ, matchResult, cv::TM_CCOEFF_NORMED);
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);

        float needed = specialWheatShop ? g_OcrAnchorThreshold : 0.70f;
        if (maxVal < needed) return false;
        outAnchorFound = true;

        if (specialWheatShop) {
            int x = maxLoc.x - 6;
            int y = maxLoc.y - 8;
            int w = (std::max)(templ.cols + 36, 64);
            int h = (std::max)(templ.rows + 14, 28);
            if (x < 0) x = 0; if (y < 0) y = 0;
            if (x + w > screen.cols) w = screen.cols - x;
            if (y + h > screen.rows) h = screen.rows - y;
            if (w <= 0 || h <= 0) return false;
            numRect = cv::Rect(x, y, w, h);
        } else {
            int offsetX = 24, offsetY = 9, roiW = 26, roiH = 23;
            int numX = maxLoc.x + offsetX;
            int numY = maxLoc.y + offsetY;
            if (numX < 0) numX = 0; if (numY < 0) numY = 0;
            int curW = roiW; int curH = roiH;
            if (numX + curW > screen.cols) curW = screen.cols - numX;
            if (numY + curH > screen.rows) curH = screen.rows - numY;
            if (curW <= 0 || curH <= 0) return false;
            numRect = cv::Rect(numX, numY, curW, curH);
        }
    }
    outRect = numRect;
    return true;
}

ItemCountDebugInfo ReadItemCountByAnchorDebug(cv::Mat screen, const std::string& templatePath, bool forceCalibratedRoi) {
    ItemCountDebugInfo info;
    if (screen.empty()) return info;
    if (g_tessApi == nullptr) InitGlobalTesseract();

    bool anchorFound = false;
    cv::Rect numRect;
    if (!TryBuildItemCountRect(screen, templatePath, numRect, anchorFound, forceCalibratedRoi)) {
        info.anchorFound = anchorFound;
        return info;
    }

    info.anchorFound = anchorFound;
    info.roiValid = true;
    info.roi = numRect;
    info.rawCrop = screen(numRect).clone();
    if (info.rawCrop.empty()) {
        info.roiValid = false;
        return info;
    }

    bool specialWheatShop = (templatePath.find("wheat_shop") != std::string::npos);
    cv::Mat segmentedDebugCrop;
    bool usedSegmentedFocus = false;
    OCRDigitsResult segmented = OCRWheatDigitsBySegmentation(info.rawCrop, segmentedDebugCrop, usedSegmentedFocus);
    if (specialWheatShop) {
        info.finalCrop = segmentedDebugCrop.empty() ? info.rawCrop.clone() : segmentedDebugCrop;
        info.usedFocusedCrop = usedSegmentedFocus;
        OCRDigitsResult ocr = segmented;
        info.count = ocr.count;
        info.ocrScore = ocr.score;
        info.digits = ocr.digits;
        info.readMethod = ocr.method.empty() ? "segmented" : ocr.method;
        if (info.ocrScore < 35) {
            info.count = 0;
        }
        return info;
    }

    if (!segmented.digits.empty() && segmented.score >= 35) {
        info.finalCrop = segmentedDebugCrop.empty() ? info.rawCrop.clone() : segmentedDebugCrop;
        info.usedFocusedCrop = usedSegmentedFocus;
        info.count = segmented.count;
        info.ocrScore = segmented.score;
        info.digits = segmented.digits;
        info.readMethod = segmented.method.empty() ? "segmented" : segmented.method;
        return info;
    }

    info.finalCrop = info.rawCrop.clone();
    info.usedFocusedCrop = false;
    OCRDigitsResult ocr = OCRDigitsBestEffortDetailed(info.finalCrop);
    info.count = ocr.count;
    info.ocrScore = ocr.score;
    info.digits = ocr.digits;
    info.readMethod = ocr.method.empty() ? "tesseract" : ocr.method;
    return info;
}

int ReadItemCountByAnchor(cv::Mat screen, const std::string& templatePath, bool forceCalibratedRoi) {
    return ReadItemCountByAnchorDebug(screen, templatePath, forceCalibratedRoi).count;
}
