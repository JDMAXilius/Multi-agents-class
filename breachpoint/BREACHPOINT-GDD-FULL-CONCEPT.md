# BREACHPOINT
## Game Design Document — Concept B (Halo-Inspired Arena FPS)

**Course:** Multi-Agent AI for Game Development (ELVTR)
**Purpose:** Concept B — written to the house GDD standard for a
head-to-head 6-week feasibility comparison against *Slash Roller: Arena*
(see `../docs/decisions/SCOPE-COMPARISON.md`)
**Author:** Juan Diego Lugo
**Date:** 29 July 2026
**Engine:** Unreal Engine 5.8 — pure native C++ / Gameplay Ability System
**Foundation:** UE 5.8 **First Person template** only. 100% own gameplay
code (own GAS core ported from OnSight). Marketplace/free **art and
animation** permitted; **no third-party gameplay code, no Lyra project.**

---

## 1. Executive Summary

### 1.1 Concept

**Breachpoint** is a **4v4 team arena first-person shooter** built on the
Halo sandbox model: recharging shields over finite health, a two-weapon
carry limit, and the golden triangle of **shoot / grenade / melee** in
every engagement. A **Grappleshot** turns the map's verticality into a
traversal toy — pull to a ledge, yank a weapon off the floor, close
distance on a reloading enemy. Bots fill any empty slot and fight through
the same input path a human uses, so a match always starts.

### 1.2 Win and Loss Conditions

- **Win (Team Slayer):** first team to **50 kills**, or the higher team
  score when the **10:00** timer expires.
- **Win (FFA Slayer):** first player to **25 kills**, or highest at 10:00.
- **Tiebreak:** sudden death — no respawns, first kill wins, **60-second
  cap**; if uncontested, higher damage dealt wins.
- **Loss:** any other scoreline at the timer.

### 1.3 Modes

| Mode | Players | Description |
|---|---|---|
| Team Slayer | 4v4 | The ship mode; bots fill every empty slot |
| FFA Slayer | 2–8 | Same scoring code, no team assignment — near-free variant |
| Firefight (PvE) | 1–4 + waves | Co-op wave survival vs. bot squads — **stretch, first cut** |

PvP is the build target; **PvE is the same code with team assignment
changed** — bots already fight as players, so Firefight is a mode
config, not a second game.

### 1.4 Platform, Format, and Production Reality

- **Engine:** Unreal Engine 5.8, **pure native C++**, no Blueprint runtime
  logic. Combat, weapons, shields, and equipment all on GAS.
- **Foundation:** the stock **First Person template** (character, camera,
  Enhanced Input, projectile scaffold) — everything above it is written
  in-house. **No Lyra**, no third-party gameplay plugins.
- **Networking:** server-authoritative, **dedicated server** target from
  day one behind `IOSServerLifecycle`; listen server during development;
  **Amazon GameLift Servers** fleet as the deploy target.
- **Identity/Store:** Steam (auth ticket validated server-side).
- **Audio:** engine-native **MetaSounds** via GameplayCues.
- **Art:** marketplace/free environment kits, weapon models, character
  meshes, and animation sets. **Art is sourced; code is authored.**
- **Team & timeline:** one principal engineer / technical director plus
  the multi-agent AI crew, **6 weeks**.
- **Exit criterion:** playable **Steam demo build** on a live fleet.

### 1.5 Elevator Pitch

*Halo Infinite's* sandbox — shields, two weapons, grenades, Grappleshot —
as a 4v4 arena shooter, built from scratch in C++ by one engineer and an
AI crew.

---

## 2. Game Mechanics (Player-Facing Actions and Loop)

### 2.1 The Match Loop

1. **Spawn** with the standard loadout: Assault Rifle + Magnum, 2 frag
   grenades, Grappleshot (recharging).
2. **Fight** — every engagement mixes the triangle: strip shields, throw
   a grenade to flush, close with melee. The **motion tracker** tells you
   where, not who.
3. **Control the map** — the Rocket Launcher spawns on a fixed timer at a
   contested point; taking it is a team objective, not a pickup.
4. **Die → respawn** in 5 seconds at the spawn zone scored farthest from
   enemy pressure.
