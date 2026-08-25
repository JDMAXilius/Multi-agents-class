# TICKET — which writer wins the aim surface: an AUDIT, not a fix

> STATUS: done — windows terminal 19 Aug 2026. AUDIT COMPLETE, nothing changed (`is_dirty:
> false` on every asset opened). It found the premise of every aim ticket before it was WRONG;
> `TASK-AIM-LYRA-VERIFY` then closed the question on 24 Aug with verdict 1, the aim chain is alive.

**Cut:** 17 August 2026 by the cloud lead · **For:** `bn-editor` / the terminal session (Unreal MCP)
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 first.**

## THE SCOPE — read this twice

This ticket **changes nothing**. It answers four questions and reports. It does not edit a graph,
does not delete a variable, does not recompile anything to "see if that fixes it", and does not
touch `Source/`. **If you find the cause, you still stop and report it.** The decision on what to
change is the lead's, and the last three attempts at this bug were lost precisely to changes made
before the cause was known.

The read-back IS the deliverable.

## Why this exists

The aim chain has survived four rounds of C++ fixes. `DIAGNOSTICS.md` §2 records the current
measured state, and it leaves exactly two suspects:

> `ABP_Mannequin_Base` still carries its twenty Lyra update functions and still runs them from
> `BlueprintThreadSafeUpdateAnimation`, which the engine invokes **after** the native thread-safe
> pass. **Where both write, the graph wins.**

and the bone-space axis (§3.3). Both live inside the asset. Nothing further can be settled from
C++ source, which is why this is an audit and not another patch.

## Q1 — which of the ABP's own functions write the aim surface?

On **`ABP_Mannequin_Base`**, list every function called from `BlueprintThreadSafeUpdateAnimation`,
and for each one report whether it writes any of:

`AimPitch` · `AimYaw` · `Pitch` · `PitchRotator` · `bFPSMode` · `LeanRotation` · `LeanOppRotation`

Report as a table: function name → which of those it writes (or "none"). **This is the whole
question.** If any function writes one of those seven, the graph is overwriting C++ every frame and
the aim fix is to stop that function running — a decision, not your edit.

## Q2 — what do the aim nodes actually READ?

In the AnimGraph, find the Transform(Modify)Bone nodes that bend the spine for aim. For each,
report the exact binding on its Rotation pin — the property name AND which object it resolves
against (`this`, or `GetMainAnimBPThreadSafe`, or a layer-local variable).

This settles the question no source read can: whether the pose consumes the main ABP's copy or the
layer's own copy of these values.

## Q3 — the axis defaults

Report the Class Defaults values of `AimPitchAxis` and `LeanAxis` on `ABP_Mannequin_Base`
(`Roll` / `Pitch` / `Yaw`). These are C++ properties with editor defaults; the founder's playtest
described lean as "a little up and down", which is the signature of aim and lean sharing one axis.

## Q4 — one live reading, if PIE is reachable

Possess a BN pawn, look up and down, and report — from the LIVE instance, not the asset:

| Read | Value |
|---|---|
| `AimPitch` on the main ABP | |
| `Pitch` on the main ABP | |
| `PitchRotator` on the main ABP | |
| `bFPSMode` on the main ABP | |
| the same four on the linked weapon layer instance | |

If the main ABP's values move with the camera and the layer's do not, the break is delivery. If
both move and the body still does not, the break is the node bindings from Q2. If the main ABP's
own values do not move at all, the graph is overwriting them and Q1 names the culprit.

## Done means

Q1's table, Q2's binding list, Q3's two values, and Q4's readings if PIE was reachable — pasted
into the Log below. **Nothing changed.** Anything you were tempted to fix goes in the Log as a
sentence, and the lead decides.

## Log

### 19 Aug 2026 — windows terminal (`bn-editor` under the lead). AUDIT DONE. Nothing changed.

Every asset opened reads `is_dirty: false`; no save, no compile, no PIE, no `Source/` edit, no
git operation. Cause **identified**.

#### THE PREMISE OF THIS TICKET — AND OF EVERY AIM TICKET BEFORE IT — IS WRONG

There are **three** `ABP_Mannequin_Base`, with three different parents, and **the pawn does not
run the one every previous aim ticket edited.** `get_parent`, live editor:

| Asset | Parent |
|---|---|
| `/Game/FPSTemplate/…/ABP_Mannequin_Base` | `UBNAnimInstance` ← all the aim work is here |
| `/Game/MigrateLyra/…/ABP_Mannequin_Base` | `UBNLAnimInstance` ← **the pawn runs THIS** |
| `/Game/Characters/…/ABP_Mannequin_Base` | `BPAnimInstance` (old module) |

