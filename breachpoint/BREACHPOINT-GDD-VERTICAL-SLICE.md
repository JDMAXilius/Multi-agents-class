# BREACHPOINT — VERTICAL SLICE
## Game Design Document (Option C — the version we build)

**Course:** Multi-Agent AI for Game Development (ELVTR)
**Author:** Juan Diego Lugo
**Date:** 29 July 2026
**Engine:** Unreal Engine 5.8 — pure native C++ / Gameplay Ability System
**Foundation:** UE 5.8 **First Person template** only. 100% authored
gameplay code (own GAS core, ported from our existing C++ codebase).
**No Lyra, no third-party gameplay code.** Marketplace/free **art and
animation** permitted and expected.
**Supersedes for build purposes:** `BREACHPOINT-GDD-FULL-CONCEPT.md` — that
document remains the Phase-2 target; **this** is the six-week build.
**Authority split (this doc is not authoritative for everything):**
schedule and milestone sequencing are owned by `BREACHPOINT-ROADMAP.md`;
bot architecture is owned by `BREACHPOINT-AI-BOTS.md`; file/class layout is
owned by `BREACHPOINT-ARCHITECTURE.md`. Where this document and one of those
disagree, **the named doc wins** and the split is a defect to fix here.

---

## 1. Executive Summary

### 1.1 Concept

**Breachpoint** is a **4v4 team arena first-person shooter** built on the
Halo sandbox model. Recharging **shields over finite health** give every
fight a break-off point. A **two-weapon carry limit** and one contested
power weapon make the map itself an objective. The **golden triangle** —
shoot, grenade, melee — is available in every engagement, and a
**Grappleshot** turns a three-level arena into a traversal playground:
pull to a ledge, yank a rocket off the floor, close on a reloading enemy.

Bots fill every empty slot and fight through the same input path a human
uses, so a match always starts.

This document specifies a **vertical slice**: the complete Halo feel at
narrow breadth. Everything cut is listed in §6 with its Phase-2 restore
path — nothing is silently dropped.

### 1.2 Win and Loss Conditions

- **Win:** first team to **25 kills**, or the higher team score when the
  **8:00** timer expires.
- **Tiebreak:** sudden death — no respawns, first kill wins, **60-second
  cap**; if uncontested, higher team damage dealt wins.
- **Loss:** any other scoreline at the timer.

### 1.3 Mode

| Mode | Players | Description |
|---|---|---|
| **Team Slayer** | 4v4 (humans + bots in any mix) | The only shipped mode. Bots fill every unfilled slot. |

Solo play is Team Slayer with seven bots. **PvE is not a separate
game** — it is the same code with the roster changed.

### 1.4 Platform, Format, and Production Reality

- **Engine:** UE 5.8, **pure native C++**, no Blueprint runtime logic.
  Combat, weapons, shields, and Grappleshot all on GAS.
- **Foundation:** stock **First Person template** (character, camera,
  Enhanced Input). Everything above it is authored in-house.
- **Networking:** server-authoritative, **Steam listen server**
  (host + invite), built behind **`IBRServerLifecycle`** so the
  dedicated-server/GameLift migration is a swap, not a rewrite.
- **Audio:** engine-native **MetaSounds** via GameplayCues.
- **Art:** sourced marketplace/free environment kit, weapon models, FPS
  arms + weapon animation sets, VFX. **Art is sourced; code is authored.**
- **Team & timeline:** one principal engineer / technical director plus
  the 12-agent crew, **6 weeks** (see §7 for the honest estimate).
- **Exit criterion:** playable **Steam demo depot**.

### 1.5 Elevator Pitch

*Halo's* sandbox — shields, two weapons, grenades, and a grappling hook —
as a 4v4 arena shooter, written from scratch in C++ by one engineer and
an AI crew in six weeks.

---

## 2. Game Mechanics (Player-Facing Actions and Loop)

### 2.1 The Match Loop

1. **Spawn** with the standard loadout: **Assault Rifle + Magnum**,
   2 frag grenades, Grappleshot (recharging).