5. **Score or clock** — first to 50 (team) or highest at 10:00, then
   carnage report and rematch.

### 2.2 The Golden Triangle — the non-negotiable core

Every player always has all three, and no single one wins an engagement:

| Verb | Role | Detail |
|---|---|---|
| **Shoot** | Ranged damage | Two carried weapons, swap is instant-ish (0.4 s) |
| **Grenade** | Area denial / flush | 2 frags, physics-thrown, arcs off geometry |
| **Melee** | Finisher | High damage at contact; **rear melee = instant kill** |

The rhythm: shoot to break shields → grenade to deny the retreat → melee
to finish. That interaction *is* the game.

### 2.3 Shields Over Health — the Halo signature

Two damage layers, both GAS attributes:

- **Shields (100)** — recharge to full **2.5 s** after last damage taken,
  at 60/s. Audible/visual recharge cue.
- **Health (100)** — **does not regenerate**; the only recovery is death.

This single mechanic produces Halo's pacing: fights have a *break-off*
point, a wounded player is a hunted player, and re-engaging is a real
decision. It is the first system built (Week 1) because everything else
is tuned against it.

### 2.4 Weapon Sandbox — roles, not tiers

Four weapons at ship. Each is a distinct *tool*, and the damage-type
table gives the sandbox its tactical layer:

| Weapon | Source | Role | vs. Shields | vs. Health |
|---|---|---|---|---|
| **Assault Rifle** | Spawn | Spray, close-mid | Normal | Normal |
| **Magnum** | Spawn | Precision, headshot ×2 | Normal | **High** |
| **Plasma Rifle** | Map pickup | Shield-stripper | **High** | Low |
| **Rocket Launcher** | Power weapon, 90 s timer | AoE, map control | High | High |

**Kinetic hurts flesh, plasma strips shields** — the classic Halo
counter-logic, delivered by one `DT_DamageTypes` table. Two-weapon carry
forces the decision: drop the AR for plasma and you win the shield race
but lose the finisher.

### 2.5 Grappleshot — the traversal pillar (replaces vehicles)

A GAS ability on a **20-second cooldown**, three uses in one:

- **Pull to geometry** — grapple a ledge and yank yourself to it
  (verticality, escapes, flanks).
- **Pull a weapon** — yank a floor weapon into your hands from range.
- **Pull an enemy** — closes distance and sets up the melee finisher.

This is the concept's signature verb and the reason the map is built
vertically. It is also the **most netcode-sensitive system in the game**
(a predicted movement ability with an attach point) — specced as a
netcode-builder packet, not a gameplay packet.

### 2.6 Motion Tracker — a gameplay system, not a HUD widget

A 25 m radar showing **movement, not identity**: enemies moving faster
than a walk appear as contacts; crouch-walking hides you. It drives
flanking, ambush, and the decision to slow down.

**Netcode rule (anti-wallhack):** contacts are **computed server-side and
replicated as contacts** — the client never receives full enemy transforms
it isn't allowed to see. Radar is a hidden-information problem, and this
is where a naive implementation ships a cheat.

### 2.7 Map & Map Control

**One arena map**, symmetric, three vertical levels connected by grapple
points, lifts, and ramps. Fixed features:

- **2 team spawn zones** + 4 neutral spawn points (scored, anti-camp)
- **Rocket Launcher spawn** at the contested center, 90 s timer, visible
  countdown to both teams — the objective the match orbits
