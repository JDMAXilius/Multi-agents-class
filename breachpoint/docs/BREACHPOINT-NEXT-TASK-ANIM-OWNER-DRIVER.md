# TASK — the anim spine learns who is driving: player, bot, or nobody

> STATUS: in-progress — mac terminal 24 Aug 2026. FIRST FIX WAS WRONG AND IS SUPERSEDED; see
> the 'correction' section at the bottom, which is the one that matters.

**Cut:** 24 August 2026, from the founder's observation: *"player uses inputs for moving
animation calls but AI has no input actions therefore they do not call moving animations."*
That is exactly right, and it is provable from engine source rather than by argument.

## The bug, in one paragraph

`ABP_Mannequin_Base` (parent `/Script/BreachpointNext.BNLAnimInstance`) binds twelve fields.
Eleven derive from the movement component's velocity, floor and the ASC's tags, and are true for
any owner. The twelfth, `HasAcceleration`, is the gate Lyra's locomotion state machine uses to
leave **Idle** — and it was answered with `GetCurrentAcceleration()` alone.

`UCharacterMovementComponent::Acceleration` is fed **only** by `ConsumeInputVector`, i.e. only by
`AddMovementInput`, i.e. only by a player (`BNPlayerController.cpp:111-112`). A bot moves by
`MoveToActor`/`MoveToLocation` → `UPathFollowingComponent` → `RequestDirectMove` →
`CMC::RequestedVelocity`. `CalcVelocity` then applies that through a **local**
`RequestedAcceleration` and never assigns the member:

```cpp
// CharacterMovementComponent.cpp, CalcVelocity
FVector RequestedAcceleration = FVector::ZeroVector;          // local
if (ApplyRequestedMove(..., RequestedAcceleration, RequestedSpeed)) { ... }
Velocity += RequestedAcceleration * DeltaTime;                 // never touches `Acceleration`
```

So a pathing bot's `GetCurrentAcceleration()` is **exactly zero**, `HasAcceleration` is false,
and the bot travels at 600 uu/s in an Idle pose. That is the slide.

**It is an AUTHORITY-ONLY bug, and this is the part that would have wasted a session.** On a
client, every other pawn is a simulated proxy and the engine already covers them —
`UCharacterMovementComponent::UpdateProxyAcceleration()` synthesises
`Acceleration = Velocity.GetSafeNormal()` when acceleration is not replicated. So the fault
reproduces on the **listen server's own view and in standalone PIE**, and does **NOT** reproduce
on a remote client watching the same bot. Verifying this fix from a client window proves nothing.

## What landed

`Source/BreachpointNext/Animation/BNLAnimInstance.{h,cpp}` only. No asset, no Blueprint, no
change to the twelve-field binding contract the ABP already compiles against — every field kept
its name and type, so no reparent and no recompile of `ABP_Mannequin_Base` is required.

