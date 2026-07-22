# HaydayMod Tools Map

Last updated: 2026-07-01

This document catalogs every tool script, build artifact, and utility in the HaydayMod workspace. Tools are organized by functional domain.

---

## 1. Bot Automation (C++ Build)

The main bot executable lives at `HD/x64/Release/premium.exe`.

### Build system
| File | Purpose |
|------|---------|
| `HD/Premium bot.sln` | Visual Studio 2022 solution |
| `HD/Premium bot/bot.vcxproj` | Project file (x64/Release is the active config) |
| `HD/Premium bot/bot.vcxproj.filters` | Filter organization |
| `HD/vcpkg-configuration.json` | vcpkg manifest |
| `HD/vcpkg.json` | vcpkg dependencies |

### Build dependencies (vcpkg at `D:\01_Apps\DevTools\vcpkg\`)
- `opencv4:x64-windows` (4.12.0) — computer vision
- `glfw3:x64-windows` (3.4) — window management
- `leptonica:x64-windows`, `libjpeg-turbo`, `libpng`, `libwebp`, etc. — image I/O

---

## 2. Network Capture & MITM Proxy

Tools for decrypting and inspecting Hay Day's HTTPS traffic via a WireGuard+mitmproxy pipeline.

| Script | Purpose | Usage |
|--------|---------|-------|
| `tools/hayday_capture.py` | mitmproxy addon script; captures Hay Day requests to `logs/hayday_capture.json` | `mitmdump --mode wireguard -s tools/hayday_capture.py --ssl-insecure` |
| `tools/summarize_capture.py` | Generates a redacted summary from `logs/hayday_capture.json` → `logs/hayday_capture_summary.json` | `python tools/summarize_capture.py` |
| `tools/extract_credentials.py` | Extracts credentials from capture output | `python tools/extract_credentials.py` |
| `tools/patch_public_key.py` | Patches public keys in captured data | `python tools/patch_public_key.py` |
| `tools/wireguard.apk` | WireGuard Android app APK for the emulator | Install via `adb install -r tools/wireguard.apk` |
| `tools/wg_client.conf` | WireGuard client config (endpoint `172.27.64.1:51820`) | Import into WireGuard app on emulator |
| `tools/mitmproxy_ca.pem` | mitmproxy CA certificate | Injected into Android system trust store |
| `tools/dotnet-install.ps1` | .NET runtime installer (for mitmproxy tooling) | N/A |

---

## 3. Certificate Injection

Tools for injecting the mitmproxy CA cert into the MEmu VM's system certificate store.

| Script | Purpose | Usage |
|--------|---------|-------|
| `tools/inject_cert_direct_vhd.py` | Injects cert directly into MEmu VHD image (recommended) | `python tools/inject_cert_direct_vhd.py --write --backup` |
| `tools/inject_cert_py.py` | Alternative cert injection method | `python tools/inject_cert_py.py` |
| `tools/inject_cert_fast.sh` | Shell-based cert injection for WSL | `bash tools/inject_cert_fast.sh` |
| `tools/credentials.json` | Credential file (used by extract_credentials.py) | — |

---

## 4. Game Data Analysis (Tools Root)

Legacy Python tools for AES key recovery, memory scanning, and storage analysis. Most are from the May 2026 investigation.

### Storage decryption & validation
| Script | Purpose | Usage |
|--------|---------|-------|
| *(in XCoder project — see `E:\HD\XCoder\scripts\HAY_DAY_DECRYPTION_GUIDE.md`)* | AES-128-ECB + Base64 storage decryption | — |

### AES key validation
| Tool | Location | Notes |
|------|----------|-------|
| Key derivation | Documented in `docs/storage-encryption.md` | `SHA256("0HMDC=MI9726MM<AGE35")` |
| Validation script | Stale reference; see storage-encryption.md | — |

### Run scripts
| File | Purpose |
|------|---------|
| `run_headless.bat` | Launches the HD bot in headless mode |
| `run_proxy.bat` | Launches the mitmproxy pipeline |

---

## 5. C++ Source Infrastructure (`HD/cpp/src/infra/`)

Reusable infrastructure modules extracted during the ongoing code migration.

