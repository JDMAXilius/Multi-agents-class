# TICKET — AIB9: Phase 6 proof — the bot plays the objective (Hill)

> STATUS: open — cut 26 Aug 2026 by the cloud lead with the Phase-6 build
> ("WRITTEN, NOT COMPILED"). TERMINAL WORK: compile, specs, tree rebuild, PIE with the
> hill on. Both founder rulings this phase waited on are CLOSED and recorded
> (ARCHITECTURE law 3 amendment; the Hill + third score int — see
> docs/AIBOT-PHASE6-PACKET.md).

## What landed (cloud, this ticket's subject)

**Module (Source/AIBot/):**
- `Core/AIBBotManager.h/.cpp` — NEW WorldSubsystem: host pushes providers once
  (`RegisterProviders`, refuses client worlds loudly), controllers pull at possession.
- Engine: `HasAmbition`, `BuildModeAmbitionSpec` (mode → spec with an ObjectiveUrgency
  consideration, VWU=0, commit 3s — never a raw registration).
- Controller: provider doors (weak, IsValid-guarded), `CachedModeAmbitions`,
  `GetObjectiveKindForCurrentAmbition()`, `RefreshAmbitions()` (clear + core +
  translated mode + Think) — OnPossess pulls from the manager; OnUnPossess nulls.
- Facts builder: objectives block (provider urgency through `ClampUrgency` — the ONE
  door, NaN scrubs to 0), nearest matching-Kind POI distance, `NearbyAllies` (with
  `bCrowdKnown` still honestly false).
- Executor: gate base grew `virtual Matches()`; `FAIBGateModeCondition`
  (hierarchy-match on `AIBot.Ambition.Mode`); `FAIBMoveToObjectiveTask` (station-keep
  on the POI, F7-loud no-goal/no-progress failures).
- Authoring: Mode branch between Roam and Fallback (GateMode + Sentinel +
  MoveToObjective + SweepLook).
- Specs: +4 in AIBAmbitionEngineSpec (host-shaped child tag through translation, gate
  hierarchy match, clear/no-leftovers, ClampUrgency incl. NaN). **Module spec total is
  now 95** (was 91).

**Game (Source/BreachpointNext/):**
- `Match/BNPlayerState` — third int `ObjectivePoints` (replicated, own OnRep),
  `GetScore() = Kills + ObjectivePoints`; leaders + kill-limit + hill-limit all read
  `GetScore()`.
- `Match/BNHillPoint` — NEW dumb actor (place + radius, real scene root, no tick/rep).
- `AIBotAdapter/BNAIBModeTags` — NEW: `AIBot.Ambition.Mode.Hold`, `AIBot.POI.Hill`.
- `AIBotAdapter/BNAIBWorldQuery` — NEW WorldSubsystem implementing IAIBWorldQuery:
  hills as POIs (HUD-grade), FFA `AreEnemies`; `QueryVisibleEnemies` returns EMPTY on
  purpose (no matured feed yet — the honest refusal), `CountNearbyAllies` 0.
- `Match/BNGameMode` — implements IAIBAmbitionProvider (one Hold ambition when
  `bHillEnabled`, urgency from the hill cache: other-holds 0.9 / contested 0.75 /
  empty 0.6 / mine 0.35); providers registered in HandleMatchIsWaitingToStart (BEFORE
  the bot fill — gen-1 bots pull at their only possession); `StartHill()` spawn-or-adopt
  + 1s `HillTick` (overlap sphere, distinct living PlayerStates, sole occupant scores,
  contested = nobody, crossing `GetScore() >= ScoreLimit` ends via FinishMatch).

## Steps (terminal)

1. Rung 1: all targets compile. Everything above is WRITTEN, NOT COMPILED.
2. Specs: `AIBot.Sim.*` — expect **95/95** (new total; AmbitionEngine went 18 → 22).
3. Probe: `Tools/aib/70_aib_assets.py` — the list is **20** node structs now
   (7 conditions + 13 tasks). NOTE: AIB7's step said "18/18" — this ticket SUPERSEDES
   that number; run AIB7's remaining steps against 20.
4. Tree rebuild: report string must read
   `[Engage, Retreat, Search, Seek, Roam, Mode, Fallback]`.
5. PIE with the hill ON — `Config/DefaultGame.ini`:
   ```ini
   [/Script/BreachpointNext.BNGameMode]
   bHillEnabled=True
   HillLocation=(X=?,Y=?,Z=?)   ; pick an open, navmesh-covered spot on the arena
   HillRadius=600.0
   HillPointsPerSecond=1
   ScoreLimit=25                 ; raise past the 3-kill test default or the hill never matters
   ```
   Watch for, in order:
   - log `hill live at ...` at match start, and `RefreshAmbitions: N wants` per bot
     possession (expect 6 = 5 core + Hold);
   - bots WALKING TO the hill with no enemy in sight (Mode branch active — the
     tree-shape observable);
   - a sole bot on the hill banking points (scoreboard moves without kills) and
     STANDING ITS GROUND there (MoveToObjective station-keeps — being there is the verb);
   - two bots on it = nobody scores (contested), and fights breaking out ON the hill;
   - a bot at full health abandoning a distant fightless roam for the hill, but NOT
     abandoning a live firefight (Engage with visible target + nerve outranks 1.2 × 0.9);
   - match ends on objective points crossing ScoreLimit with the holder announced.
6. Slayer regression: one match with `bHillEnabled=False` — zero mode ambitions
   (RefreshAmbitions logs 5), no hill actor, kill-limit path unchanged.
7. Watch-list (assumed APIs to confirm at compile): `UE_DEFINE_GAMEPLAY_TAG_STATIC` in
   a spec cpp; `GetSubsystem<UBNAIBWorldQuery>()` cross-module from the game mode;
   `TActorIterator<ABNHillPoint>` adopt path.

## Done when

- [ ] Rung 1 + 95/95 + probe 20/20 + tree shows the Mode branch
- [ ] The five hill observables above seen in one PIE match (say which rung: PIE ≠ MP)
- [ ] Slayer regression clean
- [ ] Numbers and log lines pasted in the Log

## Log

_(terminal: outputs verbatim)_
