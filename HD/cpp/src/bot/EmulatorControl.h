#pragma once

#include <string>
#include <vector>

std::string GetMEmuConfig(int instanceId, std::string key);
void StartMinitouchStealth(int instanceId);
void EmulatorCrashWatchdog(int instanceId);
void HandleReviveHeartbeat(int instanceId);
std::string ExecuteMEmuCommandHidden(std::string args);
std::vector<int> GetExistingMEmuInstances();
void RunEmulatorFactory(int requestedInstance);