| Module | Files | Purpose |
|--------|-------|---------|
| **MinitouchClient** | `MinitouchClient.h`, `.cpp` | Minitouch socket protocol: connection, primitives, and 3 unified gesture helpers (drag, pinch, V-sweep) |
| **Adb** | `Adb.h`, `.cpp` | ADB command execution, `AdbTap()`, output capture |
| **PersistentAdb** | `PersistentAdb.h`, `.cpp` | Persistent `adb shell` + persistent `adb exec-out screencap` |
| **NativeCapture** | `NativeCapture.h`, `.cpp` | Screen capture via ADB |
| **ProcessRunner** | `ProcessRunner.h`, `.cpp` | Hidden Windows process execution |
| **TouchInput** | `TouchInput.h`, `.cpp` | Input event device auto-detection (`AutoDetectTouchDevice`) |
| **ActionDebugLog** | `ActionDebugLog.h`, `.cpp` | CSV action-level debug logging |
| **Paths** | `Paths.h`, `.cpp` | AppData path resolution, backup folder creation |
| **Config** | `Config.h`, `.cpp` | Path validation |
| **RuntimeRelease** | `RuntimeRelease.h`, `.cpp` | Runtime string formatting for UI/RPC |
| **AhjieCrypto** | `AhjieCrypto.h`, `.cpp` | `.Ahjie` file AES-256-GCM encrypt/decrypt |
| **EmulatorControl** | bot/`EmulatorControl.*` | Minitouch startup, watchdog, revive heartbeat, emulator factory |
| **ScreenRecovery** | bot/`ScreenRecovery.*` | Cross detection, panel dismissal, zoom recovery |
| **AccountFiles** | bot/`AccountFiles.*` | Account backup/restore, injection, zoom hack |

---

## 6. Rooting & Boot Tools (`magisk_memu_boot/`)

Tools for Magisk-on-MEmu boot partition patching.

### Python tools
| Script | Purpose |
|--------|---------|
| `tools/patch_sda3_ramdisk_ext2.py` | Patches ramdisk in ext2 filesystem at sda3 offset |
| `tools/patch_dynamic_vhd_region.py` | Dynamic VHD region patcher/restorer for boot partition |

### Shell scripts (`magisk_memu_boot/scripts/`)
| Script | Purpose |
|--------|---------|
| `patch_magisk_standalone_ramdisk.sh` | Patches Magisk into a standalone ramdisk |
| `patch_magisk_nobackup_ramdisk.sh` | Patches Magisk without backup (space-saving) |
| `make_full_nocharger_ramdisk.sh` | Creates Magisk ramdisk without `/sbin/charger` |
| `write_magisk_ramdisk_to_sda3.sh` | Writes patched ramdisk to boot partition |
| `check_sda3_space.sh` | Checks available space on sda3 |
| `test_raw_sda3_write.sh` | Tests raw write access to sda3 |
| `debug_sda3_rw.sh` / `debug_sda3_rw_ext2.sh` | Debugging boot partition R/W |
| `finish_magisk_standalone_ramdisk.sh` | Completes Magisk ramdisk in guest |
| `install_lsposed_module.sh` | Installs LSPosed Zygisk module via Magisk |
| `install_zygiskfrida.sh` | Installs Zygisk-Frida module via Magisk |
| `install_riru_only.sh` | Installs Riru core module |
| `status_lsposed.sh` | Checks LSPosed status |
| `try_ramdisk_compression.sh` | Tests ramdisk compression variants |
| `inspect_magisk_db.sh` | Inspects Magisk database settings |
| `show_magisk_settings.sh` | Shows Magisk configuration |
| `decompress_test_magisk_ramdisk.sh` | Tests decompression of Magisk ramdisk |
| `check_kernel_rd_config.sh` | Checks kernel ramdisk config |
| `manual_install_lsposed.sh` / `official_install_lsposed.sh` | LSPosed installation variants |
| `partitions_sda.sh` | Lists partition table for sda |

### Boot partition images
| File | Purpose |
|------|---------|
| `sda3_stock_20260506_212225.img` | Stock boot partition backup |
| `sda3_magisk_full_nocharger.img` | Working Magisk patched boot partition |
| `sda3_magisk_full_xz.img` | Failed xz-compressed variant |
| `sda3_magisk_nobackup.img` | Failed no-backup variant |
| `ramdisk.magisk_full_nocharger.gz` | Working Magisk ramdisk |
| `ramdisk` | Stock ramdisk |

---

## 7. LSPosed Module (`lsposed_hayday_promon_aes/`)

An Xposed/LSPosed module for hooking Hay Day's Promon Shield and AES crypto at runtime.

| Path | Purpose |
|------|---------|
| `lsposed_hayday_promon_aes/` | Source (smali) project |
| `lsposed_hayday_promon_aes/build/` | Build outputs |
| `lsposed_hayday_promon_aes_signed/` | Signed APK output |
| `lsposed_hayday_promon_aes/AndroidManifest.xml` | Module manifest (targets `com.supercell.hayday`) |

**Note**: LSPosed Zygisk was found incompatible with MEmu Android 9 x86_64 (wedges userspace). The module source is preserved for reference.

---

## 8. Third-Party Downloads (`third_party/`)

