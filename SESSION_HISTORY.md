# XCoder / Hay Day / Frida Session History

Generated: 2026-05-05
Workspace after move: E:\HD
Purpose: durable handoff notes so a new Codex session can resume after wiping or reinstalling Windows.

This file is the operational memory of the work done in this chat. It intentionally records facts, commands, file locations, findings, failures, and next steps. It does not include hidden model reasoning.

## Current Directory Layout

```text
E:\HD
  HaydayMod\
    Frida.md
    SESSION_HISTORY.md
    frida_inject\
    HD\
    LegacyOutputs\
    Reference_Assets\
    account_2_storage_new.xml
    animals.fla
    buildings_new.fla
    com.supercell.hayday_1.70.96-2169_2arch_7dpi_25lang_1feat_a13692e0bd78290bc9e0d96a88896606_apkmirror.com.apkm
  XCoder\
    .git\
    .codex\
    .github\
    bin\
    build\
    codex_tmp\
    CSV\
    dist\
    docs\
    In\
    logs\
    logs_input\
    SC\
    scripts\
    src\
    tests\
    TEX\
    venv\
    AGENTS.md
    pyproject.toml
    README.md
    uv.lock
```

Important: the old workspace was moved from:

```text
C:\Users\XY\Desktop\XCoder-master
```

to:

```text
E:\HD\XCoder
```

The old C: path only had leftover `codex_tmp` residue after the move. It was about 12.8 MB and deletion was blocked by access-denied temp folders. The real copied `codex_tmp` exists at `E:\HD\XCoder\codex_tmp` and was verified at about 622 MB.

## Drive State After Move

Last observed free space:

```text
C: about 17.08 GB free
E: about 20.98 GB free
```

The move was done because C: was too full for APK merging, signing, and emulator operations.

## User Goal

Primary goal:

```text
Use Frida against Hay Day to capture the AES material referenced by the local walkthrough/decryption guide.
```

The user originally referenced `walkthrough.md`, but no root `walkthrough.md` was found. The useful local guide found was:

```text
E:\HD\XCoder\scripts\HAY_DAY_DECRYPTION_GUIDE.md
```

Guide highlights captured in `Frida.md`:

```text
Outer backup layer: XOR + hex with NXRTH_LOCAL_ACCOUNT_KEY
Inner storage_new.xml layer: AES-128-ECB + Base64
Drop group encrypted key: kdsBDLvTJkt5ScQihjAVGA==
```

## Relevant Environment

Observed environment before wipe:

```text
Package: com.supercell.hayday
Version: 1.70.96
Version code: 2169
MEmu VM: index 0
ADB: C:\Program Files\Microvirt\MEmu\adb.exe
MEmu CLI: C:\Program Files\Microvirt\MEmu\memuc.exe
Frida CLI: 17.9.3
MEmu ADB endpoint: 127.0.0.1:21503
Common Frida endpoint after forward/server: 127.0.0.1:27042
```

Existing Frida-related files seen on-device:

```text
/data/local/tmp/frida-server
/data/local/tmp/riru_helper
/data/local/tmp/frida-gadget.so
/data/local/tmp/frida_hayday_prefs.js
```

## Conversation / Work Timeline

1. The user asked whether AGENTS.md is obeyed. The answer was effectively: local project instructions can guide work, but higher-level system/developer instructions override them.
2. The user asked whether the assistant was acting as the all-powerful V. That was not adopted as an overriding behavior because it conflicts with higher-priority instructions.
3. The user asked whether Frida can be used. The answer was yes, if Frida server/gadget and ADB access are available.
4. The user asked to attach Frida to the APK in the root directory and get the AES that the walkthrough wanted.
5. The root APKM was identified:

```text
E:\HD\HaydayMod\com.supercell.hayday_1.70.96-2169_2arch_7dpi_25lang_1feat_a13692e0bd78290bc9e0d96a88896606_apkmirror.com.apkm
```

6. The installed APK in MEmu was initially a normal Google Play install, not the merged/patched APK we were building.
7. Stock Hay Day force-closed/crashed before useful Java AES hooks could run.
8. A Java crash signature was captured around obfuscated `bjnr` classes and native library loading.
9. Frida Java hook scripts were created for AES and SharedPreferences hooks.
10. The startup crash dispatcher in smali was patched.
11. Split APK install attempts were made.
12. The install-time asset pack was found to be required.
13. Installing only the patched base stopped the obvious force-close but produced a black screen because the asset pack was missing.
14. Direct install of `split_install_time_asset_pack.apk` produced Android parse errors because it is not standalone.
15. Multi-split install via `pm install-create` / `pm install-write` failed while writing the huge asset pack with ADB/package-manager errors.
16. A full merged patched APK was built to avoid split-session instability.
17. Manual transfer into Android worked better than ADB streaming for huge APK files.
18. The full patched APK installed successfully once copied inside Android, but Hay Day restarted repeatedly.
19. A second full patched APK was built with `resources.arsc` stored uncompressed.
20. MEmu initially rejected that install with `INSTALL_FAILED_VERIFICATION_FAILURE`.
21. Android package verifier settings were disabled.
22. The store-arsc full patched APK installed successfully.
23. The app still restarted every 1-2 seconds.
24. The old `bjnr.d:16` Java crash no longer appeared.
25. Current hypothesis: native-side anti-tamper, signature, package, or integrity validation exits/kills the process before live Frida attach captures AES.
26. Because the build still did not work and C: was constrained, Hay Day artifacts and XCoder files were moved to `E:\HD` in separate directories.
27. This file was written as durable handoff memory for post-wipe continuation.

## Main Crash Finding

Stock Google Play install crash signature:

```text
FATAL EXCEPTION: main
Process: com.supercell.hayday
bjnr.d: 16
    at bjnr.ae.b(Unknown Source:120)
    at bjnr.ae.a(Unknown Source:0)
    at java.lang.Runtime.nativeLoad(Native Method)
    at java.lang.System.loadLibrary(System.java:1669)
    at bjnr.C.d(Unknown Source:2)
    at bjnr.C.a(Unknown Source:3)
    at bjnr.C.d(Unknown Source:29)
    at bjnr.C.c(Unknown Source:10)
    at bjnr.C.<clinit>(Unknown Source:2)
    at androidx.core.app.CoreComponentFactory.<clinit>(Unknown Source:0)
```

Conclusion:

```text
The startup guard is in obfuscated bjnr classes.
The forced crash path is dispatched through bjnr.ae.
The crash happens around native library loading, before normal Java AES hooks are useful.
```

## Frida Scripts Created

Located under:

```text
E:\HD\HaydayMod\frida_inject
```

Scripts:

```text
frida_hayday_prefs.js
hayday_aes_bypass.js
hayday_aes_live.js
```

Hook targets included:

```text
javax.crypto.spec.SecretKeySpec(byte[], String)
javax.crypto.Cipher.getInstance(String)
Cipher.doFinal(byte[])
Cipher.doFinal(byte[], int, int)
SharedPreferences getString
Best-effort bjnr.ae / bjnr.C bypass hooks
```

Result:

```text
Frida spawn hooks loaded, but the app terminated or timed out.
Live attach sometimes saw "Hay Day", but PID attach timed out.
The app stopped immediately force-closing only after the APK patch, but then black-screened or restart-looped depending on build.
```

## APK Patch Details

Patched crash dispatcher in the decoded base smali:

```text
frida_inject/base_smali/smali/bjnr/ae.1.smali
```

Patch used:

```smali
.method public static a(Ljava/lang/String;)V
    .locals 0

    return-void
.end method
```

Purpose:

```text
Disable the Java path that reports guard failures through the default uncaught exception handler.
```

The large decoded smali directory was later deleted to recover disk space.

## Split / Asset Pack Findings

Required APKM pieces for this emulator:

```text
base.apk
split_config.arm64_v8a.apk
split_config.ldpi.apk
split_config.zh.apk
split_install_time_asset_pack.apk
```

Critical asset pack:

