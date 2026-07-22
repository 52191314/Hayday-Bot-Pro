#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#include "State.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

extern TemplateThresholds g_Thresholds;

extern std::string shop_templatePath;
extern std::string roadside_shop_templatePath;
extern std::string wheatshop_templatePath;
extern std::string soldcrate_templatePath;
extern std::string crate_templatePath;
extern std::string plus_templatePath;
extern std::string cross_templatePath;
extern std::string advertise_templatePath;
extern std::string occupied_crate_advertise_templatePath;
extern std::string create_sale_templatePath;
extern std::string create_ad_templatePath;
extern std::string occupied_crate_create_ad_templatePath;
extern std::string c_templatePath;
extern std::string cornshop_templatePath;
extern std::string barn_market_templatePath;
extern std::string crate_wheat_templatePath;
extern std::string crate_corn_templatePath;
extern std::string carrot_templatePath;
extern std::string carrot_shop_templatePath;
extern std::string crate_carrot_templatePath;
extern std::string crate_soybean_templatePath;
extern std::string crate_sugarcane_templatePath;
extern std::string soybean_templatePath;
extern std::string soybean_shop_templatePath;
extern std::string sugarcane_templatePath;
extern std::string sugarcane_shop_templatePath;
extern std::string silo_full_templatePath;

namespace {

struct TemplateProbeSpec {
    std::string path;
    float threshold = 0.0f;
    bool grayscale = false;
    bool useMargins = false;
};

struct UiSignalProbe {
    bool found = false;
    int x = -1;
    int y = -1;
    double score = 0.0;
    float threshold = 0.0f;
    bool grayscale = false;
};

struct SeedTemplateInfo {
    std::string path;
    float threshold = 0.0f;
};

void KeepBest(MatchResult& best, const MatchResult& candidate) {
    if (candidate.found && (!best.found || candidate.score > best.score)) best = candidate;
}

bool IsTopLeftHudPoint(const cv::Mat& screen, int x, int y) {
    if (screen.empty()) return false;
    return x >= 0 && y >= 0 &&
        x < static_cast<int>(screen.cols * 0.20f) &&
        y < static_cast<int>(screen.rows * 0.22f);
}

bool IsRoadsideShopBannerPointPlausible(const cv::Mat& screen, int x, int y) {
    if (screen.empty()) return false;
    int minY = static_cast<int>(screen.rows * 0.08f);
    int maxY = static_cast<int>(screen.rows * 0.58f);
    int minX = static_cast<int>(screen.cols * 0.08f);
    int maxX = static_cast<int>(screen.cols * 0.92f);
    return x >= minX && x <= maxX && y >= minY && y <= maxY;
}

void KeepBestRoadsideBanner(const cv::Mat& screen, MatchResult& best, const MatchResult& candidate) {
    if (!candidate.found) return;
    if (!IsRoadsideShopBannerPointPlausible(screen, candidate.x, candidate.y)) return;
    KeepBest(best, candidate);
}

SeedTemplateInfo GetSeedTemplateForCrop(int cropMode) {
    switch (cropMode) {
    case 1: return { c_templatePath, g_Thresholds.cornThreshold };
    case 2: return { carrot_templatePath, g_Thresholds.carrotThreshold };
    case 3: return { soybean_templatePath, g_Thresholds.soybeanThreshold };
    case 4: return { sugarcane_templatePath, g_Thresholds.sugarcaneThreshold };
    default: return { "", 0.0f };
    }
}

MatchResult FindRoadsideShopBannerMatch(const cv::Mat& screen) {
    MatchResult best{ false, 0, 0, 0.0 };
    float strictThreshold = (std::max)(0.62f, g_Thresholds.shopThreshold - 0.10f);
    float relaxedThreshold = (std::max)(0.56f, g_Thresholds.shopThreshold - 0.18f);
    KeepBestRoadsideBanner(screen, best, FindImage(screen, roadside_shop_templatePath, strictThreshold, false, 1.0f, false, false));
    KeepBestRoadsideBanner(screen, best, FindImage(screen, roadside_shop_templatePath, strictThreshold, true, 1.0f, false, false));
    KeepBestRoadsideBanner(screen, best, FindImage(screen, roadside_shop_templatePath, relaxedThreshold, false, 1.0f, false, false));
    KeepBestRoadsideBanner(screen, best, FindImage(screen, roadside_shop_templatePath, relaxedThreshold, true, 1.0f, false, false));
    return best;
}

UiSignalProbe ProbeTemplate(const cv::Mat& screen, const TemplateProbeSpec& spec) {
    UiSignalProbe probe;
    probe.threshold = spec.threshold;
    probe.grayscale = spec.grayscale;
    if (screen.empty()) return probe;

    int topMargin = spec.useMargins ? 60 : 0;
    int sideMargin = spec.useMargins ? 50 : 0;
    if (screen.cols < 200 || screen.rows < 200) {
        topMargin = 0;
        sideMargin = 0;
    }

    int roiWidth = screen.cols - (2 * sideMargin);
    int roiHeight = screen.rows - topMargin;
    if (roiWidth <= 0 || roiHeight <= 0) return probe;

    cv::Rect roiRect(sideMargin, topMargin, roiWidth, roiHeight);
    cv::Mat searchArea = screen(roiRect);
    cv::Mat templ = cv::imread(ResolveTemplatePath(spec.path), spec.grayscale ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR);
    if (templ.empty() || templ.cols > searchArea.cols || templ.rows > searchArea.rows) return probe;

    cv::Mat processedScreen;
    if (spec.grayscale) {
        if (searchArea.channels() == 3) cv::cvtColor(searchArea, processedScreen, cv::COLOR_BGR2GRAY);
        else if (searchArea.channels() == 4) cv::cvtColor(searchArea, processedScreen, cv::COLOR_BGRA2GRAY);
        else processedScreen = searchArea;
    }
    else {
        if (searchArea.channels() == 4) cv::cvtColor(searchArea, processedScreen, cv::COLOR_BGRA2BGR);
        else if (searchArea.channels() == 1) cv::cvtColor(searchArea, processedScreen, cv::COLOR_GRAY2BGR);
        else processedScreen = searchArea;
    }

    cv::Mat matchResult;
    try {
        cv::matchTemplate(processedScreen, templ, matchResult, cv::TM_CCOEFF_NORMED);
    }
    catch (...) {
        return probe;
    }

    double minVal = 0.0;
    double maxVal = 0.0;
    cv::Point minLoc;
    cv::Point maxLoc;
    cv::minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);
    probe.score = maxVal;
    probe.found = maxVal >= spec.threshold;
    probe.x = maxLoc.x + sideMargin + (templ.cols / 2);
    probe.y = maxLoc.y + topMargin + (templ.rows / 2);
    return probe;
}

