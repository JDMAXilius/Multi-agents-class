# TICKET — AIB11: Phase 6 proof — the bot plays the objective (Hill)

> STATUS: blocked — mac terminal 26 Aug 2026. Steps 1-4 and 6 PASS. Step 5 BLOCKED by a
> HIGH module defect: a mode ambition that fails its branch never yields, starving every
> other behaviour (76 kills with the hill off vs 0 with it on). bHillEnabled restored False.
>
> THE HIGH IS FIXED — by the TERMINAL (AIB16, archived; compiled, spec'd 117/117, and
> live-proven: switches 7 → 589, kills 0 → 32, suppressions 1:1 with failures). The
> cloud wrote the same fix concurrently and DROPPED it at the merge — the terminal's
> escalating-strikes version (3s → 6s → … 20s cap, forget-after-clean-spell) is
> strictly better and proven. Remaining for this ticket: the hill REACHABILITY (bots
> now cycle instead of freezing but still bank 0 points — 42 cannot-reach per match),
> which AIB16 hands to AIB9's traversal work (DescribeMoveFailure into the objective
> task). bHillEnabled stays False until then.
> Original: open — cut 26 Aug 2026 by the cloud lead with the Phase-6 build
> ("WRITTEN, NOT COMPILED"). TERMINAL WORK: compile, specs, tree rebuild, PIE with the
> hill on. Both founder rulings this phase waited on are CLOSED and recorded
> (ARCHITECTURE law 3 amendment; the Hill + third score int — see
> docs/AIBOT-PHASE6-PACKET.md).
>
> NUMBERING: cut as AIB9 (the Phase-6 commit message says so); renumbered to AIB11 at
> the merge — the terminal's off-mesh (AIB9) and strafe-arc (AIB10) tickets took the
> numbers in flight. Open ticket yields, closed commits keep their citations — the
> AIB5/AIB6/AIB7 rule, third application. My NAVLINK_GAP ticket (briefly AIB8) is
> RETIRED unnumbered: the terminal's archived AIB8 ran that exact experiment
> (JumpLength 600 REFUTED, the baseline a mirage) and their AIB9 carries the real
> defect (bots leave the mesh) — nothing of mine remained to do.

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
2. Specs: `AIBot.Sim.*` — expect **97/97** (AmbitionEngine 18 → 22 with Phase 6;
   AimPolicy 7 → 9 with the P4+5 review barrier — see docs/AIBOT-P45-REVIEW.md,
   which also landed module fixes riding this same proof run).
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

- [x] Rung 1 + 114/114 (supersedes 95) + probe 20/20 + tree shows the Mode branch
- [ ] BLOCKED — observables 1-2 seen, 3-5 unreachable behind a HIGH defect (see Log)
- [x] Slayer regression clean — and it refuted my first diagnosis
- [x] Numbers and log lines pasted in the Log

## Log

_(terminal: outputs verbatim)_

### 2026-08-26 — steps 1-4 PASS; the hill loop is BLOCKED by traversal, not by Phase 6

**Step 1 — Rung 1 PASS** (Editor + Game; Server environmental). Three compile breaks had
to be fixed first, all in BN game code from this phase, none in the module — see commit
`969b165`: `FOverlapResult` missing include; `AAIController` unknown in `BNGameMode.h`
(a latent transitive-include bug that only surfaced once the first fix pushed the .cpp
out of the unity blob); and a `for` loop whose unconditional `break` made `++It`
unreachable under `-Werror,-Wunreachable-code-loop-increment`.

**Step 2 — Specs 114/114/0** reconciled. This SUPERSEDES the ticket's 95/97: Phases 7-10
landed on top before this ran, and 114 is the count for the merged tree.

**Step 3 — Probe PASS, 20/20** (7 conditions + 13 tasks), checked both directions. The
owed SeekWeapon→Seek rename is visible in the vocabulary (`FAIBGateSeekCondition`,
`FAIBSeekDestinationTask`, `FAIBMoveToPOITask`).

**Step 4 — Tree rebuilt.** The report reads EXACTLY the required string:

```
states : Root > [Engage, Retreat, Search, Seek, Roam, Mode, Fallback]
compile: OK          save: OK
```

The rebuild was genuinely mandatory and the log proves it: immediately before it the
editor threw `LogCore: Warning: Unable to find serialized UScriptStruct -> ... reset to
empty FInstancedStruct. LinkerRoot:/Game/AIBot/AI/ST_AIBBot` — stale node references the
rename had orphaned. The asset was silently losing nodes.

