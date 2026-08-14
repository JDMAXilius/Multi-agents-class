# TICKET — the cue Blueprints: C++ keeps the logic, the editor keeps the references

**Cut:** 13 August 2026 by the cloud lead, from the founder's standing ruling ·
**For:** the terminal session (editor + Unreal MCP)
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §7 first** — it is the rule this executes,
and §5 (do only what this ticket lists) still governs.

**Owner path:** `Content/BN/AbilitySystem/Cues/`. **Never `Source/`.**

## Why this exists

The C++ cue classes already declare their assets as soft references. The **values** live in
`Config/DefaultGame.ini`, which means changing a muzzle flash is a config edit. The founder's rule:
soft in C++, **set in the editor**, wired by you through the MCP.

The C++ prerequisite has landed — the registrar now keeps the **most-derived** class per tag, so a
Blueprint child correctly wins over its C++ parent instead of the two fighting non-deterministically.
**Do not start this against a build that predates that commit.**

## Step 1 — four Blueprint children, defaults only

Create in `/Game/BN/AbilitySystem/Cues/`. Each is a **direct BP child of its C++ class**, with
**empty graphs and no new variables** — defaults only. That is R26's narrow exception; a BP that
adds a graph or a member is a finding.

| Create | Parent C++ class |
|---|---|
| `BP_BNCue_MuzzleFlash` | `UBNGameplayCue_MuzzleFlash` |
| `BP_BNCue_Impact` | `UBNGameplayCue_Impact` |
| `BP_BNCue_Tracer` | `UBNGameplayCue_Tracer` |
| `BP_BNCue_Explosion` | `UBNGameplayCue_Explosion` |

`BlueprintTools.create`'s parent-class picker has wedged the game thread before
(`BREACHPOINT-NEXT-CONTENT-LAYOUT.md`) — set the parent **in the create call**, not after.

## Step 2 — set the asset references, from these exact paths

Every path is verified to exist on disk. They are the same values the ini currently carries, so a
correct result changes nothing visible — **that is the point**: this migration must be invisible.

**`BP_BNCue_MuzzleFlash`**
| Property | Value |
|---|---|
| `Effect` | `/Game/FPSTemplate/Demo/Effects/Particles/Weapons/NS_WeaponFire_MuzzleFlash_Rifle` |
| `Sound` | `/Game/FPSTemplate/Demo/Audio/Sounds/Weapons/Rifle2/MSS_Weapons_Rifle2_Fire` |

**`BP_BNCue_Tracer`**
| Property | Value |
|---|---|
| `Effect` | `/Game/FPSTemplate/Demo/Effects/Particles/Weapons/NS_WeaponFire_Tracer` |

**`BP_BNCue_Explosion`**
| Property | Value |
|---|---|
| `Effect` | `/Game/FPSTemplate/Effects/Particles/Explosion/NS_Grenade_Explosion` |
| `Sound` | `/Game/FPSTemplate/Demo/Audio/Sounds/Explosions/MSS_Explosions_Grenade` |

**`BP_BNCue_Impact`**
| Property | Value |
|---|---|
| `Effect` | `/Game/FPSTemplate/Demo/Effects/Particles/Impacts/NS_ImpactConcrete` |
| `Decal` | `/Game/FPSTemplate/Demo/Effects/Particles/Impacts/NS_ImpactDecals` |
| `SurfaceRows[0]` | Surface `SurfaceType1` · Effect `…/Impacts/NS_ImpactSparksCharacter2` · Sound `…/MetaSounds/sfx_ImpactCharacter_nl_meta_Preset` |
| `SurfaceRows[1]` | Surface `SurfaceType2` · Effect `…/Impacts/NS_ImpactConcrete` · Sound `…/MetaSounds/sfx_ImpactPlaster_nl_meta` |
| `SurfaceRows[2]` | Surface `SurfaceType3` · Effect `…/Impacts/NS_ImpactGlass` · Sound `…/MetaSounds/sfx_ImpactGlass_nl_meta_Preset` |

MetaSounds are under `/Game/FPSTemplate/Demo/Audio/MetaSounds/`; impact particles under
`/Game/FPSTemplate/Demo/Effects/Particles/Impacts/`.

**The soft-ref trap, already paid for once:** through the MCP, `set_rows` and property setters
silently drop `{"refPath": …}` objects on soft-pointer properties — they read back `None`. **Plain
soft-path strings land.** Read every one back.

## Step 3 — read back, and prove which class answers

Reload each Blueprint fresh and print intent vs actual per property.

Then **start PIE and find these log lines**:

```
BNCues: GameplayCue.Weapon.MuzzleFlash -> BP_BNCue_MuzzleFlash_C
BNCues: GameplayCue.Weapon.Impact      -> BP_BNCue_Impact_C
BNCues: GameplayCue.Weapon.Tracer      -> BP_BNCue_Tracer_C
BNCues: GameplayCue.Grenade.Explode    -> BP_BNCue_Explosion_C
```

**If any line still names the C++ class** (`UBNGameplayCue_MuzzleFlash` etc.), the Blueprint is not
winning its tag — **stop and report**. Do not "fix" it by deleting the ini lines; that would only
hide which handler ran.

## What this ticket does NOT do

- **Do not touch `Config/DefaultGame.ini`.** The ini lines stay as the fallback until the founder
  has playtested the Blueprints. Removing them is the lead's, in a later commit, and only then.
- **Do not touch `Source/`.** If something here cannot be done without a C++ change, hand it back.
- **Do not create Blueprints for abilities, effects or the weapon FX set.** Those are separate
  steps in `BREACHPOINT-NEXT-PACKET-EDITOR-ASSETS.md`, deliberately sequenced after this one, and
  two of them are behaviour changes rather than reference moves.
- **Do not add a graph node or a variable to any of these four.** Defaults only.

## Done means

Four Blueprints exist with every property set and read back, the four `BNCues:` log lines name the
**Blueprint** classes, firing a weapon and a grenade still looks and sounds exactly as it did, and
the read-back table is pasted into the Log below.

## Log

_(terminal: append the read-back table, the four BNCues lines verbatim, and anything handed back)_

- 14 Aug 2026 (mac terminal) — **DONE.** Four BP children created with the parent set in the
  create call (no picker raised), defaults only, every property set as plain soft-path strings
  (the refPath trap avoided on read-back attempt #1). Fresh read-backs all exact, including
  Impact's three SurfaceRows with sounds. **PIE log, verbatim per tag (final registration):**
  `GameplayCue.Weapon.MuzzleFlash -> BP_BNCue_MuzzleFlash_C` · `GameplayCue.Weapon.Impact ->
  BP_BNCue_Impact_C` · `GameplayCue.Weapon.Tracer -> BP_BNCue_Tracer_C` ·
  `GameplayCue.Grenade.Explode -> BP_BNCue_Explosion_C`. **Finding for the lead (not fixed):**
  the registrar also logs a `SKEL_BP_BNCue_*_C` registration line per tag before the real one —
  GetDerivedClasses picks up the editor-only skeleton classes; harmless in PIE, worth a
  class-flags filter. The ini lines were NOT touched, per this ticket's own fence.
