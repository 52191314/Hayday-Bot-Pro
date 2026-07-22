#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "Paths.h"

#include <filesystem>
#include <string>
#include <windows.h>
#include <shlobj.h>

std::string GetAppDataPath() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::string nxrthPath = std::string(path) + "\\NXRTH_Premium";

        if (!std::filesystem::exists(nxrthPath)) {
            std::filesystem::create_directories(nxrthPath);
            for (int i = 0; i < 6; i++) {
                std::filesystem::create_directories(nxrthPath + "\\Backups\\Instance_" + std::to_string(i));
            }
        }
        return nxrthPath;
    }
    return ".";
}

