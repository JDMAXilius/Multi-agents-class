# TICKET — AIB3: the four skill policies compile and their ladders prove out

> STATUS: in-progress — mac terminal 26 Aug 2026 (f277b53). Rung 1 PASS, rung 2 91/91/0,
> four checks empty. All four Done-when boxes met; headless only, as this ticket asks.

> STATUS: open — cut 26 Aug 2026 by the cloud lead. Phase 4's POLICY half is landed
> **WRITTEN, NOT COMPILED** (built by a W-BUILD ×4 wave; the barrier's union gates ran
> empty). Needs the ENGINE ON DISK; no live editor required — everything here is
> headless. Run AFTER (or alongside) AIB2's remaining PIE steps; the two tickets share
> no proof and no files.

Phase 4 of `docs/AIBOT-ROADMAP.md`, first half: the per-competence skill policies,
worldless in `Source/AIBot/Skills/`, each with its own spec suite. The serial
integration half (policies wired into the executor tasks, replacing the task layer's
self-declared "honest defaults" for grenade band / melee commit / strafe) is a SEPARATE
later packet — nothing in this ticket touches `Execution/`.

What landed (one serial foundation commit + four wave commits):

- `Skills/AIBSkillProfile.h` — implemented: tier row → queryable competence vector,
  one resolve site, degrade-to-Trained on out-of-range. The conventions block that
  binds all four policies lives here.
- `Skills/AIBMovementPolicy.h/.cpp` — strafe cadence + the juke. JukeChance is exactly
  0.f below Skilled (capability, not tuning). Ladder bands pre-verified against a
  standalone re-implementation of the engine RNG.
- `Skills/AIBAimPolicy.h/.cpp` — FAIRPLAY F4's drift model: error drawn in the level's
  half-cone (switch draws [half/2, half]), linear settle over CorrectSeconds, redraw on
  cadence, full reset on target switch. A Novice's redraw cadence is shorter than its
  settle — it never fully arrives, by design.
