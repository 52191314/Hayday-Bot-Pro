#pragma once

#include <string>

extern bool g_EnableDebugActionLog;
void AppendActionDebugLog(int instanceId, const std::string& category, const std::string& action, const std::string& detail);

