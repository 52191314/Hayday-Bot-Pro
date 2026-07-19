# 🌾 Hayday-Bot-Pro (Project Wheat)



## 📖 Overview

This repository houses a comprehensive suite of automation and reverse-engineering tools for **Hay Day**:
1. **Premium C++ Bot Engine & GUI**: Direct color-state graphic modification (bypassing slow image-template scans), built-in OCR engine, multi-emulator (MEmu/LDPlayer) support with background execution, and automated account rotation.
2. **Account Security (`.YaJing`)**: AES-256-GCM encryption secured by Windows DPAPI to store local accounts safely.
3. **Storage Crypto Bypasses**: Python scripts to read, validate, and manipulate the game's encrypted local preferences file (`storage_new.xml`) to alter account parameters (e.g. drop groups).
4. **Native Security Hooks**: LSPosed/Zygisk modules and Frida scripts to hook Hay Day at runtime, bypassing Promon shielding and intercepting AES keys.
5. **Decryption Proxy & Headless Client**: A custom C# .NET proxy for MitM traffic analysis and automated actions (e.g., tutorial skipping) over a WireGuard VPN tunnel.

---

## 🚀 Key Features

* **Visual Graphic Modifications**: Modifies game asset layouts to render fields/crops in solid colors (magenta, orange, cyan), allowing high-reliability color matching instead of fragile image scans.
* **Background Multi-Instance Execution**: Runs up to **5 emulator instances** with **15 accounts rotating** on each. Supports minimized MEmu or LDPlayer windows.
* **Built-in OCR**: Reads levels, coin counts, and numbers directly from the game screen.
* **Cryptographic Accounts (`.YaJing`)**: Integrates modern AES-256-GCM encryption. Master keys are protected via Windows DPAPI to link profile backups to your Windows user account.
* **Drop-Group Control**: Directly modify your account's drop rate group (0 to 25) by decrypting and rewriting the game's `storage_new.xml` file.
* **Zygisk Promon Shield Bypass**: Dynamically intercepts Hay Day's execution, preventing runtime exit and dumping the AES key from memory.
* **Transparent MitM Decryption**: Direct-to-VHD Android certificate injection and local WireGuard tunnel to intercept and decrypt game HTTPS traffic.

---

## ⚙️ How to Use

### 1. Emulator & Settings Preparation
For the bot to successfully interact with the emulator, configure your settings as follows:
* **Display Resolution**: Set to exactly `640x480` with `100 DPI` (DPI is critical for OCR and alignment).
* **Render Mode**: Set to **DirectX** (OpenGL rendering may cause graphical glitches with injected assets).
* **Root Permissions**: Enable Root in MEmu/LDPlayer settings.
* **Game Language**: Set the in-game language to **English**.
* **Launch**: Click **Launch Emulator + Game** in the UI to perform the initial setup.

