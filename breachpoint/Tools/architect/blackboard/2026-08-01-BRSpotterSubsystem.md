# Blackboard — BRSpotterSubsystem — 2026-08-01

Written by `Tools/architect/architect.py --blackboard` BEFORE any generation. If this file is
absent, nothing was authorised. Nothing reaches the codebase unlogged.

## What it scored

Top 12 of 44. Ties break on lowest ticket number. **Zero API calls** produced this order.

| # | unit | ticket | state | depth | blockers | tier | state | TOTAL |
|---|---|---|---|---|---|---|---|---|
| 1 | BRSpotterSubsystem | BP11 | MISSING | 4 | 0 | 0 | 100 | **104** |
| 2 | BRGA_Grenade | BP05 | MISSING | 2 | 1 | 0 | 100 | **103** |
| 3 | BRGA_Melee | BP05 | MISSING | 2 | 1 | 0 | 100 | **103** |
| 4 | BRGA_Grapple | BP06 | MISSING | 3 | 0 | 0 | 100 | **103** |
| 5 | BRGA_WeaponFire | BP03 | MISSING | 2 | 0 | 0 | 100 | **102** |
| 6 | BRGA_WeaponUtility | BP03 | MISSING | 2 | 0 | 0 | 100 | **102** |
| 7 | BRBotDeterminismSpec | BP00 | MISSING | 0 | 0 | 0 | 100 | **100** |
| 8 | BRCombatSpec | BP00 | MISSING | 0 | 0 | 0 | 100 | **100** |
| 9 | BRShieldSpec | BP00 | MISSING | 0 | 0 | 0 | 100 | **100** |
| 10 | BRGameLiftLifecycle | BP11 | MISSING | 4 | 0 | -100 | 100 | **4** |
| 11 | BRCharacter | BP01 | BUILT | 6 | 7 | 0 | -1000 | **-987** |
| 12 | BRDamageExecCalc | BP02 | BUILT | 4 | 7 | 0 | -1000 | **-989** |

**Selected: `BRSpotterSubsystem`** — BP11, MISSING, total 104.

## What it will issue

Prompt handed to **builder** (verbatim):

> Implement `BRSpotterSubsystem` in `Source/Breachpoint/Telemetry/` per `BREACHPOINT-ARCHITECTURE.md` §3's entry for it.
> Server-authoritative; clients send intent. Attributes mutate only via GameplayEffects; the
> engine damage API is banned. Tuning numbers live in `Content/Data/*.csv` and reach C++ through
> `Source/Breachpoint/Data/BRDataRows.h` as SOFT refs. No gameplay Tick. Write ONLY inside
> `Source/Breachpoint/Telemetry/`. If you are blocked, file a `contract_gap` in the ticket and STOP.

Contracts attached: `gas-purity.md`, `data-and-assets.md`, `netcode.md` (if the unit adds a
replicated surface).

## What it will generate

| | |
|---|---|
| target | `Source/Breachpoint/Telemetry/BRSpotterSubsystem.h` / `.cpp` |
| owner_path | `Source/Breachpoint/Telemetry/` |
| ticket | BP11 |
| gate A | diff confined to owner_path — a diff outside is auto-rejected |

## Rungs owed

Rung 1 all three targets · rung 2 its spec · rung 4 green or **BLOCKED with a reason** — never
silently skipped. Per R30, a networked surface owes 4a (dedicated) and, if the path differs
host-vs-remote, 4b (listen + 1 remote); 4a green is not 4b evidence.
