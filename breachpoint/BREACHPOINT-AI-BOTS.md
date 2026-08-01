# BREACHPOINT — Bot AI Architecture: Goals over a StateTree Spine

**Doctrine in one line:** a GOAP-style *goal layer* decides **what a bot wants**; a StateTree
*execution spine* decides **how it acts on that**; GAS is the **only hand it acts with**.
One brain, three layers, every layer deterministic and data-driven.

This document is the researched design behind ticket BP08 and the `ai-builder` agent.
Binding rulings: R8–R12 in `docs/DESIGN-RULINGS.md`.

---

## 1. The research: how Halo actually does it

Bungie's AI is the most-documented combat AI lineage in the industry (Damian Isla's GDC
material on the Halo 2 AI and its descendants), and four of its lessons are load-bearing
for us:

**1. Behavior trees for *acting*, not for *wanting*.** Halo 2 popularized the behavior
tree — but the BT was the *execution* layer: prioritized, hierarchical action selection
(engage, seek cover, flee, search). The *desire* side (which territory to fight for, when
to break off) came from separate systems fed by designer-authored spatial data. The lesson:
**don't make one structure do both jobs.** A tree that encodes goals AND actions becomes
the unmaintainable mega-tree everyone regrets.

**2. Space is authored, not inferred.** Halo's combat reads as smart because the AI fights
*about places*: firing points, cover points, territories — all authored data the AI
evaluates at runtime. That is exactly UE's EQS model, and it is why our
`arena_manifest.json` (landmarks, cover volumes, spawn scoring) is not just level-design
data — **it is the bots' spatial vocabulary.** The Rocket pad landmark is a thing a bot can
*want*.

**3. Legibility beats optimality.** Halo's most famous AI feature is that players can
*read* it: Grunts flee when the Elite dies; Elites break off when shields crack; enemies
react visibly to the player's power plays. The intelligence is an *illusion built from
readable state changes tied to observable events*. Our version is ruling R12: every tier
expresses a fantasy (Recruit over-commits, Veteran times the rocket), and break-off on
shield-crack is the same signature move players learn from Halo. A bot that wins more but
reads as random fails review.

**4. Fights are directed, not emergent-only.** Halo's encounter designers place goals;
the AI improvises the tactics. Our equivalent at the match scale: the *goal layer* holds
the strategic wants (control the rocket timer, hold the tower), and improvisation lives
below it.

**Where we take GOAP** (Orkin's F.E.A.R. planner — actions with preconditions/effects,
searched into a plan): full A* planning over a large action space is overkill for a 4v4
arena bot and hostile to determinism budgets. We take GOAP's *shape* — goals scored by
utility, actions with preconditions/effects, short plans — and bound it hard (§3). This is
the honest industry pattern: modern shooters run utility-scored goals over BT/StateTree
execution far more often than they run full planners.

---

## 2. The three layers

```
┌──────────────────────────────────────────────────────────────────────────┐
│ LAYER 1 — AMBITIONS (GOAP-style goal layer)         UBRBotBrain (pure C++)│
│ WHAT DO I WANT?  Utility-scored goals from DT_BotAmbitions × perception   │
│ facts × DT_BotTuning personality weights. Rescored ON EVENT, never per    │
│ tick. Output: the active Ambition + a plan of ≤ 3 Steps.                  │
│   Ambitions: ControlRocket · WinThisFight · Survive · TakeGround ·        │
│              HuntWeapon · (Warmup) Roam                                   │
├──────────────────────────────────────────────────────────────────────────┤
│ LAYER 2 — EXECUTION (StateTree spine)               ST_Bot + BRStateTree* │
│ HOW DO I DO IT?  One StateTree asset executes the plan's current Step.    │
│ States are Halo-legible stances: Seek / Engage / Flush / Reposition /     │
│ Retreat / ContestRocket. BT-shaped selector logic lives INSIDE states     │
│ as StateTree tasks — no separate BehaviorTree asset (ruling R9).          │
│ EQS answers every "where": firing point, cover, grapple perch.            │
├──────────────────────────────────────────────────────────────────────────┤
│ LAYER 3 — THE HAND (GAS, the human path)            existing ability set  │
│ Acting = pressing the same buttons a human does: InputTag activation on   │
│ its PlayerState ASC, same abilities, same costs, same cooldowns. The     │
│ brain's Step preconditions are ASC queries (CanActivate, tag present,     │
│ cooldown clear) — GOAP preconditions and GAS state are THE SAME FACTS.    │
└──────────────────────────────────────────────────────────────────────────┘
```

**Why this is GOAP + GAS and not GOAP *beside* GAS:** a classic GOAP action is
`{preconditions, effects, cost}`. In Breachpoint every combat action already *is* a GAS
ability whose activation requirements (tags, costs, cooldowns) ARE machine-readable
preconditions, and whose applied effects/tags ARE its effects. The planner doesn't need a
parallel world-model — **the ASC is the world-model.** `ThrowGrenade` is plannable because
the ASC can answer "can I?" (cooldown tag clear, grenade count > 0) and "what changes?"
(`State.Combat.GrenadeOut`, enemy `State.Shields.Broken` likely). That fusion is the novel
part of this design, and it is what keeps the iron rule intact: the brain can only want
things the hand can legally do.

## 3. Layer 1 — Ambitions, precisely

