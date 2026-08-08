# ANIM PORT LEDGER — what `Content/FPSTemplate/` gives us, asset by asset

**Written 8 Aug 2026, BP82 step 2.** Every verdict below cites `mcp-bp/bp_inventory.json` —
a property count, a schema diff, or a named field. **A verdict with no evidence from the
inventory is not a verdict** (ticket step 2), and none here is.

Three verdicts: **PORT** (logic → C++), **KEEP** (content stays an asset), **DROP** (not ours).

Read `docs/contracts/animation.md` Amendment A first; it is the design law this ledger applies.

---

> # ⚠️ THE HEADLINE BELOW WAS WRONG. Read this first.
>
> **Corrected 8 Aug 2026, on founder challenge.** The line *"of 14 Blueprints, exactly ONE carries
> a state model"* was true of the fourteen assets `bp_extract.py` was pointed at, and **false of
> the template**. Its target list never looked in `Blueprints/Components/`.
>
> The template has **43** extractable assets, not 14. The missing 29 include an entire
> **procedural weapon-motion system** — five components on the character plus a base, and sixteen
> `S_Procedural_*` data structs. Corrected count: **five assets carry logic worth porting, not
> one.** The revised verdicts are in "The 29 that were missed" below; the original table is left
> intact beneath it as the dated record of what was concluded from a partial read.
>
> **`BP_FPST_Character`'s own CDO named them the whole time** — `bPC_FPST_Procedural_Recoil`,
> `_SwayAndLag`, `_AimAndLean`, `_PoseOffsets`, `_Manager` were sitting in the committed JSON as
> component properties and nobody followed the names. Sway springs and lean were then written
> from first principles against a purchased, structured implementation that had never been read.
>
> **The transferable lesson, which is worth more than the correction:** a curated target list is a
> *claim about what matters*, and any conclusion drawn from it inherits that claim silently.
> **"14/14 found" reads as completeness and only ever meant "14/14 of what I was told to look
> at."** The count was never the coverage. It is now recorded in `bp_extract.py` beside the list
> itself, because that is where the next person will be tempted to trust it.

## The one-line finding

**The purchase is worth what it cost, and almost none of it is code.** Of 14 extracted
Blueprints, **exactly one carries a state model worth porting** (`ABP_Mannequin_Base`, 96
properties). Everything else is either animation *content* wearing a class (the layer ABPs add
**zero** variables to their base — schema diff 0), stock engine members (`ABP_Mannequin_Retarget`
and `_CopyPose` carry 11 properties each and **all 11 are stock `UAnimInstance`**), or logic that
is already someone else's ticket and violates our laws besides (`BP_FPST_Character`).

The ticket estimated "about five carry logic." The inventory says **one**, and the four it
displaces are named below with the count that displaced them.

---

## anim_spine — the set that mattered

| Asset | Props | Verdict | Evidence, and why |
|---|---|---|---|
| `ABP_Mannequin_Base` | **96** | **PORT** | The prize, as predicted. This IS the anim state model: `localVelocity2D`, `localVelocityDirectionAngle`, `cardinalDirectionDeadZone` (10), `rootYawOffset` + `rootYawOffsetAngleClamp` (`{-120,100}`, crouched `{-90,80}`), `leanRotation`/`leanOppRotation`, `aimPitch`/`aimYaw`, `displacementSpeed`, `yawDeltaSpeed`, `timeSinceFiredWeapon` (9999). Ports to `FPS/BRAnimInstance`. |
| `ABP_ItemAnimLayersBase` | 102 | **KEEP (asset) + PORT (its non-pose half)** | ⚠️ **Overstated on first writing — corrected below.** 55 of 102 are object refs; 47 are not. |
| `ALI_ItemAnimLayers` | **0** | **KEEP (asset), contract split** | ⚠️ **The evidence is a tool failure, not a measurement — see the correction below.** |
| `ABP_Mannequin_Retarget` | 11 | **KEEP** | All 11 are stock `UAnimInstance`: `bPropagateNotifiesToLinkedInstances`, `componentPlayingAnim`, `rootMotionMode`, the six `onMontage*` delegates. **Zero custom variables** ⇒ it is a graph and nothing else. Nothing to port. |
| `ABP_Mannequin_CopyPose` | 11 | **KEEP** | Byte-identical property list to `_Retarget` (same 11 stock names). Same verdict, same evidence. |

