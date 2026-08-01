# BREACHPOINT — BUILD STATE

> **Generated — do not edit; edit the architecture or the scanner.**
> Written by `Tools/architect/build_state.py` from `Tools/architect/state/perception.json`,
> `Tools/architect/state/ranking.json` and `git log`. Edits made here are erased by the next
> run and are invisible until then, which is worse — a hand-edited state file is a state
> file that lies. To change a **state**, change the code the scanner reads; to change a
> **rank**, change `architect.py`'s terms; to change the **manifest**, change
> `BREACHPOINT-ARCHITECTURE.md` §3.

Deterministic by construction: no clock, no host, no wall-clock date. The date below comes
from the blackboard filename, SHAs from `git log`, states and scores from the two JSON
inputs. Two runs from a clean checkout are byte-identical.

| | |
|---|---|
| state of record | **2026-08-01** (from `Tools/architect/blackboard/`) |
| manifest | `BREACHPOINT-ARCHITECTURE.md §3` |
| budget | 43 in-slice + 1 reserved = **44** units |
| tally | **BUILT 31** · **STUB 3** · **MISSING 10** |
| API calls that produced this | **0** (perception) · **0** (ranking) |

## BUILT

31 units the scanner classified BUILT (a header plus a `.cpp` with a real body).
`commit` is the commit that **added** the unit's files (`git log --diff-filter=A`, oldest
entry). A unit whose landing commit cannot be determined shows `-` — never a guessed SHA.

| unit | folder | ticket | commit | landed | what landed it |
|---|---|---|---|---|---|
| `BRBotBrain` | AI/ | BP08 | `abfef2f` | 2026-08-01 | BP08: the three-layer bot brain -- one function is the whole AI->world surface |
| `BRBotController` | AI/ | BP08 | `abfef2f` | 2026-08-01 | BP08: the three-layer bot brain -- one function is the whole AI->world surface |
| `BRBotManagerComponent` | AI/ | BP08 | `abfef2f` | 2026-08-01 | BP08: the three-layer bot brain -- one function is the whole AI->world surface |
| `BREnvQueryContexts` | AI/ | BP08 | `abfef2f` | 2026-08-01 | BP08: the three-layer bot brain -- one function is the whole AI->world surface |
| `BRStateTreeTasks` | AI/ | BP08 | `abfef2f` | 2026-08-01 | BP08: the three-layer bot brain -- one function is the whole AI->world surface |
| `BRAbilitySet` | AbilitySystem/ | BP02 | `3514a11` | 2026-08-01 | BP02 steps 3-4: ability base, ExecCalc, BRGA_Sprint, and the six GE classes |
| `BRAbilitySystemComponent` | AbilitySystem/ | BP02 | `3bbc530` | 2026-08-01 | BP02 steps 1-2: the ASC, BRPlayerState, and the attribute set |
| `BRAttributeSet` | AbilitySystem/ | BP02 | `3bbc530` | 2026-08-01 | BP02 steps 1-2: the ASC, BRPlayerState, and the attribute set |
| `BRDamageExecCalc` | AbilitySystem/ | BP02 | `3514a11` | 2026-08-01 | BP02 steps 3-4: ability base, ExecCalc, BRGA_Sprint, and the six GE classes |
| `BRGA_Sprint` | AbilitySystem/ | BP02 | `3514a11` | 2026-08-01 | BP02 steps 3-4: ability base, ExecCalc, BRGA_Sprint, and the six GE classes |
| `BRGameplayAbility` | AbilitySystem/ | BP02 | `3514a11` | 2026-08-01 | BP02 steps 3-4: ability base, ExecCalc, BRGA_Sprint, and the six GE classes |
| `BRCharacter` | Character/ | BP01 | `103e6a4` | 2026-07-31 | BP01 step 4: BRCharacter, CMC, BRPlayerController + LogBRInput (R24) |
| `BRCharacterMovementComponent` | Character/ | BP01 | `103e6a4` | 2026-07-31 | BP01 step 4: BRCharacter, CMC, BRPlayerController + LogBRInput (R24) |
| `BRCore` | Core/ | BP01 | `cf3cae3` | 2026-07-31 | BP01 step 2: Core/ -- 29 native gameplay tags + log channels + collision aliases |
| `BRGameplayTags` | Core/ | BP01 | `cf3cae3` | 2026-07-31 | BP01 step 2: Core/ -- 29 native gameplay tags + log channels + collision aliases |
| `BRInputComponent` | Input/ | BP01 | `ce48871` | 2026-07-31 | BP01 step 3a: the owned input layer, C++ half (assets deferred) |
| `BRInputConfig` | Input/ | BP01 | `ce48871` | 2026-07-31 | BP01 step 3a: the owned input layer, C++ half (assets deferred) |
| `BRGameMode` | Match/ | BP04 | `d60fa6d` | 2026-08-01 | BP04: match frame -- phases, kill attribution, one replicated end-time |
| `BRGameState` | Match/ | BP04 | `d60fa6d` | 2026-08-01 | BP04: match frame -- phases, kill attribution, one replicated end-time |
| `BRPlayerController` | Match/ | BP04 | `103e6a4` | 2026-07-31 | BP01 step 4: BRCharacter, CMC, BRPlayerController + LogBRInput (R24) |
| `BRPlayerState` | Match/ | BP04 | `3bbc530` | 2026-08-01 | BP02 steps 1-2: the ASC, BRPlayerState, and the attribute set |
| `BRListenServerLifecycle` | Online/ | BP11 | `c83d077` | 2026-08-01 | BP11 (partial): IBRServerLifecycle seam, sessions, telemetry, host-quit DEFINED |
| `BRSessionsSubsystem` | Online/ | BP11 | `c83d077` | 2026-08-01 | BP11 (partial): IBRServerLifecycle seam, sessions, telemetry, host-quit DEFINED |
| `BRTelemetrySubsystem` | Telemetry/ | BP11 | `c83d077` | 2026-08-01 | BP11 (partial): IBRServerLifecycle seam, sessions, telemetry, host-quit DEFINED |
| `BRActivatableWidget` | UI/ | BP10 | `2075f8b` | 2026-08-01 | BP10: UI layer + MVVM ViewModels, C++ only, zero polling |
| `BRHUDLayout` | UI/ | BP10 | `2075f8b` | 2026-08-01 | BP10: UI layer + MVVM ViewModels, C++ only, zero polling |
| `BRUIManagerSubsystem` | UI/ | BP10 | `2075f8b` | 2026-08-01 | BP10: UI layer + MVVM ViewModels, C++ only, zero polling |
| `BRViewModels` | UI/ | BP10 | `2075f8b` | 2026-08-01 | BP10: UI layer + MVVM ViewModels, C++ only, zero polling |
| `BREquipmentComponent` | Weapons/ | BP03 | `fb9ddb2` | 2026-08-01 | BP03 step 1: equipment, weapon instance, pickups -- ammo is owner-only |
| `BRWeaponInstance` | Weapons/ | BP03 | `fb9ddb2` | 2026-08-01 | BP03 step 1: equipment, weapon instance, pickups -- ammo is owner-only |
| `BRWeaponPickup` | Weapons/ | BP03 | `fb9ddb2` | 2026-08-01 | BP03 step 1: equipment, weapon instance, pickups -- ammo is owner-only |

