# THE MYCHARACTER CONTRACT — how the founder's reference class actually does it

**Cut:** 14 August 2026 by the cloud lead, from `Source/Breachpoint/Character/MyCharacter.{h,cpp}`
(the exec-order C++ port of `BP_FPST_Character`) and `Source/Breachpoint/anim/ABPMannequinBase.h`
(the extracted variable record of the shared ABP). This is the reference the founder keeps
pointing at; every BN divergence should be judged against THIS page.

## 1. The communication model — the sentence that settles the architecture

MyCharacter.cpp:1124 says it outright:

> *"The template does NOT push state through a component and it does NOT have the AnimBP cast
> back to the character and pull. It is a Blueprint INTERFACE implemented by ABP_Mannequin_Base,
> and the character sends messages to `Mesh->GetAnimInstance()`. ABP_Mannequin_Base stores each
> into a variable; ABP_ItemAnimLayersBase and its per-weapon children READ THOSE to choose the
> pose."*

Three consequences, each load-bearing for BN:

- **Every message lands on the MAIN anim instance.** Nothing is ever sent to a layer.
- **The layers READ the main ABP's variables.** They do not hold authoritative local copies fed
  separately — the main ABP's variables ARE the surface.
- **Therefore: writing the main ABP's variables directly (BN's native path) is functionally
  identical to the template's messages having arrived.** The native path is not a deviation from
  the template's model — it is the same model with the messenger inlined.

Which messages come from where:
- **The character itself** sends the BOOL messages, event-driven: `SetSprinting`, `SetADS`,
  `SetADS_Upper`, `SetUnarmed`, `SetFPSWalkMode` (MyCharacter.cpp:1278-1305, via reflection).
- **BPC_FPSComp** sends the per-frame view messages: `SetControllerPitch`, `SetFPSMode`
  (the terminal's 14 Aug trace; MyCharacter deliberately wires none of the float/struct ones).
- **BPC_FPST_Procedural_AimAndLean** sends `SetAimAndLeanInfo` (lean rotations + spine weights).

## 2. What the class comparison found missing in BN — both fixed 14 Aug

1. **`IsADS_Upper` had no writer.** The ABP has TWO ADS variables (`GameplayTag_IsADS` :351 and
   `IsADS_Upper` :361 in the extraction), and `OnAimStarted` sends BOTH `SetADSUpper(true)` and
   `SetADS(true)` (MyCharacter.cpp:964-976). BN only ever wrote the first — the upper-body/weapon
   side of the ADS pose never heard about ADS. Now written by native each frame (by reflection,
   NOT a declared C++ property — declaring it would `_0`-rename the ABP's variable on
   reparent-compile, the GroundDistance trap) and pushed to layers.
2. **The ADS POSE call did not exist.** MyCharacter.cpp:1307: *"ChangePose and
   ChangeCameraTargetFOV are what make ADS read as ADS: the weapon pose interpolates to the aim
   offset and the camera FOV narrows. Both were empty hooks, so aiming changed the walk speed and
   the anim-interface bools and nothing visibly moved."* BN had the FOV half only — ADS zoomed
   while the gun stayed at hip. Now: `ABNCharacter::HandleADSTagChanged` listens to the
   replicated `State.Weapon.ADS` tag (every machine — cosmetics stay per-machine) and calls
   `ChangePose(InPoseType, InScopeType, InChangeSpeed)` on the Blueprint PoseOffsets component by
   reflection, with the graph's own ordinals (hip/aim × stand/crouch = 1/2/6/7, scope 0,
   speed 18). Warns once, MyCharacter's convention, if the component or function is missing.

## 3. How MyCharacter handles each remaining system, and where BN stands

- **Lean:** Q/E → `SetLeaning(-1/0/+1)` on the AimAndLean COMPONENT; the component interpolates
  `CurrLeaning` and feeds the spine weights (cpp:992-995, 1427-1459). BN inlines exactly that
  interp natively (targets ±1/0 from the replicated `State.Lean.*` tags, interp 8) — same math,
  tag-driven input, and the 14 Aug fix made native its unconditional owner. Equivalent.
- **Melee:** montage on the mesh's anim instance, `OnPlayMontageNotifyBegin` bound ONCE lazily,
  trace gated on the notify NAME `AN_FPST_Melee` (cpp:1257-1276). **Detail that matters:** the
  port's note says that notify is verified on `AM_MM_Knife_Swing01` — the rifle/pistol melee
  montages may not carry it, in which case BN's 0.25s fallback clock is what connects. BN's GA
  mirrors the whole shape (bind once per activation, same notify name, fallback hedge). Equivalent
  plus a safety net.
- **Weapons:** `CreateWeapons` spawns ALL FOUR from a config list of soft class paths, attach
  socket read off the weapon, `NextWeapon`/`PrevWeapon` cycle the available array, hide-all +
  show-current, per-weapon anim layer via `LinkAnimClassLayers` with the class read off the
  weapon (h:69-95, 177-196). BN is the same design with rows instead of reflection — and BN's
  four-weapon gap is purely the two missing DT rows, exactly as diagnosed.
- **Aim ray / traces:** controller view point, never the camera component (whose transform sits
  at the feet — GetAimRay's documented trap). BN's fire/melee traces already do this.
- **Spread/pellets/fire modes:** per-weapon variables read by reflection (ShotCount 6 = shotgun).
  BN carries them as row columns. Equivalent.

## 4. The weights question, closed

The extraction (ABPMannequinBase.h:95-169, 295-307) shows `AimSpineWeights_UE4/UE5` and
`LeanSpineWeights_UE4/UE5` with NON-ZERO defaults (spine 0.1-0.3, head-opposite 0.25,
FPSPelvisWeight 0.2). So the per-bone weight multipliers are live even when nothing sends
`SetAimAndLeanInfo` — a zero-weights chain is NOT the frozen-aim culprit, and the terminal's
component install did not need to happen for the weights' sake. Struck from the suspect list.

## 5. What stays true after this page

BN's native path = the template's model with the messenger inlined (§1). The reference research
(RESEARCH-AIM-REFERENCES) already ruled native the default owner; this page adds MyCharacter's
own testimony to the same verdict. The open question the probe answers on the founder's next
run is unchanged: whether the LAYERS' read of the main ABP's variables survived the reparent —
`BNAimDebug`'s per-layer lines are that answer.