## anim_layers — proven content, with better evidence than expected

| Asset | Props | Schema diff vs `ABP_ItemAnimLayersBase` | Value diffs | Verdict |
|---|---|---|---|---|
| `ABP_RifleAnimLayers` | 102 | **0** | 42 | **KEEP** |
| `ABP_PistolAnimLayers` | 102 | **0** | 40 | **KEEP** |
| `ABP_UnarmedAnimLayers` | 102 | **0** | 34 | **KEEP** |

The ticket predicted *"if a layer ABP comes back with zero variables, that PROVES it is pose
content."* What came back is stronger and more precise: they carry 102 properties and **add zero
of their own** — schema diff 0 against their base, three times over. **They are not three
classes. They are three rows of data that UE requires be shaped as classes**, because
`LinkAnimClassLayers()` takes a `TSubclassOf`.

> **CORRECTION (critic REFUTER, finding M10).** This entry originally said *"every one of the
> 34–42 differences is an animation asset slot."* **That is false, and the exceptions matter
> more than the rule.** Recomputed from the JSON:
> - `ABP_RifleAnimLayers`: `raiseWeaponAfterFiringWhenCrouched` false → **true** — a behaviour flag.
> - `ABP_UnarmedAnimLayers`: `disableHandIK` false → **true**.
>
> `disableHandIK` is *precisely* the field `IBRAnimLayer::GetOverridesHandPose()` exists to
> expose. The overstatement threw away the single best piece of evidence that the interface is
> justified. The KEEP verdict stands — a behaviour flag as a class default is R26-shaped, not
> logic — but it stands on **"two flags and ~40 pose slots"**, not on "all pose slots".

They stay assets — and per Amendment A §A.3 that is the *point*: adding a weapon is a row plus a
layer asset and **zero C++**.

## character — nothing survives, and the reason is a law

| Asset | Props | Verdict |
|---|---|---|
| `BP_FPST_Character` | 137 (42 Blueprint-added, 95 stock) | **DROP — in full** |

Not one of the 42 comes to `FPS/`. Each is displaced by a law or another ticket, cited:

- `defaultWalkSpeed` 500 · `sprintWalkSpeed` 800 · `aimWalkSpeed` 220 — **tuning numbers on a
  class**. Law 3: numbers live in `Content/Data/*.csv`. They are evidence of what the pack was
  built to feel like, and they belong in a row, not a header.
- `allWeapons` · `availableWeapons` `["Unarmed","Pistol","Rifle","Shotgun","Knife"]` ·
  `currentWeaponIndex` 2 · `weaponType` — an **inventory on the pawn**. `GAMEPLAY-REWORK §3.5`:
  *"the pawn is a body, not a brain. No health, no scoring, no weapon logic on the pawn, ever."*
  This is `BREquipmentComponent`, i.e. **BP97**.
- `forwardAxisValue` · `rightAxisValue` · `turnAxisValue` · `lookupAxisValue` ·
  `lookSensitivity` · `isAiming?` — **raw input cached on the pawn**. That is **BP92**'s
  Enhanced Input → InputTag → ASC bridge.
- `grenadeSpawnLocation` / `grenadeSpawnDirection` — ability concern, **BP101**.
- `onTakeAnyDamage` · `onTakePointDamage` · `onTakeRadialDamage` — stock delegates, but their
  presence says the template routes damage through the **engine damage API**, which law 2 bans
  outright. One damage door, and it is `BRDamage`.

`ABRCharacter` already exists at `Source/Breachpoint/Character/BRCharacter.h` and is BP96's.
**BP82 writes none of it** — that is what the 8 Aug `FPS/` amendment bought.

## weapons — not this ticket, but the finding is filed

| Asset | Props | Schema diff vs `BP_FPST_BaseWeapon` | Value diffs |
|---|---|---|---|
| `BP_FPST_Weapon_Rifle` | 124 | **0** | 18 |
| `BP_FPST_Weapon_Pistol` | 124 | **0** | 19 |
| `BP_FPST_Weapon_Shotgun` | 124 | **0** | 20 |
| `BP_FPST_Weapon_Knife` | 124 | **0** | 6 |

