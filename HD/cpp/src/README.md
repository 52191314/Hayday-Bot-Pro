# HD Bot Source Layout

This folder is the incremental module layout for the Hay Day bot rewrite.

Initial rule: only move low-risk infrastructure first. Farming and market logic stay in the legacy files until the module boundary is stable.

Planned areas:

- `app`: GUI and application shell code.
- `bot`: bot orchestration and instance lifecycle.
- `creator`: account/emulator creation workflows.
- `domain`: shared data models and state.
- `game`: Hay Day screen concepts and game-specific constants.
- `infra`: platform services such as paths, ADB, logging, config, capture, process execution, and touch input.
- `operations/farming`: harvest, plant, scan, rotation, and cycle logic.
- `operations/market`: roadside shop, sale flow, adverts, offer UI, cache, and market state.

Extracted so far:

- `infra/Paths`: AppData path and backup folder creation.
- `infra/ProcessRunner`: hidden Windows process execution.
- `infra/Adb`: ADB command execution, ADB output capture, and tap helper.
- `infra/TouchInput`: input-event auto detection.
- `infra/MinitouchClient`: minitouch socket connection, protocol primitives, and high-level gesture helpers (drag, pinch, V-sweep). Replaces 3 duplicated inline implementations.
- `infra/ActionDebugLog`: CSV action-level debug log writer.
- `infra/RuntimeRelease`: runtime string formatting for UI/RPC surfaces.
- `infra/Config`: path validation. Full save/load remains legacy until UI state is untangled.
- `infra/NativeCapture`: ADB screenshot capture with direct stdout and file fallback.
- `bot/FlowLogging`: farm/sale state transition tracking; action timing CSV output is opt-in for diagnostics.
- `bot/ScreenRecovery`: cross detection, panel dismissal, silo-full popup cleanup, and zoom recovery.
- `bot/DataSync`: manual HUD, barn, and silo sync routines.
- `bot/TemplateTests`: manual template/test-mode execution.
- `bot/AccountFiles`: account backup/restore, injection, zoom hack verification, and rotation reporting.
- `bot/EmulatorControl`: minitouch startup, watchdog, revive heartbeat, MEmu config, and emulator factory.
- `bot/PremiumLoop`: premium bot account loop and per-account farm cycle orchestration.
- `operations/storage`: storage-transfer radar, friend request, and storage master routines.
