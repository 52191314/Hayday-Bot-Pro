# Frida / Hay Day AES Worklog

Last updated: 2026-05-06

## Goal

Use Frida or an equivalent runtime method against Hay Day to recover the AES material referenced by XCoder's local decryption guide.

No root `walkthrough.md` was found. The useful guide is:

```text
E:\HD\XCoder\scripts\HAY_DAY_DECRYPTION_GUIDE.md
```

Guide-relevant facts:

```text
Outer backup layer: XOR + hex with NXRTH_LOCAL_ACCOUNT_KEY
Inner storage_new.xml layer: AES-128-ECB + Base64
Known encrypted drop-group key sample: kdsBDLvTJkt5ScQihjAVGA==
Likely needed runtime secret: native-generated AES key used by Hay Day storage code
```

## Current Environment

```text
Package: com.supercell.hayday
Version: 1.70.96
Version code: 2169
Current installed ABI: arm64-v8a
Current installed signature: original APKM/Play signature [2f310a46]
MEmu VM: index 0
ADB: C:\Program Files\Microvirt\MEmu\adb.exe
MEmu CLI: C:\Program Files\Microvirt\MEmu\memuc.exe
Device endpoint: 127.0.0.1:21503
Frida CLI/server/Gadget: 17.9.3
Canonical Hay Day workspace: E:\HD\HaydayMod
Canonical XCoder workspace: E:\HD\XCoder
```

Current installed package state verified after the last crash/restart:

```text
Package [com.supercell.hayday]
primaryCpuAbi=arm64-v8a
versionCode=2169 minSdk=24 targetSdk=35
versionName=1.70.96
signatures=[2f310a46]
```

## Important Artifacts

```text
E:\HD\HaydayMod\com.supercell.hayday_1.70.96-2169_2arch_7dpi_25lang_1feat_a13692e0bd78290bc9e0d96a88896606_apkmirror.com.apkm
E:\HD\HaydayMod\frida_inject\original_splits_arm64\base.apk
E:\HD\HaydayMod\frida_inject\original_splits_arm64\split_config.arm64_v8a.apk
E:\HD\HaydayMod\frida_inject\original_splits_arm64\split_config.ldpi.apk
E:\HD\HaydayMod\frida_inject\original_splits_arm64\split_config.zh.apk
E:\HD\HaydayMod\frida_inject\original_splits_arm64\split_install_time_asset_pack.apk
E:\HD\HaydayMod\frida_inject\original_splits\base.apk
E:\HD\HaydayMod\frida_inject\original_splits\split_config.armeabi_v7a.apk
E:\HD\HaydayMod\frida_inject\selected_splits\
E:\HD\HaydayMod\frida_inject\base_gadget_smali\
E:\HD\HaydayMod\frida_inject\tools\frida-gadget-arm.so
E:\HD\HaydayMod\frida_inject\tools\frida-gadget-arm64.so
E:\HD\HaydayMod\frida_inject\tools\frida-gadget-x86.so
E:\HD\HaydayMod\frida_inject\tools\frida-server
E:\HD\HaydayMod\frida_inject\tools\debug.keystore
E:\HD\HaydayMod\frida_inject\tools\apktool.jar
E:\HD\HaydayMod\frida_inject\tools\uber-apk-signer-1.3.0.jar
```

Generated or useful scripts:

```text
E:\HD\HaydayMod\frida_inject\frida_hayday_prefs.js
E:\HD\HaydayMod\frida_inject\hayday_aes_bypass.js
E:\HD\HaydayMod\frida_inject\hayday_aes_live.js
E:\HD\HaydayMod\frida_inject\hayday_native_guard_trace.js
E:\HD\HaydayMod\frida_inject\hayday_exit_trace_min.js
E:\HD\HaydayMod\frida_inject\hayday_gadget_block_exit.js
E:\HD\HaydayMod\frida_inject\hayday_gadget_autoload.js
E:\HD\HaydayMod\frida_inject\hayday_java_gadget_autoload.js
E:\HD\HaydayMod\frida_inject\attach_probe.js
E:\HD\HaydayMod\frida_inject\make_gadget_splits.py
E:\HD\HaydayMod\frida_inject\make_gadget_script_split.py
E:\HD\HaydayMod\frida_inject\merge_apk.py
E:\HD\HaydayMod\frida_inject\merge_armeabi_from_full.py
```

## Main Findings

### 1. Clean ARM64 APKM Split Set Runs

The original ARM64 split set from APKMirror installs and launches on MEmu.

Required split set:

```text
base.apk
split_config.arm64_v8a.apk
split_config.ldpi.apk
split_config.zh.apk
split_install_time_asset_pack.apk
```

