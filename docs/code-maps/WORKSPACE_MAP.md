# HaydayMod Workspace Map

This workspace is a multi-repository, multi-language environment centered around Hay Day reverse engineering and bot automation. It includes:
- **`HD/` (`Project_Wheat`)**: A C++ visual-injection bot solution using ImGui and ADB.
- **`lsposed_hayday_promon_aes/`**: An LSPosed smali module for hooking Promon Shield and AES.
- **`magisk_memu_boot/scripts/`**: Shell scripts for patching boot partitions.
- **`tools/`**: Network interception, decryption, and certificate injection tools.
- **`docs/`**: Reverse engineering documentation, analysis sheets, and architecture notes.

---

## Active Modules & Directory Mapping

| Folder | Technology / Stack | Purpose | Start Map |
|---|---|---|---|
| `HD/` | C++ (MSVC, GLFW, OpenCV, ImGui, vcpkg) | Core multibot engine (`premium.exe`). Controls instances, virtual injection (color-based scanning), crop logic, OCR, and account rotation. | `code-maps/BOT_MAP.md` |
| `tools/` | Python 3.x, mitmproxy, WireGuard | MITM traffic proxy and cert injection tools to dump encrypted game traffic. | `code-maps/TOOLS_MAP.md` |
| `magisk_memu_boot/` | Bash scripts, Python | Magisk-on-MEmu boot partition patching scripts (sda3 ramdisk). | `code-maps/TOOLS_MAP.md` |
| `lsposed_hayday_promon_aes/` | Smali, Apktool | LSPosed/Xposed module for hooking Supercell Promon/AES crypto. | `code-maps/TOOLS_MAP.md` |
| `docs/` | Markdown | Deep architecture and design documentation (storage encryption, cert injection, etc.). | `docs/` |
| `_indexes/` | Text files | Frida and Hayday reverse engineering log indexes. | `_indexes/` |

---

## Shared Workspace Docs

| Path | Purpose |
|---|---|
| `Frida.md` | Frida hooking scripts and memory analysis commands. |
| `SESSION_HISTORY.md` | Log of research, debug steps, and analysis sessions. |
| `docs/bot-architecture.md` | High-level system design: visual injection, emulator coordination, and account rotation. |
| `docs/code-evaluation-framework.md` | Project Wheat code-quality audit rubric (surfaces, scores, checklists). |
| `docs/storage-encryption.md` | Details on `storage_new.xml` AES-128-ECB decryption/encryption. |
| `docs/sctx-xcoder-patching.md` | Workflow for patching texture files with the XCoder toolchain. |
| `docs/system-cert-injection-and-network-analysis.md` | Setup guide for mitmproxy CA trust in MEmu. |
| `docs/yajing-encryption.md` | Format analysis of `.YaJing` encrypted payloads. |

---

## Ignored & Archived Areas (Heavy Files)

These folders are excluded from Git version control due to size or transient nature:
- `HD/vcpkg_installed/` — Local C++ dependencies (managed by vcpkg).
- `analysis_artifacts/` (~472 MB) — Historical dumps from reverse engineering (read-only reference).
- `source_packages/` (~429 MB) — Untouched and backup upstream APKs/APKM bundles.
- `third_party/` (~612 MB) — Prebuilt reverse engineering tools (blutter, pc-dart, zygisk bundles).
- `Reference_Assets/` (~52 MB) — Flash `.fla` source vectors for animal/building assets.
- `venv_frida_16_5_6/` (~80 MB) — Python virtual environment for Frida.