UiSignalProbe ProbeBestTemplate(const cv::Mat& screen, const std::vector<TemplateProbeSpec>& specs) {
    UiSignalProbe best;
    bool initialized = false;
    for (const auto& spec : specs) {
        UiSignalProbe probe = ProbeTemplate(screen, spec);
        if (!initialized ||
            (probe.found && !best.found) ||
            (probe.found == best.found && probe.score > best.score)) {
            best = probe;
            initialized = true;
        }
    }
    return best;
}

std::string FormatSignalProbe(const char* name, const UiSignalProbe& probe) {
    std::ostringstream oss;
    oss << name << "=" << (probe.found ? "1" : "0")
        << "(" << FormatMarketScore(probe.score)
        << (probe.found ? ">=" : "<")
        << FormatMarketScore(probe.threshold)
        << " gray=" << (probe.grayscale ? "1" : "0");
    if (probe.x >= 0 && probe.y >= 0) oss << " x=" << probe.x << " y=" << probe.y;
    oss << ")";
    return oss.str();
}

MatchResult FindEmptyShopCrateMatch(const cv::Mat& screen, AccountSlot* account) {
    MatchResult best{ false, 0, 0, 0.0 };
    if (account) {
        best = FindEmptyCratePreferCache(screen, *account);
        if (best.found) return best;
    }

    float strictThreshold = (std::max)(0.68f, g_Thresholds.crateThreshold - 0.10f);
    float relaxedThreshold = (std::max)(0.58f, g_Thresholds.crateThreshold - 0.20f);
    KeepBest(best, FindImage(screen, crate_templatePath, strictThreshold, false, 1.0f, false, false));
    KeepBest(best, FindImage(screen, crate_templatePath, strictThreshold, true, 1.0f, false, false));
    KeepBest(best, FindImage(screen, crate_templatePath, relaxedThreshold, false, 1.0f, false, false));
    KeepBest(best, FindImage(screen, crate_templatePath, relaxedThreshold, true, 1.0f, false, false));
    return best;
}

