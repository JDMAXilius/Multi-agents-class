# TICKET — AIB22: Phase 11 EGRESS — no bot stands on a platform sweeping; roam the whole level

> STATUS: open, VERIFIED 2026-09-03 — the idle HARD bar (0) is not met: idle tactic=none 25.6 s Spillway / 44.9 s Arena01 per bot per 300 s (baseline 72 / 199), refusals 0, stalls ≤ 4 s, kills/min 11 / 14 (baseline 1 / 1.8); three named follow-ups in the Log close-out. Earlier: in-progress — mac terminal (lead, session 014esNfHwPnkiAJkRKBMwR7b) 2 Sep 2026 (061f8f82). Founder rulings and law F9 in `docs/AIBOT-ROADMAP-2.md` §5. Waves per `docs/AIBOT-WAVES.md`.
> and law F9 (motion is the default) in `docs/AIBOT-ROADMAP-2.md` §5. Waves per `docs/AIBOT-WAVES.md`.

Founder: a bot on a raised platform with no path off it turns in place "looking for a target".
Wanted: roam the whole level through random points until a target is seen, then go for it; step
off platforms (drop/jump), climb back up when needed; NEVER stand still outside a named tactic.

**Ordering law:** metrics land FIRST (a baseline before any behaviour changes); nav links land
serial (editor is global); the path-following jump hook is a serial header before the Egress
tactic; EQS roam last (it depends on the visit-heat grid, a Phase 12 Team-Mind member that this
packet introduces in its minimal form).

## Kickoff (machine-checkable)
- requires: engine-installed (C++, headless runs) · editor-live for the nav-link and tree steps
- `docs/AIBOT-ROADMAP-2.md` exists with §5 rulings (it does)
- `Tools/aib/80_aib_metrics.py` runs against a current `LogAIBot` (verifier confirms in W-AUDIT)
- owner_path (per writer, disjoint):
  - aib-builder: `Plugins/AIBot/Source/AIBot/`
  - aib-editor: `Content/AIBot/`, `Tools/aib/`, `Tools/blockout/` (nav-link placement scripts)
  - lead: `Config/DefaultEngine.ini` (RecastNavMesh block only), `docs/tickets/`, `docs/AIBOT-*.md`
  - NOT touched: `Source/Breachpoint/`, `Source/BreachpointNext/` (adapter changes = their own packet)

## Waves
- W-AUDIT ×3 (2 Sep): aib-critic "tree on an island + minimal fair Egress" · arena-architect
  "which platform edges lack links, coordinates" · aib-verifier "what to log, how to run, baseline".
  Merged findings: see Log.
- Serial: metrics lines (aib-builder) → baseline runs (aib-verifier).
- Serial (editor): nav-link placement scripts + regen (aib-editor), ini envelope (lead).
- Serial header: `UAIBPathFollowingComponent` jump hook (aib-builder).
- W-BUILD ×2 (disjoint files): Egress tactic + island fact ∥ sweep budget + sweep-while-moving.
- W-REVIEW ×4: containment · fairness · utility pathologies · server-only.
- W-VERIFY ×2: `AIBot.Sim.*` specs ∥ headless seeded 4v4 metrics vs baseline; PIE watch of a
  top-platform spawn leaving within 5 s.

## Steps (in order) — refined at the audit merge
1. Metrics: `stuck_seconds`, `no_path_requests`, `sweep_seconds`, `idle_seconds`,
   `island_egress_count` as structured `LogAIBot` lines + parser + gate; five-run baseline.
2. Nav: `NavLinkProxy` per uncovered platform edge from the blockout scripts; `BN_Drop`
   envelope to the measured gaps; regenerate; count generated + authored links.
3. Path-following jump hook (`SetMoveSegment` on a jump area → JUMP verb) — the only place a
   traversal verb fires from a path.
4. Island fact + Egress tactic under Roam; SweepLook gets a budget and moves while it sweeps.
5. Roam over the whole level: EQS pathing grid scored by visit recency (team visit-heat grid).
6. Review wave, verify wave, log, push.

## Done when
- [x] Baseline report (5 runs, medians + spread) committed under Tools/aib/ before step 2 — `Tools/aib/baselines/aib22-*-v2.json`
- [x] Every platform on Spillway and Arena01 has a way down; the floor has a way up (link count) — tiers/stairs PASS at 19 uu cells (3 Sep); gantry/core tops = Egress or AIB28
- [ ] `idle_seconds == 0` outside named tactics; stationary sweep: longest single ≤ 2.5 s and
      ≤ 5 % of the match per bot (re-based 2026-09-02, W-REVIEW M5); `stuck_seconds` per bot
      under the gate the verifier proposes; kills/min not worse than baseline
- [ ] PIE: a bot placed on the top platform leaves it within 5 s (captured)
- [ ] W-REVIEW: no `high`

## Log
### W-AUDIT member 3 (aib-verifier) — metrics + headless protocol (2 Sep)
- Existing `LogAIBot` lines carry no match time and none of the five metrics sums to seconds
  today. Refusals are partly derivable (`f7` regex on Log-level `cannot path…` lines with
  `DescribeMoveFailure`); the belief/POI refusals and wedge re-issues are Verbose or silent.
  SweepLook logs nothing. No idle line exists. Egress: nothing (AIB19 grapple lines are the
  nearest).
