# BREACHPOINT NEXT — Roadmap 2: The Combat Spine

**Cut:** 13 August 2026 · **Domain:** Gameplay (trunk) + the animation states weapons unlock ·
multiplayer-correct throughout · **Binds to:** the NEXT doc family only
([STRUCTURE](BREACHPOINT-NEXT-STRUCTURE.md) · [DOMAINS](BREACHPOINT-NEXT-DOMAINS.md) ·
[ROADMAP-1](BREACHPOINT-NEXT-ROADMAP-1.md), whose **Operating rules** section governs this
roadmap unchanged).

## The one-line goal

**Shoot → damage → die → respawn.** The first moment Breachpoint is a game rather than a
character controller — and the moment the weapon seam built in R1 finally has a weapon in it.

## Where R1 left the tree

**15 units built** of the manifest's 63: `Core/BNGameplayTags` · `Input/BNInputConfig` +
`BNInputComponent` · `AbilitySystem/{BNAbilitySystemComponent, BNAbilitySet, BNGameplayAbility,
Attributes/BNAttributeSet, Effects/BNGameplayEffects, Abilities/BNMovementAbilities}` ·
`Characters/BNCharacter` · `Animation/BNAnimInstance` · `Match/{BNGameMode, BNPlayerController,
BNPlayerState}`.

**Empty folders R2 fills:** `Data/` (nothing at all yet) · `Weapons/` · `Actors/` ·
`Utilities/` · and the damage half of `AbilitySystem/Effects/`.

---

## Everything deferred INTO Roadmap 2 — the full inventory

Nothing here is new. Each line was deferred deliberately during R1 and is quoted from where it
was recorded, so the reasoning survives the gap.

### A. Recorded critic debts — these MUST be paid by the death/respawn work

| # | Debt | Recorded in |
|---|---|---|
| A1 | **Avatar destroyed while airborne leaves the jump spec stuck active** — `LandedDelegate` never fires, `EndAbility` never runs, the Jumping/InAir GE sticks on the *persistent* PlayerState ASC forever and Space becomes a dead key. Unreachable in R1 (no death existed); reachable the instant death lands. | Wave 3 critic, commit `27302a7` |
| A2 | **The crouch GE outlives a destroyed pawn.** `OnEndCrouch` never fires on destruction, the handle dies with the character, and the next pawn spawns permanently tagged Crouching (a fresh crouch then stacks a second GE). Fix is `EndPlay` cleanup mirroring the `MoveSpeed` delegate pattern, or a remove-by-tag on spawn. | Crouch critic, commit `9b59d79` |

### B. Animation states that need R2 gameplay to exist

| Item | Blocked on | Recorded in |
|---|---|---|
| `bSprinting` — selects the `fPS_Sprint` pose slot | the sprint ability (`BNMovementAbilities` shipped Jump + Crouch only; Sprint was always the third UCLASS) | `bUnarmed` packet, `79527b1` |
| `isADS_Upper` · `gameplayTag_IsADS` · `aDSStateChanged` · `wasADSLastUpdate` | ADS, which needs camera FOV + pose offsets | same |
| `bFPSMode` | **a founder ruling** — `MyCharacter` only ever *reads* it (`:1548`), never sends it, so the record does not prove what fills it | same |
| The `S_Procedural_*` struct messages (`SetAimAndLeanInfo`, `SetPoseTransform`, `SetMontagePoseOffset`, `SetFPSPelvisWeight`) | the procedural layer — see §D | `MyCharacter.cpp:1141`, task doc |

### C. Manifest units R2 needs (from the ✅ SPINE tier, unbuilt)

`Data/BNDataRows` · `Data/BNGameData` · `Data/BNAssetSettings` · `Weapons/BNWeapon` ·
`Weapons/BNEquipmentComponent` · `AbilitySystem/Effects/BNDamage` ·
`AbilitySystem/Effects/BNDamageExecution` · `AbilitySystem/Abilities/BNGA_Fire` ·
`AbilitySystem/Abilities/BNGA_Death` · `AbilitySystem/BNGameplayCues` ·
`Characters/BNHealthComponent` · `Utilities/BNCheatManager` · `Actors/BNPlayerStart`.

### D. Deferred to Roadmap 3 — named here so they are not silently pulled forward

- **The procedural animation layer** — recoil, sway-and-lag, aim-and-lean, pose offsets, the
  procedural manager. In the template these are nine `BPC_*` Blueprint components; **BN builds
  them as C++**, using `MyCharacter`'s hooks as the map of *where* they plug in, never as code to
  copy. This is what makes the arms feel alive rather than merely animated — it is not what makes
  them animate.