Install method that worked:

```powershell
$adb='C:\Program Files\Microvirt\MEmu\adb.exe'
$share='C:\Users\XY\Downloads\MEmu Download'
$splits='E:\HD\HaydayMod\frida_inject\original_splits_arm64'
$files=@(
  @{Name='orig64_base.apk'; Src=(Join-Path $splits 'base.apk'); Split='base'},
  @{Name='orig64_split_config.arm64_v8a.apk'; Src=(Join-Path $splits 'split_config.arm64_v8a.apk'); Split='config.arm64_v8a'},
  @{Name='orig64_split_config.ldpi.apk'; Src=(Join-Path $splits 'split_config.ldpi.apk'); Split='config.ldpi'},
  @{Name='orig64_split_config.zh.apk'; Src=(Join-Path $splits 'split_config.zh.apk'); Split='config.zh'},
  @{Name='orig64_split_install_time_asset_pack.apk'; Src=(Join-Path $splits 'split_install_time_asset_pack.apk'); Split='split_install_time_asset_pack'}
)
foreach($f in $files){ Copy-Item -LiteralPath $f.Src -Destination (Join-Path $share $f.Name) -Force }
& $adb shell am force-stop com.supercell.hayday 2>$null | Out-Null
& $adb shell pm uninstall com.supercell.hayday 2>$null | Out-Null
$total=0; foreach($f in $files){ $total += (Get-Item -LiteralPath (Join-Path $share $f.Name)).Length }
$create=& $adb shell pm install-create -r --abi arm64-v8a -S $total
$text=($create -join "`n"); $session=([regex]::Match($text,'\[(\d+)\]').Groups[1].Value)
foreach($f in $files){
  $size=(Get-Item -LiteralPath (Join-Path $share $f.Name)).Length
  & $adb shell pm install-write -S $size $session $($f.Split) /sdcard/Download/$($f.Name)
}
& $adb shell pm install-commit $session
```

Observed after launch:

```text
Process stays alive.
Activity displays as com.supercell.hayday/.GameApp.
Logs show Flutter bridge/channel setup and Play services startup.
No immediate bjnr.d:16 crash when no Frida server is running.
```

Conclusion:

```text
Use clean ARM64 splits as the stable baseline.
Do not return to patched merged APKs unless specifically testing tamper behavior.
```

### 2. Clean 32-bit APKM Split Set Fails On MEmu

The original 32-bit split path installs with:

```text
primaryCpuAbi=armeabi-v7a
```

But it crashes at startup:

```text
FATAL EXCEPTION: main
Process: com.supercell.hayday
bjnr.d: 16
    at bjnr.ae.b(...)
    at bjnr.ae.a(...)
    at java.lang.Runtime.nativeLoad(...)
    at java.lang.System.loadLibrary(...)
    at bjnr.C.d(...)
    at bjnr.C.a(...)
    at bjnr.C.d(...)
    at bjnr.C.c(...)
    at bjnr.C.<clinit>(...)
    at androidx.core.app.CoreComponentFactory.<clinit>(...)