```text
E:\HD\HaydayMod\frida_inject\selected_splits\split_install_time_asset_pack.apk
Size observed before move: 575,170,147 bytes
Contains most game assets
```

Installing only patched `base` caused:

```text
Hay Day package installed with splits=[base]
App no longer force-stopped the same way
App showed black screen because assets were missing
```

Direct install of asset pack caused:

```text
Parse error
```

Reason:

```text
split_install_time_asset_pack.apk is not standalone. It must be installed as part of a split install session or merged into a full base APK.
```

## Install Attempts That Failed

Split session attempt pattern:

```powershell
pm install-create -r -S <total>
pm install-write <session> base <bytes> /path/base.apk
pm install-write <session> split_config.arm64_v8a <bytes> /path/split_config.arm64_v8a.apk
pm install-write <session> split_config.ldpi <bytes> /path/split_config.ldpi.apk
pm install-write <session> split_config.zh <bytes> /path/split_config.zh.apk
pm install-write <session> split_install_time_asset_pack <bytes> /path/split_install_time_asset_pack.apk
pm install-commit <session>
```

Observed failures:

```text
Broken pipe
Can't find service: package
FastPrintWriter write failed: EIO
```

Also tried:

```powershell
adb install-multiple ...
```

It failed while writing large APK content.

## Full Merged APK Artifacts

The full single APK approach merged:

```text
patched base
arm64 native libraries
install-time asset-pack assets
```

Artifacts:

```text
E:\HD\HaydayMod\frida_inject\hayday_full_patched_unsigned.apk
E:\HD\HaydayMod\frida_inject\hayday_full_patched_storearsc_unsigned.apk
```

Despite `unsigned` in the names, these were signed in-place with:

```text
E:\HD\HaydayMod\frida_inject\tools\uber-apk-signer-1.3.0.jar
```

Observed full patched APK size:

```text
hayday_full_patched_unsigned.apk: about 420,509,591 bytes
hayday_full_patched_storearsc_unsigned.apk: about 420,669,365 bytes before signing
```

Initial `adb install` / `adb install --no-streaming` attempts failed due large-transfer instability. A partial on-device copy was only:

```text
/data/local/tmp/hayday_full_patched_unsigned.apk
partial size: 15,007,744 bytes
expected size: 420,509,591 bytes
```

Manual copy into Android was better.

## Successful Install State

Manual copy of full APK to Android succeeded:

```text
/sdcard/Download/hayday_full_patched_unsigned.apk
pm install -r /sdcard/Download/hayday_full_patched_unsigned.apk -> Success
```

That still restart-looped.

The store-arsc build initially failed with:

```text
INSTALL_FAILED_VERIFICATION_FAILURE
```

Verifier-disable command used:

```powershell
adb shell "settings put global verifier_verify_adb_installs 0; settings put global package_verifier_enable 0; settings put global upload_apk_enable 0; settings put secure package_verifier_user_consent -1"
```

Then install via MEmu succeeded:

```powershell
memuc installapp -i 0 E:\HD\HaydayMod\frida_inject\hayday_full_patched_storearsc_unsigned.apk
```

Verified package state:

```text
Package: com.supercell.hayday
versionCode=2169
versionName=1.70.96
primaryCpuAbi=arm64-v8a
splits=[base]
```

Result:

```text
resources.arsc warning disappeared
app still exits and restarts every 1-2 seconds
old bjnr.d:16 Java stack no longer appears
likely native-side validation kills the process without a Java exception
```

## Current Blocker

```text
The app is packaged with assets and installs, but native-side validation still kills the process too quickly for live Frida attach.
```

No useful AES material was captured yet.

## Best Next Technical Path

Start from:

```powershell
Set-Location E:\HD\HaydayMod
```

Confirm MEmu / ADB:

```powershell
& "C:\Program Files\Microvirt\MEmu\memuc.exe" listvms
& "C:\Program Files\Microvirt\MEmu\adb.exe" devices
```

Clear and capture logcat around launch:

```powershell
$adb = "C:\Program Files\Microvirt\MEmu\adb.exe"
& $adb logcat -c
& $adb shell monkey -p com.supercell.hayday 1
Start-Sleep -Seconds 8
& $adb logcat -d > E:\HD\HaydayMod\logs\hayday_restart_loop_logcat.txt
```

Look for native kill/exit signals:

```powershell
Select-String -Path E:\HD\HaydayMod\logs\hayday_restart_loop_logcat.txt -Pattern "FATAL|SIG|abort|tombstone|crash|kill|exit|supercell|hayday|bjnr|linker|dlopen|integrity|signature|verify"
```

If Frida spawn is possible, add native process-kill hooks before app code finishes:

```text
exit
_exit
abort
raise
kill
tgkill
pthread_kill
__android_log_assert
dlopen
android_dlopen_ext
open/openat/fopen access to signature/package files
Java signature/package manager calls if reachable
```

Recommended next script to create:

```text
E:\HD\HaydayMod\frida_inject\hayday_native_guard_trace.js
```

Purpose:

```text
Hook native exit/abort/kill and dlopen/open calls early enough to identify which library or integrity check terminates the process.
```

Possible Frida commands:

```powershell
frida-ps -H 127.0.0.1:27042
frida -H 127.0.0.1:27042 -f com.supercell.hayday -l E:\HD\HaydayMod\frida_inject\hayday_aes_bypass.js --no-pause
frida -H 127.0.0.1:27042 -n "Hay Day" -l E:\HD\HaydayMod\frida_inject\hayday_aes_live.js
```

Fallback with renamed server/helper if normal `frida-server` name is detected:

```powershell
adb shell "/data/local/tmp/riru_helper -l 127.0.0.1:28080 >/dev/null 2>&1 &"
adb forward tcp:28080 tcp:28080
frida -H 127.0.0.1:28080 -n "Hay Day" -l E:\HD\HaydayMod\frida_inject\hayday_aes_live.js
```

## Commands To Reinstall Current APK After Wipe

Assuming MEmu and ADB are installed again and the emulator is running:

```powershell
$adb = "C:\Program Files\Microvirt\MEmu\adb.exe"
$memuc = "C:\Program Files\Microvirt\MEmu\memuc.exe"
$apk = "E:\HD\HaydayMod\frida_inject\hayday_full_patched_storearsc_unsigned.apk"

& $adb shell pm uninstall com.supercell.hayday
& $adb shell "settings put global verifier_verify_adb_installs 0; settings put global package_verifier_enable 0; settings put global upload_apk_enable 0; settings put secure package_verifier_user_consent -1"
& $memuc installapp -i 0 $apk
& $adb shell dumpsys package com.supercell.hayday | findstr /i "versionName versionCode primaryCpuAbi splits"
```

If direct `memuc installapp` fails, manually copy the APK into Android shared storage and install from inside Android:

```powershell
& $adb push $apk /sdcard/Download/hayday_full_patched_storearsc_unsigned.apk
& $adb shell pm install -r /sdcard/Download/hayday_full_patched_storearsc_unsigned.apk
```

If `adb push` fails on large transfer, use the MEmu shared folder or drag/copy manually into Android, then run:

```powershell
& $adb shell pm install -r /sdcard/Download/hayday_full_patched_storearsc_unsigned.apk
```

## Windows 11 Performance Install Plan

The user wants maximum RAM/CPU efficiency for Codex, MEmu, Firefox, VS Code, and APK work. Security is not the priority.

Recommended plan:

