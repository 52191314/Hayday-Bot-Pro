#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <string>
#include <mutex>
#include <vector>
#include <windows.h>
#include <opencv2/opencv.hpp>

// =========================================================================
// Persistent ADB Shell — eliminates process-creation overhead for
// tap/swipe commands by keeping a single 'adb shell' process alive.
// =========================================================================
struct PersistentShell {
    HANDLE hProcess = nullptr;
    HANDLE hThread  = nullptr;
    HANDLE hStdinWrite = nullptr;
    std::string serial;
    std::string adbPath;
    std::mutex  mtx;
    bool alive = false;

    bool Start(const std::string& adbPath, const std::string& serial);
    bool SendCommand(const std::string& cmd);
    bool IsAlive();
    void Stop();
};

// =========================================================================
// Persistent Screen Capture — keeps an 'adb exec-out' process alive
// that runs 'screencap' on demand via stdin trigger.
// Reads raw width/height/format and pixel bytes to avoid PNG encoding overhead.
// =========================================================================
struct PersistentCapture {
    HANDLE hProcess    = nullptr;
    HANDLE hThread     = nullptr;
    HANDLE hStdinWrite = nullptr;
    HANDLE hStdoutRead = nullptr;
    std::string serial;
    std::string adbPath;
    std::mutex  mtx;
    bool alive = false;

    bool Start(const std::string& adbPath, const std::string& serial);
    cv::Mat Capture(int timeoutMs = 5000);
    bool IsAlive();
    void Stop();
};

// Public API — all functions are thread-safe per instance.
// instanceId must be 0-5.

// Send a shell command via persistent ADB shell (fire-and-forget).
// Returns true if sent successfully.
bool PersistentShellCommand(int instanceId, const std::string& adbPath,
                            const std::string& serial, const std::string& cmd);

// Capture screen via persistent ADB exec-out.
// Returns empty Mat on failure (caller should fall back to old approach).
cv::Mat PersistentCaptureScreen(int instanceId, const std::string& adbPath,
                                const std::string& serial);

// Shutdown persistent connections for one instance or all.
void ShutdownPersistentAdb(int instanceId);
void ShutdownAllPersistentAdb();