**Tooling defect found:** `Tools/aib/70_aib_assets.py` reported BOTH assets `MISSING` in
its MCP read-back while the editor's own C++ read-back, written the same second, reported
`FOUND ... compiled: YES (ready to run)` with 5 tier rows. The C++ path is the one of
record (the script's own header says so), so nothing is wrong with the assets — but the
script's MCP lookup misreports and would mislead anyone reading only its output.

#### Step 5 — three matches, each fixing a real defect and exposing the next

| run | hill vs possession | no-POI fails | cannot-reach | kills |
|---|---|---|---|---|
| 1 — hill at (2000,1600,430) | **2ms LATE** | 7 | — | 0 |
| 2 — after the ordering fix | 644ms early | **0** | 31/bot | 0 |
| 3 — hill moved onto a spawn | early | 0 | **226** | 0 |

**Run 1 exposed a deadlock, and it is the most important finding here.** Chronology:

```
13.46.35:678-691  all 7 bots possess, win Mode.Hold (0.72 over Roam 0.20), and fail:
                  "won a mode want but the world query offered no POI of kind
                   AIBot.POI.Hill — branch fails (F7)"
13.46.35:693      hill registers — two milliseconds too late
```

Then ZERO AIBot decision lines for the remaining four minutes. Not one bot re-evaluated.
`Mode.Hold` kept winning at 0.72, the branch kept failing, and because the WINNER NEVER
CHANGED nothing logged it — the only AIBot output left was `jumped to clear whatever it is
wedged on`, ~30x per bot. Seven bots deadlocked, silently, for the whole match.

Cause: `HandleMatchIsWaitingToStart` registers providers then calls `EnsureBotFill`, and
**a gen-1 bot's possession is the only pull it ever makes**. `StartHill()` ran later, at
match start. The comment at that call site states the assumption that breaks it — "before
the rebody loop so the first post-thaw think already has the POI" — true for later
generations, false for the bots born in warmup.

FIX (one line): `StartHill()` moved ahead of `EnsureBotFill()`. Safe to run early because
`HillTick` already guards on `IsMatchInProgress()`, so the warmup-squatter concern that
motivated the late call is handled elsewhere; the later call stays as the restart path's,
idempotent on `!Hill.IsValid()`. Verified: run 2 has the hill live 644ms BEFORE possession
and **zero** no-POI failures.

**Runs 2 and 3 hit a different wall: the bots cannot REACH the hill.** I first assumed bad
placement — 300uu from a spawn is not proof of navmesh coverage — and moved the hill ONTO
the PlayerStart at (2000,1300,405), where a body stands every respawn. Failures got WORSE
(31/bot → 226 total), which rules placement out.

The task is behaving correctly. `FAIBMoveToObjectiveTask::Tick` stands when inside the
acceptance radius and only fails on NO PROGRESS, and it routes through AIB5's projection
helper (`MoveToNavPoint`, line 1225) so the goal is navmesh-projected. And the bodies are
alive and trying: **200 `Input.Sprint` activations and 232 wedge-clear jumps** in run 3,
with zero forward progress.

I first concluded that this was the AIB9 traversal defect — bots pinned, goals
unreachable across this arena's navmesh — and wrote that into this Log. **Step 6 refuted
it**, which is the entire reason the control run exists.

#### Step 6 — the slayer regression, and what it overturns

Same arena, same bots, same build, `bHillEnabled=False`:

```
mode ambitions registered : 102 x "registered 0 mode ambition"   (correct: no provider want)
hill actor                : 0
eliminations              : 76        <- hill runs: 0
matches completed         : 4
ambition switches         : Roam 932, Engage 847, Search 375, Retreat 123  (2277 total)
```

The regression itself is CLEAN on every count the ticket asks for: zero mode ambitions,
no hill actor, kill-limit path untouched, four matches finished.

But read it against the hill runs and it says something much larger. With the hill OFF
these bots fight, roam, search, retreat and kill **76** times across 2,277 ambition
switches. With the hill ON: **0 kills, 7 switches, ever**. Same navmesh, same traversal,
same everything else.

**So traversal is not the blocker, and my earlier read was wrong.** The blocker is that
`Mode.Hold` wins at 0.72, beats Roam (0.20) and Engage alike, fails its branch, and then
KEEPS WINNING — forever. Every other behaviour starves. The bots sprint and wedge-jump
(200 and 232 times in run 3) because the objective task keeps re-issuing a move it cannot
complete, and nothing ever lets a different ambition have a turn.

#### The real defect, and it IS Phase 6's

A mode ambition that fails its branch does not yield. There is no failure suppression, no
decay on repeated F7, no cooldown — so a single unreachable POI converts a working bot
into a permanently stuck one, and the utility contest can never correct it because the
score does not depend on whether the branch has ever succeeded.

Run 1 is the pure form (deadlock with no POI at all, zero decisions in four minutes) and
runs 2-3 are the same shape with a POI present but unreachable. The ordering fix removed
one trigger; it did not touch the mechanism.

This is a HIGH finding against Phase 6, not a traversal complaint, and it belongs to the
module (ambition scoring / branch-failure feedback), not to the arena. Until it is fixed,
`bHillEnabled=True` should be treated as a bot-disabling setting.

Whether the hill at (2000,1300,405) is ALSO unreachable is now a separate, open question
that this evidence cannot answer — the starvation would look identical either way.

#### Scope note

`Source/BreachpointNext/Match/` was outside this ticket's declared owner_path; I extended
the claim to make the one-line ordering fix, because no observable past #3 is reachable
while every bot is deadlocked. Recording it rather than widening scope quietly.
