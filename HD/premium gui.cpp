#define _WIN32_WINNT 0x0600
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <array>
#include <windows.h>
#include <dbghelp.h>
#include <shellapi.h>
#include <commdlg.h>
#include <ctime>
#include <regex>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <intrin.h>
#include <filesystem>
#include <direct.h>
#include <mutex>
#include <intrin.h> 
// OpenCV Headers
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Dbghelp.lib")

// --- Function to help navigate appdata path ---
std::string GetAppDataPath() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::string nxrthPath = std::string(path) + "\\NXRTH_Premium";

		// If theres no NXRTH_Premium folder in AppData, create it along with the 6 instance backup folders
        if (!std::filesystem::exists(nxrthPath)) {
            std::filesystem::create_directories(nxrthPath);
            // Creates 6 instance folders
            for (int i = 0; i < 6; i++) {
                std::filesystem::create_directories(nxrthPath + "\\Backups\\Instance_" + std::to_string(i));
            }
        }
        return nxrthPath;
    }
	return "."; // Remain in .exe directory if AppData path retrieval fails
}
// --- IMGUI & GLAD & GLFW ---
#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include <glad/glad.h>
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "tesseract_ocr.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "BotEngine.h"
#include "Discord.h"
#include "XorStr.h"
#include "bot_logic.h"
#include "Language.h"
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#pragma comment(lib, "comdlg32.lib")

// Emulator Mode
int g_GlobalEmulatorMode = 0; // 0 = MEmu, 1 = LDPlayer


namespace fs = std::filesystem;
std::wstring ToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}


char g_Username[64] = "Open Source";
char g_DiscordID[64] = "";
int g_TargetTabToSelect = -1; // To change tabs

extern int g_TransferThreshold; // auto item transfer min amount count
extern std::string g_StorageTag; // storage tag for the auto item transfer
char g_StorageTagBuf[64] = ""; // empty buffer for imgui


// --- GLOBAL TEXTURE ID's ---
GLuint icon_dashboard = 0;
GLuint icon_config = 0;
GLuint icon_logs = 0;
GLuint icon_cloud = 0;
GLuint icon_settings = 0;
GLuint icon_templates = 0;
GLuint icon_logo = 0;
GLuint icon_warning = 0;
GLuint icon_coin = 0;
GLuint icon_dia = 0;
GLuint icon_question = 0;
// --- GLOBAL SETTINGS ---
char g_AdbPathBuf[260] = "C:\\Program Files\\Microvirt\\MEmu\\adb.exe";
char g_MEmuPathBuf[260] = "C:\\Program Files\\Microvirt\\MEmu\\MEmuConsole.exe";
bool g_AdbValid = true;
bool g_MEmuValid = true;
bool g_EnableDiscordRPC = true;
bool g_SeparateTemplates = false;
bool g_EnableWebhookImage = false; 
bool g_OnlySellIfSiloFull = false;
CoordinateProfile g_CoordinateProfile;


// Path's
 std::string kAdbPath = g_AdbPathBuf;
 std::string kMEmuConsolePath = g_MEmuPathBuf;
 // paths for injecting
 std::string GAME_DATA_PATH = "/data/data/com.supercell.hayday/shared_prefs/storage_new.xml";
 std::string ZOOM_DATA_PATH = "/data/data/com.supercell.hayday/update/data/game_config.csv";

// --- TEMPLATE THRESHOLDS ---

TemplateThresholds g_Thresholds;

// --- TEMPLATE PATHS ---
std::string f_templatePath = "templates\\field.png";
std::string w_templatePath = "templates\\wheat.png";
std::string s_templatePath = "templates\\sickle.png";
std::string g_templatePath = "templates\\grown.png";
std::string shop_templatePath = "templates\\shop.png";
std::string roadside_shop_templatePath = "templates\\Roadside_Shop.png";
std::string wheatshop_templatePath = "templates\\wheat_shop.png";
std::string soldcrate_templatePath = "templates\\sold_crate.png";
std::string crate_templatePath = "templates\\shop_crate.png";
std::string arrows_templatePath = "templates\\arrows.png";
std::string plus_templatePath = "templates\\plus.png";
std::string cross_templatePath = "templates\\cross.png";
std::string advertise_templatePath = "templates\\advertise.png";
std::string occupied_crate_advertise_templatePath = "templates\\open_ad_2.png";
std::string create_ad_templatePath = "templates\\createad.png";
std::string occupied_crate_create_ad_templatePath = "templates\\Create_ad_2.png";
std::string feed_mill_templatePath = "templates\\feed_mill.png";
std::string cow_feed_templatePath = "templates\\cow_feed.png";
std::string dairy_templatePath = "templates\\dairy.png";
std::string cream_templatePath = "templates\\cream.png";
std::string butter_templatePath = "templates\\butter.png";
std::string cheese_templatePath = "templates\\cheese.png";
std::string not_enough_templatePath = "templates\\Not_enough.png";
std::string feed_mill_cross_templatePath = "templates\\feed_mill_cross.png";
std::string cow_pasture_templatePath = "templates\\cow_pasture.png";
std::string create_sale_templatePath = "templates\\create_sale.png";
std::string c_templatePath = "templates\\corn.png";
std::string gc_templatePath = "templates\\grown_corn.png";
std::string cornshop_templatePath = "templates\\corn_shop.png";
std::string barn_market_templatePath = "templates\\barn_market.png";
std::string silo_market_templatePath = "templates\\silo_market.png";
std::string mailbox_templatePath = "templates\\mailbox.png";
std::string loading_templatePath = "templates\\loading.png";
std::string crate_wheat_templatePath = "templates\\crate_wheat.png";
std::string crate_corn_templatePath = "templates\\crate_corn.png";
std::string levelup_templatePath = "templates\\levelup.png";
std::string levelup_continue_templatePath = "templates\\levelup_continue.png";
std::string carrot_templatePath = "templates\\carrot.png";
std::string grown_carrot_templatePath = "templates\\grown_carrot.png";
std::string carrot_shop_templatePath = "templates\\carrot_shop.png";
std::string crate_carrot_templatePath = "templates\\crate_carrot.png";
std::string crate_soybean_templatePath = "templates\\crate_soybean.png";
std::string crate_sugarcane_templatePath = "templates\\crate_sugarcane.png";
std::string soybean_templatePath = "templates\\soybean.png";
std::string grown_soybean_templatePath = "templates\\grown_soybean.png";
std::string soybean_shop_templatePath = "templates\\soybean_shop.png";

std::string sugarcane_templatePath = "templates\\sugarcane.png";
std::string grown_sugarcane_templatePath = "templates\\grown_sugarcane.png";
std::string sugarcane_shop_templatePath = "templates\\sugarcane_shop.png";
std::string silo_full_templatePath = "templates\\silo_full.png";
std::string silo_full_cross_templatePath = "templates\\silo_full_cross.png";
std::string market_close_crosstemplatePath = "templates\\market_close_cross.png";
std::string bolt_material_templatePath = "templates\\bolt_material.png";
std::string tape_material_templatePath = "templates\\tape_material.png";
std::string plank_material_templatePath = "templates\\plank_material.png";
std::string nail_material_templatePath = "templates\\nail_material.png";
std::string screw_material_templatePath = "templates\\screw_material.png";
std::string panel_material_templatePath = "templates\\panel_material.png";
std::string deed_material_templatePath = "templates\\deed_material.png";
std::string mallet_material_templatePath = "templates\\mallet_material.png";
std::string marker_material_templatePath = "templates\\marker_material.png";
std::string map_material_templatePath = "templates\\map_material.png";
std::string dynamite_templatePath = "templates\\dynamite.png";
std::string tnt_templatePath = "templates\\TNT.png";
std::string axe_templatePath = "templates\\axe.png";
std::string saw_templatePath = "templates\\saw.png";
std::string shovel_templatePath = "templates\\shovel.png";

// Buffers
char g_fieldPathBuf[260] = "templates\\field.png";
char g_wheatPathBuf[260] = "templates\\wheat.png";
char g_sicklePathBuf[260] = "templates\\sickle.png";
char g_grownPathBuf[260] = "templates\\grown.png";
char g_shopPathBuf[260] = "templates\\shop.png";
char g_roadsideShopPathBuf[260] = "templates\\Roadside_Shop.png";
char g_wheatshopPathBuf[260] = "templates\\wheat_shop.png";
char g_soldcratePathBuf[260] = "templates\\sold_crate.png";
char g_cratePathBuf[260] = "templates\\shop_crate.png";
char g_arrowsPathBuf[260] = "templates\\arrows.png";
char g_plusPathBuf[260] = "templates\\plus.png";
char g_crossPathBuf[260] = "templates\\cross.png";
char g_advertisePathBuf[260] = "templates\\advertise.png";
char g_occupiedCrateAdvertisePathBuf[260] = "templates\\open_ad_2.png";
char g_createAdPathBuf[260] = "templates\\createad.png";
char g_occupiedCrateCreateAdPathBuf[260] = "templates\\Create_ad_2.png";
char g_feedMillPathBuf[260] = "templates\\feed_mill.png";
char g_cowFeedPathBuf[260] = "templates\\cow_feed.png";
char g_dairyPathBuf[260] = "templates\\dairy.png";
char g_creamPathBuf[260] = "templates\\cream.png";
char g_butterPathBuf[260] = "templates\\butter.png";
char g_cheesePathBuf[260] = "templates\\cheese.png";
char g_notEnoughPathBuf[260] = "templates\\Not_enough.png";
char g_feedMillCrossPathBuf[260] = "templates\\feed_mill_cross.png";
char g_cowPasturePathBuf[260] = "templates\\cow_pasture.png";
char g_createSalePathBuf[260] = "templates\\create_sale.png";
char g_cornPathBuf[260] = "templates\\corn.png";
char g_grownCornPathBuf[260] = "templates\\grown_corn.png";
char g_cornShopPathBuf[260] = "templates\\corn_shop.png";
char g_barnMarketBuf[260] = "templates\\barn_market.png";
char g_siloMarketBuf[260] = "templates\\silo_market.png";
char g_mailboxPathBuf[260] = "templates\\mailbox.png";
char g_loadingPathBuf[260] = "templates\\loading.png";
char g_crateWheatPathBuf[260] = "templates\\crate_wheat.png";
char g_crateCornPathBuf[260] = "templates\\crate_corn.png";
char g_levelupPathBuf[260] = "templates\\levelup.png";
char g_levelupContinuePathBuf[260] = "templates\\levelup_continue.png";
char g_carrotPathBuf[260] = "templates\\carrot.png";
char g_grownCarrotPathBuf[260] = "templates\\grown_carrot.png";
char g_carrotShopPathBuf[260] = "templates\\carrot_shop.png";

char g_soybeanPathBuf[260] = "templates\\soybean.png";
char g_grownSoybeanPathBuf[260] = "templates\\grown_soybean.png";
char g_soybeanShopPathBuf[260] = "templates\\soybean_shop.png";

char g_sugarcanePathBuf[260] = "templates\\sugarcane.png";
char g_grownSugarcanePathBuf[260] = "templates\\grown_sugarcane.png";
char g_sugarcaneShopPathBuf[260] = "templates\\sugarcane_shop.png";
char g_siloFullPathBuf[260] = "templates\\silo_full.png";
char g_siloFullCrossPathBuf[260] = "templates\\silo_full_cross.png";
char g_marketCloseCrossPathBuf[260] = "templates\\market_close_cross.png";
char g_boltMaterialPathBuf[260] = "templates\\bolt_material.png";
char g_tapeMaterialPathBuf[260] = "templates\\tape_material.png";
char g_plankMaterialPathBuf[260] = "templates\\plank_material.png";
char g_nailMaterialPathBuf[260] = "templates\\nail_material.png";
char g_screwMaterialPathBuf[260] = "templates\\screw_material.png";
char g_panelMaterialPathBuf[260] = "templates\\panel_material.png";
char g_deedMaterialPathBuf[260] = "templates\\deed_material.png";
char g_malletMaterialPathBuf[260] = "templates\\mallet_material.png";
char g_markerMaterialPathBuf[260] = "templates\\marker_material.png";
char g_mapMaterialPathBuf[260] = "templates\\map_material.png";
char g_dynamitePathBuf[260] = "templates\\dynamite.png";
char g_tntPathBuf[260] = "templates\\TNT.png";
char g_axePathBuf[260] = "templates\\axe.png";
char g_sawPathBuf[260] = "templates\\saw.png";
char g_shovelPathBuf[260] = "templates\\shovel.png";

IntervalSettings g_Intervals;



// =========================================================
// LOG SYSTEM
// =========================================================
struct LogEntry {
    std::string timestamp;
    std::string message;
    int instanceId;
    ImVec4 color;
    std::string category;
    bool warningLike = false;
    int repeatCount = 1;
};

std::vector<LogEntry> g_GlobalLogs;
std::mutex g_LogMutex;
int g_LogFilter = -1;
bool g_AutoScrollLogs = true;
char g_LogSearchBuf[128] = "";
constexpr size_t kMaxGlobalLogEntries = 1000;
bool g_AutoClearLogs = false;
int g_AutoClearLogMinutes = 30;
std::chrono::steady_clock::time_point g_LastLogAutoClear = std::chrono::steady_clock::now();

struct FarmConfigProfile {
    std::string name = "default";
    int cropMode = 0;
    bool randomSaleCycle = false;
    int sellCycles = 0;
    bool sellAtMaxPrice = true;
    int saleStackMode = 0;
    int fixedSaleStack = 10;
    int keepWheatReserve = 0;
    int emergencyWheatStackLimit = 0;
    bool autoCowFeedEnabled = false;
    bool cowFeedUseFourSlots = false;
    bool autoDairyEnabled = false;
    int dairyProductMode = 0;
    int dairyQueueCount = 1;
};

std::vector<FarmConfigProfile> g_FarmConfigs{ FarmConfigProfile{} };
int g_SelectedFarmConfigIndex = 0;
char g_FarmConfigNameEditBuf[64] = "default";

static std::string NormalizeLogMessage(std::string message) {
    for (char& c : message) {
        if (c == '\r' || c == '\n' || c == '\t') c = ' ';
    }

    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    message.erase(message.begin(), std::find_if(message.begin(), message.end(), [&](unsigned char ch) { return !isSpace(ch); }));
    message.erase(std::find_if(message.rbegin(), message.rend(), [&](unsigned char ch) { return !isSpace(ch); }).base(), message.end());
    return message;
}

static bool LogColorsEqual(const ImVec4& a, const ImVec4& b) {
    return std::fabs(a.x - b.x) < 0.001f &&
        std::fabs(a.y - b.y) < 0.001f &&
        std::fabs(a.z - b.z) < 0.001f &&
        std::fabs(a.w - b.w) < 0.001f;
}

static bool ContainsCaseInsensitive(std::string haystack, const char* needle) {
    if (needle == nullptr || needle[0] == '\0') return true;

    std::string loweredNeedle = needle;
    auto lowerInPlace = [](std::string& text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
            });
    };

    lowerInPlace(haystack);
    lowerInPlace(loweredNeedle);
    return haystack.find(loweredNeedle) != std::string::npos;
}

static std::string ToUpperCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
        });
    return text;
}

static std::string DetectLogCategory(const std::string& message, int instanceId) {
    std::string upper = ToUpperCopy(message);
    if (upper.find("OCR") != std::string::npos || upper.find("PRESERVE") != std::string::npos) return "OCR";
    if (upper.find("SILO FULL") != std::string::npos || upper.find("POPUP") != std::string::npos || upper.find("CROSS") != std::string::npos || upper.find("MENU") != std::string::npos) return "POPUP";
    if (upper.find("HARVEST") != std::string::npos || upper.find("SICKLE") != std::string::npos || upper.find("FIELD") != std::string::npos || upper.find("GROW") != std::string::npos) return "HARVEST";
    if (upper.find("SHOP") != std::string::npos || upper.find("SALE") != std::string::npos || upper.find("SELL") != std::string::npos || upper.find("CRATE") != std::string::npos || upper.find("ADVERTISEMENT") != std::string::npos || upper.find("AD ") != std::string::npos) return "SALE";
    if (upper.find("BARN") != std::string::npos || upper.find("SILO") != std::string::npos || upper.find("MATERIAL") != std::string::npos) return "SCAN";
    if (instanceId == -1) return "SYSTEM";
    return "GENERAL";
}

static bool IsWarningLikeLog(const std::string& message, const ImVec4& color) {
    std::string upper = ToUpperCopy(message);
    if (upper.find("ERROR") != std::string::npos || upper.find("WARNING") != std::string::npos || upper.find("FAILED") != std::string::npos ||
        upper.find("UNREADABLE") != std::string::npos || upper.find("ABORT") != std::string::npos || upper.find("CRITICAL") != std::string::npos ||
        upper.find("TIMEOUT") != std::string::npos || upper.find("NOT FOUND") != std::string::npos || upper.find("QUARANTINE") != std::string::npos) {
        return true;
    }
    return color.x >= 0.95f && (color.y <= 0.8f || color.z <= 0.8f);
}
// gets clipboard text, used for quickly pasting player tags into the gui. dont worry this is not working randomly or used for malicious stuff. only used when bot pressese copy player tag button
std::string GetClipboardText() {
    if (!OpenClipboard(nullptr)) return "NO_TAG";
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr) { CloseClipboard(); return "NO_TAG"; }

    char* pszText = static_cast<char*>(GlobalLock(hData));
    std::string text;
    if (pszText) {
        
        SIZE_T max_len = GlobalSize(hData);
        text = std::string(pszText, strnlen(pszText, max_len));
    }
    else {
        text = "NO_TAG";
    }

    if (hData) GlobalUnlock(hData);
    CloseClipboard();

    std::string cleanText = "";
    for (char c : text) {
        if (c != '\r' && c != '\n' && c != ' ') cleanText += std::toupper(c);
    }
    return cleanText.empty() ? "NO_TAG" : cleanText;
}
// gets time for the logs
std::string GetTimeStr() {
    time_t rawtime;
    struct tm* timeinfo;
    char buffer[80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
    return std::string(buffer);
}

static std::string GetActionTimingLogPath() {
    return GetAppDataPath() + "\\action_timing_log.csv";
}

static bool EnsureActionTimingLogFile() {
    std::string timingPath = GetActionTimingLogPath();
    std::error_code ec;
    bool needsHeader = !fs::exists(timingPath, ec) || fs::file_size(timingPath, ec) == 0;

    std::ofstream out(timingPath, std::ios::app);
    if (!out.is_open()) return false;

    if (needsHeader) {
        out << "timestamp,instance,slot,category,action,duration_ms,duration_s,detail\n";
    }
    return true;
}

// Log adding function
void AddLog(int instanceId, std::string message, ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f)) {
    std::lock_guard<std::mutex> lock(g_LogMutex);
    message = NormalizeLogMessage(std::move(message));
    if (message.empty()) return;

    auto now = std::chrono::steady_clock::now();
    if (g_AutoClearLogs) {
        int clearMinutes = (std::max)(1, (std::min)(1440, g_AutoClearLogMinutes));
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - g_LastLogAutoClear).count();
        if (elapsed >= clearMinutes) {
            std::vector<LogEntry>().swap(g_GlobalLogs);
            g_LastLogAutoClear = now;
        }
    }

    std::string timestamp = GetTimeStr();
    if (!g_GlobalLogs.empty()) {
        LogEntry& lastLog = g_GlobalLogs.back();
        if (lastLog.instanceId == instanceId &&
            lastLog.message == message &&
            LogColorsEqual(lastLog.color, color)) {
            lastLog.timestamp = timestamp;
            lastLog.repeatCount++;
            return;
        }
    }

    LogEntry newLog;
    newLog.timestamp = std::move(timestamp);
    newLog.message = message;
    newLog.instanceId = instanceId;
    newLog.color = color;
    newLog.category = DetectLogCategory(message, instanceId);
    newLog.warningLike = IsWarningLikeLog(message, color);
    g_GlobalLogs.push_back(newLog);
    if (g_GlobalLogs.size() > kMaxGlobalLogEntries) {
        g_GlobalLogs.erase(g_GlobalLogs.begin(), g_GlobalLogs.begin() + (g_GlobalLogs.size() - kMaxGlobalLogEntries));
    }
}

BotInstance g_Bots[6];
static char g_MultiModeSkipBuf[6][64]{};
static bool g_MultiModeSkipBufSyncNeeded[6]{ true, true, true, true, true, true };

// --- Variables ---

char g_WebhookURL[256] = "";
bool g_EnableBarnWebhook = false; // off by default


char g_LicenseKey[256] = "";
int g_CurrentTab = 0; // current tab in the GUI
int g_SelectedInstanceUI = 0; // selected instance
extern void RunEmulatorFactory(int requestedInstance); // Defining the function in botengine.cpp

std::chrono::steady_clock::time_point g_BotStartTime;

static bool DrawCoordinateInputRow(const char* label, int& x, int& y) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(290.0f);
    ImGui::SetNextItemWidth(90.0f);
    changed |= ImGui::InputInt("X", &x, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    changed |= ImGui::InputInt("Y", &y, 0, 0);
    ImGui::PopID();
    return changed;
}

// --- FUNCTIONS ---
void ValidatePaths();
void SaveConfig();
void LoadConfig();
void SaveInventoryData();
void LoadInventoryData();
void InitializeBots();

std::string OpenFileDialog(const char* filter = "PNG Files\0*.png\0All Files\0*.*\0") {
    OPENFILENAMEA ofn;
    char fileName[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "png";
    if (GetOpenFileNameA(&ofn)) return std::string(fileName);
    return "";
}
std::string SaveFileDialog(const char* filter, const char* defaultExt) {
    OPENFILENAMEA ofn;
    char fileName[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = defaultExt;
    if (GetSaveFileNameA(&ofn)) return std::string(fileName);
    return "";
}
static std::string GetAccountBackupPath(int instanceId, int slotIndex) {
    return GetAppDataPath() + "\\Backups\\Instance_" + std::to_string(instanceId) +
           "\\account_" + std::to_string(slotIndex + 1) + ".nxrth";
}
static bool ReadFileToString(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}
static bool ReadFileToBytes(const std::string& path, std::vector<char>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();
    if (size < 0) return false;
    in.seekg(0, std::ios::beg);
    out.assign(static_cast<size_t>(size), 0);
    if (!out.empty()) in.read(out.data(), size);
    return in.good() || in.eof();
}
static bool WriteStringToFile(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return out.good();
}
static bool WriteBytesToFile(const std::string& path, const std::vector<char>& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    if (!data.empty()) out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return out.good();
}
static std::string EncodeConfigText(const std::string& value) {
    std::string encoded;
    encoded.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
        case '\\': encoded += "\\\\"; break;
        case '\n': encoded += "\\n"; break;
        case '\r': break;
        default: encoded += ch; break;
        }
    }
    return encoded;
}
static std::string DecodeConfigText(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        char ch = value[i];
        if (ch == '\\' && i + 1 < value.size()) {
            char next = value[i + 1];
            if (next == 'n') {
                decoded += '\n';
                ++i;
                continue;
            }
            if (next == '\\') {
                decoded += '\\';
                ++i;
                continue;
            }
        }
        decoded += ch;
    }
    return decoded;
}

static void EnsureFarmConfigs() {
    if (g_FarmConfigs.empty()) {
        g_FarmConfigs.push_back(FarmConfigProfile{});
    }
    bool hasDefault = false;
    for (const auto& cfg : g_FarmConfigs) {
        if (cfg.name == "default") {
            hasDefault = true;
            break;
        }
    }
    if (!hasDefault) {
        FarmConfigProfile defaultCfg;
        g_FarmConfigs.insert(g_FarmConfigs.begin(), defaultCfg);
    }
    g_SelectedFarmConfigIndex = (std::max)(0, (std::min)(static_cast<int>(g_FarmConfigs.size()) - 1, g_SelectedFarmConfigIndex));
}

static int FindFarmConfigIndex(const std::string& name) {
    EnsureFarmConfigs();
    for (size_t i = 0; i < g_FarmConfigs.size(); ++i) {
        if (g_FarmConfigs[i].name == name) return static_cast<int>(i);
    }
    return 0;
}

static FarmConfigProfile& GetFarmConfigForAccount(AccountSlot& account) {
    EnsureFarmConfigs();
    int idx = FindFarmConfigIndex(account.farmConfigName);
    if (account.farmConfigName.empty() || g_FarmConfigs[idx].name != account.farmConfigName) {
        account.farmConfigName = g_FarmConfigs[idx].name;
    }
    return g_FarmConfigs[idx];
}

void ApplyFarmConfigForSlot(int instanceId, int accountIndex) {
    if (instanceId < 0 || instanceId >= 6 || accountIndex < 0 || accountIndex >= 15) return;
    BotInstance& bot = g_Bots[instanceId];
    AccountSlot& account = bot.accounts[accountIndex];
    FarmConfigProfile& cfg = GetFarmConfigForAccount(account);

    bot.testCropMode = (std::max)(0, (std::min)(4, cfg.cropMode));
    bot.enableRandomSaleCycle = cfg.randomSaleCycle;
    account.targetCyclesBeforeSale = (std::max)(0, (std::min)(10, cfg.sellCycles));
    account.sellAtMaxPrice = cfg.sellAtMaxPrice;
    account.saleStackMode = (std::max)(0, (std::min)(3, cfg.saleStackMode));
    account.fixedSaleStack = (std::max)(1, (std::min)(10, cfg.fixedSaleStack));
    account.keepWheatReserve = (std::max)(0, (std::min)(100, cfg.keepWheatReserve));
    account.emergencyWheatStackLimit = (std::max)(0, (std::min)(20, cfg.emergencyWheatStackLimit));
    account.autoCowFeedEnabled = cfg.autoCowFeedEnabled;
    account.cowFeedUseFourSlots = cfg.cowFeedUseFourSlots;
    account.autoDairyEnabled = cfg.autoDairyEnabled;
    account.dairyProductMode = (std::max)(0, (std::min)(2, cfg.dairyProductMode));
    account.dairyQueueCount = (std::max)(1, (std::min)(4, cfg.dairyQueueCount));
}