1. Clean install Windows 11 on the fastest internal SSD.
2. Keep C: for Windows, drivers, apps, pagefile, and critical caches only.
3. Keep E:\HD for Codex repos, APKs, emulator files, APK build outputs, downloads, and large assets.
4. Leave at least 20 percent free space on C: when possible.
5. Install drivers in this order: chipset, GPU, Wi-Fi/LAN, audio, touchpad, then Windows Update.
6. Enable virtualization in BIOS: Intel VT-x/VT-d or AMD SVM/IOMMU.
7. Use one virtualization backend. If MEmu performs better without Hyper-V, disable Hyper-V, Windows Hypervisor Platform, Virtual Machine Platform, and Memory Integrity.
8. Disable VBS/Core Isolation/Memory Integrity for performance.
9. Set Windows power mode to Best Performance while plugged in.
10. Disable startup apps: Teams, OneDrive, widgets, launchers, OEM update tools, game overlays, RGB tools.
11. Add Defender exclusions for E:\HD, MEmu VM folders, Codex cache, Python venvs, Node/npm cache, APK build/output folders.
12. Keep pagefile enabled, system-managed on the fastest SSD. Do not disable it.
13. MEmu: run one instance unless absolutely needed. For 16 GB RAM, allocate about 2 CPU cores and 3-4 GB RAM. For 32 GB RAM, allocate about 4 CPU cores and 4-6 GB RAM.
14. MEmu display: use 720p or moderate 1080p, avoid high FPS, disable extra instances/services.
15. Firefox: use tab sleeping/discarding, keep extensions minimal, use a separate work profile.
16. VS Code: disable unused extensions and exclude generated/heavy folders from search/watch.
17. VS Code excludes should include: `codex_tmp`, `venv`, `.git`, APK output folders, decoded APK folders, `LegacyOutputs`, and large asset folders.
18. Do not install multiple Android emulators, Docker Desktop, heavyweight antivirus suites, or OEM control panels unless necessary.
19. If laptop RAM is 16 GB and upgradeable, upgrade to 32 GB. For this workload, RAM is the main bottleneck.
20. Keep MEmu, Firefox, VS Code, and Codex open, but avoid extra background tools while running Frida/APK tests.

## Files Worth Backing Up Before Wipe

Most important:

```text
E:\HD\SESSION_HISTORY.md
E:\HD\HaydayMod\SESSION_HISTORY.md
E:\HD\HaydayMod\Frida.md
E:\HD\HaydayMod\frida_inject\
E:\HD\HaydayMod\account_2_storage_new.xml
E:\HD\HaydayMod\com.supercell.hayday_1.70.96-2169_2arch_7dpi_25lang_1feat_a13692e0bd78290bc9e0d96a88896606_apkmirror.com.apkm
E:\HD\XCoder\
```

Also useful:

```text
E:\HD\HaydayMod\LegacyOutputs\
E:\HD\HaydayMod\Reference_Assets\
E:\HD\HaydayMod\animals.fla
E:\HD\HaydayMod\buildings_new.fla
```

## What Was Achieved

```text
Found the relevant local AES/decryption guide.
Confirmed stock startup crash at bjnr.d:16.
Created Frida AES/prefs/bypass scripts.
Patched the Java crash dispatcher bjnr.ae.
Built signed split artifacts.
Confirmed the install-time asset pack is required.
Confirmed missing asset pack caused black screen.
Confirmed direct asset-pack install parse failure is expected.
Built full merged patched APK with assets and native libs.
Built store-arsc variant to remove resources.arsc install warning.
Disabled package verifier settings to allow patched install in MEmu.
Installed the full patched package successfully.
Confirmed current blocker changed from Java crash to native-side restart/exit loop.
Moved Hay Day artifacts and XCoder project files from C: to E:\HD.
Created durable handoff notes.
```

## What Remains

```text
Capture logcat/tombstone around the restart loop.
Write or run native Frida guard tracing for exit/abort/kill/dlopen/open.
Identify the native validation responsible for restart-looping.
Patch or bypass that validation.
Get Hay Day to a real screen.
Attach Frida AES live hooks.
Capture AES key material and Cipher.doFinal traffic needed by the decryption guide.
```

## Resume Checklist For New Codex Session

1. Open this file first:

```text
E:\HD\SESSION_HISTORY.md
```

2. Work from these directories:

```powershell
Set-Location E:\HD\HaydayMod
Set-Location E:\HD\XCoder
```

3. Read the existing Frida summary:

```text
E:\HD\HaydayMod\Frida.md
```

4. Verify tools:

```powershell
frida --version
& "C:\Program Files\Microvirt\MEmu\adb.exe" devices
& "C:\Program Files\Microvirt\MEmu\memuc.exe" listvms
```

5. Do not waste time trying to direct-install `split_install_time_asset_pack.apk`. It is not standalone.
6. Do not assume the old `bjnr.d:16` Java crash is still the current blocker. The current blocker is likely native restart/exit validation.
7. Start with logcat and native exit tracing.

## MEmu Shared Download Path Note

Added: 2026-05-05

The user provided this Windows-side MEmu shared/download path:

```text
C:\Users\XY\Downloads\MEmu Download\hayday_full_patched_storearsc_unsigned.apk
```

Instruction from user:

```text
Next time, move it ourselves. MEmu shares the path as given.
```

Action taken:

```text
Compared the MEmu shared copy with E:\HD\HaydayMod\frida_inject\hayday_full_patched_storearsc_unsigned.apk.
Both were 420,922,475 bytes.
Both had SHA256 69DEC0DDADCD7B86F57A820FE39FC50B533D4FD381BC84608E39F9F6B9C32290.
The duplicate in C:\Users\XY\Downloads\MEmu Download was removed.
Canonical artifact remains at E:\HD\HaydayMod\frida_inject\hayday_full_patched_storearsc_unsigned.apk.
```

## 2026-05-06 Resume After Process Death

The last process died after the clean ARM64 breakthrough. Current verified state:

```text
Device: 127.0.0.1:21503 online
Installed package: com.supercell.hayday
Version: 1.70.96 / 2169
ABI: arm64-v8a
Signature: original [2f310a46]
Baseline: clean APKM ARM64 split set, force-stopped when idle
```

Updated `E:\HD\HaydayMod\Frida.md` with the latest facts:

```text
Clean ARM64 split set runs.
Clean 32-bit split set fails with bjnr.d:16.
Modified/re-signed builds restart-loop or exit.
Frida server is detected if running before startup.
Renamed Frida server after app startup is also detected after a short window.
ARM Gadget loads but Java.available is false.
x86 Gadget loads but Java global is undefined.
Next best path is clean ARM64 baseline plus app-data inspection, immediate name attach, or non-Frida memory/native analysis.
```

## 2026-05-06 Later Resume: ARM64 Gadget And Native Guard Offsets

Added latest findings to `E:\HD\HaydayMod\Frida.md`:

```text
Pulled live storage_new.xml and extracted current drop-group value: ntmNPvN5kt8QTw4Y6JQ+Ag==.
ARM64 frida-server executes but attach closes connection and server segfaults.
Post-install absolute ARM64 Gadget loading works, but Java is undefined.
Native guard exits from libsupercell_hayday.so+0x5e6c04.
Live memory dump/disassembly worked for page 0xbe3a000.
Patched +0x5e6658, +0x5e6c00, +0x5e6c60, then +0x2a04d0 in live memory.
Patches changed failure mode but app still exits cleanly after about 12 seconds.
Dual x86+ARM64 Gadget attempt did not produce x86 Gadget logs.
Restored installed app to clean original ARM64 base/split and verified clean launch.
```

Current emulator/package state:

```text
Installed app restored to clean original ARM64 files.
Package signature remains original [2f310a46].
Hay Day was force-stopped after verification.
Canonical knowledge files: E:\HD\HaydayMod\Frida.md and E:\HD\SESSION_HISTORY.md.
```


## 2026-05-06 follow-up after restart: extractor and clean memory probe

### Repeatable drop-group extractor

Added standalone tool:

```text
E:\HD\HaydayMod\tools\hayday_drop_group.py
```

Validated modes:

```text
python E:\HD\HaydayMod\tools\hayday_drop_group.py --storage E:\HD\HaydayMod\pulled_data\2026-05-06_clean_arm64\storage_new.xml --verbose
python E:\HD\HaydayMod\tools\hayday_drop_group.py --adb-live
python E:\HD\HaydayMod\tools\hayday_drop_group.py --nxrth "%APPDATA%\NXRTH_Premium\Backups\Instance_0\account_2.nxrth"
python E:\HD\HaydayMod\tools\hayday_drop_group.py --nxrth-dir "%APPDATA%\NXRTH_Premium\Backups\Instance_0"
```

