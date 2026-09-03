# TICKET — AIB22: Phase 11 EGRESS — no bot stands on a platform sweeping; roam the whole level

> STATUS: claimed — Claude (session 014esNfHwPnkiAJkRKBMwR7b), lead, 2026-09-02. Founder rulings
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
- [ ] Baseline report (5 runs, medians + spread) committed under Tools/aib/ before step 2
- [ ] Every platform on Spillway and Arena01 has a way down; the floor has a way up (link count)
- [ ] `idle_seconds == 0` outside named tactics; `sweep_seconds == 0`; `stuck_seconds` per bot
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
