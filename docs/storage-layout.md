# HaydayMod Storage Layout

Last updated: 2026-06-08

This document records the current top-level organization for `D:\02_Projects\HD\HaydayMod` after the June 2026 cleanup. The goal is to keep the main folder usable without deleting historical reverse-engineering, Frida, APK, and emulator artifacts.

## Root Folders

- `_indexes`: generated inventory files. These may contain old absolute paths from earlier storage locations and should be treated as historical references.
- `Account_backup`: active account-backup location referenced by `HD\cpp\src\bot\AccountFiles.cpp`; keep this at the root unless the code is updated.
- `analysis_artifacts`: historical generated outputs, extracted payloads, memory scans, probe files, and scratch comparisons.
- `docs`: maintained notes and architecture references.
- `HD`: active project source.
- `lsposed_hayday_promon_aes`: LSPosed source project; keep at the root for now.
- `lsposed_hayday_promon_aes_signed`: signed APK output folder; keep next to the LSPosed source for install/debug convenience.
- `magisk_memu_boot`: emulator/rooting boot images and scripts; keep at the root unless the workflow is retired.
- `Reference_Assets`: visual/source reference assets such as PNG and FLA files.
- `source_packages`: original upstream package inputs, such as APKM downloads.
- `third_party`: dependency/vendor material.
- `tools`: maintained helper scripts and tool inputs.
- `venv_frida_16_5_6`: local Frida Python environment; keep at the root unless scripts and activation notes are updated.

`Frida.md` and `SESSION_HISTORY.md` intentionally remain at the root. They are the canonical worklog/handoff files for this HaydayMod investigation.

## Analysis Artifacts

`analysis_artifacts` contains data that is useful as evidence or prior research, but should not clutter the active root:

- `account_exports`: captured account/storage XML exports.
- `apk_outputs`: loose generated APK outputs.
- `apkm_arm64_extract`: extracted ARM64 package files.
- `blutter_out_2026-05-06`: Blutter output and related generated files.
- `memscan_experiments`: all historical `memscan_*` experiment folders.
- `native_promon_20260506`: native library/string extraction output.
- `pulled_data`: pulled device/app data and scan captures.
- `runtime_payload_20260506_1828`: runtime payload APK, dex, apktool, and baksmali output.
- `scratch_work`: temporary comparison/probe folders, including empty historical probes.

## June 2026 Move Log

These moves were organizational only; no files were intentionally deleted.

- Moved all root-level `memscan_*` folders into `analysis_artifacts\memscan_experiments`.
- Moved `apkm_arm64_extract`, `blutter_out_2026-05-06`, `native_promon_20260506`, `pulled_data`, and `runtime_payload_20260506_1828` into `analysis_artifacts`.
- Moved `codex_tmp_compare_trees`, `tmp_keyparam_follow`, and `tmp_sizecheck_probe` into `analysis_artifacts\scratch_work`.
- Moved `account_2_storage_new.xml` into `analysis_artifacts\account_exports`.
- Moved `lsposed_hayday_promon_aes_unsigned.apk` into `analysis_artifacts\apk_outputs`.
- Moved `probe_pull.bin` into `analysis_artifacts\loose_probe_outputs`.
- Moved `animals.fla` and `buildings_new.fla` into `Reference_Assets`.
- Moved the APKMirror `.apkm` package into `source_packages`.

## July 2026 Move Log

These moves were organizational and corrective only; no files were deleted.

- Moved root-level build and login log files (`build_log.txt`, `login_output.txt`) into `analysis_artifacts\scratch_work`.
- Moved decrypted `live_storage_new.xml` into `analysis_artifacts\account_exports`.
- Moved packet captures (`packet_10100.bin`, `packet_20100.bin`) into `analysis_artifacts\loose_probe_outputs`.
- Fixed root-level batch scripts (`run_headless.bat`, `run_proxy.bat`) to use relative `%~dp0` project path references rather than absolute ones.

## Before Moving More

Check path references before moving any of these root-level folders:

- `Account_backup`
- `HD`
- `tools`
- `third_party`
- `venv_frida_16_5_6`
- `lsposed_hayday_promon_aes`
- `lsposed_hayday_promon_aes_signed`
- `magisk_memu_boot`

Generated `_indexes` files and old session notes may still mention previous `E:\HD\HaydayMod` paths. Do not treat those old paths as authoritative for the current D-drive layout.