static void EnsureAccountFarmConfigAssignments() {
    EnsureFarmConfigs();
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < MAX_ACCOUNT_SLOTS; ++j) {
            AccountSlot& acc = g_Bots[i].accounts[j];
            if (acc.farmConfigName.empty() || FindFarmConfigIndex(acc.farmConfigName) < 0) {
                acc.farmConfigName = g_FarmConfigs[0].name;
            }
        }
    }
}
template <typename T>
static bool WriteBinaryValue(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return out.good();
}
template <typename T>
static bool ReadBinaryValue(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return in.good();
}
static void RefreshAccountBackupState() {
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < MAX_ACCOUNT_SLOTS; ++j) {
            std::string path = GetAccountBackupPath(i, j);
            bool exists = fs::exists(path) && fs::file_size(path) > 0;
            g_Bots[i].accounts[j].hasFile = exists;
            g_Bots[i].accounts[j].fileName = exists ? path : "";
        }
    }
}
static bool ExportSettingsBundle(const std::string& bundlePath, std::string& errorOut) {
    SaveConfig();
    SaveInventoryData();

    std::string configPath = GetAppDataPath() + "\\nxrth_config.ini";
    std::string inventoryPath = GetAppDataPath() + "\\nxrth_inventory.ini";
    std::string configData;
    std::string inventoryData;
    if (!ReadFileToString(configPath, configData)) {
        errorOut = "Could not read nxrth_config.ini for export.";
        return false;
    }
    if (!ReadFileToString(inventoryPath, inventoryData)) {
        errorOut = "Could not read nxrth_inventory.ini for export.";
        return false;
    }

    std::ofstream out(bundlePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        errorOut = "Could not create the settings export file.";
        return false;
    }

    const std::string magic = "NXRTHSET1";
    uint64_t configSize = static_cast<uint64_t>(configData.size());
    uint64_t inventorySize = static_cast<uint64_t>(inventoryData.size());
    out.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!WriteBinaryValue(out, configSize) || !WriteBinaryValue(out, inventorySize)) {
        errorOut = "Could not write the settings export header.";
        return false;
    }
    if (!configData.empty()) out.write(configData.data(), static_cast<std::streamsize>(configData.size()));
    if (!inventoryData.empty()) out.write(inventoryData.data(), static_cast<std::streamsize>(inventoryData.size()));
    if (!out.good()) {
        errorOut = "Could not finish writing the settings export.";
        return false;
    }
    return true;
}
static bool ImportSettingsBundle(const std::string& bundlePath, std::string& errorOut) {
    std::ifstream in(bundlePath, std::ios::binary);
    if (!in.is_open()) {
        errorOut = "Could not open the selected settings file.";
        return false;
    }

    const std::string magic = "NXRTHSET1";
    std::string magicRead(magic.size(), '\0');
    in.read(&magicRead[0], static_cast<std::streamsize>(magicRead.size()));
    if (magicRead != magic) {
        errorOut = "The selected file is not a valid NXRTH settings export.";
        return false;
    }

    uint64_t configSize = 0;
    uint64_t inventorySize = 0;
    if (!ReadBinaryValue(in, configSize) || !ReadBinaryValue(in, inventorySize)) {
        errorOut = "The settings export header is incomplete.";
        return false;
    }

    std::string configData(static_cast<size_t>(configSize), '\0');
    std::string inventoryData(static_cast<size_t>(inventorySize), '\0');
    if (configSize > 0) in.read(&configData[0], static_cast<std::streamsize>(configSize));
    if (inventorySize > 0) in.read(&inventoryData[0], static_cast<std::streamsize>(inventorySize));
    if (!in.good() && !in.eof()) {
        errorOut = "The settings export file is corrupted.";
        return false;
    }

    std::string appData = GetAppDataPath();
    if (!WriteStringToFile(appData + "\\nxrth_config.ini", configData)) {
        errorOut = "Could not write nxrth_config.ini during import.";
        return false;
    }
    if (!WriteStringToFile(appData + "\\nxrth_inventory.ini", inventoryData)) {
        errorOut = "Could not write nxrth_inventory.ini during import.";
        return false;
    }

    LoadConfig();
    ValidatePaths();
    InitializeBots();
    LoadInventoryData();
    RefreshAccountBackupState();
    return true;
}
static bool ExportAccountsBundle(const std::string& bundlePath, std::string& errorOut) {
    struct AccountBundleEntry {
        uint32_t instanceId = 0;
        uint32_t slotIndex = 0;
        std::vector<char> data;
    };

    std::vector<AccountBundleEntry> entries;
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < MAX_ACCOUNT_SLOTS; ++j) {
            std::string path = GetAccountBackupPath(i, j);
            if (!fs::exists(path) || fs::file_size(path) == 0) continue;
            AccountBundleEntry entry;
            entry.instanceId = static_cast<uint32_t>(i);
            entry.slotIndex = static_cast<uint32_t>(j);
            if (!ReadFileToBytes(path, entry.data)) {
                errorOut = "Could not read one of the saved account slots.";
                return false;
            }
            entries.push_back(std::move(entry));
        }
    }

    std::ofstream out(bundlePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        errorOut = "Could not create the accounts export file.";
        return false;
    }

    const std::string magic = "NXRTHACC1";
    uint32_t entryCount = static_cast<uint32_t>(entries.size());
    out.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!WriteBinaryValue(out, entryCount)) {
        errorOut = "Could not write the accounts export header.";
        return false;
    }

    for (const AccountBundleEntry& entry : entries) {
        uint64_t dataSize = static_cast<uint64_t>(entry.data.size());
        if (!WriteBinaryValue(out, entry.instanceId) ||
            !WriteBinaryValue(out, entry.slotIndex) ||
            !WriteBinaryValue(out, dataSize)) {
            errorOut = "Could not write one of the account bundle entries.";
            return false;
        }
        if (!entry.data.empty()) {
            out.write(entry.data.data(), static_cast<std::streamsize>(entry.data.size()));
            if (!out.good()) {
                errorOut = "Could not finish writing one of the account bundle entries.";
                return false;
            }
        }
    }
    return true;
}
static bool ImportAccountsBundle(const std::string& bundlePath, std::string& errorOut) {
    std::ifstream in(bundlePath, std::ios::binary);
    if (!in.is_open()) {
        errorOut = "Could not open the selected accounts file.";
        return false;
    }

    const std::string magic = "NXRTHACC1";
    std::string magicRead(magic.size(), '\0');
    in.read(&magicRead[0], static_cast<std::streamsize>(magicRead.size()));
    if (magicRead != magic) {
        errorOut = "The selected file is not a valid NXRTH accounts export.";
        return false;
    }

    uint32_t entryCount = 0;
    if (!ReadBinaryValue(in, entryCount)) {
        errorOut = "The accounts export header is incomplete.";
        return false;
    }

    for (uint32_t index = 0; index < entryCount; ++index) {
        uint32_t instanceId = 0;
        uint32_t slotIndex = 0;
        uint64_t dataSize = 0;
        if (!ReadBinaryValue(in, instanceId) ||
            !ReadBinaryValue(in, slotIndex) ||
            !ReadBinaryValue(in, dataSize)) {
            errorOut = "The accounts export file is corrupted.";
            return false;
        }
        if (instanceId >= 6 || slotIndex >= 15) {
            errorOut = "The accounts export file contains an invalid slot id.";
            return false;
        }

        std::vector<char> data(static_cast<size_t>(dataSize), 0);
        if (dataSize > 0) in.read(data.data(), static_cast<std::streamsize>(dataSize));
        if (!in.good() && !in.eof()) {
            errorOut = "The accounts export file ended unexpectedly.";
            return false;
        }

        std::string folderPath = GetAppDataPath() + "\\Backups\\Instance_" + std::to_string(instanceId);
        std::error_code ec;
        fs::create_directories(folderPath, ec);
        if (ec) {
            errorOut = "Could not create the account backup folder during import.";
            return false;
        }

        if (!WriteBytesToFile(GetAccountBackupPath(static_cast<int>(instanceId), static_cast<int>(slotIndex)), data)) {
            errorOut = "Could not write one of the imported account slots.";
            return false;
        }
    }

    RefreshAccountBackupState();
    return true;
}
// Checks if the paths are correct
void ValidatePaths() {
    g_AdbValid = fs::exists(g_AdbPathBuf);
    g_MEmuValid = fs::exists(g_MEmuPathBuf);
    if (g_AdbValid) kAdbPath = std::string(g_AdbPathBuf);
    if (g_MEmuValid) kMEmuConsolePath = std::string(g_MEmuPathBuf);
}
// settings saver
void SaveConfig() {
    std::string realPath = GetAppDataPath() + "\\nxrth_config.ini";
    std::string tempPath = GetAppDataPath() + "\\nxrth_config.tmp"; 

    std::ofstream out(tempPath);
    if (out.is_open()) {
        out << "AdbPath=" << g_AdbPathBuf << "\n";
        out << "MEmuPath=" << g_MEmuPathBuf << "\n";
        out << "WebhookURL=" << g_WebhookURL << "\n";
        
        out << "EnableBarnWebhook=" << (g_EnableBarnWebhook ? "1" : "0") << "\n";
        out << "EnableWebhookImage=" << (g_EnableWebhookImage ? "1" : "0") << "\n"; 
        out << "GameLoadWait=" << g_Intervals.gameLoadWait << "\n";
        out << "AfterHarvestWait=" << g_Intervals.afterHarvestWait << "\n";
        out << "AfterPlantWait=" << g_Intervals.afterPlantWait << "\n";
        out << "ShopEnterWait=" << g_Intervals.shopEnterWait << "\n";
        out << "CrateClickWait=" << g_Intervals.crateClickWait << "\n";
        out << "NextAccountWait=" << g_Intervals.nextAccountWait << "\n";
       
        out << "CoinCollectWait=" << g_Intervals.coinCollectWait << "\n";
        out << "ProductSelectWait=" << g_Intervals.productSelectWait << "\n";
        out << "CreateSaleWait=" << g_Intervals.createSaleWait << "\n";
        out << "AutoZoomOutAfterMailbox=" << (g_Intervals.autoZoomOutAfterMailbox ? "1" : "0") << "\n";
        out << "MailboxZoomDelayMs=" << g_Intervals.mailboxZoomDelayMs << "\n";
        out << "OcrAnchorThreshold=" << g_OcrAnchorThreshold << "\n";
        out << "WheatPreserveScanMode=" << g_WheatPreserveScanMode << "\n";
        out << "WheatPreserveCoordX=" << g_WheatPreserveCoordX << "\n";
        out << "WheatPreserveCoordY=" << g_WheatPreserveCoordY << "\n";
        out << "WheatPreserveRoiLeft=" << g_WheatPreserveRoiLeft << "\n";
        out << "WheatPreserveRoiTop=" << g_WheatPreserveRoiTop << "\n";
        out << "WheatPreserveRoiRight=" << g_WheatPreserveRoiRight << "\n";
        out << "WheatPreserveRoiBottom=" << g_WheatPreserveRoiBottom << "\n";
        out << "TransferThreshold=" << g_TransferThreshold << "\n";
        out << "StorageTag=" << g_StorageTagBuf << "\n";
        out << "GlobalEmuMode=" << g_GlobalEmulatorMode << "\n";
        out << "OnlySellIfSiloFull=" << (g_OnlySellIfSiloFull ? "1" : "0") << "\n";
        out << "AutoClearLogs=" << (g_AutoClearLogs ? "1" : "0") << "\n";
        out << "AutoClearLogMinutes=" << g_AutoClearLogMinutes << "\n";
        EnsureFarmConfigs();
        out << "FarmConfigCount=" << g_FarmConfigs.size() << "\n";
        for (size_t cfgIdx = 0; cfgIdx < g_FarmConfigs.size(); ++cfgIdx) {
            const FarmConfigProfile& cfg = g_FarmConfigs[cfgIdx];
            std::string cfgPfx = "FarmConfig_" + std::to_string(cfgIdx) + "_";
            out << cfgPfx << "Name=" << EncodeConfigText(cfg.name) << "\n";
            out << cfgPfx << "Crop=" << cfg.cropMode << "\n";
            out << cfgPfx << "RandCycle=" << (cfg.randomSaleCycle ? "1" : "0") << "\n";
            out << cfgPfx << "SellCycles=" << cfg.sellCycles << "\n";
            out << cfgPfx << "SellMax=" << (cfg.sellAtMaxPrice ? "1" : "0") << "\n";
            out << cfgPfx << "SaleStackMode=" << cfg.saleStackMode << "\n";
            out << cfgPfx << "FixedStack=" << cfg.fixedSaleStack << "\n";
            out << cfgPfx << "KeepWheat=" << cfg.keepWheatReserve << "\n";
            out << cfgPfx << "EmergencyWheatLimit=" << cfg.emergencyWheatStackLimit << "\n";
            out << cfgPfx << "AutoCowFeed=" << (cfg.autoCowFeedEnabled ? "1" : "0") << "\n";
            out << cfgPfx << "CowFeedFourSlots=" << (cfg.cowFeedUseFourSlots ? "1" : "0") << "\n";
            out << cfgPfx << "AutoDairy=" << (cfg.autoDairyEnabled ? "1" : "0") << "\n";
            out << cfgPfx << "DairyProduct=" << cfg.dairyProductMode << "\n";
            out << cfgPfx << "DairyQueue=" << cfg.dairyQueueCount << "\n";
        }
        out << "CoordProfileOpenX=" << g_CoordinateProfile.profileOpenX << "\n";
        out << "CoordProfileOpenY=" << g_CoordinateProfile.profileOpenY << "\n";
        out << "CoordProfileCopyTagX=" << g_CoordinateProfile.profileCopyTagX << "\n";
        out << "CoordProfileCopyTagY=" << g_CoordinateProfile.profileCopyTagY << "\n";
        out << "CoordProfileCloseX=" << g_CoordinateProfile.profileCloseX << "\n";
        out << "CoordProfileCloseY=" << g_CoordinateProfile.profileCloseY << "\n";
        out << "CoordSalePanelCloseX=" << g_CoordinateProfile.salePanelCloseX << "\n";
        out << "CoordSalePanelCloseY=" << g_CoordinateProfile.salePanelCloseY << "\n";
        out << "CoordMarketCloseX=" << g_CoordinateProfile.marketCloseX << "\n";
        out << "CoordMarketCloseY=" << g_CoordinateProfile.marketCloseY << "\n";
        out << "CoordMarketCloseSecondX=" << g_CoordinateProfile.marketCloseSecondX << "\n";
        out << "CoordMarketCloseSecondY=" << g_CoordinateProfile.marketCloseSecondY << "\n";
        out << "CoordOccupiedCrateAdOpenX=" << g_CoordinateProfile.occupiedCrateAdOpenX << "\n";
        out << "CoordOccupiedCrateAdOpenY=" << g_CoordinateProfile.occupiedCrateAdOpenY << "\n";
        out << "CoordCreateAdConfirmX=" << g_CoordinateProfile.createAdConfirmX << "\n";
        out << "CoordCreateAdConfirmY=" << g_CoordinateProfile.createAdConfirmY << "\n";
        out << "CoordSaleQuantityPlusX=" << g_CoordinateProfile.saleQuantityPlusX << "\n";
        out << "CoordSaleQuantityPlusY=" << g_CoordinateProfile.saleQuantityPlusY << "\n";
        out << "CoordSaleQuantityMinusX=" << g_CoordinateProfile.saleQuantityMinusX << "\n";
        out << "CoordSaleQuantityMinusY=" << g_CoordinateProfile.saleQuantityMinusY << "\n";
        out << "CoordSaleMaxPriceX=" << g_CoordinateProfile.saleMaxPriceX << "\n";
        out << "CoordSaleMaxPriceY=" << g_CoordinateProfile.saleMaxPriceY << "\n";
        out << "CoordSalePriceMinusX=" << g_CoordinateProfile.salePriceMinusX << "\n";
        out << "CoordSalePriceMinusY=" << g_CoordinateProfile.salePriceMinusY << "\n";
        out << "CoordPutOnSaleX=" << g_CoordinateProfile.putOnSaleX << "\n";
        out << "CoordPutOnSaleY=" << g_CoordinateProfile.putOnSaleY << "\n";
        for (int i = 0; i < 6; i++) {
            out << "Inst_" << i << "_Active=" << (g_Bots[i].isActive ? "1" : "0") << "\n";
            out << "Inst_" << i << "_Touch=" << g_Bots[i].inputDevice << "\n";
            out << "Inst_" << i << "_Adb=" << g_Bots[i].adbSerial << "\n";
            out << "Inst_" << i << "_Vm=" << g_Bots[i].vmName << "\n";
            out << "Inst_" << i << "_Crop=" << g_Bots[i].testCropMode << "\n";
            out << "Inst_" << i << "_Single=" << (g_Bots[i].useSingleMode ? "1" : "0") << "\n";
            out << "Inst_" << i << "_Multi=" << (g_Bots[i].useMultiMode ? "1" : "0") << "\n";
            out << "Inst_" << i << "_RandCycle=" << (g_Bots[i].enableRandomSaleCycle ? "1" : "0") << "\n";
            out << "Inst_" << i << "_Revive=" << (g_Bots[i].useReviveMode ? "1" : "0") << "\n";
            out << "Inst_" << i << "_ReviveSec=" << g_Bots[i].reviveCheckInterval << "\n";
            out << "Inst_" << i << "_ReviveTpl=" << EncodeConfigText(g_Bots[i].reviveTemplatePath) << "\n";
            out << "Inst_" << i << "_SkipSlots=" << EncodeConfigText(g_Bots[i].multiModeSkipSlots) << "\n";
            out << "Inst_" << i << "_RunLimit=" << (g_Bots[i].enableAccountRunLimit ? "1" : "0") << "\n";
            out << "Inst_" << i << "_RunMinutes=" << g_Bots[i].accountRunMinutes << "\n";
            out << "Inst_" << i << "_RestMinutes=" << g_Bots[i].accountRestMinutes << "\n";
            out << "Inst_" << i << "_ReplaceDuringRest=" << (g_Bots[i].replaceAccountDuringRest ? "1" : "0") << "\n";
            out << "Inst_" << i << "_CyclesPerAccount=" << g_Bots[i].cyclesPerAccount << "\n";
            out << "Inst_" << i << "_PairRotation=" << (g_Bots[i].enablePairRotation ? "1" : "0") << "\n";
            out << "Inst_" << i << "_PairGroupSize=" << g_Bots[i].pairGroupSize << "\n";
            out << "Inst_" << i << "_PairRotateMin=" << g_Bots[i].pairRotationMinutes << "\n";
            out << "Inst_" << i << "_PairOffset=" << g_Bots[i].currentPairOffset << "\n";
            out << "Inst_" << i << "_Janitor=" << (g_Bots[i].enableJanitor ? "1" : "0") << "\n";
            out << "Inst_" << i << "_JanitorLimit=" << g_Bots[i].janitorLimit << "\n";
        }
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < MAX_ACCOUNT_SLOTS; j++) {
                AccountSlot& acc = g_Bots[i].accounts[j];
                std::string pfx = "Tom_" + std::to_string(i) + "_" + std::to_string(j) + "_";
                out << pfx << "En=" << (acc.autoTomEnabled ? "1" : "0") << "\n";
                out << pfx << "Rem=" << acc.tomRemainingHours << "\n";
                out << pfx << "RemMin=" << acc.tomRemainingMinutes << "\n";
                out << pfx << "Start=" << acc.tomStartTime << "\n";
                out << pfx << "Next=" << acc.nextTomTime << "\n";
                out << pfx << "Cat=" << acc.tomCategory << "\n";
                out << pfx << "Item=" << acc.tomItemName << "\n";
                std::string accPfx = "Acc_" + std::to_string(i) + "_" + std::to_string(j) + "_";
                out << accPfx << "CustomName=" << acc.name << "\n";
                out << accPfx << "Note=" << EncodeConfigText(acc.note) << "\n";
                out << accPfx << "FarmConfig=" << EncodeConfigText(acc.farmConfigName) << "\n";
                out << accPfx << "Lvl=" << acc.level << "\n";
                out << accPfx << "Tag=" << acc.playerTag << "\n";
                out << accPfx << "Friend=" << (acc.isFriendWithStorage ? "1" : "0") << "\n";
                out << accPfx << "Name=" << acc.farmName << "\n";
                out << accPfx << "Coin=" << acc.coinAmount << "\n";
                out << accPfx << "Dia=" << acc.diamondAmount << "\n";
                out << accPfx << "SellCycles=" << acc.targetCyclesBeforeSale << "\n";
                out << accPfx << "SellCounter=" << acc.currentCyclesWithoutSale << "\n";
                out << accPfx << "SellMax=" << (acc.sellAtMaxPrice ? "1" : "0") << "\n";
                out << accPfx << "SaleStackMode=" << acc.saleStackMode << "\n";
                out << accPfx << "FixedStack=" << acc.fixedSaleStack << "\n";
                out << accPfx << "KeepWheat=" << acc.keepWheatReserve << "\n";
                out << accPfx << "EmergencyWheatLimit=" << acc.emergencyWheatStackLimit << "\n";
                out << accPfx << "AutoCowFeed=" << (acc.autoCowFeedEnabled ? "1" : "0") << "\n";
                out << accPfx << "CowFeedFourSlots=" << (acc.cowFeedUseFourSlots ? "1" : "0") << "\n";
                out << accPfx << "CowFeedRecoveryRequested=" << (acc.cowFeedCropRecoveryRequested ? "1" : "0") << "\n";
                out << accPfx << "CowFeedRecoveryPlanted=" << (acc.cowFeedCropRecoveryPlanted ? "1" : "0") << "\n";
                out << accPfx << "AutoDairy=" << (acc.autoDairyEnabled ? "1" : "0") << "\n";
                out << accPfx << "DairyProduct=" << acc.dairyProductMode << "\n";
                out << accPfx << "DairyQueue=" << acc.dairyQueueCount << "\n";
                
            
            }
        }
        
        out.close();
        std::error_code ec;
        std::filesystem::remove(realPath, ec); // Delete old path
        std::filesystem::rename(tempPath, realPath, ec);
    }
}
//config loader
void LoadConfig() {
    std::string configPath = GetAppDataPath() + "\\nxrth_config.ini";
    std::ifstream in(configPath);
    if (in.is_open()) {
        g_FarmConfigs.clear();
        g_SelectedFarmConfigIndex = 0;
        std::string line;
        while (std::getline(in, line)) {
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = line.substr(0, eqPos);
                std::string val = line.substr(eqPos + 1);

                if (key == "AdbPath") strncpy(g_AdbPathBuf, val.c_str(), 260);
                else if (key == "MEmuPath") strncpy(g_MEmuPathBuf, val.c_str(), 260);
                else if (key == "WebhookURL") strncpy(g_WebhookURL, val.c_str(), 256);
               
                else if (key == "EnableBarnWebhook") g_EnableBarnWebhook = (val == "1");
               
                else if (key == "EnableWebhookImage") g_EnableWebhookImage = (val == "1"); 
                else if (key == "DiscordID") strncpy(g_DiscordID, val.c_str(), 64);
                else if (key == "GameLoadWait") g_Intervals.gameLoadWait = std::stoi(val);
                else if (key == "AfterHarvestWait") g_Intervals.afterHarvestWait = std::stoi(val);
                else if (key == "AfterPlantWait") g_Intervals.afterPlantWait = std::stoi(val);
                else if (key == "ShopEnterWait") g_Intervals.shopEnterWait = std::stoi(val);
                else if (key == "CrateClickWait") g_Intervals.crateClickWait = std::stoi(val);
                else if (key == "NextAccountWait") g_Intervals.nextAccountWait = std::stoi(val);
                
                else if (key == "GlobalEmuMode") g_GlobalEmulatorMode = std::stoi(val);
                else if (key == "CoinCollectWait") g_Intervals.coinCollectWait = std::stoi(val);
                else if (key == "ProductSelectWait") g_Intervals.productSelectWait = std::stoi(val);
                else if (key == "CreateSaleWait") g_Intervals.createSaleWait = std::stoi(val);
                else if (key == "AutoZoomOutAfterMailbox") g_Intervals.autoZoomOutAfterMailbox = (val == "1");
                else if (key == "MailboxZoomDelayMs") g_Intervals.mailboxZoomDelayMs = (std::max)(0, std::stoi(val));
                else if (key == "OcrAnchorThreshold") g_OcrAnchorThreshold = std::stof(val);
                else if (key == "WheatPreserveScanMode") g_WheatPreserveScanMode = (std::max)(0, (std::min)(2, std::stoi(val)));
                else if (key == "WheatPreserveCoordX") g_WheatPreserveCoordX = std::stoi(val);
                else if (key == "WheatPreserveCoordY") g_WheatPreserveCoordY = std::stoi(val);
                else if (key == "WheatPreserveRoiLeft") g_WheatPreserveRoiLeft = std::stoi(val);
                else if (key == "WheatPreserveRoiTop") g_WheatPreserveRoiTop = std::stoi(val);
                else if (key == "WheatPreserveRoiRight") g_WheatPreserveRoiRight = std::stoi(val);
                else if (key == "WheatPreserveRoiBottom") g_WheatPreserveRoiBottom = std::stoi(val);
                else if (key == "TransferThreshold") g_TransferThreshold = std::stoi(val);
                else if (key == "OnlySellIfSiloFull") g_OnlySellIfSiloFull = (val == "1");
                else if (key == "AutoClearLogs") g_AutoClearLogs = (val == "1");
                else if (key == "AutoClearLogMinutes") g_AutoClearLogMinutes = (std::max)(1, (std::min)(1440, std::stoi(val)));
                else if (key == "FarmConfigCount") {
                    int count = (std::max)(1, (std::min)(64, std::stoi(val)));
                    g_FarmConfigs.resize(count);
                    for (int idx = 0; idx < count; ++idx) {
                        if (g_FarmConfigs[idx].name.empty() || g_FarmConfigs[idx].name == "default") {
                            g_FarmConfigs[idx].name = (idx == 0) ? "default" : ("config_" + std::to_string(idx + 1));
                        }
                    }
                }
                else if (key.rfind("FarmConfig_", 0) == 0) {
                    int cfgIdx = -1;
                    char subKeyBuf[64]{};
                    if (sscanf(key.c_str(), "FarmConfig_%d_%63s", &cfgIdx, subKeyBuf) == 2 && cfgIdx >= 0 && cfgIdx < 64) {
                        if (static_cast<int>(g_FarmConfigs.size()) <= cfgIdx) g_FarmConfigs.resize(cfgIdx + 1);
                        FarmConfigProfile& cfg = g_FarmConfigs[cfgIdx];
                        if (cfg.name.empty()) cfg.name = (cfgIdx == 0) ? "default" : ("config_" + std::to_string(cfgIdx + 1));
                        std::string subKey(subKeyBuf);
                        if (subKey == "Name") cfg.name = DecodeConfigText(val);
                        else if (subKey == "Crop") cfg.cropMode = (std::max)(0, (std::min)(4, std::stoi(val)));
                        else if (subKey == "RandCycle") cfg.randomSaleCycle = (val == "1");
                        else if (subKey == "SellCycles") cfg.sellCycles = (std::max)(0, (std::min)(10, std::stoi(val)));
                        else if (subKey == "SellMax") cfg.sellAtMaxPrice = (val == "1");
                        else if (subKey == "SaleStackMode") cfg.saleStackMode = (std::max)(0, (std::min)(3, std::stoi(val)));
                        else if (subKey == "FixedStack") cfg.fixedSaleStack = (std::max)(1, (std::min)(10, std::stoi(val)));
                        else if (subKey == "KeepWheat") cfg.keepWheatReserve = (std::max)(0, (std::min)(100, std::stoi(val)));
                        else if (subKey == "EmergencyWheatLimit") cfg.emergencyWheatStackLimit = (std::max)(0, (std::min)(20, std::stoi(val)));
                        else if (subKey == "AutoCowFeed") cfg.autoCowFeedEnabled = (val == "1");
                        else if (subKey == "CowFeedFourSlots") cfg.cowFeedUseFourSlots = (val == "1");
                        else if (subKey == "AutoDairy") cfg.autoDairyEnabled = (val == "1");
                        else if (subKey == "DairyProduct") cfg.dairyProductMode = (std::max)(0, (std::min)(2, std::stoi(val)));
                        else if (subKey == "DairyQueue") cfg.dairyQueueCount = (std::max)(1, (std::min)(4, std::stoi(val)));
                    }
                }
                else if (key == "CoordProfileOpenX") g_CoordinateProfile.profileOpenX = std::stoi(val);
                else if (key == "CoordProfileOpenY") g_CoordinateProfile.profileOpenY = std::stoi(val);
                else if (key == "CoordProfileCopyTagX") g_CoordinateProfile.profileCopyTagX = std::stoi(val);
                else if (key == "CoordProfileCopyTagY") g_CoordinateProfile.profileCopyTagY = std::stoi(val);
                else if (key == "CoordProfileCloseX") g_CoordinateProfile.profileCloseX = std::stoi(val);
                else if (key == "CoordProfileCloseY") g_CoordinateProfile.profileCloseY = std::stoi(val);
                else if (key == "CoordSalePanelCloseX") g_CoordinateProfile.salePanelCloseX = std::stoi(val);
                else if (key == "CoordSalePanelCloseY") g_CoordinateProfile.salePanelCloseY = std::stoi(val);
                else if (key == "CoordMarketCloseX") g_CoordinateProfile.marketCloseX = std::stoi(val);
                else if (key == "CoordMarketCloseY") g_CoordinateProfile.marketCloseY = std::stoi(val);
                else if (key == "CoordMarketCloseSecondX") g_CoordinateProfile.marketCloseSecondX = std::stoi(val);
                else if (key == "CoordMarketCloseSecondY") g_CoordinateProfile.marketCloseSecondY = std::stoi(val);
                else if (key == "CoordOccupiedCrateAdOpenX") g_CoordinateProfile.occupiedCrateAdOpenX = std::stoi(val);
                else if (key == "CoordOccupiedCrateAdOpenY") g_CoordinateProfile.occupiedCrateAdOpenY = std::stoi(val);
                else if (key == "CoordCreateAdConfirmX") g_CoordinateProfile.createAdConfirmX = std::stoi(val);
                else if (key == "CoordCreateAdConfirmY") g_CoordinateProfile.createAdConfirmY = std::stoi(val);
                else if (key == "CoordSaleQuantityPlusX") g_CoordinateProfile.saleQuantityPlusX = std::stoi(val);
                else if (key == "CoordSaleQuantityPlusY") g_CoordinateProfile.saleQuantityPlusY = std::stoi(val);
                else if (key == "CoordSaleQuantityMinusX") g_CoordinateProfile.saleQuantityMinusX = std::stoi(val);
                else if (key == "CoordSaleQuantityMinusY") g_CoordinateProfile.saleQuantityMinusY = std::stoi(val);
                else if (key == "CoordSaleMaxPriceX") g_CoordinateProfile.saleMaxPriceX = std::stoi(val);
                else if (key == "CoordSaleMaxPriceY") g_CoordinateProfile.saleMaxPriceY = std::stoi(val);
                else if (key == "CoordSalePriceMinusX") g_CoordinateProfile.salePriceMinusX = std::stoi(val);
                else if (key == "CoordSalePriceMinusY") g_CoordinateProfile.salePriceMinusY = std::stoi(val);
                else if (key == "CoordPutOnSaleX") g_CoordinateProfile.putOnSaleX = std::stoi(val);
                else if (key == "CoordPutOnSaleY") g_CoordinateProfile.putOnSaleY = std::stoi(val);

                else if (key == "StorageTag") {
                    strncpy(g_StorageTagBuf, val.c_str(), sizeof(g_StorageTagBuf));
                    g_StorageTag = val; 
                }
                else if (key.rfind("Inst_", 0) == 0) {
                    int i = key[5] - '0';
                    std::string subKey = key.substr(7);
                    if (i >= 0 && i < 6) {
                        if (subKey == "Active") g_Bots[i].isActive = (val == "1");
                        else if (subKey == "Touch") strncpy(g_Bots[i].inputDevice, val.c_str(), 64);
                        else if (subKey == "Adb") strncpy(g_Bots[i].adbSerial, val.c_str(), 64);
                        else if (subKey == "Vm") strncpy(g_Bots[i].vmName, val.c_str(), 64);
                        else if (subKey == "Crop") g_Bots[i].testCropMode = (std::max)(0, (std::min)(4, std::stoi(val)));
                        else if (subKey == "Single") g_Bots[i].useSingleMode = (val == "1");
                        else if (subKey == "Multi") g_Bots[i].useMultiMode = (val == "1");
                        else if (subKey == "RandCycle") g_Bots[i].enableRandomSaleCycle = (val == "1");
                        else if (subKey == "Revive") g_Bots[i].useReviveMode = (val == "1");
                        else if (subKey == "ReviveSec") g_Bots[i].reviveCheckInterval = (std::max)(60, (std::min)(600, std::stoi(val)));
                        else if (subKey == "ReviveTpl") strncpy(g_Bots[i].reviveTemplatePath, DecodeConfigText(val).c_str(), MAX_PATH);
                        else if (subKey == "SkipSlots") g_Bots[i].multiModeSkipSlots = DecodeConfigText(val);
                        else if (subKey == "RunLimit") g_Bots[i].enableAccountRunLimit = (val == "1");
                        else if (subKey == "RunMinutes") g_Bots[i].accountRunMinutes = (std::max)(1, (std::min)(1440, std::stoi(val)));
                        else if (subKey == "RestMinutes") g_Bots[i].accountRestMinutes = (std::max)(1, (std::min)(1440, std::stoi(val)));
                        else if (subKey == "ReplaceDuringRest") g_Bots[i].replaceAccountDuringRest = (val == "1");
                        else if (subKey == "CyclesPerAccount") g_Bots[i].cyclesPerAccount = (std::max)(1, (std::min)(50, std::stoi(val)));
                        else if (subKey == "PairRotation") g_Bots[i].enablePairRotation = (val == "1");
                        else if (subKey == "PairGroupSize") g_Bots[i].pairGroupSize = (std::max)(1, (std::min)(5, std::stoi(val)));
                        else if (subKey == "PairRotateMin") g_Bots[i].pairRotationMinutes = (std::max)(5, (std::min)(1440, std::stoi(val)));
                        else if (subKey == "PairOffset") g_Bots[i].currentPairOffset = (std::max)(0, std::stoi(val));
                        else if (subKey == "Janitor") g_Bots[i].enableJanitor = (val == "1");
                        else if (subKey == "JanitorLimit") g_Bots[i].janitorLimit = (std::max)(3, (std::min)(50, std::stoi(val)));
                    }
                }
               else if (key.rfind("Tom_", 0) == 0) { // If starts with tom
                    int i, j;
                    char subKeyBuf[64];
                    
                    if (sscanf(key.c_str(), "Tom_%d_%d_%s", &i, &j, subKeyBuf) == 3) {
                        if (i >= 0 && i < 6 && j >= 0 && j < MAX_ACCOUNT_SLOTS) {
                            AccountSlot& acc = g_Bots[i].accounts[j];
                            std::string subKey(subKeyBuf);
                            if (subKey == "En") acc.autoTomEnabled = (val == "1");
                            else if (subKey == "Rem") acc.tomRemainingHours = std::stoi(val);
                            else if (subKey == "RemMin") acc.tomRemainingMinutes = (std::max)(0, (std::min)(59, std::stoi(val)));
                            else if (subKey == "Start") acc.tomStartTime = std::stoll(val);
                            else if (subKey == "Next") acc.nextTomTime = std::stoll(val);
                            else if (subKey == "Cat") acc.tomCategory = std::stoi(val);
                            else if (subKey == "Item") acc.tomItemName = val;
                        }
                    }
                }
               else if (key.rfind("Acc_", 0) == 0) { 
                    int i, j;
                    char subKeyBuf[64];
                    if (sscanf(key.c_str(), "Acc_%d_%d_%s", &i, &j, subKeyBuf) == 3) {
                        if (i >= 0 && i < 6 && j >= 0 && j < MAX_ACCOUNT_SLOTS) {
                            AccountSlot& acc = g_Bots[i].accounts[j];
                            std::string subKey(subKeyBuf);
                            try {
                                if (subKey == "CustomName") acc.name = val;
                                else if (subKey == "Note") acc.note = DecodeConfigText(val);
                                else if (subKey == "FarmConfig") acc.farmConfigName = DecodeConfigText(val);
                                else if (subKey == "Lvl") acc.level = std::stoi(val);
                                else if (subKey == "Tag") acc.playerTag = val;
                                else if (subKey == "Name") acc.farmName = val;
                                else if (subKey == "Coin") acc.coinAmount = std::stoi(val);
                                else if (subKey == "Dia") acc.diamondAmount = std::stoi(val);
                                else if (subKey == "SellCycles") acc.targetCyclesBeforeSale = std::max(0, std::stoi(val));
                                else if (subKey == "SellCounter") acc.currentCyclesWithoutSale = std::max(0, std::stoi(val));
                                else if (subKey == "SellMax") acc.sellAtMaxPrice = (val == "1");
                                else if (subKey == "SaleStackMode") {
                                    int mode = std::stoi(val);
                                    acc.saleStackMode = (std::max)(0, (std::min)(3, mode));
                                }
                                else if (subKey == "FixedStack") acc.fixedSaleStack = (std::max)(1, (std::min)(10, std::stoi(val)));
                                else if (subKey == "KeepWheat") acc.keepWheatReserve = (std::max)(0, std::stoi(val));
                                else if (subKey == "EmergencyWheatLimit") acc.emergencyWheatStackLimit = (std::max)(0, (std::min)(20, std::stoi(val)));
                                else if (subKey == "AutoCowFeed") acc.autoCowFeedEnabled = (val == "1");
                                else if (subKey == "CowFeedFourSlots") acc.cowFeedUseFourSlots = (val == "1");
                                else if (subKey == "CowFeedRecoveryRequested") acc.cowFeedCropRecoveryRequested = (val == "1");
                                else if (subKey == "CowFeedRecoveryPlanted") acc.cowFeedCropRecoveryPlanted = (val == "1");
                                else if (subKey == "AutoDairy") acc.autoDairyEnabled = (val == "1");
                                else if (subKey == "DairyProduct") acc.dairyProductMode = (std::max)(0, (std::min)(2, std::stoi(val)));
                                else if (subKey == "DairyQueue") acc.dairyQueueCount = (std::max)(1, (std::min)(4, std::stoi(val)));
                                else if (subKey == "Friend") acc.isFriendWithStorage = (val == "1");
                            }
                            catch (...) {} 
                        }
                    }
                }
            }
        }
        in.close();
        EnsureFarmConfigs();
        EnsureAccountFarmConfigAssignments();
        for (int i = 0; i < 6; ++i) {
            if (g_Bots[i].useMultiMode) g_Bots[i].useSingleMode = false;
            if (!g_Bots[i].useMultiMode && !g_Bots[i].useSingleMode) g_Bots[i].useSingleMode = true;
        }
        memset(g_MultiModeSkipBufSyncNeeded, 1, sizeof(g_MultiModeSkipBufSyncNeeded));
    }
}

// Saves barn info to the config file. so it can be displayed or be viewed in GUI
void SaveInventoryData() {
    std::string path = GetAppDataPath() + "\\nxrth_inventory.ini";
    std::ofstream out(path);
    if (out.is_open()) {
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < MAX_ACCOUNT_SLOTS; j++) {
                InventoryData& inv = g_Bots[i].accounts[j].currentInv;
                out << i << "," << j << "," << inv.bolt << "," << inv.tape << "," << inv.plank << ","
                    << inv.nail << "," << inv.screw << "," << inv.panel << ","
                    << inv.deed << "," << inv.mallet << "," << inv.marker << "," << inv.map << ","
                    << inv.dynamite << "," << inv.tnt << "," << inv.axe << "," << inv.saw << "," << inv.shovel << "\n";
            }
        }
        out.close();
    }
}
// Loads barn info from the config file.
void LoadInventoryData() {
    std::string path = GetAppDataPath() + "\\nxrth_inventory.ini";
    std::ifstream in(path);
    if (in.is_open()) {
        std::string line;
        while (std::getline(in, line)) {
            std::stringstream ss(line);
            std::string item;
            std::vector<int> vals;
            while (std::getline(ss, item, ',')) {
                try { vals.push_back(std::stoi(item)); }
                catch (...) { vals.push_back(0); }
            }
            if (vals.size() == 12 || vals.size() == 17) {
                int i = vals[0]; int j = vals[1];
                if (i >= 0 && i < 6 && j >= 0 && j < MAX_ACCOUNT_SLOTS) {
                    InventoryData& inv = g_Bots[i].accounts[j].currentInv;
                    inv.bolt = vals[2]; inv.tape = vals[3]; inv.plank = vals[4];
                    inv.nail = vals[5]; inv.screw = vals[6]; inv.panel = vals[7];
                    inv.deed = vals[8]; inv.mallet = vals[9]; inv.marker = vals[10]; inv.map = vals[11];
                    if (vals.size() >= 17) {
                        inv.dynamite = vals[12];
                        inv.tnt = vals[13];
                        inv.axe = vals[14];
                        inv.saw = vals[15];
                        inv.shovel = vals[16];
                    }
                    else {
                        inv.dynamite = 0;
                        inv.tnt = 0;
                        inv.axe = 0;
                        inv.saw = 0;
                        inv.shovel = 0;
                    }
                    
                    g_Bots[i].accounts[j].previousInv = inv;
                }
            }
        }
        in.close();
    }
}

std::mutex g_VisionMutex;
cv::Mat g_VisionMat; // 
GLuint g_VisionTexture = 0; // 
int g_VisionTexWidth = 0;
int g_VisionTexHeight = 0;
std::atomic<bool> g_VisionLiveRunning = false;
int g_VisionSelectedInst = 0;
// Crash logger

static std::string BuildCrashTimestamp(const char* format) {
    time_t now = time(nullptr);
    struct tm localTm{};
    localtime_s(&localTm, &now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), format, &localTm);
    return std::string(buffer);
}

static std::string GetModulePathForAddress(void* address, unsigned long long* moduleBaseOut = nullptr) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(address, &mbi, sizeof(mbi)) != 0 && mbi.AllocationBase != nullptr) {
        if (moduleBaseOut) {
            *moduleBaseOut = reinterpret_cast<unsigned long long>(mbi.AllocationBase);
        }
        char modulePath[MAX_PATH] = {};
        if (GetModuleFileNameA(static_cast<HMODULE>(mbi.AllocationBase), modulePath, MAX_PATH) > 0) {
            return std::string(modulePath);
        }
    }
    if (moduleBaseOut) {
        *moduleBaseOut = 0;
    }
    return "UNKNOWN";
}

static void AppendRecentLogsForCrash(std::ofstream& out) {
    if (!g_LogMutex.try_lock()) {
        out << "[RECENT LOGS]: unavailable because the log mutex was busy during the crash.\n";
        return;
    }

    const size_t logCount = g_GlobalLogs.size();
    if (logCount == 0) {
        out << "[RECENT LOGS]: none captured before the crash.\n";
        g_LogMutex.unlock();
        return;
    }

    const size_t maxEntries = 40;
    const size_t startIndex = (logCount > maxEntries) ? (logCount - maxEntries) : 0;
    out << "[RECENT LOGS]:\n";
    for (size_t idx = startIndex; idx < logCount; ++idx) {
        const LogEntry& log = g_GlobalLogs[idx];
        out << "  [" << log.timestamp << "] ";
        if (log.instanceId >= 0) {
            out << "[I" << (log.instanceId + 1) << "] ";
        }
        if (!log.category.empty()) {
            out << "[" << log.category << "] ";
        }
        out << log.message;
        if (log.repeatCount > 1) {
            out << " (x" << log.repeatCount << ")";
        }
        out << "\n";
    }

    g_LogMutex.unlock();
}

LONG WINAPI NxrthCrashHandler(EXCEPTION_POINTERS* ExceptionInfo) {
    std::string appDataPath = GetAppDataPath();
    std::string logPath = appDataPath + "\\crash_log.txt";
    std::string dumpPath = appDataPath + "\\crash_dump_" + BuildCrashTimestamp("%Y%m%d_%H%M%S") + ".dmp";
    const unsigned long long excAddress = reinterpret_cast<unsigned long long>(ExceptionInfo->ExceptionRecord->ExceptionAddress);
    unsigned long long moduleBase = 0;
    std::string modulePath = GetModulePathForAddress(ExceptionInfo->ExceptionRecord->ExceptionAddress, &moduleBase);
    bool dumpWritten = false;

    HANDLE dumpFile = CreateFileA(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dumpFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dumpInfo{};
        dumpInfo.ThreadId = GetCurrentThreadId();
        dumpInfo.ExceptionPointers = ExceptionInfo;
        dumpInfo.ClientPointers = FALSE;
        dumpWritten = MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            dumpFile,
            static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithThreadInfo),
            &dumpInfo,
            nullptr,
            nullptr) == TRUE;
        CloseHandle(dumpFile);
    }

    std::ofstream out(logPath, std::ios::app); 
    if (out.is_open()) {
        out << "\n=========================================\n";
        out << "[CRASH TIME] : " << BuildCrashTimestamp("%Y-%m-%d %H:%M:%S") << "\n";
        out << "[FATAL ERROR]: NXRTH encountered a critical exception!\n";
        out << "[EXC CODE]   : 0x" << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode << "\n";
        out << "[EXC ADDRESS]: 0x" << excAddress << "\n";
        out << "[EXC MODULE] : " << modulePath << "\n";
        if (moduleBase != 0 && excAddress >= moduleBase) {
            out << "[EXC OFFSET] : 0x" << (excAddress - moduleBase) << "\n";
        }
        out << std::dec;
        out << "[THREAD ID]  : " << GetCurrentThreadId() << "\n";
        out << "[MINIDUMP]   : " << (dumpWritten ? dumpPath : "FAILED TO WRITE") << "\n";
        AppendRecentLogsForCrash(out);
        out << "=========================================\n";
        out.close();
    }
	// Close the program after logging the crash details
    return EXCEPTION_EXECUTE_HANDLER;
}

// Bridge to convert openCV Mat to OpenGL Texture.
void UpdateVisionTexture(const cv::Mat& mat) {
    if (mat.empty()) return;
    cv::Mat rgba;
	// OPencv uses BGR, OpenGL uses RGB(A), so we need to convert the color format. 
    if (mat.channels() == 3) cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGBA);
    else if (mat.channels() == 4) cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
    else cv::cvtColor(mat, rgba, cv::COLOR_GRAY2RGBA);

    if (g_VisionTexture != 0) {
        glDeleteTextures(1, &g_VisionTexture); // delete old image, so ram usage drops.
    }
    glGenTextures(1, &g_VisionTexture);
    glBindTexture(GL_TEXTURE_2D, g_VisionTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba.cols, rgba.rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data);
    g_VisionTexWidth = rgba.cols;
    g_VisionTexHeight = rgba.rows;
}