`BP_BNCharacter.Default__BP_BNCharacter_C:CharacterMesh0` reads
`AnimClass = /Game/MigrateLyra/…/ABP_Mannequin_Base_C`, `AnimationMode = AnimationBlueprint`.

`UBNLAnimInstance` declares **none** of the seven: no `AimPitch`, `AimYaw`, `Pitch`,
`PitchRotator`, `bFPSMode`, `LeanRotation`, `LeanOppRotation`, and no `AimPitchAxis`/`LeanAxis`.
Its own header says so — *"Lyra / NewMoons test parent … `UBNAnimInstance` stays the production
spine."*

**And that choice reroutes the layers too**, in C++: `ABNCharacter::UsesLyraAnim()`
(`BNCharacter.cpp:511`) tests `IsA(UBNLAnimInstance)`, so `ResolveLyraLayerForRow` picks
`LyraRifleAnimLayer` / `LyraPistolAnimLayer` from `DefaultGame.ini` (`/Game/MigrateLyra/…`) —
**not** `DT_BNWeapons.AnimLayerClass`, which TASK-AIM-NATIVE-OWNER repointed at the FPSTemplate
originals. That MigrateLyra layer set has **no aim ModifyBone chain** and **zero** occurrences of
`PitchRotator`, `LeanRotation`, `LeanOppRotation`, `bFPSMode`.

So the native aim surface, the `PitchRotator` publish, the axis levers, the per-frame push into
linked layers, the `bNativeOwnsAimSurface` checkbox and the `DT_BNWeapons` cell repoint all live
on a class and an asset pair the game never loads. ASSET-RULES §1b's "three copies" hazard,
firing at the TOP of the chain rather than in a layer.

#### Q1 — which functions write the aim surface

**FPSTemplate ABP** (`UBNAnimInstance`, not the live one). `BlueprintThreadSafeUpdateAnimation`
calls twelve functions, not twenty. Three write the surface, all AFTER the native pass, so where
both write the graph wins:

| function | writes |
|---|---|
| `UpdateRotationData` | `PitchRotator` |
| `UpdateRootYawOffset` → `SetRootYawOffset` | `AimYaw` |
| `UpdateAimingData` | `AimPitch` |

Nothing writes `Pitch`, `bFPSMode`, `LeanRotation`, `LeanOppRotation`. These are **not**
Blueprint-locals — `list_variables` returns 46 names and none of the seven; the `Variables|BN|Aim|…`
prefix is the C++ `UPROPERTY(Category="BN|Aim")`. Two measured consequences:

- `UpdateAimingData` overwrites `AimPitch` with raw `TryGetPawnOwner.GetBaseAimRotation.Pitch`,
  discarding `AimPitchClamp` and the whole `ProxyAimInterpSpeed = 15` remote-pawn smoothing
  (`SmoothedAimPitch`). Silent on your own pawn; an **aim staircase on every other player's**.
- `UpdateRotationData` builds `MakeRotator` with **`Pitch` wired to pin 0 = `Roll`** (verbatim from
  the wire: `input_pins[0] name "Roll" <- GetPitch`; Pitch and Yaw pins unconnected, 0.0). The aim
  bend is hard-pinned to Roll every frame **regardless of `AimPitchAxis`** — setting that lever to
  anything but `Roll` cannot survive one frame. It currently *is* `Roll`, so today the write is
  value-identical.

**MigrateLyra ABP (the live one).** Two writers — `UpdateAimingData` → `AimPitch`,
`SetRootYawOffset` → `AimYaw` — and they are the **only** writers in the entire chain, because
`UBNLAnimInstance` publishes no aim at all. Category is `AimingData`: these are Blueprint-local
variables (`list_variables` returns 40 names including `AimPitch`, `AimYaw`, `AdditiveLeanAngle`).
`PitchRotator`, `bFPSMode`, `LeanRotation`, `LeanOppRotation` do not exist on this path in any form.

#### Q2 — what the aim nodes READ

**FPSTemplate path.** The main ABP has **zero** `Transform(Modify)Bone` nodes across all 126 of its
graphs; its `FSP_FullBody_Aiming_Pitch_FPS_Upper`/`_Neck` are empty interface stubs. The 32 aim
ModifyBone nodes live in `/Game/FPSTemplate/…/LinkedLayers/ABP_ItemAnimLayersBase`, 16 per graph on
`spine_01..05`, `neck_01`, `neck_02`, `head`. Rotation pins are unwired and driven by property
bindings, **every one prefixed `GetMainAnimBPThreadSafe`** — the main ABP instance, never `this`:

