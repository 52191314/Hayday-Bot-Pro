#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "PersistentAdb.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

// PNG IEND chunk: length(4) + "IEND"(4) + CRC(4) = 12 bytes
// This sequence always terminates a valid PNG file.
static const unsigned char PNG_IEND_MARKER[] = {
    0x00, 0x00, 0x00, 0x00,                         // chunk length = 0
    0x49, 0x45, 0x4E, 0x44,                         // chunk type = "IEND"
    0xAE, 0x42, 0x60, 0x82                          // CRC-32
};
static constexpr int PNG_IEND_LEN = 12;

// PNG file signature (first 8 bytes)
static const unsigned char PNG_SIGNATURE[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

static constexpr int MAX_INSTANCES = 6;
static PersistentShell   g_Shells[MAX_INSTANCES];
static PersistentCapture g_Captures[MAX_INSTANCES];

// =========================================================================
// PersistentShell implementation
// =========================================================================

bool PersistentShell::Start(const std::string& adb, const std::string& ser) {
    Stop();

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    // Child inherits the read end of stdin pipe
    SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput  = hRead;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    std::string cmd = "\"" + adb + "\" -s " + ser + " shell";
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr,
                             TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hRead); // We keep only the write end

    if (!ok) {
        CloseHandle(hWrite);
        return false;
    }

    hProcess    = pi.hProcess;
    hThread     = pi.hThread;
    hStdinWrite = hWrite;
    serial      = ser;
    adbPath     = adb;
    alive       = true;
    return true;
}

bool PersistentShell::SendCommand(const std::string& cmd) {
    if (!alive || hStdinWrite == nullptr) return false;

    // Check if process is still running
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(hProcess, &exitCode) || exitCode != STILL_ACTIVE) {
        alive = false;
        return false;
    }

    std::string line = cmd + "\n";
    DWORD written = 0;
    BOOL ok = WriteFile(hStdinWrite, line.c_str(), (DWORD)line.size(), &written, nullptr);
    if (!ok || written != line.size()) {
        alive = false;
        return false;
    }
    return true;
}

bool PersistentShell::IsAlive() {
    if (!alive || hProcess == nullptr) return false;
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(hProcess, &exitCode) || exitCode != STILL_ACTIVE) {
        alive = false;
        return false;
    }
    return true;
}

void PersistentShell::Stop() {
    if (hStdinWrite) { CloseHandle(hStdinWrite); hStdinWrite = nullptr; }
    if (hProcess) {
        // Give the shell 200ms to exit gracefully
        if (WaitForSingleObject(hProcess, 200) == WAIT_TIMEOUT) {
            TerminateProcess(hProcess, 1);
        }
        CloseHandle(hProcess);
        hProcess = nullptr;
    }
    if (hThread) { CloseHandle(hThread); hThread = nullptr; }
    alive = false;
}

// =========================================================================
// PersistentCapture implementation
// =========================================================================

bool PersistentCapture::Start(const std::string& adb, const std::string& ser) {
    Stop();

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    // Stdin pipe (we write trigger commands)
    HANDLE hStdinRead = nullptr, hStdinW = nullptr;
    if (!CreatePipe(&hStdinRead, &hStdinW, &sa, 0)) return false;
    SetHandleInformation(hStdinW, HANDLE_FLAG_INHERIT, 0);

    // Stdout pipe (we read PNG data)
    HANDLE hStdoutR = nullptr, hStdoutWrite = nullptr;
    if (!CreatePipe(&hStdoutR, &hStdoutWrite, &sa, 0)) {
        CloseHandle(hStdinRead); CloseHandle(hStdinW);
        return false;
    }
    SetHandleInformation(hStdoutR, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput  = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError  = hStdoutWrite;

    // The loop: wait for a line of input, then run screencap
    std::string cmd = "\"" + adb + "\" -s " + ser
                    + " exec-out sh -c \"while read trigger; do screencap; done\"";
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr,
                             TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);

    if (!ok) {
        CloseHandle(hStdinW);
        CloseHandle(hStdoutR);
        return false;
    }

    hProcess    = pi.hProcess;
    hThread     = pi.hThread;
    hStdinWrite = hStdinW;
    hStdoutRead = hStdoutR;
    serial      = ser;
    adbPath     = adb;
    alive       = true;
    return true;
}

static bool ReadBytes(HANDLE hPipe, unsigned char* dest, DWORD bytesToRead, HANDLE hProcess, int timeoutMs) {
    DWORD bytesReadTotal = 0;
    DWORD startTick = GetTickCount();
    while (bytesReadTotal < bytesToRead) {
        if (GetTickCount() - startTick > (DWORD)timeoutMs) {
            return false; // Timeout
        }
        DWORD available = 0;
        if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &available, nullptr)) {
            return false; // Pipe error
        }
        if (available == 0) {
            // Check if process died
            if (WaitForSingleObject(hProcess, 25) == WAIT_OBJECT_0) {
                return false;
            }
            continue;
        }
        DWORD toRead = (std::min)(available, bytesToRead - bytesReadTotal);
        DWORD read = 0;
        if (!ReadFile(hPipe, dest + bytesReadTotal, toRead, &read, nullptr) || read == 0) {
            return false;
        }
        bytesReadTotal += read;
    }
    return true;
}

