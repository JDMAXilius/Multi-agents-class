# BREACHPOINT — Project Overview

**A multiplayer arena first-person shooter in Unreal Engine 5.8, built by a crew of AI
agents under a human director, with every claim graded against the evidence that supports
it.**

Final submission document. Written 3 September 2026 for review.

---

## 0. How to read this document

Two things are on trial here, and the document keeps them separate:

- **The game** — a playable arena FPS with a bot opponent that plays by the same rules a
  human does.
- **The method** — a multi-agent development process where AI agents write, review and
  verify each other's work under written law, and where **the honesty of a claim is
  itself an artifact**. §6 summarises it; **`docs/HOW-THIS-WAS-BUILT.md` is the full
  account** — the two attempts, the crew, the waves, the MCP-first editor policy, and
  where the method failed.

Every capability below carries the rung it has actually reached. The project uses a fixed
ladder, and no statement in this document is allowed to skip it:

> **written → compiles → automated test passes → runs in-editor (PIE) → runs in a packaged
> build.**

"Compiles" is not "works." "PIE" is not "shipped." Where something is unproven, this
document says so — that is the point, not a caveat. A section that claims less than you
expected is a section being accurate.

---

## 1. What the game is

A 4v4-scale arena shooter in the Halo tradition: **shields over health**, a readable
time-to-kill, and movement verbs that make positioning matter more than raw aim.

One player, up to fifteen bots, on a single machine. Pick a map, pick Free-For-All or Team
Deathmatch, pick how many bodies are in the match, and play. There is no online
matchmaking — the architecture is server-authoritative throughout, so the game runs as a
listen server with one local player, and adding real clients is a networking task rather
than a rewrite.

**Player verbs:** move, look, jump, crouch, sprint, fire, aim down sights, reload, melee,
grenade, weapon swap (next/previous), lean left/right, **grapple**, **dash**.
Full control table: `docs/HOW-TO-PLAY.md`.

**The two signature verbs.** The *grapple* fires a hook and pulls the player to it — the
fast route to high ground, across a gap, or out of a losing fight. The *dash* is a short
burst that breaks an opponent's aim or clears a blast radius. Both are on cooldowns, and
**the bots use both through the same interface the player does** (see §3.4).

**Survival loop.** Shields absorb damage first and recharge after a quiet moment; health
does not. Breaking contact is therefore a real tactic rather than a retreat, and it is why
the AI has an explicit *Retreat* ambition with a defend band (§3.2).

**Match rules.** First to the score limit, or the highest score when the clock runs out.
Both are chosen in the menu before launch: score cycles **10 / 20 / 30** (default 10) and
the clock cycles **5 / 10 / 15 / 20 minutes** (default 10). `DefaultGame.ini` carries a
separate `ScoreLimit=7`, which applies only to a boot that passes no URL option — the menu
always passes one, so 7 is not what a player meets.

**Modes.** Free-For-All puts everyone on `NoTeam`; Team Deathmatch splits into two sides
with automatic balancing. One combat codebase serves both — FFA is simply "everyone is on no
team", which removes an entire class of mode-branching bugs.

**Content:** three weapons (AR, Magnum, Rocket) defined entirely in a data table; eleven
medals (Double Kill, Killing Spree, Grapple Kill, Blast Radius, Blindside, Headshot, Area
Denial, Denial, Breach, Last Word, Spree Ender); seven maps, of which two are the shipping
arenas (**BR_Spillway**, **BR_Arena01**) and the rest are a front-end stage, two research
blockouts and two instrumentation gyms.

---

## 2. Player systems and gameplay architecture

**Gameplay Ability System (GAS), used strictly.** Every action a body can take is a
GameplayAbility: fire, reload, ADS, melee, grenade, grapple, equip/swap, hit-react, death,
and the movement set. Attributes (health, shield, grenades, move speed, sprint and ADS
multipliers) mutate **only** through GameplayEffects — never by direct assignment. Costs
and cooldowns are GEs; states are GE-applied tags; all effects are GameplayCues.

**One damage pipeline.** The engine's `TakeDamage`/`ApplyRadialDamage` are *banned* project
-wide. Damage flows through a single authored path, so falloff, body-section multipliers,
headshots and shield-then-health ordering are resolved in exactly one place and cannot
disagree with what a weapon's data table says.

