#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "cpp/src/bot/RuntimeDeps.h"
#include "cpp/src/bot/FlowLogging.h"

namespace fs = std::filesystem;

bool g_EnableActionTimingLog = false;
static std::mutex g_ActionTimingLogMutex;

static std::string EscapeCsvField(const std::string& value) {
    bool needsQuotes = false;
    std::string escaped;
    escaped.reserve(value.size() + 2);
    for (char ch : value) {
        if (ch == '"' || ch == ',' || ch == '\n' || ch == '\r') {
            needsQuotes = true;
        }
        if (ch == '"') escaped += "\"\"";
        else escaped += ch;
    }
    return needsQuotes ? ("\"" + escaped + "\"") : escaped;
}

static std::string GetActionTimingTimestamp() {
    std::time_t rawTime = std::time(nullptr);
    std::tm timeInfo{};
    localtime_s(&timeInfo, &rawTime);

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
    return std::string(buffer);
}

static std::string FormatDurationSeconds(long long durationMs) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << (static_cast<double>(durationMs) / 1000.0);
    return out.str();
}

static int FindAccountSlotIndex(int instanceId, const AccountSlot& account) {
    if (instanceId < 0 || instanceId >= 6) return -1;

    const BotInstance& bot = g_Bots[instanceId];
    for (int slotIndex = 0; slotIndex < MAX_ACCOUNT_SLOTS; ++slotIndex) {
        if (&bot.accounts[slotIndex] == &account) return slotIndex;
    }
    return -1;
}

static void AppendActionTimingCsv(int instanceId, int slotIndex, const std::string& category, const std::string& action, long long durationMs, const std::string& detail) {
    std::lock_guard<std::mutex> lock(g_ActionTimingLogMutex);

    fs::path timingPath = fs::path(GetAppDataPath()) / "action_timing_log.csv";
    std::error_code ec;
    bool needsHeader = !fs::exists(timingPath, ec) || fs::file_size(timingPath, ec) == 0;

    std::ofstream out(timingPath, std::ios::app);
    if (!out.is_open()) return;

    if (needsHeader) {
        out << "timestamp,instance,slot,category,action,duration_ms,duration_s,detail\n";
    }

    out << EscapeCsvField(GetActionTimingTimestamp()) << ","
        << (instanceId + 1) << ","
        << (slotIndex >= 0 ? slotIndex + 1 : 0) << ","
        << EscapeCsvField(category) << ","
        << EscapeCsvField(action) << ","
        << durationMs << ","
        << EscapeCsvField(FormatDurationSeconds(durationMs)) << ","
        << EscapeCsvField(detail) << "\n";
}

static void RecordCompletedFlowAction(int instanceId, AccountSlot& account, const char* category, const char* action, const std::string& detail, std::chrono::steady_clock::time_point startedAt) {
    if (!g_EnableActionTimingLog) return;
    if (!startedAt.time_since_epoch().count() || std::string(action) == "Idle") return;

    auto now = std::chrono::steady_clock::now();
    long long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - startedAt).count();
    if (durationMs < 0) return;

    int slotIndex = FindAccountSlotIndex(instanceId, account);
    AppendActionTimingCsv(instanceId, slotIndex, category, action, durationMs, detail);

    AddLog(instanceId,
        std::string("TIMING [") + category + "] " + action + " finished in " + FormatDurationSeconds(durationMs) + "s | " + detail,
        ImVec4(0.95f, 0.78f, 0.35f, 1.0f));
}

static std::string BuildCurrentPassId(int instanceId, const AccountSlot& account) {
    int slotIndex = FindAccountSlotIndex(instanceId, account);
    std::string pass = "I" + std::to_string(instanceId + 1);
    if (slotIndex >= 0) pass += "-S" + std::to_string(slotIndex + 1);
    pass += "-F" + std::to_string((std::max)(0, account.cyclesSinceStart));
    pass += "-SC" + std::to_string((std::max)(0, account.salesCycleCount));
    return pass;
}