// Bot's eye: Takes image, Draws points and grids
cv::Mat ProcessVisionFrame(int instanceId, cv::Mat rawScreen) {
    if (rawScreen.empty()) return rawScreen;
    cv::Mat drawMat = rawScreen.clone();

    struct VisionTemplateMeta {
        cv::Size size;
        cv::Mat mask;
    };

    static std::mutex s_VisionTemplateMetaMutex;
    static std::unordered_map<std::string, VisionTemplateMeta> s_VisionTemplateMetaCache;
    static const std::unordered_map<std::string, std::string> kVisionAliases = {
        {"Mailbox", "mb"},
        {"Shop", "sh"},
        {"Roadside Shop", "rs"},
        {"Barn Tab", "bt"},
        {"Silo Tab", "st"},
        {"Cross", "cx"},
        {"Market Cross", "mc"},
        {"Silo Cross", "sfc"},
        {"Plus", "pl"},
        {"Put on Sale", "ps"},
        {"Open Ad (Sale)", "oas"},
        {"Open Ad (Crate)", "oac"},
        {"Create Ad (Sale)", "cas"},
        {"Create Ad (Crate)", "cac"},
        {"Feed Mill", "fm"},
        {"Cow Feed", "cf"},
        {"Not Enough", "ne"},
        {"Feed Mill Cross", "fmc"},
        {"Cow Pasture", "cp"},
        {"Level Up", "lu"},
        {"Continue", "ct"},
        {"Sold Crate", "sc"},
        {"Empty Crate", "ec"},
        {"Wheat Seed", "ws"},
        {"Corn Seed", "cs"},
        {"Carrot Seed", "crs"},
        {"Soybean Seed", "sys"},
        {"Cane Seed", "cns"},
        {"Wheat Shop", "wsh"},
        {"Corn Shop", "csh"},
        {"Carrot Shop", "crh"},
        {"Soy Shop", "syh"},
        {"Cane Shop", "cah"},
        {"Silo Full", "sf"},
        {"Bolt", "bm"},
        {"Tape", "tm"},
        {"Plank", "pm"},
        {"Nail", "nm"},
        {"Screw", "sm"},
        {"Panel", "pn"},
        {"Deed", "dm"},
        {"Mallet", "ml"},
        {"Marker", "mk"},
        {"Map", "mp"},
        {"Dynamite", "dy"},
        {"TNT", "tnt"},
        {"Axe", "ax"},
        {"Saw", "sw"},
        {"Shovel", "sv"}
    };

    auto GetVisionAlias = [&](const std::string& label) -> std::string {
        auto it = kVisionAliases.find(label);
        if (it != kVisionAliases.end()) return it->second;

        std::string alias;
        for (char ch : label) {
            if (std::isalnum(static_cast<unsigned char>(ch))) {
                alias.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                if (alias.size() == 3) break;
            }
        }
        return alias.empty() ? "tag" : alias;
    };

    auto GetVisionTemplateMeta = [&](const std::string& tplPath) -> VisionTemplateMeta {
        std::lock_guard<std::mutex> lock(s_VisionTemplateMetaMutex);
        auto found = s_VisionTemplateMetaCache.find(tplPath);
        if (found != s_VisionTemplateMetaCache.end()) return found->second;

        VisionTemplateMeta meta;
        cv::Mat templ = cv::imread(ResolveTemplatePath(tplPath), cv::IMREAD_UNCHANGED);
        if (!templ.empty()) {
            meta.size = templ.size();
            if (templ.channels() == 4) {
                std::vector<cv::Mat> channels;
                cv::split(templ, channels);
                cv::threshold(channels[3], meta.mask, 12, 255, cv::THRESH_BINARY);
            }
            else {
                cv::Mat gray;
                if (templ.channels() == 1) gray = templ;
                else cv::cvtColor(templ, gray, cv::COLOR_BGR2GRAY);
                cv::threshold(gray, meta.mask, 2, 255, cv::THRESH_BINARY);
            }

            if (meta.mask.empty() || cv::countNonZero(meta.mask) == 0) {
                meta.mask = cv::Mat(meta.size, CV_8UC1, cv::Scalar(255));
            }
        }

        s_VisionTemplateMetaCache[tplPath] = meta;
        return meta;
    };

    auto BuildVisionTemplateRect = [&](const MatchResult& res, const std::string& tplPath) -> cv::Rect {
        VisionTemplateMeta meta = GetVisionTemplateMeta(tplPath);
        int width = meta.size.width > 0 ? meta.size.width : 40;
        int height = meta.size.height > 0 ? meta.size.height : 40;
        cv::Rect rect(res.x - width / 2, res.y - height / 2, width, height);
        return rect & cv::Rect(0, 0, drawMat.cols, drawMat.rows);
    };

    auto PaintTemplateFootprint = [&](const MatchResult& res, const std::string& tplPath, cv::Scalar color) -> cv::Rect {
        cv::Rect bounded = BuildVisionTemplateRect(res, tplPath);
        if (bounded.width <= 0 || bounded.height <= 0) return bounded;

        VisionTemplateMeta meta = GetVisionTemplateMeta(tplPath);
        if (!meta.mask.empty() && meta.size.width > 0 && meta.size.height > 0) {
            cv::Rect fullRect(res.x - meta.size.width / 2, res.y - meta.size.height / 2, meta.size.width, meta.size.height);
            cv::Rect maskRect(bounded.x - fullRect.x, bounded.y - fullRect.y, bounded.width, bounded.height);
            if (maskRect.width > 0 && maskRect.height > 0) {
                cv::Mat roi = drawMat(bounded);
                roi.setTo(cv::Scalar(0, 0, 0), meta.mask(maskRect));
            }
        }
        else {
            cv::rectangle(drawMat, bounded, cv::Scalar(0, 0, 0), cv::FILLED);
        }

        cv::rectangle(drawMat, bounded, color, 1);
        return bounded;
    };

    auto DrawAliasChip = [&](const cv::Point& preferredAnchor, cv::Scalar color, const std::string& label) {
        std::string alias = GetVisionAlias(label);
        int baseline = 0;
        double fontScale = 0.5;
        int thickness = 1;
        cv::Size textSize = cv::getTextSize(alias, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);
        int textX = (std::max)(4, (std::min)(preferredAnchor.x, drawMat.cols - textSize.width - 10));
        int textY = (std::max)(textSize.height + 8, (std::min)(preferredAnchor.y, drawMat.rows - 4));
        cv::Rect chip(textX - 4, textY - textSize.height - 5, textSize.width + 8, textSize.height + baseline + 7);
        chip &= cv::Rect(0, 0, drawMat.cols, drawMat.rows);
        if (chip.width > 0 && chip.height > 0) {
            cv::rectangle(drawMat, chip, cv::Scalar(0, 0, 0), cv::FILLED);
            cv::rectangle(drawMat, chip, color, 1);
        }
        cv::putText(drawMat, alias, cv::Point(textX, textY), cv::FONT_HERSHEY_SIMPLEX, fontScale, color, thickness, cv::LINE_AA);
    };

    auto DrawResolvedTarget = [&](const MatchResult& res, const std::string& tplPath, cv::Scalar color, const std::string& label) {
        if (!res.found) return;
        cv::Rect matchRect = PaintTemplateFootprint(res, tplPath, color);
        cv::circle(drawMat, cv::Point(res.x, res.y), 5, color, -1);
        cv::circle(drawMat, cv::Point(res.x, res.y), 8, cv::Scalar(255, 255, 255), 1);
        cv::Point chipAnchor(
            matchRect.width > 0 ? matchRect.x + matchRect.width + 8 : res.x + 14,
            matchRect.height > 0 ? matchRect.y + (std::max)(18, matchRect.height / 2) : res.y + 6
        );
        DrawAliasChip(chipAnchor, color, label);
    };

    // 1. SINGLE TARGETS (static Stuff that needs to be found once - Example: Shop, Sickle)
    auto DrawSingleTarget = [&](const std::string& tplPath, float thresh, cv::Scalar color, const std::string& label) {
        // we use useMargins = false so bot can scan EVERYTHING on the screen, not just the center.
        MatchResult res = FindImage(rawScreen, tplPath, thresh, false, 1.0f, false);
        DrawResolvedTarget(res, tplPath, color, label);
        };

    auto BuildSaleDialogRect = [&](const cv::Mat& screen) -> cv::Rect {
        int left = static_cast<int>(screen.cols * 0.28f);
        int top = static_cast<int>(screen.rows * 0.14f);
        int right = static_cast<int>(screen.cols * 0.77f);
        int bottom = static_cast<int>(screen.rows * 0.90f);
        left = (std::max)(0, left);
        top = (std::max)(0, top);
        right = (std::min)(screen.cols, right);
        bottom = (std::min)(screen.rows, bottom);
        int width = right - left;
        int height = bottom - top;
        if (width <= 0 || height <= 0) return cv::Rect();
        return cv::Rect(left, top, width, height);
    };

    auto FindTemplateInRect = [&](const cv::Rect& rect, const std::string& tplPath, float thresh, bool useGray) -> MatchResult {
        MatchResult res{ false, 0, 0, 0.0 };
        if (rect.width <= 0 || rect.height <= 0) return res;
        cv::Rect bounded = rect & cv::Rect(0, 0, rawScreen.cols, rawScreen.rows);
        if (bounded.width <= 0 || bounded.height <= 0) return res;
        cv::Mat roi = rawScreen(bounded);
        res = FindImage(roi, tplPath, thresh, useGray, 1.0f, false);
        if (res.found) {
            res.x += bounded.x;
            res.y += bounded.y;
        }
        return res;
    };

    auto FindDialogUiTarget = [&](const std::string& tplPath, float thresh) -> MatchResult {
        MatchResult best{ false, 0, 0, 0.0 };
        auto consider = [&](const MatchResult& candidate) {
            if (candidate.found && (!best.found || candidate.score > best.score)) best = candidate;
        };
        cv::Rect dialogRect = BuildSaleDialogRect(rawScreen);
        float relaxedThreshold = (std::max)(0.58f, thresh - 0.08f);
        consider(FindTemplateInRect(dialogRect, tplPath, thresh, true));
        consider(FindTemplateInRect(dialogRect, tplPath, relaxedThreshold, false));
        consider(FindImage(rawScreen, tplPath, relaxedThreshold, true, 1.0f, false));
        consider(FindImage(rawScreen, tplPath, relaxedThreshold, false, 1.0f, false));
        return best;
    };

    // 2. Multi targets (Stuff that can be more than one in the frame - Example: Crates in the shop)
    auto DrawMultiTargets = [&](const std::string& tplPath, float thresh, cv::Scalar color, const std::string& label) {
        std::vector<MatchResult> results = FindAllImages(rawScreen, tplPath, thresh, 20, false);
        for (auto& res : results) {
            DrawResolvedTarget(res, tplPath, color, label);
        }
        };

    // =====================================================================
    // BOT VISION TAB STUFF
    // =====================================================================

    // --- A. Buildings / entry points ---
    DrawSingleTarget(mailbox_templatePath, g_Thresholds.mailboxThreshold, cv::Scalar(255, 0, 0), "Mailbox");
    MatchResult roadsideShopRes = FindImage(rawScreen, roadside_shop_templatePath, g_Thresholds.shopThreshold, false, 1.0f, false);
    MatchResult createSaleRes = FindImage(rawScreen, create_sale_templatePath, g_Thresholds.createSaleThreshold, false, 1.0f, false);
    MatchResult advertiseRes = FindDialogUiTarget(advertise_templatePath, g_Thresholds.advertiseThreshold);
    MatchResult occupiedAdvertiseRes = FindDialogUiTarget(occupied_crate_advertise_templatePath, g_Thresholds.advertiseThreshold);
    MatchResult createAdRes = FindDialogUiTarget(create_ad_templatePath, g_Thresholds.createAdThreshold);
    MatchResult occupiedCreateAdRes = FindDialogUiTarget(occupied_crate_create_ad_templatePath, g_Thresholds.createAdThreshold);
    bool shopInteriorVisible = roadsideShopRes.found || createSaleRes.found || advertiseRes.found || occupiedAdvertiseRes.found || createAdRes.found || occupiedCreateAdRes.found;
    if (!shopInteriorVisible) {
        DrawSingleTarget(shop_templatePath, g_Thresholds.shopThreshold, cv::Scalar(0, 255, 255), "Shop");
    }
    DrawResolvedTarget(roadsideShopRes, roadside_shop_templatePath, cv::Scalar(0, 220, 255), "Roadside Shop");
    DrawSingleTarget(barn_market_templatePath, g_Thresholds.barnTabThreshold, cv::Scalar(180, 220, 255), "Barn Tab");
    DrawSingleTarget(silo_market_templatePath, g_Thresholds.siloTabThreshold, cv::Scalar(220, 255, 180), "Silo Tab");

    // --- B. UI BUTTONS / CROSSES ---
    DrawSingleTarget(cross_templatePath, g_Thresholds.crossThreshold, cv::Scalar(0, 0, 255), "Cross");
    DrawSingleTarget(market_close_crosstemplatePath, g_Thresholds.marketCloseCrossThreshold, cv::Scalar(0, 140, 255), "Market Cross");
    DrawSingleTarget(silo_full_cross_templatePath, g_Thresholds.siloFullCrossThreshold, cv::Scalar(255, 0, 255), "Silo Cross");
    DrawSingleTarget(plus_templatePath, g_Thresholds.plusThreshold, cv::Scalar(0, 255, 0), "Plus");
    DrawResolvedTarget(createSaleRes, create_sale_templatePath, cv::Scalar(0, 255, 0), "Put on Sale");
    DrawResolvedTarget(advertiseRes, advertise_templatePath, cv::Scalar(200, 200, 200), "Open Ad (Sale)");
    DrawResolvedTarget(occupiedAdvertiseRes, occupied_crate_advertise_templatePath, cv::Scalar(210, 230, 255), "Open Ad (Crate)");
    DrawResolvedTarget(createAdRes, create_ad_templatePath, cv::Scalar(220, 220, 255), "Create Ad (Sale)");
    DrawResolvedTarget(occupiedCreateAdRes, occupied_crate_create_ad_templatePath, cv::Scalar(255, 220, 220), "Create Ad (Crate)");
    DrawSingleTarget(feed_mill_templatePath, g_Thresholds.feedMillThreshold, cv::Scalar(80, 220, 255), "Feed Mill");
    DrawSingleTarget(cow_feed_templatePath, g_Thresholds.cowFeedThreshold, cv::Scalar(255, 220, 80), "Cow Feed");
    DrawSingleTarget(dairy_templatePath, g_Thresholds.dairyThreshold, cv::Scalar(255, 120, 220), "Dairy");
    DrawSingleTarget(cream_templatePath, g_Thresholds.creamThreshold, cv::Scalar(240, 240, 210), "Cream");
    DrawSingleTarget(butter_templatePath, g_Thresholds.creamThreshold, cv::Scalar(255, 235, 120), "Butter");
    DrawSingleTarget(cheese_templatePath, g_Thresholds.creamThreshold, cv::Scalar(255, 190, 80), "Cheese");
    DrawSingleTarget(not_enough_templatePath, g_Thresholds.notEnoughThreshold, cv::Scalar(80, 80, 255), "Not Enough");
    DrawSingleTarget(feed_mill_cross_templatePath, g_Thresholds.feedMillCrossThreshold, cv::Scalar(255, 120, 120), "Feed Mill Cross");
    DrawSingleTarget(cow_pasture_templatePath, g_Thresholds.cowPastureThreshold, cv::Scalar(120, 255, 180), "Cow Pasture");
    DrawSingleTarget(levelup_templatePath, g_Thresholds.levelUpThreshold, cv::Scalar(255, 215, 0), "Level Up");
    DrawSingleTarget(levelup_continue_templatePath, g_Thresholds.levelUpThreshold, cv::Scalar(255, 200, 80), "Continue");
    DrawSingleTarget(soldcrate_templatePath, 0.80f, cv::Scalar(50, 200, 50), "Sold Crate");

    // --- C. SHOP CRATES (MULTI SCAN) ---
    DrawMultiTargets(crate_templatePath, g_Thresholds.crateThreshold, cv::Scalar(200, 150, 100), "Empty Crate");

    // --- D. SEEDS, BUT ONLY SEARCH THE SELECTED CROP MODE. USER CAN CHOOSE THIS IN BOT CONFIGURATION TAB ---
    int cropMode = g_Bots[instanceId].testCropMode;
    std::string seedTpl = w_templatePath;
    float seedThresh = g_Thresholds.wheatThreshold;
    std::string seedName = "Wheat Seed";

    if (cropMode == 1) { seedTpl = c_templatePath; seedThresh = g_Thresholds.cornThreshold; seedName = "Corn Seed"; }
    else if (cropMode == 2) { seedTpl = carrot_templatePath; seedThresh = g_Thresholds.carrotThreshold; seedName = "Carrot Seed"; }
    else if (cropMode == 3) { seedTpl = soybean_templatePath; seedThresh = g_Thresholds.soybeanThreshold; seedName = "Soybean Seed"; }
    else if (cropMode == 4) { seedTpl = sugarcane_templatePath; seedThresh = g_Thresholds.sugarcaneThreshold; seedName = "Cane Seed"; }

    DrawSingleTarget(seedTpl, seedThresh, cv::Scalar(0, 255, 0), seedName);
    DrawSingleTarget(wheatshop_templatePath, g_Thresholds.wheatShopThreshold, cv::Scalar(255, 255, 0), "Wheat Shop");
    DrawSingleTarget(cornshop_templatePath, g_Thresholds.cornShopThreshold, cv::Scalar(0, 200, 255), "Corn Shop");
    DrawSingleTarget(carrot_shop_templatePath, g_Thresholds.carrotShopThreshold, cv::Scalar(0, 128, 255), "Carrot Shop");
    DrawSingleTarget(soybean_shop_templatePath, g_Thresholds.soybeanShopThreshold, cv::Scalar(128, 255, 0), "Soy Shop");
    DrawSingleTarget(sugarcane_shop_templatePath, g_Thresholds.sugarcaneShopThreshold, cv::Scalar(150, 255, 150), "Cane Shop");
    DrawSingleTarget(silo_full_templatePath, 0.75f, cv::Scalar(255, 80, 80), "Silo Full");

    // --- Material anchors ---
    DrawSingleTarget(bolt_material_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(255, 180, 0), "Bolt");
    DrawSingleTarget(tape_material_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(255, 180, 40), "Tape");
    DrawSingleTarget(plank_material_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(200, 120, 80), "Plank");
    DrawSingleTarget(nail_material_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(180, 180, 180), "Nail");
    DrawSingleTarget(screw_material_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(180, 220, 180), "Screw");
    DrawSingleTarget(panel_material_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(220, 220, 120), "Panel");
    DrawSingleTarget(deed_material_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(150, 200, 255), "Deed");
    DrawSingleTarget(mallet_material_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(160, 120, 255), "Mallet");
    DrawSingleTarget(marker_material_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(255, 120, 180), "Marker");
    DrawSingleTarget(map_material_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(120, 255, 220), "Map");
    DrawSingleTarget(dynamite_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(80, 180, 255), "Dynamite");
    DrawSingleTarget(tnt_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(60, 120, 255), "TNT");
    DrawSingleTarget(axe_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(180, 200, 220), "Axe");
    DrawSingleTarget(saw_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(200, 200, 200), "Saw");
    DrawSingleTarget(shovel_templatePath, g_Thresholds.materialTemplateThreshold, cv::Scalar(120, 180, 200), "Shovel");

    // --- E. DYNAMIC AREA TARGETS (COLOR FILTERED) ---
    // 1. GROWN CROPS (GREEN Grid)
    std::vector<MatchResult> grownCrops = FindGrownCrops(rawScreen, cropMode);
    for (auto& g : grownCrops) {
        cv::rectangle(drawMat, cv::Point(g.x - 15, g.y - 15), cv::Point(g.x + 15, g.y + 15), cv::Scalar(0, 255, 0), 2);
        DrawAliasChip(cv::Point(g.x + 18, g.y - 4), cv::Scalar(0, 255, 0), "Crop");
    }

    // 2. EMPTY FIELDS (PINK Grid)
    std::vector<MatchResult> emptyFields = FindEmptyFields(rawScreen, false);
    for (auto& e : emptyFields) {
        cv::rectangle(drawMat, cv::Point(e.x - 15, e.y - 15), cv::Point(e.x + 15, e.y + 15), cv::Scalar(255, 0, 255), 2);
        DrawAliasChip(cv::Point(e.x + 18, e.y - 4), cv::Scalar(255, 0, 255), "Empty");
    }

    return drawMat;
}
//====================================================================
// GUI STUFF GOES HERE
//====================================================================
void RenderCustomTitleBar(GLFWwindow* window) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
    ImGui::BeginChild("TitleBar", ImVec2(ImGui::GetWindowWidth(), 30), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::SetCursorPosY(6);
    ImGui::SetCursorPosX(15);
    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), "NXRTH");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "MULTI BOT");

    if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        static POINT offset;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            int wx, wy;
            glfwGetWindowPos(window, &wx, &wy);
            offset.x = cursorPos.x - wx;
            offset.y = cursorPos.y - wy;
        }
        glfwSetWindowPos(window, cursorPos.x - offset.x, cursorPos.y - offset.y);
    }

    // =========================================================
	// EMULATOR SELECTION 
    // =========================================================
    ImGui::SameLine(ImGui::GetWindowWidth() - 250);
    ImGui::SetCursorPosY(4);
    ImGui::SetNextItemWidth(140);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    if (ImGui::Combo("##GlobalEmuMode", &g_GlobalEmulatorMode, "MEmu\0LDPlayer\0")) {
        if (g_GlobalEmulatorMode == 0) { // MEmu SELECTED
            strncpy(g_AdbPathBuf, "C:\\Program Files\\Microvirt\\MEmu\\adb.exe", 260);
            strncpy(g_MEmuPathBuf, "C:\\Program Files\\Microvirt\\MEmu\\MEmuConsole.exe", 260);

            // UPDATE PATHS IN THE BOT TOO.
            kAdbPath = "C:\\Program Files\\Microvirt\\MEmu\\adb.exe";
            kMEmuConsolePath = "C:\\Program Files\\Microvirt\\MEmu\\MEmuConsole.exe";

            for (int i = 0; i < 6; i++) {
                g_Bots[i].emulatorType = 0;
                std::string dp = "127.0.0.1:" + std::to_string(21503 + (i * 10));
                strncpy(g_Bots[i].adbSerial, dp.c_str(), 64);
                std::string vm = (i == 0) ? "MEmu" : "MEmu_" + std::to_string(i);
                strncpy(g_Bots[i].vmName, vm.c_str(), 64);
            }
        }
        else { // LDPlayer SELECTED
            strncpy(g_AdbPathBuf, "C:\\LDPlayer\\LDPlayer9\\adb.exe", 260);
            strncpy(g_MEmuPathBuf, "C:\\LDPlayer\\LDPlayer9\\ldconsole.exe", 260);

            // UPDATE PATHS IN THE BOT TOO.
            kAdbPath = "C:\\LDPlayer\\LDPlayer9\\adb.exe";
            kMEmuConsolePath = "C:\\LDPlayer\\LDPlayer9\\ldconsole.exe";

            for (int i = 0; i < 6; i++) {
                g_Bots[i].emulatorType = 1;
                g_Bots[i].emuIndex = i;
                std::string dp = "127.0.0.1:" + std::to_string(5555 + (i * 2));
                strncpy(g_Bots[i].adbSerial, dp.c_str(), 64);
                std::string vm = "LDPlayer-" + std::to_string(i);
                strncpy(g_Bots[i].vmName, vm.c_str(), 64);
            }
        }
        SaveConfig();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(Tr("Select Emulator Engine"));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    // --- MINIMIZE AND CROSS BUTTONS ---
    ImGui::SameLine(ImGui::GetWindowWidth() - 110);
    ImGui::SetCursorPosY(4);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.40f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
    if (ImGui::Button("_", ImVec2(30, 22))) {
        glfwIconifyWindow(window);
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine(ImGui::GetWindowWidth() - 75);
    ImGui::SetCursorPosY(4);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.40f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
    if (ImGui::Button("[]", ImVec2(30, 22))) {
        if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) glfwRestoreWindow(window);
        else glfwMaximizeWindow(window);
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine(ImGui::GetWindowWidth() - 40);
    ImGui::SetCursorPosY(4);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.14f, 0.14f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.16f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.42f, 0.08f, 0.08f, 1.0f));
    if (ImGui::Button("X", ImVec2(30, 22))) {
        ExitProcess(0);
    }
    ImGui::PopStyleColor(4);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}




void InitializeBots() {


    for (int i = 0; i < 6; i++) {
        g_Bots[i].id = i;
        g_Bots[i].emulatorType = g_GlobalEmulatorMode;
        g_Bots[i].emuIndex = i;

		// PORTS CHANGE BASED ON THE EMULATOR CHOICE, SO WE SET THEM ACCORDINGLY.
        if (g_GlobalEmulatorMode == 1) { // LDPlayer
            std::string defaultPort = "127.0.0.1:" + std::to_string(5555 + (i * 2));
            strcpy(g_Bots[i].adbSerial, defaultPort.c_str());
            std::string defaultVM = "LDPlayer-" + std::to_string(i);
            strcpy(g_Bots[i].vmName, defaultVM.c_str());
        }
        else { // MEmu
            std::string defaultPort = "127.0.0.1:" + std::to_string(21503 + (i * 10));
            strcpy(g_Bots[i].adbSerial, defaultPort.c_str());
            std::string defaultVM = (i == 0) ? "MEmu" : "MEmu_" + std::to_string(i);
            strcpy(g_Bots[i].vmName, defaultVM.c_str());
        }

        for (int j = 0; j < MAX_ACCOUNT_SLOTS; j++) {
            if (g_Bots[i].accounts[j].name.empty()) {
                g_Bots[i].accounts[j].name = "Account " + std::to_string(j + 1);
            }

            std::string path = GetAccountBackupPath(i, j);
            bool exists = fs::exists(path) && fs::file_size(path) > 0;
            g_Bots[i].accounts[j].hasFile = exists;
            g_Bots[i].accounts[j].fileName = exists ? path : "";
        }
    }
    g_Bots[0].isActive = true;
}

bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height) {
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL) return false;
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);
    *out_texture = image_texture;
    if (out_width) *out_width = image_width;
    if (out_height) *out_height = image_height;
    return true;
}
// TOTAL RUNTIME OF THE BOT
std::string GetRuntimeStr() {
    bool anyRunning = false;
    for (int i = 0; i < 6; i++) if (g_Bots[i].isRunning) anyRunning = true;
    if (!anyRunning) return "00:00:00";
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_BotStartTime).count();
    int h = static_cast<int>(elapsed / 3600);
    int m = static_cast<int>((elapsed % 3600) / 60);
    int s = static_cast<int>(elapsed % 60);
    char buf[32];
    sprintf(buf, "%02d:%02d:%02d", h, m, s);
    return std::string(buf);
}
// GUI THEME: CLASSIC LIGHT TAB UI
void SetPremiumTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsLight();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 5.0f;
    style.ScrollbarRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.ItemSpacing = ImVec2(9.0f, 7.0f);
    style.WindowPadding = ImVec2(12.0f, 10.0f);
    style.FramePadding = ImVec2(9.0f, 5.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.955f, 0.960f, 0.965f, 1.00f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.995f, 0.995f, 0.990f, 1.00f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.72f, 0.75f, 0.78f, 1.00f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.20f, 0.23f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.23f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.88f, 0.90f, 0.91f, 1.00f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.82f, 0.88f, 0.94f, 1.00f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.995f, 0.995f, 0.990f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.87f, 0.89f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.79f, 0.86f, 0.93f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.62f, 0.75f, 0.88f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.82f, 0.88f, 0.94f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.75f, 0.84f, 0.92f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.62f, 0.76f, 0.89f, 1.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 0.995f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.94f, 0.97f, 0.99f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.90f, 0.95f, 0.99f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.12f, 0.42f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.25f, 0.55f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.10f, 0.42f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.39f, 0.42f, 0.45f, 1.00f);
}

void RenderLogo() {
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextColored(ImVec4(0.5f, 0.0f, 0.8f, 1.0f), "NX");
    ImGui::SameLine(0, 0);
    ImGui::TextColored(ImVec4(0.7f, 0.3f, 1.0f, 1.0f), "RTH");

    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}
// BUTTONS ON THE GUI, WITH GOLDEN HIGHLIGHT WHEN SELECTED. ALSO HANDLES THE SELECTION LOGIC.
void ModernButton(const char* label, GLuint icon, int id, int& current) {
    bool isSelected = (current == id);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float btn_width = 250.0f;
    float btn_height = 50.0f;

    ImU32 bg_col = isSelected ? IM_COL32(217, 165, 30, 50) : IM_COL32(40, 40, 40, 255);
    if (ImGui::IsMouseHoveringRect(p, ImVec2(p.x + btn_width, p.y + btn_height))) {
        bg_col = IM_COL32(217, 165, 30, 80);
        if (ImGui::IsMouseClicked(0)) current = id;
    }
    draw_list->AddRectFilled(p, ImVec2(p.x + btn_width, p.y + btn_height), bg_col, 8.0f);
    if (icon != 0) draw_list->AddImage((void*)(intptr_t)icon, ImVec2(p.x + 15, p.y + 10), ImVec2(p.x + 45, p.y + 40));
    ImU32 text_col = isSelected ? IM_COL32(217, 165, 30, 255) : IM_COL32(230, 230, 230, 255);
    draw_list->AddText(ImVec2(p.x + 60, p.y + 15), text_col, label);
    ImGui::Dummy(ImVec2(btn_width, btn_height));
    ImGui::Spacing();
}

// STARTS EMULATOR AND THE GAME! FUNCTION NAME IS MEMUANDGAME BUT IT ALSO WORKS WITH LDPLAYER BECAUSE I AM TOO LAZY TO CHANGE IT.
void StartMEmuAndGame(int instanceId) {
    AddLog(instanceId, "Starting Emulator Environment...", ImVec4(0, 1, 1, 1));

    std::thread([instanceId]() {
        BotInstance& bot = g_Bots[instanceId];

		// 1. KILL ADB SERVER TO PREVENT ANY CONNECTION ISSUES.
        RunCmdHidden("cmd.exe /c \"\"" + kAdbPath + "\" kill-server\"");

        // 2. STARTS EMULATOR
        std::string cmd;
        if (bot.emulatorType == 1) { // LDPlayer
            cmd = "cmd.exe /c \"\"" + kMEmuConsolePath + "\" launch --index " + std::to_string(bot.emuIndex) + "\"";
        }
        else { // MEmu
            cmd = "cmd.exe /c \"\"" + kMEmuConsolePath + "\" " + std::string(bot.vmName) + "\"";
        }
        RunCmdHidden(cmd);

        AddLog(instanceId, "Waiting for Android to boot (Takes 15-30s)...", ImVec4(1, 1, 0, 1));

        // 3. WAIT FOR THE ANDROID TO LOAD AND CONNCET ADB!
        // WARNING: THIS IS VERY IMPORTANT FOR LDPLAYER BECAUSE LDPLAYER NOT WORKING LIKE MEMU, YOU HAVE TO CONNECT ADB AND STAY CONNECTED. IN MEMU YOU JUST SEND COMMANDS BUT THIS IS NOT LIKE THAT
        // SO PLEASE WATCH OUT FOR THIS PART BECAUSE ITS THE MOST IMPORTANT IN MY OPINION.
        bool booted = false;
		for (int i = 0; i < 40; i++) { // MAX 80 SECONDS WAIT (40*2=80)
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // FORCE CONNECTION
            RunCmdHidden("cmd.exe /c \"\"" + kAdbPath + "\" connect " + std::string(bot.adbSerial) + "\"");

            // CHECK BOOT STATUS
            std::string tempFile = "C:\\Users\\Public\\boot_check_" + std::to_string(instanceId) + ".txt";
            remove(tempFile.c_str());
            std::string checkCmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + std::string(bot.adbSerial) + " shell getprop sys.boot_completed > \"" + tempFile + "\"\"";
            RunCmdHidden(checkCmd);

            std::ifstream file(tempFile);
            std::string result;
            if (file.is_open()) {
                std::getline(file, result);
                file.close();
            }

            if (result.find("1") != std::string::npos) {
                booted = true;
                break;
            }
        }

        if (!booted) {
            AddLog(instanceId, "ERROR: Android boot timeout! ADB offline.", ImVec4(1, 0, 0, 1));
            return;
        }

        AddLog(instanceId, "Android is ready. Launching Hay Day...", ImVec4(0, 1, 0, 1));
        std::this_thread::sleep_for(std::chrono::seconds(3)); // WAIT 3 SECS FOR ANDROID HOMEPAGE LOAD.

        // 4. START THE GAME
        std::string launchCmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + std::string(bot.adbSerial) + " shell monkey -p com.supercell.hayday -c android.intent.category.LAUNCHER 1\"";
        RunCmdHidden(launchCmd);

        AddLog(instanceId, "App Launched successfully.", ImVec4(0, 1, 0, 1));
        }).detach();
}




void RenderTemplateRow(const char* label, char* buffer, size_t bufferSize, std::string& targetPath, float* threshold = nullptr) {
    ImGui::PushID(label);
    float availWidth = ImGui::GetContentRegionAvail().x;
    float loadButtonWidth = 80.0f;
    float thresholdWidth = threshold ? 150.0f : 0.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float inputWidth = availWidth - loadButtonWidth - thresholdWidth - spacing * (threshold ? 2 : 1);

    ImGui::TextColored(ImVec4(0.10f, 0.24f, 0.42f, 1.0f), "%s", label);
    ImGui::SetNextItemWidth(inputWidth);
    if (ImGui::InputText("##path", buffer, bufferSize)) {
        targetPath = buffer;
    }
    ImGui::SameLine();
    if (ImGui::Button(Tr("Load"), ImVec2(loadButtonWidth, 0))) {
        std::string filePath = OpenFileDialog("PNG Files\0*.png\0All Files\0*.*\0");
        if (!filePath.empty()) {
            strncpy(buffer, filePath.c_str(), bufferSize - 1);
            buffer[bufferSize - 1] = '\0';
            targetPath = filePath;
            AddLog(0, std::string(Tr("Loaded template: ")) + filePath, ImVec4(0, 1, 0, 1));
        }
    }
    if (threshold) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(thresholdWidth);
        ImGui::SliderFloat("##threshold", threshold, 0.1f, 0.99f, "%.2f");
    }
    ImGui::PopID();
}

static int g_ClassicSelectedInstance = 0;
static int g_ClassicSelectedSlot = 0;

enum ClassicPageId {
    ClassicPage_Start = 0,
    ClassicPage_Accounts,
    ClassicPage_Farm,
    ClassicPage_Templates,
    ClassicPage_Logs,
    ClassicPage_Settings
};

static int g_ClassicPage = ClassicPage_Start;

static void ClassicBeginPanel(const char* title, ImVec2 size = ImVec2(0, 0)) {
    ImGui::BeginChild(title, size, true);
    ImGui::TextColored(ImVec4(0.12f, 0.24f, 0.34f, 1.0f), "%s", title);
    ImGui::Separator();
    ImGui::Spacing();
}

static bool ClassicButton(const char* label, const ImVec4& color, ImVec2 size = ImVec2(-1, 28)) {
    float luminance = (0.2126f * color.x) + (0.7152f * color.y) + (0.0722f * color.z);
    ImVec4 textColor = luminance < 0.58f ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4((std::min)(color.x + 0.08f, 1.0f), (std::min)(color.y + 0.08f, 1.0f), (std::min)(color.z + 0.08f, 1.0f), 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4((std::max)(color.x - 0.08f, 0.0f), (std::max)(color.y - 0.08f, 0.0f), (std::max)(color.z - 0.08f, 0.0f), 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return clicked;
}

static const char* ClassicPageTitle(int page) {
    switch (page) {
    case ClassicPage_Start: return "Start";
    case ClassicPage_Accounts: return "Accounts";
    case ClassicPage_Farm: return "Farm";
    case ClassicPage_Templates: return "Templates";
    case ClassicPage_Logs: return "Logs";
    case ClassicPage_Settings: return "Settings";
    default: return "Start";
    }
}

static std::string ClassicShortText(const std::string& text, size_t maxLen) {
    if (text.length() <= maxLen) return text;
    if (maxLen <= 2) return text.substr(0, maxLen);
    return text.substr(0, maxLen - 2) + "..";
}

static std::string ClassicAccountDisplayName(const AccountSlot& acc) {
    if (acc.hasFile && !acc.farmName.empty()) return acc.farmName;
    if (acc.hasFile && !acc.name.empty()) return acc.name;
    if (acc.hasFile) return "Saved account";
    return "Empty slot";
}

static int ClassicSavedAccountCount(const BotInstance& bot) {
    int count = 0;
    for (int i = 0; i < MAX_ACCOUNT_SLOTS; ++i) {
        if (bot.accounts[i].hasFile) ++count;
    }
    return count;
}

static int ClassicRunnableAccountCount(const BotInstance& bot) {
    auto skipMask = ParseMultiModeSkipMask(bot.multiModeSkipSlots);
    int count = 0;
    for (int i = 0; i < MAX_ACCOUNT_SLOTS; ++i) {
        if (bot.accounts[i].hasFile && !IsMultiModeSlotSkipped(skipMask, i)) ++count;
    }
    return count;
}

static ImVec4 ClassicBotStatusColor(const BotInstance& bot) {
    if (!bot.isActive) return ImVec4(0.47f, 0.49f, 0.51f, 1.0f);
    if (!bot.isRunning) return ImVec4(0.28f, 0.50f, 0.68f, 1.0f);
    if (bot.statusText.find("ERROR") != std::string::npos || bot.statusText.find("FAIL") != std::string::npos) return ImVec4(0.72f, 0.12f, 0.12f, 1.0f);
    if (bot.statusText.find("WAIT") != std::string::npos || bot.statusText.find("LOAD") != std::string::npos) return ImVec4(0.78f, 0.48f, 0.08f, 1.0f);
    return ImVec4(0.10f, 0.55f, 0.24f, 1.0f);
}

static std::string ClassicBotStatusLabel(const BotInstance& bot) {
    if (!bot.isActive) return "Disabled";
    if (!bot.isRunning) return "Idle";
    if (!bot.statusText.empty()) return ClassicShortText(bot.statusText, 24);
    return "Running";
}

static void ClassicStatusPill(const char* text, const ImVec4& bg, const ImVec4& fg = ImVec4(1, 1, 1, 1)) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImVec2 pad = ImVec2(8.0f, 4.0f);
    ImVec2 size = ImVec2(textSize.x + pad.x * 2.0f, textSize.y + pad.y * 2.0f);
    drawList->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), ImGui::GetColorU32(bg), 5.0f);
    drawList->AddText(ImVec2(p.x + pad.x, p.y + pad.y), ImGui::GetColorU32(fg), text);
    ImGui::Dummy(size);
}

