# R3 G1 — why the arms never followed the camera

**Cut:** 13 August 2026 by the cloud lead, from the founder's playtest + a source read of
`MyCharacter` · **Binds to:** the NEXT doc family.

## The finding, in one line

**Nothing in BN ever wrote `PitchRotator`** — the property the aim layers actually read — and
the Blueprint hop that was supposed to build it from `Pitch` is not landing.

## How the working template does it, and why that misled us

`MyCharacter` does **not** compute aim or lean. It is a reflection shim over the FPS template's
Blueprint components, and its own header says so at `MyCharacter.cpp:1141-1144`:

> Only the bool-taking messages are wired here. The struct-taking ones (`SetAimAndLeanInfo`,
> `SetPoseTransform`, `SetProcApplyTransform`) carry `S_Procedural_*` user-defined structs whose
> layouts would have to be mirrored to be passed … **they land with their procedural components.**

So the live aim writer in the template is `BPC_FPST_Procedural_AimAndLean` — it ticks, computes,
packs the structs and calls `SetAimAndLeanInfo` on the anim instance **directly**. The character
only pokes `SetLeaning` at it (`MyCharacter.cpp:1427`). The founder's hypothesis — *"it might be
the procedural components"* — was **correct**.

That component is what BN removed and never replaced. R3 Wave 1 replaced half of it: it published
the scalar `Pitch` and relied on the asset's `UpdateRotationData` to turn that into `PitchRotator`.

## Why the half-fix could not work

The linked aim layers (`FSP_FullBody_Aiming_Pitch_FPS_Upper` / `_Neck`) bind their
Transform(Modify)Bone rotation pins to **`GetMainAnimBPThreadSafe.PitchRotator`** — not to
`Pitch`. `PitchRotator` appears **nowhere in `Source/BreachpointNext/`**; it existed only as a
Blueprint variable written by a Blueprint function. Every bone in the chain therefore applied a
zero rotator. The camera still moved because `UCameraComponent::bUsePawnControlRotation` takes
control rotation directly and never goes near the pose — which is exactly the split the founder
described: **the shot goes where you look, the body does not.**

## The fix

`UBNAnimInstance` now declares and writes `PitchRotator` itself, removing the Blueprint hop.
`Pitch` is still published, so the asset's own graph agrees either way.

## The axis, which is measured and not derivable

Aim and lean are Transform(Modify)Bone nodes in **bone space**. A spine bone's local axes are not
the world's, so "pitch the torso up" is not `FRotator::Pitch`. Two pieces of evidence:

- The asset's own graph builds its aim rotator as `MakeRotator(Pitch)` into the **first** pin,
  which is **Roll**.
- The founder's playtest reported lean — which BN was writing as **Roll** — as *"just a little up
  and down"*, i.e. Roll produced the **aim** bend, not a sideways tilt.

Both point the same way: **aim = Roll, and lean must be a different axis.** Lean now defaults to
Yaw. Neither is provable from source, so both are `EBNSpineAxis` properties with live console
levers rather than literals — a rebuild per guess is the wrong price.

| Command | Does |
|---|---|
| `BNAimDebug` | Logs the whole chain on one line: BaseAimPitch → ActorPitch → AimPitch → Pitch → PitchRotator (+ axis), `bFPSMode`, `bUnarmed`, lean, linked layer |
| `BNAimAxis 0\|1\|2` | Aim axis: 0 Roll · 1 Pitch · 2 Yaw |
| `BNLeanAxis 0\|1\|2` | Lean axis, same ordinals |

All three are local-only and cosmetic — an anim instance is per-machine, so nothing forwards to
the server. They work in a client window (they are controller execs, per the `AddCheats` finding).

`BNAimDebug` is also the PIE probe R3 has been owed since the terminal's session was cut short. A
zero in it names the broken link: no `BaseAimPitch` = the view is not reaching the pawn; `Pitch`
good but `PitchRotator` zero = the publish; both good and still no motion = the ABP binding or the
`bFPSMode` gate.

## ⚠ The one editor risk this carries

A C++ property only absorbs a Blueprint variable when the names match **and the Blueprint's own
copy is gone**. If `ABP_Mannequin_Base` still carries a BP-local `PitchRotator`, the reparent
renames it `PitchRotator_0` and the graph nodes follow the **renamed** one — leaving the new C++
property orphaned and the pose still frozen. This is the same mechanism as the `GroundDistance_0`
error from Roadmap 1.

**After the rebuild:** open `ABP_Mannequin_Base`, and if a `PitchRotator_0` appears (or the
compiler warns), delete the BP-local duplicate so the binding re-resolves to the C++ property.

## Lean's second open question

The terminal's two passes disagreed: the first found `LeanRotation` had **writers only, no
reader**; the last found the lean chain's ModifyBone bindings **do** read it (mined from the
package name table). The founder's playtest settles it — leaning visibly moves something, so a
reader exists. The first read was an MCP reflection limit, not a fact.

## Still owed on R3

- **Melee** (G3) — never built.
- **Grenade** — not in any roadmap; `MyCharacter` has `OnGrenadeKey` / `GrenadeThrowDelay` /
  `GrenadeClass` as the reference.
- **Per-weapon fire sound** — the cue now plays `DT_BNWeapons.FireSound` with a Config fallback,
  but the table's two rows still need filling (`MSS_Weapons_Rifle2_Fire`, `MSS_Weapons_Pistol_Fire`).
- **Editor-side GAS assets** — the founder's ruling that cues/abilities carry their asset refs in
  the editor rather than in ini. Supersedes ASSET-RULES §3; needs its own packet.