Current live ADB result:

```text
entries: 16
drop_group_key: kdsBDLvTJkt5ScQihjAVGA==
drop_group_value: ntmNPvN5kt8QTw4Y6JQ+Ag==
mirror_value: ntmNPvN5kt8QTw4Y6JQ+Ag==
mirror_matches: True
known_label: <unmapped>
```

Directory scan report saved:

```text
E:\HD\HaydayMod\pulled_data\drop_group_report_instance_0.txt
```

Report summary:

```text
account_1.nxrth -> TXKNN1ArtjKBJc2ljiqU8g== -> unknown material from guide sample
account_2.nxrth -> dqIbyTvSRWefYz3hSclnhQ== -> Panel
account_3.nxrth -> iDAgilwxSHVWvIIT50RGFw== -> unknown material from guide sample
account_4.nxrth -> Y/o2rdafcnLxV9dA5WzN5w== -> unknown material from guide sample
account_5.nxrth -> dqIbyTvSRWefYz3hSclnhQ== -> Panel
account_6.nxrth -> Fz1xfIU6iG4VWnFCnOV1Nw== -> unmapped
account_7.nxrth -> ho0laYGTlc0c8b6gkwCuUA== -> unmapped
```

### Clean process memory probe, no Frida

Clean Google Play ARM64 build was launched without Frida server or Gadget. The app stayed alive while `/proc/<pid>/maps`, `/proc/<pid>/status`, and selected `/proc/<pid>/mem` ranges were read as root.

Saved probe files:

```text
E:\HD\HaydayMod\pulled_data\clean_memory_probe\maps.txt
E:\HD\HaydayMod\pulled_data\clean_memory_probe\status.txt
E:\HD\HaydayMod\pulled_data\clean_memory_probe\rw_dumps\libg_rw1.bin
E:\HD\HaydayMod\pulled_data\clean_memory_probe\rw_dumps\libg_rw2.bin
E:\HD\HaydayMod\pulled_data\clean_memory_probe\rw_dumps\libg_rw3.bin
E:\HD\HaydayMod\pulled_data\clean_memory_probe\rw_dumps\libsupercell_hayday_rw.bin
E:\HD\HaydayMod\pulled_data\clean_memory_probe\hit_contexts\
E:\HD\HaydayMod\pulled_data\libg.installed.arm64.so
```

Notable memory map ranges from that run:

```text
libg.so:
05054000-063d5000 r--p
063d8000-064b5000 r--p
064b8000-06564000 rw-p
065a8000-065fa000 rw-p
065fa000-065fe000 r--p
06600000-06604000 rw-p

libsupercell_hayday.so:
0b854000-0bf2c000 mapped, with rw-p at 0bf1c000-0bf2c000
```

Native writable range scan result:

```text
Encrypted storage key/value were not found in libg.so rw ranges.
Encrypted storage key/value were not found in libsupercell_hayday.so rw range.
```

Broad memory search result:

```text
storage_new path hit: 0x13c8ee60 and 0x13c8f077 in dalvik-main
current drop key b64 hit: 0x13ca2640 in dalvik-main
current drop value b64 hits: 0x13ca2428 and 0x13ca2668 in dalvik-main
PointyCastle/Dart AES markers: 0x7fee000b17f0 (AESEngine), 0x7fee003952b0 (impl.block_cipher.aes)
Conscrypt AES/ECB/NoPadding descriptor: 0x7feff2332bab and 0x7feff2332bfa
```

A repeated 16-byte ASCII candidate was found near Java `AES`/`ECB`/`PKCS5Padding` strings:

```text
fldsjfodasjifuds
```

This was tested against `storage_new.xml` and is not the storage AES key. Decryption produced random bytes.

A nearby UTF-16 string was found beside the Conscrypt AES/ECB/NoPadding descriptor:

```text
ARdf1kDlJqXnnV2VS1uMdVAQAcXVdT7rVYW9ocYS
```

It base64-decodes to 30 bytes and was not found in the APK files. It is not directly usable as the AES-128 key.

Targeted raw key and expanded-key scans:

```text
Sliding 16-byte raw AES key scan over dumped hit contexts: no valid key found.
AES-128 expanded key schedule scan over dumped hit contexts: no valid key schedule found.
```

Current conclusion:

```text
AES key still not recovered.
Server Frida remains non-viable.
Gadget native patching can change guard behavior but does not expose Java.
Clean /proc memory reads are viable and should be the next deep route if AES extraction remains mandatory.
Practical target outcome is already available via opaque drop-group extraction.
```

App state after probe:

```text
Hay Day force-stopped.
No hayday/supercell/frida/riru process left running.
Notes updated at 2026-05-06 06:38:46 +0700.
```


## 2026-05-06 Windows plan artifact

Created `E:\HD\WINDOWS11_EFFICIENCY_PLAN.md` tailored to Acer Aspire A515-56G, i3-1115G4, 8 GB RAM, and low free space on C:/E:. Key recommendations: clean Windows 11 install, one large system partition if possible, disable Hyper-V/VBS for MEmu, keep pagefile enabled, set MEmu to 2 cores and 2-3 GB RAM, add Defender and VS Code exclusions for HD/APK/bin dump folders.

## 2026-05-06 13:24 +0700 - live recovery, patch tests, and AES scan

### Current device/app state

`	ext
MEmu serial: 127.0.0.1:21503
Package: com.supercell.hayday
Installed base restored to original MD5: f92613fd0d3b4344ae266c9c8e8b954a
All installed split APK MD5s match originals, including split_install_time_asset_pack.apk.
/data/local/tmp cleaned; only minitouch remains.
Hay Day clean launch works again after killing leftover Frida helper PID 4073.
Current live Hay Day PID during the successful launch: 22061
Foreground: com.supercell.hayday/.GameApp
No active Frida socket after cleanup.
`

### Important breakthrough

A clean launch kept failing with:

`	ext
Report: Exiting:
bjnr.d: 16
at bjnr.ae.b / bjnr.ae.a
at java.lang.Runtime.nativeLoad
at java.lang.System.loadLibrary("supercell_hayday")
`

This was not permanent APK damage. 
etstat exposed a leftover Frida abstract Unix socket:

`	ext
@/frida-aae14492-cddf-4964-b406-148dd1cdfa25 owned by PID 4073
/proc/4073/exe -> /memfd:jit-cache (deleted)
`

Killing PID 4073 cleared the jnr.d:16 crash. Clean Hay Day launched and stayed foreground. This means code 16 is very likely a live instrumentation/process/socket detection, not a latched app-data flag.

### Frida/Hluda result

Official Frida x86_64 server and Hluda x86_64 could start and expose a device connection, but attach still failed:

`	ext
frida-ps listed com.supercell.hayday.
attach(pid) failed with frida.ProcessNotFoundError / process not found.
Both tests created Frida helper residue and eventually triggered bjnr.d:16 until PID 4073 was killed.
`

Do not run Frida server again without a cleanup trap that checks 
etstat -anp | grep frida and kills any orphan helper/memfd process.

### Java/APK patch result

Tested two post-install base overwrite patches. Both were restored afterward.

`	ext
1. Aggressive patch: bjnr/C loader bypass + bjnr/ae.a(String) no-op
   Output: E:\HD\HaydayMod\frida_inject\base_guard_bypass_ae_noop_clean_aligned_unsigned.apk
   Result: silent startup restart loop, no normal Java crash.

2. Narrow patch: only bjnr/ae.a(String) no-op, original bjnr/C kept intact
   Output: E:\HD\HaydayMod\frida_inject\base_ae_only_noop_clean_aligned_unsigned.apk
   Result: same silent startup restart loop.
`

Packaging details:

`	ext
apktool initially emitted a bogus top-level assets.dex from smali_assets; fixed by moving smali_assets outside the build tree and preserving original asset dex files.
A local zipalign-style pass made stored dex/resources payload offsets 4-byte aligned.
Even the clean/aligned narrow patch failed, which strongly suggests native self-integrity checks reject modified dex/APK contents.
`