- **ADS** — needs the camera FOV blend and pose offsets, i.e. it needs §D to exist first.
- **The rest of the component hierarchy** — `CameraBoom` + `FollowCamera` (third-person/debug
  views), `Arrow_MeleeTraceStart` (melee). They arrive with their features, per R1's ruling.
- **Melee, grenade, grapple, reload** as abilities; **projectiles** and **pickups** as actors.
- **UI** — `UI/` is untouched by R2; the HUD reads a ViewModel that does not exist yet.

### E. Housekeeping and reconciliations R1 left open (decisions, not features)

| # | Open item | Why it matters |
|---|---|---|
| E1 | **`FPSTemplate/` is no longer read-only in practice.** The ruling existed to protect `BP_FPSCharacter`, which now inherits `UBNAnimInstance` through the shared main ABP and **was never separately exercised**. | A broken reference costs us the only working comparison we have |
| E2 | **`Content/BN/Animation/ABP_BNMannequin.uasset` is unused** by the shipped route | It rots into a second source of truth if left undecided — keep or delete |
| E3 | **Graph-clear day** — clearing `ABP_Mannequin_Base`'s event graph **and** flipping `bNativeOwnsTurnState` to `true`, in one change | Doing either alone leaves the turn accumulators either double-written or unowned |
| E4 | **The four asset-enum properties** (`LocalVelocityDirection`, `…NoOffset`, `CardinalDirectionFromAcceleration`, `RootYawOffsetMode`) become C++ `UENUM`s with the graph retyped, in the same change as E3 | They are the only graph-facing state C++ does not own |
| E5 | **The owed three-way verification** — every R1 claim is founder-reported *standalone*. Listen-server + client is still unproven for the whole spine. | R2 doubles the replicated surface; unproven ground under it compounds |

---

## The cut — six goals

Each ends in something the founder can play. The **testing protocol is R1's, unchanged**: Stage A
standalone, then Stage B listen-server + client, and a goal is DONE only at Stage B.

### G1 — The data layer
*Numbers leave the code. Nothing after this hardcodes a gameplay value.*

| # | Task |
|---|---|
| 1.1 | `Data/BNDataRows.h` — header-only. `FBNWeaponRow` first: damage, headshot multiplier, fire rate, spread, range, magazine, plus **soft** refs for mesh and anim layer class |
| 1.2 | `Data/BNGameData.h/.cpp` — GameInstance subsystem: owns the tables, resolves soft refs, the one read path |
| 1.3 | `Data/BNAssetSettings.h/.cpp` — `UDeveloperSettings`, soft paths to the tables |
| 1.4 | **Editor handoff:** `DT_BNWeapons` created from `FBNWeaponRow`. Announced, not assumed |

**Objective:** a weapon row reads in C++ from a table, with zero hard asset refs anywhere.

### G2 — The one damage door
*Built before anything can shoot, so there is never a second path.*

| # | Task |
|---|---|
| 2.1 | `AbilitySystem/Effects/BNDamage.h/.cpp` — the **only** spec builder that will ever exist |
| 2.2 | `AbilitySystem/Effects/BNDamageExecution.h/.cpp` — shields absorb first, then health; headshot multiplier; friendly-fire rule |
| 2.3 | `BNAttributeSet` gains `IncomingDamage` as a server-only meta attribute |
| 2.4 | `Characters/BNHealthComponent.h/.cpp` — the attribute→gameplay bridge and death detection, with its delegate |
| 2.5 | `Utilities/BNCheatManager.h/.cpp` — **the testing lever, and it is why this goal is testable before weapons exist**: damage self/target, set health, refill shields |

**Objective (Checkpoint D):** a console command damages a player; shields drop first, then health; identical values on server and both clients.

### G3 — Death and respawn *(pays debts A1 and A2)*

| # | Task |
|---|---|
| 3.1 | `AbilitySystem/Abilities/BNGA_Death.h/.cpp` — triggered by the health component, ends every other ability, applies the dead-state tag |
| 3.2 | `Actors/BNPlayerStart.h/.cpp` + spawn selection in `BNGameMode`; respawn timer from `DT_MatchRules`-style data, not a literal |
| 3.3 | **Debt A1:** `BNGA_Jump` must survive avatar destruction — no stuck spec, no orphaned Jumping/InAir GE on the persistent ASC |
| 3.4 | **Debt A2:** the crouch GE is cleaned in `EndPlay` (mirroring the `MoveSpeed` delegate pattern) and/or swept by tag on spawn |
| 3.5 | Attributes reset on respawn — via the init GE, never by hand-setting |

