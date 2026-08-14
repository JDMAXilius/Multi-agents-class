# ADS — aim down sights: the research

**Cut:** 13 August 2026 by the cloud lead · **Status:** BUILT — the critic's findings were fixed
first, then the C++ landed exactly per §7's plan. Uncompiled like everything else this session.

ROADMAP-3 predicted this packet's shape when it deferred it: *"ADS (needs G1's aim to exist first —
it is the same machinery with a camera FOV blend)."* G1 exists. This is that machinery.

---

## 1. What the survey found — three unknowns settled before design

**The ABP is already tag-shaped for ADS.** The old module's port of `ABP_Mannequin_Base` names the
variable outright: **`GameplayTag_IsADS`** (`ABPMannequinBase.h:351`) — the animation pack itself
thought of ADS as a gameplay-tag mirror. BN's whole discipline is "the ASC holds state, the anim
instance publishes tags into asset-named properties" — this variable was *waiting* for that. Also
ported beside it: `IsADS_Upper`, `WasADSLastUpdate`, `ADSStateChanged` (see §5 for the publish
rules those last two demand).

**The input asset already exists and is the template's.** `IA_FPST_Aim` ships in
`/Game/FPSTemplate/Input/Actions/` — ASSET-RULES §4 says reuse it, so ADS needs **no new input
asset**, only a `DA_BNInput` row and an IMC mapping (right mouse). Added to the INPUT-WIRING
ticket as row 4.

**The anim content is complete.** Per-weapon ADS idles, additives, ADS move blendspaces and aim
offsets (`AO_MM_Rifle_Idle_ADS`, `Anim_FPS_*_ADS_*`, `BS_MM_*_ADS_Move_*`) for rifle, pistol and
shotgun — all consumed inside the existing anim layers, which select the ADS path off the ABP's
variables. Nothing to author, nothing to wire per-asset: **flip the variable, get the pose.**

**The reference's measured numbers** (`MyCharacter.h`): `AimWalkSpeed` 250 (vs 600 base →
multiplier ≈ 0.4167), `AimFOV` 80, `AimPoseChangeSpeed` 18.

## 2. The design — sprint's shape, almost exactly

ADS is a held state that changes move speed and pose. BN already has one of those, reviewed and
founder-tested: **sprint**. The design copies it deliberately, piece by piece:

| Piece | Sprint has | ADS gets |
|---|---|---|
| Ability | `UBNGA_Sprint`, LocalPredicted, lives while held | `UBNGA_ADS`, same, input tag `Input.Weapon.ADS` on `IA_FPST_Aim` (RMB) |
| State | `State.Movement.Sprinting` via GE tag (Mixed → sim proxies see it) | `State.Weapon.ADS`, same mechanism |
| Speed | `UBNGE_Sprint`: MULTIPLY on MoveSpeed, magnitude captured from the `SprintSpeedMultiplier` **attribute**, non-snapshot | `UBNGE_ADS`: identical, from a new `ADSSpeedMultiplier` attribute, init 0.4167 — tuning never touches ability code, removal restores base through GE aggregation |
| Grant | PlayerState (body verb, survives swaps) | Same |
| Pose | tag → anim instance snapshot → publish | tag → publish **`GameplayTag_IsADS`** (the asset's own name — the reparent absorbs it) |

**Mutual exclusion with sprint, one direction:** ADS is refused while `State.Movement.Sprinting`
is up (you do not aim down sights mid-sprint), checked in `CanActivateAbility` exactly the way the
dead-check works. Sprint is NOT blocked by ADS — pressing sprint while aiming simply wins, and its
gate's speed GE stacking with the ADS multiplier is prevented by the exclusion. One rule, not two.

**Per-weapon gating is data:** `FBNWeaponRow` gains `bCanADS` (default true; the Knife row sets
false — you do not aim down a knife). The ability checks the current row, same as fire checks ammo.

## 3. The FOV — the one genuinely new mechanism, and the law it brushes

The camera is owner-only cosmetics; sim proxies get their ADS look from the pose. The blend
(90 → 80 at ~`AimPoseChangeSpeed`) needs a per-frame interp, and **law 4 says no gameplay Tick**.
The lawful home already exists: `UBNAnimInstance::NativeUpdateAnimation` runs every frame on the
game thread and is the project's established presentation brain — it will drive the camera FOV
interp for the **locally controlled** pawn only, off the same ADS tag snapshot it already takes.
The camera already rides the mesh; the mesh's brain adjusting the lens is the same ownership.
No new tick, no timer pretending to be one.

## 4. Descope — the recorded trigger fires

The founder's Halo ruling put descope on record: *hit while aiming → knocked out of ADS.* The
signal exists — `State.Combat.RecentDamage` is applied by every landed hit — and `UBNGA_ADS`
cancels itself on that tag's arrival (RegisterGameplayTagEvent, the anim instance's own pattern).

**This reopens a gate on schedule.** The damage packet skipped applying RecentDamage while shields
are off, recording: *"IF State.Combat.RecentDamage ever gains a second reader, this gate has to go
with it."* Descope IS the second reader. The gate comes out in the ADS packet — RecentDamage
applies on every hit again, unconditionally — and the recorded trigger has done exactly its job.

## 5. Publish discipline for the ABP's ADS fields

- **`GameplayTag_IsADS`** — published ungated, single-writer argument identical to `Pitch`'s: the
  event graph's only writer is the never-fired `SetADS` interface event.
- **`WasADSLastUpdate` / `ADSStateChanged`** — RMW/edge accumulators, the `CrouchStateChange`
  class exactly: native computes them against private history, publishes **only under
  `bNativeOwnsTurnState`**, because the live event graph still runs the same edge math and two
  writers double-count.
- **`IsADS_Upper`** — NOT published in v1. `SetADS_Upper` was a separate template message and what
  reads the variable is unverified from here; a same-named property published wrong is worse than
  absent. Logged as the packet's one editor question (find its reader, then decide).

## 6. Deliberately not in v1, with triggers

| Deferred | Trigger |
|---|---|
| ADS fire/reload montages (`AM_MM_Rifle_Fire_Aim`, the row's `Aim*` variants) | First playtest where hip-fire montage while aiming reads wrong — a row column each, no code shape change |
| ADS spread reduction | The founder asking for it — one row field read where fire reads `SpreadAngle` |
| Zoom levels / scopes (sniper 2×…) | Scoped weapons existing |
| `IsADS_Upper` | Its reader identified in the editor |
| Sensitivity scaling while ADS | Settings existing |

## 7. Execution plan (C++ starts after the critic's findings land)

1. Tags `Input.Weapon.ADS`, `State.Weapon.ADS` · `ADSSpeedMultiplier` attribute + init modifier ·
   `UBNGE_ADS` (sprint's constructor, one attribute swapped)
2. `UBNGA_ADS` — LocalPredicted, held; sprint-block + row `bCanADS` check; descope listener;
   RecentDamage gate removed in the same commit
3. `FBNWeaponRow.bCanADS` · anim instance: tag listener + `GameplayTag_IsADS` publish + FOV interp
4. INPUT-WIRING ticket row 4 (already amended): `Input.Weapon.ADS` → `IA_FPST_Aim` → RMB
5. Editor questions for the terminal, LOOK-only: `IsADS_Upper`'s reader; confirm
   `GameplayTag_IsADS` survives the reparent un-renamed (the `_0` collision check)