- Proposed lines (one emit site each, `t=` = World seconds): `move REFUSED` in `MoveToNavPoint`;
  `stall over — Ns … jumped= resolved=` on the wedge resume branch + ReleaseLocomotion;
  `sweep over — Ns, moved Nuu, state=` in SweepLook ExitState; `idle over — Ns state= tactic=`
  from the controller's 0.1 s Think sampling `GetLastMovementInputVector`; `island egress — via
  drop|link|jump|grapple … after Ns stranded`. `idle_seconds` = idle-over with `tactic=none`.
- Headless 4v4 (no human): `UnrealEditor-Cmd <uproject> "/Game/Maps/BR_Spillway?TargetPlayers=8
  ?Teams=1?TimeLimit=300" -server -nullrhi -unattended -nopause -nosplash -log -SECONDS=330
  -FixedSeed -BENCHMARK -FPS=60 "-ini:Game:[/Script/BreachpointNext.BNGameMode]:MinPlayers=0"
  -LogCmds="LogAIBot Verbose" -abslog=Tools/Logs/aib22-base-<map>-<n>.log` (ReadyToStartMatch
  counts humans; MinPlayers=0 is required). Fallback proven on this Mac: `-game -windowed`.
  NO match seed exists in AIB (LifeSeed = HashCombine(UniqueID, LifeIndex) + global FRand);
  `-FixedSeed -BENCHMARK -FPS=60` is the nearest; reproducibility UNMEASURED (diff the
  `ambition ->` lines of two runs). Wall-clock unmeasured.
- Baseline: 5 runs × 2 maps → `Tools/aib/baselines/aib22-<map>-<date>.json` via
  `80_aib_metrics.py --json` (script needs the five regexes + medians). Gates: `idle_seconds
  (tactic=none) == 0` HARD; `sweep_seconds == 0` HARD; `stuck_seconds ≤ 10`/bot and no stall
  > 3 s PROVISIONAL; refusals per bot ≤ ½ baseline median; egress report-only + PIE "stranded
  ≤ 5 s"; kills/min ≥ baseline median − spread. Rungs: specs = 2, headless = 3, nothing = 4
  until Phase 13's listen+client.
### W-AUDIT member 1 (aib-critic) — the island today, minimal fair Egress (2 Sep)
PREMISE CORRECTED: `MoveToLocation` defaults `bAllowPartialPath=true`, so an unreachable goal on
a connected mesh does NOT refuse — it returns a PARTIAL path to the island edge and succeeds.
Refusals are only the off-mesh-self case (`self=NO`: mid-fall, knockback, fresh spawn). Actual
island sequence: Search enters → MoveToLastKnown gets a partial path → SweepLook (sibling, from
tick 1) claims the yaw so the bot crab-walks to the edge spinning → at the partial end the wedge
watchdog hops every ~1.5 s and re-issues the same partial path → GiveUpAfterNoProgress 8 s →
Search Fails → 0.5 s dead standing → Root re-selects Search (memory fresh, nothing calls
`Memory().Forget()` on failure) → 8.5 s cycle until memory ages → Roam draws
`GetRandomReachablePointInRadius(Pawn, 2500)`, reachability-constrained, so every draw lands on
the island: small platform = draw inside acceptance → Succeeded → redraw forever; big platform = a
private patrol. The AIB19 grapple drop is dead in practice (needs a route, Movement≥Trained, a
0.5 roll, and picks the nearest route by EITHER end, unbounded — often not on this island).
Engine facts: `TasksCompletion=Any` (any task Failed ⇒ state Failed); completion transitions
RECREATE task instance data (`CompletedTransitionStatesCreateNewStates`), so any budget kept in
instance data resets on every re-entry — the existing 30 s traverse cooldown is dead for this
reason and its "re-entry must not re-arm" comment is false.
Findings, ranked:
- H1 sweep budget must live on the controller (or better: END THE WANT — call `Forget()` so
  Search loses utility and Roam wins), never in instance data.
- H2 a SweepLook that returns Failed kills the whole Search state (Any completion): expire into a
  no-op with the yaw RELEASED; F9 is met when the sweep stops owning the yaw — sweep = a bounded
  pan around the travel heading while walking; a stationary sweep only if bounded, or none.
- H3 the ally-fight wander draw is reachable from the HEARD point, not from the pawn → partial
  path → 8 s stall → Roam Fails → 2 s dead standing. An `idle_seconds==0` gate fails here even
  before Egress; fix the draw (from the pawn) in Phase 11.
- M4 Roam has CommitSeconds 0 by ruling (P2 H-1) → an Egress tactic under Roam inherits zero
  commitment; its commit must be re-derivable from the world (at the lip / falling / off-mesh),
  not instance state.
- M5 island-fact thrash: latch the CLEAR on N consecutive pumps; Egress survives until grounded on
  a different island component; `StopMovement` in ExitState mid-fall must not cancel the drop.
- L6 `GetGrappleRoute` is unbounded map knowledge (F3-tolerable) — Egress must pick an edge ON
  MY island, from geometry I stand on; do not copy the precedent.
Design: island fact = "three consecutive reachable draws all within X uu of me" (free, gives the
island radius); `TestPathSync` only behind that signal (unreachable = exhaustive = expensive).
Egress = nearest edge of my island (own-tile boundary or an authored `NavLinkProxy` whose start
poly is my island) → walk to the lip → the fall IS the move (reuse AIB19's step-off:
`bUsePathfinding=false, bProjectDestinationToNavigation=false`); never project the landing.
Jump verb from `UAIBPathFollowingComponent::SetMoveSegment` only when the segment is a link/jump
area — the watchdog's blind hop and its partial re-issue go. Fairness: island boolean FAIR,
own-island edge FAIR, walk+jump+gravity FAIR (F6); the visit-heat grid must record MY TEAM's
visits only (enemy presence in it = a wallhack). Handoff: the SweepLook AreaDenial throw's
alignment gate touches this change; `GetGrappleRoute` needs its own fairness pass.
### W-AUDIT member 2 (arena-architect) — edges vs the link envelopes (2 Sep)
- Both maps derived from their generators (`arena_manifest.json`, `gen_spillway.py`; umaps are
  binary). Largest horizontal gap anywhere: 200 uu (Arena01 gantry→drum). Largest drop from a
  spawnable surface: 800 (gantry→ground; the 893 capsule-to-ground number is the AIB9 "~880"
  cluster, fixed by the 1000 cap). Every Spillway inter-tier drop is 400 at gap 0; the only
  non-zero gap is crown→T2 (100). **No edge on either map exceeds BN_Drop.** Do NOT widen
  JumpLength (AIB8 measured 600 as 11× worse).
- UP: no rise on either map is jumpable (100–1200 vs a 70 link / 90 body ceiling); the way up is
  ramps/treads or the 8 grapple routes (GP1–GP8 coordinates listed in the agent report).
- THREATS TO EGRESS THAT ARE NOT LINKS: (1) BN34 defect 3 — every ramp head on Spillway (16) and
  both Arena01 stair heads abut their deck at an exact 0.00 coplanar top; on Aquarius 6/8 such
  ramps failed to merge nav islands. If that holds, Spillway's whole UP route is broken and no
  proxy fixes it. Generator fix: head overlap ≥50 uu into the deck, top sunk 3–5 uu; test on the
  3-piece isolation map FIRST. (2) Yard ramps overhang the shaft mouths (200×200 open pit at each
  ramp foot; one-number fix: yard ramp x0 −2200 → −2000). (3) Nav bounds: rev D manifest says
  9200×8400×3200 vs BN37's 8400×6400×3000 — confirm the live volume before any link read.
- NavLinkProxy set = INSURANCE ONLY, placed where the editor link-draw shows a miss: Arena01
  gantry E/S edges (→ drum 400 / → ground 0), spire S edge; Spillway crown N → T2, T2 N → ground,
  gallery S → ground (coordinates in the report). All simple links, down only.
- The AIB9 300–499 band: arithmetic says gantry(893)→mid(410–488) and mid→ground are the only
  pairings; hypothesis = the gantry's arc landed on the then-solid stair-box top (a coplanar
  island), so the drum got no link — the same stranded population as the 880 cluster. Decisive
  test: log the bot's own z on every refusal and bucket by (bot tier, goal tier).

### MERGE (lead, 2 Sep) — contradictions named, steps refined
CONTRADICTION 1 (roadmap §0 vs critic): islands do not REFUSE moves; partial paths SUCCEED to the
edge. The spinning is: sweep owning the yaw while walking + the 8 s partial-path stall/hop cycle +
Search re-entry with fresh memory + Roam's reachable-only draws staying on the island. Refusals
are the off-mesh-self population (AIB9 §2) — a separate defect, still owed its measurement.
CONTRADICTION 2 (roadmap Phase 11 "place NavLinkProxy at every edge" vs architect): the drop
envelope already covers every edge; the real nav risk is island MERGE at coplanar ramp heads
(untested) plus a generator overhang. Proxies become insurance, placed only where the link-draw
shows a miss. Nothing in Phase 11 widens JumpLength.
AGREEMENT: metrics first (verifier), the jump verb belongs to path-following on link segments
(critic + research), the sweep must not own the yaw (critic + F9), the heard-point wander draw
trips the idle gate on its own (critic H3), the heat grid is team-visits only (critic).

Refined steps (replace §Steps):
1. Metrics (IN FLIGHT: aib-builder emit sites ∥ aib-editor parser). Add the bot's own z to every
   refusal line (architect's decisive test) — folded into the same builder packet.
2. Baseline: 5 × 2 maps headless per the verifier's command; medians + spread; `-FixedSeed`
   reproducibility diff; commit under Tools/aib/baselines/.
3. Nav (serial, aib-editor + lead): (a) 3-piece isolation map for the coplanar-merge hypothesis;
   (b) if confirmed, generator fix (ramp head overlap ≥50, top sunk 3–5) + yard-ramp x0 fix +
   regen both maps; (c) confirm the live nav bounds; (d) link-draw walkthrough; proxies only
   where missing. No JumpLength change.
4. Serial header (aib-builder): `UAIBPathFollowingComponent::SetMoveSegment` presses the JUMP
   verb only on link/jump-area segments; remove the watchdog's blind hop + partial re-issue.
5. W-BUILD ×2 (disjoint files):
   (A) Search/Sweep: mover failure calls `Memory().Forget()` (ends the want); sweep = bounded pan
       around the travel heading while moving, no yaw claim while moving; any stationary sweep is
       bounded by a CONTROLLER-held budget (instance data resets on re-entry) and expires into a
       no-op with the yaw released.
   (B) Roam/Egress: wander draw from the PAWN (H3); island fact = 3-consecutive-draw latch (+
       optional gated TestPathSync); `Egress` tactic under Roam — nearest edge OF MY ISLAND →
       lip → reuse AIB19's step-off (fall is the move, never project the landing); commit
       re-derived from the world (at lip / falling / off-mesh); ExitState never cancels a fall.
6. EQS roam across the whole level: pathing grid scored by visit recency against a TEAM-ONLY
   visit-heat grid (the Team Mind's first member).
7. W-REVIEW ×4 (add: SweepLook AreaDenial alignment gate; `GetGrappleRoute` unbounded read).
8. W-VERIFY ×2 vs baseline; PIE watch: top-platform spawn leaves within 5 s (captured).
- STEP 4 AMENDED (from the Phase 13 audit, 2 Sep): the jump hook is NOT a `SetMoveSegment`
  PathPoints read (dead under Detour Crowd in Phase 13). `UAIBPathFollowingComponent` overrides
  `StartUsingCustomLink(INavLinkCustomInterface*, const FVector&)` → press JUMP verb, and for
  jump AREAS reads the corridor (`NavMeshPath->PathCorridor[PathStartIndex]` + `GetLinkEndPoints`).
  Built once, survives the crowd rebase.
### Step 2 — BASELINE (aib-verifier, 2 Sep, rung 3 headless `-game` fallback; JSON in Tools/aib/baselines/)
- The audited `-server` form starts and ENDS the match at frame 0 with 0 bots possessed (match
  over, tie, 1.2 s) — bot fill does not happen without a human in `-server`. Game-side defect,
  lead packet later. Fallback `-game -windowed -nullrhi` = 7 bots + 1 idle human, 4/4 teams,
  ~30 s wall per 300 s match under -BENCHMARK. 7/10 runs hit the 300 s limit; 3 ended on
  ScoreLimit=7 (~110 s) and the parser stamps 300 s for them (kills/min understated ~2.6×).
- `-FixedSeed -BENCHMARK -FPS=60` does NOT reproduce a match (first divergence at ambition line 5).
  Phase 14's MatchSeed + BotIndex is required for any replay claim.
- Spillway medians (per bot, 300 s): idle 72 s (worst 199), sweep 30 s (worst 76), stuck 194 s
  (worst 250), max stall 9 s (worst 32), refusals 70/bot, egress 0, kills/min 1.0.
- Arena01 medians: idle 199 s (worst 305 = the whole match), sweep 46 s (worst 135), stuck 198 s
  (worst 302), max stall 8 s (worst 33), refusals median 0 but WORST 34,072 (Ctrl_4 log 4: goal
  21 uu below, both ends on mesh, no path — 30 refusals per 0.1 s for 114 s) = the island case at
  frame rate; kills/min 1.8. Fairness: 0 acquisitions under the 200 ms floor across 2,367 samples.
- Gates (expected FAIL, they ARE the defect): idle HARD, sweep HARD, stuck PROV, stall PROV,
  arena01 refusals/switch 9.88 vs 1.0. PASS: unserved, wiring, FFA grants, pile-up, thrash.
- Parser gaps found on live logs (to fix before W-VERIFY): `sweep over` only accepts
  state=Search|Roam (Mode sweeps uncounted → sweep UNDERCOUNT); `acquire` regex misses
  "(N believed)." → acquisitions=0 and the F1 gate is silently absent; two F7-labelled refusal
  lines (`POI path refused`, `could not path to the belief`) outside the f7 alternation;
  `match_seconds` ignores the match-over time; `after -1.000s reaction` sentinel unparsed;
  `has no POI provider for kind None` (688/log) is an unclassified wiring-shaped warning.
- Parser gaps fixed (aib-editor) and the baselines re-read (`*-v2.json`, the ones W-VERIFY compares
  against): sweep_seconds median is really 104 s Spillway / 176 s Arena01 per bot (Mode-state
  sweeps were uncounted); F1 floor now measured — fastest acquisition 0.233 s across 3,716
  samples on both maps, PASS; match_seconds from the match-over line (score-limit matches read
  6.9 kills/min, not 2.6); `wiring_pois` (no POI provider for kind None) 500–690 per Spillway
  log, 0 on Arena01 — visible, ungated; `POI walk stalled — giving up (F7)` is a third shape
  still outside the alternation (one word, next parser touch).
- 2026-09-02 step 3 (lead, editor-live over the Unreal MCP — the Slate inspector typed the two `py`
  lines into the status-bar console; python remote-exec never advertised, see DefaultEngine.ini):
  (a) `BR_NavIsolation` measured with the live recast agent (radius 35, step 35, cell 38/19/19,
  tile 1000): A_Abut (coplanar ramp head, the BN34-defect-3 control) floor->deck valid, NOT partial,
  1648 uu; B_Fix 1658 uu; all 10 probe points on nav. **Coplanar-merge hypothesis REFUTED** —
  the generator's ramp heads merge. (b) SKIPPED on that evidence (no ramp overlap / sink change,
  no yard-ramp x0 change, no regen). (c) `Tools/aib/aib22_nav_bounds.py` on both maps: Spillway
  one volume ±4600/±5250/−600..1800 vs geometry — every top inside it except SM_SkySphere; the 26
  covered-but-not-on-nav tops are walls, parapets, fins, roof/shelf covers (too thin for r35) and
  the three sunken shaft ramps (AABB top-centre is mid-air over the slope — probe artefact, not a
  gap). Arena01 BR_NavBounds −200..4200 × −200..1400 covers the −100..4100 geometry; only the four
  walls lack a top. **Bounds PASS.** (d) link-draw walkthrough is visual — folded into the step-8
  PIE watch; NO proxies placed. Open: Arena01's "goal 21 uu below, both ends on mesh, no path"
  island (baseline Ctrl_4) is therefore NOT a coplanar merge — first suspect is now the drop-link
  direction (one-way BN_Drop with no climb back), to be read off the PIE link-draw.
- 2026-09-02 lane A (aib-builder) reported: Search/Sweep written, NOT compiled (7 files +
  `Tests/AIBSweepBudgetSpec.cpp`); `ForgetSearchMemory` uses a const_cast pending a one-line
  `ForgetMemory()` on the sensorium (lane B's serial step); parser's `sweep over` still counts
  task-active seconds — W-VERIFY must key the sweep gate on the new `sweep budget spent` line.
- 2026-09-02 lane A verified (lead): BreachpointEditor PASS (game/server targets deferred to the
  lane A+B rung-1); `Tools/run-specs.sh AIBot` 173 success / 0 fail incl. the six new
  `AIBot.Sim.SweepBudget` cases (Tools/Logs/specs-20260902-232255.log). Lane B dispatched.
- 2026-09-02 box 2 tooling (lead): `Tools/aib/aib22_platform_census.py` — a platform has a way
  down/up iff `find_path_to_location_synchronously` top<->floor is valid and NOT partial (the
  BN_Drop/BN_Climb links live in the tiles; no python API lists them, a path IS the census).
  Under `-run=pythonscript` it is meaningless (Spillway: 0 tops on nav; Arena01: 28/28 fail both
  ways, ramps included) — the commandlet never brings the nav system up. Editor-live, next
  editor session (with the PIE top-platform watch). Python remote-exec ini section removed: it
  never advertised; the MCP Slate inspector typing into the console is the working route.
- 2026-09-02 W-REVIEW lane A (aib-critic on 9d944098) — two HIGH, three MEDIUM, two LOW; a fix
  packet follows lane B (same file). Rulings (lead):
  H1 Forget is not durable: a refused path Forgets the lead, an audible enemy re-supplies it,
     Search re-wins, refused again — at the new 0.1 s delay that is ~10 refused queries/s standing
     still; and none of the Search exits arms `NoteCurrentAmbitionFailed` (only MoveToObjective
     does). RULING: a refused path says nothing about the LEAD — keep the memory, arm the 3 s
     failure suppression instead; Forget only on "post swept, nothing there" and give-up-at-post,
     and those arm suppression too.
  H2 budget is per still-spell, not per post: Mode's hill hold never moves, so its 2 s budget
     spends once and the bot faces one direction for the whole hold; a Search that starts inside
     acceptance radius gets a zero-length look and Forgets on tick 1. RULING: the budget refills
     when SweepLook enters a post more than AcceptanceRadius from the last swept post (controller
     stores `LastSweptPost`); Mode's stand becomes a NAMED still tactic (`SetStillTactic(Hold)`)
     with an unbudgeted slow scan — a hold is the tactical exception the founder allowed, a
     frozen head is not.
  M3 the 0.5 s Idle give-up is a no-progress ratchet, so one Idle frame after any detour fires
     it. RULING: since-enter grace + Idle for 0.3 s consecutive.
  M4 `AIBSweepBudgetSpec` case 1 asserts a local outlives an inner block; a task-held budget
     passes it. RULING: model the controller as a long-lived owner and the task scratch as a
     recreated copy; add the negative case (a task-local copy does NOT survive).
  M5 the gates: `sweep over` sums the whole state duration (now mostly walking) for Mode and
     Search alike, and SweepLook never names a still tactic, so both HARD gates are un-passable
     by construction. RULING (gate re-based, W-VERIFY reads this): the line reports STATIONARY
     sweep seconds only; SweepLook's stationary spell is `SetStillTactic(Sweep)`; gate becomes
     `max single sweep ≤ SweepMaxSeconds + 0.5` and `sweep_seconds ≤ 5 % of match`, idle gate
     unchanged (named tactics excluded).
  L6 AreaDenial claims the yaw while walking and never spends the budget — fix in the same
     packet (steer only when stationary, spend while aligning). L7 `ForgetSearchMemory` uses
     `Remembers(Actor)` as an identity oracle for a log line — risk register, not fixed.
  PASS verified: const_cast well-defined; server-only clean; F1/F4 clean; quiet-case Forget
  design correct.
- 2026-09-02 lane B (aib-builder) landed at rung "compiles" (BreachpointEditor PASS first build;
  specs running, result below): wander draws from the PAWN on a full path only (H3); island
  latch = 3 consecutive no-full-path draws, held on the controller (`FAIBIslandLatch`, same
  pattern as the sweep budget); `Egress` under Roam behind `FAIBOnIslandCondition` — 16-ray
  `NavigationRaycast` fan finds MY island's lip (Detour skips off-mesh links, so every hit is
  my boundary), lip only if nav exists ≥ IslandMinDropUU below past it, step-off is AIB19's
  exact projectionless MoveToLocation, phase re-derived from the world each Tick, ExitState
  never cancels a fall; `ForgetMemory()` on the sensorium retires lane A's const_cast. Row
  fields IslandLatchDraws 3 / IslandLipStandoffUU 60 / IslandLipProbeUU 150 / IslandMinDropUU
  120 (defaults apply; DT_AIBTiers reimport only if a tier wants an override). **ST_AIBBot must
  be rebuilt** (`Tools/aib/70_aib_assets.py`, editor-live) — new states/nodes are not
  default-on-load. Parser (lead): `island latched`, `island egress starts`, `island egress
  FAILED` regexes + per-bot latch/failed counts, selftest updated.
- 2026-09-02 lane B specs (lead): `Tools/run-specs.sh AIBot` 179 success / 0 fail incl. the six
  `AIBot.Sim.IslandLatch` cases (Tools/Logs/specs-20260902-234016.log). Fix packet for the lane A
  review dispatched; lane B under W-REVIEW.
- 2026-09-02 W-REVIEW lane B (aib-critic on d7d68d6a) — two HIGH, four MEDIUM, three LOW; fix
  packet follows the lane A fix (same files). Rulings (lead):
  H1 Egress<->Wander is a 4 s standing oscillator: no lip -> clear -> 2 s stand -> 3 bad draws
     -> latch -> 2 s stand -> Egress ... (culvert case: nothing ≥120 below, every upward draw
     partial). RULING: Wander NEVER fails with a goal in hand — when no full path exists it
     walks the LONGEST partial draw (on an island that is motion toward the edge, which is the
     point); an Egress failure arms a controller `EgressCooldownSeconds` (5) during which draws
     do not latch; both failure delays 2.0 -> 0.1 s.
  H2 `CostLimit = 4 x WanderRadius` makes "far but reachable" read as partial -> false latch on
     open ground near any tier stack (~p^3 per entry), and Egress cashes a false latch out as a
     real step-off — herding healthy bots into the culvert. RULING: no cost limit on the draw's
     path test; the latch is a HYPOTHESIS — `FAIBOnIslandCondition` confirms it with one
     cost-unlimited `TestPathSync` from the feet to the bot's own spawn point (recorded in
     OnPossess: reachable ground by definition); no full path = island, else clear + cooldown.
  M3 latch is stale across ambitions (latched in the culvert, chases up to T2, steps off T2).
     RULING: any COMPLETED full-path move (any state) clears it; `LatchMaxAgeSeconds` 10.
  M4 `bAirborneSeen` without a `bSteppedOff` guard — one ungrounded frame on the walk aborts
     the tactic. RULING: guard on `bSteppedOff`.
  M5 no maximum drop; `FAIBTraversalPolicy` bypassed — the only bound is ProbeExtent.Z = 1000 by
     accident (policy says 800). RULING: Egress asks `Choose(Drop)` for each candidate lip and
     skips refused ones; the probe extent is derived from the policy's limit, not restated.
  M6 ExitState mid-fall leaves the unpathed MoveRequest alive after landing. RULING: controller
     one-shot `bStopOnLanding` set by ExitState when airborne, consumed by Think on the first
     grounded sample (StopMovement only if no newer move was issued).
  L7 projection box finds same-level neighbours behind thin walls (lip invisible). RULING:
     probe box centred BELOW the point (z - limit/2, half-extent limit/2) so only lower nav
     qualifies. L8 ramp heads PASS (merged cluster, no hit). L9 the IslandLatch re-entry case
     has the same defect as the sweep one — negative cases added to BOTH specs.
  PASS verified: server-only, fairness, controller-held latch survives completion transitions.
- 2026-09-02 lane A fix packet (aib-builder) at rung "compiles" (BreachpointEditor PASS; specs
  running): H1 refused path keeps the lead + arms `NoteCurrentAmbitionFailed` (Forget only on
  swept/give-up, both suppressed); H2 `FAIBSweepBudget::ArriveAt(Post)` refills per post, Mode's
  stand is `EAIBStillTactic::Hold` with an unbudgeted 40°/s scan; M3 since-enter 0.5 s AND Idle
  0.3 s consecutive; M4 spec remodelled (owner vs recreated scratch, negative case, ArriveAt
  cases — 8); M5 `EAIBStillTactic::Sweep` while spending, `sweep over` reports stationary
  seconds; L6 denial steers only when standing and spends. Tree unchanged. Builder flagged:
  lane B's Egress lip-walk has the same M3 ratchet shape (:2226) — folded into the lane B fix.
- 2026-09-02 lane A fix specs (lead): 181 success / 0 fail, `AIBot.Sim.SweepBudget` now 8 cases
  incl. the negative task-held one (Tools/Logs/specs-20260902-234905.log).
- 2026-09-03 lane B fix packet (aib-builder) written; building. H1 Wander walks the LONGEST
  partial when no full draw exists, Egress failures arm `EgressCooldownSeconds` (5) via
  `ClearWithCooldown`, both failure delays 0.1 s; H2 no CostLimit, `FAIBOnIslandCondition`
  confirms the latch with one cost-unlimited `TestPathSync` feet -> recorded spawn (fallback:
  latch alone, logged); M3 `OnMoveCompleted` clears on `DidMoveReachGoal`, `LatchMaxAgeSeconds`
  10; M4 airborne only after step-off; M5 lip filtered by `FAIBTraversalPolicy::Choose`, probe
  Z from `SafeDropUU x TraversalCommitFraction`; M6 `ArmStopOnLanding` consumed by Think if the
  move request id is unchanged; L7 probe box below the point; L9 spec now 10 cases. Known
  ceiling: a single-poly island redraws in place every 0.25 s until the gate confirms.
- 2026-09-03 lane B fix specs (lead): 185 success / 0 fail, `AIBot.Sim.IslandLatch` 10 cases
  (Tools/Logs/specs-20260903-000057.log). Game + server targets building for rung 1; both fix
  packets under a second W-REVIEW.
- 2026-09-03 rung 1 on 6258a645 (lead): BreachpointEditor PASS, Breachpoint PASS,
  BreachpointServer NOT BUILDABLE — UBT: "Server targets are not currently supported from this
  engine distribution" (launcher UE 5.8, not source-built; Tools/Logs/ubt-BreachpointServer.log).
  Reported as PARTIAL per testing.md; the server rung needs the source-built engine machine.
- 2026-09-03 W-REVIEW #3 (aib-critic on 4caae004 + 6258a645): lane A H1/H2/M3/M4/M5/L6 CLOSED,
  lane B M3/M4/M5/M6/L7/L9 CLOSED, B-H1/B-H2 PARTIAL; three NEW high. Rulings (lead), fix
  packet #3:
  HIGH-1 the idle episode ORs still tactics over the whole spell, so one 2 s `Sweep` labels a
     280 s stand `tactic=Sweep` and both HARD gates pass on the founder's exact defect.
     RULING: the episode CLOSES whenever the still-tactic set changes — seconds are attributed
     to the tactic that was actually set while they elapsed; `tactic=none` resumes the moment
     the tactic clears.
  HIGH-2 a lipless micro-island (every partial ends inside acceptance) redraws at 4 Hz forever,
     12 exhaustive pathfinds/s, `tactic=none`. RULING: after Egress fails on a CONFIRMED island,
     the bot is STRANDED — `EAIBStillTactic::Stranded` named, one `stranded — no legal lip`
     Log line, no draws for the cooldown (a stranded bot is a MAP defect the verifier must
     see, not a silent spin); still counts against the idle gate's tactical column, reported.
  HIGH-3 the spawn anchor is on the deck tier for four Spillway spawns (SP_W3/W4/E3/E4,
     z 420) and the census shows the T1 deck has no full path up, so a deck-spawned bot on
     healthy floor confirms a FALSE island and Egress walks it into a shaft pit. RULING: the
     anchor is the goal of the LAST COMPLETED full-path move (recorded at the `DidMoveReachGoal`
     site), spawn only until the first completion; a bot on healthy ground refreshes it every
     move, a bot on an island never does.
  MED-4 refill band 150–225 uu (at-post 1.5R vs refill >R). RULING: refill only beyond 1.5R.
  MED-5 the policy's commit fraction caps Egress at 800 while the gantry is 893, and the
     horizontal term is a probe artefact. RULING: Egress is the last resort — it asks the policy
     with the SURVIVABLE limit (`SafeDropUU`, no commit fraction) and passes the lip standoff
     as the horizontal, not the projection scatter.
  MED-6 the enter condition has side effects and runs an exhaustive TestPathSync per
     evaluation. RULING: confirm ONCE per latch (result cached on the latch, re-tested only
     when re-latched); the condition reads the cache.
  LOW-7 `Hold` is a label on standing still. RULING for Phase 11: risk register + `hold` seconds
     reported ungated; Phase 13 (separation/strafing) makes the hill hold a strafe-hold with
     the existing `FAIBStrafeTask`. LOW-8 specs are worldless — accepted, engine contract
     verified by the critic against PathFollowingComponent.cpp:602.
- 2026-09-03 box 2 census, editor-live over the MCP console (`Tools/aib/aib22_platform_census.py`,
  anchor = PlayerStart, "way" = full non-partial path):
  Spillway 94 platforms, 19 fail both ways — all prop tops (cover blocks z+400, lane crates
  z+240, roof crates, columns, mast, tower mass) that nothing walks to; the one platform-named
  failure, `SPW_Tower_Deck_T1`, is a 2800x2800 slab whose 5x5 top grid has NO nav (buried
  under the tower) — the centre-point sliver was the artefact, not an island.
  Arena01 7 platforms, 7 fail both ways: `BR_LM_The_Gantry_01/02` (z 800, 400x400, whole grid
  nav-but-no-path either way) and `BR_LM_The_Core_02..06` (z 900). The gantry is a true island
  in BOTH directions — no generated BN_Drop link leaves it despite an 800 drop under the 1000
  ceiling (suspect: landing farther than JumpLength 400 from the edge / under the core mass).
  RULING: box 2 re-scoped — a platform's "way down" is a generated link OR the Egress
  step-off; the PIE top-platform watch (box 4) is the measurement, on the gantry. The link
  generator question is filed for the arena-architect (not Phase 11 code).
- 2026-09-03 ST_AIBBot + DT_AIBTiers rebuilt editor-live (`Tools/aib/70_aib_assets.py build`,
  read-back in the editor log): states `Root > [Evade, Engage, Retreat, Search, Seek,
  Roam > [Egress, Wander], Mode, Fallback]`, compiled YES, saved OK; DT_AIBTiers 5 rows
  mirrored from the C++ registry (new Search/Roam fields at their defaults). Committed.
- 2026-09-03 fix packet #3 (aib-builder) written; editor closed, building: HIGH-1 idle episode
  closes on any still-tactic set change; HIGH-2 `FAIBIslandLatch::Strand()` +
  `EAIBStillTactic::Stranded`, one `stranded — no legal lip` line per latch, Wander holds
  without drawing until the cooldown lapses; HIGH-3 `MoveTo` override records a pathed
  request's destination, `OnMoveCompleted` on `DidMoveReachGoal` sets `LastFullPathGoal`,
  anchor = last full-path goal else spawn (named in the log line); MED-4 1.5R band; MED-5
  `Choose(Request, bLastResort)` with `SafeDropUU`, horizontal = lip standoff; MED-6
  `EConfirm` cached per latch. Tree unchanged. Specs: IslandLatch 12, TraversalPolicy +1.
  Parser (lead): `stranded` regex + per-bot `stranded_count`, reported not gated.
- 2026-09-03 W-VERIFY v3 (5 x 2 maps at 1b19351f, `Tools/aib/aib22_verify.sh v3`, logs
  Tools/Logs/aib22-verify-*-v3-*.log): Spillway idle median 56 s (baseline ~150), worst 95 s;
  longest single sweep 7.0 s; stuck worst 236 s; refusals median 95 vs baseline 70 (WORSE —
  dominated by `self=NO` off-mesh-self refusals from one spot, 163 of 940 in log 4); egress 0,
  latches 6, stranded 2; kills/min 1.8 (baseline 1.0). Arena01 REGRESSION: idle median 276 s
  of 300, stuck 294 s, kills/min median 0 (baseline 1.8), one match over at 86 s; bots that
  leave the mezzanine (spawns + objective, z 410) stall at z 218/10 "280uu up — link=no" and
  never return. Verifier breakdown pending. All HARD gates FAIL on both maps.
- 2026-09-03 root-cause evidence (lead, editor-live on Arena01): the extended census (tiers
  above AND below the PlayerStart anchor) shows the floor and all 13 catwalk stair steps have
  no full path to the mezzanine in EITHER direction (partial ends ~400–530 uu short = the
  storey). RecastNavMesh-Default: generate_nav_links True, BN_Drop 400/1000/50 and BN_Climb
  250/−70/90 present, runtime generation DYNAMIC. Zero `link traverse` lines in all 10
  matches (the hook only sees custom links, so not decisive alone). The nav-build warning
  "BorderForLinks (23 vx) exceeds tileSize (0 vx)" is cosmetic (RecastNavMeshGenerator.cpp:3682
  still takes the max border). Every catwalk step reads at floor height — suspect the stair
  generator lays the steps flat, i.e. Arena01 has NO climb to its own spawn/objective tier.
- 2026-09-03 W-VERIFY v3 breakdown (aib-verifier) — Spillway improved (idle −22 %, stuck −12 %,
  kills 1.8 vs 1.0) but FAIL; Arena01 regressed (idle +39 %, stuck +49 %, kills 0.0 vs 1.8).
  Causes ranked by seconds: C1 the island confirmation refutes ITSELF — Arena01's corner spawn
  pads (z 218) are islands, so feet->spawn is a path within the island (276 of 316
  refutations), and the last-full-path-move anchor refutes the same way when that move was on
  the island (40 more); ~4,900 s of 5 Arena matches. C2 a 170 lines/s refusal storm on a tier
  island: `POI path refused` every frame — MoveToPOI never arms suppression (25,182 refusals,
  one bot). C3 Mode stands `tactic=none` while the mover stalls 3–8 s against `link=no`
  (give-up window resets on acquire/SWITCHED: 21.9 s and 46.5 s stalls), and SweepLook refills
  on the POST moving, not the bot (7.0 s single sweep). Also: bots grappled onto the gantry/core
  read `grounded=no` (feet 959–1217 vs nav 910) so Egress REFUSES on the exact platform the
  ticket exists for; Spillway's extra refusals are 95 % the t<2 s spawn burst before the mesh
  exists (`self=NO`), not the new code; parser `idle over` regex dropped `tactic=Sweep|Stranded`
  (fixed: `\S+`); `wiring_pois` 4,297 Spillway / 1,355 Arena (bots on z≥800).
  RULINGS (fix packet #4):
  R1 confirmation anchors are a LIST and "island" means NONE has a full path: the current
     ambition's goal (objective / POI / last-known), every PlayerStart in the level, and the
     last full-path goal — a spawn pad that is itself an island then confirms correctly and a
     floor bot refutes via the objective.
  R2 MoveToPOI (and every mover) arms `NoteCurrentAmbitionFailed` on a refused path, exactly as
     Search does since fix #1 — no per-frame retry anywhere.
  R3 a stall whose diagnosis is `link=no` (goal a storey away, no link) abandons on the FIRST
     read, arms suppression, and the give-up window never resets on perception events.
  R4 SweepLook refill keys on the BOT having moved ≥1.5R since the last refill, not the post.
  R5 Mode outside GoalReach with the objective unreachable is not a hold: it fails (R3) —
     Mode's `Hold` stays only for the on-objective stand.
  R6 Egress grounding = the avatar's movement state (not the nav projection): a bot standing on
     geometry above the mesh is grounded and OFF-MESH; Egress first walks (pathfinding off) to
     the nearest nav point within IslandLipProbeUU, then runs the lip fan; lips are searched
     from that on-mesh point. This is also the off-mesh-self recovery.
  R7 no Think/move issue before the pawn projects onto nav once (kills the spawn-burst
     refusals); log one `waiting for nav` line.
  Map defects filed for the arena-architect (NOT Phase 11 code): Arena01 corner spawn pads are
  islands; the catwalk stairs read at floor height (no climb to the mezzanine); gantry/core top
  geometry sits above its navmesh.
- 2026-09-03 PIE gantry watch at fix #3 (Arena01, `aib22_pie_gantry_watch.py`, 7 bots placed on
  `BR_LM_The_Gantry_01` top z 800): at t+15.5 s six were on the floor (z 98), one still on
  (z 898) — box 4 FAIL as measured, and the six LEFT BY THE WRONG MECHANISM: `stall over —
  3.4–4.5s … jumped=yes` = the stall watchdog's blind hop off an 800 uu edge with no policy
  check (step 4 was meant to retire it). Bot 3 ran the designed path: `island latched` ->
  `CONFIRMED — no full path to the last full-path move anchor` -> `egress starts — lip 89uu
  away, drop 800uu` -> but the LIP WALK completing fired `island latch cleared — a full-path
  move completed` (fix #3 M3) one tick before `steps off the island's lip`, the gate went
  false, ExitState stopped the grounded bot, and it stayed on top (stalled again at z 898).
  RULINGS (added to fix packet #4):
  R8 Egress's own moves (lip walk, step-off) never clear the latch — the controller knows the
     request ids Egress issued; the completion-clear skips them.
  R9 the stall watchdog never jumps blind: a stall with `link=no` abandons (R3); the only
     jumps are link segments (step 4) and the traversal policy's verbs. Remove the hop.
- 2026-09-03 fix packet #4 (aib-builder) at rung "compiles" (Editor + Game PASS; server target
  unbuildable here): R1 anchor LIST (want goal + last full-path goal + every PlayerStart),
  pure `FAIBIslandLatch::Confirm`; R2 suppression armed at every refusal/give-up site; R3
  `FAIBLocomotionState` on the controller, `link=no` storey stalls abandon at 1.5 s, window
  survives Engage flaps; R4 refill keyed on the BODY's displacement (≥1.5R); R5 via R3; R6
  grounding = avatar `IsGrounded`, off-mesh recovery walk (pathfinding off) then the lip fan,
  Wander runs the same recovery; R7 no decisions until the pawn projects onto nav once
  (`waiting for nav`); R8 `MarkEgressMove` — Egress's requests never clear the latch nor set
  the anchor; R9 finding: the watchdog has pressed nothing since step 4 — `jumped=yes` was a
  bookkeeping flag; now reads whether a LINK jump was pressed. So what pushed the fix #3 bots
  off the gantry is still unexplained (suspect: the partial walk to the edge) — PIE watch v4
  decides. Tree unchanged. Parser: `stall abandoned` (counts as stuck seconds), `off-mesh
  recovery`, `waiting for nav`.