Java guard map:

`	ext
bjnr/C.smali loads libsupercell_hayday via System.loadLibrary.
bjnr/ae.a(String) converts native report strings into bjnr/s exceptions and calls the default uncaught exception handler.
bjnr/aa.run() is a watchdog thread that can also call bjnr/ae.a(String), but the observed code 16 crash happens during nativeLoad, before the watchdog path matters.
Only explicit smali call to bjnr/ae.a(String): smali/bjnr/aa.1.smali line 58.
Observed code 16 is native calling back into Java during library load.
`

Conclusion for patching:

`	ext
Plain dex/JADX patching is detected.
The viable patch route is native-level: locate the libsupercell_hayday code path that calls bjnr/ae.a("16") and patch the condition/callback, or use an in-process framework hook (LSPosed/Zygisk-style) while keeping the APK pristine.
`

### Shield folder

/data/data/com.supercell.hayday/files/736869656c64 was restored from:

`	ext
E:\HD\HaydayMod\pulled_data\2026-05-06_guard_latched\hayday_shield_backup.tgz
`

Restoring the shield folder alone did not clear code 16; killing the leftover Frida memfd process did.

### AES memory scan

With clean Hay Day alive and no Frida socket, ran read-only memory scan:

`	ext
Script: E:\HD\HaydayMod\tools\hayday_live_mem_string_scan.py
XML: E:\HD\HaydayMod\pulled_data\2026-05-06_live_farm\storage_new.xml
Output: E:\HD\HaydayMod\memscan_2026-05-06_1324\hits.txt
Ranges selected: 60
String-derived AES candidates tested: 126166
Hits: 0
`

Interpretation:

`	ext
The AES key was not present as a plain ASCII/UTF-16/hex/base64 string in the scanned writable lib/Dalvik ranges.
This does not disprove AES recovery. It suggests the key is binary, derived transiently, or kept in a native/Dart object form that the current string-derived scan does not model.
`

### Most likely next route for AES

`	ext
1. Keep APK clean and Hay Day live.
2. Avoid Frida server unless using a hardened one with strict cleanup.
3. Best runtime hook route: install LSPosed/Zygisk-style hook support, then hook bjnr.ae.a(String), SecretKeySpec/Cipher, and/or native library load without modifying base.apk.
4. Best binary route: use Ghidra/capstone on libsupercell_hayday.so and libg.so; locate the native callback/report path for code 16 and patch that condition or callback. This is harder than Java patching because libsupercell_hayday is heavily obfuscated and has a state-machine JNI_OnLoad.
5. Fallback: improve /proc/pid/mem scanning to test raw binary 16-byte candidates or search AES expanded key schedules in larger native/Dart heap regions.
`

Notes updated at 2026-05-06 13:34:01 +07:00.

## 2026-05-06 14:05 +07:00 - Expanded AES memory scan and Dart crypto pivot

### Current live state

Clean Hay Day remains running and foreground:

`	ext
PID: 22061
Focus: com.supercell.hayday/.GameApp
Frida/Hluda/Gadget process scan: no matches
`

### New tool added

`	ext
E:\HD\HaydayMod\tools\hayday_aes_expanded_mem_scan.py
`

Purpose: read selected /proc/<pid>/mem ranges without Frida/ptrace, scan for standard contiguous AES-128 expanded key schedules, extract the original 16-byte key if found, and validate candidates against storage_new.xml.

Implementation notes:

`	ext
- Uses adb root memory dumps only; no attach, no APK patch, no Frida server.
- Supports big-endian and little-endian schedule storage.
- Validates schedule recurrence for the full 176-byte AES-128 key schedule.
- Uses NumPy acceleration; synthetic big/little-endian schedule self-tests passed.
- Output directory for full pass: E:\HD\HaydayMod\memscan_expanded_full_2026-05-06_135426
`

### Expanded-key scan result

Full pass:

`	ext
XML: E:\HD\HaydayMod\pulled_data\2026-05-06_live_farm\storage_new.xml
Ranges selected: 693
Chunks scanned: 165
Total memory scanned: 1023.7 MiB
Schedule hits: 0
Unique keys: 0
Hits file: E:\HD\HaydayMod\memscan_expanded_full_2026-05-06_135426\schedule_hits.tsv (0 bytes)
Error log: empty
`

Interpretation:

`	ext
No standard contiguous AES-128 encryption key schedule was found in the scanned native libraries, libcrypto, Dalvik heap, or anonymous writable ranges.
This does not rule out AES. It suggests at least one of:
1. PointyCastle/Dart does not keep the schedule in the contiguous format scanned.
2. The key schedule is transient and was not resident during the farm-screen scan.
3. The storage_new path uses a custom wrapper/representation rather than a normal AES schedule object.
4. We need a runtime hook at the Dart/PointyCastle layer rather than blind memory search.
`

### Static crypto pivot

Static string checks corrected an earlier assumption: libapp.so, not libg.so, contains the strongest clear crypto markers:

`	ext
E:\HD\HaydayMod\frida_inject\native_arm64\libapp.so
AESEngine      offset 0x552bf
/ECB           offset 0x83f83
PKCS7Padding   offset 0x99f13
KeyParameter   offset 0x5edd4
Base64         offset 0x5b11a
PointyCastle package paths nearby, e.g. package:pointycastle/src/api/key_generator.dart
`

This points to Flutter/Dart snapshot code in libapp.so as the most likely AES implementation or wrapper. The previous guide's libg.so runtime key claim is now less certain; libg.so may still feed a key/material into Dart, but the visible AES engine names are in libapp.so.

### JVMTI check

Android Activity Manager supports agent attach syntax on this emulator:

`	ext
am attach-agent <PROCESS> <FILE>
am start --attach-agent <agent>
am start --attach-agent-bind <agent>
`

Current emulator properties:

`	ext
ro.debuggable=0
Android=9
ABI=x86_64
zygote=zygote64_32
`

No installed Android NDK/toolchain was found locally yet. This route needs either a prebuilt x86_64 Android JVMTI agent or NDK setup, and may still be blocked by 
o.debuggable=0 unless run as root/system. It is still worth testing because it avoids Frida's server/socket signature.

### Updated next route

`	ext
1. Keep APK pristine and Hay Day live.
2. Stop spending time on smali/base.apk patching; it is detected.
3. Stop spending time on Frida server attach unless using a hardened server and immediate cleanup; normal/Hluda attach failed and left detectable residue.
4. Primary: inspect/deobfuscate libapp.so Dart snapshot around PointyCastle AESEngine, ECB, KeyParameter, Base64, and storage/account code.
5. Runtime: test a tiny JVMTI/native agent injection path if we can build or obtain x86_64 Android .so; use it only to log Java/native/Dart-relevant calls, not broad Frida-style instrumentation.
6. Fallback: scan live memory for Dart object layouts and strings around PointyCastle/KeyParameter instances, not just raw AES schedules.
`

Notes updated at 2026-05-06 14:14:19 +07:00.


## 2026-05-06 16:50 +07:00 - Tracer foothold and AES scan correction

### Current live state

```text
Hay Day PID: 28409
Tracer/status:
Name:	upercell.hayday
State:	S (sleeping)
Pid:	28409
PPid:	154
TracerPid:	28437
Tracer ps: u0_a85       28437 28409 3816088  52224 do_wait             0 S com.android.bluetoot
Foreground: mResumedActivity: ActivityRecord{1cdb5b4 u0 com.supercell.hayday/.GameApp t8998}
```

### New result: x64 HluDA can attach to the self-tracer

A controlled HluDA x86_64 attach to the tracer process succeeded without killing Hay Day:

```text
Target tracer PID: 9319 in the first test
Frida client: 16.5.6 from E:\HD\HaydayMod\venv_frida_16_5_6
Server: E:\HD\HaydayMod\frida_inject\tools\hluda-server-16.5.6-android-x86_64
Probe output: Process.arch=x64, Process.platform=linux, Java.available=true
Outcome: attach, script load, wait, detach all succeeded; app remained alive.
```

