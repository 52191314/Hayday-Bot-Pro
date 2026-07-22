#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include "cpp/src/bot/RuntimeDeps.h"
#include "cpp/src/bot/EmulatorControl.h"
#include "cpp/src/infra/MinitouchClient.h"

namespace fs = std::filesystem;

std::string GetMEmuConfig(int instanceId, std::string key) {
	// FIND MEMUC.EXE BASED ON THE ADB PATH (IN CASE USER MOVED MEmu FOLDER OR RENAMED IT, WE CAN STILL FIND MEMUC.EXE) BECAUSE MEMUC EXE IS IN THE SAME FOLDER AS ADB.EXE
    std::string memucPath = kAdbPath;
    size_t lastSlash = memucPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        memucPath = memucPath.substr(0, lastSlash + 1) + "memuc.exe";
        // EXAMPLE "D:\MEmu\adb.exe" -> "D:\MEmu\" + "memuc.exe"
    }

    // 2. PREPARE THE COMMAND
    std::string cmd = "cmd.exe /c \"\"" + memucPath + "\" getconfig -i " + std::to_string(instanceId) + " " + key + "\"";

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
    si.wShowWindow = SW_HIDE; // CMD DOESNT POPS UP

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 3000); //WAIT UP TO 3 SECONDS
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    CloseHandle(hWrite);

    DWORD read;
    char buffer[128];
    ZeroMemory(buffer, sizeof(buffer));
    ReadFile(hRead, buffer, sizeof(buffer) - 1, &read, NULL);
    CloseHandle(hRead);

    std::string rawResult = buffer;
    std::string cleanResult = "";

    // CHECK RETURNED STRING AND EXTRACT NECESSARY PART (WE NEED DIGITS ONLY)
    for (char c : rawResult) {
        if (isdigit(c)) { 
            cleanResult += c; 
        }
    }
    // FOR EXAMPLE: IF RETURNED STRING IS " 640\r\n", cleanResult WILL TURN TO "640".

    return cleanResult;
}

void StartMinitouchStealth(int instanceId) {
    int minitouchPort = MinitouchPort(instanceId);
    RunAdbCommand(instanceId, "forward tcp:" + std::to_string(minitouchPort) + " localabstract:minitouch");

    std::string serial = g_Bots[instanceId].adbSerial;
    std::string cmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + serial + " shell /data/local/tmp/minitouch\"";
    WinExec(cmd.c_str(), SW_HIDE);
}

