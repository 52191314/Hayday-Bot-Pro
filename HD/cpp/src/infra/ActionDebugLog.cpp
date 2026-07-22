#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "ActionDebugLog.h"

#include "Paths.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <windows.h>

bool g_EnableDebugActionLog = false;
static std::mutex g_DebugActionLogMutex;
static std::string g_DebugActionLogPath;
static constexpr unsigned long long kMaxDebugActionLogBytes = 4ull * 1024ull * 1024ull;
static constexpr DWORD kHighVolumeDebugLogMinMs = 250;
static std::unordered_map<std::string, DWORD> g_LastHighVolumeDebugLogTicks;

static std::string GetDebugActionLogPath() {
    if (!g_DebugActionLogPath.empty()) return g_DebugActionLogPath;
    std::string dir = GetAppDataPath();
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        std::filesystem::create_directories(dir, ec);
    }
    g_DebugActionLogPath = dir + "\\debug_action_log.csv";
    return g_DebugActionLogPath;
}

static std::string GetDebugTimestamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[32];
    sprintf_s(buf, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return std::string(buf);
}

static std::string EscapeCsvFieldStatic(const std::string& value) {
    bool needsQuotes = false;
    std::string escaped;
    escaped.reserve(value.size() + 2);
    for (char ch : value) {
        if (ch == '"' || ch == ',' || ch == '\n' || ch == '\r') needsQuotes = true;
        if (ch == '"') escaped += "\"\"";
        else escaped += ch;
    }
    return needsQuotes ? ("\"" + escaped + "\"") : escaped;
}

void AppendActionDebugLog(int instanceId, const std::string& category, const std::string& action, const std::string& detail) {
    if (!g_EnableDebugActionLog) return;
    std::lock_guard<std::mutex> lock(g_DebugActionLogMutex);

    if (category == "FIND_IMAGE" || category == "FIND_ALL" || category == "FIND_GROWN" || category == "FIND_EMPTY_FIELDS") {
        DWORD nowTick = GetTickCount();
        std::string throttleKey = category + "|" + action;
        DWORD& lastTick = g_LastHighVolumeDebugLogTicks[throttleKey];
        if (lastTick != 0 && nowTick - lastTick < kHighVolumeDebugLogMinMs) return;
        lastTick = nowTick;
    }

    std::string logPath = GetDebugActionLogPath();
    std::error_code ec;
    bool exists = std::filesystem::exists(logPath, ec);
    uintmax_t currentSize = exists ? std::filesystem::file_size(logPath, ec) : 0;
    if (!ec && currentSize > kMaxDebugActionLogBytes) return;
    bool needsHeader = !exists || currentSize == 0;

    std::ofstream out(logPath, std::ios::app);
    if (!out.is_open()) return;

    if (needsHeader) {
        out << "timestamp,instance,category,action,detail\n";
    }

    out << EscapeCsvFieldStatic(GetDebugTimestamp()) << ","
        << (instanceId + 1) << ","
        << EscapeCsvFieldStatic(category) << ","
        << EscapeCsvFieldStatic(action) << ","
        << EscapeCsvFieldStatic(detail) << "\n";
}