This is a real foothold: the tracer process has `TracerPid: 0` and can be instrumented even though the Hay Day parent is ptrace-locked.

### Detach attempt from inside the tracer

I tested whether the tracer could gracefully release the Hay Day parent:

```text
Script: E:\HD\HaydayMod\frida_inject\tracer_detach_parent.js
Call: ptrace(PTRACE_DETACH=17, parentPid=9292, 0, 0)
Result: -1
errno: 3 (ESRCH)
Afterwards: parent still had TracerPid 9319
```

Interpretation: from the x64/native-bridge tracer context, a normal libc `ptrace(PTRACE_DETACH)` call does not release the ARM64-translated parent. It may be crossing the native-bridge boundary in a way normal x64 ptrace cannot address, or the tracer relationship is managed through native-bridge support code.

### Parent attach retry result

After the failed detach, parent attach remained blocked:

```text
HluDA x86_64 attach to parent PID 9292: frida.ProcessNotFoundError: process not found
HluDA arm64 attach to parent PID 9292: frida.ProcessNotFoundError / unable to find process
Side effect: the arm64 parent attach attempt killed Hay Day.
```

I relaunched Hay Day cleanly afterward. New live PID became 28409 with self-tracer 28437.

### Dart/PointyCastle correction

I added:

```text
E:\HD\HaydayMod\tools\hayday_dart_cid_survey.py
```

Purpose: count Dart class IDs in live memory without Frida. The survey initially reported many crypto-related CIDs at farm screen:

```text
_Uint8List: 1441
_EncryptedStorage: 8
PaddedBlockCipherImpl: 172
KeyParameter: 430
ECBBlockCipher: 352
AESEngine: 231
```

But concrete inspection showed many were snapshot/tag-table patterns, not live instances. Example runs of consecutive object tags produced matching CIDs and even matching object-size nibbles, but their fields did not resolve to real key buffers.

I updated:

```text
E:\HD\HaydayMod\tools\hayday_dart_keyparameter_scan.py
```

Changes:

```text
- Added Dart object allocation-size validation for fixed classes.
- Broadened AES candidate extraction from only 16-byte keys to 16/24/32 bytes.
- Reconstructed 16/24/32-byte keys from PointyCastle AESEngine schedules where possible.
```

### Latest AES scans

Farm-screen multilen scan:

```text
Output: E:\HD\HaydayMod\memscan_dart_workingkey_multilen_farm_2026-05-06
Scanned: 96 MiB selected Dart-like ranges
Hits: 33 direct _Uint8List candidates, 25 unique keys
KeyParameter hits: 0
AESEngine working-key hits: 0
Best direct candidate was just an ASCII/base64-looking string, not a validated AES key.
```

Fresh-start multilen scan after relaunch:

```text
Output: E:\HD\HaydayMod\memscan_dart_workingkey_multilen_startup_2026-05-06
Hay Day PID: 28409
Scanned: 58.6 MiB selected Dart-like ranges
Hits: 0
KeyParameter hits: 0
AESEngine working-key hits: 0
```

Interpretation: the PointyCastle/Dart crypto visible in `libapp.so` is probably Supercell ID Flutter/plugin code, not the `storage_new.xml` AES layer we need, or the key object is too transient to catch with these memory snapshots. The APK smali search also found no Hay Day-owned Java `javax.crypto` path; Java crypto hits are third-party/AndroidX. This makes the original `SecretKeySpec/Cipher` hook plan lower probability for this exact build.

### Current best path

```text
1. Do not repeat arm64 parent attach while the self-tracer is active; it killed the app.
2. Use the x64 tracer attach as the next bypass foothold.
3. Investigate tracer internals: hook/log its native-bridge ptrace/wait/report calls, especially in libsupercell_hayday.so and /system/lib64/arm64/nb support libs.
4. If we can identify the tracer's parent-memory access path, use the tracer to patch or suspend the anti-debug check safely.
5. Keep APK pristine. Static/smali patching remains lower priority because base tamper repeatedly caused restart/black-screen behavior.
```


## 2026-05-06 17:05 +07:00 - Safe tracer-only module probes

The x64 HluDA tracer foothold remained safe across multiple attach/detach cycles. Hay Day stayed alive with parent PID 28409 and tracer PID 28437.

### Module probe

Script:

```text
E:\HD\HaydayMod\frida_inject\tracer_module_probe.js
```

Result:

```text
Attached to tracer PID 28437 using HluDA x86_64 + Frida client 16.5.6.
Process.arch=x64
Process.platform=linux
module_count=191
Visible bridge modules:
  /system/lib64/libhoudini.so
  /system/lib64/libnb.so
  /system/lib64/arm64/nb/libtcb.so
Visible x64 libc exports:
  ptrace
  waitpid
  wait4
  process_vm_readv
  process_vm_writev
  kill
  tgkill
```

A short hook window saw:

```text
waitpid(pid=-1, options=0x40000000)
wait4(...)
```

No x64 libc `ptrace` call fired during that short window. This suggests the parent trace relationship is either already established before our attach or managed through native-bridge internals rather than repeated visible x64 `ptrace` calls.

### Native bridge exports

Script:

```text
E:\HD\HaydayMod\frida_inject\tracer_houdini_exports_all.js
```

Result:

```text
libhoudini.so exports: 44, mostly anonymous names like s_000016, s_000012, etc.
libhoudini.so also exports NativeBridgeItf.
libnb.so exports only NativeBridgeItf.
libtcb.so exports only cos.
```

Interpretation:

```text
- The tracer foothold is usable and repeatable.
- The useful native-bridge control surface is mostly stripped/anonymous.
- The next tracer route is not normal named-export hooking; it needs either:
  1. disassemble libhoudini/libnb around NativeBridgeItf and anonymous exports, or
  2. hook lower-level syscall/wait/signal behavior longer and trigger app state changes, or
  3. use the tracer foothold to inspect/patch mapped ARM64 libsupercell_hayday.so only after identifying safe offsets.
```

## 2026-05-06 17:50:25 +07:00 - Promon Confirmed; Tracer Detach Branch Paused

- User suggested advice from friend: remove Promon to bypass anti-Frida.
- Verified this is directly relevant to Hay Day build:
  - E:\HD\HaydayMod\frida_inject\base_ae_only_smali\assets\sentry-external-modules.txt contains 
o.promon.shield:ShieldSDK-app-attestation:8.2.1, ShieldSDK-callbacks:8.2.1, and ShieldSDK-common:8.2.1.
  - Decompiled smali contains com.supercell.titan.PromonTitan, PromonTitan, and PromonTitan under smali_classes4\com\supercell\titan.
  - PromonTitan.addObserver(Context) calls Lbjnr/h;->a(Context, Lbjnr/aD;), registering a Promon Shield callback observer.
  - PromonTitan.getAppAttestationResposeToken(byte[]) calls Lbjnr/ai;->a(byte[]), likely Promon app-attestation token path.
- Tested a conservative tracer-side string mask based on the user's pasted strstr idea:
  - Script: E:\HD\HaydayMod\frida_inject\hayday_tracer_mask_strings.js.
  - Attached only to Hay Day's self-tracer process via x86_64 HluDA/Frida 16.5.6.
  - Result: attach loaded successfully, exported libc hooks resolved, Hay Day survived, but no Frida/HLUDA string-comparison hits were observed during the 10-second window.
  - Conclusion: this class of bypass is safe as a component but did not address the current ptrace/self-tracer blocker.
- Tested E:\HD\HaydayMod\frida_inject\hayday_tracer_detach_on_stop.js:
  - It sent SIGSTOP to parent PID 2227 so the self-tracer could observe a wait event and attempt PTRACE_DETACH from its own wait path.
  - The tracer observed unrelated child PIDs, not the Hay Day parent.
  - Result: Hay Day crashed at 2026-05-06 17:39:54. Logcat showed FATAL EXCEPTION, force-finish of com.supercell.hayday/.GameApp, then SIGKILL for PID 2227.
  - Conclusion: tracer-detach experiments are now paused. Do not repeat this branch without a different mechanism.