static void ClassicHealthRow(const char* label, bool ok, const char* detail = nullptr) {
    ClassicStatusPill(ok ? "OK" : "FIX", ok ? ImVec4(0.10f, 0.55f, 0.24f, 1.0f) : ImVec4(0.78f, 0.48f, 0.08f, 1.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    if (detail && detail[0] != '\0') {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", detail);
    }
}

static void ClassicNavItem(const char* label, int page) {
    bool selected = (g_ClassicPage == page);
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.62f, 0.76f, 0.89f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.62f, 0.76f, 0.89f, 1.0f));
    }
    if (ImGui::Selectable(label, selected, 0, ImVec2(0, 32))) {
        g_ClassicPage = page;
    }
    if (selected) ImGui::PopStyleColor(2);
}

static void ClassicInstanceCombo(const char* label) {
    const char* items[] = { "Emu 1", "Emu 2", "Emu 3", "Emu 4", "Emu 5", "Emu 6" };
    g_ClassicSelectedInstance = (std::max)(0, (std::min)(5, g_ClassicSelectedInstance));
    ImGui::Combo(label, &g_ClassicSelectedInstance, items, IM_ARRAYSIZE(items));
    g_SelectedInstanceUI = g_ClassicSelectedInstance;
}

static bool ClassicFarmConfigCombo(const char* label, std::string& selectedName) {
    EnsureFarmConfigs();
    int currentIdx = FindFarmConfigIndex(selectedName);
    bool changed = false;
    const char* preview = g_FarmConfigs[currentIdx].name.c_str();
    if (ImGui::BeginCombo(label, preview)) {
        for (size_t i = 0; i < g_FarmConfigs.size(); ++i) {
            bool selected = (static_cast<int>(i) == currentIdx);
            if (ImGui::Selectable(g_FarmConfigs[i].name.c_str(), selected)) {
                selectedName = g_FarmConfigs[i].name;
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

static std::string BuildFarmConfigSlotList(int instanceId, const std::string& configName) {
    if (instanceId < 0 || instanceId >= 6) return "";
    std::string result;
    for (int slot = 0; slot < MAX_ACCOUNT_SLOTS; ++slot) {
        if (g_Bots[instanceId].accounts[slot].farmConfigName != configName) continue;
        if (!result.empty()) result += ",";
        result += std::to_string(slot + 1);
    }
    return result;
}

static void ApplyFarmConfigSlotList(int instanceId, const std::string& configName, const std::string& spec) {
    if (instanceId < 0 || instanceId >= 6 || configName.empty()) return;
    std::array<bool, MAX_ACCOUNT_SLOTS> selected{};
    std::stringstream ss(spec);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = NormalizeLogMessage(token);
        if (token.empty()) continue;
        size_t dash = token.find('-');
        try {
            if (dash == std::string::npos) {
                int slot = std::stoi(token);
                if (slot >= 1 && slot <= 15) selected[slot - 1] = true;
            }
            else {
                int start = std::stoi(token.substr(0, dash));
                int end = std::stoi(token.substr(dash + 1));
                if (start > end) std::swap(start, end);
                start = (std::max)(1, (std::min)(15, start));
                end = (std::max)(1, (std::min)(15, end));
                for (int slot = start; slot <= end; ++slot) selected[slot - 1] = true;
            }
        }
        catch (...) {}
    }

    for (int slot = 0; slot < MAX_ACCOUNT_SLOTS; ++slot) {
        if (selected[slot]) {
            g_Bots[instanceId].accounts[slot].farmConfigName = configName;
            ApplyFarmConfigForSlot(instanceId, slot);
        }
        else if (g_Bots[instanceId].accounts[slot].farmConfigName == configName && !g_FarmConfigs.empty()) {
            g_Bots[instanceId].accounts[slot].farmConfigName = g_FarmConfigs[0].name;
            ApplyFarmConfigForSlot(instanceId, slot);
        }
    }
}

static ImVec4 ClassicReadableLogColor(ImVec4 color) {
    if (color.x > 0.8f && color.y > 0.8f && color.z < 0.25f) return ImVec4(0.54f, 0.42f, 0.00f, 1.0f);
    if (color.x < 0.25f && color.y > 0.8f && color.z > 0.8f) return ImVec4(0.00f, 0.42f, 0.60f, 1.0f);
    if (color.x < 0.25f && color.y > 0.75f && color.z < 0.25f) return ImVec4(0.00f, 0.48f, 0.10f, 1.0f);
    if (color.x > 0.8f && color.y < 0.35f && color.z < 0.35f) return ImVec4(0.72f, 0.08f, 0.08f, 1.0f);
    if (color.x > 0.7f && color.y > 0.35f && color.y < 0.75f && color.z < 0.35f) return ImVec4(0.78f, 0.34f, 0.00f, 1.0f);
    float luminance = (0.2126f * color.x) + (0.7152f * color.y) + (0.0722f * color.z);
    if (luminance > 0.72f) return ImVec4(color.x * 0.55f, color.y * 0.55f, color.z * 0.55f, 1.0f);
    return color;
}

static bool ClassicLogMatches(const LogEntry& log) {
    bool matchesInstance = (g_LogFilter == -1) ||
        (g_LogFilter == -2 && log.instanceId == -1) ||
        (g_LogFilter >= 0 && g_LogFilter == log.instanceId);
    if (!matchesInstance) return false;

    std::string prefix = (log.instanceId == -1) ? "[SYSTEM]" : ("[INST " + std::to_string(log.instanceId + 1) + "]");
    std::string searchable = log.timestamp + " " + prefix + " [" + log.category + "] " + log.message;
    return ContainsCaseInsensitive(searchable, g_LogSearchBuf);
}

static void ClassicClearLogs() {
    std::lock_guard<std::mutex> lock(g_LogMutex);
    std::vector<LogEntry>().swap(g_GlobalLogs);
}

static void ClassicDrawLogFilterCombo(float width = 130.0f) {
    const char* filterItems[] = { "All", "System", "Emu 1", "Emu 2", "Emu 3", "Emu 4", "Emu 5", "Emu 6" };
    int filterIdx = 0;
    if (g_LogFilter == -2) filterIdx = 1;
    else if (g_LogFilter >= 0) filterIdx = g_LogFilter + 2;
    filterIdx = (std::max)(0, (std::min)(static_cast<int>(IM_ARRAYSIZE(filterItems)) - 1, filterIdx));

    ImGui::SetNextItemWidth(width);
    if (ImGui::Combo("##classiclogfilter", &filterIdx, filterItems, IM_ARRAYSIZE(filterItems))) {
        g_LogFilter = (filterIdx == 0) ? -1 : (filterIdx == 1 ? -2 : filterIdx - 2);
    }
}

static void ClassicDrawLogs(float height = 0.0f) {
    ImGui::BeginChild("ClassicLogsList", ImVec2(0, height), true, ImGuiWindowFlags_HorizontalScrollbar);
    bool shouldStickToBottom = g_AutoScrollLogs && (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f);
    std::lock_guard<std::mutex> lock(g_LogMutex);
    for (const auto& log : g_GlobalLogs) {
        if (!ClassicLogMatches(log)) continue;
        std::string prefix = (log.instanceId == -1) ? "[SYSTEM]" : ("[INST " + std::to_string(log.instanceId + 1) + "]");
        std::string repeatSuffix;
        if (log.repeatCount > 1) repeatSuffix = " [x" + std::to_string(log.repeatCount) + "]";
        ImGui::TextColored(ClassicReadableLogColor(log.color), "[%s] %s [%s] %s%s", log.timestamp.c_str(), prefix.c_str(), log.category.c_str(), log.message.c_str(), repeatSuffix.c_str());
    }
    if (shouldStickToBottom) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

static void ClassicStartStopBot(int instanceId) {
    BotInstance& bot = g_Bots[instanceId];
    if (bot.isRunning) {
        bot.isRunning = false;
        bot.statusText = "STOPPED";
        AddLog(instanceId, "BOT Stopped by User.", ImVec4(1, 0.5f, 0.5f, 1));
    }
    else {
        bot.isRunning = true;
        bot.statusText = "STARTING...";
        AddLog(instanceId, "BOT Started.", ImVec4(0, 1, 0, 1));
        std::thread([instanceId]() { RunPremiumBot(instanceId); }).detach();
    }
}

static void ClassicDrawTopStatus() {
    BotInstance& bot = g_Bots[g_ClassicSelectedInstance];
    int slot = (std::max)(0, (std::min)(MAX_ACCOUNT_SLOTS - 1, bot.currentAccountIndex));
    AccountSlot& acc = bot.accounts[slot];
    std::string status = ClassicBotStatusLabel(bot);
    std::string accountName = ClassicShortText(ClassicAccountDisplayName(acc), 26);
    int savedCount = ClassicSavedAccountCount(bot);
    int runnableCount = ClassicRunnableAccountCount(bot);

    ImGui::BeginChild("ClassicStatusHeader", ImVec2(0, 94), true);
    ImGui::Columns(2, "ClassicStatusHeaderCols", false);
    ImGui::SetColumnWidth(0, (std::max)(420.0f, ImGui::GetWindowWidth() - 286.0f));
    ImGui::TextColored(ImVec4(0.12f, 0.24f, 0.34f, 1.0f), "%s", ClassicPageTitle(g_ClassicPage));
    ImGui::SameLine();
    ClassicStatusPill(status.c_str(), ClassicBotStatusColor(bot));
    ImGui::TextDisabled("Emu %d  |  Slot %d: %s  |  Saved %d / Runnable %d  |  Runtime %s",
        g_ClassicSelectedInstance + 1, slot + 1, accountName.c_str(), savedCount, runnableCount, GetRuntimeStr().c_str());

    ClassicStatusPill(g_AdbValid ? "ADB OK" : "ADB Missing", g_AdbValid ? ImVec4(0.10f, 0.55f, 0.24f, 1.0f) : ImVec4(0.72f, 0.12f, 0.12f, 1.0f));
    ImGui::SameLine();
    ClassicStatusPill(g_MEmuValid ? "Console OK" : "Console Missing", g_MEmuValid ? ImVec4(0.10f, 0.55f, 0.24f, 1.0f) : ImVec4(0.72f, 0.12f, 0.12f, 1.0f));
    ImGui::SameLine();
    ClassicStatusPill(bot.isActive ? "Enabled" : "Disabled", bot.isActive ? ImVec4(0.28f, 0.50f, 0.68f, 1.0f) : ImVec4(0.47f, 0.49f, 0.51f, 1.0f));
    ImGui::SameLine();
    ClassicStatusPill(bot.useMultiMode ? "Multi Mode" : "Single Mode", ImVec4(0.36f, 0.43f, 0.50f, 1.0f));

    ImGui::NextColumn();
    if (ClassicButton(bot.isRunning ? "Stop Bot" : "Start Bot", bot.isRunning ? ImVec4(0.84f, 0.30f, 0.30f, 1.0f) : ImVec4(0.20f, 0.62f, 0.34f, 1.0f), ImVec2(-1, 30))) {
        ClassicStartStopBot(g_ClassicSelectedInstance);
    }
    if (ClassicButton("Launch Game", ImVec4(0.25f, 0.52f, 0.78f, 1.0f), ImVec2(-1, 30))) {
        StartMEmuAndGame(g_ClassicSelectedInstance);
    }
    ImGui::Columns(1);
    ImGui::EndChild();
}

static void ClassicStartTab() {
    BotInstance& bot = g_Bots[g_ClassicSelectedInstance];
    g_ClassicSelectedSlot = (std::max)(0, (std::min)(MAX_ACCOUNT_SLOTS - 1, g_ClassicSelectedSlot));
    AccountSlot* acc = &bot.accounts[g_ClassicSelectedSlot];

    float fullWidth = ImGui::GetContentRegionAvail().x;
    ImGui::Columns(2, "ClassicStartCols", false);
    ImGui::SetColumnWidth(0, (std::max)(420.0f, fullWidth * 0.48f));

    ClassicBeginPanel("Run", ImVec2(0, 224));
    ImGui::Text("Selected emulator: Emu %d", g_ClassicSelectedInstance + 1);
    ImGui::SameLine();
    ClassicStatusPill(ClassicBotStatusLabel(bot).c_str(), ClassicBotStatusColor(bot));
    ImGui::Spacing();
    if (ClassicButton(bot.isRunning ? "Stop Bot" : "Start Bot", bot.isRunning ? ImVec4(0.84f, 0.30f, 0.30f, 1.0f) : ImVec4(0.20f, 0.62f, 0.34f, 1.0f), ImVec2(-1, 42))) {
        ClassicStartStopBot(g_ClassicSelectedInstance);
    }
    if (ClassicButton("Launch Emulator + Hay Day", ImVec4(0.25f, 0.52f, 0.78f, 1.0f), ImVec2(-1, 34))) {
        StartMEmuAndGame(g_ClassicSelectedInstance);
    }
    if (ClassicButton("Apply Fix / Inject Files", ImVec4(0.35f, 0.45f, 0.58f, 1.0f), ImVec2(-1, 30))) {
        InjectImportantFiles(g_ClassicSelectedInstance);
    }
    if (ClassicButton("New Account: Clear Game Data", ImVec4(0.78f, 0.48f, 0.08f, 1.0f), ImVec2(-1, 30))) {
        int targetInst = g_ClassicSelectedInstance;
        std::thread([targetInst]() {
            AddLog(targetInst, Tr("Wiping game data to create new account..."), ImVec4(1, 0.5f, 0, 1));
            RunAdbCommand(targetInst, "shell am force-stop com.supercell.hayday");
            RunAdbCommand(targetInst, "shell rm /data/data/com.supercell.hayday/shared_prefs/storage.xml");
            RunAdbCommand(targetInst, "shell rm /data/data/com.supercell.hayday/shared_prefs/storage_new.xml");
            AddLog(targetInst, Tr("Data wiped successfully! Launch game to start fresh."), ImVec4(0, 1, 0, 1));
            }).detach();
    }
    ImGui::EndChild();

    ClassicBeginPanel("Mode", ImVec2(0, 0));
    if (ImGui::Checkbox("Enable this emulator", &bot.isActive)) SaveConfig();
    int mode = bot.useMultiMode ? 1 : 0;
    if (ImGui::RadioButton("Single slot", mode == 0)) {
        bot.useSingleMode = true;
        bot.useMultiMode = false;
        SaveConfig();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Account rotation", mode == 1)) {
        bot.useSingleMode = false;
        bot.useMultiMode = true;
        SaveConfig();
    }
    const char* crops[] = { "Wheat (120s)", "Corn (300s)", "Carrot (600s)", "Soybean (1200s)", "Sugarcane (1800s)" };
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("Crop", &bot.testCropMode, crops, IM_ARRAYSIZE(crops))) SaveConfig();
    if (ImGui::Checkbox("Random sale cycle", &bot.enableRandomSaleCycle)) SaveConfig();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("Skipped slots", "2, 5-7", g_MultiModeSkipBuf[g_ClassicSelectedInstance], IM_ARRAYSIZE(g_MultiModeSkipBuf[g_ClassicSelectedInstance]));
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        bot.multiModeSkipSlots = g_MultiModeSkipBuf[g_ClassicSelectedInstance];
        SaveConfig();
    }
    ImGui::EndChild();

    ImGui::NextColumn();
    ClassicBeginPanel("Selected Account", ImVec2(0, 224));
    std::string slotPreview = std::to_string(g_ClassicSelectedSlot + 1) + " - " + ClassicShortText(ClassicAccountDisplayName(*acc), 34);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("Slot", slotPreview.c_str())) {
        for (int slot = 0; slot < MAX_ACCOUNT_SLOTS; ++slot) {
            AccountSlot& rowAcc = bot.accounts[slot];
            std::string rowLabel = std::to_string(slot + 1) + " - " + ClassicShortText(ClassicAccountDisplayName(rowAcc), 36);
            bool selected = (slot == g_ClassicSelectedSlot);
            if (ImGui::Selectable(rowLabel.c_str(), selected)) {
                g_ClassicSelectedSlot = slot;
                bot.currentAccountIndex = slot;
                acc = &bot.accounts[g_ClassicSelectedSlot];
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    acc = &bot.accounts[g_ClassicSelectedSlot];
    ImGui::Text("Slot %d", g_ClassicSelectedSlot + 1);
    ImGui::SameLine();
    ClassicStatusPill(acc->hasFile ? "Saved" : "Empty", acc->hasFile ? ImVec4(0.10f, 0.55f, 0.24f, 1.0f) : ImVec4(0.47f, 0.49f, 0.51f, 1.0f));
    ImGui::Text("Name: %s", ClassicShortText(ClassicAccountDisplayName(*acc), 38).c_str());
    ImGui::Text("Level: %d", acc->level);
    ImGui::Text("Coins: %d", acc->coinAmount);
    ImGui::Text("Tag: %s", acc->playerTag.empty() ? "-" : acc->playerTag.c_str());
    ImGui::Spacing();
    if (ClassicButton("Save Current Game to Slot", ImVec4(0.20f, 0.62f, 0.34f, 1.0f), ImVec2(-1, 30))) {
        int inst = g_ClassicSelectedInstance, slot = g_ClassicSelectedSlot;
        std::thread([inst, slot]() { SaveAccountToSlot(inst, slot); }).detach();
    }
    bool canLoad = acc->hasFile && bot.isActive && strlen(bot.adbSerial) > 0;
    if (!canLoad) ImGui::BeginDisabled();
    if (ClassicButton("Load Selected Slot", ImVec4(0.25f, 0.52f, 0.78f, 1.0f), ImVec2(-1, 30))) {
        int inst = g_ClassicSelectedInstance, slot = g_ClassicSelectedSlot;
        bot.currentAccountIndex = slot;
        std::thread([inst, slot]() { LoadAccountFromSlot(inst, slot); }).detach();
    }
    if (!canLoad) ImGui::EndDisabled();
    if (ClassicButton("Open Account Manager", ImVec4(0.60f, 0.64f, 0.67f, 1.0f), ImVec2(-1, 28))) {
        g_ClassicPage = ClassicPage_Accounts;
    }
    ImGui::EndChild();

    ClassicBeginPanel("Setup Health", ImVec2(0, 0));
    ClassicHealthRow("ADB path", g_AdbValid);
    ClassicHealthRow("Emulator console path", g_MEmuValid);
    ClassicHealthRow("ADB serial", strlen(bot.adbSerial) > 0, strlen(bot.adbSerial) > 0 ? bot.adbSerial : "");
    ClassicHealthRow("Saved accounts", ClassicSavedAccountCount(bot) > 0);
    ClassicHealthRow("Runnable slots", ClassicRunnableAccountCount(bot) > 0);
    ImGui::Spacing();
    if (ClassicButton("Validate Paths", ImVec4(0.20f, 0.62f, 0.34f, 1.0f), ImVec2(140, 28))) {
        SaveConfig();
        ValidatePaths();
    }
    ImGui::SameLine();
    if (ClassicButton("Open Settings", ImVec4(0.60f, 0.64f, 0.67f, 1.0f), ImVec2(140, 28))) {
        g_ClassicPage = ClassicPage_Settings;
    }
    ImGui::EndChild();

    ImGui::Columns(1);
}

static void ClassicConnectionTab() {
    BotInstance& bot = g_Bots[g_ClassicSelectedInstance];
    ImGui::Columns(3, "DashConnCols", false);

    ClassicBeginPanel("Emulator & Connection", ImVec2(0, 168));
    if (ImGui::Combo("Engine", &g_GlobalEmulatorMode, "MEmu\0LDPlayer\0")) {
        for (int i = 0; i < 6; ++i) g_Bots[i].emulatorType = g_GlobalEmulatorMode;
        SaveConfig();
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("ADB Serial", bot.adbSerial, 64);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("Touch Device", bot.inputDevice, 64);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("VM Name", bot.vmName, 64);
    ImGui::EndChild();

    ImGui::NextColumn();
    ClassicBeginPanel("Game Controls", ImVec2(0, 168));
    if (ClassicButton("Launch Game", ImVec4(0.34f, 0.72f, 0.34f, 1.0f))) StartMEmuAndGame(g_ClassicSelectedInstance);
    if (ClassicButton(bot.isRunning ? "Stop Bot" : "Start Bot", bot.isRunning ? ImVec4(0.84f, 0.30f, 0.30f, 1.0f) : ImVec4(0.34f, 0.72f, 0.34f, 1.0f))) ClassicStartStopBot(g_ClassicSelectedInstance);
    if (ClassicButton("Apply Fix / Inject", ImVec4(0.25f, 0.62f, 0.88f, 1.0f))) InjectImportantFiles(g_ClassicSelectedInstance);
    if (ClassicButton("Clear Game Data", ImVec4(0.92f, 0.58f, 0.22f, 1.0f))) {
        int targetInst = g_ClassicSelectedInstance;
        std::thread([targetInst]() {
            AddLog(targetInst, Tr("Wiping game data to create new account..."), ImVec4(1, 0.5f, 0, 1));
            RunAdbCommand(targetInst, "shell am force-stop com.supercell.hayday");
            RunAdbCommand(targetInst, "shell rm /data/data/com.supercell.hayday/shared_prefs/storage.xml");
            RunAdbCommand(targetInst, "shell rm /data/data/com.supercell.hayday/shared_prefs/storage_new.xml");
            AddLog(targetInst, Tr("Data wiped successfully! Launch game to start fresh."), ImVec4(0, 1, 0, 1));
            }).detach();
    }
    ImGui::EndChild();

    ImGui::NextColumn();
    ClassicBeginPanel("Bot Mode", ImVec2(0, 168));
    ImGui::Checkbox("Enable Instance", &bot.isActive);
    if (ImGui::Checkbox("Single Mode", &bot.useSingleMode) && bot.useSingleMode) bot.useMultiMode = false;
    if (ImGui::Checkbox("Multi Mode", &bot.useMultiMode) && bot.useMultiMode) bot.useSingleMode = false;
    const char* crops[] = { "Wheat (120s)", "Corn (300s)", "Carrot (600s)", "Soybean (1200s)", "Sugarcane (1800s)" };
    ImGui::Combo("Crop Mode", &bot.testCropMode, crops, IM_ARRAYSIZE(crops));
    ImGui::Checkbox("Random Sale Cycle", &bot.enableRandomSaleCycle);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("Skip Slots", "e.g. 2, 5-7", g_MultiModeSkipBuf[g_ClassicSelectedInstance], IM_ARRAYSIZE(g_MultiModeSkipBuf[g_ClassicSelectedInstance]));
    if (ImGui::IsItemDeactivatedAfterEdit()) bot.multiModeSkipSlots = g_MultiModeSkipBuf[g_ClassicSelectedInstance];
    ImGui::EndChild();

    ImGui::Columns(1);
    ImGui::Spacing();
}

static void ClassicAccountManagerTab() {
    g_ClassicSelectedSlot = (std::max)(0, (std::min)(MAX_ACCOUNT_SLOTS - 1, g_ClassicSelectedSlot));
    BotInstance& bot = g_Bots[g_ClassicSelectedInstance];
    AccountSlot& acc = bot.accounts[g_ClassicSelectedSlot];

    // Two columns: narrow account list (left), wide details (right)
    ImGui::Columns(2, "AccMgrCols", true);
    static bool initCols = true;
    if (initCols) {
        ImGui::SetColumnWidth(0, 360.0f);
        initCols = false;
    }

    // === LEFT: Account List + Quick Actions ===
    ClassicBeginPanel("Accounts", ImVec2(0, 0));
    auto currentMask = ParseMultiModeSkipMask(bot.multiModeSkipSlots);
    for (int slot = 0; slot < MAX_ACCOUNT_SLOTS; ++slot) {
        AccountSlot& rowAcc = bot.accounts[slot];
        ImGui::PushID(slot);
        bool isSelected = (g_ClassicSelectedSlot == slot);
        ImVec4 slotColor = rowAcc.hasFile ? ImVec4(0.2f, 0.8f, 0.3f, 1.0f) : ImVec4(0.45f, 0.45f, 0.45f, 1.0f);

        // Compact row: slot number + name
        std::string label = std::to_string(slot + 1) + ". ";
        if (rowAcc.hasFile && !rowAcc.farmName.empty()) label += rowAcc.farmName;
        else if (rowAcc.hasFile) label += rowAcc.name.empty() ? "Saved" : rowAcc.name;
        else label += "---";
        if (label.length() > 38) label = label.substr(0, 36) + "..";

        if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, 20))) {
            g_ClassicSelectedSlot = slot;
            bot.currentAccountIndex = slot;
        }
        if (rowAcc.hasFile) {
            ImGui::SameLine(320.0f);
            bool runThisSlot = !IsMultiModeSlotSkipped(currentMask, slot);
            if (ImGui::Checkbox("##run", &runThisSlot)) {
                currentMask[slot] = !runThisSlot;
                bot.multiModeSkipSlots = GenerateMultiModeSkipString(currentMask);
                SaveConfig();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Include in Multi Mode Rotation");
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    if (ClassicButton("Save Current Game to Slot", ImVec4(0.34f, 0.72f, 0.34f, 1.0f), ImVec2(-1, 24))) {
        int inst = g_ClassicSelectedInstance, slot = g_ClassicSelectedSlot;
        std::thread([inst, slot]() { SaveAccountToSlot(inst, slot); }).detach();
    }
    bool canLoad = acc.hasFile && bot.isActive && strlen(bot.adbSerial) > 0;
    if (!canLoad) ImGui::BeginDisabled();
    if (ClassicButton("Load Selected Slot", ImVec4(0.26f, 0.58f, 0.88f, 1.0f), ImVec2(-1, 24))) {
        int inst = g_ClassicSelectedInstance, slot = g_ClassicSelectedSlot;
        bot.currentAccountIndex = slot;
        std::thread([inst, slot]() { LoadAccountFromSlot(inst, slot); }).detach();
    }
    if (!canLoad) ImGui::EndDisabled();
    ImGui::EndChild();

    // === RIGHT: Selected Account Details ===
    ImGui::NextColumn();
    ClassicBeginPanel("Selected Account", ImVec2(0, 0));

    // Farm info + Barn state side by side
    ImGui::Columns(2, "SelAccDetailCols", false);
    ImGui::SetColumnWidth(0, 250.0f);
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Farm Info");
    ImGui::Separator();
    ImGui::Text("Level: %d", acc.level);
    ImGui::Text("Coins: %d", acc.coinAmount);
    ImGui::Text("Gems: %d", acc.diamondAmount);
    ImGui::Text("Tag: %s", acc.playerTag.empty() ? "-" : acc.playerTag.c_str());
    ImGui::Text("Slot: %d", g_ClassicSelectedSlot + 1);
    ImGui::SetNextItemWidth(150.0f);
    if (ClassicFarmConfigCombo("Config", acc.farmConfigName)) {
        ApplyFarmConfigForSlot(g_ClassicSelectedInstance, g_ClassicSelectedSlot);
        SaveConfig();
    }
    char nameBuf[64];
    strncpy(nameBuf, acc.name.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) acc.name = nameBuf;
    if (ImGui::IsItemDeactivatedAfterEdit()) SaveConfig();


    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Barn Materials");
    ImGui::Separator();
    InventoryData& inv = acc.currentInv;
    ImGui::Text("Bolt %d   Plank %d   Tape %d", inv.bolt, inv.plank, inv.tape);
    ImGui::Text("Nail %d   Screw %d   Panel %d", inv.nail, inv.screw, inv.panel);
    ImGui::Text("Deed %d   Mallet %d  Marker %d", inv.deed, inv.mallet, inv.marker);
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Land Materials");
    ImGui::Separator();
    ImGui::Text("Axe %d   Saw %d   Shovel %d", inv.axe, inv.saw, inv.shovel);
    ImGui::Text("Dyna %d  TNT %d  Map %d", inv.dynamite, inv.tnt, inv.map);
    ImGui::Columns(1);

    ImGui::Separator();
    if (ImGui::TreeNode("Tom Config")) {
        if (ImGui::Checkbox("Enable Auto Tom", &acc.autoTomEnabled)) SaveConfig();
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Hours", &acc.tomRemainingHours)) {
            acc.tomRemainingHours = (std::max)(0, acc.tomRemainingHours);
            SaveConfig();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Minutes", &acc.tomRemainingMinutes)) {
            acc.tomRemainingMinutes = (std::max)(0, (std::min)(59, acc.tomRemainingMinutes));
            SaveConfig();
        }
        const char* tomCategories[] = { "Barn", "Silo" };
        ImGui::SetNextItemWidth(120);
        if (ImGui::Combo("Category", &acc.tomCategory, tomCategories, IM_ARRAYSIZE(tomCategories))) SaveConfig();
        char tomItemBuf[64];
        strncpy(tomItemBuf, acc.tomItemName.c_str(), sizeof(tomItemBuf) - 1);
        tomItemBuf[sizeof(tomItemBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(180);
        if (ImGui::InputText("Target Item", tomItemBuf, sizeof(tomItemBuf))) acc.tomItemName = tomItemBuf;
        if (ImGui::IsItemDeactivatedAfterEdit()) SaveConfig();
        auto tomNow = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (ImGui::Button("Tom Start Now", ImVec2(110, 24))) {
            acc.nextTomTime = 0;
            SaveConfig();
        }
        ImGui::SameLine();
        if (ImGui::Button("Tom In 2H", ImVec2(80, 24))) {
            acc.nextTomTime = tomNow + (2LL * 3600LL);
            SaveConfig();
        }
        static int classicTomDelayMinutes = 120;
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Delay (min)", &classicTomDelayMinutes, 1, 15)) {
            classicTomDelayMinutes = (std::max)(0, (std::min)(4320, classicTomDelayMinutes));
        }
        ImGui::SameLine();
        if (ImGui::Button("Schedule", ImVec2(80, 24))) {
            acc.nextTomTime = tomNow + (static_cast<long long>(classicTomDelayMinutes) * 60LL);
            SaveConfig();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Tom", ImVec2(80, 24))) {
            acc.tomStartTime = 0;
            acc.nextTomTime = 0;
            SaveConfig();
        }
        ImGui::TreePop();
    }
    ImGui::EndChild();

    ImGui::Columns(1);
}

static void ClassicBotControlTab() {
    // Content merged into Dashboard sidebar + connection panel.
    // Janitor and scheduling settings are in Farm Config tab.
}

static void ClassicFarmConfigTab() {
    BotInstance& bot = g_Bots[g_ClassicSelectedInstance];
    AccountSlot& acc = bot.accounts[(std::max)(0, (std::min)(MAX_ACCOUNT_SLOTS - 1, bot.currentAccountIndex))];
    static char slotAssignmentBuf[128] = "";
    static int slotAssignmentInstance = -1;
    static std::string slotAssignmentConfig;
    EnsureFarmConfigs();
    g_SelectedFarmConfigIndex = (std::max)(0, (std::min)(static_cast<int>(g_FarmConfigs.size()) - 1, g_SelectedFarmConfigIndex));
    FarmConfigProfile selectedCfgSnapshot = g_FarmConfigs[g_SelectedFarmConfigIndex];
    if (slotAssignmentInstance != g_ClassicSelectedInstance || slotAssignmentConfig != selectedCfgSnapshot.name) {
        std::string assigned = BuildFarmConfigSlotList(g_ClassicSelectedInstance, selectedCfgSnapshot.name);
        strncpy(slotAssignmentBuf, assigned.c_str(), sizeof(slotAssignmentBuf) - 1);
        slotAssignmentBuf[sizeof(slotAssignmentBuf) - 1] = '\0';
        slotAssignmentInstance = g_ClassicSelectedInstance;
        slotAssignmentConfig = selectedCfgSnapshot.name;
    }

    ImGui::TextUnformatted("Configuration Selection");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("Config", selectedCfgSnapshot.name.c_str())) {
        for (size_t i = 0; i < g_FarmConfigs.size(); ++i) {
            bool selected = (static_cast<int>(i) == g_SelectedFarmConfigIndex);
            if (ImGui::Selectable(g_FarmConfigs[i].name.c_str(), selected)) {
                g_SelectedFarmConfigIndex = static_cast<int>(i);
                strncpy(g_FarmConfigNameEditBuf, g_FarmConfigs[i].name.c_str(), sizeof(g_FarmConfigNameEditBuf) - 1);
                g_FarmConfigNameEditBuf[sizeof(g_FarmConfigNameEditBuf) - 1] = '\0';
                slotAssignmentInstance = -1;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ClassicButton("New Config", ImVec4(0.82f, 0.82f, 0.82f, 1.0f), ImVec2(120, 26))) {
        FarmConfigProfile newCfg;
        newCfg.name = "config_" + std::to_string(g_FarmConfigs.size() + 1);
        g_FarmConfigs.push_back(newCfg);
        g_SelectedFarmConfigIndex = static_cast<int>(g_FarmConfigs.size()) - 1;
        strncpy(g_FarmConfigNameEditBuf, newCfg.name.c_str(), sizeof(g_FarmConfigNameEditBuf) - 1);
        slotAssignmentInstance = -1;
        SaveConfig();
    }
    ImGui::SameLine();
    if (ClassicButton("Copy Config", ImVec4(0.82f, 0.82f, 0.82f, 1.0f), ImVec2(120, 26))) {
        FarmConfigProfile copy = selectedCfgSnapshot;
        copy.name = selectedCfgSnapshot.name + "_copy";
        g_FarmConfigs.push_back(copy);
        g_SelectedFarmConfigIndex = static_cast<int>(g_FarmConfigs.size()) - 1;
        strncpy(g_FarmConfigNameEditBuf, copy.name.c_str(), sizeof(g_FarmConfigNameEditBuf) - 1);
        slotAssignmentInstance = -1;
        SaveConfig();
    }
    ImGui::SameLine();
    if (ClassicButton("Save Config", ImVec4(0.34f, 0.72f, 0.34f, 1.0f), ImVec2(120, 26))) SaveConfig();
    ImGui::Separator();

    bool configChanged = false;
    ImGui::SetNextItemWidth(220);
    if (ImGui::InputText("Config Name", g_FarmConfigNameEditBuf, IM_ARRAYSIZE(g_FarmConfigNameEditBuf))) {
        std::string newName = NormalizeLogMessage(g_FarmConfigNameEditBuf);
        if (!newName.empty()) {
            std::string oldName = g_FarmConfigs[g_SelectedFarmConfigIndex].name;
            g_FarmConfigs[g_SelectedFarmConfigIndex].name = newName;
            for (int i = 0; i < 6; ++i) {
                for (int j = 0; j < MAX_ACCOUNT_SLOTS; ++j) {
                    if (g_Bots[i].accounts[j].farmConfigName == oldName) g_Bots[i].accounts[j].farmConfigName = newName;
                }
            }
            slotAssignmentInstance = -1;
            configChanged = true;
        }
    }
    ImGui::SameLine();
    bool canDeleteConfig = g_FarmConfigs.size() > 1 && g_FarmConfigs[g_SelectedFarmConfigIndex].name != "default";
    if (!canDeleteConfig) ImGui::BeginDisabled();
    if (ClassicButton("Delete Config", ImVec4(0.86f, 0.32f, 0.32f, 1.0f), ImVec2(130, 26))) {
        std::string deletedName = g_FarmConfigs[g_SelectedFarmConfigIndex].name;
        g_FarmConfigs.erase(g_FarmConfigs.begin() + g_SelectedFarmConfigIndex);
        g_SelectedFarmConfigIndex = 0;
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < MAX_ACCOUNT_SLOTS; ++j) {
                if (g_Bots[i].accounts[j].farmConfigName == deletedName) g_Bots[i].accounts[j].farmConfigName = g_FarmConfigs[0].name;
            }
        }
        SaveConfig();
        return;
    }
    if (!canDeleteConfig) ImGui::EndDisabled();
    FarmConfigProfile& cfg = g_FarmConfigs[g_SelectedFarmConfigIndex];
    ImGui::Spacing();
    ImGui::Text("Slot assignment: Instance %d / Slot %d", g_ClassicSelectedInstance + 1, bot.currentAccountIndex + 1);
    ImGui::SameLine();
    if (ClassicFarmConfigCombo("Use Config##activeSlotConfig", acc.farmConfigName)) {
        ApplyFarmConfigForSlot(g_ClassicSelectedInstance, bot.currentAccountIndex);
        slotAssignmentInstance = -1;
        SaveConfig();
    }
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("Slots using this config", "e.g. 1,3,5-7", slotAssignmentBuf, IM_ARRAYSIZE(slotAssignmentBuf));
    ImGui::SameLine();
    if (ClassicButton("Apply Slots", ImVec4(0.34f, 0.72f, 0.34f, 1.0f), ImVec2(110, 26))) {
        ApplyFarmConfigSlotList(g_ClassicSelectedInstance, g_FarmConfigs[g_SelectedFarmConfigIndex].name, slotAssignmentBuf);
        slotAssignmentInstance = -1;
        SaveConfig();
    }
    ImGui::Separator();

    ImGui::Columns(4, "ClassicFarmColumns", false);
    ClassicBeginPanel("Farm Settings", ImVec2(0, 0));
    ImGui::Checkbox("Enable Farming Operations", &bot.isActive);
    const char* crops[] = { "Wheat", "Corn", "Carrot", "Soybean", "Sugarcane" };
    configChanged |= ImGui::Combo("Plant Rotation", &cfg.cropMode, crops, IM_ARRAYSIZE(crops));
    ImGui::Checkbox("Wait for Crop Growth", &bot.useSingleMode);
    ImGui::TextDisabled("Applied per slot when that account runs.");
    ImGui::EndChild();

    ImGui::NextColumn();
    ClassicBeginPanel("Market Settings", ImVec2(0, 0));
    configChanged |= ImGui::Checkbox("Random Sale Cycle", &cfg.randomSaleCycle);
    ImGui::Checkbox("Market Operations Only When Silo Full", &g_OnlySellIfSiloFull);
    ImGui::TextDisabled("Silo-full-only is still global.");
    configChanged |= ImGui::Checkbox("Use Max Price", &cfg.sellAtMaxPrice);
    const char* saleModes[] = { "Game Default", "Max Stack", "Fixed Stack", "Preserve OCR" };
    configChanged |= ImGui::Combo("Sale Stack Mode", &cfg.saleStackMode, saleModes, IM_ARRAYSIZE(saleModes));
    configChanged |= ImGui::SliderInt("Sell Every Cycles", &cfg.sellCycles, 0, 10);
    configChanged |= ImGui::SliderInt("Fixed Stack", &cfg.fixedSaleStack, 1, 10);
    configChanged |= ImGui::SliderInt("Keep Wheat Min", &cfg.keepWheatReserve, 0, 100);
    configChanged |= ImGui::SliderInt("Emergency Wheat Limit", &cfg.emergencyWheatStackLimit, 0, 20);
    ImGui::EndChild();

    ImGui::NextColumn();
    ClassicBeginPanel("Machine Settings", ImVec2(0, 0));
    ImGui::TextUnformatted("Feed Mill");
    configChanged |= ImGui::Checkbox("Enable Feed Mill Usage", &cfg.autoCowFeedEnabled);
    configChanged |= ImGui::Checkbox("Feed mill has 4 slots", &cfg.cowFeedUseFourSlots);
    ImGui::Text("Run Every: 60 min");
    ImGui::Text("Cow Feed: %d per run", cfg.cowFeedUseFourSlots ? 4 : 3);
    ImGui::TextDisabled("If feed resources are short, next planting uses soybean/corn 2:1.");
    ImGui::Separator();
    ImGui::TextUnformatted("Dairy");
    configChanged |= ImGui::Checkbox("Enable Dairy Usage", &cfg.autoDairyEnabled);
    const char* dairyProducts[] = { "Cream", "Butter", "Cheese" };
    configChanged |= ImGui::Combo("Dairy Product", &cfg.dairyProductMode, dairyProducts, IM_ARRAYSIZE(dairyProducts));
    ImGui::SetNextItemWidth(96.0f);
    if (ImGui::InputInt("Dairy Slots", &cfg.dairyQueueCount, 1, 1)) {
        cfg.dairyQueueCount = (std::max)(1, (std::min)(4, cfg.dairyQueueCount));
        configChanged = true;
    }
    ImGui::TextDisabled("Uses the selected product template first; butter/cheese fall back to cream offsets if their templates are missing.");
    ImGui::EndChild();

    ImGui::NextColumn();
    ClassicBeginPanel("Scheduling & Maintenance", ImVec2(0, 0));
    BotInstance& selBot = g_Bots[g_ClassicSelectedInstance];
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Batch Farming");
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderInt("Cycles/Account", &selBot.cyclesPerAccount, 1, 50)) {
        selBot.cyclesPerAccount = (std::max)(1, (std::min)(50, selBot.cyclesPerAccount));
        SaveConfig();
    }
    ImGui::TextDisabled("Farm N cycles on one account before switching. 1 = rotate every cycle.");
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Pair Rotation");
    if (ImGui::Checkbox("Enable Pair Rotation", &selBot.enablePairRotation)) SaveConfig();
    if (selBot.enablePairRotation) {
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderInt("Group Size", &selBot.pairGroupSize, 1, 5)) {
            selBot.pairGroupSize = (std::max)(1, (std::min)(5, selBot.pairGroupSize));
            SaveConfig();
        }
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt("Rotate (min)", &selBot.pairRotationMinutes, 15, 30)) {
            selBot.pairRotationMinutes = (std::max)(5, (std::min)(1440, selBot.pairRotationMinutes));
            SaveConfig();
        }
        // Show current group status
        std::vector<int> eligible;
        auto mask = ParseMultiModeSkipMask(selBot.multiModeSkipSlots);
        for (int k = 0; k < MAX_ACCOUNT_SLOTS; k++) {
            if (selBot.accounts[k].hasFile && !IsMultiModeSlotSkipped(mask, k))
                eligible.push_back(k);
        }
        if (!eligible.empty()) {
            int gs = (std::max)(1, (std::min)((int)eligible.size(), selBot.pairGroupSize));
            int off = selBot.currentPairOffset % (int)eligible.size();
            std::string activeStr;
            for (int g = 0; g < gs && g < (int)eligible.size(); g++) {
                int idx = (off + g) % (int)eligible.size();
                if (g > 0) activeStr += ", ";
                activeStr += std::to_string(eligible[idx] + 1);
            }
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "Active: Slots %s", activeStr.c_str());
            if (selBot.pairRotationStartedAt.time_since_epoch().count() != 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
                    std::chrono::steady_clock::now() - selBot.pairRotationStartedAt).count();
                int remaining = selBot.pairRotationMinutes - (int)elapsed;
                if (remaining > 0) ImGui::TextDisabled("Next rotation in %d min", remaining);
            }
        }
        if (ImGui::SmallButton("Rotate Now")) {
            std::vector<int> el;
            for (int k = 0; k < MAX_ACCOUNT_SLOTS; k++) {
                if (selBot.accounts[k].hasFile && !IsMultiModeSlotSkipped(mask, k))
                    el.push_back(k);
            }
            int gs = (std::max)(1, (std::min)((int)el.size(), selBot.pairGroupSize));
            selBot.currentPairOffset = (selBot.currentPairOffset + gs) % (std::max)(1, (int)el.size());
            selBot.pairRotationStartedAt = std::chrono::steady_clock::now();
            SaveConfig();
        }
    }
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Account Run Limit");
    if (ImGui::Checkbox("Enable Run Limit", &selBot.enableAccountRunLimit)) SaveConfig();
    if (selBot.enableAccountRunLimit) {
        ImGui::SetNextItemWidth(90);
        if (ImGui::InputInt("Run (min)", &selBot.accountRunMinutes, 1, 5)) {
            selBot.accountRunMinutes = (std::max)(1, (std::min)(1440, selBot.accountRunMinutes));
            SaveConfig();
        }
        ImGui::SetNextItemWidth(90);
        if (ImGui::InputInt("Rest (min)", &selBot.accountRestMinutes, 1, 5)) {
            selBot.accountRestMinutes = (std::max)(1, (std::min)(1440, selBot.accountRestMinutes));
            SaveConfig();
        }
        if (ImGui::Checkbox("Replace During Rest", &selBot.replaceAccountDuringRest)) SaveConfig();
        ImGui::TextDisabled("Off: AFK. On: load another account while resting.");
    }
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "RAM Cleaner (Janitor)");
    if (ImGui::Checkbox("Enable Janitor", &selBot.enableJanitor)) SaveConfig();
    if (selBot.enableJanitor) {
        if (ImGui::SliderInt("Cycle Limit", &selBot.janitorLimit, 3, 50)) SaveConfig();
        ImGui::TextDisabled("Reboots emulator after N cycles.");
    }
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Animals");
    configChanged |= ImGui::Checkbox("Enable Cow", &cfg.autoCowFeedEnabled);
    ImGui::TextDisabled("Cow pasture + feed routine (60 min).");
    ImGui::BeginDisabled();
    static bool unsupportedAnimal = false;
    ImGui::Checkbox("Chicken / Pig / Sheep", &unsupportedAnimal);
    ImGui::TextDisabled("Not implemented yet.");
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::Columns(1);

    if (configChanged) {
        ApplyFarmConfigForSlot(g_ClassicSelectedInstance, bot.currentAccountIndex);
        SaveConfig();
    }
}