```
GetMainAnimBPThreadSafe.PitchRotator     x16
GetMainAnimBPThreadSafe.LeanRotation     x14
GetMainAnimBPThreadSafe.LeanOppRotation  x2     (16+14+2 = 32 = the node count exactly)
GetMainAnimBPThreadSafe.bFPSMode         x3
```

No `.AimPitch` / `.AimYaw` / `.Pitch` binding exists in the file. "A layer's own copy" is ruled out
live: `list_variables` on the layer returns 92 names and holds none of those four. Spine weights are
healthy — `AimSpineWeights_UE5` sums to 1.0.

**MigrateLyra path (live).** There are **no aim ModifyBone nodes at all** — 53 graphs, no
`FSP_FullBody_Aiming_Pitch_FPS_*`, only `weapon_r` and `root` for weapon placement and turn-in-place.
Aim arrives as **layer function input parameters** instead: `LinkedInputPose` outputs `AimYaw`/`AimPitch`
into two `RotationOffsetBlendSpace` nodes (`AO_MM_Unarmed_Idle_Ready`), and the caller's
`ALI_ItemAnimLayers-FullBody_Aiming` node property-binds those pins to `this.AimPitch`/`this.AimYaw`.

**Could not read:** per-node binding attribution. `UAnimGraphNodeBinding_Base::PropertyBindings` is a
`TMap` and is not reflected — `get_properties(["PropertyBindings"])` errors. The counts above come from
the on-disk string table and match the node count exactly, which is why the aggregate is stated as fact
and no claim is made that a *specific* ModifyBone is `PitchRotator`.

#### Q3 — the axis defaults

FPSTemplate ABP CDO: `AimPitchAxis = Roll`, `LeanAxis = Yaw`, `AimYawAxis = Yaw`,
`AimPitchClamp = (-90,90)`, `AimYawClamp = (-90,90)`, `LeanAngle = 12`, `LeanInterpSpeed = 8`,
`ProxyAimInterpSpeed = 15`.

The ticket's hypothesis — aim and lean sharing one axis — is **not** what the defaults show for
pitch/lean. But `AimYawAxis` and `LeanAxis` **are both `Yaw`, on the same spine bones**, which is a
real shared axis and the nearest thing to the reported symptom. Reported, not acted on.

On the live MigrateLyra ABP these properties **do not exist** (`"could not be read"`) — Q3 is
unanswerable there. `bNativeOwnsAimSurface` no longer exists on the FPSTemplate ABP either,
consistent with DIAGNOSTICS §2's record that the gate was removed on 14 Aug.

#### Q4 — NOT ANSWERED

`IsPIERunning` → `false`, checked at both ends of the pass; the packet was forbidden to start PIE.
Every value above is asset/CDO, not a live instance. Note for whoever does run it: the probe this
question implies **no longer exists** — DIAGNOSTICS §2 records `BNAimDebug`/`BNAimNative`/`BNAimAxis`/
`BNLeanAxis`/`BNLayerCheck` as removed on 14 Aug. A live Q4 needs a property read on the running anim
instance, not a console command.

#### Seen, reported, not acted on

- `GetCameraPitchWeight` on the FPSTemplate ABP is `(fn GetCameraPitchWeight () (return 0.0))` — a
  hardcoded zero. If anything uses it as a blend alpha it is a constant blend-out. Consumers not traced.
- Blueprint-local `GroundDistance_0` still present and still consumed via
  `GetMainAnimBPThreadSafe.Ground Distance 0` — the Roadmap-1 shadow-rename artifact. Affects ground
  distance, not aim. Re-confirms DIAGNOSTICS §3.2: none of the seven aim names is shadowed.

#### Lead's note, same day

This is the **same root cause** as the missing ADS zoom, found independently: `UBNAnimInstance::
UpdateADSFieldOfView` is on the production spine the pawn does not run, which is why aiming down
sights changed speed and pose but never the lens. One structural fault, at least two symptoms.
The FOV half is now addressed by `FBNADSCameraBlend`, which both anim instances own, so the zoom
works regardless of which instance runs. **The aim half is NOT addressed** — founder parked the
anim-instance decision on 19 Aug. No fix proposed here as an action; the decision is the founder's.
