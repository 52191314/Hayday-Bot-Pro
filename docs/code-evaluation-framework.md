# Project Wheat — Code Evaluation Framework (v2)

Last updated: 2026-07-19  
Scope: C++ bot product code under `HD/` (Project Wheat)  
Related maps: [code-maps/BOT_MAP.md](code-maps/BOT_MAP.md), [bot-architecture.md](bot-architecture.md), [runtime-logging-and-performance.md](runtime-logging-and-performance.md), [HD/cpp/src/README.md](../HD/cpp/src/README.md)

This is an evaluation **rubric**, not a highlight reel. Use it to score reliability, maintainability, and multi-instance safety of the farming bot.

---

## 1. Purpose

| Goal | Non-goal |
|------|----------|
| Baseline quality audit of the live bot path (start → load account → inject → farm/sell → save → rotate) | Full reverse-engineering of Hay Day network/crypto unless a bot path depends on it |
| Rank risks by production impact (farms stuck, wrong sales, account corruption) | Style-only nits without runtime effect |
| Produce a scored findings list + ordered fix backlog | Rewriting the whole codebase in one pass |

**Primary question:** *Can N instances run account rotation for hours without silent wrong actions or unrecoverable stalls?*

---

## 2. Architecture Assumptions (score against these)

1. **Resolution contract** — Emulator is **640×480 @ 100 DPI**. Coordinates, templates, and many ROIs assume this. See `BOT_MAP.md`.
2. **Vision is injection-first** — Runtime prefers injected SCTX markers + **HSV state detection** over heavy template matching. Crop states: empty magenta `H≈150`, growing orange `H≈25`, grown cyan `H≈90`.
3. **Templates/OCR are secondary** — Templates for UI chrome/menus; OCR for silo/barn/price digits, not primary crop identity.
4. **Low-resource defaults** — Vision GPU assist off by default; OpenCV threads capped; timing CSV and DEBUG ingest are opt-in (`runtime-logging-and-performance.md`).
5. **Module layout is transitional** — `HD/cpp/src/` is the incremental extraction; legacy still lives in `bot_logic.cpp`, `BotEngine.*`, `premium gui.cpp`. Evaluate both layers.

---

## 3. Evaluation Surfaces (mapped to tree)

Score each surface independently. Primary files first; related second.

### S1 — Orchestration & account rotation

| Role | Path |
|------|------|
| Premium loop | `HD/cpp/src/bot/PremiumLoop.cpp` |
| Per-instance cycle | `HD/cpp/src/bot/Cycles.cpp` |
| Account load/save / inject entry | `HD/cpp/src/bot/AccountFiles.cpp` |
| Instance / slot model | `HD/bot_logic.h` (`BotInstance`, `AccountSlot`) |
| Cross-instance transfer | `HD/BotEngine.*` (`TransferRequest`) |
| Shell / entry | `HD/premium gui.cpp` |

**Look for:** slot rotation order, force-stop → push XML → relaunch, failure leaving half-loaded state, metrics save before rotate.

### S2 — Visual injection & HSV contract

| Role | Path |
|------|------|
| Inject payloads | `AccountFiles.cpp` → `InjectImportantFiles` |
| HSV ranges / crop helpers | `HD/bot_logic.cpp` (`GetCropStateHsvRange`, field scanners) |
| Payload rebuild | `tools/rebuild_hayday_sctx_payloads.py` (outside `HD/`, but bot-critical) |
| Marker contract docs | `docs/sctx-xcoder-patching.md`, `docs/bot-architecture.md` |

**Look for:** missing optional `nature_new_3.sctx` not aborting; inject after account load; markers still visible; no recursive mask floods.

### S3 — Template matching & capture (secondary vision)

| Role | Path |
|------|------|
| Match / cache / OpenCL path | `HD/bot_logic.cpp` (`LoadTemplateCached`, `RunTemplateMatchAdaptive`, `FindImage`) |
| Capture | `HD/cpp/src/infra/NativeCapture.cpp` |
| Manual template tests | `HD/cpp/src/bot/TemplateTests.cpp` |

**Look for:** cache size/LRU + mutex contention; GPU assist gated by config; safe CPU fallback; ROI correctness vs full-frame cost.

### S4 — OCR

| Role | Path |
|------|------|
| Shared OCR | `HD/tesseract_ocr.cpp` (`thread_local TessBaseAPI`) |
| Call sites | `Farming.cpp`, market `Sell.cpp` / `State.cpp`, `Storage.cpp`, `DataSync.cpp` |

