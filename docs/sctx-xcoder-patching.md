# XCoder / SCTX Visual Patching

This guide describes how the bot rebuilds Hay Day visual payloads from the installed APK assets, patches selected SCTX atlases, and emits `.YaJing` injector files.

## Tooling

Main script:

```text
tools/rebuild_hayday_sctx_payloads.py
```

Default inputs:

```text
APK splits: D:/HD/HaydayMod/installed_backup/memu_splits_20260514_current
XCoder:     D:/XCoder-master
Output:     D:/HD/HaydayMod/installed_backup/fresh_sctx_build_<timestamp>
Release:    D:/HD/HaydayMod/HD/x64/Release/injecthacks
```

Use the bundled Codex Python when the old XCoder venv is broken:

```powershell
C:\Users\52191314\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe tools\rebuild_hayday_sctx_payloads.py
```

## Pipeline

1. Extract `assets/sc` from the installed APK split files.
2. Decode required SCTX files with XCoder/mb-sc-tools.
3. Parse `.sc` metadata with `RenderParser`.
4. Build texture masks for selected exports.
5. Patch decoded PNG atlases.
6. Re-encode the workspace to valid SCTX.
7. Encrypt each payload as `.YaJing`.
8. Replace `HD/x64/Release/injecthacks`, after backing up the old folder.
9. Write a manifest and raw device-push files under the build folder.

## Current Patch Spec

### Trees and Nature

In `nature_new.sc`, normal tree and tree-kind nature assets are transparent.

Current tree asset list includes fir and leafy variants such as:

- `fir_big_01`
- `fir_medium_01`
- `fir_small_01`
- `leafy_big_01`
- `leafy_big_02`
- `leafy_small_01`

### Crop Fields

Crop field colors are state-only markers for HSV detection:

- Empty: magenta, OpenCV HSV around `H=150`
- Growing: orange/yellow, OpenCV HSV around `H=25`
- Grown: cyan, OpenCV HSV around `H=90`

As of the 2026-05-18 rebuild, crop-state markers use direct export triangle masks. Do not use recursive movieclip child masks for crop states; they can select shared atlas geometry and flood unrelated plantation/crop art.

Empty exports:

- `field_empty`
- `field_placeholder`
- `map_field_empty`

Growing exports:

- `map_crop_wheatgrowing`
- This marker must stay visible so planted wheat is not treated as invisible/unknown during the next farm scan.

Grown exports:

- `crop_*`
- `map_crop_*`
- Excludes names containing `growing`

Latest active rebuild:

```text
installed_backup/fresh_sctx_build_20260518_165805
```

That rebuild matched 3 empty marker exports, 1 growing marker export, and 51 grown marker exports. The active `HD/x64/Release/injecthacks` folder was replaced from that build.

Unmasked pixels keep the original Hay Day colors.

### Cows

Cow visuals are invisible. The patch is cow-only and should not blindly delete all animal atlases.

The script parses animal `.sc` files and selects cow visual exports such as:

- `cow01_idle01`
- `cow01_hungry01`
- `cow01_ready01`
- `cow01_harvest01`
- `cow01_walk01`
- seasonal cow variants

### Buildings and Landmarks

`buildings_new.sc` patches key bot landmarks:

- Feed mill / sauce mixer mask is black with `FM`.
- Cow pasture mask is black with `CP`.
- Dairy mask is black with `DY`.
- Cow fence is black.
- Cow trough is transparent.

Tags are white text with a dark stroke for template readability.

## Generated Payloads

Base nature payloads:

```text
inject.YaJing   -> nature_new.sc
inject2.YaJing  -> nature_new_0.sctx
inject3.YaJing  -> nature_new_1.sctx
inject4.YaJing  -> nature_new_2.sctx
inject5.YaJing  -> nature_new_3.sctx, optional
```

Extra payloads follow:

```text
inject_extra_<stem>_<texture>.YaJing
```

Examples:

```text
inject_extra_trees_0.YaJing
inject_extra_animals_0.YaJing
inject_extra_buildings_new_0.YaJing
```

## Device Push Verification

The rebuild script writes raw patched files to:

```text
installed_backup/fresh_sctx_build_<timestamp>/device_push
```

When MEmu is connected:

```powershell
adb -s 127.0.0.1:21503 shell su -c "mkdir -p /data/data/com.supercell.hayday/update/sc"
adb -s 127.0.0.1:21503 push installed_backup\fresh_sctx_build_<timestamp>\device_push /data/local/tmp/hayday_sc_patch
adb -s 127.0.0.1:21503 shell su -c "cp /data/local/tmp/hayday_sc_patch/* /data/data/com.supercell.hayday/update/sc/"
adb -s 127.0.0.1:21503 shell su -c "chmod 777 /data/data/com.supercell.hayday/update/sc/*"
```

Then compare local hashes with remote hashes. If `adb devices` shows no connected device, push/hash verification is blocked until MEmu is running and connected.

## Common Failures

- `nature_new_3.sctx` absent: OK. It should be skipped and `inject5.YaJing` should remain optional.
- Pillow import error from `D:/XCoder-master/venv`: use the bundled Python shown above.
- Decrypt failure in the injector: rebuild payloads and make sure the C++ asset-key seed matches the Python seed.
- Crops not detected: inspect the patched `nature_new_*` atlas previews and verify magenta/orange/cyan markers survived SCTX re-encode without large atlas floods.
