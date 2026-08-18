# RESEARCH — the bots: references read, the minimal slice designed

**Cut:** 18 August 2026 by the cloud lead · founder's brief: *"the simple setup — the AI just be
there, set up properly, move, identify players (even if it is AI), kill each other, identify the
interest points — and fill the match to the target player count. GAS, StateTree, maybe BT, GOAP
later with weighted objectives. Less is more."*

Sources read directly: Epic's **Variant_Shooter** AI (recovered from git history — the BP90
demolition deleted it; 12 files extracted from `ade430a~1`), **Lyra**'s bot creation + bot
controller, **Zorans**' AI controller, the old module's `Source/Breachpoint/AI/` (2,826 lines),
and the standing doctrine `BREACHPOINT-AI-BOTS.md` (rulings R8–R12).

## 1. What each reference actually does

| | Controller | Brain | Perception / teams | Bot ↔ GAS | Fill |
|---|---|---|---|---|---|
| **Variant_Shooter** | `AAIController` + `UStateTreeAIComponent` (`StartLogic` on possess) + `UAIPerceptionComponent`, perception delegates forwarded into StateTree tasks | one StateTree; 6-piece C++ vocabulary: `SenseEnemies` task, `LineOfSightToTarget` condition, `FaceActor`, `FaceLocation`, `ShootAtTarget`, `SetRandomFloat` | actor **Tags** (`FName("Enemy")`) — primitive | none — a separate `ShooterNPC` class duplicating the player's weapon plumbing, engine `TakeDamage` | standalone spawner actor, count + respawn delay |
| **Lyra** | `ALyraPlayerBotController` with a **real PlayerState** | (BT assets, not in source) | `IGenericTeamAgentInterface` on the CONTROLLER — `GetTeamAttitudeTowards` compares team IDs | same pawn data as humans; `OnUnPossess` clears the ASC avatar | **the pattern**: spawn AIController → name its PlayerState → `GenericPlayerInitialization` → `RestartPlayer`; count from config + `?NumBots=` URL option |
| **Zorans** | BehaviorTree + Blackboard, half commented out | BT | — | parallel path | — |
| **Old module** | `bWantsPlayerState = true` + StateTreeAI + sight perception configured in C++ | GOAP ambitions (`BRBotBrain`, headless) — the full doctrine | perception delegate → typed facts | **`PressInputTag` → `ASC->AbilityInputTagPressed`** — the bot presses the same buttons a player does | `BRBotManagerComponent` slot-fill |

**Verdicts.** Variant_Shooter contributes the modern controller shape and the StateTree task
vocabulary; its separate-NPC-class is the anti-pattern the founder already named ("don't mix — but
don't duplicate either"). Lyra contributes the fill pattern and the team interface. Zorans
contributes nothing BN needs. The old module contributes the ONE piece none of the others have:
the GAS hand (`PressInputTag`), which is identical to what `ABNPlayerController`'s handlers do.

## 2. The reuse audit — why BN bots are nearly free

`ABNCharacter::PossessedBy` runs `InitializeAbilitySystem` + `GrantDefaults` +
`InitializeCarriedWeapons` for **any** controller. So a bot AIController with
`bWantsPlayerState = true` possessing the SAME pawn class the humans use gets, with zero new code:
a real `ABNPlayerState`, the ASC and attributes, every granted ability, all four weapons, health,
death, ragdoll, hit reactions, kill credit (BNDamage already resolves credit through the
instigator's PlayerState), scoring, respawn (`HandlePlayerDeath`/`RespawnPlayer` take
`AController`), and the match freeze. **The bot is a player whose controller is code.** That is
the founder's modularity requirement satisfied by architecture, not by copying.

## 3. The four integration gaps the audit found (all small, all named)

1. **`OnPostLogin` never fires for AIControllers** — today a bot would get no death subscription,
   no freeze, no score. Fix: move that block into an override of **`GenericPlayerInitialization`**
   (the engine calls it for humans in the login flow; our fill calls it for bots — Lyra's exact
   seam). One function, both kinds of player.
2. **`BNGA_Fire` and `BNGA_Melee` read the view through `ActorInfo->PlayerController`** — null on
   a bot, so bots cannot shoot or swing today. Fix: read through `AController::GetPlayerViewPoint`
   (works for both; AI view = pawn eyes). Two one-line-shaped edits.
3. **`GetNumPlayers()` counts only humans** — correct, and exactly what fill math wants:
   `BotsNeeded = TargetPlayers − Humans`.
4. **Perception affiliation**: FFA has no teams (17 Aug ruling), so the sight sense detects
   EVERYTHING (`DetectEnemies/Neutrals/Friendlies` all true) and the controller's rule is one
   line: any live `ABNCharacter` that is not my pawn is a target. `IGenericTeamAgentInterface`
   lands on the controller now (Lyra's shape) with everyone on team 255 — the slot teams fill
   later without touching perception again.

## 4. What is deliberately DEFERRED, with its slot named

Per the founder's "simple setup first", the full doctrine (BREACHPOINT-AI-BOTS.md) stays the
destination; its own file map says the goal layer is *"an addition of two testable files, not a
rebuild"* — deferring it costs nothing structural:

- **GOAP ambitions + weights** (`BNBotBrain`, `DT_BotAmbitions`) — the brain slots ABOVE the
  StateTree by setting its parameters; nothing in the simple slice has to move.
- **EQS** (cover, firing points, perches) — wave-1 POIs are picked by distance, not scored.
- **Behavior Tree assets** — ruling R9 stands: BT *patterns* live inside StateTree states; no
  second brain. Revisiting BT is a founder decision, not a drift.
- **Aim humanization beyond one error cone, reaction-time tiers, personalities, backfill mid-match,
  bot removal when a human joins mid-match** — the remove seam exists (Lyra's `RemoveOneBot`),
  wave 1 fills at match start only.

## 5. The wave-1 behavior, stated as what you will see

Eight (or four) names in the match. Bots roam between points of interest; when one sees a player
— human or bot — it turns, closes to weapon range, and fires **through the same fire ability,
paying the same ammo, the same cooldowns, the same freeze**; the victim's death prints the same
kill line and scores the same score; the dead bot respawns on the same timer. `BNInput:` lines in
the log show the bot's presses exactly like a human's, and the match ends and restarts with bots
counted like anyone else.