static void ClassicTemplateManagerTab() {
    if (ImGui::BeginTabBar("ClassicTemplateTabs")) {
        if (ImGui::BeginTabItem("Template Testing")) {
            ImGui::Columns(2, "ClassicTemplateTestingCols", false);
            ClassicBeginPanel("Screenshot Controls", ImVec2(310, 0));
            ClassicInstanceCombo("Emulator##template");
            if (ClassicButton("Take Screenshot", ImVec4(0.25f, 0.62f, 0.88f, 1.0f))) {
                int inst = g_ClassicSelectedInstance;
                cv::Mat raw = CaptureInstanceScreen(inst, kAdbPath, g_Bots[inst].adbSerial);
                cv::Mat processed = ProcessVisionFrame(inst, raw);
                UpdateVisionTexture(processed.empty() ? raw : processed);
            }
            ImGui::Separator();
            if (ImGui::Button("Test Field", ImVec2(135, 28))) PerformTemplateTest(g_ClassicSelectedInstance, f_templatePath, "Field Check", g_Thresholds.soilThreshold, false);
            ImGui::SameLine();
            if (ImGui::Button("Test Seed", ImVec2(135, 28))) {
                std::string tPath = (g_Bots[g_ClassicSelectedInstance].testCropMode == 0) ? w_templatePath : (g_Bots[g_ClassicSelectedInstance].testCropMode == 1 ? c_templatePath : (g_Bots[g_ClassicSelectedInstance].testCropMode == 2 ? carrot_templatePath : (g_Bots[g_ClassicSelectedInstance].testCropMode == 3 ? soybean_templatePath : sugarcane_templatePath)));
                PerformTemplateTest(g_ClassicSelectedInstance, tPath, "Seed Check", g_Thresholds.wheatThreshold, false);
            }
            if (ImGui::Button("Test Sickle", ImVec2(135, 28))) PerformTemplateTest(g_ClassicSelectedInstance, s_templatePath, "Sickle Check", g_Thresholds.sickleThreshold, false);
            ImGui::SameLine();
            if (ImGui::Button("Test Cow Feed", ImVec2(135, 28))) PerformTemplateTest(g_ClassicSelectedInstance, feed_mill_templatePath, "Cow Feed Check", g_Thresholds.feedMillThreshold, false);
            if (ImGui::Button("Test Dairy", ImVec2(135, 28))) PerformTemplateTest(g_ClassicSelectedInstance, dairy_templatePath, "Dairy Check", g_Thresholds.dairyThreshold, false);
            ImGui::EndChild();
            ImGui::NextColumn();
            ClassicBeginPanel("Screenshot Display", ImVec2(0, 0));
            if (g_VisionTexture != 0 && g_VisionTexWidth > 0 && g_VisionTexHeight > 0) {
                float availW = ImGui::GetContentRegionAvail().x - 20.0f;
                float availH = ImGui::GetContentRegionAvail().y - 40.0f;
                float scale = (std::min)(availW / static_cast<float>(g_VisionTexWidth), availH / static_cast<float>(g_VisionTexHeight));
                scale = (std::max)(0.1f, scale);
                ImGui::Image((void*)(intptr_t)g_VisionTexture, ImVec2(g_VisionTexWidth * scale, g_VisionTexHeight * scale));
                ImGui::Text("Screenshot Info: %d x %d pixels", g_VisionTexWidth, g_VisionTexHeight);
            }
            else {
                ImGui::TextDisabled("Take a screenshot to show bot vision here.");
            }
            ImGui::EndChild();
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Template Replacing")) {
            ImGui::BeginChild("ClassicTemplateReplacing", ImVec2(0, 0), true);
            ImGui::Checkbox("Separate all templates for each instance", &g_SeparateTemplates);
            RenderTemplateRow("Mailbox", g_mailboxPathBuf, IM_ARRAYSIZE(g_mailboxPathBuf), mailbox_templatePath, &g_Thresholds.mailboxThreshold);
            RenderTemplateRow("Loading", g_loadingPathBuf, IM_ARRAYSIZE(g_loadingPathBuf), loading_templatePath, &g_Thresholds.loadingThreshold);
            RenderTemplateRow("Field", g_fieldPathBuf, IM_ARRAYSIZE(g_fieldPathBuf), f_templatePath, &g_Thresholds.soilThreshold);
            RenderTemplateRow("Wheat Seed", g_wheatPathBuf, IM_ARRAYSIZE(g_wheatPathBuf), w_templatePath, &g_Thresholds.wheatThreshold);
            RenderTemplateRow("Corn Seed", g_cornPathBuf, IM_ARRAYSIZE(g_cornPathBuf), c_templatePath, &g_Thresholds.cornThreshold);
            RenderTemplateRow("Soybean Seed", g_soybeanPathBuf, IM_ARRAYSIZE(g_soybeanPathBuf), soybean_templatePath, &g_Thresholds.soybeanThreshold);
            RenderTemplateRow("Feed Mill", g_feedMillPathBuf, IM_ARRAYSIZE(g_feedMillPathBuf), feed_mill_templatePath, &g_Thresholds.feedMillThreshold);
            RenderTemplateRow("Cow Feed", g_cowFeedPathBuf, IM_ARRAYSIZE(g_cowFeedPathBuf), cow_feed_templatePath, &g_Thresholds.cowFeedThreshold);
            RenderTemplateRow("Cow Pasture", g_cowPasturePathBuf, IM_ARRAYSIZE(g_cowPasturePathBuf), cow_pasture_templatePath, &g_Thresholds.cowPastureThreshold);
            RenderTemplateRow("Dairy", g_dairyPathBuf, IM_ARRAYSIZE(g_dairyPathBuf), dairy_templatePath, &g_Thresholds.dairyThreshold);
            RenderTemplateRow("Cream", g_creamPathBuf, IM_ARRAYSIZE(g_creamPathBuf), cream_templatePath, &g_Thresholds.creamThreshold);
            RenderTemplateRow("Butter", g_butterPathBuf, IM_ARRAYSIZE(g_butterPathBuf), butter_templatePath, &g_Thresholds.creamThreshold);
            RenderTemplateRow("Cheese", g_cheesePathBuf, IM_ARRAYSIZE(g_cheesePathBuf), cheese_templatePath, &g_Thresholds.creamThreshold);
            RenderTemplateRow("Not Enough", g_notEnoughPathBuf, IM_ARRAYSIZE(g_notEnoughPathBuf), not_enough_templatePath, &g_Thresholds.notEnoughThreshold);
            RenderTemplateRow("Cross", g_crossPathBuf, IM_ARRAYSIZE(g_crossPathBuf), cross_templatePath, &g_Thresholds.crossThreshold);
            if (ClassicButton("Save Template Config", ImVec4(0.34f, 0.72f, 0.34f, 1.0f), ImVec2(180, 28))) SaveConfig();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Template Maker")) {
            static char tmplStatus[192] = "Ready.";
            static ImVec4 tmplColor = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);

            ClassicBeginPanel("Save Screenshot", ImVec2(360, 0));
            ClassicInstanceCombo("Emulator##templatemaker");
            ImGui::TextWrapped("Capture a fresh game screen into the templates folder, then crop it into a template PNG.");
            if (ClassicButton("Take Screenshot and Save", ImVec4(0.25f, 0.62f, 0.88f, 1.0f), ImVec2(-1, 34))) {
                int inst = g_ClassicSelectedInstance;
                strncpy(tmplStatus, "Capturing...", sizeof(tmplStatus) - 1);
                tmplStatus[sizeof(tmplStatus) - 1] = '\0';
                tmplColor = ImVec4(0.54f, 0.42f, 0.00f, 1.0f);

                std::thread([inst]() {
                    std::string currentAdb = GetUniversalAdbPath(inst);
                    cv::Mat rawScreen = CaptureInstanceScreen(inst, currentAdb, g_Bots[inst].adbSerial);
                    if (rawScreen.empty()) {
                        strncpy(tmplStatus, "Error: could not capture screen.", sizeof(tmplStatus) - 1);
                        tmplStatus[sizeof(tmplStatus) - 1] = '\0';
                        tmplColor = ImVec4(0.72f, 0.08f, 0.08f, 1.0f);
                        AddLog(inst, "Screenshot failed: empty frame.", ImVec4(1, 0, 0, 1));
                        return;
                    }

                    time_t now = time(0);
                    struct tm tstruct;
                    char timeBuf[80];
                    localtime_s(&tstruct, &now);
                    strftime(timeBuf, sizeof(timeBuf), "%H%M%S", &tstruct);

                    std::error_code ec;
                    if (!fs::exists("templates", ec)) fs::create_directories("templates", ec);
                    std::string filename = "screenshot_inst" + std::to_string(inst + 1) + "_" + std::string(timeBuf) + ".png";
                    std::string fullPath = "templates\\" + filename;

                    if (cv::imwrite(fullPath, rawScreen)) {
                        std::string msg = "Saved: " + fullPath;
                        strncpy(tmplStatus, msg.c_str(), sizeof(tmplStatus) - 1);
                        tmplStatus[sizeof(tmplStatus) - 1] = '\0';
                        tmplColor = ImVec4(0.00f, 0.48f, 0.10f, 1.0f);
                        AddLog(inst, "Screenshot saved to: " + fullPath, ImVec4(0, 1, 0, 1));
                    }
                    else {
                        strncpy(tmplStatus, "Error: could not write screenshot.", sizeof(tmplStatus) - 1);
                        tmplStatus[sizeof(tmplStatus) - 1] = '\0';
                        tmplColor = ImVec4(0.72f, 0.08f, 0.08f, 1.0f);
                    }
                    }).detach();
            }
            ImGui::Spacing();
            ImGui::TextColored(tmplColor, "Status: %s", tmplStatus);
            ImGui::Separator();
            ImGui::TextWrapped("Template file names the bot already uses are listed in Template Replacing.");
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

static void ClassicLogsTab() {
    if (ImGui::Button("Clear Logs", ImVec2(100, 28))) {
        ClassicClearLogs();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Scroll", &g_AutoScrollLogs);
    ImGui::SameLine();
    if (ImGui::Checkbox("Auto-Clear Logs", &g_AutoClearLogs)) {
        g_LastLogAutoClear = std::chrono::steady_clock::now();
        SaveConfig();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::InputInt("min##classiclogclear", &g_AutoClearLogMinutes, 1, 10)) {
        g_AutoClearLogMinutes = (std::max)(1, (std::min)(1440, g_AutoClearLogMinutes));
        SaveConfig();
    }
    ImGui::SameLine();
    ClassicDrawLogFilterCombo(130.0f);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##classiclogsearch", "Search logs", g_LogSearchBuf, IM_ARRAYSIZE(g_LogSearchBuf));
    ImGui::SameLine();
    if (ImGui::Button("Open Timing CSV", ImVec2(140, 28))) {
        std::string timingPath = GetActionTimingLogPath();
        if (EnsureActionTimingLogFile()) {
            ShellExecuteA(nullptr, "open", timingPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        else {
            AddLog(-1, "Failed to create or open action timing CSV.", ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        }
    }
    ImGui::TextDisabled("Search filters the always-visible log strip too. Timing file: %s", GetActionTimingLogPath().c_str());
    ImGui::Separator();
    ClassicDrawLogs(0.0f);
}

static void ClassicSettingsTab() {
    ImGui::Columns(3, "SettingsCols3", false);

    // === Column 1: Coordinate Profile ===
    ClassicBeginPanel("Coordinate Profile", ImVec2(0, 0));
    ImGui::TextDisabled("Fallback tap coordinates.");
    bool coordChanged = false;
    coordChanged |= DrawCoordinateInputRow("Profile Open", g_CoordinateProfile.profileOpenX, g_CoordinateProfile.profileOpenY);
    coordChanged |= DrawCoordinateInputRow("Copy Tag", g_CoordinateProfile.profileCopyTagX, g_CoordinateProfile.profileCopyTagY);
    coordChanged |= DrawCoordinateInputRow("Profile Close", g_CoordinateProfile.profileCloseX, g_CoordinateProfile.profileCloseY);
    coordChanged |= DrawCoordinateInputRow("Sale Close", g_CoordinateProfile.salePanelCloseX, g_CoordinateProfile.salePanelCloseY);
    coordChanged |= DrawCoordinateInputRow("Market Close", g_CoordinateProfile.marketCloseX, g_CoordinateProfile.marketCloseY);
    coordChanged |= DrawCoordinateInputRow("Market Close 2", g_CoordinateProfile.marketCloseSecondX, g_CoordinateProfile.marketCloseSecondY);
    coordChanged |= DrawCoordinateInputRow("Crate Ad", g_CoordinateProfile.occupiedCrateAdOpenX, g_CoordinateProfile.occupiedCrateAdOpenY);
    coordChanged |= DrawCoordinateInputRow("Ad Confirm", g_CoordinateProfile.createAdConfirmX, g_CoordinateProfile.createAdConfirmY);
    coordChanged |= DrawCoordinateInputRow("Qty Plus", g_CoordinateProfile.saleQuantityPlusX, g_CoordinateProfile.saleQuantityPlusY);
    coordChanged |= DrawCoordinateInputRow("Qty Minus", g_CoordinateProfile.saleQuantityMinusX, g_CoordinateProfile.saleQuantityMinusY);
    coordChanged |= DrawCoordinateInputRow("Max Price", g_CoordinateProfile.saleMaxPriceX, g_CoordinateProfile.saleMaxPriceY);
    coordChanged |= DrawCoordinateInputRow("Price Minus", g_CoordinateProfile.salePriceMinusX, g_CoordinateProfile.salePriceMinusY);
    coordChanged |= DrawCoordinateInputRow("Put On Sale", g_CoordinateProfile.putOnSaleX, g_CoordinateProfile.putOnSaleY);
    if (coordChanged) SaveConfig();
    if (ClassicButton("Reset Coordinates", ImVec4(0.86f, 0.32f, 0.32f, 1.0f), ImVec2(-1, 26))) {
        g_CoordinateProfile = CoordinateProfile{};
        SaveConfig();
    }
    // Coordinate Tester
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Tester");
    static int coordTestIndex = 0;
    const char* coordNames[] = {
        "Profile Open", "Copy Tag", "Profile Close", "Sale Close",
        "Market Close", "Market Close 2", "Crate Ad", "Ad Confirm",
        "Qty Plus", "Qty Minus", "Max Price", "Price Minus", "Put On Sale"
    };
    coordTestIndex = (std::max)(0, (std::min)(static_cast<int>(IM_ARRAYSIZE(coordNames)) - 1, coordTestIndex));
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##coordsel", &coordTestIndex, coordNames, IM_ARRAYSIZE(coordNames));
    auto selectedCoord = [&]() -> POINT {
        switch (coordTestIndex) {
        case 0: return POINT{ g_CoordinateProfile.profileOpenX, g_CoordinateProfile.profileOpenY };
        case 1: return POINT{ g_CoordinateProfile.profileCopyTagX, g_CoordinateProfile.profileCopyTagY };
        case 2: return POINT{ g_CoordinateProfile.profileCloseX, g_CoordinateProfile.profileCloseY };
        case 3: return POINT{ g_CoordinateProfile.salePanelCloseX, g_CoordinateProfile.salePanelCloseY };
        case 4: return POINT{ g_CoordinateProfile.marketCloseX, g_CoordinateProfile.marketCloseY };
        case 5: return POINT{ g_CoordinateProfile.marketCloseSecondX, g_CoordinateProfile.marketCloseSecondY };
        case 6: return POINT{ g_CoordinateProfile.occupiedCrateAdOpenX, g_CoordinateProfile.occupiedCrateAdOpenY };
        case 7: return POINT{ g_CoordinateProfile.createAdConfirmX, g_CoordinateProfile.createAdConfirmY };
        case 8: return POINT{ g_CoordinateProfile.saleQuantityPlusX, g_CoordinateProfile.saleQuantityPlusY };
        case 9: return POINT{ g_CoordinateProfile.saleQuantityMinusX, g_CoordinateProfile.saleQuantityMinusY };
        case 10: return POINT{ g_CoordinateProfile.saleMaxPriceX, g_CoordinateProfile.saleMaxPriceY };
        case 11: return POINT{ g_CoordinateProfile.salePriceMinusX, g_CoordinateProfile.salePriceMinusY };
        default: return POINT{ g_CoordinateProfile.putOnSaleX, g_CoordinateProfile.putOnSaleY };
        }
    };
    POINT coord = selectedCoord();
    ImGui::Text("X %d / Y %d", coord.x, coord.y);
    ImGui::SameLine();
    if (ImGui::SmallButton("Tap")) {
        int inst = g_ClassicSelectedInstance;
        POINT tapCoord = selectedCoord();
        std::thread([inst, tapCoord]() {
            AdbTap(inst, tapCoord.x, tapCoord.y);
            AddLog(inst, "Tapped X " + std::to_string(tapCoord.x) + " / Y " + std::to_string(tapCoord.y), ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
            }).detach();
    }
    ImGui::EndChild();

    // === Column 2: Timing & Presets ===
    ImGui::NextColumn();
    ClassicBeginPanel("Timing & Presets", ImVec2(0, 0));
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Farm Cycle");
    bool timingChanged = false;
    ImGui::SetNextItemWidth(-120);
    timingChanged |= ImGui::SliderInt("Game Load (s)", &g_Intervals.gameLoadWait, 5, 45);
    ImGui::SetNextItemWidth(-120);
    timingChanged |= ImGui::SliderInt("Harvest (ms)", &g_Intervals.afterHarvestWait, 300, 5000);
    ImGui::SetNextItemWidth(-120);
    timingChanged |= ImGui::SliderInt("Plant (ms)", &g_Intervals.afterPlantWait, 300, 5000);
    ImGui::SetNextItemWidth(-120);
    timingChanged |= ImGui::SliderInt("Next Acc (ms)", &g_Intervals.nextAccountWait, 500, 5000);
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Shop Cycle");
    ImGui::SetNextItemWidth(-120);
    timingChanged |= ImGui::SliderInt("Shop (ms)", &g_Intervals.shopEnterWait, 500, 5000);
    ImGui::SetNextItemWidth(-120);
    timingChanged |= ImGui::SliderInt("Crate (ms)", &g_Intervals.crateClickWait, 300, 3000);
    ImGui::SetNextItemWidth(-120);
    timingChanged |= ImGui::SliderInt("Coins (ms)", &g_Intervals.coinCollectWait, 200, 2000);
    ImGui::SetNextItemWidth(-120);
    timingChanged |= ImGui::SliderInt("Product (ms)", &g_Intervals.productSelectWait, 200, 2000);
    ImGui::SetNextItemWidth(-120);
    timingChanged |= ImGui::SliderInt("Sale (ms)", &g_Intervals.createSaleWait, 300, 3000);
    if (timingChanged) SaveConfig();
    ImGui::Separator();
    if (ClassicButton("TURBO FARM", ImVec4(0.85f, 0.25f, 0.25f, 1.0f), ImVec2(-1, 28))) {
        g_Intervals.gameLoadWait = 10;
        g_Intervals.afterHarvestWait = 500;
        g_Intervals.afterPlantWait = 500;
        g_Intervals.nextAccountWait = 1000;
        g_Intervals.shopEnterWait = 1250;
        g_Intervals.crateClickWait = 500;
        g_Intervals.coinCollectWait = 310;
        g_Intervals.productSelectWait = 310;
        g_Intervals.createSaleWait = 560;
        SaveConfig();
    }
    if (ClassicButton("FAST SALE", ImVec4(0.25f, 0.62f, 0.88f, 1.0f), ImVec2(-1, 28))) {
        g_Intervals.shopEnterWait = 1200;
        g_Intervals.crateClickWait = 450;
        g_Intervals.coinCollectWait = 300;
        g_Intervals.productSelectWait = 250;
        g_Intervals.createSaleWait = 550;
        SaveConfig();
    }
    if (ClassicButton("DEFAULTS", ImVec4(0.5f, 0.5f, 0.5f, 1.0f), ImVec2(-1, 28))) {
        g_Intervals = IntervalSettings{};
        SaveConfig();
    }
    ImGui::Separator();
    if (ImGui::Checkbox("Zoom After Mailbox", &g_Intervals.autoZoomOutAfterMailbox)) SaveConfig();
    if (g_Intervals.autoZoomOutAfterMailbox) {
        ImGui::SetNextItemWidth(-120);
        if (ImGui::SliderInt("Zoom (ms)", &g_Intervals.mailboxZoomDelayMs, 0, 10000)) SaveConfig();
    }
    ImGui::EndChild();

    // === Column 3: General Settings + Paths ===
    ImGui::NextColumn();
    ClassicBeginPanel("General", ImVec2(0, 0));
    if (ImGui::Checkbox("Only sell if silo full", &g_OnlySellIfSiloFull)) SaveConfig();
    if (ImGui::Checkbox("Separate templates/instance", &g_SeparateTemplates)) SaveConfig();
    if (ImGui::Checkbox("Webhook with screenshot", &g_EnableWebhookImage)) SaveConfig();
    if (ImGui::Checkbox("Auto-Clear Logs", &g_AutoClearLogs)) {
        g_LastLogAutoClear = std::chrono::steady_clock::now();
        SaveConfig();
    }
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("Clear interval (min)", &g_AutoClearLogMinutes, 1, 10)) {
        g_AutoClearLogMinutes = (std::max)(1, (std::min)(1440, g_AutoClearLogMinutes));
        SaveConfig();
    }
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Paths");
    ImGui::TextUnformatted("ADB:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##adbpath", g_AdbPathBuf, IM_ARRAYSIZE(g_AdbPathBuf));
    if (ImGui::SmallButton("Browse##adb")) {
        std::string p = OpenFileDialog("Executable\0*.exe\0All Files\0*.*\0");
        if (!p.empty()) { strncpy(g_AdbPathBuf, p.c_str(), 260); SaveConfig(); ValidatePaths(); }
    }
    ImGui::TextUnformatted("Console:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##emupath", g_MEmuPathBuf, IM_ARRAYSIZE(g_MEmuPathBuf));
    if (ImGui::SmallButton("Browse##emu")) {
        std::string p = OpenFileDialog("Executable\0*.exe\0All Files\0*.*\0");
        if (!p.empty()) { strncpy(g_MEmuPathBuf, p.c_str(), 260); SaveConfig(); ValidatePaths(); }
    }
    ImGui::Text("ADB: %s  Console: %s", g_AdbValid ? "OK" : "MISSING", g_MEmuValid ? "OK" : "MISSING");
    if (ClassicButton("Validate Paths", ImVec4(0.34f, 0.72f, 0.34f, 1.0f), ImVec2(-1, 24))) {
        SaveConfig();
        ValidatePaths();
    }
    if (ImGui::SmallButton("Config Folder")) ShellExecuteA(0, "open", GetAppDataPath().c_str(), 0, 0, SW_SHOW);
    ImGui::SameLine();
    if (ImGui::SmallButton("Templates Folder")) ShellExecuteA(0, "open", "templates", 0, 0, SW_SHOW);
    ImGui::EndChild();

    ImGui::Columns(1);
}

static void RenderSidebar() {
    ImGui::BeginChild("Sidebar", ImVec2(178, 0), true);
    ImGui::TextColored(ImVec4(0.12f, 0.24f, 0.34f, 1.0f), "WORKFLOW");
    ImGui::Separator();
    ClassicNavItem("Start", ClassicPage_Start);
    ClassicNavItem("Accounts", ClassicPage_Accounts);
    ClassicNavItem("Farm", ClassicPage_Farm);
    ClassicNavItem("Templates", ClassicPage_Templates);
    ClassicNavItem("Logs", ClassicPage_Logs);
    ClassicNavItem("Settings", ClassicPage_Settings);
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.12f, 0.24f, 0.34f, 1.0f), "EMULATORS");
    ImGui::Separator();
    for (int i = 0; i < 6; ++i) {
        BotInstance& bot = g_Bots[i];
        ImGui::PushID(i);
        bool selected = (g_ClassicSelectedInstance == i);
        ImVec4 dotColor = bot.isRunning ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        if (bot.isRunning && bot.statusText.find("WAIT") != std::string::npos) dotColor = ImVec4(1.0f, 0.85f, 0.2f, 1.0f);

        if (ImGui::Selectable("##inst", selected, 0, ImVec2(0, 48))) {
            g_ClassicSelectedInstance = i;
            g_SelectedInstanceUI = i;
        }
        ImGui::SameLine(8.0f);
        ImGui::BeginGroup();
        ImGui::Text("Emu %d", i + 1);
        ImGui::SameLine();
        ImGui::TextColored(dotColor, "%s", bot.isRunning ? "RUN" : (bot.isActive ? "IDLE" : "OFF"));
        AccountSlot& acc = bot.accounts[(std::max)(0, (std::min)(MAX_ACCOUNT_SLOTS - 1, bot.currentAccountIndex))];
        std::string accLabel = acc.hasFile && !acc.farmName.empty() ? acc.farmName : "No Account";
        if (accLabel.length() > 14) accLabel = accLabel.substr(0, 12) + "..";
        ImGui::TextDisabled("S%d %s", bot.currentAccountIndex + 1, accLabel.c_str());
        std::string status = bot.isRunning ? bot.statusText : "Idle";
        if (status.length() > 18) status = status.substr(0, 16) + "..";
        ImGui::TextColored(dotColor, "%s", status.c_str());
        ImGui::EndGroup();
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::Spacing();
    if (ClassicButton("Save All", ImVec4(0.34f, 0.72f, 0.34f, 1.0f), ImVec2(-1, 26))) SaveConfig();
    ImGui::EndChild();
}

static void RenderClassicApp() {
    ValidatePaths();
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + 30.0f));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - 30.0f));

    ImGui::Begin("MainApp", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::TextUnformatted("Hay Day Bot v2.7.6");
    ImGui::SameLine(ImGui::GetWindowWidth() - 470.0f);
    ImGui::TextColored(ImVec4(0.10f, 0.45f, 0.10f, 1.0f), "Local build | Customer: %s | Runtime: %s", g_Username, GetRuntimeStr().c_str());

    float logStripHeight = 130.0f;
    float contentHeight = ImGui::GetContentRegionAvail().y - logStripHeight - 8.0f;

    // === TOP AREA: Sidebar + current workflow ===
    ImGui::BeginChild("TopArea", ImVec2(0, contentHeight), false);
    RenderSidebar();
    ImGui::SameLine();
    ImGui::BeginChild("MainContent", ImVec2(0, 0), false);
    if (g_ClassicPage < ClassicPage_Start || g_ClassicPage > ClassicPage_Settings) g_ClassicPage = ClassicPage_Start;
    ClassicDrawTopStatus();
    ImGui::Spacing();
    switch (g_ClassicPage) {
    case ClassicPage_Start:
        ClassicStartTab();
        break;
    case ClassicPage_Accounts:
        ClassicAccountManagerTab();
        break;
    case ClassicPage_Farm:
        ClassicFarmConfigTab();
        break;
    case ClassicPage_Templates:
        ClassicTemplateManagerTab();
        break;
    case ClassicPage_Logs:
        ClassicLogsTab();
        break;
    case ClassicPage_Settings:
        ClassicSettingsTab();
        break;
    default:
        ClassicStartTab();
        break;
    }
    ImGui::EndChild();
    ImGui::EndChild();

    // === BOTTOM: Log Strip ===
    ImGui::Separator();
    ImGui::BeginChild("LogStrip", ImVec2(0, logStripHeight), true);
    ImGui::Text("Activity");
    ImGui::SameLine(74);
    if (ImGui::SmallButton("Clear")) ClassicClearLogs();
    ImGui::SameLine();
    ImGui::Checkbox("Auto##scroll", &g_AutoScrollLogs);
    ImGui::SameLine();
    ClassicDrawLogFilterCombo(120.0f);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##logstripsearch", "Search logs", g_LogSearchBuf, IM_ARRAYSIZE(g_LogSearchBuf));
    ImGui::SameLine();
    if (ImGui::SmallButton("Full Logs")) g_ClassicPage = ClassicPage_Logs;
    ClassicDrawLogs(logStripHeight - 34.0f);
    ImGui::EndChild();

    ImGui::End();
}

void RenderApp() {
    RenderClassicApp();
    return;

    ValidatePaths();
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + 30.0f));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - 30.0f));

    ImGui::Begin("MainApp", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::BeginChild("Sidebar", ImVec2(270, 0), true, ImGuiWindowFlags_NoScrollbar);
    {
        if (icon_logo != 0) {
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 100) * 0.5f);
            ImGui::Image((void*)(intptr_t)icon_logo, ImVec2(100, 100));
        }
        else RenderLogo();

        if (icon_logo != 0) {
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 120) * 0.5f);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "PREMIUM v2.0");
        }
        ImGui::Spacing();

        ModernButton(Tr("DASHBOARD"), icon_dashboard, 0, g_CurrentTab);
        ModernButton("NOTES", icon_logs, 10, g_CurrentTab);
        ModernButton(Tr("BOT MANAGER"), icon_config, 1, g_CurrentTab);
        ModernButton(Tr("LOGS"), icon_logs, 4, g_CurrentTab);
        ModernButton(Tr("TEMPLATES"), icon_templates, 5, g_CurrentTab);
        ModernButton(Tr("REMOTE & WEBHOOK"), icon_cloud, 2, g_CurrentTab);

        GLuint settingsIconToUse = icon_settings;
        if (!g_AdbValid || !g_MEmuValid) settingsIconToUse = icon_warning;
        ModernButton(Tr("SETTINGS"), settingsIconToUse, 3, g_CurrentTab);
        ModernButton(Tr("HOW TO USE"), icon_question, 6, g_CurrentTab);
        ModernButton(Tr("BOT VISION"), icon_dashboard, 7, g_CurrentTab);
        ModernButton("EXPERIMENTAL", icon_warning, 8, g_CurrentTab);
        ModernButton("TEMPLATE NAMES", icon_templates, 9, g_CurrentTab);
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 120);
        ImGui::Separator();

        ImGui::SetNextItemWidth(180);
        ImGui::Combo("##Lang", &g_Language, "English\0Türkçe\0Español\0Português\0Русский\0Deutsch\0");
        if (ImGui::IsItemDeactivatedAfterEdit()) SaveConfig();

        ImGui::Spacing();
        int activeCount = 0;
        for (int i = 0; i < 6; i++) activeCount += (int)g_Bots[i].isActive;
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Active Instances: %d/6", activeCount);
        ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("User: %s"), g_Username);

    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("Content", ImVec2(0, 0), true);
    {
        if (g_CurrentTab == 0) {
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), "CONTROL DECK");
            ImGui::TextDisabled("A compact overview. Open Inventory or Diagnostics on a card only when you need the details.");
            ImGui::Separator(); ImGui::Spacing();

            auto CompactText = [](const std::string& text, size_t maxLen) {
                if (text.size() <= maxLen) return text;
                if (maxLen <= 3) return text.substr(0, maxLen);
                return text.substr(0, maxLen - 3) + "...";
                };

            auto NotePreview = [&](const std::string& note) {
                size_t firstBreak = note.find('\n');
                std::string firstLine = firstBreak == std::string::npos ? note : note.substr(0, firstBreak);
                return CompactText(firstLine, 72);
                };

            int activeCount = 0;
            int globalHarvests = 0, globalSales = 0, globalCoins = 0, globalDiamonds = 0;
            for (int i = 0; i < 6; i++) {
                if (g_Bots[i].isActive) {
                    activeCount++;
                    globalHarvests += g_Bots[i].totalHarvest;
                    globalSales += g_Bots[i].totalSales;
                    globalCoins += g_Bots[i].accounts[g_Bots[i].currentAccountIndex].coinAmount;
                    globalDiamonds += g_Bots[i].accounts[g_Bots[i].currentAccountIndex].diamondAmount;
                }
            }

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
            ImGui::BeginChild("GlobalStats", ImVec2(0, 74), true);
            ImGui::Columns(5, "GlobalCols", false);

            ImGui::TextDisabled("ACTIVE");
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%d / 6", activeCount);
            ImGui::NextColumn();

            ImGui::TextDisabled(Tr("RUNTIME"));
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", GetRuntimeStr().c_str());
            ImGui::NextColumn();

            ImGui::TextDisabled("HARVEST / SALES");
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%d / %d", globalHarvests, globalSales);
            ImGui::NextColumn();

            ImGui::TextDisabled(Tr("COINS"));
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%d", globalCoins);
            ImGui::NextColumn();

            ImGui::TextDisabled(Tr("DIAMONDS"));
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "%d", globalDiamonds);

            ImGui::Columns(1);
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            float dashboardWidth = ImGui::GetContentRegionAvail().x;
            int dashColumns = dashboardWidth > 1080.0f ? 3 : 2;
            ImGui::BeginChild("DashboardCards", ImVec2(0, 0), false);
            ImGui::Columns(dashColumns, "DashGrid", false);
            for (int i = 0; i < 6; i++) {
                ImGui::PushID(i);
                if (!g_Bots[i].isActive) ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.13f, 0.70f));
                else ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.12f, 0.10f, 1.0f));

                std::string cardId = "InstanceCard" + std::to_string(i);
                ImGui::BeginChild(cardId.c_str(), ImVec2(0, 232), true);

                ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), "Instance %d", i + 1);
                ImGui::SameLine();
                if (g_Bots[i].isActive) ImGui::TextColored(g_Bots[i].isRunning ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.65f, 0, 1), "%s", g_Bots[i].isRunning ? "RUNNING" : "READY");
                else ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "OFFLINE");
                ImGui::Separator();

                if (g_Bots[i].isActive) {
                    AccountSlot& activeAcc = g_Bots[i].accounts[g_Bots[i].currentAccountIndex];
                    InventoryData& activeInv = activeAcc.currentInv;
                    ImVec4 stColor = g_Bots[i].isRunning ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.5f, 0, 1);

                    std::string farmName = CompactText(activeAcc.farmName.empty() ? "Unknown Farm" : activeAcc.farmName, 34);
                    std::string slotName = CompactText(activeAcc.name.empty() ? "Unnamed Slot" : activeAcc.name, 34);
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "%s", farmName.c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled("Lvl %d", activeAcc.level);
                    ImGui::TextDisabled("Slot: %s", slotName.c_str());
                    ImGui::TextDisabled("Tag: %s", activeAcc.playerTag.c_str());

                    if (!activeAcc.note.empty()) {
                        ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.65f, 1.0f), "Note: %s", NotePreview(activeAcc.note).c_str());
                    }

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Coins: %d", activeAcc.coinAmount);
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "Diamonds: %d", activeAcc.diamondAmount);
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Harvests: %d | Sales: %d | Cycles: %d", g_Bots[i].totalHarvest, g_Bots[i].totalSales, activeAcc.cyclesSinceStart);
                    ImGui::TextColored(stColor, "Status: %s", Tr(g_Bots[i].statusText.c_str()));

                    if (ImGui::TreeNodeEx("Inventory", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Barn: Bolt %d | Tape %d | Plank %d", activeInv.bolt, activeInv.tape, activeInv.plank);
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Silo: Nail %d | Screw %d | Panel %d", activeInv.nail, activeInv.screw, activeInv.panel);
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Land: Deed %d | Mallet %d | Marker %d | Map %d", activeInv.deed, activeInv.mallet, activeInv.marker, activeInv.map);
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Mine: Dyn %d | TNT %d | Axe %d | Saw %d | Shovel %d", activeInv.dynamite, activeInv.tnt, activeInv.axe, activeInv.saw, activeInv.shovel);
                        ImGui::TreePop();
                    }

                    if (ImGui::TreeNodeEx("Diagnostics", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                        ImGui::TextDisabled("ADB: %s", g_Bots[i].adbSerial);
                        ImGui::TextDisabled("Farm: %s", activeAcc.farmFlowDetail.c_str());
                        ImGui::TextDisabled("Sale: %s", activeAcc.salesFlowDetail.c_str());
                        ImGui::PushTextWrapPos(0.0f);
                        ImGui::TextDisabled("HUD: %s", activeAcc.hudReadDetail.c_str());
                        ImGui::TextDisabled("Preserve OCR: %s", activeAcc.preserveReadDetail.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::TreePop();
                    }
                }
                else {
                    ImGui::TextDisabled("Enable this instance in Bot Manager.");
                    ImGui::TextDisabled("The dashboard now keeps inactive cards quiet.");
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
                ImGui::PopID();
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndChild();
        }

        if (g_CurrentTab == 10) {
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), "ACCOUNT NOTES");
            ImGui::Separator(); ImGui::Spacing();

            bool hasConfiguredAccounts = false;
            for (int i = 0; i < 6; i++) {
                bool instanceHasAccounts = false;
                for (int acc = 0; acc < MAX_ACCOUNT_SLOTS; acc++) {
                    if (g_Bots[i].accounts[acc].hasFile) {
                        instanceHasAccounts = true;
                        hasConfiguredAccounts = true;
                        break;
                    }
                }
                if (!instanceHasAccounts) continue;

                std::string instanceLabel = "INSTANCE #" + std::to_string(i + 1);
                if (g_Bots[i].isActive) instanceLabel += " [ONLINE]";

                if (ImGui::CollapsingHeader(instanceLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (int acc = 0; acc < MAX_ACCOUNT_SLOTS; acc++) {
                        AccountSlot& noteAcc = g_Bots[i].accounts[acc];
                        if (!noteAcc.hasFile) continue;

                        ImGui::PushID(i * 100 + acc);
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.10f, 1.0f));
                        ImGui::BeginChild("AccountNoteCard", ImVec2(0, 176), true);

                        const char* slotName = noteAcc.name.empty() ? "Unnamed Slot" : noteAcc.name.c_str();
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", slotName);
                        if (acc == g_Bots[i].currentAccountIndex) {
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[ACTIVE SLOT]");
                        }

                        if (!noteAcc.farmName.empty()) ImGui::TextDisabled("Farm: %s", noteAcc.farmName.c_str());
                        else ImGui::TextDisabled("Farm: Not detected yet");

                        if (noteAcc.level > 0) ImGui::TextDisabled("Level: %d", noteAcc.level);
                        else ImGui::TextDisabled("Level: -");

                        bool hasAdbTarget = g_Bots[i].isActive && strlen(g_Bots[i].adbSerial) > 0;
                        bool canLoadAccount = !g_Bots[i].isRunning && hasAdbTarget;
                        if (!canLoadAccount) ImGui::BeginDisabled();
                        if (ImGui::Button("LOAD TO EMULATOR", ImVec2(160.0f, 24.0f))) {
                            g_Bots[i].currentAccountIndex = acc;
                            std::thread([i, acc]() { LoadAccountFromSlot(i, acc); }).detach();
                        }
                        if (!canLoadAccount) ImGui::EndDisabled();
                        ImGui::SameLine();
                        if (!g_Bots[i].isRunning && !hasAdbTarget) ImGui::TextDisabled("Enable this instance and make sure it has an ADB serial first.");
                        else ImGui::TextDisabled(canLoadAccount ? "Switch this saved account into the emulator." : "Stop this bot before loading accounts.");

                        char noteBuf[512];
                        strncpy(noteBuf, noteAcc.note.c_str(), sizeof(noteBuf) - 1);
                        noteBuf[sizeof(noteBuf) - 1] = '\0';
                        if (ImGui::InputTextMultiline("##accountnote", noteBuf, sizeof(noteBuf), ImVec2(-1.0f, 70.0f))) {
                            noteAcc.note = noteBuf;
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit()) SaveConfig();

                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                        ImGui::PopID();
                        ImGui::Spacing();
                    }
                }
            }

            if (!hasConfiguredAccounts) {
                ImGui::TextDisabled("No configured accounts yet. Add or load an account in 'Bot Manager' first.");
            }
        }

        if (g_CurrentTab == 1) {
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("BOT INSTANCE MANAGER"));
            ImGui::Separator();
            if (ImGui::BeginTabBar("InstanceTabs")) {
                for (int i = 0; i < 6; i++) {
                    std::string tabName = std::string(Tr("Instance #")) + std::to_string(i + 1);

                   
                    ImGuiTabItemFlags tabFlags = 0;
                    if (g_TargetTabToSelect == i) {
                        tabFlags |= ImGuiTabItemFlags_SetSelected;
                        g_TargetTabToSelect = -1; 
                    }

                    if (ImGui::BeginTabItem(tabName.c_str(), nullptr, tabFlags)) {
                        g_SelectedInstanceUI = i;
                        ImGui::Spacing();
                        if (ImGui::Checkbox(Tr("Enable This Instance"), &g_Bots[i].isActive)) {
                            SaveConfig();
                        }
                        ImGui::Separator();

                        if (g_Bots[i].isActive) {
                            ImGui::Spacing();
                            BotInstance& bot = g_Bots[i];
                            int savedAccs = 0;
                            for (int slot = 0; slot < MAX_ACCOUNT_SLOTS; slot++) if (bot.accounts[slot].hasFile) savedAccs++;
                            if (g_MultiModeSkipBufSyncNeeded[i]) {
                                strncpy(g_MultiModeSkipBuf[i], bot.multiModeSkipSlots.c_str(), sizeof(g_MultiModeSkipBuf[i]) - 1);
                                g_MultiModeSkipBuf[i][sizeof(g_MultiModeSkipBuf[i]) - 1] = '\0';
                                g_MultiModeSkipBufSyncNeeded[i] = false;
                            }
                            auto skipMask = ParseMultiModeSkipMask(bot.multiModeSkipSlots);
                            auto buildRotationSummary = [&](const std::array<bool, MAX_ACCOUNT_SLOTS>& currentSkipMask, int& selectedCountOut) {
                                std::string summary = "Rotation slots: ";
                                selectedCountOut = 0;
                                for (int slot = 0; slot < MAX_ACCOUNT_SLOTS; ++slot) {
                                    if (!bot.accounts[slot].hasFile || IsMultiModeSlotSkipped(currentSkipMask, slot)) continue;
                                    if (selectedCountOut > 0) summary += ", ";
                                    summary += std::to_string(slot + 1);
                                    selectedCountOut++;
                                }
                                if (selectedCountOut == 0) summary += "none";
                                return summary;
                            };
                            int rotationSlotCount = 0;
                            std::string rotationSummary = buildRotationSummary(skipMask, rotationSlotCount);
                            bool instanceConfigChanged = false;

                            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), "INSTANCE CONTROL");
                            ImGui::SameLine();
                            ImGui::TextDisabled("Saved slots: %d/15 | Rotation: %d | Status: %s", savedAccs, rotationSlotCount, bot.isRunning ? "Running" : "Idle");
                            ImGui::Separator();

                            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.085f, 0.085f, 0.095f, 1.0f));
                            ImGui::Columns(3, "BotManagerCards", false);

                            ImGui::BeginChild("ModeCard", ImVec2(0, 260), true);
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "MODE");
                            ImGui::TextDisabled("Choose how this instance runs.");
                            ImGui::Separator();
                            if (ImGui::Checkbox("Single Mode", &bot.useSingleMode)) {
                                bot.useMultiMode = !bot.useSingleMode;
                                instanceConfigChanged = true;
                            }
                            bool multiLock = (savedAccs < 2);
                            if (multiLock) {
                                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                                if (bot.useMultiMode) {
                                    bot.useMultiMode = false;
                                    bot.useSingleMode = true;
                                    instanceConfigChanged = true;
                                }
                            }
                            if (ImGui::Checkbox("Multi Mode", &bot.useMultiMode)) {
                                bot.useSingleMode = !bot.useMultiMode;
                                instanceConfigChanged = true;
                            }
                            if (multiLock && ImGui::IsItemHovered()) ImGui::SetTooltip("Save at least 2 accounts before enabling rotation.");
                            if (multiLock) ImGui::PopStyleVar();
                            ImGui::Spacing();
                            ImGui::TextDisabled("Skip slots in multi mode");
                            ImGui::SetNextItemWidth(-1);
                            if (ImGui::InputTextWithHint("##multiskip", "e.g. 2, 5-7", g_MultiModeSkipBuf[i], IM_ARRAYSIZE(g_MultiModeSkipBuf[i]))) {
                                bot.multiModeSkipSlots = g_MultiModeSkipBuf[i];
                                skipMask = ParseMultiModeSkipMask(bot.multiModeSkipSlots);
                                rotationSummary = buildRotationSummary(skipMask, rotationSlotCount);
                                instanceConfigChanged = true;
                            }
                            ImGui::TextDisabled("%s", rotationSummary.c_str());
                            ImGui::Spacing();
                            if (ImGui::Checkbox("Account Run Limit", &bot.enableAccountRunLimit)) instanceConfigChanged = true;
                            if (bot.enableAccountRunLimit) {
                                ImGui::SetNextItemWidth(90);
                                if (ImGui::InputInt("Run Min", &bot.accountRunMinutes, 1, 5)) {
                                    bot.accountRunMinutes = (std::max)(1, (std::min)(1440, bot.accountRunMinutes));
                                    instanceConfigChanged = true;
                                }
                                ImGui::SetNextItemWidth(90);
                                if (ImGui::InputInt("Rest Min", &bot.accountRestMinutes, 1, 5)) {
                                    bot.accountRestMinutes = (std::max)(1, (std::min)(1440, bot.accountRestMinutes));
                                    instanceConfigChanged = true;
                                }
                                if (ImGui::Checkbox("Replace During Rest", &bot.replaceAccountDuringRest)) instanceConfigChanged = true;
                                ImGui::TextDisabled("Off: stay AFK. On: skip rested slots and load another saved account.");
                            }
                            if (bot.useSingleMode) {
                                ImGui::Spacing();
                                if (ImGui::Checkbox("Revive Mode", &bot.useReviveMode)) instanceConfigChanged = true;
                                if (bot.useReviveMode) {
                                    ImGui::SetNextItemWidth(130);
                                    if (ImGui::SliderInt("Check Sec", &bot.reviveCheckInterval, 60, 600)) instanceConfigChanged = true;
                                    ImGui::SetNextItemWidth(170);
                                    ImGui::InputText("Template##revive", bot.reviveTemplatePath, MAX_PATH);
                                    if (ImGui::IsItemDeactivatedAfterEdit()) instanceConfigChanged = true;
                                    ImGui::SameLine();
                                    if (ImGui::Button("Browse##revive")) {
                                        std::string path = OpenFileDialog();
                                        if (!path.empty()) {
                                            strncpy(bot.reviveTemplatePath, path.c_str(), MAX_PATH);
                                            instanceConfigChanged = true;
                                        }
                                    }
                                }
                            }
                            ImGui::EndChild();

                            ImGui::NextColumn();
                            ImGui::BeginChild("SetupCard", ImVec2(0, 196), true);
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "SETUP");
                            ImGui::TextDisabled("Connection, crop, and tools.");
                            ImGui::Separator();
                            ImGui::TextDisabled("ADB");
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(170);
                            ImGui::InputText("##adb", bot.adbSerial, 64);
                            if (ImGui::IsItemDeactivatedAfterEdit()) instanceConfigChanged = true;
                            ImGui::TextDisabled("Touch / VM");
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(115);
                            ImGui::InputText("##inputdev", bot.inputDevice, 64);
                            if (ImGui::IsItemDeactivatedAfterEdit()) instanceConfigChanged = true;
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(115);
                            ImGui::InputText("##vm", bot.vmName, 64);
                            if (ImGui::IsItemDeactivatedAfterEdit()) instanceConfigChanged = true;
                            if (i != 5) {
                                const char* crops[] = { Tr("Wheat (2m)"), Tr("Corn (5m)"), Tr("Carrot (10m)"), Tr("Soybean (20m)"), Tr("Sugarcane (30m)") };
                                ImGui::SetNextItemWidth(170);
                                if (ImGui::Combo("Crop Mode", &bot.testCropMode, crops, IM_ARRAYSIZE(crops))) instanceConfigChanged = true;
                                if (ImGui::Checkbox("Random Sale Cycle", &bot.enableRandomSaleCycle)) instanceConfigChanged = true;
                            }
                            else {
                                ImGui::SetNextItemWidth(170);
                                ImGui::InputText("Storage Tag", g_StorageTagBuf, IM_ARRAYSIZE(g_StorageTagBuf));
                                if (ImGui::IsItemDeactivatedAfterEdit()) {
                                    g_StorageTag = g_StorageTagBuf;
                                    SaveConfig();
                                }
                                ImGui::SetNextItemWidth(170);
                                ImGui::SliderInt("Transfer Threshold", &g_TransferThreshold, 3, 150);
                            }
                            ImGui::EndChild();

                            ImGui::NextColumn();
                            ImGui::BeginChild("ActionsCard", ImVec2(0, 196), true);
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "ACTIONS");
                            ImGui::TextDisabled("Launch, run, and maintenance.");
                            ImGui::Separator();
                            if (ImGui::Button(Tr("LAUNCH EMULATOR + HAY DAY"), ImVec2(-1, 32))) {
                                StartMEmuAndGame(i);
                            }
                            if (i != 5) {
                                ImGui::PushStyleColor(ImGuiCol_Button, bot.isRunning ? ImVec4(0.7f, 0.2f, 0.2f, 1.0f) : ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                                if (ImGui::Button(bot.isRunning ? Tr("STOP BOT") : Tr("START BOT"), ImVec2(-1, 44))) {
                                    if (bot.isRunning) {
                                        bot.isRunning = false;
                                        bot.statusText = "STOPPED";
                                        AddLog(i, "BOT Stopped by User.", ImVec4(1, 0.5f, 0.5f, 1));
                                    }
                                    else {
                                        bot.isRunning = true;
                                        bot.statusText = "STARTING...";
                                        AddLog(i, "BOT Started.", ImVec4(0, 1, 0, 1));
                                        std::thread([i]() { RunPremiumBot(i); }).detach();
                                    }
                                }
                                ImGui::PopStyleColor();
                                if (ImGui::Button("Inject Important Files", ImVec2(-1, 30))) {
                                    InjectImportantFiles(i);
                                }
                            }
                            else {
                                ImGui::PushStyleColor(ImGuiCol_Button, bot.isRunning ? ImVec4(0.5f, 0.2f, 0.5f, 1.0f) : ImVec4(0.6f, 0.2f, 0.8f, 1.0f));
                                if (ImGui::Button(bot.isRunning ? Tr("STOP AUTO TRANSFER") : Tr("START AUTO TRANSFER"), ImVec2(-1, 44))) {
                                    if (bot.isRunning) {
                                        bot.isRunning = false;
                                        bot.statusText = "TRANSFER STOPPED";
                                        AddLog(i, "Storage Auto-Transfer Mode Halted.", ImVec4(1, 0.5f, 0.5f, 1));
                                    }
                                    else {
                                        bot.isRunning = true;
                                        bot.statusText = "LISTENING FOR SIGNALS";
                                        AddLog(i, "Storage Auto-Transfer Activated. Listening on encrypted radio channel...", ImVec4(0.8f, 0.4f, 1.0f, 1.0f));
                                        std::thread([i]() { RunStorageMaster(i); }).detach();
                                    }
                                }
                                ImGui::PopStyleColor();
                            }
                            if (ImGui::Checkbox("Auto RAM Cleaner", &bot.enableJanitor)) instanceConfigChanged = true;
                            if (bot.enableJanitor) {
                                ImGui::SetNextItemWidth(170);
                                if (ImGui::SliderInt("Clean Every", &bot.janitorLimit, 3, 50)) instanceConfigChanged = true;
                            }
                            if (g_Bots[i].isCreatingEmulator) {
                                ImGui::BeginDisabled();
                                ImGui::Button(Tr("CREATING MEMU..."), ImVec2(-1, 28));
                                ImGui::EndDisabled();
                            }
                            else if (ImGui::Button(Tr("CREATE NEW MEMU & AUTO-LINK"), ImVec2(-1, 28))) {
                                g_Bots[i].isCreatingEmulator = true;
                                std::thread([i]() { RunEmulatorFactory(i); }).detach();
                            }
                            if (instanceConfigChanged) {
                                bot.multiModeSkipSlots = g_MultiModeSkipBuf[i];
                                SaveConfig();
                            }
                            ImGui::EndChild();

                            ImGui::Columns(1);
                            ImGui::PopStyleColor();
                            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                            
                            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("ACCOUNT MANAGER & INSPECTOR"));
                            ImGui::TextDisabled(Tr("Click on a slot to expand details, edit names, and manage saves."));
                            ImGui::Separator(); ImGui::Spacing();

                            
                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));

                            for (int acc = 0; acc < MAX_ACCOUNT_SLOTS; acc++) {
                                AccountSlot& currAcc = g_Bots[i].accounts[acc];
                                ImGui::PushID(acc); // PREVENT ID COLLISIONS

                                
                                std::string headerLabel = "SLOT " + std::to_string(acc + 1) + " : " + currAcc.name;
                                if (currAcc.hasFile) {
                                    headerLabel += " | " + (currAcc.farmName.empty() ? "Unknown" : currAcc.farmName) + " (Lvl " + std::to_string(currAcc.level) + ")";
                                    if (IsMultiModeSlotSkipped(skipMask, acc)) {
                                        headerLabel += " | [SKIP ROTATION]";
                                    }
                                }
                                else {
                                    headerLabel += " | [EMPTY]";
                                }
                                headerLabel += "###AccHeader_" + std::to_string(acc);
                               
                                bool isCurrent = (g_Bots[i].currentAccountIndex == acc);
                                if (isCurrent) {
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.65f, 0.12f, 1.0f));
                                }
                                else if (!currAcc.hasFile) {
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)); // GREY IF EMPTY | GREY FIVE NINE TO THE GRAVE
                                }

                                bool isOpen = ImGui::CollapsingHeader(headerLabel.c_str());

                                if (isCurrent || !currAcc.hasFile) ImGui::PopStyleColor(); 

                                if (isOpen) {
                                    ImGui::Indent(15.0f);
                                    ImGui::Spacing();

                                    ImGui::TextDisabled(Tr("Custom Name:"));
                                    ImGui::SameLine();
                                    char nameBuf[64];
                                    strncpy(nameBuf, currAcc.name.c_str(), sizeof(nameBuf));
                                    ImGui::PushItemWidth(250.0f);
                                    if (ImGui::InputText("##accname", nameBuf, sizeof(nameBuf))) {
                                        currAcc.name = nameBuf;
                                        SaveConfig();
                                    }
                                    if (ImGui::IsItemDeactivatedAfterEdit()) SaveConfig();
                                    ImGui::PopItemWidth();

                                    ImGui::SameLine(0, 30.0f);

                                
                                    if (!isCurrent) {
                                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                                        if (ImGui::Button(Tr("SET AS ACTIVE SLOT"), ImVec2(150, 25))) {
                                            g_Bots[i].currentAccountIndex = acc;
                                        }
                                        ImGui::PopStyleColor();
                                    }
                                    else {
                                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), Tr("[ CURRENTLY ACTIVE ]"));
                                    }

                                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                                    
                                    if (currAcc.hasFile) {
                                        ImGui::Columns(3, "AccDetailsCols", false);

                                        
                                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Tr("PROFILE"));
                                        ImGui::Text(Tr("Tag: %s"), currAcc.playerTag.c_str());
                                        if (icon_coin != 0) { ImGui::Image((void*)(intptr_t)icon_coin, ImVec2(14, 14)); ImGui::SameLine(); }
                                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%d", currAcc.coinAmount);
                                        if (icon_dia != 0) { ImGui::Image((void*)(intptr_t)icon_dia, ImVec2(14, 14)); ImGui::SameLine(); }
                                        ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "%d", currAcc.diamondAmount);
                                        ImGui::NextColumn();

                                      
                                        InventoryData& inv = currAcc.currentInv;
                                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), Tr("BARN"));
                                        ImGui::Text(Tr("Bolt: %d"), inv.bolt);
                                        ImGui::Text(Tr("Plank: %d"), inv.plank);
                                        ImGui::Text(Tr("Tape: %d"), inv.tape);
                                        ImGui::NextColumn();

                                      
                                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), Tr("SILO"));
                                        ImGui::Text(Tr("Nail: %d"), inv.nail);
                                        ImGui::Text(Tr("Screw: %d"), inv.screw);
                                        ImGui::Text(Tr("Panel: %d"), inv.panel);

                                        ImGui::Columns(1);
                                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "LAND  Deed: %d | Mallet: %d | Marker: %d | Map: %d", inv.deed, inv.mallet, inv.marker, inv.map);
                                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "MINE  Dynamite: %d | TNT: %d | Axe: %d | Saw: %d | Shovel: %d", inv.dynamite, inv.tnt, inv.axe, inv.saw, inv.shovel);
                                        ImGui::Spacing();
                                        bool canManualSync = !bot.isRunning;
                                        if (!canManualSync) ImGui::BeginDisabled();
                                        if (ImGui::Button("SYNC HUD", ImVec2(120, 28))) {
                                            std::thread([i, acc]() { SyncHudDataForSlot(i, acc); }).detach();
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::Button("SYNC BARN/SILO", ImVec2(150, 28))) {
                                            std::thread([i, acc]() { SyncBarnSiloDataForSlot(i, acc); }).detach();
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::Button("SYNC ALL", ImVec2(120, 28))) {
                                            std::thread([i, acc]() { SyncAllDataForSlot(i, acc); }).detach();
                                        }
                                        if (!canManualSync) ImGui::EndDisabled();
                                        if (!canManualSync && ImGui::IsItemHovered()) {
                                            ImGui::SetTooltip("Stop the bot before running a manual sync.");
                                        }
                                        ImGui::TextDisabled("Sync HUD refreshes coins, diamonds, and level from the current farm screen.");
                                        ImGui::TextDisabled("Sync Barn/Silo runs the market scan flow. Sync All runs both.");
                                        ImGui::PushTextWrapPos(0.0f);
                                        ImGui::TextDisabled("HUD Read: %s", currAcc.hudReadDetail.c_str());
                                        ImGui::TextDisabled("Preserve OCR: %s", currAcc.preserveReadDetail.c_str());
                                        ImGui::PopTextWrapPos();
                                        ImGui::Spacing();
                                        ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), "SALE SETTINGS");
                                        ImGui::PushItemWidth(220.0f);
                                        int sellCycles = currAcc.targetCyclesBeforeSale;
                                        if (ImGui::SliderInt("Sell every X cycles##sellcycles", &sellCycles, 0, 10)) {
                                            currAcc.targetCyclesBeforeSale = sellCycles;
                                            SaveConfig();
                                        }
                                        int emergencyWheatLimit = currAcc.emergencyWheatStackLimit;
                                        if (ImGui::SliderInt("Emergency Wheat Stack Limit##emergencywheat", &emergencyWheatLimit, 0, 20)) {
                                            currAcc.emergencyWheatStackLimit = emergencyWheatLimit;
                                            SaveConfig();
                                        }
                                        bool sellMax = currAcc.sellAtMaxPrice;
                                        if (ImGui::Checkbox("Use Max Price##sellmax", &sellMax)) {
                                            currAcc.sellAtMaxPrice = sellMax;
                                            SaveConfig();
                                        }
                                        const char* saleStackModes[] = { "Game Default", "Max Stack", "Fixed Stack", "Preserve OCR" };
                                        int saleStackMode = currAcc.saleStackMode;
                                        if (ImGui::Combo("Sale Stack Mode##salestackmode", &saleStackMode, saleStackModes, IM_ARRAYSIZE(saleStackModes))) {
                                            currAcc.saleStackMode = saleStackMode;
                                            SaveConfig();
                                        }
                                        int fixedSaleStack = currAcc.fixedSaleStack;
                                        if (currAcc.saleStackMode != 2) ImGui::BeginDisabled();
                                        if (ImGui::SliderInt("Fixed Sale Stack##fixedstack", &fixedSaleStack, 1, 10)) {
                                            currAcc.fixedSaleStack = fixedSaleStack;
                                            SaveConfig();
                                        }
                                        if (currAcc.saleStackMode != 2) ImGui::EndDisabled();
                                        int keepWheat = currAcc.keepWheatReserve;
                                        if (currAcc.saleStackMode != 3) ImGui::BeginDisabled();
                                        if (ImGui::SliderInt("Keep Wheat Reserve##keepwheat", &keepWheat, 0, 100)) {
                                            currAcc.keepWheatReserve = keepWheat;
                                            SaveConfig();
                                        }
                                        if (currAcc.saleStackMode != 3) ImGui::EndDisabled();
                                        ImGui::TextDisabled("Preserve OCR uses the wheat sale count and keeps this many wheat in storage before listing more. If OCR fails, it sells full stacks for that pass.");
                                        ImGui::TextDisabled("Emergency Wheat Stack Limit only applies during SILO FULL recovery. 0 means unlimited emergency wheat sales.");
                                        bool autoCowFeed = currAcc.autoCowFeedEnabled;
                                        if (ImGui::Checkbox("Auto Cow Feed##autocowfeed", &autoCowFeed)) {
                                            currAcc.autoCowFeedEnabled = autoCowFeed;
                                            SaveConfig();
                                        }
                                        bool cowFeedFourSlots = currAcc.cowFeedUseFourSlots;
                                        if (ImGui::Checkbox("Feed mill has 4 slots##cowfeedslots", &cowFeedFourSlots)) {
                                            currAcc.cowFeedUseFourSlots = cowFeedFourSlots;
                                            SaveConfig();
                                        }
                                        ImGui::TextDisabled("When enabled, this slot queues cow feed and feeds the cow pasture at most once per hour. Use 4 slots only on accounts whose feed mill has the extra slot.");
                                        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                                        ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), "AUTO TOM");
                                        ImGui::TextDisabled("Runs before harvest when Tom is active, has hours left, and a target item is set.");
                                        if (ImGui::Checkbox("Enable Auto Tom##slotautotom", &currAcc.autoTomEnabled)) {
                                            SaveConfig();
                                        }
                                        int tomHours = currAcc.tomRemainingHours;
                                        if (ImGui::InputInt("Remaining Hours##tomhours", &tomHours)) {
                                            currAcc.tomRemainingHours = (std::max)(0, tomHours);
                                            SaveConfig();
                                        }
                                        auto tomNow = std::chrono::duration_cast<std::chrono::seconds>(
                                            std::chrono::system_clock::now().time_since_epoch()).count();
                                        auto formatTomDelay = [](long long seconds) -> std::string {
                                            seconds = (std::max)(0LL, seconds);
                                            long long hours = seconds / 3600;
                                            long long minutes = (seconds % 3600) / 60;
                                            if (hours > 0) return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
                                            return std::to_string((seconds + 59) / 60) + "m";
                                            };
                                        if (currAcc.nextTomTime > tomNow) {
                                            ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), "Next Tom run: in %s", formatTomDelay(currAcc.nextTomTime - tomNow).c_str());
                                        }
                                        else {
                                            ImGui::TextDisabled("Next Tom run: ready now.");
                                        }
                                        static int tomDelayHours = 2;
                                        ImGui::SetNextItemWidth(130.0f);
                                        if (ImGui::InputInt("Start Delay Hours##tomdelayhours", &tomDelayHours)) {
                                            tomDelayHours = (std::max)(0, (std::min)(72, tomDelayHours));
                                        }
                                        if (ImGui::Button("START IN 2H##tom2h", ImVec2(118, 26))) {
                                            currAcc.nextTomTime = tomNow + (2LL * 3600LL);
                                            SaveConfig();
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::Button("SCHEDULE DELAY##tomdelay", ImVec2(150, 26))) {
                                            currAcc.nextTomTime = tomNow + (static_cast<long long>(tomDelayHours) * 3600LL);
                                            SaveConfig();
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::Button("START NOW##tomnow", ImVec2(105, 26))) {
                                            currAcc.nextTomTime = 0;
                                            SaveConfig();
                                        }
                                        const char* tomCategories[] = { Tr("Barn"), Tr("Silo") };
                                        int tomCategory = currAcc.tomCategory;
                                        if (ImGui::Combo("Search Category##tomcat", &tomCategory, tomCategories, IM_ARRAYSIZE(tomCategories))) {
                                            currAcc.tomCategory = (std::max)(0, (std::min)(1, tomCategory));
                                            SaveConfig();
                                        }
                                        char tomItemBuf[64];
                                        strncpy(tomItemBuf, currAcc.tomItemName.c_str(), sizeof(tomItemBuf) - 1);
                                        tomItemBuf[sizeof(tomItemBuf) - 1] = '\0';
                                        if (ImGui::InputText("Target Item##tomitem", tomItemBuf, sizeof(tomItemBuf))) {
                                            currAcc.tomItemName = tomItemBuf;
                                        }
                                        if (ImGui::IsItemDeactivatedAfterEdit()) SaveConfig();
                                        if (ImGui::Button("RESET TOM TIMER", ImVec2(150, 26))) {
                                            currAcc.tomStartTime = 0;
                                            currAcc.nextTomTime = 0;
                                            SaveConfig();
                                        }
                                        ImGui::SameLine();
                                        ImGui::TextDisabled("Use this after changing contracts or target item.");
                                        ImGui::PopItemWidth();
                                    }
                                    else {
                                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), Tr("EMPTY SLOT - NO DATA SAVED YET."));
                                        ImGui::TextDisabled(Tr("Enter game, skip tutorial, reach level 7 and click 'SAVE GAME' below."));
                                    }

                                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                                    
                                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.50f, 0.10f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.60f, 0.20f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.40f, 0.00f, 1.0f));
                                    if (ImGui::Button(Tr("SAVE GAME TO THIS SLOT"), ImVec2(250, 40))) {
                                        std::thread([i, acc]() { SaveAccountToSlot(i, acc); }).detach();
                                    }
                                    ImGui::PopStyleColor(3);

                                    ImGui::SameLine();

                                    
                                    bool canManualLoad = currAcc.hasFile && g_Bots[i].isActive && strlen(g_Bots[i].adbSerial) > 0;
                                    if (!canManualLoad) ImGui::BeginDisabled();
                                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.85f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.70f, 0.95f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.50f, 0.75f, 1.0f));
                                    if (ImGui::Button(Tr("LOAD SLOT TO EMULATOR"), ImVec2(250, 40))) {
                                        std::thread([i, acc]() { LoadAccountFromSlot(i, acc); }).detach();
                                    }
                                    ImGui::PopStyleColor(3);
                                    if (!canManualLoad) ImGui::EndDisabled();

                                    ImGui::Spacing(); ImGui::Spacing();
                                    ImGui::Unindent(15.0f);
                                }
                                ImGui::PopID();
                            }
                            ImGui::PopStyleColor(3); 
                        }
                        else {
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), Tr("Instance is disabled."));
                        }
                        ImGui::EndTabItem();
                    }
                }

                if (ImGui::BeginTabItem(Tr("Account Management"))) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("TRANSFER ACCOUNTS BETWEEN INSTANCES"));
                    ImGui::TextDisabled(Tr("Move an account data file from one slot to another."));
                    ImGui::Separator(); ImGui::Spacing();

                    static int swpSrcInst = 0, swpSrcSlot = 0;
                    static int swpDstInst = 1, swpDstSlot = 0;

                    const char* instItems[] = {
     Tr("Instance 1"), Tr("Instance 2"), Tr("Instance 3"),
     Tr("Instance 4"), Tr("Instance 5"), Tr("Instance 6 (STORAGE)")
                    };
                    const char* slotItems[] = {
                        Tr("Slot 1"), Tr("Slot 2"), Tr("Slot 3"), Tr("Slot 4"), Tr("Slot 5"),
                        Tr("Slot 6"), Tr("Slot 7"), Tr("Slot 8"), Tr("Slot 9"), Tr("Slot 10"),
                        Tr("Slot 11"), Tr("Slot 12"), Tr("Slot 13"), Tr("Slot 14"), Tr("Slot 15")
                    };

                    ImGui::Columns(2, "SwapperCols", false);
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), Tr("SOURCE (From)"));
                    ImGui::Combo(Tr("Instance##src"), &swpSrcInst, instItems, IM_ARRAYSIZE(instItems));
                    ImGui::Combo(Tr("Slot##src"), &swpSrcSlot, slotItems, IM_ARRAYSIZE(slotItems));

                    ImGui::NextColumn();
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), Tr("DESTINATION (To)"));
                    ImGui::Combo(Tr("Instance##dst"), &swpDstInst, instItems, IM_ARRAYSIZE(instItems));
                    ImGui::Combo(Tr("Slot##dst"), &swpDstSlot, slotItems, IM_ARRAYSIZE(slotItems));
                    ImGui::Columns(1);

                    ImGui::Spacing();
                    if (ImGui::Button(Tr("MOVE ACCOUNT"), ImVec2(200, 40))) {
                        if (swpSrcInst == swpDstInst && swpSrcSlot == swpDstSlot) {
                            AddLog(-1, Tr("Swapper Error: Source and Destination cannot be the same!"), ImVec4(1, 0, 0, 1));
                        }
                        else {
                            std::string srcFile = GetAppDataPath() + "\\Backups\\Instance_" + std::to_string(swpSrcInst) + "\\account_" + std::to_string(swpSrcSlot + 1) + ".nxrth";
                            std::string dstFolder = GetAppDataPath() + "\\Backups\\Instance_" + std::to_string(swpDstInst);
                            std::string dstFile = dstFolder + "\\account_" + std::to_string(swpDstSlot + 1) + ".nxrth";

                            if (!fs::exists(srcFile)) {
                                AddLog(-1, Tr("Swapper Error: Source slot is empty!"), ImVec4(1, 0, 0, 1));
                            }
                            else {
                                if (!fs::exists(dstFolder)) fs::create_directories(dstFolder);
                                try {
                                    fs::copy_file(srcFile, dstFile, fs::copy_options::overwrite_existing);
                                    fs::remove(srcFile);
                                    g_Bots[swpSrcInst].accounts[swpSrcSlot].hasFile = false;
                                    g_Bots[swpSrcInst].accounts[swpSrcSlot].fileName = "";
                                    g_Bots[swpDstInst].accounts[swpDstSlot].hasFile = true;
                                    g_Bots[swpDstInst].accounts[swpDstSlot].fileName = dstFile;
                                    AddLog(-1, Tr("Account successfully moved!"), ImVec4(0, 1, 0, 1));
                                }
                                catch (const std::exception& e) {
                                    AddLog(-1, std::string("Swapper Error: ") + e.what(), ImVec4(1, 0, 0, 1));
                                }
                            }
                        }
                    }

                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("CREATE NEW ACCOUNT (WIPE GAME DATA)"));
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), Tr("BEWARE! This will delete current game data. Make sure you saved the account!"));
                    ImGui::Separator(); ImGui::Spacing();

                    static int wipeInst = 0;
                    ImGui::PushItemWidth(200);
                    ImGui::Combo("##wipe", &wipeInst, instItems, IM_ARRAYSIZE(instItems));
                    ImGui::PopItemWidth();

                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));
                    if (ImGui::Button(Tr("WIPE DATA & CREATE NEW"), ImVec2(250, 40))) {
                        int targetInst = wipeInst;
                        std::thread([targetInst]() {
                            AddLog(targetInst, Tr("Wiping game data to create new account..."), ImVec4(1, 0.5f, 0, 1));
                            RunAdbCommand(targetInst, "shell am force-stop com.supercell.hayday");
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            RunAdbCommand(targetInst, "shell rm /data/data/com.supercell.hayday/shared_prefs/storage.xml");
                            RunAdbCommand(targetInst, "shell rm /data/data/com.supercell.hayday/shared_prefs/storage_new.xml");
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            AddLog(targetInst, Tr("Data wiped successfully! Launch game to start fresh."), ImVec4(0, 1, 0, 1));
                            }).detach();
                    }
                    ImGui::PopStyleColor(3);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(Tr("Auto Tom Config"))) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("AUTOMATED TOM MANAGER"));
                    ImGui::TextDisabled(Tr("Tom will be triggered every 2 hours if contract is active."));
                    ImGui::Separator(); ImGui::Spacing();

                    static int tomInst = 0, tomSlot = 0;
                    const char* instItems[] = { Tr("Instance 1"), Tr("Instance 2"), Tr("Instance 3"), Tr("Instance 4"), Tr("Instance 5"), Tr("Instance 6 (STORAGE)") };
                    const char* slotItems[] = {
                        Tr("Slot 1"), Tr("Slot 2"), Tr("Slot 3"), Tr("Slot 4"), Tr("Slot 5"),
                        Tr("Slot 6"), Tr("Slot 7"), Tr("Slot 8"), Tr("Slot 9"), Tr("Slot 10"),
                        Tr("Slot 11"), Tr("Slot 12"), Tr("Slot 13"), Tr("Slot 14"), Tr("Slot 15")
                    };

                    ImGui::Combo(Tr("Instance##tom"), &tomInst, instItems, IM_ARRAYSIZE(instItems));
                    ImGui::Combo(Tr("Slot##tom"), &tomSlot, slotItems, IM_ARRAYSIZE(slotItems));
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                    AccountSlot& tAcc = g_Bots[tomInst].accounts[tomSlot];
                    if (ImGui::Checkbox(Tr("Enable Auto Tom For This Slot"), &tAcc.autoTomEnabled)) {
                        SaveConfig();
                    }
                    ImGui::Spacing();

                    ImGui::SetNextItemWidth(150);
                    int tabTomHours = tAcc.tomRemainingHours;
                    if (ImGui::InputInt(Tr("Remaining Hours"), &tabTomHours)) {
                        tAcc.tomRemainingHours = (std::max)(0, tabTomHours);
                        SaveConfig();
                    }
                    ImGui::SetNextItemWidth(150);
                    int tabTomMinutes = tAcc.tomRemainingMinutes;
                    if (ImGui::InputInt("Remaining Minutes", &tabTomMinutes)) {
                        tAcc.tomRemainingMinutes = (std::max)(0, (std::min)(59, tabTomMinutes));
                        SaveConfig();
                    }
                    ImGui::TextDisabled(Tr("Enter how much time is left on Tom's contract."));

                    auto tomNow = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    auto formatTomDelay = [](long long seconds) -> std::string {
                        seconds = (std::max)(0LL, seconds);
                        long long hours = seconds / 3600;
                        long long minutes = (seconds % 3600) / 60;
                        if (hours > 0) return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
                        return std::to_string((seconds + 59) / 60) + "m";
                        };
                    if (tAcc.nextTomTime > tomNow) {
                        ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), "Next Tom run: in %s", formatTomDelay(tAcc.nextTomTime - tomNow).c_str());
                    }
                    else {
                        ImGui::TextDisabled("Next Tom run: ready now.");
                    }
                    static int tabTomDelayHours = 2;
                    static int tabTomDelayMinutes = 120;
                    ImGui::SetNextItemWidth(150);
                    if (ImGui::InputInt("Start Delay Hours##tabtomdelayhours", &tabTomDelayHours)) {
                        tabTomDelayHours = (std::max)(0, (std::min)(72, tabTomDelayHours));
                    }
                    ImGui::SetNextItemWidth(150);
                    if (ImGui::InputInt("Start Delay Minutes##tabtomdelayminutes", &tabTomDelayMinutes)) {
                        tabTomDelayMinutes = (std::max)(0, (std::min)(4320, tabTomDelayMinutes));
                    }
                    if (ImGui::Button("START IN 2H##tabtom2h", ImVec2(118, 26))) {
                        tAcc.nextTomTime = tomNow + (2LL * 3600LL);
                        SaveConfig();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("SCHEDULE DELAY##tabtomdelay", ImVec2(150, 26))) {
                        long long delaySeconds =
                            (static_cast<long long>(tabTomDelayHours) * 3600LL) +
                            (static_cast<long long>(tabTomDelayMinutes) * 60LL);
                        tAcc.nextTomTime = tomNow + delaySeconds;
                        SaveConfig();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("START NOW##tabtomnow", ImVec2(105, 26))) {
                        tAcc.nextTomTime = 0;
                        SaveConfig();
                    }

                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(150);
                    const char* catItems[] = { Tr("Barn"), Tr("Silo") };
                    int tabTomCategory = tAcc.tomCategory;
                    if (ImGui::Combo(Tr("Search Category"), &tabTomCategory, catItems, IM_ARRAYSIZE(catItems))) {
                        tAcc.tomCategory = (std::max)(0, (std::min)(1, tabTomCategory));
                        SaveConfig();
                    }

                    ImGui::Spacing();
                    char itemBuf[64];
                    strncpy(itemBuf, tAcc.tomItemName.c_str(), sizeof(itemBuf) - 1);
                    itemBuf[sizeof(itemBuf) - 1] = '\0';
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::InputText("Target Item##itemsearch", itemBuf, IM_ARRAYSIZE(itemBuf))) {
                        tAcc.tomItemName = itemBuf;
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        SaveConfig();
                    }

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), Tr("Current Target Item: [%s]"), tAcc.tomItemName.empty() ? "NONE" : tAcc.tomItemName.c_str());

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(Tr("Farm Inspector"))) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("LIVE FARM OVERVIEW & INVENTORY"));
                    ImGui::TextDisabled(Tr("Select an instance and slot to view real-time AI-extracted data."));
                    ImGui::Separator(); ImGui::Spacing();

                    static int infoInst = 0, infoSlot = 0;
                    const char* instItems[] = { Tr("Instance 1"), Tr("Instance 2"), Tr("Instance 3"), Tr("Instance 4"), Tr("Instance 5"), Tr("Instance 6 (STORAGE)") };
                    const char* slotItems[] = { Tr("Slot 1"), Tr("Slot 2"), Tr("Slot 3"), Tr("Slot 4"), Tr("Slot 5") };

                    ImGui::PushItemWidth(150);
                    ImGui::Combo(Tr("Instance##info"), &infoInst, instItems, IM_ARRAYSIZE(instItems));
                    ImGui::SameLine();
                    ImGui::Combo(Tr("Slot##info"), &infoSlot, slotItems, IM_ARRAYSIZE(slotItems));
                    ImGui::PopItemWidth();
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                    AccountSlot& viewAcc = g_Bots[infoInst].accounts[infoSlot];

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.15f, 0.12f, 1.0f));
                    ImGui::BeginChild("ID_Card", ImVec2(0, 110), true);

                    ImGui::SetWindowFontScale(1.5f);
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "[%d] %s", viewAcc.level, viewAcc.farmName.c_str());
                    ImGui::SetWindowFontScale(1.0f);

                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), Tr("Player Tag: "));
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", viewAcc.playerTag.c_str());
                    ImGui::Spacing();

                    if (icon_coin != 0) {
                        ImGui::Image((void*)(intptr_t)icon_coin, ImVec2(16.0f, 16.0f));
                        ImGui::SameLine();
                    }
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), Tr("Coins: %d"), viewAcc.coinAmount);

                    ImGui::SameLine(200);

                    if (icon_dia != 0) {
                        ImGui::Image((void*)(intptr_t)icon_dia, ImVec2(16.0f, 16.0f));
                        ImGui::SameLine();
                    }
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), Tr("Diamonds: %d"), viewAcc.diamondAmount);

                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::Spacing();

                    InventoryData& curr = viewAcc.currentInv;
                    InventoryData& prev = viewAcc.previousInv;

                    auto formatDiff = [](int c, int p) -> std::string {
                        int d = c - p;
                        if (d > 0) return " (+" + std::to_string(d) + ")";
                        if (d < 0) return " (" + std::to_string(d) + ")";
                        return " (+0)";
                        };

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
                    ImGui::BeginChild("BarnDataDisplay", ImVec2(0, 0), true);
                    ImGui::Columns(4, "BarnDataCols", false);

                    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("[ BARN EXPANSION ]"));
                    ImGui::Separator();
                    ImGui::Text(Tr("Bolt: %d %s"), curr.bolt, formatDiff(curr.bolt, prev.bolt).c_str());
                    ImGui::Text(Tr("Plank: %d %s"), curr.plank, formatDiff(curr.plank, prev.plank).c_str());
                    ImGui::Text(Tr("Tape: %d %s"), curr.tape, formatDiff(curr.tape, prev.tape).c_str());
                    ImGui::NextColumn();

                    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("[ SILO EXPANSION ]"));
                    ImGui::Separator();
                    ImGui::Text(Tr("Nail: %d %s"), curr.nail, formatDiff(curr.nail, prev.nail).c_str());
                    ImGui::Text(Tr("Screw: %d %s"), curr.screw, formatDiff(curr.screw, prev.screw).c_str());
                    ImGui::Text(Tr("Panel: %d %s"), curr.panel, formatDiff(curr.panel, prev.panel).c_str());
                    ImGui::NextColumn();

                    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("[ LAND EXPANSION ]"));
                    ImGui::Separator();
                    ImGui::Text(Tr("Deed: %d %s"), curr.deed, formatDiff(curr.deed, prev.deed).c_str());
                    ImGui::Text(Tr("Mallet: %d %s"), curr.mallet, formatDiff(curr.mallet, prev.mallet).c_str());
                    ImGui::Text(Tr("Marker: %d %s"), curr.marker, formatDiff(curr.marker, prev.marker).c_str());
                    ImGui::Text(Tr("Map: %d %s"), curr.map, formatDiff(curr.map, prev.map).c_str());
                    ImGui::NextColumn();

                    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), "[ MINE TOOLS ]");
                    ImGui::Separator();
                    ImGui::Text("Dynamite: %d %s", curr.dynamite, formatDiff(curr.dynamite, prev.dynamite).c_str());
                    ImGui::Text("TNT: %d %s", curr.tnt, formatDiff(curr.tnt, prev.tnt).c_str());
                    ImGui::Text("Axe: %d %s", curr.axe, formatDiff(curr.axe, prev.axe).c_str());
                    ImGui::Text("Saw: %d %s", curr.saw, formatDiff(curr.saw, prev.saw).c_str());
                    ImGui::Text("Shovel: %d %s", curr.shovel, formatDiff(curr.shovel, prev.shovel).c_str());

                    ImGui::Columns(1);
                    ImGui::EndChild();
                    ImGui::PopStyleColor();

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        if (g_CurrentTab == 4) {
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("SYSTEM EVENT LOGS"));
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button(Tr("CLEAR LOGS"), ImVec2(100, 30))) {
                std::lock_guard<std::mutex> lock(g_LogMutex);
                std::vector<LogEntry>().swap(g_GlobalLogs);
            }
            ImGui::SameLine();
            ImGui::Checkbox(Tr("Auto-Scroll"), &g_AutoScrollLogs);
            ImGui::SameLine();
            bool autoClearLogs = g_AutoClearLogs;
            if (ImGui::Checkbox("Auto-Clear Logs", &autoClearLogs)) {
                g_AutoClearLogs = autoClearLogs;
                g_LastLogAutoClear = std::chrono::steady_clock::now();
                SaveConfig();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            int autoClearMinutes = g_AutoClearLogMinutes;
            if (ImGui::InputInt("min##autoclearlogsminutes", &autoClearMinutes, 1, 10)) {
                g_AutoClearLogMinutes = (std::max)(1, (std::min)(1440, autoClearMinutes));
                SaveConfig();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(220.0f);
            ImGui::InputTextWithHint("##logsearch", "Search logs", g_LogSearchBuf, IM_ARRAYSIZE(g_LogSearchBuf));
            ImGui::SameLine();
            if (ImGui::Button("OPEN TIMING CSV", ImVec2(150, 30))) {
                std::string timingPath = GetActionTimingLogPath();
                if (EnsureActionTimingLogFile()) {
                    ShellExecuteA(nullptr, "open", timingPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
                else {
                    AddLog(-1, "Failed to create or open action timing CSV.", ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
                }
            }
            ImGui::TextDisabled("Action timing file: %s", GetActionTimingLogPath().c_str());

            ImGui::Spacing();
            ImGui::Text(Tr("Filter:")); ImGui::SameLine();

            auto FilterButton = [&](const char* label, int id) {
                bool isSelected = (g_LogFilter == id);
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.12f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
                }
                if (ImGui::Button(label, ImVec2(60, 25))) g_LogFilter = id;
                if (isSelected) ImGui::PopStyleColor(2);
                ImGui::SameLine();
                };

            FilterButton(Tr("ALL"), -1);
            FilterButton(Tr("SYSTEM"), -2);
            FilterButton(Tr("INST #1"), 0);
            FilterButton(Tr("INST #2"), 1);
            FilterButton(Tr("INST #3"), 2);
            FilterButton(Tr("INST #4"), 3);
            FilterButton(Tr("INST #5"), 4); 
            FilterButton(Tr("INST #6"), 5);
            ImGui::Separator();

            {
                std::lock_guard<std::mutex> lock(g_LogMutex);
                std::array<std::string, 7> latestWarnings;
                for (auto it = g_GlobalLogs.rbegin(); it != g_GlobalLogs.rend(); ++it) {
                    if (!it->warningLike) continue;
                    int slot = (it->instanceId == -1) ? 0 : (it->instanceId + 1);
                    if (slot < 0 || slot >= static_cast<int>(latestWarnings.size()) || !latestWarnings[slot].empty()) continue;
                    std::string owner = (it->instanceId == -1) ? "SYSTEM" : ("INST " + std::to_string(it->instanceId + 1));
                    latestWarnings[slot] = owner + " [" + it->category + "] " + it->message;
                }

                bool hasWarnings = false;
                for (const auto& warning : latestWarnings) {
                    if (!warning.empty()) { hasWarnings = true; break; }
                }
                if (hasWarnings) {
                    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "LATEST WARNINGS");
                    for (const auto& warning : latestWarnings) {
                        if (!warning.empty()) ImGui::TextWrapped("%s", warning.c_str());
                    }
                    ImGui::Separator();
                }
            }

            ImGui::BeginChild("LogRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
            {
                bool shouldStickToBottom = g_AutoScrollLogs && (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f);
                std::lock_guard<std::mutex> lock(g_LogMutex);
                for (const auto& log : g_GlobalLogs) {
                    bool matchesInstance = (g_LogFilter == -1) ||
                        (g_LogFilter == -2 && log.instanceId == -1) ||
                        (g_LogFilter >= 0 && g_LogFilter == log.instanceId);

                    std::string prefix = (log.instanceId == -1) ? Tr("[SYSTEM]") : std::string(Tr("[INST ")) + std::to_string(log.instanceId + 1) + "]";
                    std::string categoryBadge = "[" + log.category + "]";
                    std::string fullText = categoryBadge + " " + prefix + " " + log.message;
                    if (matchesInstance && ContainsCaseInsensitive(fullText, g_LogSearchBuf)) {
                        std::string repeatSuffix;
                        if (log.repeatCount > 1) {
                            repeatSuffix = " [x" + std::to_string(log.repeatCount) + "]";
                        }
                        ImGui::TextColored(log.color, "[%s] %s %s %s%s", log.timestamp.c_str(), categoryBadge.c_str(), prefix.c_str(), Tr(log.message.c_str()), repeatSuffix.c_str());
                    }
                }
                if (shouldStickToBottom) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }

        if (g_CurrentTab == 5) {
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("TEMPLATE CONFIGURATION & MAKER"));
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::BeginTabBar("TemplateTabBar")) {
                if (ImGui::BeginTabItem(Tr("Configuration"))) {
                    ImGui::Spacing();
                    ImGui::Checkbox(Tr("Separate all templates for each instance"), &g_SeparateTemplates);
                    ImGui::TextDisabled(Tr("(Checked = Use templates_0, templates_1...)"));

                    ImGui::Spacing();
                    ImGui::BeginChild("TemplatesScroll", ImVec2(0, 0), false);
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Tr("GAME CHECKERS"));
                    ImGui::Separator();
                    RenderTemplateRow(Tr("Mailbox"), g_mailboxPathBuf, IM_ARRAYSIZE(g_mailboxPathBuf), mailbox_templatePath, &g_Thresholds.mailboxThreshold);
                    RenderTemplateRow("Loading", g_loadingPathBuf, IM_ARRAYSIZE(g_loadingPathBuf), loading_templatePath, &g_Thresholds.loadingThreshold);
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Tr("FARMING TEMPLATES"));
                    ImGui::Separator();
                    RenderTemplateRow(Tr("Field"), g_fieldPathBuf, IM_ARRAYSIZE(g_fieldPathBuf), f_templatePath, &g_Thresholds.soilThreshold);
                    RenderTemplateRow(Tr("Wheat"), g_wheatPathBuf, IM_ARRAYSIZE(g_wheatPathBuf), w_templatePath, &g_Thresholds.wheatThreshold);
                    RenderTemplateRow(Tr("Sickle"), g_sicklePathBuf, IM_ARRAYSIZE(g_sicklePathBuf), s_templatePath, &g_Thresholds.sickleThreshold);

                    RenderTemplateRow(Tr("Corn Seed"), g_cornPathBuf, IM_ARRAYSIZE(g_cornPathBuf), c_templatePath, &g_Thresholds.cornThreshold);
                   
                    RenderTemplateRow(Tr("Carrot Seed"), g_carrotPathBuf, IM_ARRAYSIZE(g_carrotPathBuf), carrot_templatePath, &g_Thresholds.carrotThreshold);
                   
                    RenderTemplateRow(Tr("Carrot Shop"), g_carrotShopPathBuf, IM_ARRAYSIZE(g_carrotShopPathBuf), carrot_shop_templatePath, &g_Thresholds.carrotShopThreshold);
                    RenderTemplateRow(Tr("Soybean Seed"), g_soybeanPathBuf, IM_ARRAYSIZE(g_soybeanPathBuf), soybean_templatePath, &g_Thresholds.soybeanThreshold);
                    
                    RenderTemplateRow(Tr("Soybean Shop"), g_soybeanShopPathBuf, IM_ARRAYSIZE(g_soybeanShopPathBuf), soybean_shop_templatePath, &g_Thresholds.soybeanShopThreshold);
                    RenderTemplateRow(Tr("Sugarcane Seed"), g_sugarcanePathBuf, IM_ARRAYSIZE(g_sugarcanePathBuf), sugarcane_templatePath, &g_Thresholds.sugarcaneThreshold);
                   
                    RenderTemplateRow(Tr("Sugarcane Shop"), g_sugarcaneShopPathBuf, IM_ARRAYSIZE(g_sugarcaneShopPathBuf), sugarcane_shop_templatePath, &g_Thresholds.sugarcaneShopThreshold);
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Tr("SHOP TEMPLATES"));
                    ImGui::Separator();
                    RenderTemplateRow(Tr("Shop"), g_shopPathBuf, IM_ARRAYSIZE(g_shopPathBuf), shop_templatePath, &g_Thresholds.shopThreshold);
                    RenderTemplateRow("Roadside Shop", g_roadsideShopPathBuf, IM_ARRAYSIZE(g_roadsideShopPathBuf), roadside_shop_templatePath, &g_Thresholds.shopThreshold);
                    RenderTemplateRow(Tr("Wheat Shop"), g_wheatshopPathBuf, IM_ARRAYSIZE(g_wheatshopPathBuf), wheatshop_templatePath, &g_Thresholds.wheatShopThreshold);
                    RenderTemplateRow(Tr("Corn Shop"), g_cornShopPathBuf, IM_ARRAYSIZE(g_cornShopPathBuf), cornshop_templatePath, &g_Thresholds.cornShopThreshold);
                    RenderTemplateRow(Tr("Crate"), g_cratePathBuf, IM_ARRAYSIZE(g_cratePathBuf), crate_templatePath, &g_Thresholds.crateThreshold);
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Tr("UI TEMPLATES"));
                    ImGui::Separator();
                    RenderTemplateRow(Tr("Cross"), g_crossPathBuf, IM_ARRAYSIZE(g_crossPathBuf), cross_templatePath, &g_Thresholds.crossThreshold);
    RenderTemplateRow(Tr("Open Ad (Sale Panel)"), g_advertisePathBuf, IM_ARRAYSIZE(g_advertisePathBuf), advertise_templatePath, &g_Thresholds.advertiseThreshold);
    RenderTemplateRow(Tr("Open Ad (Occupied Crate)"), g_occupiedCrateAdvertisePathBuf, IM_ARRAYSIZE(g_occupiedCrateAdvertisePathBuf), occupied_crate_advertise_templatePath, &g_Thresholds.advertiseThreshold);
    RenderTemplateRow(Tr("Create Ad (Sale Panel)"), g_createAdPathBuf, IM_ARRAYSIZE(g_createAdPathBuf), create_ad_templatePath, &g_Thresholds.createAdThreshold);
    RenderTemplateRow(Tr("Create Ad (Occupied Crate)"), g_occupiedCrateCreateAdPathBuf, IM_ARRAYSIZE(g_occupiedCrateCreateAdPathBuf), occupied_crate_create_ad_templatePath, &g_Thresholds.createAdThreshold);
                    RenderTemplateRow("Feed Mill", g_feedMillPathBuf, IM_ARRAYSIZE(g_feedMillPathBuf), feed_mill_templatePath, &g_Thresholds.feedMillThreshold);
                    RenderTemplateRow("Cow Feed", g_cowFeedPathBuf, IM_ARRAYSIZE(g_cowFeedPathBuf), cow_feed_templatePath, &g_Thresholds.cowFeedThreshold);
                    RenderTemplateRow("Dairy", g_dairyPathBuf, IM_ARRAYSIZE(g_dairyPathBuf), dairy_templatePath, &g_Thresholds.dairyThreshold);
                    RenderTemplateRow("Cream", g_creamPathBuf, IM_ARRAYSIZE(g_creamPathBuf), cream_templatePath, &g_Thresholds.creamThreshold);
                    RenderTemplateRow("Butter", g_butterPathBuf, IM_ARRAYSIZE(g_butterPathBuf), butter_templatePath, &g_Thresholds.creamThreshold);
                    RenderTemplateRow("Cheese", g_cheesePathBuf, IM_ARRAYSIZE(g_cheesePathBuf), cheese_templatePath, &g_Thresholds.creamThreshold);
                    RenderTemplateRow("Not Enough", g_notEnoughPathBuf, IM_ARRAYSIZE(g_notEnoughPathBuf), not_enough_templatePath, &g_Thresholds.notEnoughThreshold);
                    RenderTemplateRow("Feed Mill Cross", g_feedMillCrossPathBuf, IM_ARRAYSIZE(g_feedMillCrossPathBuf), feed_mill_cross_templatePath, &g_Thresholds.feedMillCrossThreshold);
                    RenderTemplateRow("Cow Pasture", g_cowPasturePathBuf, IM_ARRAYSIZE(g_cowPasturePathBuf), cow_pasture_templatePath, &g_Thresholds.cowPastureThreshold);
                    RenderTemplateRow(Tr("Create Sale"), g_createSalePathBuf, IM_ARRAYSIZE(g_createSalePathBuf), create_sale_templatePath, &g_Thresholds.createSaleThreshold);
                    RenderTemplateRow(Tr("Level Up"), g_levelupPathBuf, IM_ARRAYSIZE(g_levelupPathBuf), levelup_templatePath, &g_Thresholds.levelUpThreshold);
                    RenderTemplateRow(Tr("Level Up Cont."), g_levelupContinuePathBuf, IM_ARRAYSIZE(g_levelupContinuePathBuf), levelup_continue_templatePath, &g_Thresholds.levelUpThreshold);
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "MATERIAL SCAN TEMPLATES");
                    ImGui::Separator();
                    RenderTemplateRow("Barn Tab", g_barnMarketBuf, IM_ARRAYSIZE(g_barnMarketBuf), barn_market_templatePath, &g_Thresholds.barnTabThreshold);
                    RenderTemplateRow("Silo Tab", g_siloMarketBuf, IM_ARRAYSIZE(g_siloMarketBuf), silo_market_templatePath, &g_Thresholds.siloTabThreshold);
                    RenderTemplateRow("Market Close Cross", g_marketCloseCrossPathBuf, IM_ARRAYSIZE(g_marketCloseCrossPathBuf), market_close_crosstemplatePath, &g_Thresholds.marketCloseCrossThreshold);
                    RenderTemplateRow("Bolt Material", g_boltMaterialPathBuf, IM_ARRAYSIZE(g_boltMaterialPathBuf), bolt_material_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Tape Material", g_tapeMaterialPathBuf, IM_ARRAYSIZE(g_tapeMaterialPathBuf), tape_material_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Plank Material", g_plankMaterialPathBuf, IM_ARRAYSIZE(g_plankMaterialPathBuf), plank_material_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Nail Material", g_nailMaterialPathBuf, IM_ARRAYSIZE(g_nailMaterialPathBuf), nail_material_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Screw Material", g_screwMaterialPathBuf, IM_ARRAYSIZE(g_screwMaterialPathBuf), screw_material_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Panel Material", g_panelMaterialPathBuf, IM_ARRAYSIZE(g_panelMaterialPathBuf), panel_material_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Deed Material", g_deedMaterialPathBuf, IM_ARRAYSIZE(g_deedMaterialPathBuf), deed_material_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Mallet Material", g_malletMaterialPathBuf, IM_ARRAYSIZE(g_malletMaterialPathBuf), mallet_material_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Marker Material", g_markerMaterialPathBuf, IM_ARRAYSIZE(g_markerMaterialPathBuf), marker_material_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Map Material", g_mapMaterialPathBuf, IM_ARRAYSIZE(g_mapMaterialPathBuf), map_material_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Dynamite", g_dynamitePathBuf, IM_ARRAYSIZE(g_dynamitePathBuf), dynamite_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("TNT", g_tntPathBuf, IM_ARRAYSIZE(g_tntPathBuf), tnt_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Axe", g_axePathBuf, IM_ARRAYSIZE(g_axePathBuf), axe_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Saw", g_sawPathBuf, IM_ARRAYSIZE(g_sawPathBuf), saw_templatePath, &g_Thresholds.materialTemplateThreshold);
                    RenderTemplateRow("Shovel", g_shovelPathBuf, IM_ARRAYSIZE(g_shovelPathBuf), shovel_templatePath, &g_Thresholds.materialTemplateThreshold);
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "OCR");
                    if (ImGui::SliderFloat("OCR Anchor Threshold", &g_OcrAnchorThreshold, 0.50f, 0.95f, "%.2f")) SaveConfig();
                    const char* preserveModes[] = { "Template Anchor", "Direct Coordinate", "Calibrated ROI" };
                    if (ImGui::Combo("Wheat Preserve Scan Mode", &g_WheatPreserveScanMode, preserveModes, IM_ARRAYSIZE(preserveModes))) SaveConfig();
                    if (g_WheatPreserveScanMode == 1) {
                        if (ImGui::InputInt("Wheat Preserve X", &g_WheatPreserveCoordX)) SaveConfig();
                        if (ImGui::InputInt("Wheat Preserve Y", &g_WheatPreserveCoordY)) SaveConfig();
                        ImGui::TextDisabled("Direct mode crops around the fixed X/Y point.");
                    }
                    else if (g_WheatPreserveScanMode == 2) {
                        if (ImGui::InputInt("Preserve ROI Left", &g_WheatPreserveRoiLeft)) SaveConfig();
                        if (ImGui::InputInt("Preserve ROI Top", &g_WheatPreserveRoiTop)) SaveConfig();
                        if (ImGui::InputInt("Preserve ROI Right", &g_WheatPreserveRoiRight)) SaveConfig();
                        if (ImGui::InputInt("Preserve ROI Bottom", &g_WheatPreserveRoiBottom)) SaveConfig();
                        ImGui::TextDisabled("Calibrated ROI is the most stable preserve OCR mode.");
                    }
                    else {
                        ImGui::TextDisabled("Template mode anchors from wheat_shop.png and reads the nearby wheat count.");
                    }
                    ImGui::TextDisabled("Use Sale Stack Mode = Preserve OCR on a wheat account to keep the configured reserve. If OCR fails, the bot falls back to full-stack selling.");
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(Tr("Template Maker"))) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Tr("CREATE YOUR OWN TEMPLATES"));
                    ImGui::TextDisabled(Tr("If the bot cannot find objects, use this tool to capture a fresh screen."));
                    ImGui::TextDisabled(Tr("Open the saved screenshot in Paint, crop the item, and overwrite it in /templates."));
                    ImGui::Separator(); ImGui::Spacing();

                    static int tmplInst = 0;
                    ImGui::SetNextItemWidth(200);
                    const char* instItems[] = { Tr("Instance 1"), Tr("Instance 2"), Tr("Instance 3"), Tr("Instance 4"), Tr("Instance 5"), Tr("Instance 6") };
                    ImGui::Combo(Tr("Select Emulator to Capture##tmpl"), &tmplInst, instItems, IM_ARRAYSIZE(instItems));

                    static char tmplStatus[128] = "Ready.";
                    static ImVec4 tmplColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

                    ImGui::Spacing(); ImGui::Spacing();

                    if (ImGui::Button(Tr("TAKE SCREENSHOT (Save to /templates)"), ImVec2(320, 50))) {
                        strcpy(tmplStatus, Tr("Capturing..."));
                        tmplColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);

                        std::thread([]() {
                            int inst = tmplInst;
                            std::string currentAdb = GetUniversalAdbPath(inst);
                            cv::Mat rawScreen = CaptureInstanceScreen(inst, currentAdb, g_Bots[inst].adbSerial);

                            if (rawScreen.empty()) {
                                strcpy(tmplStatus, Tr("Error: Could not capture screen!"));
                                tmplColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                                AddLog(inst, Tr("Screenshot failed: Empty frame."), ImVec4(1, 0, 0, 1));
                                return;
                            }

                            time_t now = time(0);
                            struct tm tstruct;
                            char timeBuf[80];
                            localtime_s(&tstruct, &now);
                            strftime(timeBuf, sizeof(timeBuf), "%H%M%S", &tstruct);

                            if (!fs::exists("templates")) fs::create_directories("templates");
                            std::string filename = "screenshot_inst" + std::to_string(inst + 1) + "_" + std::string(timeBuf) + ".png";
                            std::string fullPath = "templates\\" + filename;

                            try {
                                if (cv::imwrite(fullPath, rawScreen)) {
                                    std::string msg = std::string(Tr("Saved: ")) + fullPath;
                                    strncpy(tmplStatus, msg.c_str(), sizeof(tmplStatus) - 1);
                                    tmplColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
                                    AddLog(inst, std::string(Tr("Screenshot saved to: ")) + fullPath, ImVec4(0, 1, 0, 1));
                                }
                                else {
                                    strcpy(tmplStatus, Tr("Error: Write failed (Permissions?)"));
                                    tmplColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                                }
                            }
                            catch (...) {
                                strcpy(tmplStatus, Tr("Exception: File write error."));
                                tmplColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                            }
                            }).detach();
                    }

                    ImGui::SameLine();
                    if (ImGui::Button(Tr("Watch Tutorial"), ImVec2(150, 50))) {
                        ShellExecuteA(0, "open", "https://youtu.be/2gfU65gIaEE", 0, 0, SW_SHOW);
                    }

                    ImGui::Spacing(); ImGui::Spacing();
                    ImGui::TextColored(tmplColor, Tr("Status: %s"), Tr(tmplStatus));

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        if (g_CurrentTab == 8) {
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), "EXPERIMENTAL");
            ImGui::Spacing();
            int i = g_SelectedInstanceUI;
            ImGui::TextDisabled("All tests are here. Starting a new one cancels the previous test flow.");
            ImGui::TextDisabled("Barn test flow: market -> empty crate -> barn -> silo -> direct close taps.");
            if (ImGui::Button("TEST SEED", ImVec2(150, 34))) {
                std::string tPath = (g_Bots[i].testCropMode == 0) ? w_templatePath : (g_Bots[i].testCropMode == 1 ? c_templatePath : (g_Bots[i].testCropMode == 2 ? carrot_templatePath : (g_Bots[i].testCropMode == 3 ? soybean_templatePath : sugarcane_templatePath)));
                float thresh = (g_Bots[i].testCropMode == 0) ? g_Thresholds.wheatThreshold : (g_Bots[i].testCropMode == 1 ? g_Thresholds.cornThreshold : (g_Bots[i].testCropMode == 2 ? g_Thresholds.carrotThreshold : (g_Bots[i].testCropMode == 3 ? g_Thresholds.soybeanThreshold : g_Thresholds.sugarcaneThreshold)));
                PerformTemplateTest(i, tPath, "Seed Check", thresh, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("TEST SICKLE", ImVec2(150, 34))) {
                PerformTemplateTest(i, s_templatePath, "Sickle Check", g_Thresholds.sickleThreshold, false);
            }
            if (ImGui::Button("TEST FIELD", ImVec2(150, 34))) {
                PerformTemplateTest(i, f_templatePath, "Field Check", g_Thresholds.soilThreshold, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("TEST GROW", ImVec2(150, 34))) {
                PerformTemplateTest(i, w_templatePath, "Grow Check", g_Thresholds.wheatThreshold, false);
            }
            if (ImGui::Button("TEST HARVEST", ImVec2(150, 34))) {
                PerformTemplateTest(i, g_templatePath, "Harvest Check", g_Thresholds.grownThreshold, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("TEST SELL", ImVec2(150, 34))) {
                PerformTemplateTest(i, wheatshop_templatePath, "Sell Check", g_Thresholds.wheatShopThreshold, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("TEST SALE MODE", ImVec2(170, 34))) {
                std::string shopTemplate = (g_Bots[i].testCropMode == 0) ? wheatshop_templatePath : (g_Bots[i].testCropMode == 1 ? cornshop_templatePath : (g_Bots[i].testCropMode == 2 ? carrot_shop_templatePath : (g_Bots[i].testCropMode == 3 ? soybean_shop_templatePath : sugarcane_shop_templatePath)));
                float shopThreshold = (g_Bots[i].testCropMode == 0) ? g_Thresholds.wheatShopThreshold : (g_Bots[i].testCropMode == 1 ? g_Thresholds.cornShopThreshold : (g_Bots[i].testCropMode == 2 ? g_Thresholds.carrotShopThreshold : (g_Bots[i].testCropMode == 3 ? g_Thresholds.soybeanShopThreshold : g_Thresholds.sugarcaneShopThreshold)));
                PerformTemplateTest(i, shopTemplate, "Sale Mode Check", shopThreshold, false);
            }
            if (ImGui::Button("TEST PRESERVE OCR", ImVec2(170, 34))) {
                PerformTemplateTest(i, wheatshop_templatePath, "Preserve OCR Check", g_Thresholds.wheatShopThreshold, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("TEST COW FEED", ImVec2(170, 34))) {
                PerformTemplateTest(i, feed_mill_templatePath, "Cow Feed Check", g_Thresholds.feedMillThreshold, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("TEST BARN/SILO", ImVec2(150, 34))) {
                PerformTemplateTest(i, barn_market_templatePath, "Barn/Silo Check", g_Thresholds.barnTabThreshold, false);
            }
            ImGui::SameLine();
            if (ImGui::Button("TEST SILO FULL CROSS", ImVec2(190, 34))) {
                PerformTemplateTest(i, silo_full_templatePath, "Silo Full Cross Check", g_Thresholds.siloFullCrossThreshold, false);
            }
            ImGui::TextDisabled("TEST SALE MODE validates the selected crop against the active sale stack mode.");
            ImGui::TextDisabled("TEST PRESERVE OCR is wheat-only and saves OCR debug captures when it runs.");
            ImGui::TextDisabled("TEST COW FEED opens the feed mill, queues cow feed three times, then drags feed across the cow pasture.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("Required barn/silo template filenames:");
            ImGui::BulletText("bolt_material.png");
            ImGui::BulletText("tape_material.png");
            ImGui::BulletText("plank_material.png");
            ImGui::BulletText("nail_material.png");
            ImGui::BulletText("screw_material.png");
            ImGui::BulletText("panel_material.png");
            ImGui::BulletText("deed_material.png");
            ImGui::BulletText("mallet_material.png");
            ImGui::BulletText("marker_material.png");
            ImGui::BulletText("map_material.png");
            ImGui::BulletText("dynamite.png");
            ImGui::BulletText("TNT.png");
            ImGui::BulletText("axe.png");
            ImGui::BulletText("saw.png");
            ImGui::BulletText("shovel.png");
        }

        if (g_CurrentTab == 9) {
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), "TEMPLATE NAMES");
            ImGui::Spacing();
            ImGui::Columns(3, "TemplateNamesCols", true);
            ImGui::Text("Filename"); ImGui::NextColumn();
            ImGui::Text("Used for"); ImGui::NextColumn();
            ImGui::Text("When used"); ImGui::NextColumn();
            ImGui::Separator();
            auto row = [&](const char* a, const char* b, const char* c){ ImGui::TextUnformatted(a); ImGui::NextColumn(); ImGui::TextWrapped("%s", b); ImGui::NextColumn(); ImGui::TextWrapped("%s", c); ImGui::NextColumn(); };
            row("field.png", "Empty field detection", "Farming and field diagnostics");
            row("grown.png", "Grown crop detection", "Harvest logic");
            row("wheat.png", "Farm wheat icon", "Seed/farm-side wheat detection");
            row("wheat_shop.png", "Sale wheat icon", "Sale panel product selection");
            row("shop.png", "Shop/market building", "Opening market");
            row("Roadside_Shop.png", "Roadside shop banner", "Confirming the shop is already open");
            row("shop_crate.png", "Empty crate", "Selling and barn test entry");
    row("advertise.png", "Open advertisement button (sale panel)", "Standard sale-panel ad button");
    row("open_ad_2.png", "Open advertisement button (occupied crate)", "Filled-crate ad flow after opening a non-empty crate");
    row("createad.png", "Create advertisement confirm (sale panel)", "Standard sale-panel ad publish confirmation");
    row("Create_ad_2.png", "Create advertisement confirm (occupied crate)", "Occupied-crate ad publish confirmation dialog");
            row("feed_mill.png", "Feed mill machine", "Opening the feed mill for cow feed crafting");
            row("cow_feed.png", "Cow feed bag", "Queueing feed in the mill and feeding the cow pasture");
            row("Not_enough.png", "Not enough resources warning", "Stopping the cow-feed craft routine when materials are missing");
            row("feed_mill_cross.png", "Feed mill dialog close", "Closing the feed mill after queueing or on error");
            row("cow_pasture.png", "Cow pasture", "Opening the cow feeding area and dragging feed across it");
            row("create_sale.png", "Put on sale button", "Final sale submit");
            row("cross.png", "Generic close", "Closing menus");
            row("market_close_cross.png", "Market close cross", "Exit sell/barn market screens");
            row("barn_market.png", "Barn tab anchor", "Barn scan inside market");
            row("silo_market.png", "Silo tab anchor", "Silo scan inside market");
            row("bolt_material.png", "Bolt count anchor", "Barn material OCR");
            row("tape_material.png", "Tape count anchor", "Barn material OCR");
            row("plank_material.png", "Plank count anchor", "Barn material OCR");
            row("nail_material.png", "Nail count anchor", "Silo material OCR");
            row("screw_material.png", "Screw count anchor", "Silo material OCR");
            row("panel_material.png", "Panel count anchor", "Silo material OCR");
            row("deed_material.png", "Deed count anchor", "Barn/land material OCR");
            row("mallet_material.png", "Mallet count anchor", "Barn/land material OCR");
            row("marker_material.png", "Marker count anchor", "Barn/land material OCR");
            row("map_material.png", "Map count anchor", "Barn/town material OCR");
            row("dynamite.png", "Dynamite count anchor", "Barn scan and mine tool OCR");
            row("TNT.png", "TNT count anchor", "Barn scan and mine tool OCR");
            row("axe.png", "Axe count anchor", "Barn scan and mine tool OCR");
            row("saw.png", "Saw count anchor", "Barn scan and mine tool OCR");
            row("shovel.png", "Shovel count anchor", "Barn scan and mine tool OCR");
            row("digit_0_hud.png", "HUD digit training screenshot", "Top-right coins/diamonds template training");
            row("digit_1_hud.png", "HUD digit training screenshot", "Top-right coins/diamonds template training");
            row("digit_2_hud.png", "HUD digit training screenshot", "Top-right coins/diamonds template training");
            row("digit_3_hud.png", "HUD digit training screenshot", "Top-right coins/diamonds template training");
            row("digit_4_hud.png", "HUD digit training screenshot", "Top-right coins/diamonds template training");
            row("digit_5_hud.png", "HUD digit training screenshot", "Top-right coins/diamonds template training");
            row("digit_6_hud.png", "HUD digit training screenshot", "Top-right coins/diamonds template training");
            row("digit_7_hud.png", "HUD digit training screenshot", "Top-right coins/diamonds template training");
            row("digit_8_hud.png", "HUD digit training screenshot", "Top-right coins/diamonds template training");
            row("digit_9_hud.png", "HUD digit training screenshot", "Top-right coins/diamonds template training");
            row("digit_0_sale.png", "Sale-panel digit training screenshot", "Preserve OCR and anchored item digit template training");
            row("digit_1_sale.png", "Sale-panel digit training screenshot", "Preserve OCR and anchored item digit template training");
            row("digit_2_sale.png", "Sale-panel digit training screenshot", "Preserve OCR and anchored item digit template training");
            row("digit_3_sale.png", "Sale-panel digit training screenshot", "Preserve OCR and anchored item digit template training");
            row("digit_4_sale.png", "Sale-panel digit training screenshot", "Preserve OCR and anchored item digit template training");
            row("digit_5_sale.png", "Sale-panel digit training screenshot", "Preserve OCR and anchored item digit template training");
            row("digit_6_sale.png", "Sale-panel digit training screenshot", "Preserve OCR and anchored item digit template training");
            row("digit_7_sale.png", "Sale-panel digit training screenshot", "Preserve OCR and anchored item digit template training");
            row("digit_8_sale.png", "Sale-panel digit training screenshot", "Preserve OCR and anchored item digit template training");
            row("digit_9_sale.png", "Sale-panel digit training screenshot", "Preserve OCR and anchored item digit template training");
            ImGui::Columns(1);
        }

        if (g_CurrentTab == 2) {
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("WEBHOOK & NOTIFICATIONS"));
            ImGui::Separator(); ImGui::Spacing();

            ImGui::BeginChild("NotificationConfig", ImVec2(0, 160), true);
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("BARN & SILO NOTIFICATIONS"));
            ImGui::Separator(); ImGui::Spacing();
            ImGui::Checkbox(Tr("Enable Webhook Notification"), &g_EnableBarnWebhook);
            if (g_EnableBarnWebhook) {
                ImGui::Spacing();
                ImGui::Text(Tr("Webhook URL:"));
                ImGui::PushItemWidth(-1);
                ImGui::InputText("##webhook", g_WebhookURL, IM_ARRAYSIZE(g_WebhookURL));
                ImGui::PopItemWidth();
                ImGui::Spacing();
                ImGui::Checkbox(Tr("Send Screenshot Image with Webhook"), &g_EnableWebhookImage);
                ImGui::TextDisabled(Tr("If disabled, only the text report will be sent (Faster)."));
            }
            ImGui::EndChild();
			ImGui::Separator(); ImGui::Spacing();
            ImGui::Text("This tab used to have remote commands, which helped users to control their bot from away. since i drop this project, i deleted that part.");
        }

        if (g_CurrentTab == 3) {
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("GLOBAL APPLICATION SETTINGS"));
            ImGui::Separator(); ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button(Tr("SAVE ALL SETTINGS"), ImVec2(-1, 35))) {
                SaveConfig();
                AddLog(-1, "Settings saved to nxrth_config.ini", ImVec4(0, 1, 0, 1));
            }
            ImGui::PopStyleColor();
            ImGui::Spacing();

            const char* settingsBundleFilter = "NXRTH Settings (*.nxrthsettings)\0*.nxrthsettings\0All Files\0*.*\0";
            const char* accountsBundleFilter = "NXRTH Accounts (*.nxrthaccounts)\0*.nxrthaccounts\0All Files\0*.*\0";
            float bundleButtonWidth = (ImGui::GetContentRegionAvail().x - 12.0f) * 0.5f;
            if (ImGui::Button("EXPORT SETTINGS", ImVec2(bundleButtonWidth, 30))) {
                std::string exportPath = SaveFileDialog(settingsBundleFilter, "nxrthsettings");
                if (!exportPath.empty()) {
                    std::string error;
                    if (ExportSettingsBundle(exportPath, error)) {
                        AddLog(-1, "Settings exported to " + exportPath, ImVec4(0, 1, 0, 1));
                    }
                    else {
                        AddLog(-1, "Settings export failed: " + error, ImVec4(1, 0.3f, 0.3f, 1));
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("IMPORT SETTINGS", ImVec2(bundleButtonWidth, 30))) {
                std::string importPath = OpenFileDialog(settingsBundleFilter);
                if (!importPath.empty()) {
                    std::string error;
                    if (ImportSettingsBundle(importPath, error)) {
                        AddLog(-1, "Settings imported from " + importPath, ImVec4(0, 1, 0, 1));
                    }
                    else {
                        AddLog(-1, "Settings import failed: " + error, ImVec4(1, 0.3f, 0.3f, 1));
                    }
                }
            }
            if (ImGui::Button("EXPORT ACCOUNTS", ImVec2(bundleButtonWidth, 30))) {
                std::string exportPath = SaveFileDialog(accountsBundleFilter, "nxrthaccounts");
                if (!exportPath.empty()) {
                    std::string error;
                    if (ExportAccountsBundle(exportPath, error)) {
                        AddLog(-1, "Accounts exported to " + exportPath, ImVec4(0, 1, 0, 1));
                    }
                    else {
                        AddLog(-1, "Accounts export failed: " + error, ImVec4(1, 0.3f, 0.3f, 1));
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("IMPORT ACCOUNTS", ImVec2(bundleButtonWidth, 30))) {
                std::string importPath = OpenFileDialog(accountsBundleFilter);
                if (!importPath.empty()) {
                    std::string error;
                    if (ImportAccountsBundle(importPath, error)) {
                        AddLog(-1, "Accounts imported from " + importPath, ImVec4(0, 1, 0, 1));
                    }
                    else {
                        AddLog(-1, "Accounts import failed: " + error, ImVec4(1, 0.3f, 0.3f, 1));
                    }
                }
            }
            ImGui::TextDisabled("Settings bundle exports nxrth_config.ini and nxrth_inventory.ini together.");
            ImGui::TextDisabled("Accounts bundle exports the encrypted .nxrth slot files only. Importing accounts does not overwrite your per-account bot settings.");
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            if (ImGui::Checkbox("Only sell if silo is full", &g_OnlySellIfSiloFull)) {
                SaveConfig();
            }
            ImGui::TextDisabled("When enabled, normal cycle-based sales are skipped. Sales only start after the silo becomes full.");
            ImGui::TextDisabled("During silo-full recovery, if all crates are occupied the bot opens a non-empty crate, clicks Advertise, then Create Ad, and skips the pass if those buttons are not available.");
            ImGui::TextDisabled("Silo-full-triggered sales use each account's normal Sale Stack Mode and Fixed Stack. Emergency Wheat Stack Limit only applies to the separate emergency sale path.");

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), "COORDINATE PROFILE");
            ImGui::TextDisabled("These fallback coordinates are used when templates are missing or confidence is too low.");
            bool coordChanged = false;
            coordChanged |= DrawCoordinateInputRow("Profile Open", g_CoordinateProfile.profileOpenX, g_CoordinateProfile.profileOpenY);
            coordChanged |= DrawCoordinateInputRow("Profile Copy Tag", g_CoordinateProfile.profileCopyTagX, g_CoordinateProfile.profileCopyTagY);
            coordChanged |= DrawCoordinateInputRow("Profile Close", g_CoordinateProfile.profileCloseX, g_CoordinateProfile.profileCloseY);
            coordChanged |= DrawCoordinateInputRow("Sale Panel Close", g_CoordinateProfile.salePanelCloseX, g_CoordinateProfile.salePanelCloseY);
            coordChanged |= DrawCoordinateInputRow("Market Close", g_CoordinateProfile.marketCloseX, g_CoordinateProfile.marketCloseY);
            coordChanged |= DrawCoordinateInputRow("Market Close 2", g_CoordinateProfile.marketCloseSecondX, g_CoordinateProfile.marketCloseSecondY);
            coordChanged |= DrawCoordinateInputRow("Occupied Crate Ad", g_CoordinateProfile.occupiedCrateAdOpenX, g_CoordinateProfile.occupiedCrateAdOpenY);
            coordChanged |= DrawCoordinateInputRow("Create Ad Confirm", g_CoordinateProfile.createAdConfirmX, g_CoordinateProfile.createAdConfirmY);
            coordChanged |= DrawCoordinateInputRow("Sale Qty Plus", g_CoordinateProfile.saleQuantityPlusX, g_CoordinateProfile.saleQuantityPlusY);
            coordChanged |= DrawCoordinateInputRow("Sale Qty Minus", g_CoordinateProfile.saleQuantityMinusX, g_CoordinateProfile.saleQuantityMinusY);
            coordChanged |= DrawCoordinateInputRow("Sale Max Price", g_CoordinateProfile.saleMaxPriceX, g_CoordinateProfile.saleMaxPriceY);
            coordChanged |= DrawCoordinateInputRow("Sale Price Minus", g_CoordinateProfile.salePriceMinusX, g_CoordinateProfile.salePriceMinusY);
            coordChanged |= DrawCoordinateInputRow("Put On Sale", g_CoordinateProfile.putOnSaleX, g_CoordinateProfile.putOnSaleY);
            if (coordChanged) SaveConfig();
            if (ImGui::Button("RESET COORDINATE PROFILE", ImVec2(220, 30))) {
                g_CoordinateProfile = CoordinateProfile{};
                SaveConfig();
            }

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            ImGui::Text(Tr("ADB Executable Path:"));
            float inputWidth = ImGui::GetContentRegionAvail().x - 130;
            ImGui::PushItemWidth(inputWidth);
            if (!g_AdbValid) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.1f, 0.1f, 1.0f));
            ImGui::InputText("##adbpath", g_AdbPathBuf, IM_ARRAYSIZE(g_AdbPathBuf));
            if (!g_AdbValid) ImGui::PopStyleColor();
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button(Tr("Browse"), ImVec2(80, 0))) {
                std::string p = OpenFileDialog("Executable\0*.exe\0All Files\0*.*\0");
                if (!p.empty()) strncpy(g_AdbPathBuf, p.c_str(), 260);
            }
            if (!g_AdbValid && icon_warning != 0) {
                ImGui::SameLine();
                ImGui::Image((void*)(intptr_t)icon_warning, ImVec2(20, 20));
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(Tr("File not found!"));
            }

            ImGui::Spacing();

            ImGui::Text(Tr("Emulator Console Path:"));
            ImGui::PushItemWidth(inputWidth);
            if (!g_MEmuValid) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.1f, 0.1f, 1.0f));
            ImGui::InputText("##memupath", g_MEmuPathBuf, IM_ARRAYSIZE(g_MEmuPathBuf));
            if (!g_MEmuValid) ImGui::PopStyleColor();
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button(Tr("Browse##memu"), ImVec2(80, 0))) {
                std::string p = OpenFileDialog("Executable\0*.exe\0All Files\0*.*\0");
                if (!p.empty()) strncpy(g_MEmuPathBuf, p.c_str(), 260);
            }
            if (!g_MEmuValid && icon_warning != 0) {
                ImGui::SameLine();
                ImGui::Image((void*)(intptr_t)icon_warning, ImVec2(20, 20));
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(Tr("File not found!"));
            }

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
           

            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Tr("TIMING & DELAY SETTINGS (Advanced)"));
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled(Tr("Adjust these values if your emulator is lagging or running too fast."));
            bool timingChanged = false;
            timingChanged |= ImGui::SliderInt(Tr("Game Load Wait (Seconds)"), &g_Intervals.gameLoadWait, 5, 45);
            timingChanged |= ImGui::SliderInt(Tr("Harvest Cooldown (ms)"), &g_Intervals.afterHarvestWait, 300, 5000);
            timingChanged |= ImGui::SliderInt(Tr("Planting Cooldown (ms)"), &g_Intervals.afterPlantWait, 300, 5000);
            timingChanged |= ImGui::SliderInt(Tr("Shop Open Wait (ms)"), &g_Intervals.shopEnterWait, 500, 5000);
            timingChanged |= ImGui::SliderInt(Tr("Next Account Wait (ms)"), &g_Intervals.nextAccountWait, 500, 5000);
            ImGui::Spacing();
            ImGui::TextDisabled(Tr("Shop Automation Speeds:"));
            timingChanged |= ImGui::SliderInt(Tr("Crate Menu Wait (ms)"), &g_Intervals.crateClickWait, 300, 3000);
            timingChanged |= ImGui::SliderInt(Tr("Coin Collect Wait (ms)"), &g_Intervals.coinCollectWait, 200, 2000);
            timingChanged |= ImGui::SliderInt(Tr("Product Select Wait (ms)"), &g_Intervals.productSelectWait, 200, 2000);
            timingChanged |= ImGui::SliderInt(Tr("Create Sale Wait (ms)"), &g_Intervals.createSaleWait, 300, 3000);
            if (timingChanged) SaveConfig();
            if (ImGui::Button("TURBO FARM", ImVec2(130, 28))) {
                g_Intervals.gameLoadWait = 8;
                g_Intervals.afterHarvestWait = 400;
                g_Intervals.afterPlantWait = 400;
                g_Intervals.nextAccountWait = 800;
                g_Intervals.shopEnterWait = 1000;
                g_Intervals.crateClickWait = 400;
                g_Intervals.coinCollectWait = 250;
                g_Intervals.productSelectWait = 250;
                g_Intervals.createSaleWait = 450;
                SaveConfig();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Max speed: harvest 400, plant 400, account 800, game 8s. May miss fields on slow emulators.");
            if (ImGui::Button("FAST SALE PRESET", ImVec2(170, 28))) {
                g_Intervals.shopEnterWait = 1200;
                g_Intervals.crateClickWait = 450;
                g_Intervals.coinCollectWait = 300;
                g_Intervals.productSelectWait = 250;
                g_Intervals.createSaleWait = 550;
                SaveConfig();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Applies a faster sale-panel profile: shop 1200, crate 450, coins 300, product 250, sale 550.");
            if (ImGui::Checkbox("Auto Zoom Out After Mailbox", &g_Intervals.autoZoomOutAfterMailbox)) SaveConfig();
            if (g_Intervals.autoZoomOutAfterMailbox) {
                if (ImGui::SliderInt("Mailbox Zoom Delay (ms)", &g_Intervals.mailboxZoomDelayMs, 0, 10000)) SaveConfig();
                ImGui::TextDisabled("After mailbox detection, the bot waits this long and then pinches inward to normalize the farm view.");
            }
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            ImGui::Checkbox(Tr("Enable Discord Rich Presence"), &g_EnableDiscordRPC);
            ImGui::TextDisabled(Tr("(Show your bot status on Discord profile)"));
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Tr("ABOUT"));
            ImGui::Separator();
            ImGui::Text(Tr("Made by North"));
            ImGui::Spacing();
            ImGui::TextWrapped(Tr("Special thanks to: Nuron, Dext3r, K T and HugoAyaz for their contributions."));
        }
        if (g_CurrentTab == 6) {
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("NXRTH BOT - USER MANUAL"));
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextWrapped(Tr("Want to ask something? ping me @Nxr in server. i will reply asap."));
            ImGui::Spacing(); ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.00f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
            ImGui::BeginChild("HowToUseArea", ImVec2(0, 0), true, 0);

            // 1. INITIAL SETUP
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            if (ImGui::CollapsingHeader(Tr("1. Initial Setup & Minitouch"), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::TextWrapped(Tr("Before starting, you must click Inject Important Files. Without this you won't be able to use the bot. it injects Minitouch, Zoom, Font, Field color changer all in one."));
                ImGui::Unindent();
                ImGui::Spacing();
            }

            // 2. BOT MANAGER
            if (ImGui::CollapsingHeader(Tr("2. Bot Manager & Accounts"))) {
                ImGui::Indent();
                ImGui::TextWrapped(Tr("Modes: Single Account Mode: It just plants, harvests, sells and repeats. Multi Account Mode: Bot Plants, Harvests, sells, changes account, repeats. After all accounts done, return to first account and so on."));
                ImGui::Spacing();
                ImGui::TextWrapped(Tr("To enable Multi account mode, you must Save at least 2 accounts and enable in bot config tab."));
                ImGui::Spacing();
                ImGui::TextWrapped(Tr("How to save account : Create New account in Account management tab, After skipping tutorials and getting to farm level 7, Press \"Save Slot Data\"."));
                ImGui::TextWrapped(Tr("If you want to change accounts manually you can press \"Load Slot Data\". Dont press this if you haven't saved account yet."));
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), Tr("IMPORTANT NOTE: DO NOT SAVE SUPERCELL ID ACCOUNTS BECAUSE YOU WILL GET COOKIES POP-UP AND BOT WONT WORK."));
                ImGui::Unindent();
                ImGui::Spacing();
            }

            // 3. AUTO TOM
            if (ImGui::CollapsingHeader(Tr("3. Auto Tom"))) {
                ImGui::Indent();
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), Tr("Read These Carefully or your bot can break."));
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), Tr("WARNING!!! I Quitted developing this bot project, and auto tom was not done. IT MAY NOT WORK PERFECTLY."));
                ImGui::TextWrapped(Tr("To use Auto Tom, make sure Tom is not on Cooldown."));
                ImGui::TextWrapped(Tr("if bot can't find tom's crate, then make your own template."));
                ImGui::Unindent();
                ImGui::Spacing();
            }

            // 4. AUTO TRANSFER BEM/SEM
            if (ImGui::CollapsingHeader(Tr("4. Auto Transfer Bem/Sem"))) {
                ImGui::Indent();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), Tr("WARNING!!! I Quitted developing this bot project, and auto transfer bem/sem was not done. IT MAY NOT WORK PERFECTLY."));
                ImGui::TextWrapped(Tr("You have to enable instance 6 and put an account there. it will Work as storage account there."));
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), Tr("DONT ADD FRIEND YOUR BOTS MANUALLY. yes, you heard it. IF you already added your bots, please remove them. my bot will automatically add friend."));
                ImGui::Spacing();
                ImGui::TextWrapped(Tr("TEST IF FARM NAME READING FUNCTION WORKS PROPERLY. IF BOTS ADDED EACH OTHER AS FRIENDS WITHOUT PROBLEM, THAT MEANS EVERYTHING IS FINE. IF NOT, CHANGE YOUR FARM NAME TO SOMETHING READABLE (IF BOT READED YOUR FARM NAME WRONG, PLEASE OPEN NXRTH_CONFIG.INI AND DELETE FARM NAME INFO)."));
                ImGui::Unindent();
                ImGui::Spacing();
            }

            // 5. REMOTE & WEBHOOK
            if (ImGui::CollapsingHeader(Tr("5. Remote & Webhook"))) {
                ImGui::Indent();
                ImGui::TextWrapped(Tr("You can always configure Remote & Webhook feature in Remote & Webhook Tab."));
                ImGui::TextWrapped(Tr("Type !id in my server to get your Discord id. To use Remote Control in Discord, You have to enter your discord id and Press \"Save Settings\" in Settings Tab."));
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Tr("Current remote controls are:"));
                ImGui::TextDisabled(Tr("[Note: <instanceid> is 1,2,3,4,5,6. For example 1 returns screenshot of first Memu.]"));
                ImGui::BulletText(Tr("!status returns status of the bots to the Webhook."));
                ImGui::BulletText(Tr("!start <instanceid> (starts memu + hayday + bot. if both memu and hayday already open its ok.)"));
                ImGui::BulletText(Tr("!ss <instanceid> (sends screenshot of the instance to the webhook address.)"));
                ImGui::BulletText(Tr("!ssall sends screenshots of the active instances at the same time."));
                ImGui::BulletText(Tr("!stop <instanceid> stops the bot."));
                ImGui::BulletText(Tr("!stopall This is emergency command. Stops all bots at once."));
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), Tr("For Telegram:"));
                ImGui::TextWrapped(Tr("Create your own telegram bot. You can search on Google for this."));
                ImGui::TextWrapped(Tr("Enter your telegram bot token and chat id"));
                ImGui::TextWrapped(Tr("Press Save settings in Settings tab."));
                ImGui::Spacing();
                ImGui::TextWrapped(Tr("Enabling Webhook only sends status of the barn after each sale cycle."));
                ImGui::TextWrapped(Tr("My number reading can make mistakes, you better enable send screenshot with webhook if you really care."));
                ImGui::Unindent();
                ImGui::Spacing();
            }

            ImGui::PopStyleColor(); 
            ImGui::EndChild();
            ImGui::PopStyleVar();   
            ImGui::PopStyleColor(); 
        }
        if (g_CurrentTab == 7) {
            ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.12f, 1.0f), Tr("BOT VISION"));
            ImGui::TextDisabled(Tr("See the world through the bot's eyes."));
            ImGui::Separator(); ImGui::Spacing();

            
            const char* instItems[] = { "Instance 1", "Instance 2", "Instance 3", "Instance 4", "Instance 5", "Instance 6" };
            ImGui::SetNextItemWidth(200);
            ImGui::Combo(Tr("Target Instance"), &g_VisionSelectedInst, instItems, IM_ARRAYSIZE(instItems));
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
            if (ImGui::Button(Tr("TAKE SNAPSHOT (Single Scan)"), ImVec2(250, 40))) {
                g_VisionLiveRunning = false; 
                std::thread([]() {
                    int inst = g_VisionSelectedInst;
                    std::string currentAdb = GetUniversalAdbPath(inst);
                    cv::Mat raw = CaptureInstanceScreen(inst, currentAdb, g_Bots[inst].adbSerial);
                    if (!raw.empty()) {
                        cv::Mat processed = ProcessVisionFrame(inst, raw);
                        std::lock_guard<std::mutex> lock(g_VisionMutex);
                        g_VisionMat = processed; 
                    }
                    }).detach();
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();

            if (g_VisionLiveRunning) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                if (ImGui::Button(Tr("STOP LIVE DETECTION"), ImVec2(250, 40))) {
                    g_VisionLiveRunning = false;
                }
                ImGui::PopStyleColor();
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.1f, 1.0f));
                if (ImGui::Button(Tr("START LIVE DETECTION (1 FPS)"), ImVec2(250, 40))) {
                    g_VisionLiveRunning = true;
                    std::thread([]() {
                        while (g_VisionLiveRunning) {
                            int inst = g_VisionSelectedInst;
                            std::string currentAdb = GetUniversalAdbPath(inst);
                            cv::Mat raw = CaptureInstanceScreen(inst, currentAdb, g_Bots[inst].adbSerial);
                            if (!raw.empty()) {
                                cv::Mat processed = ProcessVisionFrame(inst, raw);
                                std::lock_guard<std::mutex> lock(g_VisionMutex);
                                g_VisionMat = processed;
                            }
                           
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }
                        }).detach();
                }
                ImGui::PopStyleColor();
            }

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

          
            {
                std::lock_guard<std::mutex> lock(g_VisionMutex);
                if (!g_VisionMat.empty()) {
                    UpdateVisionTexture(g_VisionMat);
                    g_VisionMat.release(); 
                }
            }

            
            if (g_VisionTexture != 0) {
                
                float availWidth = ImGui::GetContentRegionAvail().x;
                float aspect = (float)g_VisionTexHeight / (float)g_VisionTexWidth;
                float renderWidth = availWidth * 0.9f;
                float renderHeight = renderWidth * aspect;

                ImGui::SetCursorPosX((availWidth - renderWidth) * 0.5f); 
                ImGui::Image((void*)(intptr_t)g_VisionTexture, ImVec2(renderWidth, renderHeight));
                ImVec2 imgMin = ImGui::GetItemRectMin();
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ImVec2 mousePos = ImGui::GetMousePos();
                    float localX = mousePos.x - imgMin.x;
                    float localY = mousePos.y - imgMin.y;
                    if (localX >= 0 && localY >= 0 && localX <= renderWidth && localY <= renderHeight && g_VisionTexWidth > 0 && g_VisionTexHeight > 0) {
                        int srcX = (int)(localX * ((float)g_VisionTexWidth / renderWidth));
                        int srcY = (int)(localY * ((float)g_VisionTexHeight / renderHeight));
                        (void)srcX;
                        (void)srcY;
                    }
                }
            }
            else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), Tr("No vision data available. Click a button above to scan."));
            }
        }


}
    ImGui::EndChild();
    ImGui::End();
}

