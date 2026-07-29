# BREACHPOINT — Project Roadmap
## The full development plan, structured as a studio of parallel pods

**Companion to:** `GDD-VERTICAL-SLICE.md` (what) · `ARCHITECTURE.md` (how/where) ·
`crew/` (who) — this document is **when, by whom, in parallel**.
**Horizon:** 6 weeks to the Steam demo (Phase 1), then the Phase-2 expansion.
**Method:** researched against how real studios structure this — Halo Studios'
reorg, industry milestone definitions, and Anthropic's multi-agent findings —
then adapted to a one-principal + agent-crew studio.

---

## 0. What the research says (and what we take from it)

**From Halo Studios' own reorganization** (343 → Halo Studios, 2024):
1. **They dropped their custom engine (Slipspace) for UE5** so effort goes into
   the game, not the technology stack. *Our version:* stock engine + FPS
   template + our GAS port; zero engine work.
2. **They split one monolithic team into multiple small project teams** to run
   work in parallel instead of everyone blocking on one critical path.
   *Our version:* six pods with hard owner-path boundaries (below).
3. **"The people making the games make the decisions on the games."**
   *Our version:* each pod's agent owns its domain doctrine; the TD arbitrates
   seams, not keystrokes.
4. **Project Foundry** — they validated the new pipeline with a research slice
   before committing games to it. *Our version:* Week 1 IS our Foundry — the
   validation ladder + GAS core + first shot prove the pipeline before feature
   commitment (M2 is the commit/kill gate).

**From industry milestone practice:** the standard ladder — *prototype → first
playable → vertical slice → alpha (all systems present) → beta/content-complete
(only bugs change) → gold (ship master)* — compressed honestly into six weekly
milestones, each with the industry meaning attached, so "alpha" means what it
means everywhere.

**From multi-agent research** (Anthropic, and our own validation doc):
orchestration design — not agent count — is where multi-agent projects fail;
parallelize only what decomposes cleanly; the human reviewer is the bottleneck,
so WIP must be capped. These three findings shape §4's operating rhythm.

---

## 1. The Studio — six pods + a production office

Each pod = one owner-path cluster + one primary agent + a stated **main goal**.
Pods run **simultaneously**; the tickets board is the only coordination
surface; two pods never write the same file (owner-path law + binary locks).

| Pod | Main goal (the one sentence) | Staff (agents) | Owner paths |
|---|---|---|---|
| **① Combat** | Every verb feels right and every number is provable | sim-builder · tuning-curator | `AbilitySystem/`, `Weapons/`, `Data/`, `Tests/` |
| **② Netcode & Match** | Truth lives on the server and nobody can cheat it | netcode-builder | `Match/`, every replicated surface, `BRGA_Grapple` |
| **③ AI** | Opponents that play like players, deterministically | ai-builder · tuning-curator | `AI/`, `Telemetry/` (Spotter) |
| **④ Experience** | The game reads instantly and sounds like Halo | ui-builder · builder (audio) | `UI/`, `Input/`, cues/MetaSounds |
| **⑤ World** | One arena worth fighting over | arena-architect · builder (integration) · anim-builder (sets) | `BR_Arena01`, `Character/` anim, sourced assets |
| **⑥ Platform & Release** | Always launchable, provably working, one-click shippable | services-builder · verifier · builder (tools) | `Online/`, `Tools/`, Steam depot, CI/ladder |
| **Production office** | The board is true and the seams are arbitrated | game-lead (terminal session) · **Juan (TD — final call)** · critic (floats: review board) | tickets, contracts |

**The critic belongs to no pod** — it is the studio's review board, pulled into
any pod's dangerous-domain packet (netcode, sim math, schemas). **The tuning-curator's
balance duty** activates in Week 5 when telemetry exists.

---

## 2. The Milestone Ladder — six weeks, industry-standard meanings

| M | Week | Name | Industry equivalent | The gate question |
|---|---|---|---|---|
| M0 | done | **Pre-production complete** | greenlight package | GDD + architecture + contracts + tickets exist ✅ |
| M1 | 1 | **Foundry** (First Shot) | prototype / pipeline proof | Can we prove things? Does shooting a dummy feel right? |
| M2 | 2 | **Golden Triangle** | first playable | ⚠️ **THE FUN GATE** — commit or kill |
| M3 | 3 | **The Playground** | vertical slice (mechanics) | Is the grapple + map loop fun to *move* in? |
| M4 | 4 | **First Match** | alpha — all systems present | ⚠️ **GO/NO-GO** — 4v4 vs bots end-to-end |
| M5 | 5 | **Two Boxes** | beta / feature complete | Two humans + six bots, online, full match |
| M6 | 6 | **Steam Demo Live** | gold / ship | A stranger downloads it and gets a kill |

---

## 3. The Roadmap — phase by phase, pod by pod

Format per phase: **main goal**, then each pod's parallel deliverable.
`—` = pod idle or supporting (pods are never blocked waiting; idle pods pull
their next-phase prep or curator work).

