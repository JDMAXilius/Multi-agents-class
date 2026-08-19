# TICKET — ST_BNBot: one StateTree, two states, and the two delays that stop the spins

**Cut:** 18 August 2026 by the cloud lead · **For:** `bn-editor` / the terminal (Unreal MCP)
**Prerequisite:** a build containing R5 Wave 2 (`1b01f70`) **and** the `BotStateTree` ini-resolve
commit (the one adding `[/Script/BreachpointNext.BNBotController]` to `DefaultGame.ini`) — the BN
task classes must appear in the StateTree editor's node picker. If `FBNHasTargetCondition` is not
there, the build is stale: **stop and report.**
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 first.**

## THE SCOPE

Create **one** StateTree asset at **exactly** `/Game/BN/AI/ST_BNBot`, place **four**
point-of-interest actors in the test arena. Nothing else — **no class default to set, no BP
child to create**: the controller already loads the tree from `DefaultGame.ini` (Step 2 is now
a verification, not an edit). The read-back is the deliverable.

## Step 1 — the asset

`/Game/BN/AI/ST_BNBot` — StateTree, **StateTreeAIComponentSchema** (context actor:
`ABNBotController`; context AIController is what every task's `Controller` instance-data binds to).

```
Root (selector)
├── Engage                    enter condition: [BN Has Target]
│     tasks, in order: [BN Face Target] · [BN Move To Target] · [BN Fire Burst]
│     on task Failed  → re-enter Engage WITH A 1.0s TRANSITION DELAY   ← deliberate, see below
│     on task Succeeded → re-enter Engage (loop the burst cycle, no delay)
└── Roam                      (no condition — the else branch)
      task: [BN Move To Point Of Interest]
      on Failed  → re-enter Roam WITH A 2.0s TRANSITION DELAY          ← deliberate, see below
      on Succeeded → re-enter Roam (next point, no delay)
```

**The two delays are the ticket's whole reason and are NOT optional.** The critic measured both
spins: a frozen bot that still sees a target has FireBurst fail instantly — without the 1.0s
delay, Engage re-selects every frame and issues a MoveToActor per frame per bot for the entire
warmup. A level with no points has Roam fail instantly — without the 2.0s delay it re-enters
per frame. The C++ warns once; the DELAY is what makes the failure cheap. Use the transition's
built-in delay property; do not invent a Wait state.

The five nodes appear in the picker under Category **BN**, with these exact display names —
if any is missing, the build is stale, stop and report:

| Node | Kind | Params (leave at C++ defaults) |
|---|---|---|
| `BN Has Target` | condition | — |
| `BN Face Target` | task | AimErrorDegrees 2.5 · ReaimSeconds 0.5 |
| `BN Move To Target` | task | AcceptanceRadius 800 |
| `BN Fire Burst` | task | BurstSeconds 0.6 |
| `BN Move To Point Of Interest` | task | DwellSeconds 2.0 |

Bind every task's `Controller` to the schema's AIController context. Leave every parameter at
its C++ default — tuning is the founder's, later, in this asset.

## Step 2 — verify the wiring (an ini line, NOT a class default)

There is **nothing to set on the controller** and no BP child to make. `ABNBotController` has a
`UPROPERTY(Config) TSoftObjectPtr<UStateTree> BotStateTree` resolved in `OnPossess` before
`StartLogic()`, and `DefaultGame.ini` already carries:

```ini
[/Script/BreachpointNext.BNBotController]
BotStateTree=/Game/BN/AI/ST_BNBot.ST_BNBot
```

Your job here is only to verify the asset you created in Step 1 lives at **exactly** that path
— object name `ST_BNBot` inside package `/Game/BN/AI/ST_BNBot`. A tree saved anywhere else, or
under any other name, resolves null and every bot logs
`BNBotController: BotStateTree '…' failed to load` and stands still. Do not "fix" a mismatch by
editing the ini — move/rename the asset to match the path; the ini is the contract.

## Step 3 — four points of interest

In the test arena (`BR_Arena01` or the current PIE map — name which in the Log), place **4** ×
`ABNPointOfInterest`, spread across the playable space (corners of the arena floor is fine),
`PointName` = North/South/East/West. Without them every bot stands still and logs the no-POI
warning — placing them is part of "bots roam".

## Step 4 — read back

1. The tree: both states, their conditions, the task lists in order, and BOTH transition delays,
   from a fresh load.
2. The asset's exact object path (`/Game/BN/AI/ST_BNBot.ST_BNBot`), confirmed against the ini
   line — that pair IS the wiring; there is no component reference to read back.
3. The four placed actors' names and locations.
4. If PIE is available: start it and paste the first 20 `LogBN` lines — expect `BNBots: filled 3
   bots to reach 4`, three bot names in `BNGameState`/kill lines as the match runs, and NO
   repeated warning spam. **The proof the wiring landed is a warning's ABSENCE**: before this
   ticket every possessed bot printed `BNBotController: BotStateTree is unset — bots will stand
   still…`; after it, that line (and its `failed to load` sibling) must not appear at all. If
   either still prints, Step 2's path check failed — fix the asset location, not the code.

## Log

_(terminal: the read-backs, and anything handed back)_
