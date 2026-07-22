# First-Pass Code Audit — Project Wheat

## Goal

Run a **static first-pass audit** of the C++ bot using [docs/code-evaluation-framework.md](../code-evaluation-framework.md), produce scored findings, and write a dated audit note under `docs/`.

**Primary question:** Can multi-instance account rotation run for hours without silent wrong actions or unrecoverable stalls?

This pass is **read-only analysis** (no functional code changes unless you later ask to fix P0/P1 items).

---

## Scope (time-boxed first pass)

Follow framework §11 order:

| Priority | Surface | Files | Weight |
|----------|---------|-------|--------|
| 1 | **S2** Injection + HSV | `AccountFiles.cpp` (`InjectImportantFiles`), `bot_logic.cpp` (HSV tables / scanners), farming color helpers | 15% |
| 1 | **S5** Farming | `operations/farming/Farming.cpp`, `Animals.cpp` (spot-check only) | 15% |
| 2 | **S1** Orchestration | `PremiumLoop.cpp`, `Cycles.cpp`, load/save order | 15% |
| 2 | **S9** Crypto / account integrity | `AccountFiles.cpp`, `AhjieCrypto.cpp`, migration paths | 8% |
| 3 | **S7** Emulator I/O | `EmulatorControl.cpp`, `Adb.cpp`, `MinitouchClient.cpp`, `TouchInput.cpp` | 12% |
| 3 | **S8** Recovery | `ScreenRecovery.cpp`, `RuntimeHealth.cpp`, `FlowLogging.cpp` | 12% |

**Out of scope this pass** (unless a P0/P1 is obvious while reading):

- Full S3 template/OpenCL deep dive
- Full S4 OCR call-site matrix (except obvious unsafe `new TessBaseAPI`)
- Full S6 market/storage transfer protocol
- S10 decomposition purity
- Live emulator runs / multi-instance stress (static only unless environment is already up and you request it)

---

## Method

For each in-scope surface:

1. Walk the **happy path** against the framework checklists (Y/N/?).
2. Record **evidence** as `file:symbol` (and line ranges where useful).
3. Score dimensions **C / R / M / O / K / P / S** (1–5 or N/A) per framework §4.
4. File findings with **P0–P4** severity per §5.
5. Seed risks R1–R10: confirm, demote, or close with evidence.

### Known hotspots to verify (already spotted)

| Lead | Why it matters |
|------|----------------|
| Resolution inspector in `PremiumLoop.cpp` appears **commented out** (~896–909) while factory still **sets** 640×480/DPI in `EmulatorControl.cpp` | R2 — contract set at create time but maybe not enforced at farm start |
| Farming uses `GetCropGrownHsvRange` / `FindEmptyFieldsByColor` | Confirms injection-first design; check fail-open vs fail-closed |
| `ScreenRecovery` APIs have bounded `maxPasses` / `maxAttempts` | Good resilience pattern; verify call sites honor bounds |
| `Storage.cpp` still has raw `new TessBaseAPI` (from earlier grep) | S4 leak / lifecycle; note as P3/P2 if crashy |
| Dual headers: `BotEngine.h` still declares `RunPremiumBot` / account APIs while implementations live under `cpp/src` | S10 ownership clarity only |

---

## Deliverable

Create:

```text
docs/audits/2026-07-19-project-wheat-first-pass.md
```

Using the template in framework §10:

- Score table for S1, S2, S5, S7, S8, S9 (+ brief N/A notes for skipped surfaces)
- Weighted overall (re-normalized over audited surfaces, or full weights with unscored = omitted — **document which**)
- Findings table: ID, sev, surface, summary, evidence, suggested fix
- Ordered fix backlog (no code changes in this pass)
- Explicit “out of scope / residual risk”

Also add a one-line link from:

- `docs/code-evaluation-framework.md` (changelog or “Audits” section)
- `docs/code-maps/README.md` if useful

---

## Execution steps

1. **S2 + S5 (injection / farm)**  
   - Trace inject payload list and optional-file behavior.  
   - Read HSV ranges and `Find*ByColor` / harvest-plant flow in `Farming.cpp`.  
   - Checklist §6 S2 + S5.

2. **S1 + S9 (rotation / accounts)**  
   - Trace `RunPremiumBot` → load → inject → farm → save → next slot.  
   - Validate force-stop, push path, failed-load abort, `.nxrth` → `.Ahjie` migration safety.  
   - Checklist §6 S1 + S9.

3. **S7 + S8 (I/O + recovery)**  
   - Minitouch start/revive/watchdog; ADB timeout behavior.  
   - Popup dismissers: bounds, NR dialog, silo-full.  
   - Note resolution gate gap if confirmed.  
   - Checklist §6 S7 + S8.

4. **Synthesize**  
   - Score surfaces, rank findings, write audit note, update framework link.

5. **Report to user**  
   - Short summary of top P0/P1/P2 and whether a fix PR plan is warranted next.

---

## Success criteria

- [ ] Audit note exists under `docs/audits/` with scores + findings + fix order  
- [ ] Every finding has file-level evidence  
- [ ] At least R1–R4 and R7 have confirm/demote status  
- [ ] No silent code changes; docs-only unless you approve fixes later  

---

## Optional follow-ups (after you approve)

- Fix pass for agreed P0/P1 items only  
- Live smoke: inject preview + single-slot farm (needs MEmu)  
- Second-pass audit: S3, S4, S6, S10
