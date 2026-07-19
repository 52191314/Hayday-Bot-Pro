# Runtime Logging and Performance

This note records the low-resource defaults used by the bot after the 2026-05-18 cleanup.

## Logging Defaults

- UI logs keep a maximum of 500 entries in memory.
- The log level filter is now an ingest filter. With the default INFO level, DEBUG rows are not stored.
- Action timing CSV output is opt-in. Enable it with the UI `Timing CSV` checkbox or `ActionTimingLog=1` in `%APPDATA%\NXRTH_Premium\nxrth_config.ini`.
- `debug_action_log.csv` is still controlled by `g_EnableDebugActionLog`. When enabled, high-volume image scan categories are throttled to at most one row per category/action every 250 ms.
- `debug_action_log.csv` stops writing once it reaches 4 MB so a long diagnostic run cannot fill the disk or keep the laptop busy with unbounded append I/O.

## Vision Defaults

- `g_EnableVisionGpuAssist` defaults to `false`.
- OpenCV worker threads are capped to 1 during vision setup.
- The UI `Vision GPU Assist` checkbox persists `VisionGpuAssist=1`, but low-end laptops should leave it off unless GPU matching is proven faster on that machine.

## Crop Marker Runtime Contract

- Empty fields are magenta and should match OpenCV HSV `H=150`.
- Growing wheat is orange/yellow and should match OpenCV HSV `H=25`.
- Grown crops are cyan and should match OpenCV HSV `H=90`.
- The active injector rebuild is `installed_backup/fresh_sctx_build_20260518_165805`.
- The rebuild uses direct export triangle masks for crop markers to avoid atlas-wide color floods from recursive child masks.

## When Debugging A Failure

1. Set the log filter to DEBUG only for a short repro.
2. Enable `Timing CSV` only while measuring slow operations.
3. Use the HSV CSV capture manually; leave `Auto CSV 50x on screenshot` off during normal runs.
4. Inspect `HD/x64/Release/injecthacks/extra_visuals/previews/crop_state_colors.png` after a rebuild before injecting visuals.