### Phase 1 · Week 1 — M1 "Foundry"
**Main goal: prove the pipeline — targets, ladder, GAS core, and one
satisfying shot — before any feature commitment.**

| Pod | Deliverable | Tickets |
|---|---|---|
| ① Combat | ASC port · attribute set · generic-GE library (incl. GE_Death) · damage exec · `BRGA_Sprint` · AR fire vs dummy | BP02 |
| ② Netcode & Match | Reviews ①'s replication settings; Match-frame design prep | BP02 (review) |
| ③ AI | — (prep: StateTree/EQS research spike, tuning schema) | — |
| ④ Experience | Skeleton + **the input layer** (config, component, tags) | BP01 |
| ⑤ World | **Anim pack + environment kit selection** (the Week-1 decision that binds ability timings) · arena brief to curator | — |
| ⑥ Platform | Three targets compile · wrappers · Gauntlet skeleton · specs red→green | BP00 |
| Gate | **Breaking a dummy's shields feels good; all rungs runnable** | |

### Phase 2 · Week 2 — M2 "Golden Triangle" ⚠️ FUN GATE
**Main goal: the complete hand-to-hand sandbox — shoot/grenade/melee — fun
against one scripted opponent, or the project pivots to Concept A while it's
still cheap.**

| Pod | Deliverable | Tickets |
|---|---|---|
| ① Combat | Magnum + 2-weapon carry/swap · equipment (BP03) · grenades · melee + rear-kill (BP05) | BP03, BP05 |
| ② Netcode & Match | Fire-path validation + **the four cheat tests** · match frame (mode/state/scoring/respawn) | BP03, BP04 |
| ③ AI | One scripted target bot (walks, shoots back crudely) for the fun test | prep |
| ④ Experience | Debug HUD (shields/health/ammo real, ugly on purpose) · hit markers (shield vs flesh) | — |
| ⑤ World | Greybox test range · arena manifest v1 from curator | — |
| ⑥ Platform | Rung-4 smoke now asserts damage-in-threes · CI on push | — |
| Gate | **The triangle is fun. Decision meeting: commit / cut / pivot.** | |

### Phase 3 · Week 3 — M3 "The Playground"
**Main goal: verticality — the Grappleshot and the three-level arena turn
combat into movement-combat.**

| Pod | Deliverable | Tickets |
|---|---|---|
| ① Combat | Rocket Launcher (shares projectile/radial with grenades) · power spawner (90 s) | BP09 |
| ② Netcode & Match | **`BRGA_Grapple`** (the netcode packet: RMS through CMC, prediction, REFUTER gate) | BP06 |
| ③ AI | Bot brain v1: StateTree states + EQS cover queries + perception (vs static layout) | BP08 |
| ④ Experience | HUD v1 real (ViewModels wired) · grapple cooldown ring · killfeed | BP10 |
| ⑤ World | **BR_Arena01 blockout landed** from manifest (grapple points, spawns, rocket node) · anim sets integrated on both meshes | BP07 |
| ⑥ Platform | Nightly soak harness ready (bot-vs-bot once ③ lands) | — |
| Gate | **Grapple used offensively in playtest; full match completes solo** | |

### Phase 4 · Week 4 — M4 "First Match" ⚠️ GO/NO-GO
**Main goal: alpha — every shipped system present and connected; a full 4v4
vs bots plays end-to-end, or the cut order executes now.** *(Tickets: BP08
completes; BP10 front-end scope.)*

| Pod | Deliverable |
|---|---|
| ① Combat | Tuning pass from first soak data · spec suite complete |
| ② Netcode & Match | Sudden death + tiebreaks · spawn scoring tuned · host/remote claims separated |
| ③ AI | **Bots complete**: 3 difficulty scalars · slot fill/backfill · determinism suite green |
| ④ Experience | Front-end v1 (menu → match) · death overlay · carnage report |
| ⑤ World | Arena art pass 1 (sourced kit dressing) · lighting |
| ⑥ Platform | **Nightly bot-vs-bot soaks live** (20 matches, seeds logged) · packaged-build sanity |
| Gate | **4v4 end-to-end. If not: execute cut order (rocket → bot tiers → medals → Spotter → menus), protect W5–6.** |

### Phase 5 · Week 5 — M5 "Two Boxes"
**Main goal: beta — feature complete across a real network; two machines,
Steam invites, full match, telemetry flowing.** *(Ticket: BP11 — sessions,
depot, Spotter, CI.)*

| Pod | Deliverable |
|---|---|
| ① Combat | Freeze mechanics; data-only balance via the **tuning-curator** (now live on soak telemetry) |
| ② Netcode & Match | Rung-4 under emulation green on every packet · host-quit behavior defined + tested |
| ③ AI | **Spotter agent** (async, canned fallback first) · telemetry consumers wired |
| ④ Experience | MetaSounds pass (footsteps = the information system) · medals · UI polish |
| ⑤ World | Art pass 2 · performance pass (rung 5 budgets) |
| ⑥ Platform | **Steam listen-server sessions (2 machines)** · depot + app config · demo build pipeline |
| Gate | **Two humans + six bots, online, "run it back" heard unprompted** | |

