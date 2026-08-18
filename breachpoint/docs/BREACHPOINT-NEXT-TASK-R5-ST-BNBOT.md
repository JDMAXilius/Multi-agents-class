# TICKET — ST_BNBot: one StateTree, two states, and the two delays that stop the spins

**Cut:** 18 August 2026 by the cloud lead · **For:** `bn-editor` / the terminal (Unreal MCP)
**Prerequisite:** a build containing R5 Wave 2 (`1b01f70`) — the BN task classes must appear in
the StateTree editor's node picker. If `FBNHasTargetCondition` is not there, the build is stale:
**stop and report.**
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 first.**

## THE SCOPE

Create **one** StateTree asset, set **one** class default, place **four** point-of-interest
actors in the test arena. Nothing else. The read-back is the deliverable.

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

Bind every task's `Controller` to the schema's AIController context. Leave every parameter at
its C++ default (AimErrorDegrees 2.5, ReaimSeconds 0.5, AcceptanceRadius 800, BurstSeconds 0.6,
DwellSeconds 2.0) — tuning is the founder's, later, in this asset.

## Step 2 — the class default

`ABNBotController` (or its BP child if one exists — check first, the pawn precedent says the BP
may out-serialise): the StateTreeAI component's **StateTree** asset reference → `ST_BNBot`.
This is the third out-serialise-trap check in a row; read it back from a fresh load.

## Step 3 — four points of interest

In the test arena (`BR_Arena01` or the current PIE map — name which in the Log), place **4** ×
`ABNPointOfInterest`, spread across the playable space (corners of the arena floor is fine),
`PointName` = North/South/East/West. Without them every bot stands still and logs the no-POI
warning — placing them is part of "bots roam".

## Step 4 — read back

1. The tree: both states, their conditions, the task lists in order, and BOTH transition delays,
   from a fresh load.
2. The controller default's StateTree reference.
3. The four placed actors' names and locations.
4. If PIE is available: start it and paste the first 20 `LogBN` lines — expect `BNBots: filled 3
   bots to reach 4`, three bot names in `BNGameState`/kill lines as the match runs, and NO
   repeated warning spam.

## Log

_(terminal: the read-backs, and anything handed back)_