**Server-authoritative by construction.** Clients send intent; the server decides. Every
`Server` RPC has a real `_Validate`. The bot presses the *same input tags* a human's
controller presses, so there is no second, privileged AI code path to keep in sync — a
design decision that pays off repeatedly in §3.

**Data is not code.** Tuning numbers live in CSV data tables (`DT_Weapons`, `DT_Medals`,
`DT_BotTuning`, `DT_BotAmbitions`, `DT_MatchRules`, `CT_Combat`); asset references in data
are soft. Retuning a weapon, a bot tier or a medal is a spreadsheet edit, not a compile.

**Zero Blueprint gameplay classes.** All gameplay logic is native C++. Blueprints exist
only where UE offers no C++ path (animation graphs, materials, Niagara, widget layout), and
even those are generated by committed scripts or built through the editor's automation API
rather than hand-placed — so the project is diffable and reviewable as text.

---

## 3. The AI opponent — the project's centrepiece

The bot is a **self-contained UE plugin** (`Plugins/AIBot`, ~21,700 lines across 100 files)
that depends on engine modules only. It cannot name a game class, an ability, or an
attribute — the boundary is enforced by the linker, not by discipline. The game supplies
its own thin adapter; the AI module never learns what is behind it.

### 3.1 The decision architecture

```
Perception ─► Sensorium ─► FACTS ─► AMBITION (utility) ─► TACTIC (utility) ─► ACTION
 (sight/                  (matured    Roam · Engage ·      Push · Flank ·     (StateTree
  sound/                   beliefs)   Retreat · Search ·   Hold · Explore      task →
  damage)                             Rally · Objective                        verb press)
                                │
                                ▼
                        TEAM MIND (per team, server-only)
                        · shared last-known enemy positions, decaying
                        · target claims with a cap and hysteresis
                        · visit + route heat grids
```

Two utility layers, one predictable execution layer. Utility scoring decides *what to want*
and *how to go about it*; a StateTree executes, so behaviour stays debuggable. Every weight
is data; every random draw comes from a seeded stream keyed on the match seed and bot
index, so a headless replay repeats exactly.

### 3.2 What the bot actually does

- **Perceives** through a sensorium with a reaction clock: sight, sound and *being shot*
  all arrive as stimuli that mature over a human-scale delay before the brain sees them.
- **Chooses a target** by score, not by recency: proximity, visibility, who-is-shooting-me
  (decaying), memory freshness, plus an incumbent bonus and a switch margin so it commits
  to a fight instead of oscillating between two enemies at similar range.
- **Coordinates**: a team mind caps how many bots pile onto one target, shares sightings as
  callouts that enter each listener's *own* reaction clock, and biases routes so teammates
  do not conga-line down one corridor.
- **Separates**: crowd-following avoidance so teammates stop shoving each other.
- **Fights**: aim with drift and settle rather than snap; grenades on a recognition ladder
  (opener / finisher / area denial); melee at a recognition range; weapon choice scored by
  expected damage-per-second at the current range, so the "shotgun is the close-range gun"
  rule is *emergent from the weapon table* rather than hardcoded — the word "shotgun"
  appears nowhere in the AI.
- **Traverses**: jump, dash, grapple or step off a ledge, chosen by geometry when the
  navmesh has no path.
- **Retreats**: breaks contact to a defend band and then *stands and fights* rather than
  jogging away with its back turned.
- **Comes in four difficulty tiers** (Recruit / Marine / ODST / Spartan) that differ only
  in data — six competence axes plus a perception envelope. Nothing in the brain, the tree
  or the tasks knows which tier it is running, which is what makes the setting safe.

### 3.3 FAIRPLAY — the AI's constitution

The bot is governed by nine written laws, enforced in code and checked by review:

| | |
|---|---|
| **F1** | A reaction floor — no bot responds faster than a human could |
| **F2** | Stimuli mature, never teleport |
| **F3** | The brain sees only what the sensorium admits |
| **F4** | Aim drifts and settles; it never snaps |
| **F5** | Memory decays — a bot that loses you must search |
| **F6** | Verbs only — the bot acts through the player's own input surface |
| **F7** | Failure is visible — a refused path or verb is logged, never hidden |
| **F8** | Raw engine perception is quarantined behind the sensorium |
| **F9** | Motion is the default — a still bot must name why it is standing |