**Every BUILT unit is attributed to a commit.** No `-` rows, and no SHA in this table
was written by hand.

**UNDECLARED — real `BR*` source on disk that §3 does not declare.** Reported, never
adopted as units: adopting them would let the scanner rewrite the architecture it is
checked against.

- `AbilitySystem/`: `BRCombatCurves`
- `UI/`: `BRRootLayout`, `BRUISettings`, `BRUITypes`

## DECISIONS

**Selected: `BRGA_WeaponFire`** — BP03, MISSING, total **106**.

The terms, not the reasoning. `score = depth + blockers + tier + state`; ties break on
lowest ticket number, then unit name — never on a model's preference. An LLM did not
choose this unit and cannot: the numbers below came out of deterministic Python.

| term | value | what it measures |
|---|---|---|
| depth | **2** | ticket-DAG depth ∪ `#include` depth |
| blockers | **4** | units that transitively wait on this one |
| tier | **0** | GDD tier — slice `0`, Phase-2 `-100` |
| state | **100** | MISSING `100` · STUB `50` · BUILT `-1000` |
| **TOTAL** | **106** | |

State is a **gate**, not a nudge, and tier is the same mechanism. The blocker term reaches
37, so a state term of 2/1/0 was swamped by it and the first draft ranked a BUILT unit
first — a *what to build next* scorer selecting something already built. The magnitudes
above make that arithmetically impossible.

**Tied at the top, broken by the rule and not by a preference:**

| unit | ticket | total | broken by |
|---|---|---|---|
| `BRGA_WeaponFire` | BP03 | 106 | selected |
| `BRGA_WeaponUtility` | BP03 | 106 | same ticket number, later unit name |

