#pragma once

#include "bot_logic.h"
#include "tesseract_ocr.h"
#include <opencv2/opencv.hpp>
#include <string>

std::string BuildHudReadDetail(const GameNumberDebugInfo& coinInfo, const GameNumberDebugInfo& diaInfo, const GameNumberDebugInfo& lvlInfo);
std::string BuildPreserveReadDetail(const ItemCountDebugInfo& info);
bool ScanBarnAndSiloFromMarket(int instanceId, int accountIndex, unsigned long long token = 0ULL, bool enforceToken = false);
bool SyncHudDataForSlot(int instanceId, int slotIndex);
bool SyncBarnSiloDataForSlot(int instanceId, int slotIndex);
bool SyncAllDataForSlot(int instanceId, int slotIndex);
