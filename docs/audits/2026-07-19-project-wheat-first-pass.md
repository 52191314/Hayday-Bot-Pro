# Audit 2026-07-19 — Project Wheat First Pass

| Field | Value |
|---|---|
| Date | 2026-07-19 |
| Auditor | Static analysis per `.opencode/plans/first-pass-code-audit.md` (v2) |
| Rubric | `docs/code-evaluation-framework.md` (v2) |
| Primary question | Can multi-instance account rotation run for hours without silent wrong actions or unrecoverable stalls? |
| Status | **No — one P0 finding prevents this** (account state never saved between rotations) |
| Time box | 4h — no degradation applied (all phases completed on budget) |

---

## Scores

Surfaces scored per framework §4 (1–5, or N/A). Weighting method: **re-normalized over the six audited surfaces** (77% of framework total). Unaudited surfaces (S3, S4, S6, S10) are omitted from the overall. Re-normalized weights: S1 19.5%, S2 19.5%, S5 19.5%, S7 15.6%, S8 15.6%, S9 10.4%.

| Surface | C | R | M | O | K | P | S | Avg |
|---------|---|---|---|---|---|---|---|-----|
| S1 — Orchestration | 2 | 3 | 3 | 3 | 3 | 4 | 2 | **2.86** |
| S2 — Injection + HSV | 4 | 3 | 4 | 3 | 3 | 3 | 4 | **3.43** |
| S5 — Farming | 4 | 4 | 4 | 4 | 3 | 3 | 4 | **3.71** |
| S7 — Emulator I/O | 3 | 3 | 4 | 2 | 3 | 3 | 4 | **3.14** |
| S8 — Recovery | 4 | 4 | 4 | 4 | 4 | 4 | 4 | **4.00** |
| S9 — Crypto/config | 4 | 3 | 4 | 3 | 3 | 4 | 4 | **3.57** |

**Weighted overall: 3.44 / 5.00**

### Dimension summary

| Dim | Lowest | Highest |
|-----|--------|---------|
| **C** Correctness | S1 (2) — missing account save | S2/S5/S8/S9 (4) |
| **R** Resilience | S1/S7/S9 (3) — missing save means any stall loses work, unbounded ADB calls | S5/S8 (4) |
| **M** Multi-instance | All (3–4) — good per-instance isolation | S2/S5/S7/S8/S9 (4) |
| **O** Observability | S7 (2) — no ADB timeout logging, no resolution check | S5/S8 (4) |
| **K** Maintainability | S1 (3) — 240-line closure-heavy function | S8 (4) |
| **P** Performance | S2/S5/S7 (3) — acceptable | S1/S8/S9 (4) |
| **S** Security | S1 (2) — missing save bypasses encryption entirely | S2/S5/S7/S8/S9 (4) |

---

## Hotspot disposition

| ID | Lead | Status | Evidence |
|----|------|--------|----------|
| **H1** | Resolution inspector commented out (R2) | **Confirmed — P1** | `PremiumLoop.cpp` `RunPremiumBot` L893–909 fully commented; `EmulatorControl.cpp` ~L322–328 sets 640×480/DPI 100 at factory but never re-verified at farm start |
| **H2** | Farming uses HSV contract | **Confirmed — design verified** | `Farming.cpp` `FindGrownCropByColor` calls `GetCropGrownHsvRange` (L198); `FindEmptyFieldsByColor` uses `GetCropStateHsvRange(Empty)` (L311); state machine driven by H=150/25/90 ranges. Fail-open when markers absent — farm continues but may miss grown crops |
| **H3** | Recovery dismissers bounded | **Confirmed — well-implemented (P4/mitigated)** | `ScreenRecovery.h` L9–14: `maxPasses=2` (purchase), `maxPasses=4` (startup), `maxAttempts=2` (silo); loops at `.cpp` L265, L318, L367 honor these bounds. Callers checked: all pass explicit or default bounds. R7 demoted |
| **H4** | Ad-hoc `new TessBaseAPI` in Storage.cpp (R9) | **Confirmed — P2** | `Storage.cpp` L136: `new tesseract::TessBaseAPI()` — raw pointer, no matching `delete` or `End()` in the code path. Leaks per market/storage scan |
| **H5** | Dual header declarations | **Confirmed — P3 (cleanliness)** | `BotEngine.h` L12/L21–22 and `PremiumLoop.h` L3, `AccountFiles.h` L4–5 both declare same symbols. Active code uses `PremiumLoop.h`/`AccountFiles.h`; `BotEngine.h` declarations are dead |

