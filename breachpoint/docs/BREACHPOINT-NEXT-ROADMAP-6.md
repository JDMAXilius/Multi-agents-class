# BREACHPOINT NEXT — Roadmap 6: The Brain (GOAP ambitions, weighted, data-driven)

**Cut:** 19 August 2026 by the cloud lead · **Crew:** `bn-builder`, `bn-critic`, `bn-editor`
**Doctrine:** `BREACHPOINT-AI-BOTS.md` Layer 1 is the destination. This roadmap is its honest
minimum for the game that EXISTS: free-for-all, kill each other, points of interest. Ambitions
that reference systems BN does not have (rocket, teammates, weapon pickups) are named slots.

## The one-line goal

**Bots WANT things: a data table of objectives with weights decides, on events and never per
tick, whether a bot fights, flees, or roams — and the founder tunes that with numbers, not code.**

## Founder rulings that bound this roadmap

- **Objectives and their weights are DATA** (`DT_BNBotAmbitions`): base utility per ambition ×
  named considerations × weights, all in the table. Zero tuning literals in C++ (law 3).
- **The brain sits ABOVE the tree and parameterizes it — it never replaces it.** R5's StateTree
  keeps executing; the brain decides what the tree is FOR right now.
- **Less is more:** ONE new class (`UBNBotBrain`). The facts struct and the enum live in its
  header. A second new class is a finding.
- **Event-driven, never per-tick** (law 4): rescore on perception change, own-damage, own-health
  thresholds, ambition completion — with a commit window so bots visibly COMMIT (hysteresis is a
  legibility feature, not just a perf one — the doctrine's Halo lesson).

## The three ambitions of the shipped game

| Ambition | It means | Tree manifestation |
|---|---|---|
| `Fight` | close and kill the current target | Engage runs as R5 built it |
| `Survive` | break contact, get away from the threat | the controller REPORTS NO TARGET while surviving (Engage exits by its own condition) and Roam's pick flips to farthest-from-threat |
| `Roam` | nothing better to want | Roam as R5 built it (nearest-not-last) |

Slots named, not built: `ControlRocket`, `HuntWeapon`, `WinThisFight`-as-distinct-from-Fight,
teammate considerations. Each lands as a table row + an evaluator when its SYSTEM exists.

---

## G1 — `AI/BNBotBrain.{h,cpp}` — the one new class

| # | Task |
|---|---|
| 1.1 | `EBNBotAmbition {Fight, Survive, Roam}` + `FBNBotFacts { bool bHasTarget; float HealthNorm; float DistToTargetNorm; }` in the brain's header. Facts are a value struct the CONTROLLER fills — the brain is a plain `UObject`, world-free, headless-testable: `Score(Facts)` never touches an actor, a world, or a clock. |
| 1.2 | Scoring: per ambition `Utility = BaseUtility × Σ(consideration × weight)` from its table row (shape: `FBNBotAmbitionRow { float BaseUtility; float HealthWeight; float TargetWeight; float DistanceWeight; float CommitSeconds; }` added to `Data/BNDataRows.h`). Considerations are CODED evaluators (health low → Survive rises; target present → Fight rises; no target → Roam wins by default) — the WEIGHTS are the founder's. |
| 1.3 | Hysteresis: `CommitSeconds` per row — a chosen ambition holds until its window passes OR a superseding event class fires (own health crossing the Survive threshold interrupts anything; the founder's bots must visibly commit, not oscillate). Window time is passed IN by the caller (world seconds) — the brain never reads a clock (headless rule). |
| 1.4 | Table access through `UBNGameData` like the weapon rows: `FindBotAmbitionRow(EBNBotAmbition)`; table path soft, in ini; **missing table = C++ default row values and ONE warning** — bots must fight even before the editor ticket lands. |

## G2 — the controller feeds and obeys the brain (EDITS only)

| # | Task |
|---|---|
| 2.1 | `BNBotController` owns a `UPROPERTY() TObjectPtr<UBNBotBrain>` (NewObject in OnPossess). Rescore call sites — the EVENTS, no tick: target gained (perception), target lost/died, own health attribute change (threshold crossings only — delegate on the ASC's Health attribute, compare against the row-driven threshold), own-damage (the existing `State.Combat.RecentDamage` tag event), StateTree state completion is NOT a brain event this wave. |
| 2.2 | The Survive obedience: `GetCurrentTarget()` returns null while the ambition is `Survive` (Engage exits via R5's own HasTarget condition — the tree is not edited for this). The underlying `TargetEnemy` is KEPT (it is the threat to flee from): expose `GetThreat()` returning it regardless of ambition. |
| 2.3 | One log line per ambition CHANGE (never per rescore): `BNBrain: <bot> wants <ambition> (u=%.2f) because <top consideration>` — the founder must be able to read a bot's mind from the log (§5c). |
| 2.4 | `AI/BNBotStateTreeTasks.cpp` — `FBNMoveToPointOfInterestTask` only: when the controller's ambition is `Survive` AND a threat exists, pick the point FARTHEST from the threat instead of nearest-not-last. No other task changes. |

## G3 — the numbers (data + config)

| # | Task |
|---|---|
| 3.1 | `Data/BNDataRows.h`: `FBNBotAmbitionRow` as in 1.2, with C++ defaults that make the shipped game sensible: Fight Base 1.0 / TargetWeight 1.0; Survive Base 1.2 / HealthWeight 1.0 (rises as health falls, wins below ~35%); Roam Base 0.2; CommitSeconds 3.0 / 5.0 / 2.0. Defaults ARE the fallback row (task 1.4) — the table overrides, never duplicates. |
| 3.2 | ini: `[/Script/BreachpointNext.BNGameData] BotAmbitionsTable=/Game/BN/Data/DT_BNBotAmbitions` (soft path; the DT is the editor ticket). |

## Waves

| Wave | Goals | Agent | Then |
|---|---|---|---|
| 1 (only) | G1 + G2 + G3 | `bn-builder` | `bn-critic` on the diff |
| — | `TASK-R6-DT-AMBITIONS` (create the DT, three rows mirroring the C++ defaults) | `bn-editor` | any time after the build |

One wave: the brain, its feeding, and its numbers are one coherent diff; splitting them would
review a brain nothing calls.

## Status — 19 Aug 2026

Single wave **LANDED** `5f0b360`, critic **PASS** with one note — the Survive interrupt was a
utility ratio that real weights could never satisfy (a bot at 5% health stood firing through its
whole commit). Fixed in the follow-up commit as the roadmap's own sentence made data:
`InterruptBelowHealthNorm` on the Survive row, shipped 0.35. Editor ticket
`TASK-R6-DT-AMBITIONS` OPEN.

**Compile-risk sweep (R5+R6), 19 Aug:** clean — unlike R4's, which caught a real error. Every
StateTree node signature was diffed against Epic's own 5.8-compiled Variant_Shooter reference
(exact match, `bShouldCallTick` confirmed a real base member, any Tick drift would fail loudly on
`override`); all cross-file includes present; the ambitions soft table resolves in
`UBNGameData::Initialize` on the weapon table's exact shape; no unused variables after the
interrupt fix. **Not compiled** — the founder's build is the first real test.

## Deferred beyond R6, deliberately

Plans of ≤3 Steps (R10 — today each ambition is one continuous behavior, so a plan is overhead);
EQS anchors; `DT_BotTuning` personalities/tiers; rocket/teammate/pickup ambitions (each waits on
its system); the determinism harness (R8) — the brain is already clock-free and stream-free,
which is the property the harness will test.
