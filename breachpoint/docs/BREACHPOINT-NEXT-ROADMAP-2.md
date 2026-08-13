# BREACHPOINT NEXT — Roadmap 2: The Weapon

**Cut:** 13 August 2026 · **Revised:** 13 August 2026 (founder re-scoped — weapon-first, damage
deliberately shallow) · **Binds to:** the NEXT doc family only
([STRUCTURE](BREACHPOINT-NEXT-STRUCTURE.md) · [DOMAINS](BREACHPOINT-NEXT-DOMAINS.md) ·
[ROADMAP-1](BREACHPOINT-NEXT-ROADMAP-1.md), whose **Operating rules** govern this roadmap
unchanged: C++ only, no new scripts, the editor's job is narrow, the time test decides who acts).

## The one-line goal

**A character who carries weapons, switches between them, sprints, leans, reloads and shoots** —
every verb a GAS ability, every state a gameplay tag, correct in client+server from the first
build.

## What this roadmap is NOT

**The damage pipeline is not built here.** It is a big framework — health component, damage
pipeline proper, death GA, respawn GA — and it gets covered extensively in a later roadmap. R2
builds only the shallow version needed to *test* that a character can take damage, die, and
respawn: a console command and a debug key, numbers in the log, nothing more. Anything deeper is
scope creep and gets refused.

Also not here: ADS, the procedural animation layer (recoil, sway, pose offsets), melee, grenades,
projectiles, pickups, UI. Roadmap 3+.

---

## `MyCharacter` is the reference — and here is exactly how far that goes

**Yes, we use it.** `Source/Breachpoint/Character/MyCharacter.{h,cpp}` (2,113 lines) is the
founder's working 1:1 C++ port of the FPS template character, and it is the best available record
of *how the template's pieces actually connect*. Read it for: weapon spawn and socket attachment,
the swap chain, montage play and notify binding, fire cadence and burst timing, the trace and
spread shape, muzzle transform resolution, and which weapon-side values exist at all.

**But it is a map, not a source.** It is explicitly not multiplayer-correct, and it solves three
things in ways BN forbids:

| `MyCharacter` does | BN does instead | Why |
|---|---|---|
| Sends state via `BPI_FPST_AnimInterface` bools (`SetSprinting`, `SetADS`, `SetUnarmed`) | **Gameplay tags.** The ASC holds the state; `UBNAnimInstance` reads tags and publishes the graph-facing property | We are on GAS. A parallel bool channel is a second source of truth |
| Reads weapon values by **reflection** off a Blueprint (`GetWeaponClassVar`, `FBPCall`) | **Typed C++ + data rows.** The weapon is a C++ class; its numbers come from `FBNWeaponRow` | Stringly-typed lookups fail silently; a renamed pin is a runtime shrug |
| Nine `BPC_*` Blueprint components carrying logic | **C++.** Where those hooks plug in is the useful part; the Blueprint implementation is not | Operating rule 2 — everything is C++ |
| Single-player throughout (local input assumptions, no authority split) | Server-authoritative from line one | Operating rule: multiplayer from the first line |

**The standing instruction:** when a packet needs to know *how the template does something*, mine
`MyCharacter` and cite the line. Never copy its mechanism.

---

## The five goals

Testing protocol is R1's, unchanged: **Stage A standalone, then Stage B listen-server + client**,
and a goal is DONE only at Stage B.

### G1 — The data layer
*Numbers leave the code before the first weapon exists, not after.*

| # | Task |
|---|---|
| 1.1 | `Data/BNDataRows.h` — header-only. `FBNWeaponRow`: damage, fire rate, spread, range, magazine size, reload time, fire mode; **soft** refs for the weapon mesh, the anim layer class, and the fire/reload montages |
| 1.2 | `Data/BNGameData.h/.cpp` — GameInstance subsystem: owns the tables, resolves soft refs, the one read path |
| 1.3 | `Data/BNAssetSettings.h/.cpp` — `UDeveloperSettings`, soft paths to the tables |
| 1.4 | **Editor handoff (announced, not assumed):** `DT_BNWeapons` built from `FBNWeaponRow` |

**Objective:** a weapon row reads in C++ from a table, zero hard asset refs anywhere.

### G2 — The weapon, in the hands, with its layer *(the headline)*

| # | Task |
|---|---|
| 2.1 | `Weapons/BNWeapon.h/.cpp` — the weapon base: replicated state, its row handle, its mesh. **No firing logic, no damage** |
| 2.2 | `Weapons/BNEquipmentComponent.h/.cpp` — the inventory of carried weapons, the current index, and grant/revoke of the weapon's `BNAbilitySet` **on authority only** |
| 2.3 | **Attach to the hand socket** — spawned server-side, replicated, attached on every machine. `MyCharacter.cpp:~2020` (`LinkWeaponAnimLayers` / `CreateWeapon`) is the reference for socket resolution |
| 2.4 | **The anim layer follows the current weapon** — `GetCurrentWeaponAnimLayer()` finally returns the row's soft layer class, and R1's seam links it. Weapon layers parent to `ABP_ItemAnimLayersBase` (per the inverted note in the ABP task doc — there is no BN base) |
| 2.5 | **Switching** — next/previous through every carried weapon; `BNGA_Equip` is the one verb family (equip · swap · drop). `bUnarmed` flips through R1's existing path with **no new anim code** |
| 2.6 | **Editor handoff:** `BP_BNWeapon` defaults-only children + the per-weapon anim layer ABPs |

**Objective (Checkpoint I):** spawn holding a weapon, see it in the hands, cycle through weapons — and the **other window** sees the right weapon and the right pose set on your character.

### G3 — Sprint and lean, tag-driven and attribute-driven

| # | Task |
|---|---|
| 3.1 | `BNGA_Sprint` — the third UCLASS `BNMovementAbilities` was always designed to hold |
| 3.2 | **Sprint speed comes from the attribute set**, not a literal: the ability applies a GE that modifies `MoveSpeed`, and R1's existing attribute→`MaxWalkSpeed` delegate does the rest. No `MaxWalkSpeed` is ever written directly |
| 3.3 | `State.Movement.Sprinting` applied as a GE-carried tag (the crouch pattern), so it replicates to sim proxies; `UBNAnimInstance` publishes `bSprinting` from it — **this is what replaces `SetSprinting`** |
| 3.4 | **Lean** — left/right input, kept simple: a lean value on the character, published through the snapshot→publish path, plus a lean state tag. `MyCharacter`'s `OnLeanLeftPressed`/`SetLeaning` is the reference for *where it plugs in*; the procedural `AimAndLean` component is NOT built here |
| 3.5 | New input actions + tags for sprint and lean, through R1's `BNInputConfig`/ini path |

**Objective (Checkpoint J):** sprint changes speed and upper-body pose; lean tilts left and right; both correct on the other window.

### G4 — Fire and reload, as abilities

| # | Task |
|---|---|
| 4.1 | `AbilitySystem/Abilities/BNGA_Fire.h/.cpp` — predicted, `LocalPredicted`; fire rate, spread, range and fire mode from the weapon row. `MyCharacter`'s `FireEvent`/`WeaponTrace`/`BurstFireTick` is the reference for cadence and spread shape |
| 4.2 | Client traces and sends **TargetData**; the **server validates and is the only authority on the hit**. No engine damage API, ever |
| 4.3 | `AbilitySystem/Abilities/BNGA_Reload.h/.cpp` — plays the reload montage from the row, commits ammo on the notify, cancels cleanly |
| 4.4 | Ammo as the named GAS-purity exception — **confirm attribute vs weapon state before building** (open decision 5) |
| 4.5 | `AbilitySystem/BNGameplayCues.h/.cpp` — muzzle, impact, tracer as **C++ cue classes**. All FX through cues |

**Objective (Checkpoint K):** shooting plays its animation and hits where you aim; reload plays and refills; the observer sees both.

### G5 — Death and respawn, the shallow testing version *(deliberately minimal)*

> **Scope fence.** Simple enough to prove the loop, no more. The real health/damage framework is a
> later roadmap and this code is expected to be replaced by it. If a packet here starts designing
> mitigation, damage types, or a full pipeline, it has gone out of scope.

| # | Task |
|---|---|
| 5.1 | `Characters/BNHealthComponent.h/.cpp` — thin: watches the Health attribute, fires a death delegate. Nothing else |
| 5.2 | One minimal damage entry — server-authoritative, applied as a GE against the existing Health/Shield attributes. **Shallow on purpose**, and the one door the real pipeline will replace |
| 5.3 | `BNGA_Death` and respawn — end other abilities, apply the dead tag, respawn at a `BNPlayerStart` with attributes reset **through the init GE**, never hand-set |
| 5.4 | **Testing levers:** `Utilities/BNCheatManager` console commands *and* a debug key bound to "damage me" / "kill me". Numbers logged on every application — that log line is the test |
| 5.5 | **Debt A1** — `BNGA_Jump` must survive avatar destruction: no stuck spec, no orphaned Jumping/InAir GE on the persistent ASC |
| 5.6 | **Debt A2** — the crouch GE cleaned in `EndPlay` (mirroring the `MoveSpeed` delegate pattern) or swept by tag on spawn |

**Objective (Checkpoint L):** press the debug key → health drops with the number in the log → die → respawn with full attributes → **jump, crouch, sprint and weapons all still work** (that clause is what tests the two debts). Both windows.

---

## Waves

| Wave | Goals | Ends at |
|---|---|---|
| 1 | G1 data | compiles; a row reads |
| 2 | G2 weapon, hands, layer, switching | **Checkpoint I** |
| 3 | G3 sprint + lean | **Checkpoint J** |
| 4 | G4 fire + reload | **Checkpoint K** |
| 5 | G5 death/respawn (shallow) + debts A1/A2 | **Checkpoint L** |

---

## Debts and open items carried from R1

### Recorded critic debts — paid in G5

| # | Debt | Recorded in |
|---|---|---|
| A1 | **Avatar destroyed while airborne leaves the jump spec stuck active** — `LandedDelegate` never fires, `EndAbility` never runs, the Jumping/InAir GE sticks on the *persistent* PlayerState ASC and Space becomes a dead key | Wave 3 critic, `27302a7` |
| A2 | **The crouch GE outlives a destroyed pawn** — `OnEndCrouch` never fires on destruction; the next pawn spawns permanently tagged Crouching | Crouch critic, `9b59d79` |

### Animation states still blocked

`isADS_Upper` · `gameplayTag_IsADS` · `aDSStateChanged` · `wasADSLastUpdate` — need ADS (R3).
`bFPSMode` — needs a **founder ruling**; `MyCharacter` only ever reads it (`:1548`), never sends
it, so the record does not prove what fills it. Not guessed.

### Housekeeping R1 left open

| # | Item |
|---|---|
| E1 | **`FPSTemplate/` is no longer read-only in practice.** The ruling existed to protect `BP_FPSCharacter`, which now inherits `UBNAnimInstance` through the shared main ABP and **was never separately exercised** |
| E2 | **`Content/BN/Animation/ABP_BNMannequin.uasset` is unused** — keep or delete before it rots into a second source of truth |
| E3 | **Graph-clear day** — clear `ABP_Mannequin_Base`'s event graph **and** flip `bNativeOwnsTurnState` to `true`, in one atomic change |
| E4 | **The four asset-enum properties** become C++ `UENUM`s with the graph retyped — same change as E3 |
| E5 | **The owed three-way verification** — every R1 claim is founder-reported *standalone*; listen+client is unproven for the whole spine, and R2 doubles the replicated surface on top of it |

## The crew for R2

**No new agents** — `bn-builder` and `bn-critic` cover it. `bn-editor`'s role is now driving the
**Unreal MCP** from the terminal for asset creation and setup, never writing scripts. Crew growth
is expected at the UI roadmap.

## Open decisions

1. **`bFPSMode`** — what state fills it? (blocks the last of the pose selectors)
2. **E2** — `ABP_BNMannequin.uasset`: keep or delete?
3. **E1** — does `BP_FPSCharacter` still work now that it inherits `UBNAnimInstance`? One PIE run decides whether the shared-main-ABP route holds
4. **E3 + E4 — graph-clear day.** A founder call on timing; the largest cleanup outstanding
5. **Ammo** — attribute (the GAS-purity exception ledger names it) or weapon state? **Gates G4.4**
6. **Weapon count for the slice** — how many weapons must the switch cycle prove? Two is enough to test switching; the row-driven design costs nothing extra per weapon after that
