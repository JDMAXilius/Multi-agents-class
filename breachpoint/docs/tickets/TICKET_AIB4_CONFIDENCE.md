# TICKET — AIB4: confidence wired into ambitions, and the damage seam goes live

> STATUS: in-progress — mac terminal 26 Aug 2026 (f277b53). Steps 1/2/4 DONE. Step 3 (live
> BotSystem=AIB PIE) OUTSTANDING — the switch is a founder call, still BotSystem=BN.

> STATUS: open — cut 26 Aug 2026 by the cloud lead. Phase 5 is landed **WRITTEN, NOT
> COMPILED** (serial build, one writer, per the wave map). Needs the ENGINE ON DISK for
> steps 1–2; step 3 wants a PIE match with `BotSystem=AIB` (can ride along any AIB2
> protocol session).

Phase 5 of `docs/AIBOT-ROADMAP.md`: the fifth skill of the combat dance. What landed:

- `Brain/AIBConfidenceModel.h/.cpp` — the DAMAGE LEDGER (momentum as half-life-decayed
  taken/dealt accumulators, fractions of max health, O(1) lazy decay) and the
  CONFIDENCE MODEL (a 0..1 read of the fight from facts a human also has — own health,
  momentum, weapon fitness, visible enemy count, NEVER enemy vitals — passed through
  the level's HELD misjudge: drawn on a cadence, held between draws, because consistent
  wrongness reads as a bad call and per-tick noise reads as a broken needle). Novice
  misjudges by ±0.30 and holds it 2s; Expert ±0.04 at 0.7s. Internals are OURS —
  provenance flagged per the roadmap.
- `Core/AIBTypes.h` — facts gain `bConfidenceKnown`/`ConfidenceNorm` (the one COMPUTED
  fact, flagged like every unknowable) and the damage-history comment names its source.
- `Brain/AIBConsideration.h/.cpp` — `ConfidenceNorm` selector, gated on the known flag.
- `Brain/AIBAmbitionEngine.cpp` — Engage gains a NERVE consideration (0→0.55, 1→1.0:
  confidence scales the appetite, never vetoes a visible enemy) and Retreat its mirror
  (0→1.0, 1→0.45: a winning bot presses through the wounds a losing one flees). BOTH
  answer 1.0 on unknown, so every pre-Phase-5 spec pin holds on a host without the
  seam — pinned by a spec.
