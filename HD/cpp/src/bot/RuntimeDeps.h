#pragma once

#define NOMINMAX
#include "BotEngine.h"
#include "Language.h"
#include "Discord.h"
#include "tesseract_ocr.h"
#include "cpp/src/infra/ActionDebugLog.h"
#include "cpp/src/operations/market/Sell.h"
#include "cpp/src/operations/market/State.h"
#include "cpp/src/operations/farming/Animals.h"
#include "cpp/src/operations/farming/Farming.h"

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include "imgui.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdio.h>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

extern TemplateThresholds g_Thresholds;
extern IntervalSettings g_Intervals;
extern BotInstance g_Bots[6];
extern CoordinateProfile g_CoordinateProfile;

extern std::string kAdbPath;
extern std::string kMEmuConsolePath;
extern std::string GetAppDataPath();
extern bool g_EnableBarnWebhook;
extern bool g_OnlySellIfSiloFull;
extern bool g_EnableWebhookImage;

extern std::string f_templatePath; extern std::string w_templatePath; extern std::string s_templatePath;
extern std::string g_templatePath; extern std::string shop_templatePath; extern std::string roadside_shop_templatePath; extern std::string wheatshop_templatePath;
extern std::string soldcrate_templatePath; extern std::string crate_templatePath; extern std::string arrows_templatePath;
extern std::string plus_templatePath; extern std::string cross_templatePath; extern std::string advertise_templatePath; extern std::string occupied_crate_advertise_templatePath;
extern std::string create_sale_templatePath; extern std::string c_templatePath; extern std::string gc_templatePath;
extern std::string max_price_templatePath;
extern std::string create_ad_templatePath; extern std::string occupied_crate_create_ad_templatePath;
extern std::string feed_mill_templatePath; extern std::string cow_feed_templatePath; extern std::string not_enough_templatePath;
extern std::string dairy_templatePath; extern std::string cream_templatePath; extern std::string butter_templatePath; extern std::string cheese_templatePath;
extern std::string feed_mill_cross_templatePath; extern std::string cow_pasture_templatePath;
extern std::string cornshop_templatePath; extern std::string barn_market_templatePath; extern std::string silo_market_templatePath;
extern std::string mailbox_templatePath; extern std::string loading_templatePath; extern std::string crate_wheat_templatePath; extern std::string crate_corn_templatePath;
extern std::string levelup_templatePath; extern std::string levelup_continue_templatePath; extern std::string carrot_templatePath;
extern std::string grown_carrot_templatePath; extern std::string carrot_shop_templatePath; extern std::string soybean_templatePath;
extern std::string grown_soybean_templatePath; extern std::string soybean_shop_templatePath; extern std::string sugarcane_templatePath;
extern std::string grown_sugarcane_templatePath; extern std::string sugarcane_shop_templatePath; extern std::string silo_full_templatePath;
extern std::string crate_carrot_templatePath; extern std::string crate_soybean_templatePath; extern std::string crate_sugarcane_templatePath;
extern std::string silo_full_cross_templatePath; extern std::string market_close_crosstemplatePath;
extern std::string bolt_material_templatePath; extern std::string tape_material_templatePath; extern std::string plank_material_templatePath;
extern std::string nail_material_templatePath; extern std::string screw_material_templatePath; extern std::string panel_material_templatePath;
extern std::string deed_material_templatePath; extern std::string mallet_material_templatePath; extern std::string marker_material_templatePath; extern std::string map_material_templatePath;
extern std::string dynamite_templatePath; extern std::string tnt_templatePath; extern std::string axe_templatePath; extern std::string saw_templatePath; extern std::string shovel_templatePath;
extern std::string farm_pack_templatePath; extern std::string event_board_templatePath; extern std::string farm_pack_cross_templatePath; extern std::string event_board_cross_templatePath; extern std::string not_responding_templatePath;

extern void AddLog(int instanceId, std::string message, ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
extern void AddLogEx(int instanceId, int level, const std::string& domain, const std::string& code, std::string message, ImVec4 color);
extern void SaveConfig();
extern void ApplyFarmConfigForSlot(int instanceId, int accountIndex);
extern void SaveInventoryData();
extern std::string GetClipboardText();