**Look for:** every path using shared OCR (no ad-hoc `new TessBaseAPI` without lifecycle); preprocessing only where needed; false-positive handling on digit reads; tessdata path resolution.

### S5 — Farming operations

| Role | Path |
|------|------|
| Plant / harvest / silo | `HD/cpp/src/operations/farming/Farming.cpp` |
| Animals / dairy | `HD/cpp/src/operations/farming/Animals.cpp` |

**Look for:** state order (silo full → harvest → plant); sickle/seed gestures; dense-field swipes vs 640×480 anchors; recovery hooks when UI is wrong.

### S6 — Market & storage operations

| Role | Path |
|------|------|
| RSS sell | `HD/cpp/src/operations/market/Sell.cpp` |
| Market state | `HD/cpp/src/operations/market/State.cpp` |
| Storage transfer / master | `HD/cpp/src/operations/storage/Storage.cpp` |
| Transfer wiring | `HD/BotEngine.*` |

**Look for:** empty-crate detection, price/OCR, advertise flag, timeout/retry, seller/buyer race, debug screenshots on failure.

### S7 — Emulator I/O & input

| Role | Path |
|------|------|
| Emulator lifecycle | `HD/cpp/src/bot/EmulatorControl.cpp` |
| Game launch | `HD/cpp/src/app/LaunchGame.cpp` |
| ADB | `HD/cpp/src/infra/Adb.cpp`, `PersistentAdb.cpp` |
| Touch | `TouchInput.cpp`, `MinitouchClient.cpp` |
| Process spawn | `ProcessRunner.cpp` |

**Look for:** serial pattern (`21503 + 10*n`), minitouch revive, crash watchdog, hung ADB, resolution/DPI enforcement at factory time.

### S8 — Recovery & runtime health

| Role | Path |
|------|------|
| Popups / NR dialog / zoom | `HD/cpp/src/bot/ScreenRecovery.cpp` |
| Health snapshots | `HD/cpp/src/bot/RuntimeHealth.cpp` |
| Flow state logging | `HD/cpp/src/bot/FlowLogging.cpp` |
| Action CSV | `HD/cpp/src/infra/ActionDebugLog.cpp` |

**Look for:** bounded retry loops, no infinite dismiss, token/cancel awareness (`TestControl`), log volume under multi-instance.

### S9 — Crypto, paths, config

| Role | Path |
|------|------|
| `.YaJing` crypto | `HD/cpp/src/infra/YaJingCrypto.cpp` |
| Paths / backups | `Paths.cpp`, `AccountFiles.cpp` |
| Config | `Config.cpp` + legacy in `premium gui.cpp` / `serialization.cpp` |
| Docs | `docs/yajing-encryption.md`, `docs/storage-encryption.md` |

**Look for:** migrate-from-`.nxrth` safety, validate XML before write, no plaintext secrets in logs, AppData layout consistency.

### S10 — Decomposition health

| Role | Path |
|------|------|
| New modules | `HD/cpp/src/**` |
| Legacy | `bot_logic.cpp`, `BotEngine.*`, `premium gui.cpp`, `Language.*` |
| Intent | `HD/cpp/src/README.md` |

**Look for:** duplicate implementations, unclear ownership, include cycles, dead backup trees under `HD/backups/` not linked by build.

---

## 4. Quality Dimensions

Score each surface **1–5** on each dimension (or N/A). Evidence required for any score ≤2 or ≥4.

| Dim | Name | 1 | 3 | 5 |
|-----|------|---|---|---|
| **C** | Correctness | Silent wrong actions | Works on happy path | Guards + validated preconditions |
| **R** | Resilience | One popup ends the run | Some recovery | Bounded recovery + escalate/skip |
| **M** | Multi-instance safety | Shared mutable races | Mostly isolated | Clear ownership + no cross-talk |
| **O** | Observability | Opaque hangs | Some logs | Actionable codes + optional timing |
| **K** | Maintainability | God-file only | Partial extract | Clear module boundary |
| **P** | Performance / resource | Disk/CPU thrash default | Acceptable | Low-resource defaults honored |
| **S** | Security / account integrity | Easy corruption/leak | Basic care | Validate + encrypt + no secret logs |

**Surface score** = average of applicable dims.  
**Overall** = weighted average:

| Surface | Weight |
|---------|--------|
| S1 Orchestration | 15% |
| S2 Injection + HSV | 15% |
| S5 Farming | 15% |
| S7 Emulator I/O | 12% |
| S8 Recovery | 12% |
| S6 Market/storage | 10% |
| S9 Crypto/config | 8% |
| S4 OCR | 5% |
| S3 Templates | 4% |
| S10 Decomposition | 4% |

