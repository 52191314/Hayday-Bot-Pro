#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "ProcessRunner.h"

#include <string>
#include <vector>
#include <windows.h>

bool RunCmdHidden(const std::string& command) {
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::string finalCmd = command;
    std::vector<char> cmdBuffer(finalCmd.begin(), finalCmd.end());
    cmdBuffer.push_back(0);

    BOOL ok = CreateProcessA(nullptr, cmdBuffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) return false;

    DWORD waitResult = WaitForSingleObject(pi.hProcess, 30000);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        // Log through the debug output channel (no instanceId available here)
        OutputDebugStringA(("[TIMEOUT] RunCmdHidden killed process after 30s: " + command + "\n").c_str());
        return false;
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0;
}