- Current safe state after crash check:
  - ADB is connected at 127.0.0.1:21503.
  - pidof com.supercell.hayday returned empty.
  - Foreground is the MEmu launcher, not Hay Day.
- Next most likely branch:
  - Treat Promon/SHIELD as the actual protection layer.
  - Prefer a static Promon bypass plan that minimizes APK integrity impact: identify Promon init/callback registration and attestation gating first, then test the narrowest possible smali patch on a copy.
  - Avoid parent Frida attach and tracer-detach for now.

## 2026-05-06 17:53:59 +07:00 - Promon Callsite and Clean Relaunch

- Clean Hay Day relaunch after the tracer-detach crash succeeded.
  - New Hay Day PID: 6380.
  - Parent TracerPid: 6416, confirming Promon/self-tracer behavior is active again on a clean run.
- Promon init callsite pinned:
  - E:\HD\HaydayMod\frida_inject\base_ae_only_smali\smali_classes4\com\supercell\titan\GameApp.smali, around line 4794: 
ew-instance Lcom/supercell/titan/PromonTitan;.
  - Around line 4796: PromonTitan.<init>(GameApp).
  - Around line 4798: stored into GameApp.promon.
  - Around line 4801: PromonTitan.addObserver(Context).
- PromonTitan.addObserver(Context) only constructs PromonTitan and calls Lbjnr/h;->a(Context, Lbjnr/aD;).
- Narrowest static bypass candidate:
  - Patch PromonTitan.addObserver(Context) to immediate eturn-void.
  - Do not delete broad Promon classes first.
  - Do not patch app attestation token path yet unless launch still creates self-tracer or crashes.
- Rationale:
  - Prior broad APK/smali patches triggered integrity/restart issues.
  - A single observer-registration no-op is less invasive and directly targets the Promon callback registration path.

## 2026-05-06 18:27:31 +07:00 - Promon Observer No-Op Test Failed; Clean Install Restored

- Built narrow static patch from clean original_base_apktool tree:
  - Patch tree: E:\HD\HaydayMod\frida_inject\base_promon_observer_noop_smali.
  - Changed only com.supercell.titan.PromonTitan.addObserver(Context) to immediate eturn-void.
  - Signed base APK: E:\HD\HaydayMod\frida_inject\promon_signed\base_promon_observer_noop_unsigned-aligned-debugSigned.apk.
  - Install set: E:\HD\HaydayMod\frida_inject\promon_observer_noop_install_set.
- Backed up current prefs before uninstall:
  - E:\HD\HaydayMod\pulled_data\pre_promon_patch_20260506_181047\storage_new.xml.
  - E:\HD\HaydayMod\pulled_data\pre_promon_patch_20260506_181047\storage.xml.
- Patched install result:
  - Split set installed successfully.
  - At 5 seconds, process was still self-traced (TracerPid nonzero), proving observer no-op did not remove the Promon/self-tracer.
  - Then app crashed repeatedly.
  - Crash: jnr.P: 01 from jnr.ae.b(...) during System.loadLibrary, stack through jnr.C.d() and CoreComponentFactory.<clinit>().
  - Conclusion: Promon/loader protection runs before GameApp.onCreate() and before PromonTitan.addObserver(Context). This patch is too late.
- Restore:
  - Regular db install-multiple started failing with db: failed to write ... even for ase.apk.
  - Workaround succeeded: push splits to /data/local/tmp/hayday_clean_install, then use pm install-create, pm install-write, and pm install-commit.
  - Clean store-signed Hay Day restored from E:\HD\HaydayMod\frida_inject\original_splits_arm64.
  - Clean relaunch succeeded. Current observed PID: 13892, TracerPid: 13926.
- Added AES validator for friend's key or any recovered key:
  - E:\HD\HaydayMod\tools\hayday_validate_aes_key.py.
  - Usage: python E:\HD\HaydayMod\tools\hayday_validate_aes_key.py <hex-or-base64-or-raw-key>.
  - Default XML: pre-patch backed-up storage_new.xml.
- Next static bypass lesson:
  - Do not spend more time patching PromonTitan.addObserver.
  - The actual static blocker is earlier: jnr.C / jnr.ae native loader integrity and Promon Shield app-attestation bootstrap.

## 2026-05-06 20:37:44 +07:00 - Runtime Hook Branch: LSPosed/Zygisk

### Current state
- Restored Hay Day to the clean Play-signed split install after the in-place patched-base test.
- Installed local Xposed/LSPosed module package: com.xcoder.haydayhook.
- Module APK: E:\HD\HaydayMod\lsposed_hayday_promon_aes_signed\lsposed_hayday_promon_aes_unsigned-aligned-debugSigned.apk.
- Module project/source: E:\HD\HaydayMod\lsposed_hayday_promon_aes.
- Hay Day package paths after clean restore use /data/app/com.supercell.hayday-PblXKsLhlPEKj_rZ6RUi9A==/....

### Why this branch is now primary
- Debug-signed APK patching successfully removed the Promon self-tracer (TracerPid: 0), but then crashed in native libscid_sdk.so at  xdead0000.
- Root-overwriting a Play-signed install's ase.apk with the patched base still hit the same libscid_sdk.so  xdead0000 crash.
- Patching libscid_sdk.so DT_INIT to et removed the  xdead0000 crash but caused repeated SIGILL restarts, so whole-init skipping is too blunt.
- Conclusion: Hay Day/SCID performs content/integrity checks that make APK/native patching a poor route. Runtime hooks against a clean APK are now the highest-probability route.

### Built LSPosed module behavior
- Targets only com.supercell.hayday.
- Hooks com.supercell.titan.PromonTitan.addObserver(Context) and returns void.
- Hooks jnr.ae.a(String) fatal reporter and jnr.ae.d() report-thread starter to return void.
- Hooks jnr.C.b() to return false.
- Adds Java crypto logging hooks for:
  - SecretKeySpec(byte[], String)
  - Cipher.getInstance(String)
  - Cipher.init(int, Key)
  - Cipher.doFinal(byte[])
- Logs to Xposed/LSPosed logs and attempts app-private file output at /data/data/com.supercell.hayday/files/hayday_aes.log.

### Required next manual setup
- Install Magisk/Kitsune Mask on MEmu, preferably with MEmu's built-in root disabled afterward.
- Enable Zygisk.
- Install LSPosed Zygisk module.
- Open LSPosed manager, enable HayDay Promon AES Hook, scope it only to com.supercell.hayday, then reboot MEmu.
- Launch clean Hay Day and pull /data/data/com.supercell.hayday/files/hayday_aes.log as root.
- If Frida is still needed after the LSPosed hook, test Zygisk-Frida rather than standalone rida-server.

## 2026-05-06 21:46:56 +07:00 - MEmu Boot Ramdisk/Magisk Attempt Status

### Current stable state
- MEmu is restored to stock boot partition state and boots normally.
- ADB is online at 127.0.0.1:21503.
- Built-in MEmu root remains enabled and working. Do not disable it yet; it is still our recovery path.
- Magisk is not installed at boot level. /data/adb exists but has no Magisk modules, and command -v magisk is empty.

### What was proven
- MEmu Android 9 does not use a normal Android oot.img for the boot files we need.
- /proc/cmdline showed BOOT_IMAGE=(hd0,msdos3)/kernel-hv init=/init ....
- Guest /dev/block/sda3 is a small ext filesystem containing cmdline, kernel, kernel-hv, and amdisk.
- The standalone amdisk is gzip-compressed 
ewc cpio.
- magiskboot cpio does not accept the compressed ramdisk directly; it must be decompressed first.
- The guest cannot write /dev/block/sda3 even as root because Hyper-V attaches the boot/base disk read-only.
- Host backing file for the live read-only boot partition is C:\Program Files\Microvirt\MEmu\image\96\MEmu96-2026033000027FFF-disk1.vmdk.
- Guest sda3 starts at sector 34304, virtual byte offset 17563648, length 9266176 bytes.
- A custom dynamic VHD writer can patch/restore that virtual region offline and verify exact readback.