**DROP (all five), and hand BP97 the evidence.** Four subclasses, **zero** added members between
them, differing only in 6–20 default values. The tool's own comment predicted this: *"if they
differ only in defaults, they are rows, not classes."* They are rows. `BRWeaponInstance` +
`DT_Weapons` is the shape; four classes is the mistake the button module already undid once.

---

## What actually becomes C++

| Concern | Lands as | Source |
|---|---|---|
| The state model | `FPS/BRAnimInstance` — locomotion, cardinal direction, root yaw offset, lean, aim, displacement | `ABP_Mannequin_Base`'s 96 |
| Tag → bool | C++ binding table + `RegisterGameplayTagEvent` | its 5 `gameplayTag_Is*` |
| Jump vs fall, apex estimate | `FPS/BRAnimInstance` | `isJumping`, `timeToJumpApex`, `isFalling` |
| Pivot detection | `FPS/BRAnimInstance` — acceleration opposing velocity | `pivotDirection2D`, `lastPivotTime`, `pivotInitialDirection` |
| One-frame transition edges | `FPS/BRAnimInstance` | `aDSStateChanged`, `crouchStateChange`, `wasADSLastUpdate`, `linkedLayerChanged` |
| Additive weights | `FPS/BRAnimInstance` | `upperbodyDynamicAdditiveWeight`, `applyCrouchAlpha`, `applySwayAlpha` |
| Sway · bob · recoil | `FPS/BRAnimInstance` thread-safe springs → one transform the graph applies | `camRot*` + `applySwayAlpha` |
| Montage → gameplay events | `FPS/BRAnimInstance` notify forwarding, R17 tags, gated to authority/owner | its 6 `onMontage*` |
| The layer contract, code half | `IBRAnimLayer` UINTERFACE | `ALI_ItemAnimLayers` |
| The layer's non-pose state | `FPS/BRAnimLayerInstance` — the C++ base layer ABPs parent to | `ABP_ItemAnimLayersBase`'s non-slot members |
| State machines, blend spaces, layer graphs | **ASSET** — no C++ path in 5.8 | R18 Tier 4 |

### `ABP_ItemAnimLayersBase` — KEEP was right, but it was not the whole answer

The first pass marked it KEEP on the evidence that its 102 properties are animation asset slots.
Re-reading the inventory, that is true of **~90** of them and false of the rest. Those 102 are
**two different kinds of thing wearing one class**:

- **Pose slots** — `aim_HipFirePose`, `bS_FPS_ADS_Idle_Move`, `crouch_Idle_Entry`, `fPS_Sprint`
  and ~86 more. Animation assets. **KEEP**, and porting them would mean hard asset references in
  C++, which law 3 bans outright.
- **Structure and configuration** — `disableHandIK`, `enableLeftHandPoseOverride`,
  `aimOffsetBlendWeight`, and the per-bone aim weights. **PORT**, to `FPS/BRAnimLayerInstance`.

`AimSpineWeights` is a `TMap<FName,float>` keyed by bone rather than eight named floats, and the
inventory is the reason: the base carried `aimSpineWeights_UE5` with **8** bones
(`head`, `neck_01`, `neck_02`, `spine_01`…`spine_05`) and `aimSpineWeights_UE4` with **5**.
Eight named members would have hard-coded one skeleton's spine into C++ and gone silently wrong
on the other.

This also stops `IBRAnimLayer` being dead code — before `BRAnimLayerInstance` existed the
interface had **zero implementors**.

### The template hands us Amendment A's proof, in writing

`ABP_Mannequin_Base` carries `gameplayTag_IsADS`, `_IsFiring`, `_IsReloading`, `_IsMelee`,
`_IsDashing`, and the inventory captured the engine's own description on each:

> *"Bound to a gameplay tag from the Ability System Component. Search for
> `GameplayTagPropertyMap` in Class Defaults."*

Amendment A called `FGameplayTagBlueprintPropertyMap` "the single highest-value finding" and
flagged its include as unverifiable from a cloud container. **Resolved on the machine:**
`GameplayEffectTypes.h` (GameplayAbilities/Public), line 1480.

