#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "Adb.h"

#include "ProcessRunner.h"
#include "PersistentAdb.h"
#include "bot_logic.h"

#include <string>
#include <windows.h>

extern std::string kAdbPath;
extern std::string kLDPlayerAdbPath;

std::string GetUniversalAdbPath(int instanceId) {
    if (g_Bots[instanceId].emulatorType == 1) return kLDPlayerAdbPath;
    return kAdbPath;
}

std::string GetAdbOutput(int instanceId, std::string args) {
    std::string serial = g_Bots[instanceId].adbSerial;
    std::string cmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + serial + " " + args + "\"";

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hRead = NULL;
    HANDLE hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return "";
    }

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    CloseHandle(hWrite);

    std::string result = "";
    DWORD read;
    char buffer[256];
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &read, NULL) && read != 0) {
        buffer[read] = '\0';
        result += buffer;
    }
    CloseHandle(hRead);
    return result;
}

bool RunAdbCommand(int instanceId, std::string args) {
    std::string serial = g_Bots[instanceId].adbSerial;
    if (serial.empty()) return false;

    std::string cmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + serial + " " + args + "\"";
    return RunCmdHidden(cmd);
}

void AdbTap(int instanceId, int x, int y) {
    if (x >= 0 && y >= 0) {
        AppendActionDebugLog(instanceId, "TAP", "AdbTap", "x=" + std::to_string(x) + ",y=" + std::to_string(y));
    }
    std::string adbPath = GetUniversalAdbPath(instanceId);
    std::string serial = g_Bots[instanceId].adbSerial;
    // Try persistent shell first (no process spawn overhead)
    if (!serial.empty() && PersistentShellCommand(instanceId, adbPath, serial,
            "input tap " + std::to_string(x) + " " + std::to_string(y))) {
        return;
    }
    // Fallback to process spawning
    RunAdbCommand(instanceId, "shell input tap " + std::to_string(x) + " " + std::to_string(y));
}

