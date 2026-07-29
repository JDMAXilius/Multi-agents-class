> ⚠️ **SUPERSEDED — NOT THE ACTIVE CONCEPT B.** This draft explored a
> *single-player* wave-survival roguelite. It was abandoned mid-draft when
> the direction changed: **Concept B is a Halo-inspired first-person
> shooter multiplayer** (vs. Arena's third-person melee multiplayer), so
> that the A/B comparison tests FPS-vs-TPS rather than MP-vs-SP.
>
> **Kept for two reasons:** (1) the wave/enemy-archetype/threat-budget
> design is directly reusable for the FPS concept's **Firefight-style PvE
> mode**, and (2) the AI Director + C++ validator pattern is the reference
> for any LLM-shapes-gameplay work. Do not treat any of it as current scope.

# SLASH ROLLER: GAUNTLET
## Game Design Document — Concept B (Scope Comparison) — SUPERSEDED DRAFT

**Course:** Multi-Agent AI for Game Development (ELVTR)
**Purpose:** Concept B — alternative capstone scope, written to the same
standard as *Slash Roller: Arena* for a head-to-head 6-week feasibility
comparison (see `../../docs/decisions/SCOPE-COMPARISON.md`)
**Author:** Juan Diego Lugo
**Date:** 24 July 2026
**Engine:** Unreal Engine 5.8 — pure native C++ / Gameplay Ability System

---

## 1. Executive Summary

### 1.1 Concept

**Slash Roller: Gauntlet** is a **single-player** wave-survival roguelite
with souls-weight melee combat. The player is a duelist sealed into one
arena for a 10-wave run against AI enemy squads — the same committed
light/heavy strikes, block, timed parry, stamina-costed dodge, and one
equipped magic slot. Between waves, an **AI Director** reshapes what comes
next based on how the player actually fought — and offers a draft of three
**boons** that bend the build. Every run is a fresh, seeded gauntlet;
every death is a scoreboard entry and a "run it back."

### 1.2 Win and Loss Conditions

- **Win a run:** clear all **10 waves**, ending with the **Warden** boss
  duel on wave 10 — *"Gauntlet Cleared."*
- **Lose a run:** die once — no revives; the run ends and scores.
- **Score:** waves cleared, then style (kills, parries landed, damage
  avoided, time). Same seed ⇒ same gauntlet, so scores on a seed are
  comparable and a run is shareable as its seed code.

### 1.3 Modes

| Mode | Description |
|---|---|
| Gauntlet Run | The core 10-wave seeded run |
| Daily Seed | One fixed seed per day, one attempt — comparable scores |
| Free Seed | Enter any seed code; practice or race a friend's run |

Single-player only — no netcode, no sessions, no lobby. A "race a
friend" is two players running the same seed and comparing scores.

### 1.4 Platform, Format, and Production Reality

- **Engine:** Unreal Engine 5.8, **pure native C++**, no Blueprint
  runtime logic; combat and boons built entirely on GAS.
- **Networking:** **none.** The entire multiplayer axis — replication,
  prediction, listen-server topology, host-advantage testing,
  join-in-progress — is out of the project.
- **Audio:** engine-native **MetaSounds**, triggered through GameplayCues.
- **Art:** marketplace environment + enemy assets; all gameplay code
  in-house.
- **Team & timeline:** one principal engineer / technical director plus
  the multi-agent AI crew, **6 weeks**, building on the shipped combat
  foundation (GAS core, soft-lock melee, input buffer, motion matching).
- **Exit criterion:** a playable **Steam demo build** (single-player
  depot — no multiplayer cert surface).

### 1.5 The Meta-Goal

Same thesis as Concept A — one senior engineer directing an agent crew
ships a professional small game fast — but Gauntlet adds a claim Arena
cannot make: **because the game is single-player, the runtime LLM is
allowed to shape gameplay.** The Director agent changes what the player
fights, not just what they read. No fairness constraint, no determinism
treaty violated: the model picks from an allowlisted menu, C++ validates
every choice, and the seed still reproduces the run.

### 1.6 Elevator Pitch

*Dark Souls* combat in a *Hades*-style run structure, with an AI Director
that watches how you fight and builds the next wave to punish it.

---

## 2. Game Mechanics (Player-Facing Actions and Loop)

### 2.1 The Run Loop

1. **Enter the gauntlet.** Pick a loadout (weapon archetype + magic
   slot) and a seed (or today's Daily).
2. **Fight the wave.** 3–6 enemies enter the arena; survive and clear it
   with the full melee kit.
3. **The intermission (15 s).** The Director speaks — one line about how
   you fought — and deals the **boon draft**: pick 1 of 3 run-modifying
   effects. Behind the curtain it also composes the next wave from your
   telemetry.
4. **Ten waves, one boss.** Wave 10 is the Warden: a full duel against a
   tier-3-class opponent wearing the run's modifiers.
5. **Score, seed, again.** Death or victory ends in the same place:
   score, seed code, "run it back?"

### 2.2 The Combat Verbs (unchanged from the shipped kit)

The player kit is identical to Concept A — this is the point: the combat
core is **reused wholesale**, and its verbs are already specified,
tuned, and shipped (light 12/15st · heavy 28/30st · block · parry 0.25 s
window, 1.5 s stagger · dodge 0.4 s i-frames/20st · magic slot on 12 s
cooldown; stamina 100, winded below 0 until 25%, regen 25/s after 1 s).
Full table: Appendix A.

### 2.3 Enemies — Three Archetypes, One Grammar

Enemies use the **same GAS ability grammar** as the player (same costs,
same telegraphs, same parryable strikes) — readable because the player
already speaks the language:

| Archetype | HP | Fantasy | Threat |
|---|---|---|---|
| **Rusher** | 40 | Fast lights, no defense | Swarms; drains your stamina through blocks |
| **Shieldbearer** | 80 | Blocks, heavies | Walls the arena; demands parry or guard-break |
| **Hexcaster** | 30 | Ranged bolts, keeps distance | Forces movement; punishes turtling |

A wave is a composition of these three plus a possible **modifier** (see
2.5). Concurrent enemies cap at 6; only 2 may attack simultaneously
(classic brawler "director token" rule) so souls-weight combat stays
readable against a crowd.

### 2.4 Boons — the Roguelite Engine (GAS makes this cheap)

A boon is a `GameplayEffect` row in `DT_Boons` — stat mods, cost mods,
and tagged triggers. Draft 1 of 3 per intermission; 9 boons per full run;
stacking allowed. First-draft slate of 12, all data:

- *Second Wind* — winded exit at 15% instead of 25%
- *Riposte* — parry-punish heals 10 HP
- *Momentum* — kill refunds 20 stamina
- *Heavy Hands* — heavy +6 dmg, +5 stamina cost (trade-off)
- *Twin Bolt* — Firebolt fires twice, cooldown +4 s (trade-off)
- …(7 more; full table Appendix B)

Boons are the C++-cheap depth engine: **zero new systems** — every boon
is a DataTable row + existing GAS machinery, curated by the crew and
refutable by the critic like any other data.

### 2.5 The Director — Pressure That Reads You

Between waves, the Director composes the next wave from run telemetry:
enemy mix, spawn pattern, and at most one **modifier** from an
allowlisted set (`fog` — sightlines shrink; `frenzy` — rushers +20%
speed; `hazard pulse` — arena edge damage ticks; `duelists` — enemies
gain parry). The composition rule is a contract: **the model chooses
from a menu; C++ validates every choice against wave-budget bounds**
(total threat points per wave scale on a fixed curve, so the Director
can shape difficulty but never spike it beyond the curve).

The player *feels* watched: turtle a wave and the next one has
Hexcasters; spam heavies and Shieldbearers arrive with a taunt naming
your habit. That line — *"Still swinging wide, duelist?"* — is the
Director's voice, and it's earned from telemetry, not canned.

### 2.6 Why It's a Game and Not a Grinder

The triangle plus stamina is the skill ceiling (unchanged); the run adds
the build layer: boon drafts force commitments the Director then tests.
Ten waves is 12–18 minutes — short enough to "run it back," long enough
for a build to matter. The seed system makes mastery legible: today's
Daily is the same gauntlet for everyone.

### 2.7 What the Player Sees (HUD)

- Health/stamina bars and the magic cooldown sweep (identical to A).
- **Top-center:** wave counter (3/10) + enemies remaining.
- **Intermission screen:** the Director's line, the three boon cards,
  the 15 s draft timer, current boon loadout strip.
- **Run end:** score breakdown, seed code (tap to copy), boon history,
  one coach line from run telemetry, "run it back?"

---

## 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay)

Two layers again — dev-time crew and runtime agents — but the boundary
moves: in single-player, the runtime layer is allowed to shape gameplay
**between waves**, under hard C++ validation.

### 3.1 The Dev-Time Crew (same crew, new products)

The formulated crew (12 agents, 5 contracts, ticket board) carries over
unchanged. Products for Gauntlet:

- **Arena Architect** → the gauntlet arena manifest (same constraints:
  ≤ 25 m sightlines, readable space).
- **Bot Trainer** → `DT_EnemyTuning` rows per archetype *and*
  `DT_WardenTuning` for the boss (same schema family as Arena's bot
  tiers).
- **Boon Curator** (the bot-trainer pattern applied to `DT_Boons`) —
  proposes boon rows with trade-off analysis; critic refutes for
  degenerate stacks (e.g., *Momentum* + *Second Wind* making winded
  unreachable).
- **Combat QA** → nightly headless runs: scripted-input player vs. every
  wave composition at every threat budget; flags unwinnable or trivial
  compositions.
- **Balance Analyst** → boon pick-rates and wave clear-rates out of
  band → data diffs.

### 3.2 The Runtime Agents

**Director Agent** — the headline. Runs in the 15 s intermission,
host... *local*-side (there is no host — it's the player's machine).
- **Role:** from run telemetry, returns the next wave's composition and
  one line of voice.
- **Gameplay effect:** the run adapts to the player's actual habits —
  the game's difficulty has a *personality*.
- **Output format:** `{wave_comp {rushers, shieldbearers, hexcasters,
  modifier ∈ allowlist | null}, director_line (≤ 18 words),
  taunt_reason (telemetry stat referenced)}`.
- **Safety rail (the contract):** C++ clamps the composition to the wave
  threat-budget curve and the allowlist. A malformed, late (> 5 s), or
  budget-breaking reply falls back to the seeded default composition —
  **the run is never blocked and the seed remains reproducible** (the
  fallback path is the seed path; Director choices are logged with the
  run so a scored run can be replayed exactly).
- **Cost:** ≤ 10 calls/run (9 intermissions + 1 coach), Haiku,
  ≈ 12k tokens ≈ **$0.01/run**.

**Coach line at run end** — same pattern as Arena's Caster coach: one
line, one telemetry stat, canned fallback.

### 3.3 Interaction Diagram

```
      DEV TIME (editor)                       RUNTIME (player's machine)
 brief ─► Arena Architect ─► arena                 wave cleared
          Bot Trainer ────► DT_EnemyTuning            │ run telemetry
          Boon Curator ───► DT_Boons                  ▼
          Combat QA ──────► wave sanity          Director Agent (15 s window)
          Balance Analyst ─► tuning diffs             │ {wave_comp, line}
                │                                     ▼
          human review ─► merge              C++ VALIDATOR (budget + allowlist)
                                                      │ pass → next wave
                                                      └ fail/late → seeded default
```

### 3.4 Determinism and Trust Boundaries

- **The seed is truth.** Enemy behavior, spawn order, and the default
  composition path are pure functions of the seed. Director deviations
  are logged into the run record, so every scored run replays exactly.
- **The model picks from a menu; code enforces the menu.** No free-text
  reaches the simulation — compositions are integers clamped to a
  budget, modifiers are enum members, lines are display strings.
- **Offline is a mode, not a failure.** No connectivity ⇒ pure seeded
  gauntlet + canned Director lines. The Daily stays fair because the
  Daily always runs the seeded path (Director disabled on Daily — one
  gauntlet for everyone).

---

## 4. Technical Strategy (Agent Roles, Token Budget, API Constraints)

### 4.1 Stack — what's reused, what's new, what's deleted

| Layer | Status |
|---|---|
| GAS combat core, input buffer, soft-lock, target assist, motion warping | **Reused wholesale** |
| Motion Matching / AL Framework | **Reused** (player); enemies use marketplace anim sets on the same notify grammar |
| Stamina + winded (Arena spec) | **Reused as specced** |
| MetaSounds via GameplayCues | **Reused as specced** |
| CommonUI stack | **Reused pattern**; fewer screens (no lobby, no host/join) |
| **Netcode, prediction audit, sessions, lobby, listen-server flows** | **DELETED — the entire axis** |
| New: wave/run flow (`OSRunSubsystem`), enemy AI (3 archetypes + Warden), boon system (data + GAS), Director validator + HTTP client, score/seed system | **The new surface** |

### 4.2 Token Budget (runtime, per run)

| Call | Model | Calls / run | Tokens in | Tokens out | Run total |
|---|---|---|---|---|---|
| Director (per intermission) | Claude Haiku | ≤ 9 | 1,100 | 90 | ≈ 10,700 |
| Coach (run end) | Claude Haiku | 1 | 900 | 80 | ≈ 1,000 |
| **Total** | | | | | **≈ 11,700 tokens ≈ $0.01** |

Caps local-side; fallback is the seeded path. Identical cost class to
Arena.

### 4.3 Constraints (named, with the decision each caused)

1. **Constraint: six weeks, one principal, C++-first.** *Therefore* the
   scope deletes the multiplayer axis entirely (the largest and
   riskiest-to-verify body of new work) and spends the reclaimed weeks
   on the two things single-player needs: enemy AI and run structure.
2. **Constraint: souls combat readability vs. crowds.** Multiple
   attackers can break commitment-based combat. *Therefore* the attack-
   token rule (max 2 simultaneous attackers), the 6-enemy cap, and enemy
   use of the player's own telegraph grammar.
3. **Constraint: an LLM in the gameplay path.** Allowed here — but only
   through a validator. *Therefore* menu-choice outputs, threat-budget
   clamps, a 5 s reply window inside a 15 s intermission, seeded
   fallback, and Daily-mode Director-off. The model gets creative
   authority, never authority over correctness.
4. **Constraint: Steam deploy in week 6.** *Therefore* single-player
   depot, no multiplayer cert surface, achievements deferred, and the
   demo is the first 5 waves of a fixed seed.

### 4.4 Scope (shipped list) and Schedule

**Shipped scope:** 10-wave seeded runs; 3 enemy archetypes + Warden
boss; 12 boons; Director agent with validator + seeded fallback; Daily
and Free Seed modes; score/seed system; CommonUI front end (menu,
intermission, run-end); MetaSounds; Steam demo build. **Nothing else.**

**Explicitly cut:** any multiplayer, second arena, meta-progression
between runs, leaderboard services (scores are local + seed-comparable),
achievements, 4th archetype.

| Week | Deliverable (each ends runnable) | Gate |
|---|---|---|
| 1 | Ladder bootstrap (crew ticket #1); arena blockout; wave/run flow; Rusher archetype; fight waves of rushers | Fighting a crowd feels like the duel game, not a mosh pit |
| 2 | Shieldbearer + Hexcaster; attack-token rule; threat-budget curve; seeded composition | All-archetype waves clear and readable |
| 3 | Boon system + draft UI + 12 boons; intermission flow | A build visibly changes a run |
| 4 | Director agent + validator + fallback; Warden boss | The run reacts to how you fight |
| 5 | Score/seed + Daily; CommonUI front end; Balance pass on QA soaks | Stranger completes a run unaided |
| 6 | Steam depot + demo (5 waves, fixed seed); soaks; polish | Shipped |

**First cut if behind:** the Director agent itself — the seeded
composition path IS the game without it (the LLM is additive by
construction). Second cut: Daily mode.

### 4.5 Success Criteria

- A stranger installs the demo and clears wave 3 inside ten minutes,
  unaided.
- Players quote a Director line back at you — the adaptation is *felt*.
- Every wave composition, boon, and tuning value in the build traces to
  a named agent output that was human-reviewed.

---

## Appendix A — Combat Tuning

Player kit identical to *Slash Roller: Arena* final (Appendix A there;
stamina regen 25/s, winded exit 25%). Gauntlet-specific:

| Parameter | Value | Note |
|---|---|---|
| Waves per run | 10 | Wave 10 = Warden duel |
| Concurrent enemies | ≤ 6 | Attack tokens: max 2 simultaneous attackers |
| Threat budget | wave n = 20 + 12n points | Rusher 8 · Shieldbearer 18 · Hexcaster 12 · modifier +15% |
| Intermission | 15 s | Director reply window 5 s, else seeded default |
| Run length target | 12–18 min | Demo = first 5 waves, fixed seed |
| Boons per run | 9 drafts of 3 | 12 boons at ship (Appendix B) |

## Appendix B — Boon Slate (12 at ship, all `DT_Boons` rows)

Second Wind · Riposte · Momentum · Heavy Hands · Twin Bolt · Stonewall
(block cost −50%) · Long Fuse (Shockwave radius +2 m) · Adrenaline
(regen delay 1.0 → 0.6 s) · Glass Edge (+15% dmg dealt and taken) ·
Patience (parry window +0.05 s) · Scavenger (winded enemies drop 10 HP) ·
Iron Lungs (stamina pool +20). Each row: effect GE, tags, trade-off
note, curator doubts.

## Appendix C — Telemetry Schema (per run)

`FOS_RunTelemetry`: per-wave clear times, damage taken by archetype,
verb usage, parries landed/suffered, times winded, boon picks, deaths
(wave, cause). Consumed by: Director (composition + lines), Coach,
Balance Analyst (boon pick/win rates), Combat QA (baseline clear rates).