**What the score does not measure:** whether the unit's *inputs* exist. Depth, blockers,
tier and state are the four terms this ticket specifies; readiness is not among them. See
NEXT — the selected unit is the most valuable next unit **and** is not startable today.
Those are different questions and only the first is scored.

## NEXT

The ranked remainder — 12 selectable units below the selection, in score
order. BUILT units are omitted: they score `-1000` on state and can never be selected.

| # | unit | ticket | state | depth | blockers | tier | state | TOTAL |
|---|---|---|---|---|---|---|---|---|
| 2 | `BRGA_WeaponUtility` | BP03 | MISSING | 2 | 4 | 0 | 100 | **106** |
| 3 | `BRSpotterSubsystem` | BP11 | MISSING | 4 | 0 | 0 | 100 | **104** |
| 4 | `BRGA_Grenade` | BP05 | MISSING | 2 | 1 | 0 | 100 | **103** |
| 5 | `BRGA_Melee` | BP05 | MISSING | 2 | 1 | 0 | 100 | **103** |
| 6 | `BRGA_Grapple` | BP06 | MISSING | 3 | 0 | 0 | 100 | **103** |
| 7 | `BRBotDeterminismSpec` | BP00 | MISSING | 0 | 0 | 0 | 100 | **100** |
| 8 | `BRCombatSpec` | BP00 | MISSING | 0 | 0 | 0 | 100 | **100** |
| 9 | `BRShieldSpec` | BP00 | MISSING | 0 | 0 | 0 | 100 | **100** |
| 10 | `BRDataRows` | BP02 | STUB | 1 | 28 | 0 | 50 | **79** |
| 11 | `BRBotFacts` | BP08 | STUB | 3 | 11 | 0 | 50 | **64** |
| 12 | `BRServerLifecycle` | BP11 | STUB | 4 | 3 | 0 | 50 | **57** |
| 13 | `BRGameLiftLifecycle` | BP11 | MISSING | 4 | 0 | -100 | 100 | **4** |

### Known blockers

Facts the scorer cannot see. Each row cites its source; none is inferred by the generator.

| unit | blocker | source |
|---|---|---|
| `BRGA_WeaponFire` | `FBRWeaponRow` / `CT_Combat` carry no trace range and no spread -- a hitscan ability would have to invent both, which is a law-3 violation by construction | BP15 Log, 1 Aug 2026 (step 4, gap 2) -- OPEN |
| `BRGA_WeaponFire` | `DT_Weapons.csv` has no `AbilitySet` column, so equip has nothing to grant and the unit could never be granted to anyone | BP15 Log, 1 Aug 2026 (step 4, gap 3) -- OPEN |
| `BRGA_WeaponFire` | `Ability.Weapon.Fire` + the three `GameplayCue.Weapon.*.Fire` tags were undeclared | BP15 Log, 1 Aug 2026 -- RESOLVED under BP03 (R23 open families); 34 EXTERN/34 DEFINE |
| `BRBotDeterminismSpec` | `Source/Breachpoint/Tests/` holds only `.gitkeep`; rung 2 has nothing to run | BP15 Log, 1 Aug 2026 (step 4, rungs) |
| `BRCombatSpec` | `Source/Breachpoint/Tests/` holds only `.gitkeep`; rung 2 has nothing to run | BP15 Log, 1 Aug 2026 (step 4, rungs) |
| `BRShieldSpec` | `Source/Breachpoint/Tests/` holds only `.gitkeep`; rung 2 has nothing to run | BP15 Log, 1 Aug 2026 (step 4, rungs) |
| `BRGameLiftLifecycle` | Phase-2 reserved -- expected MISSING for the entire slice and ranked last by the tier term, so it is never selectable | ARCHITECTURE §3.8 + BP15 step 1 |

### Ladder blockers

| rung | status | source |
|---|---|---|
| rung 1 (UBT, three targets) | BLOCKED -- a UE editor session and a build must not overlap (R29.3) | BP15 Log, 1 Aug 2026 |
| rung 2 (specs) | BLOCKED -- same editor lock, and `Source/Breachpoint/Tests/` holds only `.gitkeep` | BP15 Log, 1 Aug 2026 |
| rung 4b (listen + 1 remote) | BLOCKED upstream by BP00's Gauntlet/NuGet failure | BP15 Log, 1 Aug 2026 |

A rung is never silently skipped — it is green or it is BLOCKED with a reason.