bool HasSoldCrateMatch(const cv::Mat& screen) {
    if (!FindAllImages(screen, soldcrate_templatePath, 0.80f, 10, false, false).empty()) return true;
    if (!FindAllImages(screen, soldcrate_templatePath, 0.72f, 10, false, false).empty()) return true;
    return !FindAllImages(screen, soldcrate_templatePath, 0.64f, 10, false, false).empty();
}

} // namespace

std::string FormatMarketScore(double score) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << score;
    return oss.str();
}

std::string GetFilledCrateTemplateForCrop(int cropMode) {
    if (cropMode == 1) return crate_corn_templatePath;
    if (cropMode == 2) return crate_carrot_templatePath;
    if (cropMode == 3) return crate_soybean_templatePath;
    if (cropMode == 4) return crate_sugarcane_templatePath;
    return crate_wheat_templatePath;
}

MatchResult FindAnyFilledCrate(const cv::Mat& screen, int cropMode, float threshold) {
    MatchResult best{ false, 0, 0, 0.0 };
    std::vector<std::string> candidates;
    auto pushUnique = [&](const std::string& path) {
        if (std::find(candidates.begin(), candidates.end(), path) == candidates.end()) candidates.push_back(path);
    };

    pushUnique(GetFilledCrateTemplateForCrop(cropMode));
    pushUnique(crate_wheat_templatePath);
    pushUnique(crate_corn_templatePath);
    pushUnique(crate_carrot_templatePath);
    pushUnique(crate_soybean_templatePath);
    pushUnique(crate_sugarcane_templatePath);

    for (const auto& path : candidates) {
        KeepBest(best, FindImage(screen, path, threshold, false, 1.0f, false, false));
    }
    return best;
}

MarketSignalSnapshot ProbeMarketSignals(const cv::Mat& screen, int cropMode, AccountSlot* account) {
    MarketSignalSnapshot snapshot;
    if (screen.empty()) return snapshot;

    snapshot.emptyCrate = FindEmptyShopCrateMatch(screen, account).found;
    snapshot.soldCrate = HasSoldCrateMatch(screen);
    snapshot.filledCrate = FindAnyFilledCrate(screen, cropMode, 0.60f).found;
    snapshot.product = IsBarnSalePanelVisible(screen);
    snapshot.shop = IsRoadsideShopBannerVisible(screen) || snapshot.emptyCrate || snapshot.soldCrate || snapshot.filledCrate;

    if (account) {
        snapshot.shopIcon = FindShopIconPreferCache(screen, *account).found;
    }
    else {
        snapshot.shopIcon = FindImage(screen, shop_templatePath, g_Thresholds.shopThreshold, false, 1.0f, false, false).found ||
            FindImage(screen, shop_templatePath, g_Thresholds.shopThreshold, true, 1.0f, false, false).found;
    }

    snapshot.ad = FindImage(screen, advertise_templatePath, std::max(0.60f, g_Thresholds.advertiseThreshold - 0.12f), false, 1.0f, false, false).found ||
        FindImage(screen, occupied_crate_advertise_templatePath, std::max(0.60f, g_Thresholds.advertiseThreshold - 0.12f), false, 1.0f, false, false).found ||
        FindImage(screen, create_ad_templatePath, std::max(0.58f, g_Thresholds.createAdThreshold - 0.10f), false, 1.0f, false, false).found ||
        FindImage(screen, occupied_crate_create_ad_templatePath, std::max(0.58f, g_Thresholds.createAdThreshold - 0.10f), false, 1.0f, false, false).found;

    snapshot.farm = snapshot.shopIcon && !snapshot.shop && !snapshot.product && !snapshot.ad;
    return snapshot;
}

std::string DescribeMarketSignalSnapshot(const MarketSignalSnapshot& snapshot) {
    auto yn = [](bool value) { return value ? "1" : "0"; };
    return std::string("farm=") + yn(snapshot.farm) +
        " shop=" + yn(snapshot.shop) +
        " product=" + yn(snapshot.product) +
        " ad=" + yn(snapshot.ad) +
        " empty=" + yn(snapshot.emptyCrate) +
        " sold=" + yn(snapshot.soldCrate) +
        " filled=" + yn(snapshot.filledCrate) +
        " shop_icon=" + yn(snapshot.shopIcon);
}