### Phase 6 · Week 6 — M6 "Steam Demo Live"
**Main goal: gold — only bugs change; the demo ships on Steam and the
capstone presents.** *(Ticket: BP12.)*

| Pod | Deliverable |
|---|---|
| ① Combat | Bug fixes only (pinned suites guard every change) |
| ② Netcode & Match | Final REFUTER sweep on the full replicated surface |
| ③ AI | Bot final tuning from soak + human playtests |
| ④ Experience | Final polish · first-time-user flow (install → kill in 2 min) |
| ⑤ World | Final dressing · performance lock |
| ⑥ Platform | **Steam demo depot LIVE** · overnight soaks green · build reproducibility |
| Production | **Capstone presentation**: the game + the studio-of-agents story |
| Gate | **A stranger installs, joins, kills, rematches — unaided** | |

### Phase 2 (post-course) — the standing backlog, in order
1. **Motion tracker** (most-missed cut; server-computed contacts)
2. **FFA Slayer** (near-free: team assignment off)
3. **Plasma Rifle + damage-type layer** (the kinetic/plasma counter-game)
4. **Dedicated servers on GameLift** (SDK5 behind `IBRServerLifecycle` — the
   swap the whole architecture pre-paid for) + Steam auth validation
5. **Firefight PvE** (wave/threat-budget design imported from the Gauntlet
   draft) · 6. Server-side rewind · 7. Second arena · 8. Bot tiers as
   distinct StateTrees · 9. Ranked/quickmatch — *then* the full Concept-B GDD
   is the product roadmap.

---

## 4. The AI-Studio Operating Model (how we actually run in parallel)

This is the part no traditional studio doc covers — ours runs on **concurrent
Claude sessions coordinated only through git + tickets**:

- **The lead session** (Claude terminal, `game-lead` skill): decomposes,
  dispatches, arbitrates, lands. One at a time.
- **Pod work** = tickets executed by crew agents (subagents inside a session,
  or parallel cloud/terminal sessions each claiming a ticket with a STATUS
  line — the claim-commit makes double-pickup impossible).
- **Juan (TD)**: reviews packet diffs, makes seam calls, owns Perforce/Steam
  credentials, plays every gate build. **The TD is the throughput ceiling** —
  which sets the WIP limit below.
- **The rhythm (per working session):** `git pull --rebase` → `/tickets list`
  → claim → execute per contracts → ladder → log findings in the ticket →
  push. Nightly: soaks run; morning: verifier report is the standup.

**Three rules from the research, enforced:**
1. **WIP cap: max 3 heavy packets in flight** (TD review is the bottleneck;
   more WIP = queue, not speed).
2. **Parallelize only what decomposes** — cross-pod refactors are lead-session
   serial jobs, never fan-outs.
3. **The board is the truth** — a decision that lives only in a chat is lost;
   it goes in the ticket Log or it didn't happen.

**Dependency spine (the critical path):**
`BP01 skeleton → BP02 GAS core → BP03 weapons → (BP04 match ∥ grapple ∥ bots)
→ online → ship.` Everything off-spine (UI, audio, arena art, anim
integration, tools) floats — that float is what the pods spend.

---

## 5. Risk Register (roadmap-level)

*(Gate thresholds, perf/net budgets, the definition-of-done card, the playtest
protocol, and the Steam ship checklist live in `QUALITY-BARS.md` — the verifier
reads that doc for rung 5.)*

| Risk | Phase | Mitigation |
|---|---|---|
| Schedule (~6.3 w compressed into 6) | all | Cut order pre-declared and *expected to be used*; M4 is the execution trigger |
| Fun gate fails | W2 | Concept A's shipped foundation is the pivot; M2 exists to make that cheap |
| Grapple prediction | W3 | Netcode packet + REFUTER + rung-4 emulation before landing |
| TD review bottleneck | all | WIP cap 3; curators produce data (cheap review); specs make diffs self-evident |
| Anim pack mismatch | W1 choice | Timings authored TO the pack; decision deadline is Friday W1 |
| Steam/OSS friction | W5 | Sessions code is a port, not new; PIE-vs-Steam rung honesty keeps claims real |
| Soak findings flood W5 | W4–5 | Soaks start W4, not W5 — one week of drain-down before beta |

---

## 6. What "done" means (the studio's exit criteria, restated)

A stranger installs the Steam demo and gets a kill inside two minutes,
unaided. Playtesters grapple offensively and run it back unprompted. Every
arena decision, bot value, and weapon number traces to a named agent output a
human reviewed. And the capstone presents both artifacts: **the game, and the
studio that built it** — six pods, twelve agents, one principal, six weeks.