void AddSaleLog(int instanceId, int level, const std::string& code, const std::string& message, ImVec4 color) {
    AddLogEx(instanceId, level, "SALE", code, message, color);
}

void AddRecoveryLog(int instanceId, int level, const std::string& code, const std::string& message, ImVec4 color) {
    AddLogEx(instanceId, level, "RECOVERY", code, message, color);
}

static void AddFlowLog(int instanceId, int level, const std::string& code, const std::string& message, ImVec4 color) {
    AddLogEx(instanceId, level, "FLOW", code, message, color);
}

void LogSaleAction(int instanceId, const std::string& code, const std::string& action, const std::string& detail) {
    std::string message = "action=" + action;
    if (!detail.empty()) message += " | " + detail;
    AppendActionDebugLog(instanceId, "SALE_ACTION", code, message);
    AddLogEx(instanceId, 0, "SALE_ACTION", code, message, ImVec4(0.55f, 0.78f, 1.0f, 1.0f));
}

void LogSaleResult(int instanceId, const std::string& code, const std::string& action, bool success, const std::string& detail, int failureLevel) {
    std::string message = "action=" + action + " success=" + std::string(success ? "1" : "0");
    if (!detail.empty()) message += " | " + detail;
    AppendActionDebugLog(instanceId, "SALE_RESULT", code, message);
    AddLogEx(instanceId,
        success ? 1 : failureLevel,
        "SALE_RESULT",
        code,
        message,
        success ? ImVec4(0.3f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.22f, 1.0f));
}

void SetFarmFlowState(int instanceId, AccountSlot& account, FarmFlowState state, const std::string& detail, bool forceLog) {
    std::string nextDetail = detail.empty() ? ToString(state) : detail;
    bool changed = forceLog || account.farmFlowState != state || account.farmFlowDetail != nextDetail;
    if (changed) {
        FarmFlowState previousState = account.farmFlowState;
        std::string previousDetail = account.farmFlowDetail;
        std::chrono::steady_clock::time_point previousStartedAt = account.farmFlowStateStartedAt;

        account.farmFlowState = state;
        account.farmFlowDetail = nextDetail;
        account.farmFlowStateStartedAt = std::chrono::steady_clock::now();

        RecordCompletedFlowAction(instanceId, account, "FARM", ToString(previousState), previousDetail, previousStartedAt);
        AddFlowLog(instanceId, 0, "FARM_STATE_CHANGE",
            std::string(ToString(previousState)) + " -> " + ToString(state) + " | reason=" + nextDetail +
            " | pass=" + BuildCurrentPassId(instanceId, account),
            ImVec4(0.55f, 0.9f, 1.0f, 1.0f));
    }
}

void SetSalesFlowState(int instanceId, AccountSlot& account, SalesFlowState state, const std::string& detail, bool forceLog) {
    std::string nextDetail = detail.empty() ? ToString(state) : detail;
    bool changed = forceLog || account.salesFlowState != state || account.salesFlowDetail != nextDetail;
    if (changed) {
        SalesFlowState previousState = account.salesFlowState;
        std::string previousDetail = account.salesFlowDetail;
        std::chrono::steady_clock::time_point previousStartedAt = account.salesFlowStateStartedAt;

        account.salesFlowState = state;
        account.salesFlowDetail = nextDetail;
        account.salesFlowStateStartedAt = std::chrono::steady_clock::now();

        RecordCompletedFlowAction(instanceId, account, "SALE", ToString(previousState), previousDetail, previousStartedAt);
        AddFlowLog(instanceId, 0, "SALE_STATE_CHANGE",
            std::string(ToString(previousState)) + " -> " + ToString(state) + " | reason=" + nextDetail +
            " | pass=" + BuildCurrentPassId(instanceId, account),
            ImVec4(0.75f, 0.95f, 0.55f, 1.0f));
    }
}