std::string DescribeMarketSignalSnapshot(const cv::Mat& screen, int cropMode, AccountSlot* account) {
    return DescribeMarketSignalSnapshot(ProbeMarketSignals(screen, cropMode, account));
}

std::string DescribeShopSignals(const cv::Mat& screen, int cropMode, AccountSlot* account) {
    float bannerStrictThreshold = (std::max)(0.62f, g_Thresholds.shopThreshold - 0.10f);
    float bannerRelaxedThreshold = (std::max)(0.56f, g_Thresholds.shopThreshold - 0.18f);
    float emptyStrictThreshold = (std::max)(0.68f, g_Thresholds.crateThreshold - 0.10f);
    float emptyRelaxedThreshold = (std::max)(0.58f, g_Thresholds.crateThreshold - 0.20f);

    UiSignalProbe banner = ProbeBestTemplate(screen, {
        { roadside_shop_templatePath, bannerStrictThreshold, false, false },
        { roadside_shop_templatePath, bannerStrictThreshold, true, false },
        { roadside_shop_templatePath, bannerRelaxedThreshold, false, false },
        { roadside_shop_templatePath, bannerRelaxedThreshold, true, false }
    });
    if (banner.found && !IsRoadsideShopBannerPointPlausible(screen, banner.x, banner.y)) {
        banner.found = false;
        banner.threshold = static_cast<float>(banner.score + 0.01);
    }

    UiSignalProbe empty = ProbeBestTemplate(screen, {
        { crate_templatePath, emptyStrictThreshold, false, false },
        { crate_templatePath, emptyStrictThreshold, true, false },
        { crate_templatePath, emptyRelaxedThreshold, false, false },
        { crate_templatePath, emptyRelaxedThreshold, true, false }
    });
    if (account) {
        MatchResult cachedEmpty = FindEmptyCratePreferCache(screen, *account);
        if (cachedEmpty.found && (!empty.found || cachedEmpty.score > empty.score)) {
            empty.found = true;
            empty.x = cachedEmpty.x;
            empty.y = cachedEmpty.y;
            empty.score = cachedEmpty.score;
            empty.threshold = g_Thresholds.crateThreshold;
        }
    }

    UiSignalProbe sold = ProbeBestTemplate(screen, {
        { soldcrate_templatePath, 0.80f, false, false },
        { soldcrate_templatePath, 0.72f, false, false },
        { soldcrate_templatePath, 0.64f, false, false }
    });

    std::vector<TemplateProbeSpec> filledSpecs;
    auto addFilledProbe = [&](const std::string& path) {
        filledSpecs.push_back({ path, 0.60f, false, false });
    };
    addFilledProbe(GetFilledCrateTemplateForCrop(cropMode));
    addFilledProbe(crate_wheat_templatePath);
    addFilledProbe(crate_corn_templatePath);
    addFilledProbe(crate_carrot_templatePath);
    addFilledProbe(crate_soybean_templatePath);
    addFilledProbe(crate_sugarcane_templatePath);
    UiSignalProbe filled = ProbeBestTemplate(screen, filledSpecs);

    UiSignalProbe barn = ProbeBestTemplate(screen, {
        { barn_market_templatePath, std::max(0.65f, g_Thresholds.barnTabThreshold), false, false }
    });

    return FormatSignalProbe("banner", banner) + " " +
        FormatSignalProbe("empty", empty) + " " +
        FormatSignalProbe("sold", sold) + " " +
        FormatSignalProbe("filled", filled) + " " +
        FormatSignalProbe("barn", barn);
}

bool IsRoadsideShopBannerVisible(const cv::Mat& screen) {
    return FindRoadsideShopBannerMatch(screen).found;
}

bool IsRoadsideShopCrateViewVisible(const cv::Mat& screen, int cropMode) {
    if (screen.empty()) return false;
    if (IsRoadsideShopBannerVisible(screen)) return true;
    if (FindEmptyShopCrateMatch(screen, nullptr).found) return true;
    if (HasSoldCrateMatch(screen)) return true;
    return FindAnyFilledCrate(screen, cropMode, 0.60f).found;
}

bool IsBarnSalePanelVisible(const cv::Mat& screen) {
    return FindImage(screen, barn_market_templatePath, std::max(0.65f, g_Thresholds.barnTabThreshold), false, 1.0f, false, false).found;
}

