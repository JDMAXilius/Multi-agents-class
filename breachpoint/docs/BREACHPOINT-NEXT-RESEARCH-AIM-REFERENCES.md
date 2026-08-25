# RESEARCH — how six reference projects move AIM from controller to pose

**Cut:** 14 August 2026 by the cloud lead, from three parallel read-only sweeps over
`references projects/` (Lyra, ShooterCore, ZoransResistance, NewMoons, OnSight,
UE5_Multiplayer_FPS). All six trees are source-only — no `.uasset` readable — so every
AnimGraph hop is inferred from what the C++ exposes, and flagged as such.

**Why:** the founder asked to double-check BN's aiming architecture (character, PC, anim
instance) against how the references do it, mid-hunt on the frozen-arms bug.

## The consensus, stated first

Across six projects, three patterns hold with zero exceptions:

1. **Nobody pushes aim into the anim instance from C++ per frame.** The anim side PULLS —
   either the anim instance reads the pawn in its own update, or the ABP reads
   `BlueprintReadOnly` properties computed on the character. (ShooterCore pushes, but only
   event-driven enums — gait, overlay — and never aim.)
2. **Nobody writes into linked anim-layer instances from C++.** Ever. Zero hits for it in
   all six trees. Layers get their values by property access into the main ABP or by
   calling the same pawn getters the main ABP uses.
3. **The aim values live on the CHARACTER (or its getters), not the anim instance** — in
   both projects that actually have an aim system. The anim instance is either absent,
   nearly empty, or a pull-and-compute worker.

## Per project, one paragraph each

- **Lyra** — the gold standard has NO aim path in C++ at all. `ULyraAnimInstance` owns
  `GroundDistance` and a tag→bool map; that is the whole class. Aim offsets, spine bend,
  turn-in-place: all in the ABP asset, pulling `GetBaseAimRotation` via engine BP nodes.
  Layers are linked BP-side and read the main ABP via Property Access. The one custom
  replicated view rotation (`LyraPlayerState::ReplicatedViewRotation`) is a
  spectator/replay camera channel — a trap to mistake for the aim transport.
- **ShooterCore** — no C++ anim instance, no replication, aim never crosses C++. Pushes
  gait/locomotion enums via a BP interface, event-driven. Links layer classes in C++
  (`LinkAnimClassLayers`) but pushes nothing into them.
- **ZoransResistance** — the novel one: aim is a replicated world-space POINT
  (`FVector AimVector`, 10 Hz, Server-unreliable RPC up, `COND_SkipOwner` down). The anim
  graph pulls a rotator through a `BlueprintThreadSafe` getter ON THE CHARACTER
  (`GetRotationFromAimVector`: torso socket → aim point). The anim instance owns zero aim
  state. Needs no pitch normalization because every machine recomputes the rotator from a
  point. (Caveats found: the "thread-safe" getter calls two non-thread-safe engine
  functions, and AI never writes the point, so AI torsos aim at world origin.)
- **NewMoons** — no aim path; six event-driven bools; its `NativeThreadSafeUpdateAnimation`
  body is commented out.
- **OnSight** — third person, no aim offsets by design (orient-to-movement). But its
  `UOSAnimInstance` is the best thread-discipline reference in the set: game-thread update
  does ONLY traces, worker thread does everything else, `NormalizeAxis` where angles
  subtract, and **lean is computed entirely inside the anim instance** from acceleration
  and yaw speed, interp at 8. Its one aim helper (`GetAimOffsetYawPitch`) is the
  anti-pattern: `GetControlRotation` + PlayerController cast = zeros on every simulated
  proxy.