- `Core/AIBBotController.h/.cpp` — the ledger, judge state, and Phase 4's
  `FAIBSkillProfile` (its first consumer: Confidence level = judgment quality) resolve
  at possession; a SEPARATE per-bot `FRandomStream` for misjudge draws (a redraw must
  not shift reaction latencies); `NoteDamageTaken` (attacker → the sensorium's damage
  stimulus: matures into MEMORY, never a lock — the host's own ruling kept 1:1) and
  `NoteDamageDealt`; Think fills damage facts + steps the model between Build and
  Rescore. `bDamageSeamSeen`: history stays an honest UNKNOWN until the host's seam has
  ever spoken — never "confidently untouched" while being shot.
- `Characters/BNHealthComponent.cpp` (game side, the blast-branch precedent) — ONE
  seam: on any Health or Shield pool decrease (authority only), the victim's controller
  learns TAKEN (with `GetLastDamage()`'s instigator) and an AIB attacker learns DEALT.
  Both pools fraction over the victim's MaxHealth; a shield+health hit SUMS across the
  two handlers. BN behaviour untouched.
- `Tests/AIBConfidenceSpec.cpp` — 14 specs (`AIBot.Sim.Confidence`): ledger decay/
  accumulate/garbage/reset, assessment directions + unknown-honesty + momentum
  symmetry, held misjudge (no draw consumed inside a hold), facts move inside a hold,
  Novice-vs-Expert wrongness spread, ladder monotone, seed determinism, and the
  roadmap's named proof as arithmetic: ONE wounded mid-fight fact row flips
  Engage↔Retreat on confidence alone.

## Kickoff (machine-checkable)

- requires: engine-installed
- owner_path: `docs/tickets/TICKET_AIB4_CONFIDENCE.md`
  <!-- Log only; compile-error protocol as AIB1–AIB3. -->

## Steps (in order)

1. **Rung 1** — `./Tools/run-ubt.sh BreachpointEditor Breachpoint`.
2. **Rung 2** — `./Tools/run-specs.sh AIBot`: **91 expected, 0 failures, reconciled** —
   Scaffold 5 + Sensorium 20 + AmbitionEngine 18 + Movement 8 + Aim 7 + Grenade 11 +
   Melee 8 + Confidence 14. (Supersedes AIB3's 77 the way AIB3 superseded AIB2's 43.)
3. **The live proof** (any `BotSystem=AIB` PIE with damage flying): grep LogAIBot for
   ambition switches and confirm BOTH directions appear in one match — a bot pressing
   (Retreat → Engage while hurt, after landing hits) and a bot breaking off (Engage →
   Retreat under un-answered fire). Paste one excerpt of each with the scores.
4. Four mechanical checks, pasted empty.

## Watch-list — written-not-compiled spots flagged for honest scrutiny

- `FRandomStream::GetCurrentSeed()` again (the no-draw pin) — same risk and same
  fallback as AIB3's aim spec; one ruling covers both files.
- `FMath::Sqrt(-1.f)` as the spec's NaN source — if the compiler folds it, swap for a
  quiet-NaN constant and say so here.
- `static constexpr float HalfLifeSeconds` on the ledger struct with AIBOT_API — an
  in-class constexpr needs no out-of-line definition in C++17; if the linker disagrees,
  that is the fix, one line.
- The health-component seam: `Attributes->GetMaxHealth()` (ATTRIBUTE_ACCESSORS-
  generated) and `IsOwnerActorAuthoritative()` — both transcribed from compiled in-repo
  use, re-typed here.
- Double-fire risk on the seam: verify in step 3's match that one plain hit produces
  ONE ledger note (not a shield echo) — the handlers split by pool, but the claim is
  live-proof class.

## Done when

- [x] Rung 1 PASS (Editor + Game; Server recorded environmental)
- [x] Rung 2: 91/91/0, reconciled, per-suite split pasted
- [ ] Both live switch directions pasted with scores
- [x] Four mechanical checks pasted, empty
- [x] Deviations recorded (watch-list resolved 4/4 in the good direction)

## Log

_(terminal: outputs verbatim)_

### 2026-08-26 — headless proof, mac terminal (f277b53)

Steps 1, 2 and 4 are DONE; step 3 (the live `BotSystem=AIB` PIE) is the only thing
outstanding and is deliberately left for a session that flips the switch.

**Step 1 — Rung 1: PASS** (Editor + Game; Server environmental — full quote in AIB3's
Log, same run). Phase 5 compiled clean on first contact, and with it **every watch-list
item resolved in the good direction**:

- `FRandomStream::GetCurrentSeed()` — compiles; the no-draw pin holds. One ruling for
  both AIB3 and AIB4, as the ticket asks: the fallback is not needed.
- `FMath::Sqrt(-1.f)` as the NaN source — NOT folded by the compiler; the ledger's
  garbage-rejection spec passes on it. No swap to a quiet-NaN constant needed.
- `static constexpr float HalfLifeSeconds` with `AIBOT_API` — the linker did NOT
  disagree. No out-of-line definition needed (C++17 in-class constexpr, as predicted).
- The health-component seam's re-typed calls — `GetMaxHealth()` (ATTRIBUTE_ACCESSORS-
  generated) and `IsOwnerActorAuthoritative()` — both compile.

**Step 2 — Rung 2: 91/91/0, reconciled, split pasted.** The prediction was exact: all
eight suites hit their predicted count, Confidence included at 14. Table and three-way
reconciliation in AIB3's Log (one run covers both tickets).

**Step 4 — the four mechanical checks: ALL FOUR EMPTY, ALL PASS.** Table in AIB3's Log.
Worth stating for THIS ticket specifically: check 1 (boundary) is empty **even though
Phase 5 opened a BN→AIB call path**. That is the intended direction. `BreachpointNext.
Build.cs` declares `"AIBot"` as a public dependency with the comment "the game depends
on the module, never the reverse", and `BNHealthComponent.cpp` is now the fifth BN file
to include an AIB header (after the adapter, `BNGameMode`, `BNProjectile`,
`BNCharacter`). The module stays ignorant of the game; the game knows the module.

**Step 3 — NOT DONE, and not silently skipped.** It needs a PIE match with
`BotSystem=AIB`. `Config/DefaultGame.ini:282` still reads `BotSystem=BN`, restored
deliberately at the end of AIB2's verb work. Flipping it is a founder call that has been
open since then, so this step waits rather than my taking it. What it must produce when
run, verbatim from this ticket:

1. A bot pressing: Retreat → Engage while hurt, after landing hits.
2. A bot breaking off: Engage → Retreat under un-answered fire.
3. The double-fire check: ONE plain hit produces ONE ledger note, not a shield echo.

On (3), the static reading is that the two handlers cannot double-count — they are keyed
to different pools and each notifies only its own drop, so a shield+health hit SUMS to
the hit rather than duplicating it. But that is a code-read, and the ticket correctly
files the claim as live-proof class. It stays UNPROVEN here.

**Honesty ladder rung: COMPILES + HEADLESS SPECS.** Phase 5's behaviour — that
confidence actually moves ambitions in a live match — is asserted only in arithmetic
(the Confidence suite's flip proof), never yet observed in a running game.