cv::Rect BuildSaleDialogSearchRect(const cv::Mat& screen) {
    if (screen.empty()) return cv::Rect();
    int left = static_cast<int>(screen.cols * 0.47);
    int top = static_cast<int>(screen.rows * 0.18);
    int right = screen.cols - 8;
    int bottom = screen.rows - 18;
    cv::Rect rect(left, top, (std::max)(1, right - left), (std::max)(1, bottom - top));
    return rect & cv::Rect(0, 0, screen.cols, screen.rows);
}

MatchResult FindTemplateInRect(const cv::Mat& screen, const cv::Rect& rect, const std::string& templatePath, float threshold, bool grayscale, bool allowFallback) {
    MatchResult result{ false, 0, 0, 0.0 };
    if (screen.empty() || rect.width <= 0 || rect.height <= 0) return result;
    cv::Rect clipped = rect & cv::Rect(0, 0, screen.cols, screen.rows);
    if (clipped.width <= 0 || clipped.height <= 0) return result;

    MatchResult local = FindImage(screen(clipped), templatePath, threshold, grayscale, 1.0f, false, allowFallback);
    if (!local.found) return result;
    local.x += clipped.x;
    local.y += clipped.y;
    return local;
}

static cv::Rect BuildProbeRectAroundPoint(const cv::Mat& screen, int centerX, int centerY, int radiusX, int radiusY) {
    if (screen.empty()) return cv::Rect();
    cv::Rect rect(centerX - radiusX, centerY - radiusY, radiusX * 2, radiusY * 2);
    return rect & cv::Rect(0, 0, screen.cols, screen.rows);
}

static MatchResult FindTemplateNearCachedPoint(const cv::Mat& screen,
    int cachedX,
    int cachedY,
    int radiusX,
    int radiusY,
    const std::string& templatePath,
    float threshold,
    bool grayscale = false) {
    if (screen.empty() || cachedX < 0 || cachedY < 0) return MatchResult{ false, 0, 0, 0.0 };
    cv::Rect probeRect = BuildProbeRectAroundPoint(screen, cachedX, cachedY, radiusX, radiusY);
    if (probeRect.width <= 0 || probeRect.height <= 0) return MatchResult{ false, 0, 0, 0.0 };
    MatchResult cachedRes = FindTemplateInRect(screen, probeRect, templatePath, threshold, grayscale, false);
    if (!cachedRes.found) {
        cachedRes = FindTemplateInRect(screen, probeRect, templatePath, (std::max)(0.60f, threshold - 0.08f), false, false);
    }
    return cachedRes;
}

MatchResult FindDialogButtonTemplate(const cv::Mat& screen, const std::string& templatePath, float threshold, bool allowFallback) {
    MatchResult best{ false, 0, 0, 0.0 };
    if (screen.empty()) return best;
    cv::Rect dialogRect = BuildSaleDialogSearchRect(screen);
    float relaxedThreshold = (std::max)(0.52f, threshold - 0.25f);
    KeepBest(best, FindTemplateInRect(screen, dialogRect, templatePath, threshold, false, allowFallback));
    KeepBest(best, FindTemplateInRect(screen, dialogRect, templatePath, threshold, true, allowFallback));
    KeepBest(best, FindTemplateInRect(screen, dialogRect, templatePath, relaxedThreshold, false, allowFallback));
    return best;
}

MatchResult FindBestDialogTemplate(const cv::Mat& screen, const std::initializer_list<std::string>& templatePaths, float threshold, bool allowFallback) {
    MatchResult best{ false, 0, 0, 0.0 };
    for (const auto& path : templatePaths) {
        KeepBest(best, FindDialogButtonTemplate(screen, path, threshold, allowFallback));
    }
    return best;
}

std::string GetSaleProductTemplateForCrop(int cropMode) {
    if (cropMode == 1) return cornshop_templatePath;
    if (cropMode == 2) return carrot_shop_templatePath;
    if (cropMode == 3) return soybean_shop_templatePath;
    if (cropMode == 4) return sugarcane_shop_templatePath;
    return wheatshop_templatePath;
}

float GetSaleProductThresholdForCrop(int cropMode) {
    if (cropMode == 1) return g_Thresholds.cornShopThreshold;
    if (cropMode == 2) return g_Thresholds.carrotShopThreshold;
    if (cropMode == 3) return g_Thresholds.soybeanShopThreshold;
    if (cropMode == 4) return g_Thresholds.sugarcaneShopThreshold;
    return g_Thresholds.wheatShopThreshold;
}

