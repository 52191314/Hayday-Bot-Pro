#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "NativeCapture.h"
#include "PersistentAdb.h"

#include <algorithm>
#include <cstdio>
#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

static bool RunCmdHiddenLogic(const std::string& command) {
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

    DWORD waitResult = WaitForSingleObject(pi.hProcess, 3000);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

static bool WaitForValidFile(const std::string& filePath, int maxWaitMs = 500) {
    int waited = 0;
    while (waited < maxWaitMs) {
        FILE* f = nullptr;
        fopen_s(&f, filePath.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fclose(f);
            if (size > 1024) return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        waited += 50;
    }
    return false;
}

static bool RunProcessCaptureStdout(const std::string& command, std::vector<unsigned char>& output, DWORD timeoutMs = 4500) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return false;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    std::vector<char> cmdBuffer(command.begin(), command.end());
    cmdBuffer.push_back(0);

    BOOL ok = CreateProcessA(nullptr, cmdBuffer.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);
    if (!ok) {
        CloseHandle(readPipe);
        return false;
    }

    output.clear();
    output.reserve(512 * 1024);
    unsigned char buffer[16384];
    DWORD startTick = GetTickCount();

    while (true) {
        DWORD available = 0;
        if (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            DWORD bytesRead = 0;
            DWORD bytesToRead = (std::min)(available, static_cast<DWORD>(sizeof(buffer)));
            if (ReadFile(readPipe, buffer, bytesToRead, &bytesRead, nullptr) && bytesRead > 0) {
                output.insert(output.end(), buffer, buffer + bytesRead);
            }
        }

        DWORD waitResult = WaitForSingleObject(pi.hProcess, 25);
        if (waitResult == WAIT_OBJECT_0) {
            while (true) {
                DWORD available = 0;
                if (!PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) || available == 0) break;

                DWORD bytesRead = 0;
                DWORD bytesToRead = (std::min)(available, static_cast<DWORD>(sizeof(buffer)));
                if (!ReadFile(readPipe, buffer, bytesToRead, &bytesRead, nullptr) || bytesRead == 0) break;
                output.insert(output.end(), buffer, buffer + bytesRead);
            }
            break;
        }

        if (GetTickCount() - startTick > timeoutMs) {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(readPipe);
            return false;
        }
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(readPipe);
    return exitCode == 0 && !output.empty();
}

cv::Mat CaptureInstanceScreen(int instanceId, const std::string& adbPath, const std::string& serial) {
    // Try persistent capture first (no process spawn overhead)
    cv::Mat persistentResult = PersistentCaptureScreen(instanceId, adbPath, serial);
    if (!persistentResult.empty()) return persistentResult;

    // Fallback to legacy per-process approach
    std::string tempFile = "C:\\Users\\Public\\adb_screen_" + std::to_string(instanceId) + ".raw";
    int maxRetries = 2;
    cv::Mat img;

    for (int i = 0; i < maxRetries; ++i) {
        std::vector<unsigned char> rawBytes;
        std::string directCmd = "\"" + adbPath + "\" -s " + serial + " exec-out screencap";
        if (RunProcessCaptureStdout(directCmd, rawBytes) && rawBytes.size() > 12) {
            uint32_t width = *reinterpret_cast<uint32_t*>(rawBytes.data());
            uint32_t height = *reinterpret_cast<uint32_t*>(rawBytes.data() + 4);
            uint32_t format = *reinterpret_cast<uint32_t*>(rawBytes.data() + 8);
            if (width > 0 && height > 0 && width <= 8000 && height <= 8000 && rawBytes.size() >= 12 + width * height * 4) {
                cv::Mat rawFrame(height, width, CV_8UC4, rawBytes.data() + 12);
                cv::Mat bgrFrame;
                cv::cvtColor(rawFrame, bgrFrame, cv::COLOR_RGBA2BGR);
                return bgrFrame.clone();
            }
        }

        remove(tempFile.c_str());
        std::string cmd = "cmd.exe /c \"\"" + adbPath + "\" -s " + serial + " exec-out screencap > \"" + tempFile + "\"\"";

        if (RunCmdHiddenLogic(cmd)) {
            if (WaitForValidFile(tempFile)) {
                FILE* f = nullptr;
                fopen_s(&f, tempFile.c_str(), "rb");
                if (f) {
                    unsigned char header[12];
                    if (fread(header, 1, 12, f) == 12) {
                        uint32_t width = *reinterpret_cast<uint32_t*>(header);
                        uint32_t height = *reinterpret_cast<uint32_t*>(header + 4);
                        uint32_t format = *reinterpret_cast<uint32_t*>(header + 8);
                        if (width > 0 && height > 0 && width <= 8000 && height <= 8000) {
                            DWORD pixelBytes = width * height * 4;
                            std::vector<unsigned char> pixels(pixelBytes);
                            if (fread(pixels.data(), 1, pixelBytes, f) == pixelBytes) {
                                cv::Mat rawFrame(height, width, CV_8UC4, pixels.data());
                                cv::Mat bgrFrame;
                                cv::cvtColor(rawFrame, bgrFrame, cv::COLOR_RGBA2BGR);
                                img = bgrFrame.clone();
                            }
                        }
                    }
                    fclose(f);
                }
                if (!img.empty()) {
                    remove(tempFile.c_str());
                    return img;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
    return cv::Mat();
}