- 2026-09-03 W-VERIFY v4 (5 x 2 maps at 8e138133 = fix #4; logs Tools/Logs/aib22-verify-*-v4-*.log):
  Arena01 kills/min median 3.4 (v3 0.0, baseline 1.8), three matches ended on the score limit
  (84–176 s), idle median 97.6 (v3 276, baseline 199), refusals 0 (R7 nav gate), latches 7/bot
  median, egress 0, stranded 0, off-mesh recoveries 0. Spillway kills/min 1.8, refusals 0,
  idle median 116 (v3 56 — worse), off-mesh recoveries 8/bot median, egress 0. NEW STORM on
  both maps: `stall abandoned` ≈2,000 lines per bot per match (≈7/s) and the parser's stuck
  sum explodes (tens of thousands of seconds) — R3's storey abandon re-fires on the same goal.
  Verifier breakdown dispatched (v4). All HARD gates still FAIL; kills/min PASS both maps.
- 2026-09-03 fix #4 specs (lead): AIBot 189 success / 0 fail (Tools/Logs/specs-20260903-062523.log),
  BreachpointNext 45 / 0 (specs-20260903-062629.log).
- 2026-09-03 W-VERIFY v4 breakdown (aib-verifier): the `stall abandoned` storm is ONE running
  stall clock re-reported every frame (77 % of consecutive lines < 0.05 s apart, `seconds` never
  resets): `Rescore` keeps the incumbent tag when every want scores 0 (Mode.Rally and Roam both
  suppressed), the tree re-enters the same state next frame (`sweep over — 0.0s` 86k lines per
  map = a Mode enter+exit per frame), `StallSeconds` only resets on ≥50 uu progress, so
  TickLocomotion returns true again — 107k lines Arena / 64k Spillway; the parser summed the
  clock per line (stuck 10^3–10^4 x too high) and double-counted the `resolved=abandoned`
  episode. Spillway's idle regression (+2,342 s) is the off-mesh recovery LOOP: the projection is
  0 uu horizontal but > StepHeight above the nav, a 0 uu walk cannot close a vertical gap, 3 s
  timeout, suppression, Wander re-enters, repeat (856 walks, 95 % FAILED, two channel spots).
  False storeys: the diagnosis compares goal Z (nav, 10) with the PAWN CENTRE (98) — |up| 88–98
  > 45 reads as a storey (5,908 lines). Arena latches 328 -> REFUTED 321 (nav says the pads and
  gantry ARE connected — the body cannot walk it: map, AIB28). Early ends are real 7-kill wins
  (max 3 kills per bot). RULINGS (fix #5): F5-1 an abandoned goal starts a FRESH clock when
  re-issued and is refused for the suppression window; `Rescore` with every want at 0 falls
  back to Roam with a fresh draw, never the incumbent (kills the per-frame flap). F5-2 a 0 uu
  horizontal recovery is a VERTICAL gap: go straight to the lip fan / step-off (R6), else fail
  once with the cooldown — never the 3 s walk loop. F5-3 storey and off-mesh Z tests use the
  FEET (capsule bottom), never the pawn centre. Parser (lead): stall_abandoned counts EPISODES
  (a new episode when `seconds` drops or the gap > 1 s), no per-line sum, no double count with
  `resolved=abandoned`; sweep HARD bars key on `moved ≤ 50uu`; `sweep over — 0.0s` counted as
  `state_flaps` (reported).
- 2026-09-03 box 2 (lead, editor-live): the Arena01 "no way up" was a NAV RESOLUTION defect —
  the default resolution's 38 uu cell erased the 42 uu stair treads (AIB28 root cause). With 19
  uu cells floor->mezzanine is a FULL path (1398 uu) and the 13 steps leave the census list.
  Spillway's 19 census failures are prop tops. Still islands: the Arena01 gantry/core tops —
  their way down is Egress (fix #5's lip fan) or a generated BN_Drop that the map does not
  produce; measured by the PIE watch and W-VERIFY v5. Box 2 ticked for stairs/tiers; the
  gantry/core remain in AIB28.
- 2026-09-03 rung 1 on main at 9bddc1bf (fix #5 + the Phase 12–15 review-fix packet + everything
  before): BreachpointEditor PASS, Breachpoint PASS (server target unbuildable on this engine).
  Specs running; then the editor session (ST_AIBBot rebuild for the tactic-child wiring, the
  gantry PIE watch with captures, the 60 s PIE observation) and W-VERIFY v5.
- 2026-09-03 rung 2 at the final tree (a7 = HEAD): AIBot 243 success / 0 fail
  (Tools/Logs/specs-20260903-072017.log), BreachpointNext 36 / 0. Editor session next: ST_AIBBot
  rebuild (tactic children complete to Engage), the gantry PIE watch with captures, the 60 s
  PIE observation on both maps; then W-VERIFY v5 headless.
- 2026-09-03 PIE gantry watch at the FINAL tree (Arena01, 7 bots placed on `The_Gantry_01`, real
  MCP session): at t+13 s five of seven were off the platform (z 98/240/498), two still on
  (bots 1, 2) — box 4 FAIL as measured, but the designed mechanism now RUNS: bot 5 `island
  latched` -> `CONFIRMED — no full path to any of 17 anchors` -> `egress starts — lip 89uu away,
  drop 453uu` -> `steps off the island's lip` -> off; bot 6 egress FAILED (lip 118 short, mover
  idle) -> 5 s cooldown -> `off-mesh recovery — vertical gap 51uu, stepping off 150uu` (fix #5)
  -> off. Two residual defects read straight off the log: (a) `island latch cleared — a
  full-path move completed` still fires between `egress starts` and `steps off` — the LIP WALK's
  completion (89–118 uu, inside acceptance, completes synchronously before `MarkEgressMove`
  records its id); (b) bot 2's latch was `REFUTED — full path to the last full-path move anchor
  (1 of 1 tested, 17 offered)` — that anchor was ON the gantry (the lip walk's goal), the list
  stopped at the first full path. Also `idle over — 0.1s state=Egress tactic=Hold`: a still
  tactic bit leaked across a state exit (the idle gate would exclude a stand it should count).
  RULINGS (fix #6): F6-1 the anchor list drops the last-full-path goal entirely (PlayerStarts +
  the want's goal are enough — 16 starts on Arena01); F6-2 Egress sets `bEgressMoveInFlight`
  BEFORE issuing the lip walk/step-off/recovery and the completion-clear skips while it is set
  (the id mark stays as belt); F6-3 every still-tactic bit is cleared on state exit (the
  sentinel's ExitState), so a label never outlives the state that set it.
- 2026-09-03 PIE observation (real MCP session, Arena01, 7 bots, `Tools/aib/aib_pie_observe.py`,
  5 samples over 64 s, captures at 0/30 s — a live match: kill feed, grenades, score 2–5 at
  9:13): sampled path per bot 3250 / 3952 / 3329 / 1873 / 1845 / 4565 / 3015 uu (straight-line
  between samples = a lower bound); longest still spell 0 s for five bots, 16.3 s (bot 4) and
  12.1 s (bot 5, mid-grapple to z 1256) for two. In the same minute the log shows every phase
  live: `ambition -> … reason=merit|veto|interrupt|fallback`, `route — lanes=2>4 …` (real lane
  ids), `team report … from …`, `target claim GRANTED (1/2)`, `tactic -> Push … reason=first`,
  `hill strafe-hold — ring 360uu`, `island egress starts … FAILED — lip 111uu short (mover
  idle)`. Observed for W-VERIFY v5: veto flip-flops between Roam (0.20) and Mode.Rally (0.00)
  several times a second on some bots — measure `ambition_switches` per minute vs baseline.
  The earlier PIE match ran to the 7-kill score limit and travelled to the front end on its own.
- 2026-09-03 W-VERIFY v5 (5 x 2 maps at f92e6346 = everything but fix #6; logs
  Tools/Logs/aib22-verify-*-v5-*.log): matches END ON THE 7-KILL SCORE LIMIT in 50–93 s
  (Spillway kills/min median 9.2 vs baseline 1.0; Arena01 13.0 vs 1.8 — one Arena01 match ran
  300 s). Per bot per match (medians): idle tactic=none 7.0 s Spillway / 9.4 s Arena01 (v4 116 /
  98; baseline 72 / 199); stuck 19.7 / 15.5; refusals 0 / 0; stall abandoned 2 / 1 (v4 ≈2,000
  per frame); state flaps 19 / 15 (v4 86,000); longest single sweep 2.0 PASS both; egress 0,
  latches 0 (no islands in normal play — the gantry is a placed-bot case); Phase 13 overlap 0 /
  1.6 s, yields 0 / 0; Phase 14 route changes 28 / 42 per bot with real lane ids; Phase 15
  flanks 0, holds 0 (the tactic layer stayed on Push in these fights). FAIL: idle HARD (worst
  18.3 s Spillway; Arena01 log 1 = the 300 s match, bot 0 idle 126.8 s / stuck 168 s); target
  pile-up buckets 8 / 9 (HARD 0 — Phase 12's claims cap is not holding in 1 s buckets); stuck
  and single-stall PROVISIONAL bars; sweep fraction 0.15 / 0.17 (the bar assumes 300 s matches
  — v6 runs with `?ScoreLimit=200` so matches go to the time limit). Verifier breakdown
  dispatched; fix #6 building.
- 2026-09-03 fix #6 merged (e722f447) and rung 1 PASS (Editor + Game). Specs running; then the
  FINAL editor verification (gantry watch + observation on both maps) and W-VERIFY v6.
- 2026-09-03 rung 2 at fix #6 (05badaa2): AIBot 243 / 0, BreachpointNext 36 / 0. Final editor
  verification starting.
- 2026-09-03 FINAL gantry watch (fix #6 build, Arena01, 7 bots placed on `The_Gantry_01`):
  t+9.9 s 1 of 7 off; t+48 s 6 of 7 off (z 98/129); t+82 s 5 of 7 off — bot 5 still on the
  gantry, bot 6 at z 1353 (a wall-top grapple perch, counted "still on" by the footprint test).
  Fix #6 verified in the log: `CONFIRMED — no full path to any of 16 anchors` (F6-1, the
  on-island anchor is gone); `egress starts -> steps off the island's lip` with NO
  `latch cleared — a full-path move completed` in between (F6-2). Residual: bot 4/5 repeat
  `egress starts — lip (1289,19xx,810) … drop 453` several times — a lip INSIDE the platform's
  footprint (x 1000–1400): the fan hits a nav boundary on the gantry top (a hole or split in
  the top's mesh) and the step-off lands back on the platform. Box 4 (leaves within 5 s):
  FAIL as written; the designed mechanism clears the platform for 5 of 7 bots within ~50 s.
  Next: grid the gantry top's nav (editor, not PIE) to see the hole; the fan must skip lips
  whose landing projects back onto the SAME island (no full path away = still on it).
- 2026-09-03 W-VERIFY v5 breakdown (aib-verifier): idle tactic=none 286 s Spillway / 907 s
  Arena01 over 5 matches each. #1 cause: the mover's 8 s give-up window at GrappleRoutes
  APPROACH points — `[BNAIBWorldQuery] GrappleRoutes` is a GLOBAL table whose Arena01 points
  ((2800,3000,0) etc., z 0 under the z-10 nav) are handed to Spillway bots too (282 s). #2:
  Arena01 log 1 (the 300 s match): Engage never appeared for 270 s while acquisitions kept
  firing (Engage want 0 — suspect ammo 0 with no pickups). #3: stands between draws that end
  on a `route —` (386 s Arena01). Roam<->Rally veto cycle = the Rally goal being a stalled
  approach point. Pile-up 8/9 = PARSER: cap-2 never breached instant-level in 10/10 (hand-off
  inside one bucket; corpse re-grant storm GRANTED/RELEASED death x7 in 0.6 s = 30–50 % of
  grants — a real defect). Switch rate 2x baseline (veto-dominated). Phase 13: yields/overlap/
  hill lines present but `crowd simulation DISABLED` for 53–56 % of lives — separation OFF for
  half the bots. Phase 14: real lanes on Arena01 (teammates on different first lanes in most
  10 s windows), Spillway 28–46 % `lanes=none`; seed source=host in 10/10. Phase 15: Push first,
  flanks start 2–8/match but `flank over` = 0 — every flank cleared by "the fight ended" within
  0.5 s (Engage's LOS law). Sweep fraction: the judge summed walking pans (fixed: stationary
  only). No farming; F1 floor PASS 10/10.
  RULINGS (fix #8, aib-builder; adapter, bn-builder): F8-1 crowd simulation re-enabled on the
  first on-nav Think when disabled at possession; F8-2 no claims on corpses (liveness checked
  before a grant); F8-3 give-up window 8 -> 3 s (`MoveGiveUpSeconds` row); F8-4 Engage with an
  acquired target and a melee available never wants 0 (floor above Roam), `decide` prints
  `ammo=`; F8-5 while a young Flank latch lives, Engage reads the belief as in-fight (the fight
  ends when the belief ages out, not when the flank breaks LOS). Adapter: grapple routes
  per map (`Map=` on the ini rows) and approach/anchor points projected to nav at load.
  Parser: pile-up counts distinct concurrent holders per instant (next); sweep sum stationary.
- 2026-09-03 PIE observations at fix #6 (real MCP session, captures on file): Arena01 fresh
  match, 3 samples over 61 s before the 7-kill end — path per bot 1602 / 3574 / 2022 / 1616 /
  2378 / 3652 / 1620 uu, five bots with no still spell, two with one 50 s low-speed gap between
  samples (dead or planted — three samples cannot tell). Spillway, 4 samples over 140 s (gaps of
  95 s while agents were dispatched — too sparse for still spells): path per bot 6945 / 2082 /
  3119 / 4359 / 3204 / 8375 / 2438 uu, z from the culvert (−402) to the tower (271). The
  headless batch (v6, 300 s matches) is the honest idle measure; the captures show live fights
  on both maps (kill feed, grenades, score).
- 2026-09-03 fix #7 merged (ba24bbea) and the editor target PASS: `FindIslandLip` takes the
  nearest lip whose landing has a full path to an island anchor (`LandingLeavesIsland`),
  skipped lips logged at Verbose; a body already at the lip (35 uu + standoff) skips the walk
  (root cause of the 88 uu "away": IsWithin measures 3D from the capsule centre); landing back
  inside the platform's footprint (+50/+30) is `island egress FAILED — landed on the same
  island (F7)` with the cooldown. Per-map grapple routes merged (9f5f85f5, adapter + ini +
  generator). Fix #8 (crowd re-enable, no corpse claims, 3 s give-up, Engage floor with melee,
  flank holds Engage) in flight; then the final build, specs, v6 (300 s matches) and the last
  editor session.
- 2026-09-03 fix #8 merged (bd5a31c5), building: F8-1 crowd re-enabled on the first on-nav
  Think (`crowd simulation ENABLED late — …`); F8-2 claims refuse corpses (`Dead` result, no
  line) and the sensorium prunes dead candidates before the pump; F8-3 `MoveGiveUpSeconds` row
  = 3 s; F8-4 root cause of the 270 s Engage absence was `WeaponCanFight=0` (EMPTY-HANDED
  cycling, 45–152 lines per match) — Engage now floors at Roam + 0.05 with a target and a melee
  available, `decide` prints `ammo=`; F8-5 `bFlankHolding` keeps Engage's Sees at 1 while a
  young latch stands. Tree unchanged. Then: specs, v6 (300 s matches), the last editor session.
- 2026-09-03 rung 1 at fix #8 (bd5a31c5+): BreachpointEditor PASS, Breachpoint PASS. Specs running.
- 2026-09-03 rung 2 at fix #8: AIBot 247 / 0 (Tools/Logs/specs-20260903-080656.log),
  BreachpointNext 36 / 0. W-VERIFY v6 launched (5 x 2 maps, `?ScoreLimit=200` so matches run to
  the 300 s time limit).
- 2026-09-03 W-VERIFY v6 — FINAL build (fix #8, bd5a31c5+), 5 x 300 s matches per map
  (`?ScoreLimit=200`; logs Tools/Logs/aib22-verify-*-v6-*.log). Per bot per match, medians:
  idle tactic=none 24.6 s Spillway (worst 39.6) / 42.0 s Arena01 (worst 59.4) vs baseline 72 /
  199 and v3 56 / 276; tactical idle 42.9 / 46.0; stuck 86 / 81 (18 / 15 abandons of ≤3–4 s —
  the 8 s window is gone, longest single stall 3.0–4.0 s vs baseline 8–9); refusals 0 / 0;
  stationary sweep longest 2.0 PASS, fraction worst 0.067 / 0.115 (bar 0.05); kills/min 11.0 /
  13.6 vs baseline 1.0 / 1.8; pile-up 0 PASS; egress 0, latches 0 / 1, stranded 0; Phase 13
  overlap 10.2 / 3.0 s, yields 4 / 1; Phase 14 route changes 122 / 222 per bot; Phase 15
  flanks 0, holds 0; switches 1505 / 1607 per match; state flaps 90 / 84. Gates: idle HARD
  FAIL (bar 0), stuck PROV FAIL (sum of short stalls), single stall PROV 4.0 vs 3.0, sweep
  fraction HARD FAIL narrowly; PASS: F1 floor, unserved, wiring, FFA, pile-up, refusals/switch,
  single sweep, thrash, refusals vs baseline, kills/min. Verifier residual breakdown dispatched.
- 2026-09-03 FINAL editor session (fix #8 build, real MCP, captures in
  docs/tickets/evidence/aib22-2026-09-03/): gantry watch — 7 bots placed on `The_Gantry_01`:
  t+11 s 4 of 7 off, t+36 s 6 of 7 off, one bot loops on an interior lip that "never left the
  ground" (fix #7's F7-2/F7-3 lines fire; a lip that cannot be fallen from is the residual —
  named below). Dense observation, Arena01, ScoreLimit raised to 200 for the session
  (`Tools/aib/aib_pie_scorelimit.py`), 6 samples over 68 s: path per bot 2832 / 1346 / 4544 /
  1768 / 3201 / 2947 / 1627 uu; longest still spell 0 s for six bots, 24.3 s for one (bot 4);
  captures at 10 / 40 / 60 s show the live fight (kill feed, score 3–2 at 9:06). Earlier in
  the session a fresh match ended in 23 s on the 7-kill limit (recap screen captured).
  RESIDUAL for a follow-up: a lip whose step-off never leaves the ground must be blacklisted
  for the cooldown (the fan re-picks it); the founder's 5 s bar for box 4 stays FAIL.
- 2026-09-03 W-VERIFY v6 breakdown (aib-verifier, final build): idle tactic=none 25.6 / 44.9 s
  per bot-match, 43 / 34 % of it in sub-1 s transition spells, 0 stalls > 4 s (the 8 s give-up
  window is gone; v5 max 33 s). Causes: (1) the 3 s give-up STAND at a Wander goal — 152 / 319 s
  (the bot stands still for the whole window before abandoning; `AIBStateTreeTasks.cpp:608`);
  (2) untagged Retreat stands (`broke contact — holding to DEFEND`, `strafe held — outside the
  engaged radius`) 66 / 97 s; (3) give-up stalls in Search/Mode/Retreat 78 / 210 s. Tactical idle
  41 / 46 s is honest (Sweep ≤ 2.1–2.5 s, StrafeHold ≤ 3.1, Hold ≤ 4.1, all row-bounded).
  Fix #8 verified: crowd re-enabled 230/230 · 267/267 lives (F8-1); corpse re-grants 40 % -> 2 %
  (F8-2); abandons 56 / 39 % at exactly 3.0 s (F8-3); F8-4 NOT effective: `ammo=0.00` lines never
  want Engage (0 of 109,289) because `bMeleeAvailable` needs a HELD WEAPON
  (`BNAIBAvatarAdapter.cpp:370-376`) and the empty-hand case has none — `SwapNext` never lands a
  weapon (EMPTY-HANDED cycling 1/5..3/5): a GAME-SIDE equipment defect, filed as high for
  bn-builder; F8-5 NOT effective: `ClearFlankLatch("the fight ended")` at
  `AIBBotController.cpp:1159` fires whenever the AMBITION leaves Engage (Retreat/Roam veto) and
  never reads `bFlankHolding` — flank_count 2 / 0 of 123 starts. Switches 42 / 45 per bot-min
  (baseline 16 / 28), veto 60 %: the Roam<->Rally triplet 2.1 / 2.6 per bot-min = Rally urgency 0
  inside `RallyNearUU` (`BNGameMode.cpp:1340`) -> veto -> Roam -> drift -> Rally (game-side
  objective shape). Per-map routes confirmed (`0 grapple routes for BR_Spillway (8 filtered
  out)`); Arena's projected approach points still 15 % of stall seconds. Stuck = 65 stalls x
  ~1.3 s per bot-match; the 10 s bar needs < 8 stalls, not a shorter cap. Pile-up PASS instant-
  level in 10/10. Phase 13 overlap 12.7 / 3.1 s, yields 3.9 / 1.5 per bot; Phase 14 `lanes=none`
  18 % Spillway / 0 % Arena01. Baselines carry no overlap/route/flank keys — "vs baseline" for
  Phases 13–15 is absolute-only until a v3 baseline is cut.
  CLOSE-OUT (lead): boxes 1, 2 ticked; box 3 idle/stuck/fraction FAIL, single sweep + kills
  PASS; box 4 FAIL at 5 s (6 of 7 within 36 s); box 5 no open high in the plugin. Phase 11 stays
  OPEN on the idle HARD bar with three named follow-ups: (a) walk toward a fresh draw during the
  3 s give-up instead of standing (or abandon at 1 s when no progress at all); (b) Retreat's
  DEFEND hold = a named still tactic or a strafe-hold; (c) the interior-lip blacklist. Game side:
  the empty-hand weapon swap (bn-builder, high) and the Rally urgency shape at RallyNearUU.

### 3 Sep (cloud) — follow-ups (a), (b) and (c), all three

**WRITTEN, NOT COMPILED.** Terminal owes rung 1, `AIBot.Sim.IslandLatch` (six new pins),
and a v7 run against the v6 medians. These attack the one HARD bar still unmet — idle
`tactic=none` 25.6 s Spillway / 44.9 s Arena01 per bot per 300 s, against a bar of 0 —
from three directions, and one of them is a measurement correction rather than a fix.

**(b) Retreat's DEFEND stand-down is now a named still tactic** (`EAIBStillTactic::Defend`,
bit 7). This is the SMALLEST change and possibly the largest number: breaking contact to
the band's floor with the threat in sight and standing to fight is the design's own answer
(founder, 28 Aug — stop jogging away with your back turned), and every second of it was
being charged to `tactic=none`. Every other intentional stand already had a name; this was
the last one that did not. Expect a real drop in the idle figure that is **bookkeeping, not
behaviour** — read it as such, and if the v7 idle number falls mostly on this, the bar was
partly measuring a decision.

**(a) The drift reflex** — the actual behaviour fix. A bot that abandons a goal takes a
failure strike, its want rests for the suppression window, and until something scores
again it stands with no tactic at all. That nameless remainder is the bug, and no branch
can fix it from the inside *because the failure is that no ambition is running*. So it
sits in `Think` beside the draw reflex, ambition-blind: no named stand, no move in flight,
still for `DriftAfterIdleSeconds` (1.5 s) → walk to a fresh reachable point within 1200 uu,
throttled to one nav query per 2 s. A mover issuing its own goal next tick simply wins —
this is filler, not a decision. **`Stranded` is excluded by the tactic gate deliberately**:
a confirmed island with no legal lip is a MAP defect the verifier must keep seeing, and a
bot shuffling contentedly around its island would hide it.

**(c) The interior-lip blacklist** — the gantry residual. The lip fan is deterministic from
the same feet *by design* ("the same feet must find the same lip"), which is exactly why a
failure had to be remembered: without it the bot re-picks the one door that does not open
on every entry. A failed lip is refused for 20 s within 120 uu, held in a four-slot ring on
the island latch (both movers run this recovery, and a StateTree recreates task instance
data — the same reason the sweep budget and the flank latch live there). Bounded three
ways on purpose: an unbounded blacklist is a leak that would eventually refuse every lip on
the map and strand the bot by the mechanism meant to free it. Two rules keep it honest —
only a lip the bot was **actually trying** is blamed (a horizontal walk to the mesh has no
lip), and a lip the body **successfully left** (airborne off the edge) is never blacklisted
whatever the landing turns out to be.

v7 gates: idle `tactic=none` down, with the Defend share reported SEPARATELY so the
bookkeeping and the behaviour can be told apart; `drift` lines present and each followed by
motion; no bot blacklisting more than a few lips per life; longest stall not worse; the
gantry's one interior-lip bot leaves.


### 3 Sep 2026 — W-VERIFY v7 launched (mac terminal, lead)

Follow-ups (a) drift reflex, (b) DEFEND named, (c) interior-lip blacklist landed in
`e2bc3eb8`; the code is in and the measurement was owed. `Tools/aib/aib22_verify.sh v7`
started 13:41 — 5 x 300 s matches per map, both maps parallel, judged by
`80_aib_metrics.py` against the 2 Sep v2 baselines.

Reading the result per this ticket's stated v7 gates: idle `tactic=none` down with the
Defend share reported SEPARATELY (bookkeeping vs behaviour must be distinguishable);
`drift` lines present and each followed by motion; no bot blacklisting more than a few
lips per life; longest stall not worse; the gantry's interior-lip bot leaves.

The gantry leg is EDITOR-LIVE and is NOT covered by this headless run. `unreal-mcp`
refused connection at session start (the editor was not yet running; it is now, serving
127.0.0.1:8000 from pid 1878). That capture stays owed until the MCP client reconnects.

Also committed this session (`8b13f767`): the v4/v5/v6 verify baselines, which the
roadmap status table already cited as evidence while they were untracked.

### 3 Sep 2026 — W-VERIFY v7 RESULT: still OPEN. (a) proven, (c) unverifiable, stuck_seconds is the new headline

Headless rung only: 5 x 300 s per map, `-nullrhi -unattended`, seeded batch. All ten
matches ran the full 300 in-game seconds. NOT PIE, NOT multiplayer, NOT the gantry.

**(a) drift reflex — PASSES, cleanly.** 223 drift events across both maps, **zero** with
no motion, median walk 833 uu, minimum 100 uu. The line carries its own evidence
(`drift — 1.5s still with no tactic, walking 567uu`), so the gate is self-proving.

**(b) DEFEND named — PASSES as bookkeeping.** The split the gate asked for now exists:

| | idle `tactic=none` (median) | idle tactical/DEFEND (median) | v6 idle tactic=none |
|---|---|---|---|
| Spillway | 22.8 s | 53.9 s | 25.6 s |
| Arena01  | 38.5 s | 58.3 s | 44.9 s |

So the naming worked and idle-with-no-tactic did fall (25.6 -> 22.8, 44.9 -> 38.5), but
the movement is marginal and **the HARD bar is 0.0. Both maps FAIL it.** Most standing is
now *named* rather than *eliminated* — which is what (b) was for, and is not the same
thing as fixing it.

**(c) interior-lip blacklist — UNVERIFIABLE from this run.** Zero occurrences of any
blacklist-shaped token across all ten logs. That is either "never needed" or "never
fires", and this run cannot tell them apart because the path emits no line. It needs a
log line before any future run can judge it.

**The headline defect is stuck_seconds, and it is not new behaviour from (a)-(c).**
Median 63.5 s Spillway / 57.6 s Arena01 per bot per 300 s match — roughly a fifth of every
match — worst single bot 150.1 s, against a PROVISIONAL bar of 10.0. `stall_abandoned_count`
median 12 per match says the abandon path is firing constantly rather than exceptionally.

Longest single stall: median 3.0 s (v6 3-4 s, so not worse), but worst 3.9 / 4.2 s still
over the 3.0 bar. Sweep gates all PASS (longest single sweep 2.0 s vs 2.5 bar; sweep
fraction 0.035 / 0.017 vs 0.05). Refusals 0 on Spillway; Arena01 REGRESSED — median
per-bot `no_path_requests` 30.0 against a baseline median of 0.

kills/min 13.2 / 16.6 vs baseline 1.0 / 1.8 — not worse, by a wide margin.

**Verdict: AIB22 stays OPEN.** (a) is done and provable. (b) is done as bookkeeping and
did not clear its bar. (c) cannot be judged. `stuck_seconds` is now the thing standing
between this ticket and its HARD bar, and it is a bigger number than idle ever was.

Evidence: `Tools/aib/baselines/aib22-{spillway,arena01}-verify-v7.json`,
`Tools/Logs/aib22-verify-*-v7-*.log`.

### 3 Sep (cloud) — reading v7 back: two defects of mine fixed, and one correction to the verdict

**Two defects in my own v7 code, both real, both fixed (WRITTEN, NOT COMPILED):**

1. **(c) could not be judged because I never gave it a voice.** The blacklist path emitted
   nothing, so "zero blacklist-shaped tokens across ten logs" was unfalsifiable — exactly
   the failure AIB19's own log-level ruling names, which I wrote and then broke. Two lines
   now: the FAILURE at `Log` (`lip blacklisted (x,y,z) for 20s — the step-off did not
   leave the island`), which is the countable event, and the REFUSAL at `Verbose` (`lip
   refused … trying another door`), which is the other half — a blacklist that never
   refuses anything is doing nothing, and only that line separates the two.
2. **The drift reflex threw away `MoveToLocation`'s result and printed its line anyway**,
   so a REFUSED drift would have reported itself as a walk. It now branches: `drift
   REFUSED — …reachable draw the mover would not path (F7)` against the walk line. This
   matters for the point below.

**A correction to the v7 verdict, from the committed baselines.** The write-up calls
`stuck_seconds` "the headline defect" and says it is not new from (a)-(c) — the second half
is right and the first is worth restating, because the numbers locate the regression
precisely and v7 is not where it happened:

| median per bot | v5 | v6 | **v7** |
|---|---|---|---|
| `stuck_seconds` Spillway | 19.7 | 85.9 | **66.8** |
| `stuck_seconds` Arena01 | 14.4 | 82.2 | **56.6** |
| `stall_abandoned_count` Spillway | 2 | 18 | **12** |
| `stall_abandoned_count` Arena01 | 2 | 16 | **9** |

`stuck_seconds` went 4-6x at **v6**, not v7 — the regression window is v5→v6 (fix #6/#7/#8
and the crowd re-enable), and **v7 moved it back down 22% / 31%**. So the next pass should
bisect v5→v6, not audit (a)-(c). It is the right headline; it is a v6 headline.

**And one thing the write-up did not flag, which IS a v7 regression and is probably mine:**

| median per bot | v5 | v6 | **v7** |
|---|---|---|---|
| `no_path_requests` Arena01 | 0 | 1 | **22** |
| `no_path_requests` Spillway | 0 | 0 | **0** |

Arena01 only, and Arena01 is the map AIB28 has open for missing drop links. **Prime
suspect: the drift reflex**, by a mechanism that is not the obvious one — it does not log
through `MoveToNavPoint`, so it cannot inflate this counter directly. What it can do is
put bots somewhere new: a bot that used to STAND in a well-connected spot now walks
1200 uu, and on a map with known connectivity holes the TASK moves issued from where it
lands are what get refused. `GetRandomReachablePointInRadius` promises reachability from
where the bot is standing, which is a weaker promise than it sounds on a map with islands.

That is a hypothesis with a mechanism, not a diagnosis. What settles it, cheaply:
- the new `drift REFUSED` line — if drifts themselves are being refused on Arena01, the
  draw is landing on unreachable ground and the fix is the draw, not the reflex;
- correlating `move REFUSED` timestamps against `drift` timestamps per bot;
- one run with the reflex disabled (raise `DriftAfterIdleSeconds` past the match length).

If it is the drift reflex, the fix is a tighter draw — path-test the point, or refuse to
drift while the island latch reads `bOnIsland` (today only `Stranded` is excluded).

**Parser (aib-editor's, filed not edited — law 5):** `80_aib_metrics.py` has no counter for
any of v7's new lines. To judge the next run it needs `drift`, `drift REFUSED`, `lip
blacklisted`, and `draw reflex` alongside the existing ones — without them (a) and (c) can
only be read by grepping raw logs, which is how (c) went unjudgeable this round.


### Log — 2026-09-03 (cloud): the v5→v6 `stuck_seconds` bisect, and the discriminator it needs

The correction above said the next pass should bisect v5→v6. This is that pass. It is a
**code-read bisect, not a measured one** — the finding names a mechanism and the run that
would confirm it; nothing here has been run.

**The window.** `edbc094c..b84f618f` (v5→v6) carries fix #7 (lip landing validation), fix
#8 (crowd re-enable, corpse claims, the 3 s give-up window, the Engage melee floor, the
flank hold) and the per-map grapple routes.

**The suspect: fix #8's crowd re-enable.** Before it, a bot whose crowd manager was not
ready at possession ran with separation off *for its whole life* — the module said so
itself (`crowd simulation DISABLED — no crowd manager or navmesh at possession; separation
is off for this life`). Fix #8 added `ApplyCrowdSettings()` and `bCrowdRetryPending`, so
**v6 is the first run in which crowd avoidance actually engaged for most bots.**

**The mechanism, confirmed in the detector.** `TickLocomotion` measures progress as pure
displacement (`FVector::Dist(Here, State.BestPoint) > WedgeProgressUU`). A crowd brake
produces exactly zero displacement, so it feeds the stall clock the same as a wall does.
That is not an oversight — Phase 13's own review made it the design: *"the stall clock
KEEPS RUNNING through a yield — only the sprint and the verdict wait for the window — so a
doorway pair still reaches its abandon on schedule and `stuck_seconds` reads the whole
stand, not 0.03s of it."* Past `MoveGiveUpSeconds` the goal abandons, which is the other
half of the v6 shape: `stall_abandoned_count` 2 → 18 / 16.

**The two instruments disagree, on purpose, and nobody wrote it down.** `idle_seconds`
EXEMPTS a crowd brake (Think names it `Crowd` so separation is not read as standing);
`stuck_seconds` COUNTS it. So a v6 bot standing in a doorway while its neighbour clears is
simultaneously "not idle" and "stuck". Both are defensible; together they mean
**`stuck_seconds` is not a wedge counter, it is a no-ground-gained counter** — and a 4-6x
jump in it across a release that switched separation on is, so far, indistinguishable from
separation working.

**Why it cannot be settled from the committed logs.** The `stall over` line carried
seconds, position, goal, `jumped=` and `resolved=` — nothing that separates a body braked
by a teammate from a body ground into geometry. Every v5/v6/v7 baseline is therefore
silent on the actual question.

**Landed (WRITTEN, NOT COMPILED):** `still=` on the `stall over` line — the union of the
named stands that were up while that segment's seconds elapsed, e.g.
`… resolved=abandoned still=Yield|Crowd`. It is not a new guess: `Crowd` is set in `Think`
only on a still sample with a move request in flight, which is separation zeroing the
velocity; a body grinding on geometry keeps feeding movement input and never earns the bit.
Pipe-joined, space-free, appended at the end — the harness's existing `stall_over` regex is
unanchored and `stuck_seconds` / `max_stall_seconds` parse unchanged (verified against both
line shapes).

**Also fixed, a gap of mine:** `Defend` (the still-tactic I added for Retreat's stand-down)
was never added to `CloseIdleEpisode`'s naming chain, so a DEFEND stand printed
`tactic=none` — F9's instrument reporting an unnamed stand for a stand that is named. It is
in the chain now, ahead of `Hold`.

**What the v8 run settles.** Split the v8 `stuck_seconds` by `still=`:
- mostly `Crowd`/`Yield` → v6 is not a regression, it is separation being visible, and the
  bar for `stuck_seconds` is wrong rather than the bots being stuck. The fix is then to the
  METRIC (a crowd-exempt variant, matching `idle_seconds`), not to the locomotion.
- mostly `none` → a real v6 wedge regression, and fix #7's lip landing validation is the
  next suspect to read.

**contract_gap (parser, aib-editor's path — filed, not edited).**
`Tools/aib/80_aib_metrics.py` needs `stall_over` to capture the new `still=` group
(optional, so old baselines still parse) and to derive two counters beside `stuck_seconds`:
`stuck_seconds_crowded` (segments whose `still=` contains `Crowd` or `Yield`) and
`stuck_seconds_wedged` (the rest). Without them the split above has to be done by grep,
and the v5/v6/v7 baselines cannot be re-judged at all. This is the same filing as the v7
one above (`drift`, `drift REFUSED`, `lip blacklisted`, `draw reflex`) — one parser pass
covers both.

### Log — 2026-09-03 (cloud): the filed parser counters, built

**Lane crossing, on the record.** `Tools/aib/` is inside this packet's `owner_path`, so the
law-5 hook permits it — the constraint was the crew's one-writer convention, which names
aib-editor for this file. The founder's instruction ("do all that from here") is the grant;
recording it here so the file's history has a writer's name against it. Two filings were
open against this parser across two rounds, and neither was actionable by a run without it.

**Landed in `Tools/aib/80_aib_metrics.py`** (all additive; every prior baseline still loads
and every prior counter is byte-identical on the old line shapes — checked):

1. **`still=` captured as an OPTIONAL group** on `stall_over`, and `stuck_seconds` split
   into `stuck_seconds_crowded` / `_wedged` / `_uncaused`. Crowded = the segment carried
   `Crowd` or `Yield`; wedged = any other named stand or none; **uncaused = a line with no
   `still=` at all**, which is every baseline committed before today. The three sum to
   `stuck_seconds` exactly and the self-test asserts the invariant per bot — so a future
   line-shape change that slips past both arms fails the test instead of silently reporting
   0/0, which a reader would take for "no stalls" rather than "no cause recorded".
2. **The v7 lines counted**: `drift_count`, `drift_refused_count`, `lip_blacklist_count`
   per bot (all Log level), and `lip_refusals` / `draw_reflexes` at match level with the
   `or None` idiom — Verbose-only, so they print "not captured (Verbose off?)" rather than
   0. That distinction is exactly the one (c) lacked when it went unjudgeable.
3. **`drift_refusal_correlation`** — the Arena01 settle, made arithmetic. A `move REFUSED`
   counts as downstream of a drift when the SAME bot drifted within 5 s before it (the walk
   is up to 1200 uu plus one decision). Refusals clustered after drifts implicate the
   reflex; refusals spread evenly exonerate it. Both arms are covered in the self-test.

**What I deliberately did NOT do: fix the drift reflex.** The write-up above called the
Arena01 `no_path_requests` 0 → 22 "a hypothesis with a mechanism, not a diagnosis", and
tightening the draw now would be firing at it blind — a change that could move the number
for reasons unrelated to whether the hypothesis was ever true, and that would then be
impossible to un-confound. The three settles I named needed instruments, not edits; two of
them now exist (the `drift REFUSED` line, landed earlier today; the correlation, landed
here). The third — one run with the reflex disabled — is a run, not a code change.

**Also documented:** `Tools/aib/baselines/README.md` now carries the caveat that every
committed baseline reads 0 crowded / 0 wedged and 100% uncaused, and that reading that 0 as
"separation was not involved" inverts the finding the split exists to test.

**Rung: this is the harness, run and passing** — both self-tests green (57 synthetic lines
in the module's exact formats, 31 hits), old baselines still load through `--baseline`, and
`stuck_seconds` / `max_stall_seconds` are unchanged on the pre-`still=` line shape. The
MODULE side of today's work (`still=`, the `Defend` naming fix) remains WRITTEN, NOT
COMPILED — no UE toolchain in the cloud.

### 3 Sep (Windows terminal, aib-critic) — W-REVIEW

Read-only pass on `640092f5` over everything AIB22 Phase 11 landed since 2026-09-01 plus
AIB26's `bSameFight` fix, which shares the `ThinkTactic` block. Four surfaces:
containment, fairness, utility pathologies, server-only. **ONE HIGH, four MEDIUM, two LOW.**

---

**HIGH-1 — follow-up (c), the interior-lip blacklist, is inert. It is erased one statement
after it is written, and it is not wired to the fan it was written for.**

`Execution/AIBStateTreeTasks.cpp:875-876` · `Core/AIBBotController.h:263-291, 350-364, 367-371`

Two independent defects; either one alone makes the feature a no-op.

*(a) Dead on write.* The only writer in the module is

    :875   Bot.GetIslandLatch().NotePendingLipFailed(WorldSeconds(Bot), AIB::FailedLipRefuseSeconds);
    :876   Bot.GetIslandLatch().Strand(WorldSeconds(Bot), Bot.GetTierRow().EgressCooldownSeconds);

`Strand()` -> `ClearWithCooldown()` -> `Clear()` -> `ForgetFailedLips()`, which zeroes every
`FailedLipUntil[i]` and resets `NextFailedLip`. Unconditional, same tick, next statement.
`RefusesLip` tests `NowSeconds < FailedLipUntil[i]`; after the wipe every slot reads `0.0`,
so it returns false for every lip, forever. The ring never holds an entry for one frame.

*(b) Not wired to the Egress fan.* `RefusesLip` has exactly one caller —
`FindVerticalGapStepOff:756`, the off-mesh **vertical-gap recovery**. `NotePendingLip` has
exactly one caller — `StartOffMeshRecovery:823`. `FindIslandLip` (`:2911-2980`), the 16-ray
navmesh-raycast fan the Egress task runs for a bot grounded **on** the mesh, neither records
a pending lip nor consults the blacklist. The residual (c) exists for — the FINAL gantry
watch's *"bot 4/5 repeat `egress starts — lip (1289,19xx,810) … drop 453` several times …
the step-off lands back on the platform"* — runs through `FindIslandLip` and
`LandedOnSameIsland` (`:3170-3177`), a path with no blacklist call of any kind. So even with
(a) fixed, the gantry case stays uncovered.

*Repro.* Place a bot on `BR_LM_The_Gantry_01` (Arena01, top z 800) at the current tree,
`-LogCmds="LogAIBot Verbose"`. It latches, confirms, `FindIslandLip` returns the interior lip
nearest the feet, steps off, `LandedOnSameIsland` fires `island egress FAILED — landed on the
same island (F7)` + 5 s cooldown. The fan is deterministic from the same feet **by this
file's own design statement**. After the cooldown the latch re-forms from the same feet and
the fan hands back the same lip. Loop until the match ends — the founder's box 4 defect, by
the mechanism (c) was written to remove.

*Falsifier, cheap:* `lip refused … trying another door` (`:761`, Verbose) **cannot appear in
any log produced by this build**. `lip blacklisted` (`:871`, Log) *can* appear, from the
recovery path, having changed nothing. That asymmetry is the signature, and it is also a
false instrument: `lip_blacklist_count` landed in `80_aib_metrics.py` this session and will
count events with zero effect, so a v8 run reading "the blacklist fires N times" concludes it
works. The v7 write-up's "zero blacklist-shaped tokens — never needed or never fires" is now
answerable from code alone: **never fires, and could not have.**

---

**MED-2 — `ResetArbitration()` wipes the whole tactic engine's failure memory, so Hold's
re-election bound and Flank's pathfind throttle are both bypassed by re-entry.**

`Core/AIBBotController.cpp:1172` · `Brain/AIBAmbitionEngine.cpp:72-79` ·
`Core/AIBBotController.cpp:1141-1146, 1208-1212` · `Execution/AIBStateTreeTasks.cpp:2766-2776`

Direct answer to "verify `HoldMaxSeconds` actually bounds": it bounds one EPISODE. The clock
fix itself is sound — `HoldSinceSeconds` now survives a Search flap, and
`HoldStationTask::Tick` fires `NoteHoldOver` + `NoteCurrentTacticFailed` at the bar. What
bounds *re-election* is the engine's escalating `3 s x Strikes` suppression.
`ResetArbitration()` calls `Failures.Reset()`, whose own docstring scopes it to unpossession
("strikes die with the body"), and `ThinkTactic` calls it on every non-Engage excursion that
is neither Search nor flank-holding. At the measured 42-45 switches per bot-minute with veto
at 60 %, that is ~0.7 wipes/s per bot.

*Scenario A (Hold).* Engage>Hold 4 s -> `hold over` -> Hold suppressed 3 s -> one veto think
to Retreat -> `ResetArbitration()` -> the strike record is destroyed, not aged -> back to
Engage -> Hold re-elected on a fresh clock. `Record.Strikes` can never reach 2, so the
3->6->9 s escalation the engine advertises never happens for any tactic.

*Scenario B (Flank — the costlier one).* `SearchFlankPoint` runs whenever Ambition==Engage
with a visible target, no latched point, no `bDone` and no suppression — it does **not**
require Flank to be the elected tactic, so every bot in a fight pays it. Cost: 16
`ProjectPointToNavigation` + up to 16 `LineTraceTestByChannel` + up to 16 cost-unlimited
synchronous `FindPathSync` (`:1116`). The comment at `:1143-1145` names the invariant it
relies on — *"the suppression window is what throttles the next search (escalating on
repeat), so eight pathfinds never run per think."* `ResetArbitration()` breaks it: think N
searches and takes a strike; think N+1 vetoes out of Engage and wipes it; think N+2 returns
and searches again — a full fan every ~0.2-0.3 s per bot, synchronous, game thread, x7 bots.
`ClearFlankLatch` on the same line also clears `bDone`, so the arrival mark is no throttle
either.

*Scoped honestly:* the AIB26 fix under review **strictly reduced** this — the wipe used to
run on every non-Engage think and now skips Search flaps and young flank latches. This is the
residual, not a regression. Medium because each Hold episode is bounded and the pathfind cost
is a tail-latency hazard, not a correctness break. It becomes `high` if v8 shows per-bot
Engage re-entries above ~1/s with `flank point latched` absent (the fan running, finding
nothing, repeatedly).

---

**MED-3 — `LandingLeavesIsland` decides from ONE anchor: the shape this ticket already ruled
`high` once (fix #6 F6-1).**

`Execution/AIBStateTreeTasks.cpp:2891-2909`

The loop `return`s the result of the **first anchor that projects to nav**, over a list
sorted by distance to the landing. `FAIBIslandLatch::Confirm` (`Core/AIBBotController.h:322-338`)
asks the mirror question over the *same* list and was explicitly rewritten by fix #4 R1 and
fix #6 F6-1 to require ALL anchors — *"island iff NONE has a full path… a spawn pad that is
itself an island then confirms correctly."* Two decisions, one anchor list, two rules, and
the weaker one gates the founder's box 4.

*Scenario.* Arena01. This ticket's own map filing (W-VERIFY v3 breakdown C1, filed for
arena-architect) says **the corner spawn pads are islands**. A bot on the gantry evaluates a
GOOD lip whose landing is healthy floor. `Anchors.Sort` puts an island spawn pad first
because it is geometrically nearest that landing. `TestPathSync(landing -> island pad)` finds
no full path -> `LandingLeavesIsland` returns false -> the good lip is skipped
(`lip … skipped — landing still islanded`) -> the fan exhausts -> `Latch.Strand()` ->
`stranded — no legal lip` -> the bot stands on the gantry for the cooldown and repeats. That
is the FINAL gantry watch's "bot 5 still on the gantry", with a mechanism.

Medium, not high, because the anchor geometry is unproven from a code read — it needs the log
to name which anchor was tested. **One-line falsifier:** the skip line at `:2970` does not
print the anchor it tested. Print it and the next gantry watch settles this without a guess.
(The single-test cost motive in the docstring is real; the fix must bound the number of
tests, not drop the bound.)

---

**MED-4 — the drift reflex is not an oscillator, but its trigger clock is reset by any
one-frame flicker of any still-tactic bit, so it starves on exactly the flapping bots.**

`Core/AIBBotController.cpp:1547-1585` (reflex) · `1270-1287` (episode) ·
`Execution/AIBStateTreeTasks.cpp:1103` (an unhysteresised toggle)

On the founder's question — **no drift/refuse/drift-back oscillator.** `NextDriftSeconds =
Now + DriftRetrySeconds` is set BEFORE the draw and unconditionally, so it is one nav query
per 2 s per bot whether the draw lands, is refused, or finds nothing; and
`GetMoveStatus() != Moving` keeps it from fighting a mover. Attacked and did not break.

What does break: the gate reads `Now - IdleSinceSeconds >= DriftAfterIdleSeconds`, and
`IdleSinceSeconds` restarts whenever the still-tactic SET changes (fix #3 HIGH-1's rule —
right for the instrument, wrong as a drift trigger). `FAIBMoveNearBeliefTask::Tick:1103`
writes `SetStillTactic(Hold, bInFightRange)` from a bare `IsWithin(Belief, FightRangeUU)`
with **no hysteresis**, every frame, against a MOVING belief. A bot whose target hovers at
the fight-range boundary toggles the Hold bit at frame rate; each toggle closes and reopens
the episode at the next 10 Hz Think; `Now - IdleSinceSeconds` never reaches 1.5 s and the
reflex never fires. The same mechanism explains the v6 breakdown's *"43 / 34 % of idle is in
sub-1 s transition spells"* — that is the instrument chopping, not the bot moving.

Shape of the fix (not mine to write): the reflex wants its own "seconds since the tactic set
was last non-empty" clock, distinct from the reporting episode; `:1103` wants a band, not a
line.

---

**MED-5 — the drift reflex can walk a bot around its own island with `tactic=none` never
accumulating, blinding this ticket's HARD gate on its own defect.**

`Core/AIBBotController.cpp:1547-1554` · `Core/AIBBotController.h:360-371`

The reflex is gated on `StillTactics == 0`, and (c)'s write-up says `Stranded` is excluded
deliberately so a map defect stays visible. But `Stranded` is set only by `Latch.Strand()` —
the lipless-confirmed-island case. Every OTHER Egress failure (`landed on the same island`,
`lip Nuu short`, `landed N below the lip`, the lip-walk give-up) calls `ClearWithCooldown`,
which does **not** set `bStranded`. So a bot that fails egress the ordinary way has
`StillTactics == 0`, drifts every 2 s, and `GetRandomReachablePointInRadius` on an island
returns island points: the bot shuffles around its platform indefinitely with
`idle tactic=none` ~ 0. `idle_seconds == 0` is this ticket's HARD bar; a bot that never
leaves its platform would pass it.

Weakened honestly: Wander's longest-partial walk (H1) already produces motion on an island,
so the reflex is not the sole cause, and v6/v7 measured latches at 0-1 per bot in normal play
— this is a gantry/placed-bot case today. The mitigation is a metric (`island_seconds`
reported beside `idle_seconds`), not a behaviour change.

---

**LOW-6 — the island path tests use the DEFAULT query filter; every move the bot issues uses
its own.** `Execution/AIBStateTreeTasks.cpp:2842, 2905` build
`FPathFindingQuery(Bot, *NavData, …)` with no filter, so the engine substitutes
`NavData->GetDefaultQueryFilter()` (`NavigationData.cpp:49-57`). AIB25 W-REVIEW M3 ruled the
opposite for the flank test (`AIBBotController.cpp:1116-1117` passes
`DefaultNavigationFilterClass`). Benign TODAY: `UAIBQueryFilter::InitializeFilter`
(`Core/AIBQueryFilter.cpp:14-30`) only calls `SetAreaCost` and never excludes an area, so
reachability is identical under both. It stops being benign the first time a lane class is
made impassable for a tier. Risk register.

**LOW-7 — the ambition sentinel's `ClearStillTactics()` also drops the `Reload` bit.**
`Execution/AIBStateTreeTasks.cpp:988-992` vs the tactic sentinel at `:996-1000`, which
deliberately keeps `EAIBStillTactic::Reload`. On an ambition change while `FireWhenAble` is
mid-reload-crouch the crouch stays rented (`bCrouchedToReload` is instance data, untouched)
but the F9 label is dropped, so those seconds are charged to `tactic=none`. Bounded by the
reload, and it biases the idle number PESSIMISTICALLY, so it hides nothing. Risk register.

---

**PASS — CONTAINMENT.** Clean at the linker and at the grep. `AIBot.Build.cs` names engine
modules only; `GameplayAbilities` absent. Zero hits for
`Breachpoint|BNGame|BNAIB|BRChar|ABR|UBR|GameplayAbilit|AbilitySystem` anywhere under
`Plugins/AIBot/Source/AIBot/` outside the Build.cs comment that explains the ban. Zero
dependencies from the plugin onto `Source/BreachpointNext/`, adapter or otherwise. `Brain/`
and `Skills/` name no `UWorld`, no `AActor`, no `GetWorld`, no trace, no FX type — the
headless law holds, and the new island fact does not leak into them (no island field on
`FAIBFacts`, no island selector in `SelectFact`). *Handoff, not a finding:* four game-side
files name AIBot symbols outside the sanctioned `AIBotAdapter/` seam —
`Characters/BNCharacter.cpp:5`, `Characters/BNHealthComponent.cpp:6`,
`Weapons/BNProjectile.cpp:3`, `Match/BNGameMode.h:6`. Direction is game->plugin, which the
Build.cs explicitly permits, and all four predate this diff. Named for whoever owns the
seam's shape.

**PASS — FAIRNESS.** Attacked and did not break:
- *R11's 200 ms floor.* One clamp site, `FMath::Max(Drawn, AIB::MinReactionSeconds)` with a
  NaN guard, `Perception/AIBReactionClock.cpp:19-20`, `MinReactionSeconds = 0.20f`. Nothing
  in this diff adds a stimulus path around `Push()`; the Phase 12 team callouts enter the
  bot's OWN clock and mature on its own draw.
- *The island fact as a wallhack.* `GetIslandAnchors` (`AIBBotController.cpp:979-1018`) reads
  PlayerStarts, objective POIs and the bot's own remembered last-known. No live enemy
  position, no enumeration. It is consumed by `FAIBOnIslandCondition` and Wander only — it
  does not reach aim or target selection, which the audit's L6 required.
- *The flank hold leaking into the trigger.* `bFlankHolding` ORs into the `TargetVisible`
  **selector** (`Brain/AIBConsideration.cpp:25-28`), which is consideration scoring only. The
  fire gate is `Facts.bTargetVisible && Facts.bWeaponCanFight && Senses.HasVisibleTarget()`
  (`AIBStateTreeTasks.cpp:1481`) — the raw fact AND a live sensorium read. No shooting on a
  belief, no grenade past a matured memory.
- *New traversal powers.* Egress is walk + gravity: the step-off is a projectionless
  `MoveToLocation` and the fall is unsteered. Every crossing is filtered by
  `FAIBTraversalPolicy::Choose` (`:2955-2961`, `:743-749`); the blind watchdog hop is gone
  (fix #4 R9). No cost is skipped that a human pays.
- *The world query surface.* `QueryVisibleEnemies` is radius-bounded and named for
  visibility; `CountNearbyAllies` returns a count; `AreEnemies` is a relation test on a
  candidate the bot already believes in; `GetGrappleRoute` carries map traversal data and no
  enemy information.

**PASS — SERVER-ONLY.** Zero hits for
`Replicated|GetLifetimeReplicatedProps|DOREPLIFETIME|NetMulticast|UFUNCTION(Server`. Zero
FX: no `GameplayCue`, `Niagara`, `SpawnEmitter`, `PlaySound`, `UGameplayStatics` anywhere in
the module. Law 4 holds — `PrimaryActorTick.bCanEverTick = false` on the controller
(`:50-51`) and on `AIBLaneVolume`; thinking rides `ThinkTimer` at `ThinkIntervalSeconds`; the
`Tick` overrides in `AIBStateTreeTasks.cpp` are `FStateTreeTaskBase::Tick`, the executor's,
not an actor's. Every entry point is authority-guarded (`Think:1248`,
`AIBPathFollowingComponent.cpp:70`, and the `NM_Client` early-outs across `AIBBotManager` and
`AIBTeamCoordinator`).

---

**VERDICT: the `W-REVIEW: no high` box CANNOT be checked.** HIGH-1 blocks under R13. It is
provable from the code alone — no map assumption, no run needed — and it is the founder's own
named residual (c) for the one box (4) this ticket has never met. MED-2 through LOW-7 go to
the risk register with the artifact.

### 3 Sep (Windows terminal, aib-verifier) — v8 measured

**RUNG 3, headless seeded batch.** NOT PIE, NOT a packaged build, NOT listen+client, NOT a
human playing. 5 x 300 s matches per map, `-nullrhi -unattended`, 7 ODST bots + no human,
`MinPlayers=0`, `LogAIBot Verbose`. All 10 matches reached `match_seconds` 299.8-300.0.
First run in which the 3 Sep cloud fixes are actually in a binary:
`Plugins/AIBot/Binaries/Win64/UnrealEditor-AIBot.dll` 20:24:01 vs source 20:13:13.

**PROTOCOL DEVIATION, declared.** The `-server` form named in this ticket's W-AUDIT member 3
protocol is DEAD on Windows too, exactly as it was on the Mac. Pilot
`Tools/Logs/aib-v8-pilot.log`: engine exits at frame 400 via
`FPlatformMisc::RequestExit(0, FEngineLoop::Tick.Benchmarking)`, **4 `LogAIBot` lines total**,
no bots. The `-game -windowed -nullrhi` fallback that `Tools/aib/aib22_verify.sh` uses gives
~100k `LogAIBot` lines per match, so that is the form these 10 runs used — identical to v7's,
which is what makes the v7-vs-v8 delta attributable to the code. `?ScoreLimit=200` kept, as
v7 had it. `-FixedSeed` DROPPED: the ticket records that AIB has no match seed and the lobby
seed is `map+clock`; pinning the global FRand risked turning n=5 into n=1.

**Parser self-test: PASS** (`python Tools/aib/80_aib_metrics.py --selftest`, exit 0) — both
blocks, 57 lines / 31 hits and the AIB23 block, including the new
`stuck_seconds_crowded + _wedged + _uncaused == stuck_seconds` partition assertion.

Evidence: `Tools/Logs/aib-v8-{spillway,arena01}-{1..5}.log`,
`Tools/aib/baselines/aib22-{spillway,arena01}-verify-v8.json`.

#### The HARD/PROVISIONAL bars

| gate | Spillway | Arena01 | bar | verdict |
|---|---|---|---|---|
| idle `tactic=none`, median/bot/match | **15.2 s** | **12.0 s** | 0.0 HARD | **FAIL** |
| idle `tactic=none`, worst bot | 31.4 s (bot 5, log 1) | 25.6 s (bot 2, log 5) | 0.0 HARD | **FAIL** |
| longest single sweep | 1.9 s | 1.5 s | 2.5 HARD | PASS |
| sweep fraction of match | 0.018 | 0.0103 | 0.05 HARD | PASS |
| `stuck_seconds` median/bot | **72.9 s** | **53.0 s** | 10.0 PROV | **FAIL** |
| `stuck_seconds` worst bot | 103.6 s | 79.7 s | 10.0 PROV | **FAIL** |
| longest single stall | 4.0 s | 3.4 s | 3.0 PROV | **FAIL** |
| claim thrash | 0 | 0 | 2 PROV | PASS |
| refusals vs baseline (median/bot) | 0.00 vs floor 35.00 | 1.00 vs floor 0.00 | <= 0.5x base | PASS / **FAIL** |
| kills/min vs baseline | 8.60 vs floor -5.657 | 17.40 vs floor -5.097 | >= base - spread | PASS |
| F1 reaction floor | 0.233 s | 0.233 s | 0.20 s HARD | PASS |
| unserved wants / wiring / FFA grants / pile-up | 0 | 0 | 0 | PASS |

idle `tactic=none` is the best it has ever been and still misses a bar of zero:
v6 24.6 / 42.0 -> v7 22.8 / 38.5 -> **v8 15.2 / 12.0**. The tactical share rose in step
(v7 53.9 / 58.3 -> v8 62.4 / 72.5), so most of Arena01's 26.5 s drop is standing that got a
NAME, not standing that stopped. kills/min did not regress: 8.60 / 17.40 against a v2
baseline of 1.003 / 1.800.

#### `stuck_seconds` — the split fires, and it says the OPPOSITE of the crowd hypothesis

`still=` is populated on every one of the 3,421 `stall over` lines in this batch, so
`stuck_seconds_uncaused` is **0.0 on both maps** — the field working, not a null result. Read
per the baselines README: a committed baseline's zero-crowded is "the cause was never written
down"; a **v8** zero-uncaused is "the cause was written down every time."

Seconds-weighted (5 logs x 7 bots each):

| | Spillway 2285.1 s | Arena01 1870.5 s |
|---|---|---|
| `still=none` | 1593.5 s (69.7 %) | 1393.2 s (74.5 %) |
| `still=Sweep` | 395.4 s (17.3 %) | 244.6 s (13.1 %) |
| `still=StrafeHold` | 92.8 s (4.1 %) | 176.0 s (9.4 %) |
| `still=Yield` (+combos) | 140.9 s (6.2 %) | 22.6 s (1.2 %) |
| `still=Crowd` (+combos) | 33.0 s (1.4 %) | 24.5 s (1.3 %) |
| `still=Stranded` / `Reload` | 29.5 s | 4.9 s |
| **-> CROWDED** | **173.9 s (7.6 %)** | **47.1 s (2.5 %)** |
| **-> WEDGED** | **2111.2 s (92.4 %)** | **1823.4 s (97.5 %)** |

**The crowd brake is NOT what `stuck_seconds` is measuring.** Separation accounts for 7.6 %
and 2.5 % of it. Seventy per cent of every map's stall seconds carry `still=none` — no named
stand of any kind was up: a body against geometry, which is what the counter was built to
find. The v6 jump was attributed to crowd avoidance engaging for the first time; on the first
run that can actually tell the two apart, that attribution does not hold at the seconds level.

**`still=Hold` appears ZERO times in 3,421 `stall over` lines** (StrafeHold 151 lines, Sweep
660, Crowd 37, Yield 55, Stranded 10, Reload 11). See the AIB26 entry — that is the AIB26
finding, measured here.

#### `drift_refusal_correlation` — the drift reflex is EXONERATED

| map | drifts | refusals | refusals within 5 s after a drift |
|---|---|---|---|
| Spillway (5 logs) | 170 | 1,190 | **0** |
| Arena01 (5 logs) | 111 | 2,192 | **103 (4.7 %)** |

Spillway: not one refusal in the window, over 1,190 refusals. Arena01's 103 are all in logs
4 and 5 (10 of 288, then 93 of 119); logs 1-3 are 0 of 1,785. Against the crude null — 15-37
drifts x a 5 s window is 25-62 % of a 300 s match — 4.7 % is far BELOW chance, i.e. refusals
are if anything ANTI-correlated with drifts. **The v7 hypothesis that the drift reflex drove
Arena01's `no_path_requests` 0 -> 22 is refuted by its own instrument.** Arena01's refusal
median also collapsed on its own: v7 30.0 -> **v8 1.00** per bot (mean 8.2, max 30, so the
variance is a lobby-level burst, not a per-bot standing rate). It still FAILS the bar only
because the v2 baseline median is 0 and half of 0 is 0.

#### UNJUDGEABLE, not zero

`lip_refusals` prints **"not captured (Verbose off?)"** in all 10 logs, and Verbose WAS on
(`draw_reflexes` 831-1,625 per log is Verbose-only and captured). `lip_blacklist_count` is 0.
So residual (c), the interior-lip blacklist, is **UNJUDGEABLE for the second run running** —
the path still emits no line, and this run cannot distinguish "never needed" from "never
fires." Same for `denial_throws`, `offmesh_self`, `offmesh_moments`, `ff_refused`.

#### Fairness spot-check (rides this PIE-equivalent run)

7,467 acquisition latencies, ODST draw `ReactionSecondsMin 0.22 / Max 0.34`
(`AIBTiers.cpp::MakeODST`), module floor 0.20 s:
- **below the 0.20 s floor: 0.** below the tier's own 0.22 min: **0**. Fastest sample
  `0.233s` (`AIBBotController_3 acquired BP_BNCharacter_C_13 after 0.267s reaction` is a
  typical line; 0.233 = 14 frames at the fixed 60 Hz step). **PASS.**
- ABOVE the tier max 0.34: **2,812 of 7,467 (37.7 %)**, out to 0.433 s. That is the bot
  reacting SLOWER than its row, never faster — not a fairness violation, so no `high`
  finding for aib-critic. Logged as an observation: the emitted latency is quantised to the
  frame and runs long, so the draw's stated ceiling is not the observed ceiling.

**VERDICT: AIB22 stays OPEN.** Four bars FAIL (idle-none, stuck_seconds, longest stall,
Arena01 refusals-vs-baseline), and the headline has changed shape rather than size: the split
proves `stuck_seconds` is 92-98 % geometry, not separation.
