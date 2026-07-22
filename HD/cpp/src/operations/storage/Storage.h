#pragma once

#include "BotEngine.h"
#include <string>

extern int g_TransferThreshold;
extern TransferRequest g_TransferRequest;

float GetSimilarityScore(const std::string& s1, const std::string& s2);
bool NXRTH_Radar(int instanceId, std::string targetName, int mode, bool skipMenuOpen);
void RunStorageMaster(int instanceId);
bool SendFriendRequestToStorage(int instanceId, std::string targetTag);
