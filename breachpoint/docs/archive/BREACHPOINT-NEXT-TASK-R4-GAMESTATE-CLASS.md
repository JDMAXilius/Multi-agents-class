# TICKET — the GameState class on BP_BNGameMode — **CLOSED 19 Aug 2026, superseded**

**Cut:** 17 August 2026 · **Closed:** 19 August 2026 by the cloud lead, without editor work.

## Why it closed

The ticket existed because a Blueprint child can out-serialise a C++ constructor default, so
`BP_BNGameMode`'s **GameState Class** dropdown had to be inspected by hand. That race no longer
exists: `ABNGameMode::InitGame` now FORCES `GameStateClass = ABNGameState::StaticClass()` at
runtime — after the Blueprint's own serialisation, before `PreInitializeComponents` spawns the
GameState — the exact precedent `DefaultPawnClassPath` already documents in `DefaultGame.ini`
("the details panel is only what you see, not what spawns").

Landed with the native match-state migration (`ABNGameMode : AGameMode`,
`ABNGameState : AGameState`), same commit.

## What remains true

- The dropdown on `BP_BNGameMode` is now cosmetic for this property. Setting it to `BNGameState`
  anyway is harmless and reads nicer; nothing depends on it.
- The failure this ticket guarded against — scores and clock landing on a GameState nothing
  reads — now presents differently: **no `BNGameState:` lines at all** means the map is not
  running `ABNGameMode` in the first place (World Settings GameMode override), per
  DIAGNOSTICS §3b.1.

## Log

Closed without editor work — the read-back this ticket asked for has nothing left to prove.