cv::Mat PersistentCapture::Capture(int timeoutMs) {
    if (!alive || hStdinWrite == nullptr || hStdoutRead == nullptr) return cv::Mat();

    // Check if process is still running
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(hProcess, &exitCode) || exitCode != STILL_ACTIVE) {
        alive = false;
        return cv::Mat();
    }

    // Flush any stale data from the stdout pipe
    {
        DWORD available = 0;
        while (PeekNamedPipe(hStdoutRead, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            char junk[4096];
            DWORD read = 0;
            DWORD toRead = (std::min)(available, (DWORD)sizeof(junk));
            ReadFile(hStdoutRead, junk, toRead, &read, nullptr);
        }
    }

    // Send trigger to start screencap
    const char* trigger = "go\n";
    DWORD written = 0;
    if (!WriteFile(hStdinWrite, trigger, 3, &written, nullptr) || written != 3) {
        alive = false;
        return cv::Mat();
    }

    // Read 12-byte header (Width, Height, Format)
    unsigned char header[12];
    if (!ReadBytes(hStdoutRead, header, 12, hProcess, timeoutMs)) {
        return cv::Mat();
    }

    // Parse header
    uint32_t width = *reinterpret_cast<uint32_t*>(header);
    uint32_t height = *reinterpret_cast<uint32_t*>(header + 4);
    uint32_t format = *reinterpret_cast<uint32_t*>(header + 8);

    if (width == 0 || height == 0 || width > 8000 || height > 8000) {
        return cv::Mat(); // Invalid dimensions
    }

    DWORD pixelBytes = width * height * 4;
    std::vector<unsigned char> pixelData(pixelBytes);

    if (!ReadBytes(hStdoutRead, pixelData.data(), pixelBytes, hProcess, timeoutMs)) {
        return cv::Mat();
    }

    // Convert raw pixel data (RGBA_8888) to BGR for OpenCV
    cv::Mat rawFrame(height, width, CV_8UC4, pixelData.data());
    cv::Mat bgrFrame;
    cv::cvtColor(rawFrame, bgrFrame, cv::COLOR_RGBA2BGR);
    return bgrFrame.clone(); // Clone to copy pixelData buffer before it goes out of scope
}

bool PersistentCapture::IsAlive() {
    if (!alive || hProcess == nullptr) return false;
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(hProcess, &exitCode) || exitCode != STILL_ACTIVE) {
        alive = false;
        return false;
    }
    return true;
}

void PersistentCapture::Stop() {
    if (hStdinWrite) { CloseHandle(hStdinWrite); hStdinWrite = nullptr; }
    if (hStdoutRead) { CloseHandle(hStdoutRead); hStdoutRead = nullptr; }
    if (hProcess) {
        if (WaitForSingleObject(hProcess, 200) == WAIT_TIMEOUT) {
            TerminateProcess(hProcess, 1);
        }
        CloseHandle(hProcess);
        hProcess = nullptr;
    }
    if (hThread) { CloseHandle(hThread); hThread = nullptr; }
    alive = false;
}

// =========================================================================
// Public API
// =========================================================================

bool PersistentShellCommand(int instanceId, const std::string& adbPath,
                            const std::string& serial, const std::string& cmd) {
    if (instanceId < 0 || instanceId >= MAX_INSTANCES || serial.empty()) return false;

    PersistentShell& shell = g_Shells[instanceId];
    std::lock_guard<std::mutex> lock(shell.mtx);

    // Restart if serial or adb path changed
    if (shell.alive && (shell.serial != serial || shell.adbPath != adbPath)) {
        shell.Stop();
    }

    // Start if not alive
    if (!shell.IsAlive()) {
        if (!shell.Start(adbPath, serial)) {
            return false;
        }
        // Give the shell a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return shell.SendCommand(cmd);
}

cv::Mat PersistentCaptureScreen(int instanceId, const std::string& adbPath,
                                const std::string& serial) {
    if (instanceId < 0 || instanceId >= MAX_INSTANCES || serial.empty()) return cv::Mat();

    PersistentCapture& cap = g_Captures[instanceId];
    std::lock_guard<std::mutex> lock(cap.mtx);

    // Restart if serial or adb path changed
    if (cap.alive && (cap.serial != serial || cap.adbPath != adbPath)) {
        cap.Stop();
    }

    // Start if not alive
    if (!cap.IsAlive()) {
        if (!cap.Start(adbPath, serial)) {
            return cv::Mat();
        }
        // Give the exec-out shell a moment to start the loop
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    cv::Mat result = cap.Capture();

    // If capture failed, stop the persistent process so next call restarts
    if (result.empty()) {
        cap.Stop();
    }

    return result;
}

void ShutdownPersistentAdb(int instanceId) {
    if (instanceId < 0 || instanceId >= MAX_INSTANCES) return;

    {
        std::lock_guard<std::mutex> lock(g_Shells[instanceId].mtx);
        g_Shells[instanceId].Stop();
    }
    {
        std::lock_guard<std::mutex> lock(g_Captures[instanceId].mtx);
        g_Captures[instanceId].Stop();
    }
}

void ShutdownAllPersistentAdb() {
    for (int i = 0; i < MAX_INSTANCES; i++) {
        ShutdownPersistentAdb(i);
    }
}