**We use the mechanism and decline the container**, and the reason is R18, not taste: the
struct's `PropertyMappings` array is `protected` and `EditAnywhere`, so the tag→property table
would be **authored on the ABP asset** — invisible to the critic, no diff, no grep, which is the
exact thing R18 exists to prevent. The engine's *own* callback (`RegisterGameplayTagEvent`,
`AbilitySystemComponent.h:720`) gives the identical no-drift guarantee from a C++ table that
`git diff` can read. This is a **finding against Amendment A's wording**, filed not fixed —
its intent ("the graph can never drift from what the ASC actually says") is met in full.

---

---

# The 29 that were missed

Extracted 8 Aug 2026 after the target list was widened. Same rule as everything above: **every
verdict cites a property count or a named field from `mcp-bp/bp_inventory.json`.**

## procedural — the system this packet was reinventing

| Asset | Props | Verdict | Evidence |
|---|---|---|---|
| `BPC_FPST_Procedural_SwayAndLag` | 24 | **PORT (shapes) / DECLINE (architecture)** | Carries `swayPrevControlRotation` → `swayControlRotDelta`: sway is driven by the **rate of change of control rotation**, never the rotation. Also `applySwayAlpha`, `swayMultiplyValue`, `lagMultiplyValue`, and an `infoMap` of per-weapon `FBRSwayAndLagInfo`. → `FPS/BRSwayAndLagTypes.h` |
| `BPC_FPST_Procedural_Recoil` | 32 | **PORT (shapes) / RULING OWED (behaviour)** | `cameraRecoil_Multiply`, `cameraReturnRotationVector`, `cameraSpringStiffness_RotationVector`, `cameraRecoilFireCount`, `forceInfoArray`, `locationSpringState` + `rotationSpringState`. **It drives the camera** → `contract_gap BP82-9`. → `FPS/BRRecoilTypes.h` |
| `BPC_FPST_Procedural_AimAndLean` | 19 | **PORT (shapes)** | `currInfo.aimSpineWeights_UE4/UE5`, `targetLeaning`/`currLeaning`, `leaningSpeed` 8 (an interp, not a spring — lean settles without overshoot). → `FPS/BRAimAndLeanTypes.h` |
| `BPC_FPST_Procedural_PoseOffsets` | **35** | **PORT (shapes)** | The largest of the five. ADS/scope/crouch offset tables. → `FPS/BRPoseOffsetTypes.h` |
| `BPC_FPST_Procedural_Manager` | 17 | **DROP** | One meaningful member: `proceduralCompAry`. It is a Tick fan-out over the other components — pure architecture, and the architecture is what law 4 declines. |
| `BPC_FPST_Procedural_Base` | 12 | **DROP** | **Zero custom properties.** A shared base carrying only stock `UActorComponent` members; its content is graph, which no tool here can read and which law 4 sends to the worker thread anyway. |

**The architecture is declined as a whole, and the reason is measurable rather than stylistic.**
All five hang off `BP_FPST_Character`, whose `primaryActorTick` reads `bCanEverTick: true` **and
`bAllowTickOnDedicatedServer: true`** — a per-frame procedural animation pass running on a server
that renders nothing. CLAUDE.md law 4 forbids gameplay Tick; `animation.md` law 1 puts anim maths
on the worker thread. BREACHPOINT computes the same quantities in `UBRAnimInstance`'s thread-safe
pass. **Measured, judged, declined** — which is a different claim from "we did it differently".

## procedural_structs — 16 shapes, all ported

**All 16 report `HOLLOW` for values and that is correct, not a failure:** a `UserDefinedStruct`
has no CDO, so there are no defaults to read — the *schema* is the deliverable, and all 16
schemas landed. (The `HOLLOW` note exists because of the earlier silent-empty defect; it is the
guard working.)

