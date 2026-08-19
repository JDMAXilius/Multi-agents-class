# TICKET — DT_BNBotAmbitions: one table, three rows, the founder's tuning surface

**Cut:** 19 August 2026 by the cloud lead · **For:** `bn-editor` / the terminal (Unreal MCP)
**Prerequisite:** a build containing R6 (`5f0b360`+) — `FBNBotAmbitionRow` must exist.
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 first.**

## THE SCOPE

Create **one** DataTable and fill **three** rows that MIRROR the C++ defaults exactly. No other
asset, no tuning of your own. The read-back is the deliverable.

## Step 1 — the asset

`/Game/BN/Data/DT_BNBotAmbitions` — DataTable, row struct **`FBNBotAmbitionRow`**
(`/Script/BreachpointNext.BNBotAmbitionRow`). `DefaultGame.ini` already points at this exact path
under `[/Script/BreachpointNext.BNGameData]`; a different name or folder lands nowhere, silently
(the C++ fallback keeps driving and warns once).

## Step 2 — three rows, mirroring `UBNBotBrain::DefaultRow` exactly

| Row name | BaseUtility | HealthWeight | TargetWeight | DistanceWeight | CommitSeconds | InterruptBelowHealthNorm |
|---|---|---|---|---|---|---|
| `Fight` | 1.0 | 0.0 | 1.0 | 0.0 | 3.0 | 0.0 |
| `Survive` | 1.2 | 1.0 | 0.0 | 0.0 | 5.0 | **0.35** |
| `Roam` | 0.2 | 0.0 | 0.0 | 0.0 | 2.0 | 0.0 |

Row names are case-exact literals — the brain looks them up as `Fight`/`Survive`/`Roam`.
`InterruptBelowHealthNorm` matters on the Survive row ONLY: it is the "flee below this health"
threshold, and the mirror value 0.35 is the shipped behavior. The founder tunes THIS TABLE from
now on — that is its whole purpose.

## Step 3 — read back

Reload fresh; print all three rows, every column, intent vs actual.

## Log

_(terminal: the read-back table, and anything handed back)_
