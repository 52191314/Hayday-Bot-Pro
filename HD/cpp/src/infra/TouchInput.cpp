#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "TouchInput.h"

#include "ProcessRunner.h"
#include "Language.h"
#include "bot_logic.h"
#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>

extern std::string kAdbPath;
extern void AddLog(int instanceId, std::string message, ImVec4 color);

bool AutoDetectTouchDevice(int instanceId) {
    AddLog(instanceId, Tr("Detecting Input..."), ImVec4(1, 1, 0, 1));
    std::string tempFile = "C:\\Users\\Public\\devicelist_" + std::to_string(instanceId) + ".txt";
    remove(tempFile.c_str());
    std::string cmd = "cmd /c \"\"" + kAdbPath + "\" -s " + std::string(g_Bots[instanceId].adbSerial) + " shell getevent -pl > \"" + tempFile + "\"\"";
    RunCmdHidden(cmd);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::ifstream file(tempFile);
    if (!file.is_open()) {
        AddLog(instanceId, Tr("Error: Could not read list. Check ADB path."), ImVec4(1, 0, 0, 1));
        return false;
    }
    std::string line;
    std::string currentDevice = "";
    bool found = false;
    while (std::getline(file, line)) {
        if (line.find("add device") != std::string::npos && line.find("/dev/input/") != std::string::npos) {
            size_t startPos = line.find("/dev/input/");
            currentDevice = line.substr(startPos);
            currentDevice.erase(std::remove(currentDevice.begin(), currentDevice.end(), '\r'), currentDevice.end());
            currentDevice.erase(std::remove(currentDevice.begin(), currentDevice.end(), '\n'), currentDevice.end());
        }
        if (!currentDevice.empty()) {
            if (line.find("ABS_MT_POSITION_X") != std::string::npos || line.find("0035") != std::string::npos) {
                strcpy(g_Bots[instanceId].inputDevice, currentDevice.c_str());
                AddLog(instanceId, std::string(Tr("Input Found: ")) + std::string(g_Bots[instanceId].inputDevice), ImVec4(0, 1, 0, 1));
                found = true;
                break;
            }
        }
    }
    file.close();
    if (!found) {
        strcpy(g_Bots[instanceId].inputDevice, "/dev/input/event1");
        AddLog(instanceId, Tr("Failed. Retrying next time..."), ImVec4(1, 0.5f, 0, 1));
        return false;
    }
    return true;
}

