# TICKET — AIB13: Phase 8 proof — four tiers observably distinct

> STATUS: open — cut 26 Aug 2026 by the cloud lead with the Phase-8 build ("WRITTEN,
> NOT COMPILED"). TERMINAL WORK: compile, specs, one A/B/C/D PIE observation, overlay
> eyeball.

## What landed (cloud)

- `Data/AIBTiers.h/.cpp` — NEW: the C++ tier registry (Recruit/Marine/ODST/Spartan as
  competence VECTORS — capability gating, not stat inflation; Marine IS the defaults
  row, restated nowhere). Sight radii are IDENTICAL across tiers on purpose (the
  engage/grenade band anchors live at 1500); tiers vary reaction draw, peripheral
  cone, memory window, and the six skill levels. Teamwork: Recruit/Marine below the
  claims gate, ODST Trained, Spartan Skilled. `ValidateRow` warns on sub-floor
  reaction (F1 clamps it silently otherwise), inverted draws, memory past the
  ceiling, and lose-sight under the band anchor.
- Controller — `BotTier` (Config, default Marine) + `SetTierName` (host door for mixed
  lobbies, takes effect next possession); OnPossess resolves from the registry
  (unknown name → defaults row, loud), re-applies the perception envelope via
  ConfigureSense (the constructor's own promise), validates, and logs the grep-able
  tier line: `resolved tier <name> (Mv..Tw, react a-b)`. BOTH "Phase 8 resolves the
  real tier" function-local-static markers are gone: the blast gate and the facts
  builder's memory window now read the RESOLVED row (the registered debt, closed).
- `Debug/AIBGameplayDebugger.h/.cpp` — the stub became the overlay: per-think
  DrawDebugString over each bot's head (tier + skill vector, the full ambition
  scoreboard with incumbent marked, confidence or its honest unknown, pending-stimuli
  depth), behind Config `bDebugOverlay`. DELIBERATELY not an FGameplayDebuggerCategory
  (no compiled call to transcribe from — recorded in the header as the seam to grow).
- `AIBTreeAuthoring::BuildTierTable` — DT_AIBTiers now mirrors the registry's four
  rows + Default (inspection surface, never authority; row warnings ride the report).
- `Data/AIBTuningData.h` — the bundling-asset contract DEFERRED by decision (no
  consumer; an asset nothing reads is the inert defect class). Revisit at Phase 10.
- `Tests/AIBTiersSpec.cpp` — NEW, 6 specs: resolution + unknown-name refusal; ladder
  monotone AND distinct per rung; the Teamwork gate split; shipped rows validate
  clean; the validator fires on a doctored row (5 named defects); envelope anchors.
  **Module spec total: 114.**

HUMANISATION NOTE (the phase's third word): the humanisation half landed EARLY,
through the P4+5 review barrier — residual aim floor, per-life hashed seeds, the
re-acquire gap rule, the burst muzzle gate, per-rung strafe legs. Phase 8 adds no new
humanisation surface; it makes the existing ladders SELECTABLE. Recorded so nobody
hunts for a missing deliverable.

## Steps (terminal)

1. Rung 1; specs — expect **114/114** (Tiers 6 new; history: 108 at AIB12).
2. Asset rebuild (the settings button): DT_AIBTiers report shows **5 rows** and zero
   row warnings; ST_AIBBot rebuild is ALREADY owed by AIB11/AIB12 (rename!).
3. Tier A/B: two PIE matches, ini `[/Script/AIBot.AIBBotController] BotTier=Recruit`
   then `BotTier=Spartan`. Countables from the log:
   - every possession logs `resolved tier <name>` with the right vector;
   - mean acquisition latency (the `acquired ... after N.NNNs reaction` lines) is
     visibly higher for Recruit (draw 0.34-0.60 vs 0.20-0.28);
   - Spartan matches show `claim GRANTED` ZERO times still (FFA — nothing claimable),
     but Teamwork now reads Skilled in the tier line (the gate is live, the world
     offers nothing to claim — AIB12's inertness result must NOT change).
4. Overlay eyeball: `bDebugOverlay=True`, one PIE minute — scoreboard, confidence and
   tier line render over heads and update at think cadence. WATCH-LIST API:
   `DrawDebugString` (transcribed shape, unproven in this repo — DrawDebugHelpers.h
   include is proven, this exact function is not; if it fails, paste the error).
5. Optional mixed lobby: call `SetTierName` from a scratch branch or set two ini
   sections via child classes — DEFERRED unless cheap; the door exists, no caller yet.

## Done when

- [ ] Rung 1 + 114/114
- [ ] DT_AIBTiers 5 rows, zero warnings
- [ ] Recruit-vs-Spartan latency delta pasted (the countable distinctness)
- [ ] Overlay screenshot or "renders and breathes" line
- [ ] Watch-list APIs confirmed or errors pasted

## Log

_(terminal: outputs verbatim)_