### 2. C++ GUI Bot (Project Wheat)
1. Build the solution using the Visual Studio project inside [HD](file:///d:/02_Projects/HD/HaydayMod/HD) or run the compiled executable from `HD/x64/Release/premium.exe`.
2. Extract and keep the `injecthacks/` folder beside your executable.
3. Click **Inject Important Files** in the GUI to push the custom color-state shaders and zoom hacks to the emulator.
4. Select your target account slots and click start to begin rotation.

### 3. Running the Headless Client & Proxy
To build and run the C# .NET decryption proxy or headless client, execute the batch scripts at the root:
* Run [run_headless.bat](file:///d:/02_Projects/HD/HaydayMod/run_headless.bat) to launch the headless client login suite.
* Run [run_proxy.bat](file:///d:/02_Projects/HD/HaydayMod/run_proxy.bat) to spin up the `SupercellProxy` MitM decryption proxy.

---

## 🔐 Storage Cryptography (`storage_new.xml`)

Hay Day uses native cryptography to protect local preferences. Key details:
* **Base Seed String**: `0HMDC=MI9726MM<AGE35`
* **Key Derivation**: The raw AES-256 key is the `SHA-256` hash of the seed:
  * **Key (Hex)**: `395701810b2ca08b8162b49bc14914d5bff3852f5e112fe79845019c50c6bbd8`
* **CBC Initialization Vector (IV)**: `fldsjfodasjifuds` (16 bytes)
* **Preference Keys**: Encrypted with **AES-256-ECB**.
* **Preference Values**: Encrypted with **AES-256-CBC**.

### Account Drop-Group Mapping
The encrypted key `kdsBDLvTJkt5ScQihjAVGA==` (`higher_env3`) controls the account's drop group. Below are the known decrypted integer values:

| Encrypted Token (AES-CBC) | Decrypted Integer | Notes |
| :--- | :--- | :--- |
| `Jl5SbAzoQwaeg2RjwlcQOw==` | `0` | Lowest drop group |
| `ntmNPvN5kt8QTw4Y6JQ+Ag==` | `4` | Default live value |
| `dqIbyTvSRWefYz3hSclnhQ==` | `13` | Panel drops group |
| `Y/o2rdafcnLxV9dA5WzN5w==` | `21` | High Tape (slot 4) |
| `JyQYP8z7U7/pjnSOGOPb2A==` | `25` | Highest possible drop rate group |

Refer to [storage-encryption.md](file:///d:/02_Projects/HD/HaydayMod/docs/storage-encryption.md) for a full table and python decryption scripts.

---

## 📁 Repository Structure

* [HD/](file:///d:/02_Projects/HD/HaydayMod/HD): Core C++ workspace containing the UI design (ImGui/Glad), ADB wrapper, OCR library, and bot logic.
* [lsposed_hayday_promon_aes/](file:///d:/02_Projects/HD/HaydayMod/lsposed_hayday_promon_aes): Source code for the Zygisk-compatible LSPosed module to hook the game and bypass anti-debugging protections.
* [third_party/SupercellProxy/](file:///d:/02_Projects/HD/HaydayMod/third_party/SupercellProxy): MitM proxy engine and headless protocol playground.
* [tools/](file:///d:/02_Projects/HD/HaydayMod/tools):
  * [inject_cert_direct_vhd.py](file:///d:/02_Projects/HD/HaydayMod/tools/inject_cert_direct_vhd.py): Safely injects CA certificates directly into emulator virtual disks (.vmdk) without mounting.
  * [convert_nxrth_to_yajing.py](file:///d:/02_Projects/HD/HaydayMod/tools/convert_nxrth_to_yajing.py): Migrates legacy XOR-hex accounts to modern `.YaJing` AES-GCM secure files.
  * [hayday_capture.py](file:///d:/02_Projects/HD/HaydayMod/tools/hayday_capture.py): mitmproxy script to capture and log game HTTPS requests.
* [docs/](file:///d:/02_Projects/HD/HaydayMod/docs): Complete developer specifications detailing project sub-systems.

---

## 🛑 Important Warnings

> [!WARNING]
> **Use at your own risk.** Automated play and reverse-engineering of game binaries violate Supercell's Terms of Service. This bot can and will get your accounts banned if detected. **NEVER** use this on your primary or main account. Always run tests on secondary/disposable accounts.

---

## 📚 Deep Dive Documentation

For developer instructions, logs, and code maps, see:
* [bot-architecture.md](file:///d:/02_Projects/HD/HaydayMod/docs/bot-architecture.md) — Runtime layout, visual injection slots, and HSV ranges.
* [storage-encryption.md](file:///d:/02_Projects/HD/HaydayMod/docs/storage-encryption.md) — Detailed overview of preference AES crypto.
* [yajing-encryption.md](file:///d:/02_Projects/HD/HaydayMod/docs/yajing-encryption.md) — Details on DPAPI and `.YaJing` binary format.
* [remaining-steps.md](file:///d:/02_Projects/HD/HaydayMod/docs/remaining-steps.md) — Step-by-step pipeline for tutorial skipping and traffic capture.
* [Frida.md](file:///d:/02_Projects/HD/HaydayMod/Frida.md) — Worklog of runtime Frida investigations, bypasses, and key dumps.