MatchResult FindShopIconPreferCache(const cv::Mat& screen, AccountSlot& account) {
    MatchResult shopRes{ false, 0, 0, 0.0 };
    auto rejectHudMatch = [&]() {
        if (shopRes.found && IsTopLeftHudPoint(screen, shopRes.x, shopRes.y)) {
            shopRes = MatchResult{ false, 0, 0, 0.0 };
            account.cachedShopX = -1;
            account.cachedShopY = -1;
        }
    };
    if (account.cachedShopX >= 0 && account.cachedShopY >= 0) {
        shopRes = FindTemplateNearCachedPoint(screen, account.cachedShopX, account.cachedShopY, 90, 90, shop_templatePath, g_Thresholds.shopThreshold);
        rejectHudMatch();
    }
    if (!shopRes.found) { shopRes = FindImage(screen, shop_templatePath, g_Thresholds.shopThreshold, false, 1.0f, false, false); rejectHudMatch(); }
    if (!shopRes.found) { shopRes = FindImage(screen, shop_templatePath, g_Thresholds.shopThreshold, true, 1.0f, false, false); rejectHudMatch(); }
    if (!shopRes.found) { shopRes = FindImage(screen, shop_templatePath, (std::max)(0.62f, g_Thresholds.shopThreshold - 0.08f), false, 1.0f, false, false); rejectHudMatch(); }
    if (shopRes.found && !shopRes.isFallback) {
        account.cachedShopX = shopRes.x;
        account.cachedShopY = shopRes.y;
    }
    return shopRes;
}

MatchResult FindEmptyCratePreferCache(const cv::Mat& screen, AccountSlot& account) {
    MatchResult emptyCrate{ false, 0, 0, 0.0 };
    if (account.cachedEmptyCrateX >= 0 && account.cachedEmptyCrateY >= 0) {
        emptyCrate = FindTemplateNearCachedPoint(screen, account.cachedEmptyCrateX, account.cachedEmptyCrateY, 150, 110, crate_templatePath, g_Thresholds.crateThreshold);
    }
    if (!emptyCrate.found) emptyCrate = FindImage(screen, crate_templatePath, g_Thresholds.crateThreshold, false, 1.0f, false, false);
    if (emptyCrate.found && !emptyCrate.isFallback) {
        account.cachedEmptyCrateX = emptyCrate.x;
        account.cachedEmptyCrateY = emptyCrate.y;
    }
    return emptyCrate;
}

MatchResult FindEmptyCrateWithFallback(const cv::Mat& screen, AccountSlot& account, bool* usedRelaxedThreshold) {
    if (usedRelaxedThreshold) *usedRelaxedThreshold = false;
    MatchResult emptyCrate = FindEmptyCratePreferCache(screen, account);
    if (emptyCrate.found) return emptyCrate;

    float relaxedThreshold = (std::max)(0.58f, g_Thresholds.crateThreshold - 0.20f);
    emptyCrate = FindImage(screen, crate_templatePath, relaxedThreshold, false, 1.0f, false, false);
    if (!emptyCrate.found) emptyCrate = FindImage(screen, crate_templatePath, relaxedThreshold, true, 1.0f, false, false);
    if (emptyCrate.found) {
        if (usedRelaxedThreshold) *usedRelaxedThreshold = true;
        account.cachedEmptyCrateX = emptyCrate.x;
        account.cachedEmptyCrateY = emptyCrate.y;
    }
    return emptyCrate;
}

