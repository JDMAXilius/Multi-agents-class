# AIBOT — the clean-room bot framework (Halo Infinite's architecture, 1:1)

> Founder directive, 25 Aug 2026: rework the bot AI **from scratch, from zero** — a
> self-contained module named `AIBot`, built as if porting Halo Infinite's multiplayer bots
> into Unreal, plugin-shaped for later extraction. BreachpointNext is the **test harness and
> a minor reference only** — nothing is built on top of its bot code, and the module never
> depends on it. The reference spec is `docs/BREACHPOINT-NEXT-RESEARCH-HALO-BOTS` (the two
> GDC 2022 talks + press record; also the published artifact "Halo Infinite Bot Anatomy").

## Prime decisions (everything follows from these five)

1. **Module boundary is law.** `Source/AIBot/` — runtime module `AIBot`, prefix `AIB`, log
   `LogAIBot`, macro `AIBOT_API`. Zero includes of BreachpointNext or any game module, ever.
   Dependency arrow: game → AIBot, never back. Mechanically checkable with one grep.
2. **The bot speaks only through interfaces it owns.** `IAIBAvatarInterface` (verbs:
   `PressVerb`/`ReleaseVerb`/`SetMoveInput`/`SetLookTarget` + read-backs), `IAIBWorldQuery`,
   `IAIBAmbitionProvider`. The game implements them in ONE adapter folder
   (`Source/BreachpointNext/AIBotAdapter/`). GAS purity by construction: the module cannot
   name an ASC, an ability, or an attribute — the adapter presses the same `Input.*` tags a
   human's keyboard presses.
3. **Server-only brain.** Nothing in the module replicates. Bots drive pawns through the
   player input path, so at the netcode layer a bot IS a player.
4. **StateTree executes; Behavior Tree is a port slot.** Execution behind `IAIBExecutor`;
   `UAIBStateTreeExecutor` first (C++ authoring via `UAIBTreeAuthoring`, the proven path);
   a BT executor can be added without touching the brain.
5. **Data-driven, C++ defaults as truth.** Curves/weights/tiers in DataTables & DataAssets,
   soft refs, mirrored FROM C++ defaults (the 13-places lesson: one direction of flow).

## The subsystems (Halo's stack, mapped)

| # | Subsystem | Halo feature it implements |
|---|---|---|
| ① | `UAIBSensorium` + reaction clock + target memory | fair perception; latency floor (200 ms class, a constant not a knob); grenades on the same clock |
| ② | `FAIBFacts` | considerations: own health, opponent health, distance, ammo, height difference, mode state, ally proximity, momentum |
| ③ | `UAIBAmbitionEngine` | Utility AI: ambitions = tag + considerations (fact selector × response curve × weight); highest wins; commit windows + hysteresis + hard interrupts (our design — 343 never published theirs) |
| ④ | `IAIBExecutor` → StateTree | behaviour-tree execution; stimulus behaviours = event-driven transitions |
| ⑤ | five skill policies (Movement/Aim/Grenade/Melee) + `UAIBConfidenceModel` | the combat dance; each skill a competence ladder novice→expert; confidence feeds back into ambition scores (internals ours, flagged) |
| ⑥ | `DT_AIBTiers` | Recruit/Marine/ODST/Spartan as **vectors of skill levels** — capability gating, not stat inflation |
| ⑦ | `UAIBTeamCoordinator` | claims board — fixes the pack-on-rocket failure per-agent utility cannot express; tier-gated |

Navigation is reused, not rebuilt: navmesh + generated jump links (BN_Drop/BN_Climb) are
project/engine-level; the module paths and presses `AIBot.Verb.Jump`.

## Phases