- **UE5_Multiplayer_FPS** — the closest FPS relative. Character computes `AO_Yaw` /
  turn-in-place in Tick into `BlueprintReadOnly` properties; the ABP pulls. Pitch goes
  through `GetFixedAimRotation()`: `GetBaseAimRotation()` + manual RemoteViewPitch
  decompression (270..360 → −90..0, remote-only) — the only fully correct proxy-pitch code
  in the whole reference set. **And the headline: its first-person arms follow the camera
  GEOMETRICALLY, not through animation** — a separate `Mesh1P` parented under the camera,
  which sits on a `bUsePawnControlRotation=true` spring arm. The whole aim-offset system
  exists only for the third-person body other players see.

## BN against the consensus

**Where BN already matches the strongest references:**

- **Pull, not push, at the main-ABP level.** `UBNAnimInstance` reads
  `GetBaseAimRotation()` off the pawn in its own update — Lyra's direction of flow,
  OnSight's thread split (snapshot on game thread, compute on worker).
- **Proxy pitch handling.** BN's `FRotator::NormalizeAxis(BaseAimPitch − ActorPitch)` is
  equivalent to the FPS project's manual remap, and strictly better than OnSight's broken
  helper. Correct as-is.
- **Lean computed inside the anim instance, interp ~8, worker thread** — exactly OnSight's
  lean system, independently arrived at. The 14 Aug fix (lean outside the ownership gate,
  native its only writer) is the reference-endorsed shape.
- **ADS as replicated state read by the anim side** — FPS project replicates `bAiming`
  `COND_SkipOwner`; BN's GE-applied `State.Weapon.ADS` tag is the same idea through GAS.

**Where BN diverges — and why, stated honestly:**

- **The reflection layer-push has no precedent in any reference.** No project writes into
  linked layer instances from C++. In Lyra, layers *read* the main ABP via Property Access
  and that simply works. BN's push exists because the FPSTemplate's layers were built for
  a third pattern none of the six references use either: **interface-event pushes from
  procedural components** — and on a BN pawn nothing fires those events. The push
  emulates the template's own messaging from native. Keep it, but know it is a
  compatibility shim for this template, not an architecture to grow.
- **The "components own the aim surface" configuration (current ABP default) is the
  template's oddity, not an industry pattern.** Six for six, the references put pose data
  where the anim graph can pull it and drive it from the pawn's own state. The
  recommendation that falls out: **native should be the default owner** — flip the ABP's
  `bNativeOwnsAimSurface` override back to true once `BNAimNative 1` proves the native
  path in PIE. The procedural components then keep what they are uniquely good at (sway,
  recoil, pose offsets) without owning the core aim chain.

## The option of last resort, recorded so it exists

If the template's full-body procedural aim keeps resisting: the UE5_Multiplayer_FPS
pattern — separate first-person arms mesh parented under the camera, aim offsets only for
the third-person body — makes "arms follow the camera" a scene-graph fact with ZERO
animation math, and is how most shipped FPS games do it. It abandons the template's
full-body first person (the founder's stated preference), so it is a founder decision,
not a lead's. Cost: a 1P/3P mesh split, montage routing per mesh, owner-see rules.
Benefit: the entire class of bug we have been hunting stops existing.

## Ruling

**14 Aug 2026, founder: "Do all."** The default-owner recommendation is executed:
[TASK-AIM-NATIVE-OWNER](archive/BREACHPOINT-NEXT-TASK-AIM-NATIVE-OWNER.md) flips the ABP default
back to native (one checkbox, terminal) and executes the shotgun/knife DT rows in the same
pass. The Mesh1P escape hatch stays PARKED — it was recorded as last resort and abandons the
full-body first person; it activates only on an explicit founder call after the native path
has had its fair test.

## What this changes right now

1. Nothing about the in-flight fixes — the layer push + `BNAimNative` A/B + probe are the
   right instruments and the research confirms the native path's shape is sound.
2. The default-owner recommendation above, pending the founder's PIE test.
3. `GetFixedAimRotation` (UE5_Multiplayer_FPS `ShooterCharacter.cpp:99-111`) is the one
   external code worth keeping open in a tab when reasoning about proxy pitch.