typedef BOOL(WINAPI* IsHungAppWindowProc)(HWND);
// WATCHDOG FOR THE EMULATOR OR GAME CRASH
void EmulatorCrashWatchdog(int instanceId) {
    BotInstance& bot = g_Bots[instanceId];
    if (!bot.isRunning) return;

 
    static std::chrono::steady_clock::time_point hungStart[6];
    static bool isHungState[6] = { false };
    static int pidFailCount[6] = { 0 };

    // =====================================================================
	// 1. WINDOWS "NOT RESPONDING" WHITE SCREEN ERROR CHECK (EMULATOR CRASHED OR FROZE)
    // =====================================================================
    HWND hwnd = FindWindowA(NULL, bot.vmName);
    if (hwnd != NULL) {
        HMODULE hUser32 = GetModuleHandleA("user32.dll");
        if (hUser32) {
            IsHungAppWindowProc IsHung = (IsHungAppWindowProc)GetProcAddress(hUser32, "IsHungAppWindow");
            if (IsHung && IsHung(hwnd)) {
                if (!isHungState[instanceId]) {
                    
                    isHungState[instanceId] = true;
                    hungStart[instanceId] = std::chrono::steady_clock::now();
                    AddLog(instanceId, "[WATCHDOG] Emulator not responding! Waiting 15s to confirm...", ImVec4(1, 0.5f, 0, 1));
                }
                else {
					// CHECK HOW LONG IT HAS BEEN IN HUNG STATE
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - hungStart[instanceId]).count();

                    if (elapsed >= 15) { // IF ITS BEEN MORE THAN 15 SEC RESTART
                        AddLog(instanceId, "[WATCHDOG] CRITICAL: Emulator DEAD for 15s! Forcing restart...", ImVec4(1, 0, 0, 1));

                        if (bot.emulatorType == 1) { // LDPlayer
                            RunCmdHidden("cmd.exe /c \"\"" + kMEmuConsolePath + "\" quit --index " + std::to_string(bot.emuIndex) + "\"");
                            std::this_thread::sleep_for(std::chrono::seconds(5));
                            RunCmdHidden("cmd.exe /c \"\"" + kMEmuConsolePath + "\" launch --index " + std::to_string(bot.emuIndex) + "\"");
                        }
                        else { // MEmu
                            RunCmdHidden("cmd.exe /c \"\"" + kMEmuConsolePath + "\" stop " + bot.vmName + "\"");
                            std::this_thread::sleep_for(std::chrono::seconds(5));
                            RunCmdHidden("cmd.exe /c \"\"" + kMEmuConsolePath + "\" " + bot.vmName + "\"");
                        }

						isHungState[instanceId] = false; // CLEAR MEMORY OF HUNG STATE BECAUSE WE JUST RESTARTED THE EMULATOR
                        std::this_thread::sleep_for(std::chrono::seconds(30)); // WAIT FOR REBOOT
                        return;
                    }
                }
            }
            else {
				// EMULATOR IS RESPONDING FINE, BUT CHECK IF WE WERE IN HUNG STATE BEFORE. IF YES, LOG RECOVERY.
                if (isHungState[instanceId]) {
                    AddLog(instanceId, "[WATCHDOG] Emulator recovered. False alarm cancelled.", ImVec4(0, 1, 0, 1));
                    isHungState[instanceId] = false;
                }
            }
        }
    }

    // =====================================================================
    // CHECK IF GAME CRASHED AND EMULATOR IS IN THE HOME PAGE.
    // =====================================================================
    std::string serial = bot.adbSerial;
    std::string tempFile = "C:\\Users\\Public\\pid_check_" + std::to_string(instanceId) + ".txt";
    remove(tempFile.c_str());

    std::string cmd = "cmd.exe /c \"\"" + kAdbPath + "\" -s " + serial + " shell pidof com.supercell.hayday > \"" + tempFile + "\"\"";
    RunCmdHidden(cmd);
    std::string currentAdb = kAdbPath;
    std::ifstream file(tempFile);
    std::string pidStr;
    if (file.is_open()) {
        std::getline(file, pidStr);
        file.close();
    }

    if (pidStr.empty() || pidStr.length() < 2) {
		pidFailCount[instanceId]++; // INCREMENT FAIL COUNT IF PID NOT FOUND

        if (pidFailCount[instanceId] >= 3) { // IF CANT FIND PID FOR 3 TIMES
            AddLog(instanceId, "[WATCHDOG] Hay Day crashed to desktop (3 Fails)! Relaunching...", ImVec4(1, 0.5f, 0, 1));
            std::string launchCmd = "cmd /c \"\"" + currentAdb + "\" -s " + serial + " shell monkey -p com.supercell.hayday -c android.intent.category.LAUNCHER 1\"";
            RunCmdHidden(launchCmd);

            pidFailCount[instanceId] = 0; // RESET
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        else {
            AddLog(instanceId, "[WATCHDOG] Game process missing... Warning " + std::to_string(pidFailCount[instanceId]) + "/3", ImVec4(1, 1, 0, 1));
        }
    }
    else {
		// PID IS BACK, RESET FAIL COUNT
        pidFailCount[instanceId] = 0;
    }
}
void HandleReviveHeartbeat(int instanceId) {
    BotInstance& bot = g_Bots[instanceId];
    if (!bot.useReviveMode || strlen(bot.reviveTemplatePath) < 5) return;

    AddLog(instanceId, "Revive: Checking custom anchor...", ImVec4(1, 1, 0, 1));

    cv::Mat screen = CaptureInstanceScreen(instanceId, kAdbPath, bot.adbSerial);
    MatchResult res = FindImage(screen, bot.reviveTemplatePath, 0.70f);

    if (res.found) {
        AddLog(instanceId, "Revive: System OK.", ImVec4(0, 1, 0, 1));
		bot.reviveFailCounter = 0; // SUCCESS, RESET FAIL COUNTER 
    }
    else {
        bot.reviveFailCounter++;
        AddLog(instanceId, "Revive: Anchor not found! Strike " + std::to_string(bot.reviveFailCounter) + "/3", ImVec4(1, 0.5f, 0, 1));

        if (bot.reviveFailCounter >= 3) {
            AddLog(instanceId, "CRITICAL: 3 Strikes! Restarting game engine...", ImVec4(1, 0, 0, 1));
            // REOPEN HAYDAY
            RunAdbCommand(instanceId, "shell am force-stop com.supercell.hayday");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            RunAdbCommand(instanceId, "shell monkey -p com.supercell.hayday -c android.intent.category.LAUNCHER 1");

            bot.reviveFailCounter = 0;
            std::this_thread::sleep_for(std::chrono::seconds(15)); // BOOT SLEEP
        }
    }
}

extern int g_TargetTabToSelect;


