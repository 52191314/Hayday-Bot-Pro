# 🌾 Hayday-Bot-Pro (Project Wheat)

## 📖 Overview

**Hayday-Bot-Pro** is a high-performance C++ automation engine and graphical user interface (GUI) for **Hay Day**:
1. **C++ Bot Engine & ImGui GUI**: Built-in OCR engine, multi-emulator (MEmu/LDPlayer) support with background execution, direct color-state graphic detection, and automated multi-account rotation.
2. **Account Security (`.Ahjie`)**: AES-256-GCM encryption secured by Windows DPAPI to store local accounts safely.

---

## 🚀 Key Features

* **Visual Graphic Modifications**: Supports high-reliability color matching and visual detection for fields, crops, and buildings.
* **Background Multi-Instance Execution**: Runs multiple emulator instances with account rotation. Supports minimized MEmu or LDPlayer windows.
* **Built-in OCR Engine**: Reads levels, coin counts, diamond counts, and item numbers directly from the game screen.
* **Cryptographic Account Storage (`.Ahjie`)**: Integrates AES-256-GCM encryption protected via Windows DPAPI to secure account profiles.

---

## ⚙️ How to Use

### 1. Emulator Preparation
Configure your emulator settings:
* **Display Resolution**: Set to `640x480` with `100 DPI` (DPI is required for OCR alignment).
* **Render Mode**: Set to **DirectX**.
* **Root Permissions**: Enable Root in MEmu/LDPlayer settings.
* **Game Language**: Set in-game language to **English**.

### 2. Building & Running the C++ GUI Bot
1. Build the solution using Visual Studio solution file `HD/Premium bot.sln` or run the executable from `HD/x64/Release/premium.exe`.
2. Configure target account slots in the GUI interface.
3. Click **Start** to begin automated execution and account rotation.

---

## 📁 Repository Structure

* [HD/](file:///d:/02_Projects/HD/HaydayMod/HD): Core C++ workspace containing the UI (ImGui/Glad), ADB wrapper, OCR library, and bot logic.
* [HD/cpp/](file:///d:/02_Projects/HD/HaydayMod/HD/cpp): Modular C++ source modules (`app`, `bot`, `infra`, `operations`).
* [HD/tessdata/](file:///d:/02_Projects/HD/HaydayMod/HD/tessdata): OCR trained language models (`farm.traineddata`, `sayi.traineddata`, `eng.traineddata`).

---

## 🛑 Important Warnings

> [!WARNING]
> Automated play of game clients may violate terms of service. Always test on secondary or disposable accounts.
