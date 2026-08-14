# TICKET — native becomes the default aim owner: ONE checkbox, nothing else

**Cut:** 14 August 2026 by the cloud lead · **For:** the terminal session (editor + Unreal MCP)
**Founder ruling:** "Do all" on the reference-research recommendations
([RESEARCH-AIM-REFERENCES](BREACHPOINT-NEXT-RESEARCH-AIM-REFERENCES.md)) — the
components-own-aim configuration matches none of the six reference projects; native owns the
aim surface by default.
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 first.**

## THE SCOPE, stated before the work because it has been a problem

This ticket changes **exactly one property default on exactly one asset** and executes
**one other already-written ticket**. It does not remove the procedural components, does not
touch any layer asset, does not touch `Source/`, does not touch any other Blueprint, does not
"improve" anything it notices. Extra work found is a **Log entry, not a licence**.

## Step 1 — the one checkbox

On the **same shared ABP whose Class Defaults you set `bNativeOwnsAimSurface = false` in your
14 Aug pass** (your own log entry in
[TASK-R3-W3 §Log](BREACHPOINT-NEXT-TASK-R3-W3-MELEE-GRENADE.md)): set
**`bNativeOwnsAimSurface` back to `true`** (equivalently: clear the override so the C++
default, which is true, stands). Compile, save — mind the stale-registry save quirk your own
log documents (`load_asset` first, or Ctrl+Shift+S).

Why: the C++ native path now delivers the aim surface to the linked LAYER instances every
frame (the missing half found 14 Aug), and the reference research is six-for-six that pose
data is pulled from the pawn's own state — the component/interface-event path owns nothing by
default anymore. The components STAY on BP_BNCharacter for what they are uniquely good at
(sway, recoil, pose offsets); they just no longer own Pitch/PitchRotator/bFPSMode.

## Step 2 — THE FOUNDER'S ROOT-CAUSE CALL: originals, not duplicates

**Founder, 14 Aug: the linked layers are the case — the rifle/pistol rows link BN DUPLICATES
of the template layer classes, not the originals.** The evidence chain agrees: the dupes were
cut in R2-W2, BEFORE the main ABP reparent moved Pitch/PitchRotator/bFPSMode to C++, so their
compiled property-access bindings predate the layout they read — silent zeros. Your own R3-W3
log records the base layer recompile and that the dupes "did not dirty". And BP_FPSCharacter
links the ORIGINALS on the SAME shared main ABP and aims correctly — the controlled experiment
already ran.

In `DT_BNWeapons`, repoint two cells (plain path strings, the soft-ref trap as always):

| Row | `AnimLayerClass` → |
|---|---|
| `Rifle` | `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/ABP_RifleAnimLayers.ABP_RifleAnimLayers_C` |
| `Pistol` | `/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/Locomotion/Pistol/ABP_PistolAnimLayers.ABP_PistolAnimLayers_C` |

Do NOT delete the `/Game/BN/Animation/ABP_BNWeaponLayers_*` duplicates in this pass — they come
out after the playtest proves the originals, not before.

## Step 3 — execute the shotgun/knife rows ticket

[TASK-DT-SHOTGUN-KNIFE](BREACHPOINT-NEXT-TASK-DT-SHOTGUN-KNIFE.md), exactly as written — and
its "one open decision" is now CLOSED by the founder's call above: use the TEMPLATE classes
(`ABP_ShotgunAnimLayers_C` / `ABP_KnifeAnimLayers_C`, non-Feminine, non-UE4), not new
duplicates. It is the only blocker for "all four weapons in the swap".

## Step 4 — read back

1. Reload the ABP fresh; print `bNativeOwnsAimSurface` from its Class Defaults — must be `true`.
2. Reload `DT_BNWeapons` fresh; print all four rows' `AnimLayerClass` — every one must resolve
   to a `/Game/FPSTemplate/...` original, none to `/Game/BN/Animation/`.
3. If PIE is available: possess, run `BNAimDebug`, paste the output. The first line must read
   `owner NATIVE`, and the `BNAim layer` lines should now name the ORIGINAL layer classes. If
   the pose still holds still while the main line moves, paste the output and STOP — that is a
   finding for the lead, not something to fix here.
4. The DT ticket's own read-back table, per its spec.

## Done means

Checkbox true, two cells repointed at originals, both new rows landed cell-perfect on template
layer classes, read-backs pasted below. **That is the whole ticket.**

## Log

_(terminal: read-backs, and anything handed back)_