2. **Fight** — every engagement mixes the triangle: strip shields with
   fire, throw a grenade to deny the retreat, close with melee to finish.
3. **Contest the rocket** — the **Rocket Launcher** respawns on a fixed
   **90-second timer** at the arena's contested mid-level, with a
   countdown visible to both teams. It is the reason teams fight over a
   place instead of wandering.
4. **Die → respawn** in 5 seconds at the spawn point scored farthest from
   enemy pressure.
5. **Score or clock** — first to 25 or highest at 8:00 → carnage report →
   rematch.

### 2.2 The Golden Triangle — the non-negotiable core

All three are always available, and none alone wins an engagement:

| Verb | Role | Detail |
|---|---|---|
| **Shoot** | Ranged damage | Two carried weapons; swap 0.4 s |
| **Grenade** | Area denial / flush | 2 frags, physics-thrown, bounces off geometry |
| **Melee** | Finisher | 70 dmg at contact; **rear melee = instant kill** |

The rhythm — *shoot to break shields → grenade to cut the retreat →
melee to finish* — **is** the game. If this is not fun by end of Week 2,
the project stops (§7.2).

### 2.3 Shields Over Health — the signature system

Two damage layers, both GAS attributes on the PlayerState-owned ASC:

- **Shields (100)** — recharge at 60/s beginning **2.5 s** after the last
  damage taken. Distinct audio + visual cue on break and on recharge.
- **Health (100)** — **never regenerates.** The only reset is death.

Consequences that define the pacing: fights have a natural break-off
point; a wounded player is a hunted player; re-engaging is a real
decision, not a reflex. **Built first (Week 1)** because every other
number is tuned against it.

### 2.4 Weapons — three, each a distinct decision

| Weapon | Source | Type | Role |
|---|---|---|---|
| **Assault Rifle** | Spawn | Hitscan, 600 RPM | Spray; the shield-stripper you always have |
| **Magnum** | Spawn | Hitscan, ×2 headshot | Precision; the finisher on a broken shield |
| **Rocket Launcher** | **Map pickup, 90 s timer** | Projectile, AoE | The power weapon; map control; the demo's spectacle |

Three weapons is the **floor** that still produces real sandbox
decisions, because the two-weapon limit makes the rocket a *trade*: pick
it up and you drop your AR or your Magnum — losing either your
shield-stripping or your finishing tool. That trade is the sandbox.

