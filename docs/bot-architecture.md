# Bot Architecture

This project is a Windows bot for Hay Day running inside an Android emulator, primarily MEmu. The desktop app drives the emulator through ADB, OpenCV screen detection, local account backup files, injected Hay Day visual assets, and minitouch input.

## Runtime Layout

- `HD/premium gui.cpp` owns most UI state, config import/export, account slot health display, account swapping, and button entry points.
- `HD/cpp/src/bot/AccountFiles.cpp` handles account save/load, visual injection, zoom/font files, and account-slot migration.
- `HD/bot_logic.cpp` contains core image matching and HSV color detectors shared by farming and UI flows.
- `HD/cpp/src/operations/farming/Farming.cpp` contains farming-specific crop runtime metadata, grown/empty/growing field helpers, and dense field gestures.
- `HD/cpp/src/infra` contains lower-level helpers such as ADB, paths, runtime release checks, and `.Ahjie` crypto.
- `tools/rebuild_hayday_sctx_payloads.py` rebuilds visual injector payloads from installed Hay Day APK split assets.

## Emulator Flow

The bot assumes each configured instance has an ADB serial. For MEmu, the default serial pattern is `127.0.0.1:21503`, then `+10` per instance. The UI stores ADB and MEmu paths in `%APPDATA%\NXRTH_Premium\nxrth_config.ini`.

Typical account rotation:

1. Select an account slot.
2. Decrypt `account_N.Ahjie` locally.
3. Force-stop Hay Day.
4. Push decrypted XML to `/data/data/com.supercell.hayday/shared_prefs/storage_new.xml`.
5. Relaunch Hay Day.
6. Use templates and HSV detectors to farm, sell, or transfer.

## Account Files

Account slots now use:

```text
%APPDATA%\NXRTH_Premium\Backups\Instance_<id>\account_<slot>.Ahjie
```

Legacy `account_<slot>.nxrth` files are read only for migration. When `LoadAccountFromSlot` sees no `.Ahjie` file but finds a legacy `.nxrth`, it decrypts the legacy file, validates the XML, writes a `.Ahjie` replacement, and keeps the old file in place.

## Visual Injection

Visual payloads live beside the release executable:

```text
HD/x64/Release/injecthacks/
```

Required base payloads:

- `inject.Ahjie` -> `nature_new.sc`
- `inject2.Ahjie` -> `nature_new_0.sctx`
- `inject3.Ahjie` -> `nature_new_1.sctx`
- `inject4.Ahjie` -> `nature_new_2.sctx`

Optional base payload:

- `inject5.Ahjie` -> `nature_new_3.sctx`

`nature_new_3.sctx` is optional because the installed APK may not ship it. The injector must not abort when it is absent.

Extra visual payloads use names like:

```text
inject_extra_trees_0.Ahjie
inject_extra_animals_0.Ahjie
inject_extra_buildings_new_0.Ahjie
```

Only files present in `injecthacks` are applied. Missing extra payloads are skipped.

## Crop Detection Model

Crop mode still decides seed/product templates and grow timers. Field color no longer identifies the crop type. Field color identifies state:

- Empty field: magenta, OpenCV HSV around `H=150`
- Growing crop: orange/yellow, OpenCV HSV around `H=25`
- Grown crop: cyan, OpenCV HSV around `H=90`

The shared HSV table is exposed by `GetCropStateHsvRange`.

The visual rebuild must keep those markers visible. Crop-state payloads use direct SCTX export triangle masks; recursive child masks are intentionally avoided because they can color unrelated atlas geometry and break planting.

## Low-Resource Runtime Defaults

The bot now defaults to low-resource diagnostics:

- Action timing CSV logging is disabled unless `ActionTimingLog=1` or the UI `Timing CSV` checkbox is enabled.
- DEBUG-level UI logs are not stored unless the log level filter is set to DEBUG.
- `debug_action_log.csv` remains opt-in, throttles high-volume image-scan rows, and stops writing once the file reaches 4 MB.
- OpenCV GPU assist is off by default, and OpenCV worker threads are capped to 1 to avoid freezing low-end laptops.

## Operational Notes

- Keep old `.nxrth` account files as backups until the matching `.Ahjie` loads successfully.
- Rebuild visual payloads after every Hay Day APK asset change.
- The asset rebuild script should be run with a Python that has Pillow and the XCoder dependencies available.
- If MEmu is not connected in `adb devices`, asset push/hash verification cannot run.