| Struct | Fields | Lands as |
|---|---|---|
| `S_Procedural_SpringSetting` | 2 | `FBRSpringSetting` — literally `{spring_Stiffness, spring_Damping}`, the same pair `FBRSpring1D` already took |
| `S_Procedural_ForceInfo` | 3 | `FBRProceduralForce` |
| `S_ProceduralSwayAndLagInfo` | 10 | `FBRSwayAndLagInfo` |
| `S_Procedural_RecoilInfo` · `_RecoilItemInfo` | 7 · 6 | `FBRRecoilInfo` · `FBRRecoilImpulse` |
| `S_Procedural_AimAndLean` | 6 | `FBRAimAndLeanInfo` |
| `S_Procedural_AimSpineInfoItem_UE5` · `_UE4` | **8 · 5** | `FBRSpineWeights` (one type) |
| `S_Procedural_LeanSpineInfoItem_UE5` · `_UE4` | **8 · 5** | `FBRSpineWeights` (same type) |
| `S_Procedural_PoseInfo` | 3 | `FBRPoseInfo` |
| `S_Procedural_PoseOffsetInfo` · `_ItemInfo` · `_ADS_ItemInfo` | 4 · 3 · 4 | `FBRPoseOffsetInfo` · `FBRPoseOffsetItem` · `FBRADSPoseOffsetItem` |
| `S_Procedural_PoseOffset_SetupInfo` · `_Scope_SetupInfo` | 5 · 3 | `FBRPoseSetup` · `FBRScopeSetup` |

**Four structs became one type, and the field counts are the argument.** The spine-weight structs
are 8 fields on UE5 and 5 on UE4 because they name the bones *in the type* — `head`, `neck_01`,
`neck_02`, `spine_01`…`spine_05` versus `head`, `neck_01`, `spine_01`…`spine_03`. One idea,
forced into two types and two code paths by a skeleton change. `FBRSpineWeights` is a
`TMap<FName,float>`: the skeleton becomes data, and a third one costs nothing.

## components — gameplay, and almost none of it ours

| Asset | Props (custom) | Verdict | Evidence |
|---|---|---|---|
| `BPC_FPST_LineTracer` | 23 (9) | **DROP → BP98** | `hitResult`, `hitBoneName`, `hitSurfaceType`, `fireDirection`. This is the **fire trace**, and law 2 admits one damage pipeline. A client-side tracer that decides hits is the shape `netcode.md` exists to prevent — the server revalidates. |
| `BPC_FPST_FireTimer` | 18 (4) | **DROP → BP98** | `fireTimerHandle`, `burstFireCount`, `fireType` "Single", `delay`. Cadence belongs to `BRGA_WeaponFire` reading `EBRFireMode` off the row; `delay` is a law-3 number. |
| `BPC_FPST_MeleeCombo` | 17 (4) | **DROP → BP100** | `meleeComboMontages`, `currComboIndex`, `enableCombo`, `attacking`. Combo state on a component is ability state; `BRGA_Melee` owns it, and its windows are R17 notify tags. |
| `BPC_FPSComp` | 28 (9) | **DROP → BP96/BP10** | Camera: `currentCameraFov` 90, `targetCameraFov`, `cameraFovInterpSpeed` 10, `cameraRotationLagSpeed`. FOV numbers are law-3 rows; the camera is the pawn's. |
| `BPC_FPST_Lyra_Integration` | 32 (10) | **DROP** | The same nine camera fields as `BPC_FPSComp` plus `lastAiming`/`lastIsCrouched`. **Integration glue for a Lyra project we are not** — and near-duplicate of the component above, which is its own finding. |
| `BPC_FPSMT_Integration` | 26 (4) | **DROP** | `aiming`, `isCrouched`, `tempControlRot`, `bMyLocalCharacter`. Glue for a different marketplace pack ("FPSMT") that this project does not use. |
| `BPC_FPST_Lyra_FireEffectComp` | 14 (**0**) | **DROP** | **Zero custom properties.** Whatever it does is entirely graph; there is nothing to port and nothing to read. |

**A finding the counts make visible:** `BPC_FPSComp` and `BPC_FPST_Lyra_Integration` carry the
same nine camera fields (`currentCameraFov` 90, `targetCameraFov` 90, `cameraFovInterpSpeed`,
`cameraRotationLagSpeed`, `enableCameraRotationLag`, `isUpdateCameraFov`, `cameraPitch`,
`bMyLocalCharacter`), differing only in interp-speed defaults (10 vs 12). Two components, one
camera model, copy-pasted — the same "one class per variant" shape the layer ABPs and the weapon
subclasses already showed. **Three independent instances of it in one purchase** is the strongest
argument in this document for keeping the pack as content and none of it as architecture.

---

## Corrections, after an adversarial pass. Kept visible rather than edited away.