*(The rocket also amortizes: it shares the projectile + radial-damage
implementation with grenades, so weapon #3 costs a fraction of weapons
#1–2.)*

### 2.5 Grappleshot — the traversal pillar

A GAS ability on a **20-second cooldown**, 20 m range, three uses in one:

- **Pull to geometry** — yank yourself to a ledge (verticality, escape,
  flank).
- **Pull a weapon** — snatch the floor rocket from range. This single
  interaction makes the power-weapon contest dramatic.
- **Pull an enemy** — close distance and set up the melee finisher.

This is the concept's signature verb and the reason the map is vertical.
It is also the **most netcode-sensitive system in the build** — a
predicted movement ability with an attach point — and is therefore
specced as a **netcode-builder** packet with a mandatory **critic REFUTER
pass** before landing (§4.4).

### 2.6 The Map — one arena, three levels

A single symmetric arena with three connected elevations:

- **Lower** — enclosed, close-quarters, AR/melee territory
- **Mid** — the contested band; **Rocket Launcher spawn** at its center
- **Upper** — catwalks and ledges reachable **primarily by Grappleshot**
  (the pillar must be load-bearing for map access, not decorative)

Fixed constraints for the Arena Architect agent:

- 2 team spawn zones + 4 scored neutral spawn points (anti-camp)
- **Sightline cap 35 m** — long enough for Magnum duels, short enough to
  stay readable
- Every upper-level position reachable by at least **two** grapple points
  (no single-point chokes)
- Rocket spawn visible from all three levels (the contest must be legible)

### 2.7 Information Without Radar — a named design consequence

The full concept ships a motion tracker; **the slice cuts it** (§6). That
removes the player's primary awareness tool, so the slice replaces it
deliberately rather than accidentally:

- **Footstep and weapon audio are the information system.** MetaSounds
  spatialization is a *gameplay* requirement here, not polish — distinct,
  directional footsteps and clearly-audible reloads carry the awareness
  load radar would have.
- **Map readability compensates:** the 35 m sightline cap and open
  three-level sightlines mean threats are usually visible before they are
  lethal.

This is a real trade and it is named as one: without radar the slice
plays slightly more twitchy and less tactical than full Halo. It is the
right cut for six weeks; it is the **first system restored** in Phase 2.

### 2.8 Bots — one brain, dialed

Bots occupy every unfilled slot. The slice ships **one authored behavior
profile** scaled by a difficulty multiplier — giving three player-facing
difficulties without authoring three behavior trees:

| Setting | Accuracy | Reaction | Grenade use | Behavior |
|---|---|---|---|---|
| Recruit | 25% | 500 ms | Rare | Same StateTree, dulled |
| Marine *(default)* | 45% | 320 ms | Situational | Baseline profile |
| Veteran | 65% | 220 ms | Tactical | Same StateTree, sharpened |

All settings drive the **same** StateTree and EQS queries — only
`DT_BotTuning` scalars change. Bots activate abilities through the same
input path a human uses: no aimbot hooks, no privileged state, no
side-channel damage. This is what makes bot-vs-bot soak tests exercise
the real combat code (§4.5).

### 2.9 What the Player Sees (HUD)

- **Top-left:** shield bar over health bar — the two-layer read is the
  most important element on screen.
- **Bottom-right:** current weapon + ammo + spare mags; second weapon
  ghosted beneath.
- **Bottom-left:** grenade count; **Grappleshot cooldown ring**.
- **Center:** reticle with **distinct hit markers for shield hits vs.
  flesh hits** — the sandbox is unreadable without this distinction.
- **Top-center:** team score, match timer, **rocket respawn countdown**.
- **Feed:** killfeed + medals (Double Kill, Killing Spree, **Grapple
  Kill**, Rocket multi-kill) — **canned, instant, deterministic**.
- **Match end:** carnage report (K/D/A, accuracy, medals) + one Spotter
  coach line + rematch prompt.

---

## 3. AI Architecture (What Each Agent Does, Through Its Effect on Gameplay)

The slice's AI story is deliberately **bots-first**: real game AI is a
stronger showcase than an LLM narrator, and it is the part a player
actually experiences. The LLM stays out of the simulation entirely.

### 3.1 In-Game AI — StateTree + EQS (no LLM)

> **Superseded in detail by `BREACHPOINT-AI-BOTS.md`** (binding for BP08,
> rulings R8–R12): the shipped brain is **three layers** — a GOAP-style
> ambition layer decides *what*, the StateTree below decides *how*, GAS is
> the only hand. This section stays accurate on the execution spine, the
> EQS role, and the no-LLM/determinism laws; it predates the ambition layer.

**Bot Brain — StateTree.** States: `Seek → Engage → Flush → Reposition →
Retreat → ContestRocket`. StateTree is UE 5.8's production-ready default
and merges behavior-tree selectors with state-machine transitions —
the correct shape for combat AI. Transitions are driven by shield state,
ammo, threat count, and the rocket timer.

**Cover and positioning — EQS.** Environment Query System scores
candidate positions by cover from the current threat, distance to the
rocket spawn, and flank angle. This is what makes a bot *read* as playing
rather than pathing.

**Explicitly not used:** **MassAI** (experimental — not shippable in a
capstone) and **Learning Agents** (reinforcement learning is a research
project, not a six-week feature). StateTree + EQS is the current,
professional, shippable choice.

**Determinism:** bot decisions are a pure function of (tuning row, match
seed, observed events); reaction jitter is seeded once per match, so QA
soaks reproduce exactly and any disputed result is replayable.

### 3.2 The Dev-Time Crew (Claude + Unreal MCP)

The formulated 12-agent crew carries over unchanged; only its products
change. Every output is reviewable data (JSON/DataTables), refuted by the
critic, landed by a builder. **Agents never commit directly.**

The content crew is **three agents** (consolidated per the S03 scope
red-flag — see the Assignment #2 GDD §5.4):

| Agent | Product for the slice |
|---|---|
| **Arena Architect** *(the One Wow agent)* | `arena_manifest.json` — three-level layout, grapple points (≥2 per upper position), scored spawns, rocket node, 35 m sightline cap |
| **Tuning Curator** | ALL gameplay numbers: `DT_Weapons` rows with TTK analysis, `DT_BotTuning` profiles + difficulty scalars, and balance diffs (proposes only outside the 45–55% band) |
| **Spotter** | The runtime agent (§3.3) — coach + killfeed strings, moderated before display |

*(Nightly bot-vs-bot soaks — stuck navmesh, spawn-kill loops, TTK
regressions — are the verifier's automated testing, not a content agent.)*

### 3.3 The Runtime Agent — Spotter

**Spotter Agent** — the only model call in the shipped game, and never
load-bearing.

- **Role:** one telemetry-grounded coach line per human player at match
  end; optional color commentary appended to notable killfeed events
  (sprees, rocket multi-kills, grapple kills).
- **Gameplay effect:** every player leaves with one specific, *earned*
  correction — *"You lost 6 fights below 40% shields — break off and let
  them recharge."*
- **Output:** `{coach_line (≤ 30 words, references ≥ 1 telemetry stat)}`;
  per event `{spotter_line (≤ 18 words) | null}`.
- **Never load-bearing:** medals and killfeed are **canned and instant** —
  Halo's announcer is deterministic by design, which is correct, not a
  limitation. Spotter lines only *append*. Host-side, async, ≤ 12
  calls/match, 3 s timeout, canned-line DataTable fallback shipped in the
  build. **No connectivity ⇒ the game is identical minus flavor.**

### 3.4 Trust Boundaries

- **No LLM in the simulation path.** Its only output is display strings.
- **Bots are server-side only**; clients see a replicated player.
- **The API key never leaves the host**; clients receive strings.
- **Hidden information stays hidden at the replication layer**, not the
  render layer — enemy transforms use the tightest replication conditions
  and relevancy culling available.

---

## 4. Technical Strategy

### 4.1 Stack — authored, ported, sourced

| Layer | Origin |
|---|---|
| First-person character, camera, Enhanced Input | **UE 5.8 FPS template** |
| GAS core (PlayerState ASC, input buffer, prediction) | **Ported** — our own existing C++ |
| Steam sessions | **Ported** — our own existing C++ (`BRSessionsSubsystem`) |
| Shields/health, weapons, grenades, melee, **Grappleshot** | **Authored** — GAS abilities + effects |
| Bot AI | **Authored** — StateTree + EQS |
| Team Slayer, scoring, respawn scoring, rocket timer | **Authored** |
| HUD + front end | **Authored** — CommonUI, C++ base classes + BP visuals |
| Audio | **Authored** — MetaSounds via GameplayCues |
| Server lifecycle abstraction | **Authored** — `IBRServerLifecycle` |
| Meshes, animations, environment kit, VFX | **Sourced** |

### 4.2 Token Budget (runtime, per match)

| Call | Model | Calls / match | Tokens in | Tokens out | Total |
|---|---|---|---|---|---|
| Spotter (events, batched ≥ 10 s) | `claude-haiku-4-5` | ≤ 12 | 600 | 60 | ≈ 7,900 |
| Coach (match end, per human) | `claude-haiku-4-5` | ≤ 8 | 900 | 80 | ≈ 7,800 |
| **Total** | | | | | **≈ 15,700 ≈ $0.021 / match** |

Priced at Haiku 4.5's current rates — **$1.00 / 1M input, $5.00 / 1M output** — the split
matters: 14,400 input tokens cost $0.0144 and 1,360 output tokens cost $0.0068. The model ID
is pinned (`claude-haiku-4-5`, 200K context) because "Claude Haiku" alone is ambiguous across
generations and the price differs by generation. Worst case at the caps is ~2¢ a match; the
caps, not the price, are the control. Re-check both against current pricing at BP11.

Caps enforced server-side; every failure path falls back to canned lines.

### 4.3 Constraints (named, with the decision each caused)

1. **Constraint: six weeks, one principal, 100% authored gameplay code.**
   *Therefore* the sandbox ships at **three weapons and one map**, the
   motion tracker is cut, bots ship as one scaled profile, and vehicles
   were never in scope. The full sandbox is Phase 2 (§6).
2. **Constraint: no Lyra, no third-party gameplay code.** *Therefore*
   every system Lyra would have donated is authored — which is the honest
   source of this project's schedule risk (§7), and the reason the GAS
   core is **ported from our own shipped codebase** rather than rebuilt.
3. **Constraint: Grappleshot is a predicted movement ability.** Prediction
   is the highest-risk code an agent crew writes (the silent-and-confident
   class). *Therefore* it is a netcode-builder packet, ships with its own
   cheat-attempt test, and requires a critic REFUTER pass and a rung-4
   Gauntlet run under 120 ms emulation before landing.
4. **Constraint: GameLift plugin support lags UE 5.8, and infra work in
   Week 1 kills capstones.** *Therefore* the slice ships on **listen
   server** behind `IBRServerLifecycle`, with the dedicated-server/
   GameLift fleet as a **post-course swap** rather than a six-week
   dependency.
5. **Constraint: LLM latency (1–4 s).** *Therefore* medals and killfeed
   are canned and instant; Spotter is async and additive only.

### 4.4 Netcode Rules (slice-specific)

- **Server-authoritative hit registration.** Clients send fire intent;
  the server validates and applies damage. No client-declared kills.
- **Every `Server` RPC ships `WithValidation`** with a real `_Validate`
  body — range, rate, and possession checks. Empty `return true;` fails
  review.
- **Grapple prediction reconciles.** A rejected grapple must leave zero
  gameplay state — no position fork, no cooldown consumed, no cue
  residue.
- **Replicate the minimum.** `COND_OwnerOnly` for private state (ammo
  reserves, exact shield value); relevancy and net-cull tuned per class.
- **The attack ships with the feature.** Each new replicated surface
  lands with its own cheat-attempt test whose **rejection** is the
  acceptance criterion; the critic re-attacks independently.

### 4.5 Verification (the ladder)

Per `docs/contracts/testing.md`: clean compile → headless
Automation Specs (`Breachpoint.Sim.*` pins TTK, shield recharge, damage
math; `Breachpoint.Bots.*` pins determinism: same seed + tuning row ⇒
identical action trace) → functional tests → **Gauntlet networked smoke**
(server + 2 clients, assert in threes, then again under network emulation
at `120 ms` lag / `5%` loss) → perf spot-checks. **A rung that cannot run
is BLOCKED, never skipped.** Overnight bot-vs-bot soaks run nightly from
Week 4.

---

## 5. Scope and Schedule

### 5.1 Shipped Scope

Team Slayer 4v4 (bots filling any slot, 3 difficulty settings); **1**
three-level arena map; **3** weapons (AR, Magnum, Rocket on a 90 s
timer); frag grenades; melee including rear-kill; **Grappleshot**;
shields-over-health; scored respawns; CommonUI HUD + minimal front end;
canned medals + killfeed; Spotter agent with canned fallback; MetaSounds
combat and footstep audio; Steam listen server + demo depot.
**Nothing else.**

### 5.2 Schedule — six weeks, each ending runnable

> **`BREACHPOINT-ROADMAP.md` is authoritative for sequencing.** The table
> below is the original week-by-week sketch; the roadmap re-cut it into six
> parallel pods and moved work accordingly (Rocket and HUD v1 pull into W3,
> bots start W3 and complete at M4, Steam holds at W5). Where the two
> disagree, follow the roadmap.

| Wk | Deliverable | Gate |
|---|---|---|
| **1** | Ladder bootstrap (specs + Gauntlet skeleton); FPS template + module layout; **GAS core ported**; **shields/health**; AR firing; `IBRServerLifecycle` stub | Breaking a dummy's shields feels good |
| **2** | Magnum + two-weapon carry + swap; **grenades**; **melee + rear-kill** | ⚠️ **THE GOLDEN TRIANGLE FUN TEST** — see §7.2 |
| **3** | **Grappleshot** (netcode packet + REFUTER pass); map blockout from Arena Architect manifest | Traversal is fun; grapple is used offensively |
| **4** | Bots (StateTree + EQS + 3 scalars); Team Slayer scoring, teams, scored respawns; nightly soaks begin | ⚠️ **GO / NO-GO** — a full 4v4 match vs. bots plays end-to-end |
| **5** | Rocket Launcher + 90 s timer; CommonUI HUD; Steam listen server + host/invite; MetaSounds cues | Two humans + six bots, online, full match |
| **6** | Balance pass (crew telemetry); polish; Steam demo depot; capstone presentation | **Shipped** |

### 5.3 Cut Order (pre-declared — used, not improvised)

If behind at any gate, cut **in this order** and log it:

1. **Rocket Launcher** → two weapons; power-weapon contest becomes a
   map-control point without a reward *(costs the sandbox trade — cut
   only if Week 5 is at risk)*
2. **Bot difficulty settings** → single "Marine" profile
3. **Medals** → killfeed only
4. **Spotter agent** → canned coach lines only *(the fallback already
   ships, so this is a config flip)*
5. **Front-end menus** → direct-to-match + Steam overlay invite only

**Never cut:** shields-over-health, the golden triangle, Grappleshot.
Those three *are* the game; cutting any of them means building a
different, worse project.

### 5.4 First Restore If Ahead

1. **FFA Slayer** (same scoring code, team assignment off — near-free)
2. **Motion tracker** (the most-missed cut system, §2.7)
3. Plasma Rifle + the plasma/kinetic damage-type layer

---

## 6. What's Cut vs. Full Breachpoint — and the Phase-2 path

Nothing here is silently dropped. Each cut has a named restore path.

| Cut from full concept | Why | Phase 2 restore |
|---|---|---|
| Motion tracker | 0.5 w; awareness carried by audio + sightlines (§2.7) | First restore post-slice; needs server-computed contacts |
| Plasma Rifle + damage-type layer | With only kinetic weapons the damage table is trivial | Add with weapon #4; table already schema'd |
| Bot tiers 2–3 as distinct behaviors | One scaled profile delivers 3 difficulties | Author distinct StateTrees per tier |
| **Dedicated server + GameLift fleet** | 1.5 w of infra that blocks gameplay work | **Interface already in place** (`IBRServerLifecycle`) — a swap, not a rewrite |
| FFA + Firefight modes | Mode variety is not the slice's thesis | Both are roster/team-assignment configs |
| Second map | Content, not systems | Arena Architect produces from the same manifest schema |
| Vehicles | Networked multi-occupant physics — the genre's biggest scope trap | Not planned; Grappleshot carries traversal permanently |

---

## 7. Honest Schedule Assessment

### 7.1 The estimate

Re-derived line-by-line from the build inventory in
`../docs/decisions/SCOPE-COMPARISON.md`, with the Option-C cuts applied:

| | Weeks |
|---|---|
| Raw build inventory | **≈ 8.4** |
| After crew compression (~25%, content and review only) | **≈ 6.3** |
| Window | **6.0** |

> **This fits six weeks only with near-zero slippage. It fits seven
> comfortably.** The earlier ≈5.5–6 w figure in the comparison document
> was optimistic; this is the corrected number.

The plan absorbs the gap three ways: the **pre-declared cut order**
(§5.3) is expected to be *used*, not held in reserve; **art is sourced,
never authored**; and the crew owns all content production (map, tuning,
QA) so the principal's weeks go to systems only.

### 7.2 The two gates that matter

**Week 2 — the Golden Triangle fun test.** Shoot/grenade/melee against a
stationary dummy and one scripted bot. If the loop is not fun here, **no
amount of later content saves it** — and Concept A's shipped foundation
is still available. This gate exists to make that switch cheap while it
is still cheap.

**Week 4 — go/no-go.** A full 4v4 vs. bots must play end-to-end. If it
does not, execute the cut order down to two weapons and one bot profile,
and protect Weeks 5–6 for shipping. **Shipping a smaller slice beats
shipping nothing** — that is the entire lesson of the scope analysis.

### 7.3 Risk register

| Risk | Severity | Mitigation |
|---|---|---|
| **Schedule (~6.3 w into 6 w)** | **High** | Pre-declared cut order used actively; Week-4 go/no-go; art fully sourced |
| **Grapple prediction under latency** | **High** | netcode-builder packet; cheat-test ships with feature; critic REFUTER; rung-4 at 120 ms before landing |
| Bots read as robotic (Halo's AI is the bar) | Medium | EQS-driven cover; nightly soaks from Week 4; tuning is data, iterable to the last day |
| Sourced animations don't fit authored abilities | Medium | **Author ability timings to the acquired anim sets**, never the reverse — decide the anim pack in Week 1 |
| No radar makes fights feel blind | Medium | Audio is a gameplay requirement (§2.7); sightline cap; restore radar first if ahead |
| Listen-server host advantage | Medium | Standing review item on every netcode packet; host and remote claims tested separately |
| StateTree/EQS learning curve | Low–Med | Excluded from estimates deliberately; Week 4 is the buffer-consuming week if it bites |

---

## 8. Success Criteria

- A stranger installs the Steam demo, joins a 4v4 with bots, and gets a
  kill inside **two minutes**, unaided.
- Playtesters use the **Grappleshot offensively** — not just to
  travel — the sign the pillar landed.
- Playtesters ask for another match unprompted at the carnage report.
- Every map decision, bot value, and weapon number in the build traces to
  a **named agent output that a human reviewed**.

---

## Appendix A — Combat Tuning (first pass)

| Parameter | Value |
|---|---|
| Shields / Health | 100 / 100 |
| Shield recharge | 60/s, begins 2.5 s after last damage |
| Health regeneration | none |
| Assault Rifle | 8 dmg/shot, 600 RPM, 32 mag, hitscan |
| Magnum | 22 dmg, ×2 headshot, 8 mag, hitscan |
| Rocket Launcher | 120 dmg, radius 4 m, 2 shots, **90 s respawn** |
| Frag grenade | 90 dmg centre, radius 5 m, 2 carried |
| Melee | 70 dmg; **rear = instant kill** |
| Grappleshot | 20 s cooldown, 20 m range |
| Sprint | +20% move speed, no cost; ends on fire/melee/grenade |
| Weapon swap | 0.4 s |
| Respawn | 5 s, scored spawn |
| Match | 25 kills or 8:00; sudden death 60 s cap |
| Map sightline cap | 35 m |

## Appendix B — Bot Tuning Schema

`DT_BotTuning`: `profile_id, display_name, accuracy_pct, reaction_ms,
burst_discipline, grenade_policy ∈ {rare, situational, tactical},
cover_preference (0..1), push_threshold (enemy shield %),
rocket_contest (bool), target_switch_bias`. **No profile may set
`reaction_ms` below 200** (superhuman guard, enforced by the schema and
asserted in `Breachpoint.Bots.*`).

## Appendix C — Telemetry Schema

`FBRMatchTelemetry` (per player, per match): kills, deaths, assists,
accuracy per weapon, TTK distribution, shield-break→kill conversion,
grenade kills, melee kills (front/rear), grapple uses and grapple kills,
rocket holds and rocket kills, **fights lost below 40% shields**, medals,
time alive. Consumed by: Spotter (coach lines), the tuning-curator
(TTK/win bands), Combat QA (regression baselines).