**Objective (Checkpoint E):** die, respawn at a spawn point with full health/shields, and **jump and crouch still work** — the debts are what that clause is testing. Both windows.

### G4 — Weapons and equipment
*The R1 anim seam finally has something to resolve.*

| # | Task |
|---|---|
| 4.1 | `Weapons/BNWeapon.h/.cpp` — the weapon actor: mesh + replicated state. **No firing logic** |
| 4.2 | `Weapons/BNEquipmentComponent.h/.cpp` — slots, grant/revoke the weapon's `BNAbilitySet` on authority, and **drive `GetCurrentWeaponAnimLayer()`** |
| 4.3 | `AbilitySystem/Abilities/BNGA_Equip.h/.cpp` — equip · swap · drop, one verb family |
| 4.4 | `bUnarmed` flips false through the existing path — **no new anim code**, which is the test that R1's seam was built right |
| 4.5 | **Editor handoff:** `BP_BNWeapon` (defaults-only child) + the per-weapon anim layer ABP. Per the inverted note: weapon layers parent to **`ABP_ItemAnimLayersBase`** — there is no BN base |

**Objective (Checkpoint F):** equip a weapon → its anim layer links → the upper body changes pose set → the *other* window sees it too.

### G5 — The fire path
*The loop closes.*

| # | Task |
|---|---|
| 5.1 | `AbilitySystem/Abilities/BNGA_Fire.h/.cpp` — predicted hitscan; client sends TargetData; **server validates and is the only authority on the hit** |
| 5.2 | Damage applied **only** through `BNDamage` — a second spec builder here is a design failure, not a shortcut |
| 5.3 | Fire rate, spread, range, magazine read from the weapon row; ammo as the named GAS-purity exception |
| 5.4 | `AbilitySystem/BNGameplayCues.h/.cpp` — muzzle, impact, tracer as **C++ cue classes**; all FX through cues |

**Objective (Checkpoint G):** shoot another player, their shields then health drop, they die, they respawn. Server, shooter and observer all agree.

### G6 — Sprint, and the pose states weapons unlocked

| # | Task |
|---|---|
| 6.1 | `BNGA_Sprint` — the third UCLASS `BNMovementAbilities` was always designed to hold |
| 6.2 | `bSprinting` published through the established snapshot→publish path, from the sprint state tag |
| 6.3 | `bFPSMode` — **wire only after the founder's ruling**; otherwise it stays deferred, not guessed |

**Objective (Checkpoint H):** sprinting changes the upper-body pose on both windows.

---

## Waves

| Wave | Goals | Ends at |
|---|---|---|
| 1 | G1 data · G2 damage door + cheat manager | **Checkpoint D** — damage by console command |
| 2 | G3 death + respawn (debts A1, A2) | **Checkpoint E** — die, respawn, movement intact |
| 3 | G4 weapons + equipment | **Checkpoint F** — equip changes the pose set |
| 4 | G5 fire path + cues | **Checkpoint G** — the loop closes |
| 5 | G6 sprint + pose states | **Checkpoint H** |

## The crew for R2

**No new agents.** Per the evolution rule, the crew grows only when a roadmap's work has no
owner — R2 is C++ plus four editor handoffs, which `bn-builder` and `bn-critic` already cover.
**`bn-editor`'s role changes**, though: under the revised operating rules it no longer writes
scripts. Its job is now to drive the **Unreal MCP** from the terminal for asset creation and
setup, and to say plainly when the founder clicking is faster.

Crew growth is expected at the **UI** roadmap, not this one.

## Open decisions to settle before or during R2

1. **`bFPSMode`** — what R1/R2 state fills it? (blocks G6.3)
2. **E2** — is `ABP_BNMannequin.uasset` kept or deleted?
3. **E1** — does `BP_FPSCharacter` still work now that it inherits `UBNAnimInstance`? One PIE run answers it, and it decides whether the shared-main-ABP route holds.
4. **E3 + E4 — graph-clear day.** Not a G-goal because it is a founder call on timing, but it is the largest single cleanup outstanding, and it must be one atomic change.
5. **Ammo** — the GAS-purity exception ledger names ammo; confirm it stays an attribute rather than weapon state before G5.3 assumes it.
