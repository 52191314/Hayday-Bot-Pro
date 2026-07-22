#define _CRT_SECURE_NO_WARNINGS

#include "LaunchGame.h"

#include "bot_logic.h"
#include "cpp/src/infra/ProcessRunner.h"
#include "imgui.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>

extern std::string kAdbPath;
extern std::string kMEmuConsolePath;
extern void AddLog(int instanceId, std::string message, ImVec4 color);

void StartMEmuAndGame(int instanceId) {
    AddLog(instanceId, "Starting Emulator Environment...", ImVec4(0, 1, 1, 1));

    std::thread([instanceId]() {
        BotInstance& bot = g_Bots[instanceId];

        RunCmdHidden("cmd.exe /c \"\"" + kAdbPath + "\" kill-server\"");

        std::string cmd;
        if (bot.emulatorType == 1) {
            cmd = "cmd.exe /c \"\"" + kMEmuConsolePath + "\" launch --index " + std::to_string(bot.emuIndex) + "\"";
        }
        else {
            cmd = "cmd.exe /c \"\"" + kMEmuConsolePath + "\" " + std::string(bot.vmName) + "\"";
        }
        RunCmdHidden(cmd);

        AddLog(instanceId, "Waiting for Android to boot (Takes 15-30s)...", ImVec4(1, 1, 0, 1));

        bool booted = false;
        for (int i = 0; i < 40; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(2));

            RunCmdHidden("cmd.exe /c \"\"" + kAdbPath + "\" connect " + std::string(bot.adbSerial) + "\"");

            std::string tempFile = "C:\\Users\\Public\\boot_check_" + std::to_string(instanceId) + ".txt";
            std::remove(tempFile.c_str());
            std::string checkCmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + std::string(bot.adbSerial) + " shell getprop sys.boot_completed > \"" + tempFile + "\"\"";
            RunCmdHidden(checkCmd);

            std::ifstream file(tempFile);
            std::string result;
            if (file.is_open()) {
                std::getline(file, result);
                file.close();
            }

            if (result.find("1") != std::string::npos) {
                booted = true;
                break;
            }
        }

        if (!booted) {
            AddLog(instanceId, "ERROR: Android boot timeout! ADB offline.", ImVec4(1, 0, 0, 1));
            return;
        }

        AddLog(instanceId, "Android is ready. Launching Hay Day...", ImVec4(0, 1, 0, 1));
        std::this_thread::sleep_for(std::chrono::seconds(3));

        std::string launchCmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + std::string(bot.adbSerial) + " shell monkey -p com.supercell.hayday -c android.intent.category.LAUNCHER 1\"";
        RunCmdHidden(launchCmd);

        AddLog(instanceId, "App Launched successfully.", ImVec4(0, 1, 0, 1));
        }).detach();
}