```

Conclusion:

```text
The 32-bit/Houdini path is bad on this emulator.
The old 32-bit Gadget route should be considered a dead end unless there is a specific reason to revisit it.
```

### 3. Modified/Re-signed Builds Are Detected

Patched/re-signed merged builds and modified 32-bit split builds installed after verifier changes, but they restart or exit quickly.

Notable artifacts:

```text
E:\HD\HaydayMod\frida_inject\hayday_full_patched_unsigned.apk
E:\HD\HaydayMod\frida_inject\hayday_full_patched_storearsc_unsigned.apk
E:\HD\HaydayMod\frida_inject\hayday_full_patched_storearsc_armeabi_v7a_unsigned.apk
```

Findings:

```text
Merged full APK with assets installs only as splits=[base].
Storing resources.arsc uncompressed fixed the resources install warning.
Verifier settings had to be disabled for patched APK install.
The app still restart-looped after install.
No useful AES hooks were reached.
```

Conclusion:

```text
Re-signing/tampering changes the failure mode from missing assets or Java crash into native-side restart/exit validation.
Clean original signature is currently more useful than patched APKs.
```

### 4. Frida Server Is Detected Before Startup

Running a Frida server before launching clean ARM64 Hay Day triggers the guard.

Observed with normal server:

```text
/data/local/tmp/frida-server -l 0.0.0.0:27042
```

Observed with renamed helper:

```text
/data/local/tmp/riru_helper -l 0.0.0.0:28080
```

Result:

```text
Hay Day crashes with bjnr.d:16 during startup.
The guard is not only looking for the literal frida-server process name or port 27042.
```

Conclusion:

```text
Never start Frida server before launching Hay Day on this build.
```

### 5. App-first, Server-after-start Still Trips Detection

Test sequence:

```text
1. Kill Frida/riru helper.
2. Force-stop Hay Day.
3. Launch clean ARM64 Hay Day.
4. Wait until it is alive.
5. Start renamed server on port 28080.
6. Run frida-ps.
7. Attempt attach.
```

Observed:

```text
frida-ps -H 127.0.0.1:28080 listed: 13859 Hay Day
frida -p 13859 failed: Failed to attach: unable to find process with pid 13859
Shortly after server startup, Hay Day crashed with bjnr.d:16.
Logcat showed dex2oat activity for /data/local/tmp/frida-helper-*.dex before the guard report.
```

Conclusion:

```text
Post-start Frida server gives a short visibility window but still triggers detection before a useful attach.
The next server test, if attempted, should attach by name immediately: frida -H 127.0.0.1:28080 -n "Hay Day" -l attach_probe.js
Risk: this may still trip the same guard.
```

### 6. Embedded ARM Gadget Loads But Cannot Use Java

ARM Gadget path:

```text
E:\HD\HaydayMod\frida_inject\tools\frida-gadget-arm.so
E:\HD\HaydayMod\frida_inject\hayday_gadget_autoload.js
```

Observed:

```text
ARM Gadget loads under Houdini/native-bridge.
Process architecture from Gadget is ARM-side/native-bridge.
Java.available remains false.
ARM libc hooks for exit/abort/syscall/kill/dlopen did not catch the clean exit path.
```

Conclusion:

```text
ARM Gadget is useful for native-side observation but not Java AES hooks in this emulator setup.
```

### 7. Embedded x86 Gadget Loads But Java Is Undefined

x86 Gadget path:

```text
System.load("/data/local/tmp/libfrida-gadget-x86.so")
E:\HD\HaydayMod\frida_inject\tools\frida-gadget-x86.so
E:\HD\HaydayMod\frida_inject\hayday_java_gadget_autoload.js
```

Observed:

```text
x86 Gadget loaded and wrote /data/data/com.supercell.hayday/frida_java_autoload.log.
Process arch reported ia32.
Process maps contained /system/lib/libart.so.
The Frida Java global was still undefined.
```

Conclusion:

```text
x86 Gadget script mode also cannot reach Frida's Java bridge inside this native-bridge app layout.
This blocks Java-level SecretKeySpec/Cipher hooks through embedded Gadget.
```

### 8. bjnr Guard Location

The recurring Java crash path is:

```text
bjnr.C native load / guard path
bjnr.ae guard report / uncaught exception path
bjnr.d: 16 crash payload
```

Patch tested in decoded smali:

```smali
.method public static a(Ljava/lang/String;)V
    .locals 0

    return-void