std::string ExecuteMEmuCommandHidden(std::string args) {
    std::string memucPath = kAdbPath;
    size_t lastSlash = memucPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        memucPath = memucPath.substr(0, lastSlash + 1) + "memuc.exe";
    }

    std::string cmd = "cmd.exe /c \"\"" + memucPath + "\" " + args + "\"";
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
        WaitForSingleObject(pi.hProcess, 45000); 
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

std::vector<int> GetExistingMEmuInstances() {
    std::vector<int> instances;
    std::string listOut = ExecuteMEmuCommandHidden("list");
    std::stringstream ss(listOut);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty() || line.find("Step:") != std::string::npos) continue; 
        size_t commaPos = line.find(',');
        if (commaPos != std::string::npos) {
            try {
             
                int index = std::stoi(line.substr(0, commaPos));
                instances.push_back(index);
            }
            catch (...) {}
        }
    }
    return instances;
}
// ========================================================================
// THE EMULATOR FACTORY: 1-CLICK MEMU CREATOR
// ========================================================================
void RunEmulatorFactory(int requestedInstance) {
    AddLog(requestedInstance, "Forging a new MEmu instance in the background... Please wait.", ImVec4(1, 1, 0, 1));

 
    std::string createOut = ExecuteMEmuCommandHidden("create");

  
    std::string lowerOut = createOut;
    for (char& c : lowerOut) c = tolower(c);

  
    int actualVm = -1;
    size_t idxPos = lowerOut.find("index:");
    if (idxPos != std::string::npos) {
        try { actualVm = std::stoi(lowerOut.substr(idxPos + 6)); }
        catch (...) {}
    }

    if (actualVm == -1) {
        std::string errMsg = "Failed to parse VM index!\n\n[MEMU RAW OUTPUT]:\n" + createOut;
        MessageBoxA(NULL, errMsg.c_str(), "Project Wheat - Fatal Error", MB_ICONERROR | MB_TOPMOST);
        g_Bots[requestedInstance].isCreatingEmulator = false;
        return;
    }

    int targetInstance = actualVm;
    bool didShift = false;

    if (targetInstance >= 6) {
        std::string limitMsg = "You created VM " + std::to_string(actualVm) + ", but Project Wheat supports max 6 instances!";
        MessageBoxA(NULL, limitMsg.c_str(), "Project Wheat - Limit Reached", MB_ICONWARNING | MB_TOPMOST);
        g_Bots[requestedInstance].isCreatingEmulator = false;
        return;
    }

    if (actualVm != requestedInstance) {
        didShift = true;
        g_TargetTabToSelect = targetInstance; 
    }

 
    AddLog(targetInstance, "VM created. Waiting for MEmu to unlock config files...", ImVec4(1, 1, 0, 1));
    std::this_thread::sleep_for(std::chrono::seconds(2));

    AddLog(targetInstance, "Applying Project Wheat strict configurations (640x480, DPI 100, Root, DirectX)...", ImVec4(1, 1, 0, 1));
    std::string vmStr = std::to_string(actualVm);

    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " is_customed_resolution 1");
    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " resolution_width 640");
    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " resolution_height 480");
    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " vbox_dpi 100");
    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " root 1");
    ExecuteMEmuCommandHidden("setconfig -i " + vmStr + " vbox_graph_mode 1"); // 1 = DirectX, 0 = OpenGL


    int calculatedPort = 21503 + (actualVm * 10);
    strncpy(g_Bots[targetInstance].adbSerial, ("127.0.0.1:" + std::to_string(calculatedPort)).c_str(), 64);
    std::string vmNameStr = (actualVm == 0) ? "MEmu" : "MEmu_" + std::to_string(actualVm);
    strncpy(g_Bots[targetInstance].vmName, vmNameStr.c_str(), 64);


    AddLog(targetInstance, " Starting Emulator Engine...", ImVec4(0, 1, 0, 1));
    ExecuteMEmuCommandHidden("start -i " + vmStr);


    if (didShift) {
        std::string msg = " SMART SHIFT ALERT\n\nYou clicked 'Create' on Instance " + std::to_string(requestedInstance + 1) +
            ", but MEmu actually created VM " + std::to_string(actualVm) +
            ".\n\nProject Wheat automatically linked it and moved you to the correct tab.";
        MessageBoxA(NULL, msg.c_str(), "Project Wheat - Auto Linker", MB_ICONINFORMATION | MB_TOPMOST);
    }
    else {
        std::string msg = " MEmu created successfully!\n\nTarget: VM " + std::to_string(actualVm) +
            "\nConfig: 640x480, 100 DPI, Root, DirectX.\n\nThe emulator is booting up now.";
        MessageBoxA(NULL, msg.c_str(), "Project Wheat - Emulator Factory", MB_ICONINFORMATION | MB_TOPMOST);
    }

    g_Bots[requestedInstance].isCreatingEmulator = false;
}