- **Utility scoring, table-driven.** Each row in `DT_BotAmbitions.csv`:
  `ambition, base_utility, considerations` — considerations are named, coded evaluators
  (`rocket_timer_norm`, `my_shield_norm`, `enemy_shield_norm`, `teammates_alive_norm`,
  `dist_to_rocket_norm`…) combined multiplicatively, then shaped by per-tier personality
  weights from `DT_BotTuning` (`rocket_contest`, `push_threshold`, `cover_preference`).
  All numbers in data; zero literals in C++ (data-is-not-code law).
- **Event-driven rescoring only**: perception events (seen/heard/damaged), GAS tag events
  (own `Shields.Broken`, target's `Shields.Broken`, `Event.Kill` nearby), match beats
  (rocket-spawn T−10 s, score change), plan completion/contradiction. Between events the
  ambition persists — **hysteresis + a quantized commit window** (seeded) prevent
  goal-thrash, which is both a perf property and the *legibility* property (a bot that
  visibly commits reads as intentional — Halo lesson 3).
- **Plans are ≤ 3 Steps** (ruling R10), assembled by bounded lookup — per ambition a small
  authored set of Step chains with preconditions, not free A* search. Example,
  `ControlRocket` @ T−10 s: `[MoveTo(EQS: pad approach w/ cover), Hold(sightline), 
  PickupOrContest]`. A Step whose precondition dies (pad taken, shields cracked) →
  **replan on that event**. A surviving contradiction is a bug, not adaptation.
- **`UBRBotBrain` is a plain UObject, world-free, headless-testable** — sim-builder purity
  rules apply to it: inputs are (fact struct, tuning row, ambitions table, seeded stream);
  output is (ambition, plan). `Breachpoint.Bots.Brain` pins exact decisions per seed.

## 4. Layer 2 — the spine, precisely

- **One `ST_Bot` StateTree asset for all tiers, forever** (BP08's "three difficulties from
  ONE StateTree via scalars" holds). The active Ambition+Step enter as StateTree
  parameters/conditions; states select on them.
- **Where "behavior tree" lives** (the user-directed both-worlds answer): StateTree is
  UE 5.8's production successor to the BT+AIController stack, and its tasks/conditions
  compose the same prioritized-selector patterns Halo's BTs pioneered. We use BT *patterns*
  inside StateTree states — e.g. Engage's task list is a priority selector
  (rocket-if-held → grenade-if-shields-cracked → fire-primary → melee-in-range) — and do
  NOT run a second BehaviorTree asset in parallel. One brain (R9). If StateTree hits a
  genuine wall, the fallback is a contract_gap to the lead, not a quiet second brain.
- **EQS answers every spatial question** (Halo lesson 2): firing points that respect the
  35 m sightline data, cover scored by `height_class` from the manifest, grapple perches
  from `landmarks[]`. Bots query the *authored* arena vocabulary — never raw nav-mesh
  divination — which is also why bot callouts ("contesting the Pad") come free.
- **Perception is the server's gameplay events** (no per-tick raycast sweeps, no hidden
  state); reaction latency = tier's `reaction_ms` (≥ 200, R11), quantized and seeded.

## 5. Determinism & honesty (unchanged law, now three-layer)

Same seed + same tuning row + same observed-event trace ⇒ **identical ambition trace,
identical plan trace, identical action trace.** `Breachpoint.Bots.*` pins all three (brain
headless; spine via the ladder's functional rung; soak seeds logged). No wall-clock, no
`FMath::RandRange`, no LLM anywhere in the loop (R8) — adaptive difficulty arrives between
matches as *data* through the tuning-curator, never mid-match as inference.

## 6. What we deliberately do NOT build

- **No full GOAP A* planner** — bounded chains beat open search for a 4v4 arena; revisit
  only if Phase-2 modes (Firefight waves) demand it, via founder decision.
- **No separate BehaviorTree asset**, no MassAI, no Learning Agents, no per-tier trees.
- **No blackboard soup**: facts live in one typed struct the brain consumes; StateTree
  parameters carry only the active ambition/step.

## 7. File map (BP08's owner_path, `Source/Breachpoint/AI/`)

| Unit | Layer | Notes |
|---|---|---|
| `BRBotBrain.h/.cpp` | 1 | pure, headless; ambitions + bounded planner |
| `BRBotFacts.h` | 1 | the typed fact struct (perception + ASC-derived) |
| `ABRBotController.h/.cpp` | glue | events → brain; plan → StateTree params; InputTag activation |
| `BRStateTreeTasks.h/.cpp` | 2 | stance tasks incl. selector-shaped Engage internals |
| `BREnvQueryContexts.h/.cpp` | 2 | cover/threat/rocket/perch scoring from manifest data |
| `BRBotManagerComponent.h/.cpp` | match | slot-fill, 10 s backfill, tier scalar application |
| `Content/Data/DT_BotTuning.csv` | data | tuning-curator's (existing schema + ambition weights) |
| `Content/Data/DT_BotAmbitions.csv` | data | NEW — ambitions, base utilities, consideration sets |
| `Content/AI/ST_Bot.uasset`, EQS assets | 2 | one owner per binary (law 7) |

*Delta from the pre-GOAP plan: + `BRBotBrain` + `BRBotFacts` + one CSV. Everything else was
already in BP08 — the goal layer is an addition of two testable files, not a rebuild.*
