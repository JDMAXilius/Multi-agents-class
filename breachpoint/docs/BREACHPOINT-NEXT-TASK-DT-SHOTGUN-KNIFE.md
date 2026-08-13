# SPEC — `DT_BNWeapons` rows: Shotgun and Knife

**Cut:** 13 August 2026 by the cloud lead · **For:** the founder at the editor
**Binds:** [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) — **§1b especially**: every path below is
`/Game/FPSTemplate/…`, and same-named duplicates exist elsewhere in `Content/` that are **not ours**.

Two new rows, `Shotgun` and `Knife`. Row names are keys — `FindWeaponRow(FName)` takes them
literally, and `StartupWeaponRows` in `DefaultGame.ini` names them.

## What I read off disk, and what I could not

I inventoried the FPSTemplate weapon folders directly, so **every path below is verified to exist**.
What I **cannot** read from this container are values inside the weapon Blueprints' CDOs — the
`.uasset` files are Git-LFS pointers here. Those rows are marked **READ FROM CDO** and must not be
guessed: a wrong socket name is silent, and the weapon attaches at the mesh root with nobody able to
see why (the `DT_BNWeapons` ticket paid for that lesson already).

---

## Row: `Shotgun`

| Field | Value | Source |
|---|---|---|
| `WeaponMesh` | `/Game/FPSTemplate/Demo/Weapons/Shotgun/Mesh/SKM_Shotgun` | **verified on disk** — note **`SKM_`**, not `SK_`. Rifle and pistol are `SK_Rifle`/`SK_Pistol`; the shotgun and knife break that convention. `SK_Shotgun_Skeleton` and `SK_Shotgun_PhysicsAsset` exist and are **not** the mesh |
| `FireMontage` | `/Game/FPSTemplate/Demo/Weapons/Shotgun/Animations/AM_MM_Shotgun_Fire` | verified — same folder shape as the rifle's |
| `ReloadMontage` | `/Game/FPSTemplate/Demo/Weapons/Shotgun/Animations/AM_MM_Shotgun_Reload` | verified |
| `MeleeMontage` | `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/Actions/AM_MM_Shotgun_Melee` | verified — melee montages live with the CHARACTER, not the weapon |
| `FireSound` | `/Game/FPSTemplate/Demo/Audio/Sounds/Weapons/Shotgun/MSS_Weapons_Shotgun_Fire` | verified |
| `AnimLayerClass` | see **the one open decision** below | — |
| `ShotCount` | **6** | The `DT_BNWeapons` ticket recorded this: *"1 on both of these (6 is the shotgun's)"* |
| `FireMode` | `Single` (0) | Pump action. Confirm against the CDO's `availableFireModes[curFireModeIndex]` |
| `AttachSocketName` | **READ FROM CDO** | Rifle `weapon_r_rifle`, pistol `weapon_r_pistol` — so `weapon_r_shotgun` is the likely shape, but **do not assume it**. A socket on the CHARACTER mesh |
| `MuzzleSocketName` | **READ FROM CDO** | Both existing rows read `Muzzle`. On the WEAPON's own mesh |
| `FireDelay`, `SpreadAngle`, `MagazineSize` | **READ FROM CDO** | Rifle and pistol both read 0.1 / 0.1. A shotgun's spread should be much wider — if the CDO says 0.1, that is worth reporting, not copying |
| `Damage` | **20 → raise it** | C++ default. 20 × 6 pellets = 120, which one-shots through a full 100 shield + 100 health. Suggest **12** (72 at point blank) and tune from there — this is the one number I would not leave at default |
| `MeleeDamage` / `MeleeRange` | 40 / 120 | C++ defaults, fine |
| `HeadshotMultiplier`, `Range`, `ReloadTime`, `BurstShotCount` | C++ defaults | As the rifle/pistol rows did |
| `AbilitySet` | `DA_BNAbilitySet_Rifle` | It grants Fire + Reload, which is all a shotgun needs. A shotgun-specific set only earns its existence when the shotgun's verbs differ |

## Row: `Knife`

The knife is **melee-only** — no fire, no reload, no ammo. The row still exists so the knife is a
weapon like any other: it has a mesh, a layer and a socket.

