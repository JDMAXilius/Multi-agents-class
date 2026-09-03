# TICKET — AIB22: Phase 11 EGRESS — no bot stands on a platform sweeping; roam the whole level

> STATUS: in-progress — mac terminal (lead, session 014esNfHwPnkiAJkRKBMwR7b) 2 Sep 2026 (061f8f82). Founder rulings and law F9 in `docs/AIBOT-ROADMAP-2.md` §5. Waves per `docs/AIBOT-WAVES.md`.
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
- [ ] Every platform on Spillway and Arena01 has a way down; the floor has a way up (link count)
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