1. **`ResolveOwnerDriver()`** — publishes four `BlueprintReadOnly` bools so the graph can branch:
   `bIsPlayerControlled`, `bIsAIControlled`, `bLocallyControlled`, `bFPSMode`
   (`bFPSMode` matches `UBNAnimInstance`'s name so the two spines read alike).
   It asks the **controller**, not the pawn's class or a spawn flag — a pawn is a body and who
   holds it is a runtime fact. **It runs every frame, deliberately.** Possession is not stable
   for the life of an anim instance: meshes get an anim instance before they are possessed, and
   BN respawns characters mid-match. A driver resolved once at init is how a respawned bot ends
   up animating as though a human held the stick.
2. **`HasAcceleration` is now owner-aware**, via a static `ComputeHasAcceleration()`:
   `!InputAcceleration.IsNearlyZero() || (bAIControlled && !RequestedVelocity.IsNearlyZero())`.
   An OR rather than a branch, so a bot that both paths AND adds input (a strafe task) still
   reads true; it can never be *less* true than the read it replaced.
   Unpossessed resolves to **neither** player nor AI, so a corpse does not walk off a path
   request its dead owner left behind.
3. The ADS gate now reuses `bFPSMode` instead of recomputing
   `IsLocallyControlled() && IsPlayerControlled()` inline — one source for one fact.

## Verification

- **Rung 1: PARTIAL** (`BreachpointEditor` only; launcher install ships no server binaries, so
  `BreachpointServer` cannot link here — the script reports this correctly and it is not routed
  around). Clean relink of `libUnrealEditor-BreachpointNext.dylib`, 0 errors, 0 new warnings.
- **Rung 2: PASS — `BreachpointNext.Sim`, 30 tests started, 0 failures**, which is the 3
  pre-existing spec files plus the 7 new ones in `Tests/BNAnimOwnerDriverSpec.cpp`.
- **The spec was proved non-vacuous.** `ComputeHasAcceleration` was temporarily reverted to the
  old one-source read, rebuilt, and re-run: **exactly one test failed**, the one named
  *"MOVES ON PATH FOLLOWING ALONE — the slide bug"*. Then restored and re-run green. A spec that
  has never been seen to fail is not evidence.
- **Honesty rung: compiles + headless specs. NOT PIE'd, not multiplayer, not packaged.** Nobody
  has watched a bot walk yet. The visual claim needs solo PIE on the listen server (see the
  authority-only note above) and is the next session's first job.

## contract_gap — rung 2 is unrunnable through its own wrapper on macOS

`Tools/run-specs.sh:41` gates on `pgrep -f "UnrealEditor"`, which also matches
**`UnrealEditorServices`** — a macOS Finder/Services helper that auto-launches and is not an
editor. With that helper alive the script reports
*"An UnrealEditor process is already running"* and exits BLOCKED **even with no editor open**,
so rung 2 can never pass on this machine. `Tools/` is outside this packet's `owner_path`, so it
was NOT edited (law 5); the specs above were run by invoking the script's own `UnrealEditor-Cmd`
command line by hand. `Tools/run-ubt.sh` has the same loose pattern but only *warns*, so it is
cosmetic there. **Needs a one-line fix in a packet that owns `Tools/`:** match
`UnrealEditor(-Cmd)?$` or the project path, not the bare substring.

## Not fixed, recorded

- `UBNAnimInstance` (the dormant spine, `BNAnimInstance.cpp:412`) carries the **same** one-source
  `HasAcceleration` read. Nothing routes to it today, so it is latent, not live — but it will
  bite whoever revives it.
- The stale comment at `BNBotStateTreeTasks.cpp:~95` still says *"UBNAnimInstance drives the
  third-person locomotion"*. It is `UBNLAnimInstance` now. Outside this packet's owner_path.


---

## CORRECTION, same session — the first fix was wrong, and the founder caught it twice

**What I shipped first did nothing, and I reported it as done.** The founder said *"ai is just
sliding while moving"* and then *"no walking anim activated"*, and the telemetry agreed: sampling
the live instances gave `HasAcceleration true 0/4` on a bot whose `DisplacementSpeed` was over 10.
Two things were wrong with the original diagnosis.

**Wrong thing 1 — `LastUpdateRequestedVelocity` is also zero.** It is consumed and cleared inside
`PerformMovement` (`LastUpdateRequestedVelocity = bHasRequestedVelocity ? RequestedVelocity : Zero;
bHasRequestedVelocity = false;`), so by the time the anim update samples it, nothing is left. The
AI source I picked was as dead as the one I replaced.

**Wrong thing 2, and the real one — nothing downstream reads the C++ field.** The founder pointed
at the two AnimBPs directly. `ABP_ItemAnimLayersBase` keeps its OWN `HasAcceleration` /
`HasVelocity` / `DisplacementSpeed` and fills them from 53 PropertyAccess nodes, and the relevant
bindings, read straight out of the `.uasset`, are:

```
GetMovementComponent.GetCurrentAcceleration
GetMovementComponent.GetLastUpdateVelocity
```

It binds **the movement component**, not `UBNLAnimInstance`. So the linked layer — which is what
gates the walk cycle — could never have seen any value the C++ spine published. Fixing
`UBNLAnimInstance::HasAcceleration` was fixing a field the walk animation does not consult.

## The actual fix: repair the SOURCE, edit no asset

`ABNCharacter`'s constructor now sets, on the character movement component:

```cpp
if (FNavMovementProperties* NavProps = MoveComp->GetNavMovementProperties())
{
    NavProps->bUseAccelerationForPaths = true;
}
```

`UPathFollowingComponent::FollowPathSegment` branches on exactly this
(`NavMovementInterface->UseAccelerationForPathFollowing()`):

| flag | path following calls | populates |
|---|---|---|
| false (engine default) | `RequestDirectMove()` | `RequestedVelocity` only — `Acceleration` stays **zero** |
| **true** | `RequestPathMove()` → `AddInputVector()` | `Acceleration`, the same road a player's input takes |

So a bot now drives the CMC through the **same pipeline a player does**, and every consumer agrees
at once: `UBNLAnimInstance`, `ABP_Mannequin_Base`, and `ABP_ItemAnimLayersBase`'s own PropertyAccess
— **without touching either AnimBP**. That is why the fix belongs here and not in the graphs the
founder pointed at: those were the right place to LOOK, and the source was the right place to fix.

It is inert for players — a player-controlled pawn never runs path following.

**The velocity fallback added mid-session was REMOVED.** It worked, but with the source correct it
only bought an artifact: a bot carried by grenade knockback or a lift would have played a walk
cycle. `ComputeHasAcceleration` is back to the two-source OR, kept solely so that turning
`bUseAccelerationForPaths` off degrades to a stale pose rather than silently restoring the slide.

## Measured, before and after

Sampled off the live anim instances in solo PIE (`<actor>.CharacterMesh0.ABP_Mannequin_Base_C_0`),
counting only frames where `DisplacementSpeed > 10`:

| | HasAcceleration true while moving |
|---|---|
| before | **0 / 4** (one bot; every other pawn idle) |
| after | **12 / 13**, across four different bots (4/4, 3/3, 3/3, 2/3) |

The single `false` was a bot at 227 uu/s that was decelerating — zero acceleration there is
correct, not a miss.

- Rung 1: **PARTIAL** (`BreachpointEditor` clean relink; launcher install cannot link the server).
- Rung 2: **PASS, `BreachpointNext.Sim` 30/30.**
- **Honesty rung: the DATA is fixed and measured on the authority in solo PIE. The VISIBLE claim —
  that a walk cycle now plays — is NOT yet confirmed by anyone's eyes, including mine.** A focused
  viewport capture was attempted and missed the movement window. The founder reported the slide
  twice; the founder's screen is the test that closes this.
