#pragma once

#include "bot_logic.h"
#include "imgui.h"
#include <string>

extern bool g_EnableActionTimingLog;

void AddSaleLog(int instanceId, int level, const std::string& code, const std::string& message, ImVec4 color);
void AddRecoveryLog(int instanceId, int level, const std::string& code, const std::string& message, ImVec4 color);
void LogSaleAction(int instanceId, const std::string& code, const std::string& action, const std::string& detail = "");
void LogSaleResult(int instanceId, const std::string& code, const std::string& action, bool success, const std::string& detail = "", int failureLevel = 2);
void SetFarmFlowState(int instanceId, AccountSlot& account, FarmFlowState state, const std::string& detail = "", bool forceLog = false);
void SetSalesFlowState(int instanceId, AccountSlot& account, SalesFlowState state, const std::string& detail = "", bool forceLog = false);