- **2 Plasma Rifle spawns** on the flanks, 45 s timer
- Sightline cap **35 m** (longer than Arena's melee spaces, still readable)

### 2.8 Bots — opponents that play like players

Bots occupy any unfilled slot in any mode, at three tiers:

| Tier | Fantasy | Accuracy | Reaction | Grenade use | Sandbox awareness |
|---|---|---|---|---|---|
| 1 — Recruit | Sprays, over-commits | 25% | 500 ms | Rare | Ignores power weapons |
| 2 — Marine | Uses cover, trades evenly | 45% | 320 ms | Situational | Contests plasma |
| 3 — Spartan | Pre-aims, flushes, rotates | 65% | 220 ms | Tactical | **Times the rocket spawn** |

All tiers activate abilities through the **same input path a human uses**
— no aimbot hooks, no privileged state — so bot-vs-bot soak tests
exercise the real combat code.

### 2.9 What the Player Sees (HUD)

- **Top-left:** shield bar over health bar (the two-layer read).
- **Bottom-right:** current weapon, ammo, spare mags; second weapon
  ghosted beneath.
- **Bottom-left:** motion tracker; grenade count; Grappleshot cooldown.
- **Center:** reticle with hit markers — *shield hit* and *flesh hit* are
  visually distinct (the sandbox is unreadable without this).
- **Top-center:** team score, match timer, power-weapon respawn countdown.
- **Feed:** killfeed + **medals** (Double Kill, Killing Spree, Grapple
  Kill) — canned, instant, deterministic.
- **Match end:** carnage report (K/D/A, accuracy, medals), rematch prompt.

---

## 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay)

Concept B's AI story is stronger than Arena's, and deliberately so:
**the bots are the headline.** Real game AI — perception, cover selection,
squad rotation, power-weapon timing — is a richer showcase than a melee
brain. The LLM stays where it belongs: out of the simulation.

### 3.1 In-Game AI — StateTree + EQS (no LLM)

**Bot Brain (StateTree).** States: `Patrol → Engage → Flank → Reposition
→ Retreat → ContestPower`. StateTree is UE 5.8's production-ready default
and merges behavior-tree selectors with state-machine transitions —
correct for combat AI that is fundamentally state-shaped.

**Cover & positioning (EQS).** Environment Query System scores positions
by cover from the current threat, distance to objective, and flank angle.
This is what makes bots *look* like they're playing rather than pathing.

**Deliberately not used:** **MassAI** (still experimental — not shippable
in a capstone) and **Learning Agents** (reinforcement learning is a
research trap for a 6-week window). StateTree + EQS is the professional,
current, shippable choice.

**Determinism:** bot decisions are a pure function of (tuning row, match
seed, observed events); reaction jitter is seeded once per match so QA
soaks reproduce exactly.

### 3.2 The Dev-Time Crew (Claude + Unreal MCP)

The formulated 12-agent crew carries over unchanged; only its products
change:

| Agent | Product for Breachpoint |
|---|---|
| **Arena Architect** *(One Wow)* | `arena_manifest.json` — three-level layout, grapple points, spawn scoring, power-weapon nodes, 35 m sightline cap |
| **Tuning Curator** | ALL gameplay numbers: `DT_Weapons` + `DT_DamageTypes` rows (TTK-defended), `DT_BotTuning` per tier, balance diffs outside the 45–55% band |

*(Nightly bot-vs-bot soak QA is the verifier's automated testing, not a
content agent — roster consolidated per the S03 "5+ agent types" red flag.)*

Every output is reviewable data (JSON/DataTables), refuted by the critic,
landed by a builder. Agents never commit directly.

### 3.3 The Runtime Agent — Spotter

**Spotter Agent** — the only model call in the shipped game, and never
load-bearing.

- **Role:** one telemetry-grounded coach line per human player at match
  end, plus optional color commentary appended to notable killfeed events
  (sprees, rocket multi-kills, grapple kills).
- **Gameplay effect:** every player leaves with one specific, earned
  correction — *"You lost 6 fights at under 40% shields — break off and
  let them recharge."*
- **Output:** `{coach_line (≤ 30 words, references ≥ 1 telemetry stat)}`;
  per event `{spotter_line (≤ 18 words) | null}`.
- **Never load-bearing:** medals and killfeed are **canned and instant**
  (Halo's announcer is deterministic by design — that's correct, not a
  limitation). Spotter lines *append*. Host-side, async, ≤ 12 calls/match,
  3 s timeout, canned fallback shipped in a DataTable.

### 3.4 Trust Boundaries

- **No LLM in the simulation path.** Its output is display strings.
- **Bots are server-side only**; clients see a replicated player.
- **The API key never leaves the server**; clients receive strings.
- **Radar contacts are server-computed** — hidden information stays hidden
  at the replication layer, not the render layer.

---

## 4. Technical Strategy

### 4.1 Stack — built, ported, sourced

| Layer | Origin |
|---|---|
| First-person character, camera, Enhanced Input | **UE 5.8 FPS template** |
| GAS core (PlayerState ASC, input buffer, prediction) | **Ported from OnSight** (own code) |
| Shields/health, weapons, grenades, melee, Grappleshot | **Authored** — GAS abilities + effects |
| Bot AI | **Authored** — StateTree + EQS |
| Game mode, scoring, respawn, power-weapon timers | **Authored** |
| UI | **Authored** — CommonUI, C++ base classes + BP visuals |
| Audio | **Authored** — MetaSounds via GameplayCues |
| Networking | **Authored** — dedicated server behind `IOSServerLifecycle` |
| Hosting | **Amazon GameLift Servers** (Server SDK 5 C++, containerized) |
| Meshes, animations, environment kits, VFX | **Sourced** (marketplace/free) |

### 4.2 Token Budget (runtime, per match)

| Call | Model | Calls / match | Tokens in | Tokens out | Total |
|---|---|---|---|---|---|
| Spotter (events, batched) | Claude Haiku | ≤ 12 | 600 | 60 | ≈ 7,900 |
| Coach (match end, per human) | Claude Haiku | ≤ 8 | 900 | 80 | ≈ 7,800 |
| **Total** | | | | | **≈ 15,700 ≈ $0.013** |

Caps enforced server-side; fallback is canned lines.

### 4.3 Constraints (named, with the decision each caused)

1. **Constraint: six weeks, one principal, 100% own gameplay code.**
   *Therefore* vehicles are cut entirely (networked multi-occupant physics
   is the genre's biggest scope trap), Grappleshot carries the traversal
   fantasy instead, and the sandbox ships at **four weapons** — the
   minimum that still produces real sandbox decisions.
2. **Constraint: no Lyra, no third-party gameplay code.** *Therefore*
   every system Lyra would have donated (weapons, teams, CommonUI spine,
   bot framework) is authored — which is the honest source of this
   concept's schedule risk (§4.5, and `../docs/decisions/SCOPE-COMPARISON.md`).
3. **Constraint: hidden information in a shooter.** A client that knows
   enemy positions is a wallhack. *Therefore* radar contacts are computed
   server-side, and replication uses the tightest conditions and relevancy
   culling available.
4. **Constraint: GameLift plugin support lags UE 5.8.** *Therefore* we
   integrate **Server SDK 5 in C++** behind `IOSServerLifecycle` and use
   the Managed Containers workflow rather than depending on the versioned
   plugin — and dev runs on listen/local-dedicated so infra never blocks
   gameplay work.
5. **Constraint: LLM latency (1–4 s).** *Therefore* medals/killfeed are
   canned and instant; Spotter is async and additive only.

### 4.4 Steam + GameLift Roadmap

| Phase | Week | Deliverable |
|---|---|---|
| Architect | 1 | `Server` build target compiles; `IOSServerLifecycle` in place; local dedicated server runs |
| Containerize | 2 | Server packaged into a container image; runs locally in Docker |
| Backend | 4 | Minimal backend: session placement request → server IP returned |
| Fleet | 5 | GameLift Servers fleet live (Managed Containers); Server SDK 5 lifecycle hooks (`InitSDK`/`ProcessReady`/`OnStartGameSession`) |
| Identity | 5 | Steam auth ticket issued client-side, **validated server-side**; Steam App ID + depot |
| Ship | 6 | Steam demo depot uploaded; client connects → matchmaking → live fleet server |

**Fallback (pre-declared):** if the fleet slips, ship on **listen server**
— it's an interface swap behind `IOSServerLifecycle`, not a rewrite.

### 4.5 Scope and Schedule

**Shipped scope:** Team Slayer 4v4 + FFA Slayer; 1 arena map; 4 weapons;
frag grenades; melee incl. rear-kill; Grappleshot; shields/health; motion
tracker; bots at 3 tiers; power-weapon spawn timers; CommonUI HUD + front
end; medals + killfeed; Spotter agent with canned fallback; MetaSounds;
dedicated server on GameLift; Steam demo build. **Nothing else.**

**Explicitly cut:** all vehicles, second map, campaign, Forge, ranked,
progression/cosmetics, equipment beyond Grappleshot, BTB, splitscreen.

| Week | Deliverable (each ends runnable) | Gate |
|---|---|---|
| 1 | Ladder bootstrap; FPS template + GAS core port; **shields/health**; AR firing; `Server` target compiles | Shooting a dummy and breaking its shields feels right |
| 2 | Weapon sandbox (4 weapons, 2-carry, damage types); grenades; melee | **The golden triangle works** |
| 3 | Grappleshot; map blockout (crew); motion tracker; Slayer scoring + respawn | Traversal is fun; a full match completes |
| 4 | Bots (StateTree + EQS, 3 tiers); CommonUI HUD | 4v4 vs. bots plays end-to-end |
| 5 | Dedicated server → GameLift fleet; Steam auth + depot; front-end menus | Online 4v4 on a live fleet |
| 6 | Balance pass (crew telemetry); MetaSounds polish; demo build; capstone presentation | Shipped |

**First cut if behind, in order:** ① GameLift → listen server ·
② 4th weapon (Rocket) → power-weapon slot becomes Plasma ·
③ Motion tracker · ④ Bot tiers 2–3 → single tier.

### 4.6 Success Criteria

- A stranger installs the demo, joins a 4v4 with bots, and gets a kill
  inside two minutes without instructions.
- Playtesters use the Grappleshot offensively (not just to travel) — the
  sign the pillar landed.
- Every map decision, bot tier, and weapon value traces to a named agent
  output that was human-reviewed.

---

## 5. Honest Risk Register

| Risk | Severity | Mitigation |
|---|---|---|
| **Schedule** — ~2× the build inventory of Concept A (see comparison doc) | **High** | Pre-declared cut order (§4.5); W2 fun-gate decides continue/cut |
| Grappleshot prediction under latency | High | netcode-builder packet + critic REFUTER pass; rung-4 Gauntlet with 120 ms emulation |
| Radar leaking hidden information | High | Server-computed contacts; critic writes the wallhack as the acceptance test |
| GameLift plugin/engine version mismatch | Medium | Server SDK 5 C++ integration, not the plugin; listen-server fallback behind the interface |
| Bot AI feels robotic (Halo's AI is the bar) | Medium | EQS-driven cover + tiered tuning as data; nightly soaks by Combat QA |
| Sourced animations don't fit authored abilities | Medium | Ability timings authored *to* the acquired anim sets, not the reverse |
| Sandbox breadth vs. 6 weeks | **High** | Four weapons is the floor, not a target; §4.5 cut order protects the triangle |

---

## Appendix A — Combat Tuning (first pass)

| Parameter | Value |
|---|---|
| Shields / Health | 100 / 100 |
| Shield recharge | 60/s after 2.5 s no damage |
| Health regen | none |
| AR | 8 dmg/shot, 600 RPM, 32 mag |
| Magnum | 22 dmg, ×2 headshot, 8 mag |
| Plasma Rifle | 18 vs shields / 6 vs health, 400 RPM |
| Rocket | 120 dmg AoE r=4 m, 2 shots, 90 s respawn |
| Frag grenade | 90 dmg center, r=5 m, 2 carried |
| Melee | 70 dmg; **rear = instant kill** |
| Grappleshot | 20 s cooldown, 20 m range |
| Motion tracker | 25 m radius; crouch-walk = hidden |
| Respawn | 5 s |
| Match | Team 50 kills / 10:00 · FFA 25 kills / 10:00 |

## Appendix B — Bot Tuning Schema

`DT_BotTuning`: `tier, display_name, accuracy_pct, reaction_ms,
burst_discipline, grenade_policy ∈ {rare, situational, tactical},
power_weapon_timing (bool), cover_preference (0..1), push_threshold
(shield %), target_switch_bias`. No tier may exceed 200 ms reaction
(superhuman guard, enforced by schema).

## Appendix C — Telemetry Schema

`FOS_MatchTelemetry`: kills, deaths, assists, accuracy per weapon, TTK
distribution, shield-break→kill conversion, grenade kills, melee kills,
grapple uses/kills, power-weapon holds, fights lost under 40% shields,
medals. Consumed by: Spotter (coach lines), the tuning-curator (TTK/win
bands), Combat QA (regression baselines).