.end method
```

Location used in patched tree:

```text
E:\HD\HaydayMod\frida_inject\base_gadget_smali\smali_classes2\bjnr\ae.1.smali
```

Other experimental patches:

```text
bjnr.C.c() loads supercell_hayday directly once.
bjnr.C.b(Q) calls native C.c(Q).
private bjnr.C.d(Q) no-op.
CoreComponentFactory.smali attempts System.load("/data/local/tmp/libfrida-gadget-x86.so") and System.loadLibrary("frida-gadget").
```

Conclusion:

```text
These patches changed failure modes but did not produce a stable screen or Java hook access.
The app also appears to have native/server/tamper checks beyond the bjnr.ae Java crash dispatcher.
```

## Current Best Baseline

```text
Clean original ARM64 split install.
No Frida server running before launch.
Hay Day force-stopped when idle.
```

Verify baseline:

```powershell
$adb='C:\Program Files\Microvirt\MEmu\adb.exe'
& $adb devices
& $adb shell 'ps -A | grep -i hayday || true'
& $adb shell dumpsys package com.supercell.hayday | Select-String -Pattern 'versionName|versionCode|primaryCpuAbi|signatures|splitNames'
```

Launch and observe:

```powershell
$adb='C:\Program Files\Microvirt\MEmu\adb.exe'
& $adb shell 'pkill -f frida-server 2>/dev/null; pkill -f riru_helper 2>/dev/null'
& $adb shell am force-stop com.supercell.hayday
& $adb shell logcat -c
& $adb shell am start -n com.supercell.hayday/.GameApp
Start-Sleep -Seconds 18
& $adb shell ps -A | Select-String -Pattern 'hayday|supercell'
& $adb shell logcat -d -v threadtime | Select-String -Pattern 'hayday|upercell|ActivityManager|AndroidRuntime|FATAL|Report|bjnr|Displayed|Flutter|Titan|SignIn|GamesService' | Select-Object -Last 250
```

## Recommended Next Steps

1. Pull app data from the clean ARM64 run and inspect whether `storage_new.xml` or related files now exist:

```powershell
$adb='C:\Program Files\Microvirt\MEmu\adb.exe'
& $adb shell 'run-as com.supercell.hayday ls 2>/dev/null || true'
& $adb shell 'su -c "find /data/data/com.supercell.hayday -maxdepth 4 -type f | head -n 300"'
& $adb shell 'su -c "ls -la /data/data/com.supercell.hayday/shared_prefs"'
```

2. Try immediate app-first attach by name, not PID, only after app is already running:

```powershell
$adb='C:\Program Files\Microvirt\MEmu\adb.exe'
$job = Start-Job -ScriptBlock { & 'C:\Program Files\Microvirt\MEmu\adb.exe' shell '/data/local/tmp/riru_helper -l 0.0.0.0:28080' }
& $adb forward tcp:28080 tcp:28080
frida -H 127.0.0.1:28080 -n "Hay Day" -l E:\HD\HaydayMod\frida_inject\attach_probe.js
Stop-Job $job; Remove-Job $job
```

3. If server attach remains detected, switch to non-server native analysis:

```text
Read /proc/<pid>/maps for native libraries.
Dump readable process memory regions from root if kernel allows it.
Search memory dump for AES constants, Base64 strings, key-length candidates, and known encrypted key context.
Consider gdbserver/process_vm_readv if /proc/<pid>/mem is blocked.
```

4. Consider post-install modification only as a controlled experiment:

```text
Install clean signed ARM64 split set.
Modify app files under /data/app after PackageManager accepts the original signatures.
Force-stop and clear dalvik/oat caches if needed.
Risk: Hay Day may self-hash APK/splits and detect this too.
```

## Do Not Repeat

```text
Do not direct-install split_install_time_asset_pack.apk. It is not standalone and gives a parse error.
Do not focus on the 32-bit split path on MEmu; clean 32-bit already fails with bjnr.d:16.
Do not launch Hay Day with Frida server already running.
Do not assume the merged patched APK path is closer to AES; clean ARM64 is currently more stable.
Do not rely on embedded Gadget for Java hooks in this native-bridge emulator; Java is unavailable/undefined there.
```

## Short Status

```text
Achieved: clean ARM64 Hay Day runs on MEmu with original signature and full asset split set.
Achieved: identified 32-bit/Houdini startup failure and server/Gadget limitations.
Achieved: confirmed Frida server is detected both before startup and shortly after post-start launch.
Blocked: Java AES hooks are not yet reachable.
Best path: keep clean ARM64 baseline, inspect app data, then try faster app-first attach or non-Frida memory/native analysis.
```

## 2026-05-06 Later Findings: Storage Pull And Native Guard Offsets

### Live storage sample pulled

After launching the clean ARM64 baseline with no Frida server, Hay Day created real storage files:

```text
E:\HD\HaydayMod\pulled_data\2026-05-06_clean_arm64\storage_new.xml
E:\HD\HaydayMod\pulled_data\2026-05-06_clean_arm64\storage.xml
E:\HD\HaydayMod\pulled_data\2026-05-06_clean_arm64\com.supercell.id.scid_plugin.ScidPlugin.xml
E:\HD\HaydayMod\pulled_data\2026-05-06_clean_arm64\INSTALLATION
```

Current `storage_new.xml` contains the guide's drop-group encrypted key:

```text
kdsBDLvTJkt5ScQihjAVGA== -> ntmNPvN5kt8QTw4Y6JQ+Ag==
```

This means the guide's statistical-bypass path works on the current emulator data even without the AES key.

### ARM64 Frida server test

Downloaded and pushed:

```text
E:\HD\HaydayMod\frida_inject\tools\frida-server-17.9.3-android-arm64.xz
E:\HD\HaydayMod\frida_inject\tools\frida-server-arm64
/data/local/tmp/frida-server-arm64
```

Result:

```text
/data/local/tmp/frida-server-arm64 --version -> 17.9.3
frida -H 127.0.0.1:29090 -p <hayday_pid> -> Failed to attach: connection closed
logcat: frida-server-arm64 SIGSEGV
Hay Day stayed alive during that server crash.
```

Conclusion:

```text
ARM64 server can execute under the native bridge but cannot attach reliably. Server-based Frida remains non-viable.
```

### Post-install Gadget route

Post-install patching can load ARM64 Gadget without re-running PackageManager:

```text
E:\HD\HaydayMod\frida_inject\base_postinstall_arm64_absolute_gadget.apk
/data/local/tmp/libfrida-gadget-arm64.so
/data/local/tmp/libfrida-gadget-arm64.config.so
/data/local/tmp/hayday_arm64_gadget_autoload.js
```

Important result:

```text
ARM64 Gadget loads in script mode and writes /data/data/com.supercell.hayday/frida_arm64_autoload.log.
Process.arch = arm64.
Java remains undefined.
```

So embedded ARM64 Gadget is useful for native tracing/patching, but still not Java AES hooks.

### Native guard offsets discovered

With ARM64 Gadget hooks installed, the app calls native exit paths before useful Java hooks exist.

Initial guard exit:

```text
syscall exit_group nr=94 code=1
Backtrace top: 0xbe3ac04
Mapped library: /data/app/.../lib/arm64/libsupercell_hayday.so
Stable native-bridge base: 0xb854000
Offset: 0x5e6c04
```

Live memory page was dumped successfully from the running process:

```text
E:\HD\HaydayMod\pulled_data\guard_page_0be3a000.bin
```

The live page is decrypted/translated enough to disassemble, unlike the static file bytes. The relevant live code around `0xbe3ac04` showed a call/tail-call into an exit wrapper:

```text
libsupercell_hayday.so + 0x5e6658  guard exit wrapper start
libsupercell_hayday.so + 0x5e6c00  call site before 0x5e6c04
libsupercell_hayday.so + 0x5e6c60  tail branch site
```

Live patch attempted by `hayday_arm64_gadget_autoload.js`:

```text
+0x5e6658 -> RET
+0x5e6c00 -> NOP
+0x5e6c60 -> RET
```

This changed the failure mode. The old `0x5e6c04` exit stopped, then the next native exit came from:

```text
Backtrace top: 0xbaf44d4
Offset: 0x2a04d4
Likely call instruction: 0xbaf44d0 / offset 0x2a04d0
```

Additional live patch attempted:

```text
+0x2a04d0 -> NOP
```

Result after all ARM64 patches:

```text
The app survived longer and reached more Java/FA/classloader initialization.
It still exited cleanly with code 1 after roughly 12 seconds.
No Java Frida bridge became available.
```

### x86 Gadget follow-up

A dual loader was built to try loading both x86 and ARM64 Gadget:

```text
E:\HD\HaydayMod\frida_inject\base_postinstall_arm64_dual_gadget.apk
E:\HD\HaydayMod\frida_inject\hayday_x86_gadget_autoload.js
```

Expected purpose:

```text
x86 Gadget: hook x86/ART-side exit/abort paths.
ARM64 Gadget: patch libsupercell_hayday.so native guard offsets.
```

Result:

```text
ARM64 Gadget still loaded and patched.
No /data/data/com.supercell.hayday/frida_x86_autoload.log was produced.
The x86 Gadget did not appear to load in this dual setup.
The app still exited cleanly.
```

### Current restored state

After these tests, the installed app was restored from backups:

```text
E:\HD\HaydayMod\installed_backup\2026-05-06_original_arm64\base.apk
E:\HD\HaydayMod\installed_backup\2026-05-06_original_arm64\split_config.arm64_v8a.apk
```

Restored on device:

```text
/data/app/com.supercell.hayday-AlGgz9ix3qihOBzzZV2rxw==/base.apk -> 68,947,197 bytes
/data/app/com.supercell.hayday-AlGgz9ix3qihOBzzZV2rxw==/split_config.arm64_v8a.apk -> 60,855,794 bytes
```

Verification after restore:

```text
primaryCpuAbi=arm64-v8a
versionName=1.70.96
versionCode=2169
signature=[2f310a46]
Hay Day launches cleanly.
ActivityManager: Displayed com.supercell.hayday/.GameApp
AndroidFlutterBridgeImpl handlers are set.
```

The app was force-stopped after verification to leave the emulator stable.

### Updated best next steps

```text
Use the clean ARM64 baseline for account/storage work.
Use storage_new.xml extraction for the drop-group value now; AES key extraction remains blocked.
If continuing instrumentation, focus on a loader that can also hook x86/ART-side clean exit, or use non-Frida memory tooling against the clean app.
For native guard work, start from live offsets +0x5e6658, +0x5e6c00, +0x5e6c60, and +0x2a04d0.
Do not leave post-install patched base.apk on-device; restore clean base after tests.
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
