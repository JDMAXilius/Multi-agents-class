# TICKET — AIB25: Phase 14 ROUTE VARIETY

> STATUS: open — cut 2 Sep 2026 by the lead (session 014esNfHwPnkiAJkRKBMwR7b) from
> `docs/AIBOT-ROADMAP-2.md` (approved; rulings in §5; law F9 motion is the default). Claimed
> when its W-AUDIT merge lands here.

Lane UNavArea classes from the blockout scripts, per-bot seeded UNavigationQueryFilter through the one mover door, team route-heat penalty, Engage tactics Push/Flank/Hold by utility with an EQS flank mid-point; a real match seed.

**Ordering law:** waves per `docs/AIBOT-WAVES.md`: W-AUDIT (read-only, one question each) →
merge → serial headers → W-BUILD with disjoint files → W-REVIEW ×4 → W-VERIFY vs the previous
phase's baseline. Metrics for this phase land BEFORE its behaviour (§4 of the roadmap).

## Kickoff (machine-checkable)
- requires: engine-installed; editor-live only for the steps that name it
- the previous phase's W-VERIFY verdict is logged in its ticket
- owner_path: aib-builder `Plugins/AIBot/Source/AIBot/` · aib-editor `Content/AIBot/`,
  `Tools/aib/`, `Tools/blockout/` · lead `Config/`, `docs/tickets/`, `docs/AIBOT-*.md`.
  Anything under `Source/Breachpoint*/` is a contract_gap raised to the founder, never edited here.

## Steps (in order) — refined at the audit merge
1. W-AUDIT (dispatched 2 Sep) → merge below.
2. Metrics + baseline for this phase.
3. Serial headers, then W-BUILD ×2 on disjoint files.
4. W-REVIEW ×4 (containment · fairness · utility pathologies · server-only); a `high` blocks.
5. W-VERIFY ×2 (specs ∥ headless seeded 4v4 vs baseline; PIE/listen rung as the phase names).

## Done when
- [ ] Merge logged; steps refined
- [ ] Builds PASS; specs PASS; no `high`
- [ ] The phase's metric gate PASSES vs the previous baseline; kills/min not worse

## Log
### W-AUDIT (aib-critic) — merged by the lead, 2 Sep; three deviations ADOPTED
- ONE filter class, not one per bot: `DefaultNavigationFilterClass = UAIBQueryFilter::StaticClass()`
  at `AIBBotController.cpp:265` covers the door AND the two raw MoveToLocation calls (:1718,
  :2270); `bInstantiateForQuerier=true` makes `InitializeFilter(NavData, Querier, Filter)` run per
  query with the controller as Querier → reads that bot's seeded lane weights + team heat and
  calls `SetAreaCost(NavData.GetAreaID(LaneClass), Cost)`. Optional per-request override param on
  MoveToNavPoint only if Flank wants a different filter than Push.
- Lanes = the generator's existing folders (05_Culvert · 10_Tower · 30_SouthLane · 40_Gallery ·
  50_Yard), id = folder ordinal. NOT `ANavModifierVolume` (a Brush; unplaceable via MCP):
  `AAIBLaneVolume` in the plugin = AActor + UBoxComponent + UNavModifierComponent(AreaClass),
  placed by `land_spillway.py` with the `SpillwayGenerated` tag for the idempotent clear. ≤6 lane
  areas (uint8 area ids); link areas stay NavArea_Default (never price egress).
- Heat = LANE BUCKETS, not a cell grid: `SetAreaCost` is per area class; a grid needs the
  `FRecastQueryFilter::getVirtualCost` path (WITH_RECAST, per-poly) — skipped. Per-TEAM
  `LaneHeatStamp[NumLanes]` on a server-only subsystem (refuse client worlds like AIBBotManager);
  writer = controller on move-accepted (path points vs lane AABBs), cleared on move-finished;
  cost `1 + HeatWeight*exp(-Age/Tau)` at read, Tau≈6 s, NORMALISED by the min lane heat. A world-
  wide store = enemy-route omniscience (`high`).
- Flank mid-point: NO EQS (AIBot has no EQS dep; the only EQS is the dead BR code). K=8 ring
  samples at FlankRadiusUU, ring offset seeded, keep those nav-projected + reachable, NOT visible
  from `Sensorium.GetLastSeenLocation()` (the belief — never the live actor/view; F2-B), lowest
  lane heat; none → Flank scores 0 (VETO). Detour clamp is load-bearing: returned path length /
  direct ≤ `MaxDetourFactor` 1.5 or Flank scores 0.
- Match seed: `UAIBBotManager::SetMatchSeed(int32)` called from `BNGameMode.cpp:139` (LEAD's own
  packet — Source/BreachpointNext is this session's path), `-AIBSeed=N` else a hashed clock,
  logged once. `GetUniqueID()` is NOT stable across runs — the manager hands each controller a
  stable `BotIndex` (spawn slot); `LifeSeed = HashCombine(HashCombine(MatchSeed, BotIndex),
  LifeIndex)`. Without BotIndex the seed is theatre. Lane weights drawn once per life:
  `1 + FRandRange(-0.3, 0.3)` from `RouteRandom` seeded with the existing prime pattern.
- Tactics: a SECOND `UAIBAmbitionEngine` instance (worldless, has hysteresis/commit/VETO) — not a
  new engine. Push (confidence↑, health↑, target health↓, allies↑); Flank (visible + mid-band
  distance, recent damage↑, allies-on-target↑ from Phase 12, gated by a hidden reachable point);
  Hold (ammo↓/cannot fight, at FightRange, height advantage) = a NAMED stillness tactic under F9
  with csv `HoldMaxSeconds`. Flank has the longest CommitSeconds (3–4 s). Tactic clocks/commit/
  lane stamps live on the controller or engine instance, never in task instance data (re-entry
  recreates it — the AIB22 lesson).
- Attack surfaces: lane classes/volumes must live in the plugin (`high` if game-side); flank trace
  against live location/view = wallhack `high`; heat shared across teams `high`; the filter never
  calls SetExcludedArea/SetIncludeFlags (partial-path storms hiding as "bad AI"); Push/Flank
  dither at the confidence boundary needs SwitchCostFactor + MinDwell (< 1 switch / 2 s).
- W-BUILD: serial header (filter param on MoveToNavPoint) → A filter+seed (AIBNavFilter new,
  BotController, BotManager) ∥ B tactics (Brain/AIBTactic.h new, Engage tasks, TreeAuthoring
  :176-189) ∥ C heat (Team/AIBRouteHeat new, Coordinator registration line) ∥ D aib-editor
  (gen/land_spillway, AIB_Tactics.csv, AIB_Lanes.csv, metrics regexes). Seed handoff in
  BNGameMode = lead packet.