int main() {
    SetUnhandledExceptionFilter(NxrthCrashHandler);
    if (!glfwInit()) return 1;
    Discord::Initialize();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1350, 820, "NXRTH Premium", NULL, NULL);

    if (!window) return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImFontConfig font_cfg;
	static const ImWchar global_ranges[] = { // ALPHABET EXTENSIONS
		0x0020, 0x00FF, // BASIC LATIN + LATIN SUPPLEMENT
		0x0100, 0x017F, // LATIN EXTENSION-A
		0x0400, 0x04FF, // KYRIL ALPHABET
        0,
    };
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Tahoma.ttf", 16.f, &font_cfg, global_ranges);

    SetPremiumTheme();
    LoadConfig();
    if (g_Language == -1) {
        AutoDetectLanguage(); 
        SaveConfig();
    }
    InitializeBots();
    LoadInventoryData();
    char exePathBuf[MAX_PATH];
    GetModuleFileNameA(NULL, exePathBuf, MAX_PATH);
    std::string currentExeDir = std::string(exePathBuf).substr(0, std::string(exePathBuf).find_last_of("\\/"));

    int w, h;
    LoadTextureFromFile((currentExeDir + "\\icons\\chart-bar-regular.png").c_str(), &icon_dashboard, &w, &h);
    LoadTextureFromFile((currentExeDir + "\\icons\\robot-solid.png").c_str(), &icon_config, &w, &h);
    LoadTextureFromFile((currentExeDir + "\\icons\\message-solid.png").c_str(), &icon_logs, &w, &h);
    LoadTextureFromFile((currentExeDir + "\\icons\\images-solid.png").c_str(), &icon_templates, &w, &h);
    LoadTextureFromFile((currentExeDir + "\\icons\\globe-solid.png").c_str(), &icon_cloud, &w, &h);
    LoadTextureFromFile((currentExeDir + "\\icons\\gears-solid.png").c_str(), &icon_settings, &w, &h);
    LoadTextureFromFile((currentExeDir + "\\icons\\triangle-exclamation-solid.png").c_str(), &icon_warning, &w, &h);
    LoadTextureFromFile((currentExeDir + "\\icons\\coin.png").c_str(), &icon_coin, &w, &h);
    LoadTextureFromFile((currentExeDir + "\\icons\\diamond.png").c_str(), &icon_dia, &w, &h);
    LoadTextureFromFile((currentExeDir +  "\\icons\\question-solid.png").c_str(), &icon_question, &w, &h);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    while (!glfwWindowShouldClose(window)) {
        if (g_EnableDiscordRPC) {
            Discord::Update(true);
        }
        else {
            Discord::Update(false);
        }

        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 30.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));

        ImGui::Begin("NXRTH_TITLE_BAR", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        RenderCustomTitleBar(window);
        ImGui::End();

        ImGui::PopStyleVar(2);

        RenderApp();

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.05f, 0.05f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    ExitProcess(0);
}
