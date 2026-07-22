# Project Wheat (C++ Bot) Code Map

Last verified: 2026-07-19

This document acts as a navigation map for the core C++ codebase of **Project Wheat** (`HD/` directory).

---

## Read First

> [!IMPORTANT]
> **Resolution Contract**: All coordinate-based inputs, window dimensions, and template images assume the MEmu/LDPlayer emulator is set to **640x480 resolution at 100 DPI**.
> 
> **Visual Injection Mode**: This bot does not rely heavily on CPU-heavy template image scanning. Instead, it injects modified texture files (`.Ahjie` assets) that color-code key items (e.g., empty fields as Magenta, grown wheat as Cyan). The C++ bot scans for specific HSV color ranges on the screen. See [sctx-xcoder-patching.md](file:///D:/02_Projects/HD/HaydayMod/docs/sctx-xcoder-patching.md) for marker details.

---

## Fast Routing

| If changing... | Start here | Related files |
|---|---|---|
| Main bot loop / startup | `HD/cpp/src/bot/PremiumLoop.cpp` | `HD/bot_logic.cpp` |
| UI & settings panel | `HD/premium gui.cpp` | `HD/serialization.cpp` |
| Account backup & rotation | `HD/cpp/src/bot/AccountFiles.cpp` | `HD/bot_logic.cpp` |
| Emulator launching & minitouch | `HD/cpp/src/bot/EmulatorControl.cpp` | `HD/cpp/src/infra/MinitouchClient.cpp` |
| Screen recovery & pop-up closes | `HD/cpp/src/bot/ScreenRecovery.cpp` | `HD/cpp/src/bot/RuntimeHealth.cpp` |
| Planting / harvesting crops | `HD/cpp/src/operations/farming/Farming.cpp` | `HD/cpp/src/operations/farming/Farming.h` |
| Cows / dairy routines | `HD/cpp/src/operations/farming/Animals.cpp` | `HD/cpp/src/operations/farming/Animals.h` |
| Roadside Shop (RSS) sales | `HD/cpp/src/operations/market/Sell.cpp` | `HD/cpp/src/operations/market/State.cpp` |
| OCR reading of item/silo counts | `HD/tesseract_ocr.cpp` | `HD/tesseract_ocr.h` |

---

## Core Symbols & Structs

| Symbol | Location | Role |
|---|---|---|
| `BotInstance` | `HD/bot_logic.h` | Manages the thread, config, state machine, and logs for one bot instance. |
| `AccountSlot` | `HD/bot_logic.h` | Stores credentials, target levels, active slots, and session metrics for one account. |
| `CoordinateProfile` | `HD/bot_logic.h` | Anchor offsets and relative swipe coordinates for shop/farm grids. |
| `TransferRequest` | `HD/BotEngine.h` | Handles cross-instance item transfers between a seller bot and storage master. |

---

## Major Implementation Flows

### 1. Main Instance Rotation Loop (`PremiumLoop.cpp`)
1. Spawns active thread via `RunPremiumBot()`.
2. Loops through active `AccountSlot` rotation.
3. For each slot:
   - Calls `LoadAccountFromSlot()` in `AccountFiles.cpp` to restore game XML data.
   - Starts emulator and launches game via `EmulatorControl.cpp` and `LaunchGame.cpp`.
   - Injects visual texture patches (`InjectImportantFiles()`).
   - Executes main routines (`ExecuteFarmingFlow()`, `RunDairyRoutine()`, `RunMarketSalesCycle()`).
   - Saves metrics and runs `SaveAccountToSlot()` before rotating to the next account.

### 2. Farming Flow (`Farming.cpp`)
1. Checks if silo is full using Tesseract OCR.
2. Performs harvesting by scanning screen for Cyan (Grown Wheat) or other crop colors using `GetCropGrownHsvRange()`.
3. Drags sickle across the field coordinates.
4. Performs planting by scanning screen for Magenta (Empty Fields), opening seed menu, and dragging the seed across field coordinates.

### 3. Roadside Shop Sales Flow (`Sell.cpp` & `State.cpp`)
1. Swipes to the roadside shop billboard and opens it.
2. Scans for empty crates.
3. Opens crate, selects target crop, sets price using OCR inputs (aims for max price or threshold-based pricing).
4. Selects **Advertise** checkbox based on settings and clicks listing confirm.
5. Saves crop count debug screenshots to `logs/` on failure for verification.

### 4. Storage Transfer Flow (`BotEngine.cpp`)
1. Wires inter-instance coordination using `TransferRequest`.
2. Seller instance lists the requested item (e.g. wheat) in designated shop coordinates.
3. Storage master instance navigates to the seller's shop via friendship tag/radar and buys it immediately.

---

## OCR Setup & Performance

- **Tesseract Engine**: Configured in `tesseract_ocr.cpp`.
- **Language Data**: Depends on `tessdata/eng.traineddata` (located in the app runtime folder).
- **OCR Logic**: Extracts sub-rects of the screen containing digits, preprocesses via OpenCV thresholding, and runs `TessBaseAPI::GetUTF8Text()`. Used specifically for checking crop levels in the silo and market listings.