A `critic` REFUTER re-derived every verdict above from `bp_inventory.json` rather than reading
this file. **Five reproduced exactly** — schema diff 0 for all three layers and all four weapons,
the 42/40/34 and 18/19/20/6 value-diff counts, `_Retarget`/`_CopyPose` as 11 stock and
byte-identical, and `ABP_Mannequin_Base`'s 96 with every named default
(`cardinalDirectionDeadZone` 10, `rootYawOffsetAngleClamp` {−120,100}, crouched {−90,80},
`timeSinceFiredWeapon` 9999). **Three overreached their evidence**, and all three are corrected
in place above and detailed here.

**C1 — `ALI_ItemAnimLayers` cited a failed read as a measurement (M9).** The entry said *"zero
properties, and that is correct, not a failure… probed three ref forms — all empty."* The cited
JSON says something different in its own words:

```json
{"found": true, "notes": ["could not parse the property list: "],
 "properties": {}, "property_count": 0}
```

`property_count: 0` is **the parse failing**, not the CDO being empty — the note is the tool
reporting that it could not read, which is exactly the `HOLLOW`-class failure the reader was
fixed to announce. The three probed ref forms were run in a throwaway shell and **appear nowhere
in the committed inventory**, so they are not evidence a reviewer can check.

The *conclusion* is still right — an Anim Layer Interface genuinely declares functions, not
variables, and `AnimBlueprint.h` confirms no C++ path. But step 2's rule is *"a verdict with no
evidence from the inventory is not a verdict."* **This verdict is now labelled as resting on the
engine headers, not on the inventory**, which is where its support actually comes from.

**C2 — the layer ABPs' differences are not all asset slots (M10).** Detailed above.

**C3 — `ABP_ItemAnimLayersBase`'s KEEP contradicted this document's own law-3 reasoning (M11).**
The entry said *"a class whose **every** member is a pose reference"*. Measured: **55 of 102 are
object refs; 47 are not**, and ~15 of those are tuning numbers —
`raiseWeaponAfterFiringDuration`, `idleBreakDelayTime`, `strideWarpingBlendInDurationScaled`,
`hipFireUpperBodyOverrideWeight`, `turnInPlaceAnimTime`, `sprintSpineAlpha`,
`handIK_Left/Right_Alpha`. The example this document offered as an "asset slot",
`alpha01_spine_01`, **is a float** (0.15).

That is the same document sending `BP_FPST_Character`'s `defaultWalkSpeed` 500 to a CSV two rows
earlier, citing law 3, and keeping a comparable float here without one. **One rule, two answers.**
The resolution: the non-pose half now ports to `FPS/BRAnimLayerInstance` (see above), and what
remains on the asset is kept under **R40** — sourced content adopted as-is — which is the honest
reason, and was not the reason originally given.

---

## The one thing that could not move, named in advance as the directive asked

**The Anim Layer Interface cannot be declared in C++.** `ALI_ItemAnimLayers` is an
`AnimLayerInterface` asset whose members are **AnimGraph layer functions**; `AnimBlueprint.h`
exposes no interface flag and UE 5.8 has no C++ authoring path for a graph function. R18 already
names AnimGraph graphs as Tier 4, so this is the engine, not a shortcut.

The contract therefore splits, and both halves are honest:

- **Asset half (unavoidable):** the layer functions the AnimGraph calls.
- **C++ half (ours):** `IBRAnimLayer`, the code-facing contract, plus **selection and linking** —
  `USkeletalMeshComponent::LinkAnimClassLayers()` (`SkeletalMeshComponent.h:1194`) is fully C++
  callable, and the layer class resolves from the **weapon row as a soft class**. So no C++
  anywhere hard-refs a per-weapon AnimInstance, which was the actual goal of Amendment A §A.3.

**Second limit, same honesty:** a custom `FAnimNode_*` needs a `UAnimGraphNode_*` in an
**editor module** to be placeable in a graph, and `Breachpoint.uproject` declares exactly one
module (`Breachpoint`, Runtime). Sway/bob/recoil therefore land as **thread-safe computation on
the AnimInstance** — same law ("the graph reads fields, never computes"), no new module. The
custom-node upgrade has a named blocker: **filed as a `contract_gap`, not worked around.**