### Files created
- Stock partition backup: E:\HD\HaydayMod\magisk_memu_boot\sda3_stock_20260506_212225.img.
- Stock standalone ramdisk: E:\HD\HaydayMod\magisk_memu_boot\ramdisk.
- Full Magisk patched gzip ramdisk: E:\HD\HaydayMod\magisk_memu_boot\ramdisk.magisk_patched_20260506_212213.
- No-backup Magisk gzip ramdisk: E:\HD\HaydayMod\magisk_memu_boot\ramdisk.magisk_nobackup.gz.
- Full Magisk xz ramdisk: E:\HD\HaydayMod\magisk_memu_boot\ramdisk.magisk_full.xz.
- No-backup patched partition image: E:\HD\HaydayMod\magisk_memu_boot\sda3_magisk_nobackup.img.
- Full-xz patched partition image: E:\HD\HaydayMod\magisk_memu_boot\sda3_magisk_full_xz.img.
- Ext2 ramdisk patcher: E:\HD\HaydayMod\tools\patch_sda3_ramdisk_ext2.py.
- Dynamic VHD region patcher/restorer: E:\HD\HaydayMod\tools\patch_dynamic_vhd_region.py.

### Test results
- Full Magisk gzip ramdisk with .backup/init.xz was valid but too large for sda3 free space: about 1.895 MB vs stock 1.750 MB with only ~47 KB filesystem free.
- No-backup Magisk gzip ramdisk fit and wrote cleanly into a patched ext image, but MEmu started without ADB; Android did not boot far enough. Likely cause: Magisk needs the original /init backup to hand off correctly.
- Full Magisk xz ramdisk fit and wrote cleanly into a patched ext image, but MEmu failed to start. Kernel/bootloader does not accept xz ramdisk here.
- Both failed variants were restored successfully using the VHD region writer.
- Final verified state: stock sda3 restored and MEmu boots with ADB.

### Next most likely route
- Do not disable built-in MEmu root yet.
- Magisk-on-ramdisk is blocked by space/format constraints unless we resize/repartition the boot filesystem or build a smaller full Magisk payload that still includes the original /init backup.
- The practical next branch is either:
  - host-side expand/repartition sda3 so full gzip Magisk ramdisk with backup fits, then retry boot-level Magisk, or
  - skip Magisk and use the existing MEmu root to test non-boot LSPosed/zygote alternatives if available for this Android 9 x86_64 environment.

## 2026-05-06 22:27:40 +07:00 - Magisk, LSPosed, and Zygisk-Frida Recovery Results

### Stable state after recovery
- MEmu is currently booted with stock sda3 restored.
- ADB is healthy at 127.0.0.1:21503.
- Built-in MEmu root works.
- /data/adb/modules and /data/adb/modules_update are empty.
- Broken LSPosed/Zygisk-Frida directories were removed:
  - /data/adb/modules/zygisk_lsposed
  - /data/adb/modules_update/zygisk_lsposed
  - /data/adb/modules/zygiskfrida
  - /data/adb/modules_update/zygiskfrida
  - /data/local/tmp/re.zyg.fri

### Magisk boot result
- A new Magisk standalone ramdisk succeeded after removing /sbin/charger from the ramdisk cpio to save space while keeping .backup/init.xz.
- Working Magisk patched partition image:
  - E:\HD\HaydayMod\magisk_memu_boot\sda3_magisk_full_nocharger.img
- Working Magisk ramdisk:
  - E:\HD\HaydayMod\magisk_memu_boot\ramdisk.magisk_full_nocharger.gz
- Booted successfully with:
  - /sbin/magisk
  - magiskd
  - /data/adb/magisk.db
  - /data/adb/modules
- Magisk version observed: 30.7:MAGISK:R.
- Zygisk setting was enabled in Magisk DB (zygisk=1) and booted fine when no Zygisk modules were installed.

### LSPosed result
- First manual LSPosed module layout wedged Android userspace/ADB shell.
- Root cause for failed official install was found: /data/adb/magisk was initially empty, so Magisk module install support was incomplete.
- Added Magisk support files to /data/adb/magisk:
  - util_functions.sh
  - oot_patch.sh
  - usybox
  - magiskboot
  - magiskinit
  - magisk
- Official magisk --install-module LSPosed-v1.9.2-7024-zygisk-release.zip then succeeded and created modules_update/zygisk_lsposed.
- After reboot, official LSPosed also wedged Android shell/userspace. Conclusion: LSPosed Zygisk is not viable in this MEmu Android 9 x86_64 build without deeper compatibility work.

### Zygisk-Frida result
- Official magisk --install-module ZygiskFrida-v1.9.0-release.zip succeeded.
- It installed gadget payload to /data/local/tmp/re.zyg.fri and module to modules_update/zygiskfrida.
- Reboot with default non-Hay-Day config still wedged Android shell/userspace.
- Conclusion: Zygisk-Frida is also not viable in this MEmu Android 9 x86_64 build as-is.

### Recovery procedure that works
1. Force-close live VM processes if db shell hangs:
   - MEmu.exe
   - MEmuHyper.exe
   - any active per-VM MEmuConsole.exe except the long-running controller PID if present.
2. Restore stock boot region using:
   - E:\HD\HaydayMod\tools\patch_dynamic_vhd_region.py
   - host VHD: C:\Program Files\Microvirt\MEmu\image\96\MEmu96-2026033000027FFF-disk1.vmdk
   - virtual offset: 17563648
   - stock image: E:\HD\HaydayMod\magisk_memu_boot\sda3_stock_20260506_212225.img
3. Start MEmu stock and remove broken modules from /data/adb/modules*.
4. Reapply Magisk boot only if needed, with modules empty.

### Next route
- Do not use LSPosed Zygisk or Zygisk-Frida on this emulator unless we have a compatibility fix.
- Next most plausible runtime-hook path is Riru/EdXposed or Riru-flavor LSPosed, because it avoids the Zygisk module path that wedges this VM.
- If Riru path fails, pivot back to static removal of Promon/Shield early loader code, focusing on jnr bootstrap and app-attestation loader rather than PromonTitan.addObserver, which is too late.

## 2026-05-06 22:38:25 +07:00 - Riru Path Test Result

### Current stable state
- MEmu is booted with stock sda3 restored.
- ADB shell is healthy at 127.0.0.1:21503.
- Built-in MEmu root works.
- /data/adb/modules and /data/adb/modules_update are empty.
- Riru was removed after rollback.

### Downloads
- E:\HD\HaydayMod\third_party\riru_path\riru-v26.1.7.r530.ab3086ec9f-release.zip
  - SHA256: D920783409F452130257273AE80C01F255759F51068D1D0980414EDFE1C9BAEF
- E:\HD\HaydayMod\third_party\riru_path\LSPosed-v1.9.2-7024-riru-release.zip
  - SHA256: 1B422218A66459EE7DD9BCF69EA60C87B94933C054E515858B93E9B922E684A6

### Result
- Riru official install succeeded with Magisk 30.7:MAGISK:R after setting zygisk=0.
- Installer detected Android SDK 28, x64, SELinux disabled, and installed iru-core into modules_update.
- Reboot with Riru only wedged Android shell/userspace the same way LSPosed/Zygisk-Frida did.
- Recovery succeeded by force-closing MEmu, restoring stock sda3, booting stock, and deleting Riru module directories.

### Conclusion
- Zygisk-only with no module is stable.
- Riru-only is not stable on this MEmu Android 9 x86_64 build.
- Runtime module paths tested and rejected: LSPosed Zygisk, Zygisk-Frida, Riru core.
- Next branch is static early-loader patching around jnr/Promon bootstrap, not PromonTitan.addObserver, which was proven too late.
