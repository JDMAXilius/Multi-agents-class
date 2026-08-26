# TICKET — AIB4: confidence wired into ambitions, and the damage seam goes live

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

- [ ] Rung 1 PASS (Editor + Game; Server recorded environmental)
- [ ] Rung 2: 91/91/0, reconciled, per-suite split pasted
- [ ] Both live switch directions pasted with scores
- [ ] Four mechanical checks pasted, empty
- [ ] Deviations recorded

## Log

_(terminal: outputs verbatim)_
