# TICKET — the GameState class on BP_BNGameMode: one check, one read-back

**Cut:** 17 August 2026 by the cloud lead · **For:** `bn-editor` / the terminal session (Unreal MCP)
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 first.** Serves R4-G1 task 1.4.

## THE SCOPE

This ticket **inspects one property on one asset** and changes it only if it is wrong. It creates
nothing, touches no other asset, and does not enter `Source/`. Extra work found is a Log entry,
not a licence. The read-back IS the deliverable.

## Why it exists

R4 adds `ABNGameState` and the game mode's C++ constructor sets
`GameStateClass = ABNGameState::StaticClass()`. **A constructor value is only a default.** This
project has already been bitten twice by a Blueprint out-serialising one:

- `BP_BNGameMode`'s *Default Pawn Class* dropdown loses to `BNGameMode::InitGame`'s ini path —
  the details panel shows one thing and another spawns (documented in `DefaultGame.ini`).
- `BP_FPSCharacter`'s serialised mesh collision can silently override the C++ trace-channel
  responses, which is why `BNHit:` now announces hittability at BeginPlay.

If `BP_BNGameMode` carries its own `GameStateClass`, the match state, the clock and every score
land on a GameState the game never spawns — and nothing errors. One look now costs nothing.

## Step 1 — inspect

On **`/Game/BN/Core/BP_BNGameMode`**, read the **GameState Class** property (Class Defaults →
Classes category).

| Reads as | Do |
|---|---|
| `BNGameState` (the C++ class) | nothing — record it and stop |
| **empty / None** | nothing — the C++ default stands, which is the intended state. Record and stop |
| any other class (e.g. `GameStateBase`, a BP GameState) | **set it to `BNGameState`**, compile, save |

If `BNGameState` is not in the class picker at all, the module is stale — **stop and report**. Do
not substitute a similar-looking class; that is how a wrong parent lands.

## Step 2 — read back

Reload the asset fresh and print `GameStateClass` again, plus the resolved class path. Paste it
into the Log below, intent vs actual.

## Done means

The property is `BNGameState` or empty, read back from a fresh load, pasted below. That is the
whole ticket.

## Log

_(terminal: the read-back, and anything handed back)_
