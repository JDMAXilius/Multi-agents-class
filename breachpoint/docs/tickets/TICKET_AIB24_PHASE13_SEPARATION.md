# TICKET — AIB24: Phase 13 SEPARATION

> STATUS: in-progress — lead (Mac, session 014esNfHwPnkiAJkRKBMwR7b) 2026-09-03 (8e324dce), founder ruling: all phases run in parallel with Phase 11, W-BUILD in isolated worktrees, merged serially behind AIB22 fix #4. Was: open — cut 2 Sep 2026 by the lead (session 014esNfHwPnkiAJkRKBMwR7b) from
> `docs/AIBOT-ROADMAP-2.md` (approved; rulings in §5; law F9 motion is the default). Claimed
> when its W-AUDIT merge lands here.

Detour Crowd with separation on every bot (no RVO), players as crowd obstacles, the jump hook on the crowd component, the wedge watchdog yields to teammates instead of jumping, no bot-vs-bot shoving; listen-server rung for the replication claim.

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
### W-AUDIT (aib-critic) — merged by the lead, 2 Sep (verified against the 5.8 headers)
- One-word plugin change: Phase 11's `UAIBPathFollowingComponent` rebases from
  `UPathFollowingComponent` to `UCrowdFollowingComponent` (include `Navigation/CrowdFollowingComponent.h`);
  ctor swap via `ObjectInitializer.SetDefaultSubobjectClass<UAIBPathFollowingComponent>(TEXT("PathFollowingComponent"))`
  at `AIBBotController.cpp:24-25` (the name is a literal in AIController.cpp:44).
- Setters (OnPossess): `SetCrowdSeparation(true)` (default false), `SetCrowdSeparationWeight(1.5, csv)`,
  `SetCrowdAvoidanceQuality(Medium)`, `SetCrowdPathOffset(true)`; leave query range 400, obstacle
  avoidance true, AffectFallingVelocity false (F6), RotateToVelocity true (TickLocomotion's facing
  writer agrees). NEVER the `SetAvoidanceGroup*` family (that is the RVO path — forbidden mixing).
- Ini: new `[/Script/AIModule.CrowdManager]` with exactly `bResolveCollisions=True` and
  `SeparationDirClamp=0.2` (without the clamp a bot brakes for a teammate behind it = conga);
  MaxAgents 50 stays (8 bots + 8 player obstacles).
- Players as obstacles: `ICrowdAgentInterface` on `ABNCharacter` (game side, AIModule already a
  dep), `RegisterAgent` in PossessedBy ONLY when `HasAuthority() && Controller is APlayerController`,
  unregister in UnPossessed/EndPlay. THE TRAP: bot pawns are ABNCharacter too — registering them
  makes every bot exist twice (follower + ghost obstacle at distance 0) → jitter/freeze = defect 3
  worse. Highest-risk line of the phase.
- Watchdog (`AIBStateTreeTasks.cpp:377-378`): `CountNearbyAllies(Pawn, ~80uu) > 0` (existing,
  fairness-reviewed) → no jump, no traversal chooser, no re-issue (a re-issued MoveTo resets the
  crowd corridor); log + one bounded yield window (~1 s) and let separation steer. REJECTED:
  `UCrowdManager::GetNearbyAgentLocations` — returns ENEMY positions with no LOS bound (wallhack door).
- Nothing in the plugin bypasses the crowd (no AddMovementInput/velocity writes; every mover is
  MoveToLocation). Frictions: `FAIBStrafeTask` re-issues MoveToLocation per 0.2–1.2 s leg →
  corridor churn; re-issue only when the destination moved > ~100 uu (measure
  `mean_pairwise_teammate_distance` first). Strafe Hold's StopMovement is a correct asymmetry.
- CROSS-PHASE, BINDING ON PHASE 11: under crowd, `UCrowdFollowingComponent::SetMoveSegment` calls
  Super ONLY when simulation is disabled, and PathPoints hold just start+end — a jump hook that
  reads PathPoints/segment flags goes SILENTLY DEAD. Phase 11's hook must be
  `StartUsingCustomLink(INavLinkCustomInterface*, const FVector&)` (virtual on the base; the crowd
  calls it by name and `FinishUsingCustomLink` resumes the agent) and/or the corridor read
  `NavMeshPath->PathCorridor[PathStartIndex]` + `GetLinkEndPoints` for jump areas. TICKET_AIB22
  step 4 updated below. Do not flip `SetOffmeshConnectionPruning`.