These are not aspirations. F6 is why there is no AI-only code path. F9 turned into a
measurable instrument: every deliberate stand (reloading, holding a position, yielding to a
teammate, defending) carries a *name*, and standing with no name is treated as a defect and
counted.

### 3.4 What "the bot plays fair" means concretely

The bot presses `Input.Weapon.Fire` on the same ability system component a player's mouse
click reaches. It has no access to enemy health, no perfect knowledge of position, no
instant reactions and no privileged movement. When a human is added to the match, **the
same code runs**. This is the single design decision that makes every other AI claim in
this document checkable.

---

## 4. User interface

Built on **CommonUI** with an **MVVM** data layer, native C++ throughout.

- **Layered stack**: four activatable layers (Game / GameMenu / Menu / Modal) per local
  player, so a modal *pushes* rather than hiding the HUD, and back-navigation works without
  bespoke code. Split-screen-correct by construction — the UI is owned per local player,
  never globally.
- **ViewModels** feed the UI by event, never by per-frame polling.
- **Screens**: main menu, play setup (map / mode / players / score / time), in-match HUD
  (shields-and-health vitals, match band, kill feed, loadout tray, reticle), scoreboard,
  death screen, pause, and a post-match player recap.
- **Built to measured geometry.** The HUD and the menu were reproduced from the design file
  node by node, against a committed "referee" document that records every box with the
  design node id it came from — so a layout dispute is settled by re-reading the source,
  not by opinion.

---

## 5. Verification — how anything here is known

This is where the project differs most from a typical submission.

**Automated tests: 30 suites**, most of them *headless* — they run in seconds with no
editor and no world, because the logic they cover was deliberately written worldless. They
include the bot's aim, grenade, melee, movement, weapon and traversal policies; target
selection and claims; the ambition engine; the sensorium's fairness rules; team mind;
separation; route bias; and the damage, teams and killfeed logic on the game side.

**Headless match batches.** The bot is measured by running 5 × 300-second matches per map,
seeded, and parsing the structured log into per-bot metrics: idle seconds, stuck seconds,
path refusals, stalls, sweeps, claims, overlaps, route changes, kills per minute. Successive
runs (v1…v7) are committed as JSON baselines, so a change is judged against the previous
run rather than against an impression.

**A worked example of the method, because it is more honest than a summary.** In the most
recent run, a change of mine intended to reduce bot idling *did* reduce it — and the report
correctly separated the part that was a real behaviour fix from the part that was merely
bookkeeping (naming a stand that had always been deliberate). Reading the committed
baselines afterwards showed a fourth thing nobody had claimed: a metric had regressed 4–6×
two runs *earlier*, and the run everyone was auditing had actually improved it. That
correction only exists because the numbers were committed, versioned and re-readable.

**Evidence in the repository:** 1,172 commits; 51 open and 30 archived work tickets, each
carrying its own log of decisions, measurements and refuted hypotheses; and a packaged
Windows build that boots to the menu, plays a match against bots and returns to the menu.

---

## 6. The development method — a crew of AI agents under law

The course subject is multi-agent systems; this project is one, and the method is a
deliverable in its own right.

> **This section is the summary.** The full account — why the project was built twice, how
> the two Claude sessions used tickets as their protocol, the MCP-first editor policy, the
> Figma-to-Unreal UI pipeline, and an honest reckoning of what the method cost and where it
> failed — is **`docs/HOW-THIS-WAS-BUILT.md`**.

**Separation of powers.** Nineteen specialist agents, each with a written definition and
an *owner path* it may write inside: sixteen top-level roles — builders (simulation,
netcode, UI, animation, AI, and two AI-plugin builders), critics, verifiers and editors —
plus three content curators (arena architect, spotter, tuning curator). A builder that needs a change outside
its path does not make it — it files a **contract gap** and stops.