| Phase | Deliverable | Proof |
|---|---|---|
| 0 | scaffold (full tree), interfaces, ARCHITECTURE.md + FAIRPLAY.md, A/B switch | compiles all targets; boundary grep empty |
| 1 | Sensorium + reaction clock + memory | `AIBot.Sim.Sensorium` |
| 2 | Facts + Ambition Engine + curves + hysteresis | headless specs |
| 3 | Executor + verbs + BN adapter + `ST_AIBBot` | terminal ticket; bot roams & shoots in PIE |
| 4 | Skills 1–4 + competence ladders | per-level specs + measured PIE samples |
| 5 | Confidence model wired into ambitions | specs + observed disengage/press |
| 6 | Game Mode Ambitions + one objective mode | bot plays the objective |
| 7 | Team coordinator (claims) | `AIBot.Sim.Claims` headless + FFA-inert live check; the two-allied-bots-one-pickup measurement is DEFERRED until a host mode has teams and ≥2 claimable slots (P7 packet, 26 Aug) — the promise is scoped to Teamwork-competent bots |
| 8 | Tiers data, humanisation, gameplay debugger | four tiers observably distinct |
| 9 | 4v4 matches, metrics harness, tuning | telemetry vs quality bars |
| 10 | extract to `Plugins/AIBot/` | plugin compiles in a blank project |

Full tree stands (founder ruling): all folders/files planned from day one; each phase fills
in real implementations. Phases 1–2 and 5 are pure headless C++ (worldless by design).

## The five proofs

1. **Headless specs** `AIBot.Sim.*` — scoring, hysteresis, interrupts, confidence table,
   latency floor, claims. Runs on the mac in seconds, no editor.
2. **The A/B switch** — `BNGameMode` ini: `BotSystem=AIB|BN`. Both systems coexist; the
   acceptance test is a mixed match, BN bots vs AIB bots, BR_Arena01. Done = beats the old
   system while reading as more human.
3. **Instrumented PIE** — grep-able `LogAIBot` one-liners (ambition switches with scores,
   confidence transitions, claim grants) + gameplay debugger overlay. Countable events,
   never impressions.
4. **Per-phase eyes-on protocol** — scripted PIE observation per phase, cut as terminal
   tickets with expected log lines.
5. **The honesty rungs, unchanged** — written → compiled → specs → PIE → listen+client.
   Cloud writes and audits; it cannot compile (no engine in the container). The terminal
   proves every rung.

## BreachpointNext footprint — THE SEAM LEDGER (amended 26 Aug 2026)

The original claim ("~3 adapter files; growth past that folder is a finding") went false
the day the game had to TELL the module things — a warning seam is game code by nature.
Kept honest the way the GAS contract keeps its exceptions: a NAMED ledger. Every game
file that names an AIBot type is listed here with its one purpose; a seam not on this
ledger is the finding — nothing joins inline.

1. `AIBotAdapter/` (bn-builder's folder) — the avatar door and, at Phase 6, the world
   query subsystem. The bulk of the footprint lives here, still.
2. `Characters/BNCharacter.cpp` — one `EnsureOn` line at possession (the adapter attach).
3. `Weapons/BNProjectile.cpp` — the blast warning seam (`NoteIncomingBlast`).
4. `Characters/BNHealthComponent.cpp` — the damage seam (`NoteDamageTaken/Dealt`,
   Phase 5): one per-hit site for every victim, which no adapter component can see.
5. `Match/BNGameMode` — the `BotSystem` A/B switch + `AIBBotControllerClass`, and (Phase
   6, planned) the provider handoff line in `SpawnBot`.
6. `Breachpoint.uproject` + Build.cs dependency + the two ini sections.

Damage pipeline, attributes, HUD, netcode: still untouched. The test is no longer "one
folder" — it is "every seam is on this list, each is a NOTE into the module (information
flows in, verbs flow out through the adapter), and none replicates anything."

## Prerequisites before Phase 0

1. **Seam audit** (cloud, read-only): confirm BN's pressable input path from a fresh
   controller class + available ammo/health read-backs, so `IAIBAvatarInterface` is shaped
   by what BN has, not by memory. Also locate the spawn seam in `BNGameMode`.
2. Terminal cadence: rung 1 after each phase lands.
3. Nothing else blocks. GDC slides / video transcript would sharpen the confidence model
   (Phase 5) — welcome any time, blocking nothing.

## Provenance honesty

Everything published (ambition architecture, five skills, capability tiers, mode ambitions,
fairness doctrine) is implemented 1:1. Confidence internals, curve shapes and all concrete
numbers were never published — those are OUR designs inside THEIR architecture, and every
doc that touches them says so.
