#include "Config.h"

#include <filesystem>
#include <string>

extern char g_AdbPathBuf[260];
extern char g_MEmuPathBuf[260];
extern bool g_AdbValid;
extern bool g_MEmuValid;
extern std::string kAdbPath;
extern std::string kMEmuConsolePath;

void ValidatePaths() {
    g_AdbValid = std::filesystem::exists(g_AdbPathBuf);
    g_MEmuValid = std::filesystem::exists(g_MEmuPathBuf);
    if (g_AdbValid) kAdbPath = std::string(g_AdbPathBuf);
    if (g_MEmuValid) kMEmuConsolePath = std::string(g_MEmuPathBuf);
}