- Replication: clean by construction; the `HasAuthority` gate on RegisterAgent is mandatory (else a
  client crowd agent that steers nothing). Listen rung three-way claim: server separation drops
  overlaps; acting client moves identically; observing client sees smooth bot motion and
  `bUseAccelerationForPaths` still honoured (crowd routes through RequestPathMove).
- "Turn off bot-vs-bot shoving" has no flag that does that (`bEnablePhysicsInteraction` is
  pawn↔physics bodies; capsule ECC_Pawn must stay Block). Separation + `bResolveCollisions=True`
  IS the mechanism; residual shoving = a measured follow-up, not a flag.
- W-BUILD ×3 disjoint: (A) plugin steering — AIBPathFollowingComponent.{h,cpp}, AIBBotController.cpp,
  AIBDataRows.h + csv row; (B) game side (LEAD, Source/BreachpointNext is this session's path) —
  BNCharacter.{h,cpp}, DefaultEngine.ini CrowdManager section; (C) watchdog — AIBStateTreeTasks.cpp
  :377-380 only.
- 2026-09-03 W-BUILD (aib-builder, worktree 19aa5be1, merge pending behind the current build):
  `UAIBPathFollowingComponent` rebased on `UCrowdFollowingComponent` (Phase 11's
  `StartUsingCustomLink` hook survives — CrowdManager.cpp:862 calls it virtually); OnPossess sets
  separation on/weight (row `CrowdSeparationWeight` 1.5), avoidance quality Medium, path offset;
  no RVO groups. Wedge watchdog: with an ally inside `TeammateYieldRadiusUU` (80) it opens ONE
  `TeammateYieldSeconds` (1.0) window as still tactic `Yield` (sprint released, stall clock
  paused, no re-issue). Mode on-objective = strafe rhythm on a `HillStrafeRadiusFraction` (0.6)
  ring (closes Phase 11 LOW-7: planted legs `StrafeHold`, `Hold` kept for SweepLook's guard scan).
  `FAIBMovementPolicy::ArcStep` lifted out of the task (worldless). Instruments at 0.25 s/1 Hz:
  `teammate overlap over — <s>s, <n> inside <r>uu`, `position (x,y,z) allies within <r>uu: <n>`,
  `yields to teammate — …`, `hill strafe-hold — ring <r>uu of reach <R>uu at (…)`; parser regexes
  + per-bot overlap_seconds / yield_count landed (lead). Spec `AIBot.Sim.Separation` 9 cases.
  Tree unchanged. DEFERRED per audit: strafe re-issue only on >100 uu moves (measure
  `mean_pairwise_teammate_distance` first). CONTRACT GAPS (lead): `[/Script/AIModule.CrowdManager]`
  ini + `bResolveCollisions=True`; players as crowd obstacles (`ICrowdAgentInterface` on the
  player pawn, HasAuthority && APlayerController gate) in Source/BreachpointNext.
- 2026-09-03 merged into main (lead): six conflicts vs AIB22 fix #4 resolved — the locomotion
  state stays on the controller (fix #4 R3) and gains `YieldUntilSeconds`; the yield branch
  returns the abandon verdict (false); fix #4's retired `bTriedWedgeJump` flag dropped from the
  yield block; `AIBSeparationSpec` TestEqual calls made unambiguous (double literals).
  `[/Script/AIModule.CrowdManager] bResolveCollisions=True SeparationDirClamp=0.2` added to
  DefaultEngine.ini (contract gap closed). Players-as-crowd-obstacles + BNGameMode match-seed
  hook dispatched to bn-builder (worktree). Build running.
- 2026-09-03 rung 1 (Editor + Game PASS; server unbuildable here) and rung 2 on main with AIB22
  fix #4 + Phase 14 + Phase 13 + the BN retirement: AIBot 206 / 0 (incl. `AIBot.Sim.Separation`
  9, `AIBot.Sim.RouteBias` 8; Tools/Logs/specs-20260903-063537.log), BreachpointNext 36 / 0.
  Game-side hooks merged (9c74bb0d): `ABNCharacter` is an `ICrowdAgentInterface` registered with
  `UCrowdManager` only when HasAuthority && APlayerController (bots never double-register),
  unregistered on UnPossessed/EndPlay; verified against 5.8 source that a non-follower agent is
  a moving obstacle the bots see and is never steered. Building.
