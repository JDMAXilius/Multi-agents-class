# TASK — the anim spine learns who is driving: player, bot, or nobody

> STATUS: done — mac terminal 24 Aug 2026. Rung 1 PARTIAL + rung 2 PASS (30/30). Not yet PIE'd.

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