| Field | Value | Source |
|---|---|---|
| `WeaponMesh` | `/Game/FPSTemplate/Demo/Weapons/Knife/Mesh/SKM_Knife` | **verified on disk** — `SKM_` again |
| `MeleeMontage` | `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/Actions/AM_MM_Knife_Swing01` | verified. The template ships **`Swing01` only** for UE5; `Swing02` exists for the UE4 mannequin only |
| `FireMontage` / `ReloadMontage` / `FireSound` | **leave empty** | None exist, and none should. Every consumer treats an empty soft ref as "nothing to play" |
| `AnimLayerClass` | see below | — |
| `AbilitySet` | **leave None** | Deliberate: no Fire, no Reload. LMB does nothing while the knife is out, which is honest — the knife's attack is melee, and melee is granted by the PlayerState, so **V works with no set at all** |
| `MeleeDamage` | **100** | It is a knife. One clean hit through a full shield-and-health bar is the point of carrying one |
| `MeleeRange` | **150** | Slightly longer than the 120 rifle-butt default — a knife lunges |
| `MagazineSize` | 0 | `HasAmmo(1)` returns false, `CheckCost` refuses, and Fire could not activate even if it were granted. Belt and braces |
| `AttachSocketName` | **READ FROM CDO** | Likely `weapon_r_knife`; do not assume |
| `MuzzleSocketName` | leave default | A knife has no muzzle. Nothing reads it without a Fire ability |
| Everything else | C++ defaults | |

---

## The one open decision: `AnimLayerClass`

The existing two rows point at **BN duplicates** — `ABP_BNWeaponLayers_Rifle_C` and
`_Pistol_C` — not at the template's `ABP_RifleAnimLayers_C` / `ABP_PistolAnimLayers_C`. The
`DT_BNWeapons` ticket log records that as *"the deliberate divergence"* per R2-W2, but **does not
record why the divergence was needed.**

That matters, because ASSET-RULES §2 permits duplication only when we must diverge — and copying a
divergence whose reason nobody wrote down is how a habit forms.

**My recommendation: try the template classes first.**

| Try | Then |
|---|---|
| `…/Locomotion/Shotgun/ABP_ShotgunAnimLayers` and `…/Locomotion/Knife/ABP_KnifeAnimLayers` (append `_C`) | If the shotgun and knife pose correctly, we have two fewer assets to keep in sync, and we have learned the rifle/pistol duplication was situational |
| If they misbehave the way the rifle did before its duplicate existed | Duplicate to `/Game/BN/Animation/ABP_BNWeaponLayers_Shotgun` and `_Knife`, and **write the reason in the Log this time** |

Both template paths are verified on disk. Use the **non-`_Feminine`, non-`_UE4`** variants — the
`_UE4` set is for the old mannequin and `_Feminine` is a body-type variant BN does not use.

## Two things that will need C++ afterwards, and are not this ticket

1. **Shotgun pellets and the server's confirm.** `UBNGA_Fire` traces `ShotCount` pellets locally and
   sends one TargetData entry per pellet, and `OnTargetDataReceived` re-traces **each** — so six
   pellets is six server traces and six impact cues per shot. That is correct but untested; six
   validated hits on one target in one frame is also six separate `BNDamage` applications, which is
   how a shotgun should work but has never been run.
2. **`StartupWeaponRows`.** Adding `+StartupWeaponRows=Shotgun` and `=Knife` to
   `[/Script/BreachpointNext.BNEquipmentComponent]` puts four weapons in the swap cycle. That is a
   one-line ini change I can make on request — say the word and I will, but it is a behaviour change
   to the loadout, so it is your call rather than mine to assume.

## Done means

Both rows exist in `/Game/BN/Data/DT_BNWeapons`, saved, with a read-back table pasted below naming
per field: the value, and whether it came from **this spec**, **the weapon BP's CDO**, or a
**C++ default**. Every `READ FROM CDO` field resolved or explicitly recorded as unresolved.

**Watch the soft-ref trap the first DT ticket paid for:** through the MCP, `set_rows` silently drops
`{"refPath": …}` objects on soft-pointer columns — they read back `None`. Plain soft-path strings
land. By hand in the editor this does not apply.

## Log

_(append the read-back table, the AnimLayerClass verdict and why, and anything unresolved)_
