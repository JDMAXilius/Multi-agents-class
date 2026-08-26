# TICKET — AIB3: the four skill policies compile and their ladders prove out

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

- [ ] Rung 1 PASS (Editor + Game; Server recorded environmental)
- [ ] Rung 2: 77/77/0, reconciled, per-suite split pasted
- [ ] Four mechanical checks pasted, empty
- [ ] Deviations recorded

## Log

_(terminal: outputs verbatim)_