---

## Findings

### P0 — Data loss / account corruption

| ID | Surface | Summary | Evidence | Suggested fix |
|----|---------|---------|----------|---------------|
| **F1** | S1, S9 | **Auto-rotation never calls SaveAccountToSlot** — account state from farming cycles is never persisted. When the rotation moves to the next slot, `LoadAccountFromSlot` overwrites the device XML with the last manually-saved (stale) state. All progress since the last manual save is lost. | `PremiumLoop.cpp` `RunPremiumBot`: `LoadAccountFromSlot` at L1156, `RunAccountFarmCycle` at L1374, then L1376 `continue` — no `SaveAccountToSlot` in between. `SaveAccountToSlot` is called **only** from `premium gui.cpp` (manual GUI save), never from the auto loop. | Add `SaveAccountToSlot(instanceId, i)` at the end of the batch loop (before L1376's `continue`), gated by `!isSingleAccountMode` to match the load pattern. |

### P1 — Run-killing or systematic wrong action

| ID | Surface | Summary | Evidence | Suggested fix |
|----|---------|---------|----------|---------------|
| **F2** | S7, S1 | **Resolution inspector commented out (R2)** — Resolution set at factory (`EmulatorControl.cpp` L322–328) but never re-verified at farm start. Any user change to 640×480/100 DPI goes undetected; all hardcoded coordinates, templates, and ROIs would be wrong. | `PremiumLoop.cpp` `RunPremiumBot` L893–909: entire block commented out. First active line after the block is L910 `AddLog("Bot Started.")`. | Uncomment the inspector with a non-fatal warning + skip action if wrong, or re-add as a pre-farm check in `RunAccountFarmCycle`. |

### P2 — Frequent stalls or flaky multi-instance

| ID | Surface | Summary | Evidence | Suggested fix |
|----|---------|---------|----------|---------------|
| **F3** | S7 | **ADB command timeout too short (R4)** — `RunCmdHidden` has a 10s timeout, but slow emulator ADB operations (file push/pull during injection) can exceed this. `PersistentShell` (used for taps) has no per-command timeout and can hang if ADB stalls. | `ProcessRunner.cpp` L24: `WaitForSingleObject(pi.hProcess, 10000)` — 10s is tight for large sctx pushes (up to 800ms/push × many files). `PersistentShell::SendCommand` is fire-and-forget with no response wait (L77–95). | Increased to 30s in `ProcessRunner.cpp`. For `PersistentShell`, the write-only pipe design prevents easy per-command timeout; the fallback `RunCmdHidden` path now has a safer 30s timeout. |
| **F4** | S4 (spot-check) | ~~**Ad-hoc OCR instance leaks (R9/H4)**~~ | **NOT A BUG** — Found cleanup code at `Storage.cpp` L213–216: `delete ri; api->End(); delete api;`. The raw `new` is  matched by `delete` + `End()`. Re-classified as P4 (raw pointer style, no leak). | No code change needed. Consider switching to `thread_local` API from `tesseract_ocr.cpp` for consistency. |
| **F5** | S5 | **Dense grid gesture margins hardcoded for 640×480** — `ExecuteDenseGridGesture` hardcodes `screenWidth=640, screenHeight=480` as fallback, and margin values (80px) are not relative to actual screen dimensions. Since the resolution gate is disabled (F2), a wrong-resolution user would get wrong gesture coordinates. | `Farming.cpp` `ExecuteDenseGridGesture` L36–49: margins hardcoded 80px; fallback dimensions 640×480; actual screen only used when capture succeeds. | Compute margins as a fraction of screen dimensions (e.g., `marginX = screenWidth * 0.125`). Best fix: re-enable the resolution gate (F2). |

### P3 — Quality / maintainability

| ID | Surface | Summary | Evidence | Suggested fix |
|----|---------|---------|----------|---------------|
| **F6** | S1, S10 | **RunAccountFarmCycle is a 240-line closure-heavy function** — TryHarvest, TryHarvestForCropMode, TryAutoTom, TryPlant, plantTargetsWithCrop are all lambdas capturing by reference, making unit testing impossible and adding cognitive load for state flow debugging. | `PremiumLoop.cpp` L70–889: all state machine logic embedded in a single `static void` function as nested closures. | Extract into named helper functions (`TryHarvestImpl`, `TryPlantImpl`, `TryAutoTomImpl`) in separate .cpp/.h files under `cpp/src/operations/farming/`. |
| **F7** | S2 | **Optional payload absence not signaled to farm flow** — `inject5.nxrth` (nature_new_3.sctx) is optional and skipped silently (L623). If markers are missing, HSV farming may produce fewer matches, but the farm flow has no awareness of this degraded mode. | `AccountFiles.cpp` L611: `{"inject5.nxrth", "nature_new_3.sctx", false}` — optional. H2 confirmed that farming uses HSV ranges that depend on injection markers. | Track a `bool optionalPayloadMissing` and log at the start of the farm cycle if vision may be degraded. |
| **F8** | S10 | **Dual header declarations not used (H5)** — `BotEngine.h` still declares `RunPremiumBot`, `SaveAccountToSlot`, `LoadAccountFromSlot` while active implementations live under `cpp/src/bot/` with their own headers. | `BotEngine.h` L12/L21–22 vs `PremiumLoop.h` L3, `AccountFiles.h` L4–5. `PremiumLoop.cpp` includes `AccountFiles.h`, not `BotEngine.h`. | Remove legacy declarations from `BotEngine.h` or add a deprecation `#pragma message`. |

---

## Fix order (by severity, then ease)

### Fixed in this pass

| Finding | Fix |
|---------|-----|
| **F1** (P0) — Missing `SaveAccountToSlot` in rotation | Added `SaveAccountToSlot(instanceId, i)` after `RunAccountFarmCycle` in PremiumLoop.cpp ~L1374, gated by `!isSingleAccountMode` |
| **F2** (P1) — Resolution inspector commented out | Uncommented with non-fatal warning in PremiumLoop.cpp `RunPremiumBot` — warns if wrong but does not stop the bot |
| **F3** (P2) — ADB timeout too short (10s) / persistent shell no timeout | Increased `RunCmdHidden` timeout from 10s to 30s in `ProcessRunner.cpp`; added `OutputDebugString` logging on timeout |
| **F4** (P2) — OCR leak in Storage.cpp | **Not a bug** — cleanup exists at Storage.cpp L213–216 (`delete ri; api->End(); delete api;`). Downgraded to P4 (style only) |
| **F5** (P2) — Gesture margins hardcoded for 640×480 | Changed margins to `(int)(screenWidth * 0.125f)` / `(int)(screenHeight * 0.167f)` in `Farming.cpp` `ExecuteDenseGridGesture` |
| **F8** (P3) — Dual header declarations | Added deprecation comments in `BotEngine.h` pointing to `PremiumLoop.h`, `AccountFiles.h`, `ScreenRecovery.h` |

### Deferred (not yet fixed)

| Finding | Reason |
|---------|--------|
| **F6** (P3) — Extract farm cycle lambdas | Significant refactor (risky); defer unless explicitly requested |
| **F7** (P3) — Track optional payload status | Requires struct changes across `bot_logic.h` and `AccountFiles.cpp`; defer unless requested |

---

## Checklist appendix

### S1 — Orchestration

| Item | Status | Evidence |
|------|--------|----------|
| Rotation always SaveAccountToSlot before next LoadAccountFromSlot | **N** — **P0 finding F1** | PremiumLoop.cpp L1156 (Load), L1374 (farm), L1376 (continue) — no save |
| Force-stop game before pushing storage_new.xml | **Y** | AccountFiles.cpp `LoadAccountFromSlot` L951: `shell am force-stop` before push at L954 |
| Inject runs after load and before farming decisions that need markers | **Y** | `InjectImportantFiles` called separately (before `RunPremiumBot`) in GUI flow; rotation assumes inject is already done |
| Failed load aborts slot work | **Y?** | AccountFiles.cpp L918–921: returns early if file missing; L922–926 checks ADB serial; L937–943 checks validation — all abort with clear error |
| TransferRequest lifetime clear across instances | **N/A** (S6 out of scope) | |
| GUI start/stop does not leave orphan threads | **?** | Not audited (GUI code out of scope) |

### S2 — Injection + HSV

| Item | Status | Evidence |
|------|--------|----------|
| Required inject payloads present next to release exe | **Y** | AccountFiles.cpp L504–509: validates required payloads (`inject.nxrth`–`inject4.nxrth`) before starting injection thread |
| Optional inject5/nature_new_3 absence does not abort | **Y** | L497–502: `inject5.nxrth` marked `false`; L623–625: skipped with log |
| Empty/growing/grown HSV match documented contract | **Y** | bot_logic.cpp L410–416: Empty H≈150, Growing H≈25, Grown H≈90. Confirmed in farming helpers |
| Field logic keys off state color, not crop-type color | **Y** | Farming.cpp `FindEmptyFieldsByColor` uses Empty HSV (L311); `FindGrownCropByColor` calls `GetCropGrownHsvRange` (L198); `FindGrowingFieldsByColor` uses Growing HSV (L391) |
| Rebuild path documented for APK asset changes | **?** | Not audited (payload build tools outside static scope) |

### S5 — Farming

| Item | Status | Evidence |
|------|--------|----------|
| Silo-full path uses recovery + market/sell pressure, not infinite harvest | **Y** | PremiumLoop.cpp L687–714: harvest → silo full → emergency plant → market sales → retry harvest |
| Harvest anchors on grown HSV; plant on empty HSV | **Y** | Farming.cpp L198 (grown), L311 (empty) |
| Gestures stay inside 640×480 profile | **Y?** | Hardcoded fallback to 640×480 (Farming.cpp L36–37); actual screen used when available — F2 concern if resolution is wrong |
| Dense field sweeps do not re-open wrong menus without recovery | **Y** | PremiumLoop.cpp L152: `RecoverFarmViewAfterDenseSweep` after harvest; L468/L576 after plant |

### S7 — Emulator I/O

| Item | Status | Evidence |
|------|--------|----------|
| Factory enforces 640×480 + DPI 100 or hard-fails | **Y** at factory (EmulatorControl.cpp L322–328), **N** at runtime (H1/F2) | Resolution check commented out in PremiumLoop |
| Minitouch start + revive heartbeat | **Y** | PremiumLoop.cpp L917: `StartMinitouchStealth`; L1015: `HandleReviveHeartbeat` in wait loop |
| Crash watchdog does not fight manual stop | **Y** | `EmulatorCrashWatchdog` at L926 — checks `bot.isRunning` before actions |
| ADB commands have timeouts / session policy documented | **N** — **P2 finding F3** | No timeout on RunAdbCommand |
| Serial selection matches MEmu 21503 + 10*instance pattern | **Y** | EmulatorControl.cpp L333: `21503 + (actualVm * 10)` |

### S8 — Recovery

| Item | Status | Evidence |
|------|--------|----------|
| Startup/purchase/silo-full dismissers are bounded | **Y** | ScreenRecovery.h L9–14: maxPasses=2/4, maxAttempts=2 — H3 confirmed |
| Not-responding dialog detection reloads safely | **Y** | ScreenRecovery.cpp `ReloadIfGameNotResponding` L548–564: detects NR dialog, calls `ReloadAccountForRecovery` which calls `LoadAccountFromSlot` |
| Zoom recovery does not fight intentional close-up flows | **Y?** | Gated by `g_Intervals.autoRecoverZoomAfterFieldSweep` (default on); anchor heuristic prevents fighting |
| Flow states log transitions at INFO appropriately | **Y** | FlowLogging.cpp `SetFarmFlowState` L131: logs at INFO level with detail |

### S9 — Crypto / config

| Item | Status | Evidence |
|------|--------|----------|
| `.YaJing` write only after XML validation | **Y** | AccountFiles.cpp `SaveAccountToSlot` L795–802: `ValidateDecryptedAccountXml` before encrypting |
| Legacy `.nxrth` migration is one-way safe (keeps backup) | **Y** | AccountFiles.cpp `MigrateLegacyAccountSlotIfNeeded` L317–366: validates, copies backup to `legacy_nxrth_backup/`, then encrypts to `.YaJing` |
| Logs never dump full session tokens / passwords | **Y** | No secrets observed in log paths inspected |
| Config paths resolve under `%APPDATA%\NXRTH_Premium\` consistently | **Y** | Paths.cpp: uses `%APPDATA%\NXRTH_Premium\` as base |

---

## Risk seed disposition

| ID | Risk | Status |
|----|------|--------|
| R1 | Inject/APK drift blinds HSV farming | **Confirmed** — optional payload absence not propagated to farm flow (F7/P3) |
| R2 | Resolution/DPI not enforced → all coords wrong | **Confirmed P1** — inspector commented out (F2/P1) |
| R3 | Account save/load race or wrong slot | **Confirmed P0** — never saved in auto-rotation (F1/P0) |
| R4 | ADB/minitouch hang without recovery | **Confirmed P2** — no timeout on RunAdbCommand (F3/P2) |
| R5 | OCR false full/empty | Not in scope (S4 excluded) |
| R6 | Transfer buyer/seller deadlock | Not in scope (S6 excluded) |
| R7 | Popup recovery infinite loop or thrash | **Demoted** — all dismissers bounded (H3 confirmed); R7 is mitigated |
| R8 | Shared caches/mutexes limit multi-instance | Not in scope (S3 excluded) |
| R9 | Ad-hoc TessBaseAPI | **Confirmed P2** — leak in Storage.cpp (F4/P2) |
| R10 | Legacy + new dual paths diverge | **Confirmed P3** — dual header declarations (F8/P3) |

---

## Out of scope / residual risk

- S3 (Templates) — full deep dive deferred to second pass
- S4 (OCR) — full call-site matrix deferred; H4 spot-check done
- S6 (Market/storage) — full protocol audit deferred
- S10 (Decomposition) — full purity audit deferred; H5 spot-check done
- Live emulator / multi-instance stress — static only
- GUI code (`premium gui.cpp`) — not audited

---

## Answer to primary question

**Can multi-instance account rotation run for hours without silent wrong actions or unrecoverable stalls?**

**No — not yet.** The P0 finding (F1 — missing `SaveAccountToSlot` in auto-rotation) means every farming cycle's progress is lost when the next account slot is loaded. After a crash, restart, or unexpected stall, all accounts revert to their last manually-saved state. Combined with the disabled resolution gate (F2/P1) and unbounded ADB calls (F3/P2), a multi-hour run will almost certainly produce one of:

- **Silent wrong action** (wrong coordinates due to undetected resolution change — F2)
- **Unrecoverable stall** (ADB hang with no timeout — F3)
- **Lost progress** (no save between rotations — F1)

**A fix pass addressing F1 (P0), F2 (P1), and F3 (P2) would change this answer to "likely yes."**

---

## Links updated

- `docs/code-evaluation-framework.md` — See changelog below
- `docs/code-maps/README.md` — Added reference to this audit
