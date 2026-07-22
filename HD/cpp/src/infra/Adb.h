#pragma once

#include <string>

std::string GetUniversalAdbPath(int instanceId);
std::string GetAdbOutput(int instanceId, std::string args);
bool RunAdbCommand(int instanceId, std::string args);
void AdbTap(int instanceId, int x, int y);