| Item | Source | Purpose |
|------|--------|---------|
| `blutter/` | Blutter Dart snapshot reverse engineering tool | Analyzing `libapp.so` |
| `pc-dart/` | PC Dart tooling | Dart snapshot analysis |
| `SupercellProxy/` | Supercell proxy tool | Network interception |
| `zygisk_lsposed_frida/` | Zygisk-LSPosed-Frida bundle | Runtime hooking (unstable on MEmu) |
| `riru_path/riru-v26.1.7.r530...zip` | Riru core | Riru module installer |
| `riru_path/LSPosed-v1.9.2-7024-riru-release.zip` | LSPosed for Riru | Riru variant of LSPosed |
| `LSPosed-v1.9.2-7024-zygisk-release.zip` | LSPosed Zygisk | Zygisk variant — wedges MEmu |
| `ZygiskFrida-v1.9.0-release.zip` | Zygisk-Frida | Frida via Zygisk — wedges MEmu |
| `Magisk-v30.7.apk` | Magisk Manager | Boot-level root |

---

## 9. Visual Reference Assets (`Reference_Assets/`)

| File | Purpose |
|------|---------|
| `animals.fla` | Animal animations source (Adobe Flash) |
| `buildings_new.fla` | Building visuals source |

---

## 10. Build Tooling (HD Project)

| Tool | Location | Purpose |
|------|----------|---------|
| MSBuild | `D:\Programs\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe` | Builds `premium.exe` |
| MSVC compiler | `D:\Programs\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe` | C++ compilation |
| vcpkg | `D:\01_Apps\DevTools\vcpkg\` | Package manager for OpenCV, GLFW, etc. |
| Windows SDK | `C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0\` | Windows headers and libraries |
| VS Installer | `C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe` | Modify VS installation components |

---

## 11. Source Packages (`source_packages/`)

Original upstream APKM and APK downloads, including the Hay Day 1.70.96 ARM64 APKM bundle from APKMirror.

---

## 12. Analysis Artifacts (`analysis_artifacts/`)

Historical read-only data from the May 2026 reverse engineering investigation. See `analysis_artifacts/README.md`.

| Subfolder | Contents |
|-----------|----------|
| `account_exports/` | Captured account/storage XML exports |
| `apk_outputs/` | Loose generated APK outputs |
| `apkm_arm64_extract/` | Extracted ARM64 package files |
| `blutter_out_2026-05-06/` | Blutter Dart analysis output |
| `memscan_experiments/` | All historical `memscan_*` experiment folders |
| `native_promon_20260506/` | Native library/string extraction output |
| `pulled_data/` | Pulled device/app data and scan captures |
| `runtime_payload_20260506_1828/` | Runtime payload APK, dex, apktool output |
| `scratch_work/` | Temporary comparison/probe folders |

---

## 13. Documentation (`docs/`)

| File | Purpose |
|------|---------|
| `bot-architecture.md` | Bot design, account rotation, visual injection, crop detection |
| `code-evaluation-framework.md` | Project Wheat audit rubric: surfaces, weights, checklists, severity |
| `remaining-steps.md` | Tutorial-skip pipeline status (verified 2026-05-26) |
| `runtime-logging-and-performance.md` | Logging defaults, vision config, crop markers |
| `sctx-xcoder-patching.md` | Visual SCTX patching with XCoder toolchain |
| `storage-encryption.md` | `storage_new.xml` AES key derivation and decryption |
| `storage-layout.md` | Top-level folder organization (last updated 2026-06-08) |
| `system-cert-injection-and-network-analysis.md` | MITM proxy + cert injection approach |
| `ahjie-encryption.md` | `.Ahjie` file format (AES-256-GCM) |
| `code-maps/TOOLS_MAP.md` | **This file** |
| `code-maps/README.md` | Code-maps directory overview |

---

## 14. Python Virtual Environment

| Path | Python | Purpose |
|------|--------|---------|
| `venv_frida_16_5_6/` | Python 3.x with Frida 16.5.6 | Frida scripting and memory analysis |

---

## Tool Discovery Index

If you're looking for a specific tool, check these locations in order:

1. **`tools/`** — Primary Python/script tools (MITM, cert injection, analysis)
2. **`HD/cpp/src/infra/`** — C++ infrastructure modules (rebuild required)
3. **`magisk_memu_boot/scripts/`** — Rooting/boot shell scripts
4. **`docs/`** — Architectural and operational documentation
5. **`lsposed_hayday_promon_aes/`** — LSPosed module source
6. **`third_party/`** — Downloaded binaries and ZIPs
7. **`HD/tools/`** — PowerShell watcher scripts (minimal)
8. **`analysis_artifacts/`** — Historical RE outputs (read-only reference)