MatchResult FindSaleProductPreferCache(const cv::Mat& screen, int cropMode, AccountSlot& account, std::string* matchSource) {
    if (matchSource) *matchSource = "";
    std::string productTemplate = GetSaleProductTemplateForCrop(cropMode);
    float productThreshold = GetSaleProductThresholdForCrop(cropMode);
    cv::Rect productPickerRect = BuildSaleDialogSearchRect(screen);

    MatchResult productRes{ false, 0, 0, 0.0 };
    if (account.cachedSaleProductX >= 0 && account.cachedSaleProductY >= 0) {
        productRes = FindTemplateNearCachedPoint(screen, account.cachedSaleProductX, account.cachedSaleProductY, 190, 130, productTemplate, productThreshold);
        if (productRes.found && !productPickerRect.contains(cv::Point(productRes.x, productRes.y))) {
            productRes = MatchResult{ false, 0, 0, 0.0 };
            account.cachedSaleProductX = -1;
            account.cachedSaleProductY = -1;
        }
        if (productRes.found && matchSource) *matchSource = "cached product";
    }
    if (!productRes.found) {
        productRes = FindTemplateInRect(screen, productPickerRect, productTemplate, productThreshold, false, false);
        if (productRes.found && matchSource) *matchSource = "product";
    }
    if (!productRes.found) {
        productRes = FindTemplateInRect(screen, productPickerRect, productTemplate, (std::max)(0.65f, productThreshold - 0.12f), false, false);
        if (productRes.found && matchSource) *matchSource = "relaxed product";
    }
    if (!productRes.found) {
        SeedTemplateInfo seed = GetSeedTemplateForCrop(cropMode);
        if (!seed.path.empty()) {
            productRes = FindTemplateInRect(screen, productPickerRect, seed.path, (std::max)(0.65f, seed.threshold - 0.10f), false, false);
            if (productRes.found && matchSource) *matchSource = "seed fallback";
        }
    }
    if (productRes.found && !productRes.isFallback) {
        account.cachedSaleProductX = productRes.x;
        account.cachedSaleProductY = productRes.y;
    }
    return productRes;
}

bool IsSaleProductPickerVisible(const cv::Mat& screen, int cropMode) {
    (void)cropMode;
    return IsBarnSalePanelVisible(screen);
}

MatchResult FindCreateSaleButtonRelaxed(const cv::Mat& screen) {
    MatchResult strictRes = FindImage(screen, create_sale_templatePath, g_Thresholds.createSaleThreshold, false, 1.0f, false, false);
    if (strictRes.found) return strictRes;
    return FindImage(screen, create_sale_templatePath, (std::max)(0.52f, g_Thresholds.createSaleThreshold - 0.28f), false, 1.0f, false, false);
}

bool IsSaleListingControlsVisible(const cv::Mat& screen) {
    return FindCreateSaleButtonRelaxed(screen).found ||
        FindBestDialogTemplate(screen, { advertise_templatePath, occupied_crate_advertise_templatePath }, g_Thresholds.advertiseThreshold, false).found ||
        FindBestDialogTemplate(screen, { create_ad_templatePath, occupied_crate_create_ad_templatePath }, g_Thresholds.createAdThreshold, false).found ||
        FindImage(screen, plus_templatePath, (std::max)(0.65f, g_Thresholds.plusThreshold - 0.15f), false, 1.0f, false, false).found;
}

bool IsSalePanelVisible(const cv::Mat& screen) {
    return IsSaleListingControlsVisible(screen);
}

bool IsSalePanelVisibleForCrop(const cv::Mat& screen, int cropMode) {
    if (IsSaleListingControlsVisible(screen)) return true;
    if (IsSaleProductPickerVisible(screen, cropMode)) return true;
    return false;
}

bool IsOccupiedCrateEditPanelVisible(const cv::Mat& screen) {
    if (screen.empty()) return false;
    if (FindDialogButtonTemplate(screen, occupied_crate_advertise_templatePath, g_Thresholds.advertiseThreshold, false).found) return true;
    if (FindDialogButtonTemplate(screen, occupied_crate_create_ad_templatePath, g_Thresholds.createAdThreshold, false).found) return true;
    cv::Rect dialogRect = BuildSaleDialogSearchRect(screen);
    MatchResult dialogCross = FindTemplateInRect(screen, dialogRect, cross_templatePath, std::max(0.65f, g_Thresholds.crossThreshold - 0.08f), false, false);
    return dialogCross.found;
}

bool IsMarketViewVisible(const cv::Mat& screen, int cropMode) {
    return IsRoadsideShopCrateViewVisible(screen, cropMode) ||
        IsSalePanelVisibleForCrop(screen, cropMode);
}

bool IsMarketViewLikelyVisible(const cv::Mat& screen, int cropMode) {
    if (screen.empty()) return false;
    MatchResult siloFullPopup = FindImage(screen, silo_full_templatePath, 0.75f, false, 1.0f, false, false);
    if (siloFullPopup.found) return false;
    if (IsMarketViewVisible(screen, cropMode)) return true;
    MatchResult relaxedFilledCrate = FindAnyFilledCrate(screen, cropMode, 0.65f);
    if (relaxedFilledCrate.found) return true;
    MatchResult relaxedCrate = FindImage(screen, crate_templatePath, std::max(0.70f, g_Thresholds.crateThreshold - 0.10f), false, 1.0f, false, false);
    return relaxedCrate.found;
}