---

## 5. Severity Rubric (findings)

| Sev | Meaning | Examples |
|-----|---------|----------|
| **P0** | Data loss / account corruption / cross-account write | Save wrong slot; inject without stop; clobber live XML |
| **P1** | Run-killing or systematic wrong game action | Plant never finds empty fields after APK update; transfer buys wrong shop |
| **P2** | Frequent stalls or flaky multi-instance | ADB hang no timeout; recovery loop; OCR false silo-full every cycle |
| **P3** | Quality / maintainability | Raw pointers; duplicate gesture code; dead paths |
| **P4** | Nit / optional polish | Naming, comment drift |

**Priority order for fixes:** P0 → P1 → P2 → then P3 only if touching the same area.

---

## 6. Checklist by surface

Use as a review form. Mark `Y` / `N` / `?` and cite file:symbol or log evidence.

### S1 Orchestration
- [ ] Rotation always `SaveAccountToSlot` before next `LoadAccountFromSlot`
- [ ] Force-stop game before pushing `storage_new.xml`
- [ ] Inject runs after load and before farming decisions that need markers
- [ ] Failed load aborts slot work (does not farm previous account state)
- [ ] `TransferRequest` lifetime is clear across instances
- [ ] GUI start/stop does not leave orphan threads

### S2 Injection + HSV
- [ ] Required inject payloads present next to release exe (`injecthacks/`)
- [ ] Optional `inject5` / `nature_new_3` absence does not abort
- [ ] Empty / growing / grown HSV match documented contract
- [ ] Field logic keys off **state color**, not crop-type color
- [ ] Rebuild path documented for APK asset changes

### S3 Templates
- [ ] Template cache bounded (documented 96 + LRU)
- [ ] Low-variance placeholders rejected (`meanStdDev`)
- [ ] OpenCL only when `VisionGpuAssist` enabled; CPU fallback works
- [ ] Match ROIs not accidentally full-frame where coords exist

### S4 OCR
- [ ] Hot path uses `thread_local` API in `tesseract_ocr.cpp`
- [ ] No unbounded `new TessBaseAPI` without matching delete (audit `Storage.cpp` etc.)
- [ ] Digit preprocessing (scale/threshold/noise) applied where digits are small
- [ ] Callers treat OCR failure as unknown, not zero/full

### S5 Farming
- [ ] Silo-full path uses recovery + market/sell pressure, not infinite harvest
- [ ] Harvest anchors on grown HSV; plant on empty HSV
- [ ] Gestures stay inside 640×480 profile / `CoordinateProfile`
- [ ] Dense field sweeps do not re-open wrong menus without recovery

### S6 Market / storage
- [ ] Empty crate detection before price UI
- [ ] Advertise respects config
- [ ] Failure writes debug evidence under `logs/` when designed to
- [ ] Storage master / seller handshake has timeout and cancel
- [ ] No hardcode that assumes single instance only

### S7 Emulator I/O
- [ ] Factory enforces 640×480 + DPI 100 (or hard-fails with clear log)
- [ ] Minitouch start + revive heartbeat
- [ ] Crash watchdog does not fight manual stop
- [ ] ADB commands have timeouts / persistent session policy documented
- [ ] Serial selection matches MEmu `21503 + 10*instance` pattern

### S8 Recovery
- [ ] Startup / purchase / silo-full dismissers are bounded
- [ ] Not-responding dialog detection reloads safely
- [ ] Zoom recovery does not fight intentional close-up flows
- [ ] Flow states (`SetFarmFlowState` / sales) log transitions at INFO appropriately

