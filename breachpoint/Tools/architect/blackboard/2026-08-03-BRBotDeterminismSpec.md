# Blackboard — BRBotDeterminismSpec — 2026-08-03

Written by `Tools/architect/architect.py --blackboard` BEFORE any generation. If this file is
absent, nothing was authorised. Nothing reaches the codebase unlogged.

## What it scored

Top 12 of 46. Ties break on lowest ticket number. **Zero API calls** produced this order.

`score = blockers + tier + state + readiness − depth` (R32: depth is SUBTRACTED — depth away
from a DAG root is distance from being *startable*. R33: readiness is the fifth term, derived
from disk. R34: `state`, `tier` and `readiness` are gates and dominate; `depth` and `blockers`
are preferences.)

| # | unit | ticket | state | −depth | blockers | tier | state | ready | TOTAL |
|---|---|---|---|---|---|---|---|---|---|
| 1 | BRBotDeterminismSpec | BP00 | MISSING | -0 | 0 | 0 | 100 | 0 | **100** |
| 2 | BRSpotterSubsystem | BP11 | MISSING | -4 | 0 | 0 | 100 | 0 | **96** |
| 3 | BRCore | BP01 | STUB | -0 | 2 | 0 | 50 | 0 | **52** |
| 4 | BRGameLiftLifecycle | BP11 | MISSING | -4 | 0 | -100 | 100 | 0 | **-4** |
| 5 | BRGameplayTags | BP01 | BUILT | -0 | 2 | 0 | -1000 | 0 | **-998** |
| 6 | BRInputConfig | BP01 | BUILT | -0 | 2 | 0 | -1000 | 0 | **-998** |
| 7 | BRInputComponent | BP01 | BUILT | -1 | 2 | 0 | -1000 | 0 | **-999** |
| 8 | BRDataRows | BP02 | BUILT | -1 | 2 | 0 | -1000 | 0 | **-999** |
| 9 | BRAbilitySystemComponent | BP02 | BUILT | -2 | 2 | 0 | -1000 | 0 | **-1000** |
| 10 | BRGameState | BP04 | BUILT | -2 | 2 | 0 | -1000 | 0 | **-1000** |
| 11 | BRAttributeSet | BP02 | BUILT | -3 | 2 | 0 | -1000 | 0 | **-1001** |
| 12 | BRGameplayAbility | BP02 | BUILT | -3 | 2 | 0 | -1000 | 0 | **-1001** |

**Selected: `BRBotDeterminismSpec`** — BP00, MISSING, readiness unknown, total 100.

## What it will issue

Prompt handed to **builder** (verbatim):

> Implement `BRBotDeterminismSpec` in `Source/Breachpoint/Tests/` per `BREACHPOINT-ARCHITECTURE.md` §3's entry for it.
> Server-authoritative; clients send intent. Attributes mutate only via GameplayEffects; the
> engine damage API is banned. Tuning numbers live in `Content/Data/*.csv` and reach C++ through
> `Source/Breachpoint/Data/BRDataRows.h` as SOFT refs. No gameplay Tick. Write ONLY inside
> `Source/Breachpoint/Tests/`. If you are blocked, file a `contract_gap` in the ticket and STOP.

Contracts attached: `gas-purity.md`, `data-and-assets.md`, `netcode.md` (if the unit adds a
replicated surface).

## What it will generate

| | |
|---|---|
| target | `Source/Breachpoint/Tests/BRBotDeterminismSpec.h` / `.cpp` |
| owner_path | `Source/Breachpoint/Tests/` |
| ticket | BP00 |
| gate A | diff confined to owner_path — a diff outside is auto-rejected |

## Rungs owed

Rung 1 all three targets · rung 2 its spec · rung 4 green or **BLOCKED with a reason** — never
silently skipped. Per R30, a networked surface owes 4a (dedicated) and, if the path differs
host-vs-remote, 4b (listen + 1 remote); 4a green is not 4b evidence.
