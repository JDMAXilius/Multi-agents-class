# BREACHPOINT NEXT — Roadmap 5: The Bots (fill, see, kill)

**Cut:** 18 August 2026 by the cloud lead · **Crew:** `bn-builder`, `bn-critic`, `bn-editor`
**Design:** [`RESEARCH-AI-BOTS`](BREACHPOINT-NEXT-RESEARCH-AI-BOTS.md) — read it first; its §2
reuse audit and §3 gap list are the load-bearing facts. Doctrine: `BREACHPOINT-AI-BOTS.md`
(rulings R8–R12) is the destination; this roadmap is its SIMPLE SLICE by founder order.

## The one-line goal

**A match fills to TargetPlayers with bots that are players in every system's eyes — they roam
between points of interest, see anyone (human or bot), and kill through the same GAS abilities,
scoring the same score.**

## Founder rulings that bound this roadmap

- **Simple first.** Exist · move · see · kill · POIs · fill. GOAP/EQS/tiers are named slots, not
  this roadmap's work.
- **The bot reuses the PLAYER's everything** — same pawn class, same ASC path, same abilities.
  Nothing is duplicated for the bot, and nothing player-only is bent to fit it: the AI adds only
  what a player does not have (eyes and a will).
- **Less is more:** 3 new classes total. A fourth is a finding.
- **FFA:** no teams. `IGenericTeamAgentInterface` lands with everyone on one team id so teams
  slot in later without touching perception again.

## What R5 is NOT

No GOAP brain, no EQS assets, no BehaviorTree asset (R9: BT patterns live inside StateTree
states), no difficulty tiers, no aim-humanization beyond one error cone, no mid-match
backfill/removal, no bot HUD/nameplates. Named here so they stay out.

---

## G1 — The head and the hand: `BNBotController`

| # | Task |
|---|---|
| 1.1 | `AI/BNBotController.{h,cpp}` — `ABNBotController : AAIController`. Ctor: `bWantsPlayerState = true` (the WHOLE reuse chain hangs on this — a real `ABNPlayerState` is the ASC); `UStateTreeAIComponent` with `SetStartLogicAutomatically(false)`; `UAIPerceptionComponent` + `UAISenseConfig_Sight` configured **in C++** (radius/lose-radius/FOV as Config UPROPERTYs; `DetectionByAffiliation` all three true — FFA). |
| 1.2 | `OnPossess`: `Super`, then `StateTreeAI->StartLogic()`. `OnUnPossess`: if the dying pawn is still the ASC's avatar, clear it (Lyra's `OnUnPossess` — a respawning bot must not leave a stale avatar on the persistent ASC). |
| 1.3 | Perception → target: `OnTargetPerceptionUpdated` handler keeps `TargetEnemy` (TWeakObjectPtr). The FFA target rule, one function: a live `ABNCharacter`, not my pawn, sensed or currently sensed. Expose `GetCurrentTarget/SetCurrentTarget/ClearCurrentTarget`. |
| 1.4 | The HAND: `PressInputTag(FGameplayTag)` / `ReleaseInputTag(FGameplayTag)` → the PlayerState ASC's `AbilityInputTagPressed/Released` — the exact API `ABNPlayerController`'s handlers use. No `TryActivateAbility` anywhere in bot code: bots press buttons. |
| 1.5 | `IGenericTeamAgentInterface` on the controller: everyone returns team 255 and `GetTeamAttitudeTowards` answers Hostile for any pawn that is not self (FFA). One comment marks it as the teams-later seam. |
| 1.6 | Dead-pawn handling: subscribe nothing new — the MODE already respawns on death via `HandlePlayerDeath` (G2 makes that true for bots). The controller only needs `StopLogic`/`StartLogic` around possession changes; verify StateTree restarts cleanly on the new pawn. |

**Done when:** a hand-placed `ABNBotController` possessing a spawned BN pawn stands in the level
with a named PlayerState, full weapons, and `BNInput:` lines appearing when its hand is pressed
from a test task.

## G2 — The two seams that make bots real players (EDITS, no new files)

| # | Task |
|---|---|
| 2.1 | `BNGA_Fire.cpp` + `BNGA_Melee.cpp`: replace every view-point read of `ActorInfo->PlayerController` with the pawn's `AController` (`GetInstigatorController()` / `ActorInfo->AvatarActor`'s controller) and `AController::GetPlayerViewPoint` — for an AIController that is the pawn's eye view. **Bots cannot shoot or swing until this lands.** Behavior for humans must be bit-identical (a PlayerController's GetPlayerViewPoint is unchanged). |
| 2.2 | `BNGameMode`: move the death-subscription + mid-freeze-join block from `OnPostLogin` into an override of **`GenericPlayerInitialization(AController*)`** — the engine calls it for humans in the login flow, and G4's fill calls it for bots. `OnPostLogin` keeps only `TryStartMatch()`. Guard for re-entry (the engine may call it more than once per controller — the subscription must not double). |

**Done when:** a bot dies to a human and the kill line prints with credit and score; the bot
respawns on the same timer; a bot joining during warmup is frozen.

## G3 — The tree's vocabulary and the interest points