### S9 Crypto / config
- [ ] `.YaJing` write only after XML validation
- [ ] Legacy `.nxrth` migration is one-way safe (keeps backup)
- [ ] Logs never dump full session tokens / passwords
- [ ] Config paths resolve under `%APPDATA%\NXRTH_Premium\` consistently

### S10 Decomposition
- [ ] New code lands under `cpp/src` when touching extracted areas
- [ ] No second copy of minitouch/ADB helpers reintroduced in legacy files
- [ ] Public headers match exported symbols used by GUI
- [ ] `HD/backups/` not part of active build

---

## 7. Evidence methods

| Method | When | How |
|--------|------|-----|
| Static review | Always | This checklist + `BOT_MAP` routing table |
| Resolution gate | Before runtime tests | Confirm emulator 640×480 / 100 DPI; factory logs |
| Injection smoke | After APK or inject rebuild | Preview `injecthacks/.../crop_state_colors.png`; one farm scan |
| HSV capture | Marker doubt | Opt-in HSV CSV; compare to H=150/25/90 contract |
| Single-slot farm | Correctness | One account: harvest → plant → silo path |
| Multi-instance | Safety | 2+ instances, staggered rotation; watch ADB/minitouch |
| OCR spot-check | Silo/price bugs | Save ROI PNGs; compare Tesseract string vs human |
| Timing CSV | Perf only | Enable `Timing CSV` / `ActionTimingLog=1` briefly |
| Transfer dry-run | Storage path | One seller + master with timeout expectations |

Do **not** enable DEBUG ingest + full action CSV + GPU assist all at once on low-end hosts when measuring “normal” performance.

---

## 8. Known strengths (baseline, re-verify)

Carry-forward from earlier notes; re-check rather than assume forever.

| Item | Location | Note |
|------|----------|------|
| Thread-local Tesseract | `tesseract_ocr.cpp` | Removes global OCR mutex on hot path |
| Template cache + variance gate | `bot_logic.cpp` | Bounded cache; rejects blank placeholders |
| Adaptive match + optional OpenCL | `bot_logic.cpp` | Capability exists; **default off** |
| OCR preprocessing | `tesseract_ocr.cpp` | Upscale / threshold / contour clean for small digits |
| Module extraction of infra + recovery | `cpp/src/infra`, `bot/ScreenRecovery` | Reduces some legacy duplication |
| Low-resource logging defaults | `runtime-logging-and-performance.md` | Caps disk and DEBUG noise |

---

## 9. Risk backlog seeds (rank during audit)

Start evaluation with these; promote/demote with evidence.

| ID | Risk | Surfaces | Likely sev |
|----|------|----------|------------|
| R1 | Inject/APK drift blinds HSV farming | S2, S5 | P1 |
| R2 | Resolution/DPI not enforced → all coords wrong | S7, S1 | P1 |
| R3 | Account save/load race or wrong slot | S1, S9 | P0 |
| R4 | ADB/minitouch hang without recovery | S7, S8 | P1–P2 |
| R5 | OCR false full/empty drives bad economy decisions | S4, S5, S6 | P2 |
| R6 | Transfer buyer/seller deadlock or wrong target | S6, S1 | P1 |
| R7 | Popup recovery infinite loop or thrash | S8 | P2 |
| R8 | Shared caches/mutexes limit multi-instance | S3, S4, M | P2–P3 |
| R9 | Ad-hoc TessBaseAPI / raw new leak or double-init | S4 | P3 (P2 if crashes) |
| R10 | Legacy + new dual paths diverge | S10 | P2–P3 |

---

## 10. Audit deliverable format

For each audit pass, produce:

```markdown
## Audit <date> — Project Wheat

### Scores
| Surface | C | R | M | O | K | P | S | Avg |
|---------|---|---|---|---|---|---|---|-----|
| S1 ...  |   |   |   |   |   |   |   |     |
...
**Weighted overall:** X.X / 5

### Findings
| ID | Sev | Surface | Summary | Evidence | Suggested fix |
|----|-----|---------|---------|----------|---------------|
| F1 | P1 | S2 | ... | file:line / log | ... |

### Fix order
1. ...
2. ...

### Out of scope this pass
- ...
```

---

## 11. Suggested first pass (time-boxed)

If only a few hours:

1. **S2 + S5** — Confirm injection + HSV is what farming actually uses; one plant/harvest path walkthrough.
2. **S1 + S9** — Load/save/inject order and slot integrity.
3. **S7 + S8** — Minitouch/ADB revive and popup bounds.
4. **S6** — Sell + transfer timeouts only if multi-account economy is in use.
5. **S3/S4** — Spot-check only; do not let OpenCL/smart-pointer cleanup dominate the report.

---

## 12. Changelog

| Ver | Date | Change |
|-----|------|--------|
| v1 | (prior) | Orientation note: a few files, strengths, two risks |
| v2 | 2026-07-19 | Full surfaces, weights, checklist, severity, evidence methods; injection/HSV-first |
| v2.1 | 2026-07-19 | First-pass audit completed → [`docs/audits/2026-07-19-project-wheat-first-pass.md`](audits/2026-07-19-project-wheat-first-pass.md); 3.44/5.00 overall, one P0 (missing account save) |
| v2.2 | 2026-07-19 | Fix pass: P0 (SaveAccountToSlot in rotation), P1 (resolution inspector), P2 (ADB timeout 30s, gesture margins), P3 (BotEngine.h deprecation notes) |