**Work packets and waves.** Work is dispatched as packets against tickets. Larger efforts
run as *waves*: `W-AUDIT` (read-only, one question per agent) → merge → `W-BUILD` (parallel
agents on disjoint files) → `W-REVIEW` (four independent critics: containment, fairness,
pathologies, server-only) → `W-VERIFY` (specs plus a measured headless batch against the
previous baseline). Only a `high`-severity finding blocks a landing; everything else lands
in a risk register with the artifact.

**The laws are hooks, not vibes.** Eight project laws (server authority, GAS purity, data
-not-code, no gameplay tick, owner paths, the honesty ladder, generated assets, closed
design rulings) are partly enforced *mechanically* — a pre-tool-call hook blocks banned
APIs and out-of-path writes as they are attempted. A blocked write is the law firing, not an
obstacle to route around.

**Memory that survives.** Agent context windows do not persist; the repository does. Every
decision, measurement and refuted hypothesis goes into its ticket's log, so a hypothesis
disproved in August is not re-tested in September. Design rulings are recorded and *closed*
— reviews judge against the ledger and never re-litigate it.

**The honesty ladder as culture.** The ladder in §0 is enforced socially and in review:
claims name their rung, multiplayer claims come in threes (server, acting client, observing
client), and an agent that cannot prove a thing says so. Several entries in this document
are less impressive than they could have been written, for exactly that reason.

**Course artifacts.** Ten assignments feed this project directly: the GDD, the agent crew,
a content pipeline, a goal-oriented agent, a generate–evaluate–refine pipeline, a style
-guide agent, a narrative engine, an adversarial QA agent, and the end-to-end AI dev
pipeline — whose generated bot callsigns appear on the scoreboard of the packaged build.

---

## 7. Technical facts

| | |
|---|---|
| Engine | Unreal Engine 5.8, native C++ |
| Modules | `Breachpoint` (legacy/UI components), `BreachpointNext` (current gameplay), `AIBot` (plugin) |
| Source scale | ~97,000 lines across 485 C++ files |
| AI plugin | ~21,700 lines, 100 files, engine-only dependencies |
| Frameworks | GAS, Enhanced Input, StateTree, CommonUI, MVVM, Navigation/Recast, Crowd following |
| Networking | Server-authoritative; listen-server topology |
| Automated tests | 30 suites, majority headless |
| Data tables | Weapons, medals, bot tuning, bot ambitions, match rules, combat curves |
| Build targets | Editor, Game, Server |

---

## 8. Known limitations, stated plainly

- **No online multiplayer.** The architecture is server-authoritative and the code path is
  the real one, but no session/matchmaking layer exists. It is single-machine against bots.
- **Gamepad is unverified.** The full layout is generated and audited, and the menu is
  structurally navigable, but no controller has been held. Ticketed, not hidden.
- **Two of the AI's phases remain open** against their own hard bars — bot idling and flank
  completion — with the causes diagnosed and the fixes written but not yet re-measured.
- **Two maps are shipping quality**; the others are blockouts and instrumentation gyms.
- **Some recent work compiles but is not yet proven in play.** It is marked as such in the
  tickets, and in this document.

None of these are discoveries made while writing this section. Each was already recorded in
a ticket before it was written here, which is the only reason the list can be trusted.

---

## 9. Where to look in the repository

| To see | Read |
|---|---|
| How to play | `docs/HOW-TO-PLAY.md` |
| The AI's constitution | `Plugins/AIBot/Source/AIBot/FAIRPLAY.md` |
| The AI's architecture and roadmap | `docs/AIBOT-ROADMAP.md`, `docs/AIBOT-ROADMAP-2.md` |
| The project's laws | `CLAUDE.md`, `docs/contracts/`, `docs/DESIGN-RULINGS.md` |
| **How the project was actually built** | **`docs/HOW-THIS-WAS-BUILT.md`** |
| The crew method | `docs/CREW_PLAYBOOK.md`, `docs/AIBOT-WAVES.md`, `.claude/agents/` |
| Measured AI evidence | `Tools/aib/baselines/*.json`, and each `docs/tickets/TICKET_AIB*.md` log |
| UI layout ground truth | `Source/BreachpointNext/UI/Content/BN/UI/Assets/00-HUD-MEASURED.md`, `01-MENU-MEASURED.md` |
| The work itself | `docs/tickets/` — 80 tickets (50 live, 30 archived) with their decision logs |