| # | Task |
|---|---|
| 3.1 | `AI/BNBotStateTreeTasks.{h,cpp}` — the C++ vocabulary, Variant_Shooter's shape (instance-data structs + `FStateTreeTaskCommonBase`/`FStateTreeConditionCommonBase`): **`FBNHasTargetCondition`** · **`FBNFaceTargetTask`** (controller focus) · **`FBNMoveToTargetTask`** (AIController MoveTo with an AcceptanceRadius bound to weapon range) · **`FBNFireBurstTask`** (press Fire tag, hold BurstSeconds, release; refuses itself while `State.Match.Frozen` — the ASC refuses anyway, this just stops futile presses) · **`FBNMoveToPointOfInterestTask`** (pick BY DISTANCE + not-the-last-one, move, dwell DwellSeconds). |
| 3.2 | Aim error, ONE knob: `FBNFireBurstTask.AimErrorDegrees` (Config-shaped instance default) applied as a random cone on the controller's focal point per burst — bots must not be hitscan-perfect. Seeded from the world's random stream, no wall clock (R8's spirit; full determinism harness is deferred with the brain). |
| 3.3 | `AI/BNPointOfInterest.{h,cpp}` — `ABNPointOfInterest : AActor`: a name (`FName PointName`) and a radius. No tick, no collision, one billboard-free root. EQS later replaces the PICK in 3.1, never this actor. |

**Done when:** the vocabulary compiles and each task's EnterState/Tick/ExitState is exercised by
the ST asset (G-editor ticket) in PIE.

## G4 — The fill: a match reaches TargetPlayers

| # | Task |
|---|---|
| 4.1 | `BNGameMode`: `Config int32 TargetPlayers = 4` · `Config TArray<FString> BotNames` · `Config TSoftClassPtr<ABNBotController>` (soft — law 3; C++ default class path in ini). |
| 4.2 | `EnsureBotFill()` — authority: `BotsNeeded = TargetPlayers − GetNumPlayers()` (humans only, which is exactly right); spawn each bot Lyra's way: spawn the controller, name its PlayerState from BotNames (fallback `Bot <n>`), `GenericPlayerInitialization(Bot)`, `RestartPlayer(Bot)`. Track in `SpawnedBots`. Called from `TryStartMatch` (before the MinPlayers check counts nothing — fill happens when the match START decision is made) and once per human `OnPostLogin` while `WaitingToStart`. |
| 4.3 | Bots and the match: verify (and fix where one line suffices) that `RestartMatch`'s respawn loop, `SetAllPlayersFrozen`, and `GetLeaders` all iterate `PlayerArray` — which includes bot PlayerStates — so bots score, freeze, restart and can WIN. A bot winner's name in `match over. Winner:` is a pass, not a bug. |
| 4.4 | `BNLoadout`-style announce: one `BNBots:` line per fill (`filled 3 bots to reach 4`), and one per spawn failure, loud. |

**Done when:** a solo PIE on `TargetPlayers=4` starts with 3 named bots, the log shows their
kills/deaths scoring, and the match can be won by a bot.

---

## Waves

| Wave | Goals | Agent | Then |
|---|---|---|---|
| 1 | G1 + G2 | `bn-builder` | `bn-critic` on the diff |
| 2 | G3 + G4 | `bn-builder` | `bn-critic` on the diff |
| — | ST asset ticket | `bn-editor` | after Wave 2 compiles (the tasks must exist in the class picker) |

## The editor ticket (bn-editor, after Wave 2)

`Content/BN/AI/ST_BNBot.uasset` — ONE StateTree (StateTreeAIComponent schema):
Root → selector: **Engage** (enter: `FBNHasTargetCondition`; tasks: FaceTarget → MoveToTarget →
FireBurst, loop) · **Roam** (else; task: MoveToPointOfInterest, loop). Set the controller class
default `StateTreeRef` to it (or the BP child's default). Read back: the tree's two states, the
bound task list, and the controller default. Nothing else.

## Config (lands with Wave 2)

```ini
[/Script/BreachpointNext.BNGameMode]
TargetPlayers=4
+BotNames=Marcus
+BotNames=Vale
+BotNames=Ossian
+BotNames=Rook
+BotNames=Halcyon
+BotNames=Juno
+BotNames=Piper
BotControllerClass=/Script/BreachpointNext.BNBotController
```
(sight radius / aim error / burst+dwell seconds ride the controller's and tasks' own Config
defaults; numbers land in ini only when the founder tunes them.)

## Known limitation, accepted with its trigger written down

**The fill can overshoot when `MinPlayers > 1`.** Human 1's arrival fills to TargetPlayers during
warmup; human 2's arrival then starts the match one wide (bot removal is deferred). Unreachable on
the shipped config (`MinPlayers=1` — the first human both fills and starts), same acceptance shape
as R4's MinPlayers limitation. **Reopen the moment `MinPlayers` is raised above 1** — the fix is
the deferred remove-a-bot seam (Lyra's `RemoveOneBot`), not a rework.

## Status — 18 Aug 2026

| Wave | Goals | State |
|---|---|---|
| 1 | G1 controller · G2 seams | **LANDED** `29ad9b2`, critic **PASS** (all five windows walked clean) |
| 2 | G3 vocabulary+POI · G4 fill | **LANDED** `1b01f70`, critic **PASS on the net dimension**, 4 notes: 2 fixed (monotonic bot names; one-time no-POI warning), 1 accepted above, 1 answered in the ST ticket's transition design |
| — | ST asset ticket | `TASK-R5-ST-BNBOT` — OPEN, terminal, after the founder's build |

**Not compiled.** The founder's build is the first real test.

## Deferred beyond R5, deliberately — each with its slot

GOAP brain + `DT_BotAmbitions` (sits above the tree, sets its parameters) · EQS (replaces the POI
pick) · difficulty tiers/personalities (`DT_BotTuning`) · reaction-time model · mid-match
backfill and bot removal when a human joins (the remove seam is Lyra's `RemoveOneBot`) ·
bot voice/callouts · nameplates/HUD affordances.