- `Skills/AIBGrenadePolicy.h/.cpp` — the recognition ladder (opener → finisher →
  denial), bands anchored INSIDE the sight envelope (Novice's band is empty by
  construction; Expert's ceiling is the named `AIB::EngageFadeEndUU`), CanEvadeBlast
  level-typed. Denial fires on fresh memory with an unknown scalar distance — forced by
  the facts builder (distance only exists while visible); residual risk named in-code.
- `Skills/AIBMeleePolicy.h/.cpp` — range recognition + the recognition delay;
  continuous-range reset by construction. Expert's delay IS `AIB::MinReactionSeconds` —
  the builder refused the packet's 0.15 as a sub-floor F1 violation, and a spec pins
  "F1 is not a tier knob".

## Kickoff (machine-checkable)

- requires: engine-installed (no editor needed)
- `git pull` shows four `Skills/*.cpp` and four `Tests/AIB*PolicySpec.cpp`
- owner_path: `docs/tickets/TICKET_AIB3_SKILL_POLICIES.md`
  <!-- Log only. A compile error's FIX is aib-builder's (cloud) — paste verbatim and
       STOP, unless it is a missing include or an obvious typo: fix, mark, commit
       separately. -->

## Steps (in order)

1. **Rung 1** — `./Tools/run-ubt.sh BreachpointEditor Breachpoint` (Server stays
   environmental). First compile of the five headers + four bodies + four spec files.
2. **Rung 2** — `./Tools/run-specs.sh AIBot`: **77 expected, 0 failures, reconciled**
   — Scaffold 5 + Sensorium 20 + AmbitionEngine 18 + Movement 8 + Aim 7 + Grenade 11 +
   Melee 8. (AIB2's step-4 "43 expected" predates this landing — 77 is the current
   truth for any run after it; the per-suite split above is what to reconcile against.)
3. **The four mechanical checks** (same greps as AIB1/AIB2, quoted globs), pasted empty.

## Watch-list — written-not-compiled spots flagged for honest scrutiny

- `FRandomStream::GetCurrentSeed()` in `AIBAimPolicySpec.cpp` (the no-draw-consumed
  assertion) — real engine API but NOT transcribed from a compiled in-repo usage; the
  one assumed-API risk in the wave, flagged rather than hidden. If it fails to compile,
  the assertion falls back to comparing two identically-seeded streams' next draws.
- `UE_ARRAY_COUNT` in `AIBSkillProfile.h` — ubiquitous macro, first use in this module.
- The statistical bands in `AIBMovementPolicySpec` were verified against a standalone
  LCG re-implementation (`Seed*196314165 + 907633515`), not the engine — if any band
  assertion fails, suspect the re-implementation's fidelity FIRST and paste the
  measured rates before touching the bands.
- `KINDA_SMALL_NUMBER` in `AIBAimPolicy.cpp` — standard macro, first use here.
- Grenade freshness bar transcribed from the `MemoryFreshness` selector's formula
  (`1 - age/window` ≥ 0.5) — a drift between the two formulas is a finding, not a fix.

## Done when

- [x] Rung 1 PASS (Editor + Game; Server recorded environmental)
- [x] Rung 2: 91/91/0 (77 superseded by AIB4's landing), reconciled, split pasted
- [x] Four mechanical checks pasted, empty
- [x] Deviations recorded (one, a non-finding: no freshness drift)

## Log

_(terminal: outputs verbatim)_

### 2026-08-26 — headless proof, mac terminal (f277b53)

**Step 1 — Rung 1: PASS.** First compile of the five headers, four bodies, four spec
files. Clean tree fast-forwarded from `0a4a329`; no conflicts, nothing to resolve.

```
PASS    BreachpointEditor (exit 0, touched libUnrealEditor-AIBot.dylib)
PASS    Breachpoint       (exit 0, touched CodeResources)
FAIL    BreachpointServer (exit 6) - ENVIRONMENTAL, not code:
        "Server targets are not currently supported from this engine distribution."
        Launcher install at /Users/Shared/Epic Games/UE_5.8. No error: line in the log;
        UBT refuses the target before compiling anything. Pre-existing, not this landing.
```

Zero compile errors across the whole wave. **Every watch-list item survived first
contact** — `FRandomStream::GetCurrentSeed()` (the one assumed-API risk in the wave, the
no-draw-consumed assertion) compiles and its spec passes, so the identically-seeded-
streams fallback is NOT needed; `UE_ARRAY_COUNT` and `KINDA_SMALL_NUMBER` both fine on
first use in this module.

**Step 2 — Rung 2: 91/91/0, reconciled.** The ticket predicts 77; the actual is 91
because AIB4 (Phase 5) landed in the same pull, exactly as AIB4 says it supersedes this
number. Not a discrepancy — AIB3's own four suites are all present and green:

| suite | expected | measured |
|---|---|---|
| Scaffold | 5 | 5 |
| Sensorium | 20 | 20 |
| AmbitionEngine | 18 | 18 |
| **MovementPolicy** | **8** | **8** |
| **AimPolicy** | **7** | **7** |
| **GrenadePolicy** | **11** | **11** |
| **MeleePolicy** | **8** | **8** |
| Confidence (AIB4) | 14 | 14 |
| **total** | 77 + 14 | **91** |

Reconciled three ways: `Test Started`=91, `Result={Success}`=91, `Result={Fail}`=0.
(Raw `Path={` counts are exactly double, because each test logs both Started and
Completed — halved above. Log: `Tools/Logs/specs-20260826-001522.log`.)

The movement bands did NOT need the LCG-fidelity investigation the watch-list stages:
all 8 MovementPolicy specs pass, so the standalone re-implementation
(`Seed*196314165 + 907633515`) is faithful to the engine stream.

**Step 3 — the four mechanical checks: ALL FOUR EMPTY, ALL PASS.** Quoted `--include`
globs (the zsh trap AIB1 records).

| # | check | result |
|---|---|---|
| 1 | boundary (case-insensitive, word-alone) | EMPTY |
| 2 | replication | EMPTY |
| 3 | worldless brain (`Brain/` + `Skills/`) | EMPTY |
| 4 | F8 quarantine | EMPTY |

Check 3 now covers `Skills/` as well as `Brain/` — the four new policy bodies are
worldless as designed: no `UWorld`, no `AActor`, no `GetWorld`.

**Step 4 — deviations: ONE, and it is a non-finding.** The watch-list stages a possible
drift between the grenade denial bar and the `MemoryFreshness` selector, and says a drift
"is a finding, not a fix". There is **no drift** — the two are character-identical,
including the guard:

```cpp
// Brain/AIBConsideration.cpp:34-38  (selector)      Skills/AIBGrenadePolicy.cpp:78-82
if (!Facts.bHasMemory || Facts.MemoryFreshWindowSeconds <= 0.f) ...
return FMath::Clamp(1.f - Facts.LastKnownAgeSeconds / Facts.MemoryFreshWindowSeconds, 0.f, 1.f);
```

They differ only in how they say "unknown" — the selector an empty `TOptional`, the
policy `-1.f` — and the policy names that equivalence in its own comment. Nothing to fix.

**Not claimed:** nothing here is a live claim. Honesty ladder rung: COMPILES + HEADLESS
SPECS. No PIE, no multiplayer, no packaged build. This ticket asks for nothing more.
