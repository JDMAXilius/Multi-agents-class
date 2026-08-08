#include "FPS/BRAnimInstance.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

#include "Core/BRGameplayTags.h"
#include "FPS/BRAnimLayerInterface.h"

namespace
{
	/**
	 * Cardinal selection with hysteresis.
	 *
	 * The dead zone is the entire point and it is not decoration. Quadrant boundaries sit at
	 * +-45 and +-135; a player strafing while turning sits exactly on one, and without
	 * hysteresis the cardinal flips every frame -- which reads as the legs shuddering. So the
	 * band belonging to the CURRENT cardinal is widened by the dead zone, and leaving it costs
	 * more than staying. The template shipped `cardinalDirectionDeadZone` = 10 for the same
	 * reason; it is config here.
	 */
	EBRAnimCardinal SelectCardinal(float AngleDegrees, EBRAnimCardinal Current, float DeadZone)
	{
		const float Angle = FRotator::NormalizeAxis(AngleDegrees);
		const float Abs = FMath::Abs(Angle);

		auto Widen = [DeadZone, Current](EBRAnimCardinal Candidate, float Bound)
		{
			return Current == Candidate ? Bound + DeadZone : Bound;
		};

		if (Abs <= Widen(EBRAnimCardinal::Forward, 45.f))
		{
			return EBRAnimCardinal::Forward;
		}
		if (Abs >= Widen(EBRAnimCardinal::Backward, 135.f))
		{
			return EBRAnimCardinal::Backward;
		}
		return Angle > 0.f ? EBRAnimCardinal::Right : EBRAnimCardinal::Left;
	}
}

UBRAnimInstance::UBRAnimInstance()
{
	// The spine's whole design is a worker-thread update (law 1). Opting in is what makes
	// NativeThreadSafeUpdateAnimation actually run off the game thread rather than silently
	// falling back to it -- a fallback that would look identical in PIE and cost frames in a match.
	bUseMultiThreadedAnimationUpdate = true;
}

void UBRAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	BindAbilitySystem();

	// Bound once, here, rather than per montage: these are the AnimInstance's own delegates, so
	// every montage this instance plays routes through them and no play site can forget to wire
	// the seam up.
	//
	// AddUNIQUEDynamic, not AddDynamic: `NativeInitializeAnimation` re-runs on any re-init (mesh
	// swap, forced InitAnim), and AddDynamic does not de-duplicate -- a second binding would
	// double every gameplay event this seam raises, silently, on the second init only.
	OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UBRAnimInstance::HandleMontageNotifyBegin);
	OnPlayMontageNotifyEnd.AddUniqueDynamic(this, &UBRAnimInstance::HandleMontageNotifyEnd);

	// The worker-thread guarantee this entire class is built on lives in a flag the ABP COMPILER
	// overwrites: `UAnimBlueprint` forces it false when the graph contains any non-thread-safe
	// node or a `BlueprintUpdateAnimation` event. If that happens, every line documented as
	// worker-thread silently runs on the game thread instead and nothing else notices. So: say so.
	// BOTH halves, because either one alone is a false pass. The ABP compiler copies its flag to
	// the CDO unconditionally, but the engine ALSO gates threaded updates on a project setting --
	// so with `bAllowMultiThreadedAnimationUpdate=False` in DefaultEngine.ini (an ordinary
	// profiling toggle) the CDO flag stays true, a check on it alone passes happily, and every
	// worker-pass line still runs on the game thread. That is the check asserting law 1 is met in
	// precisely the configuration where it is not.
	const bool bEngineAllows = GetDefault<UEngine>()->bAllowMultiThreadedAnimationUpdate;
	ensureMsgf(bEngineAllows && bUseMultiThreadedAnimationUpdate,
		TEXT("UBRAnimInstance: threaded anim update is OFF for %s (engine allows: %d, class: %d). "
			 "Either the project setting is disabled or the ABP graph has a non-thread-safe node "
			 "or a BlueprintUpdateAnimation event. NativeThreadSafeUpdateAnimation is running on "
			 "the GAME thread and animation.md law 1 is NOT being met."),
		*GetNameSafe(GetClass()), bEngineAllows ? 1 : 0, bUseMultiThreadedAnimationUpdate ? 1 : 0);
}

void UBRAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// GAME THREAD. Read UObjects here and nowhere else.

	// The ASC is NOT reliably available at NativeInitializeAnimation and that is a netcode
	// fact, not a timing quirk: BRCharacter forwards GetAbilitySystemComponent() to the
	// PlayerState, and on a client the PlayerState arrives by replication -- OnRep_PlayerState
	// can land many frames after the mesh has initialised. Binding once at init works on a
	// listen server and silently leaves every remote client's animation tag-blind. So it retries.
	if (!BoundASC.IsValid())
	{
		BindAbilitySystem();
	}

	const APawn* Pawn = TryGetPawnOwner();
	if (!Pawn)
	{
		Snapshot.bValid = false;
		return;
	}

	const FRotator NewRotation = Pawn->GetActorRotation();

	// Asked HERE because only the game pass can tell "new data arrived" from "nothing moved".
	// On a simulated proxy these two are indistinguishable to the worker: both look like a zero
	// delta, and one of them is a player mid-turn whose update has not landed yet.
	Snapshot.bRotationChanged = !NewRotation.Equals(Snapshot.WorldRotation, 0.01f);

	Snapshot.WorldLocation = Pawn->GetActorLocation();
	Snapshot.WorldRotation = NewRotation;
	Snapshot.WorldVelocity = Pawn->GetVelocity();
	Snapshot.BaseAimRotation = Pawn->GetBaseAimRotation();

	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Snapshot.bIsCrouched = Character->bIsCrouched;

		if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Snapshot.Acceleration = Movement->GetCurrentAcceleration();
			Snapshot.MaxSpeed = Movement->GetMaxSpeed();
			Snapshot.bIsFalling = Movement->IsFalling();
			Snapshot.bIsOnGround = Movement->IsMovingOnGround();

			// Never assume -980: a gravity scale makes it wrong, and the apex estimate is what
			// the jump's whole blend hangs off.
			const float Gravity = Movement->GetGravityZ();
			Snapshot.GravityZ = FMath::IsNearlyZero(Gravity) ? -980.f : Gravity;
		}
	}

	// The linked layer is a UObject question, so it is asked here and never in the worker pass.
	RefreshLinkedLayer();

	// Latch the ASC callback's copy. This is what makes `bADSStateChanged` safe to compute: the
	// worker compares and stores against ONE value that cannot change underneath it. Reading the
	// live callback field instead would let a tag flip between the compare and the store, losing
	// the edge permanently and leaving a state machine waiting on it stuck forever.
	Snapshot.Tags = TagState;

	Snapshot.bValid = true;
}

void UBRAnimInstance::RefreshLinkedLayer()
{
	FName Row;
	bool bOverridesHands = false;

	// `GetLinkedAnimLayerInstanceByClass` takes the LAYER class; asking by our own interface
	// keeps this free of any per-weapon class name. If no layer is linked the row is NAME_None,
	// which is a meaningful answer (unarmed) rather than a failure.
	if (const USkeletalMeshComponent* Mesh = GetOwningComponent())
	{
		for (UAnimInstance* Linked : Mesh->GetLinkedAnimInstances())
		{
			if (Linked && Linked->Implements<UBRAnimLayer>())
			{
				Row = IBRAnimLayer::Execute_GetLayerWeaponRow(Linked);
				bOverridesHands = IBRAnimLayer::Execute_GetOverridesHandPose(Linked);
				break;
			}
		}
	}

	Snapshot.LayerRow = Row;
	Snapshot.bLayerOverridesHandPose = bOverridesHands;
}

void UBRAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// WORKER THREAD. `Snapshot` and this object's own fields only. No UObject reads, no
	// allocation, no locks -- law 1. If a line below ever needs the pawn, it belongs in the
	// game-thread pass and the value belongs in the snapshot.

	if (!Snapshot.bValid)
	{
		return;
	}

	// Every integrator below sees a BOUNDED step. A level-load hitch hands this function a delta
	// of hundreds of milliseconds; semi-implicit Euler at that step overshoots enormously and the
	// weapon leaves the screen for a frame. Clamping is correct for presentation -- a spring that
	// resolves slightly slow through a hitch is invisible, one that explodes is not. Elapsed-time
	// accumulators deliberately keep the REAL delta: they measure wall clock, not motion.
	const float Step = FMath::Min(DeltaSeconds, MaxIntegrationStep);

	// ------------------------------------------------------------------ fire stamp
	if (bFirePending.exchange(false))
	{
		TimeSinceFired = 0.f;
	}
	// Saturate rather than accumulate forever. `9999` is the pack's own sentinel for "not
	// recently"; past it the value carries no information and float precision only degrades.
	TimeSinceFired = FMath::Min(TimeSinceFired + DeltaSeconds, BR_NeverFired);

	// ------------------------------------------------------------------ publish snapshot state
	// Every graph-read field is written HERE, on the worker thread, from the snapshot. The ASC
	// callback writes only its private copy, so no field the graph reads has two writers.
	bIsOnGround = Snapshot.bIsOnGround;
	bIsFalling = Snapshot.bIsFalling;
	bIsCrouched = Snapshot.bIsCrouched;

	bIsSprinting = Snapshot.Tags.bSprinting;
	bIsReloading = Snapshot.Tags.bReloading;
	bIsSwapping = Snapshot.Tags.bSwapping;
	bIsMeleeing = Snapshot.Tags.bMeleeing;
	bIsGrappling = Snapshot.Tags.bGrappling;
	bIsThrowingGrenade = Snapshot.Tags.bThrowingGrenade;
	bIsDead = Snapshot.Tags.bDead;
	bIsADS = Snapshot.Tags.bADS;
	bIsFiring = Snapshot.Tags.bFiring;

	LinkedLayerRow = Snapshot.LayerRow;
	bLayerOverridesHandPose = Snapshot.bLayerOverridesHandPose;

	const FVector WorldVelocity2D(Snapshot.WorldVelocity.X, Snapshot.WorldVelocity.Y, 0.f);
	const FVector WorldAccel2D(Snapshot.Acceleration.X, Snapshot.Acceleration.Y, 0.f);

	LocalVelocity2D = Snapshot.WorldRotation.UnrotateVector(WorldVelocity2D);
	LocalAcceleration2D = Snapshot.WorldRotation.UnrotateVector(WorldAccel2D);

	GroundSpeed = WorldVelocity2D.Size();
	bHasVelocity = !FMath::IsNearlyZero(GroundSpeed, 1.f);
	bHasAcceleration = !WorldAccel2D.IsNearlyZero(1.f);

	if (bHasVelocity)
	{
		LocalVelocityDirectionAngle =
			FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity2D.Y, LocalVelocity2D.X));
		VelocityCardinal = SelectCardinal(LocalVelocityDirectionAngle, VelocityCardinal, CardinalDeadZone);
	}
	// Standing still deliberately KEEPS the last cardinal. Snapping to Forward on stop makes the
	// stop animation play in the wrong direction, which is the artefact distance matching exists
	// to remove.

	if (bHasAcceleration)
	{
		const float AccelAngle =
			FMath::RadiansToDegrees(FMath::Atan2(LocalAcceleration2D.Y, LocalAcceleration2D.X));
		AccelerationCardinal = SelectCardinal(AccelAngle, AccelerationCardinal, CardinalDeadZone);
	}

	// ------------------------------------------------------------------ pivot
	// A pivot is acceleration OPPOSING velocity: the player asked for the other direction while
	// still travelling this one. Waiting for velocity to flip is too late -- the plant has been
	// missed and the turn skates.
	bIsPivoting = false;
	if (bHasVelocity && bHasAcceleration)
	{
		const float Opposition =
			FVector::DotProduct(WorldVelocity2D.GetSafeNormal(), WorldAccel2D.GetSafeNormal());
		if (Opposition <= PivotOpposingDot)
		{
			bIsPivoting = true;
			PivotDirection2D = LocalAcceleration2D.GetSafeNormal();
			TimeSincePivot = 0.f;
		}
	}
	TimeSincePivot = FMath::Min(TimeSincePivot + DeltaSeconds, BR_NeverFired);

	// ------------------------------------------------------------------ air
	VelocityZ = Snapshot.WorldVelocity.Z;

	// Falling AND rising is a jump; falling and descending is a drop. One "in air" bool would
	// make every walk-off a ledge look like a deliberate hop.
	bIsJumping = Snapshot.bIsFalling && VelocityZ > 0.f;
	TimeToJumpApex = bIsJumping ? -VelocityZ / Snapshot.GravityZ : 0.f;

	// ------------------------------------------------------------------ transition edges
	// True for exactly one update. A state machine transitioning on a LEVEL re-enters its entry
	// state for as long as the level holds; what it needs is the moment the level changed.
	//
	// Suppressed on the first pass, same family as the pitch and yaw guards: the "previous"
	// fields initialise to the ABSENT value, so a pawn that spawns already crouched -- or already
	// holding a weapon, so already layer-linked -- would emit a change edge for a change that
	// never happened, on the one frame the graph is deciding which state to start in.
	bCrouchStateChanged = bHasPublishedOnce && bIsCrouched != bWasCrouchedLastUpdate;
	bADSStateChanged = bHasPublishedOnce && bIsADS != bWasADSLastUpdate;
	bLinkedLayerChanged = bHasPublishedOnce && LinkedLayerRow != PreviousLayerRow;
	bHasPublishedOnce = true;

	bWasCrouchedLastUpdate = bIsCrouched;
	bWasADSLastUpdate = bIsADS;
	PreviousLayerRow = LinkedLayerRow;

	// ------------------------------------------------------------------ deltas
	// First frame has no previous, and a delta against a zeroed previous is a teleport: the
	// character would lean hard and the root would snap on the frame it spawns.
	if (bHasPreviousFrame && DeltaSeconds > 0.f)
	{
		DisplacementSinceLastUpdate = FVector::Dist2D(Snapshot.WorldLocation, PreviousLocation);
		DisplacementSpeed = DisplacementSinceLastUpdate / DeltaSeconds;

		YawDeltaSinceLastUpdate = FRotator::NormalizeAxis(Snapshot.WorldRotation.Yaw - PreviousYaw);
		YawDeltaSpeed = YawDeltaSinceLastUpdate / DeltaSeconds;
	}
	else
	{
		DisplacementSinceLastUpdate = 0.f;
		DisplacementSpeed = 0.f;
		YawDeltaSinceLastUpdate = 0.f;
		YawDeltaSpeed = 0.f;
	}

	PreviousLocation = Snapshot.WorldLocation;
	PreviousYaw = Snapshot.WorldRotation.Yaw;
	bHasPreviousFrame = true;

	// ------------------------------------------------------------------ aim offset
	const FRotator AimDelta =
		(Snapshot.BaseAimRotation - Snapshot.WorldRotation).GetNormalized();
	AimYaw = AimDelta.Yaw;
	AimPitch = AimDelta.Pitch;

	// ------------------------------------------------------------------ turn in place
	// Hold the root back against the turn while the feet are planted, then bleed it off. Moving
	// cancels it outright -- turn-in-place is by definition what happens when you are NOT moving,
	// and leaving an offset applied through a run is how a character ends up crabbing sideways.
	const float ClampMin = bIsCrouched ? RootYawOffsetMinCrouched : RootYawOffsetMin;
	const float ClampMax = bIsCrouched ? RootYawOffsetMaxCrouched : RootYawOffsetMax;
	const bool bSnapshotRotationChanged = Snapshot.bRotationChanged;

	if (bHasVelocity)
	{
		RootYawOffset = FMath::FInterpConstantTo(RootYawOffset, 0.f, DeltaSeconds, RootYawOffsetBleedSpeed);
	}
	else
	{
		RootYawOffset = FMath::Clamp(RootYawOffset - YawDeltaSinceLastUpdate, ClampMin, ClampMax);

		// STANDING RECOVERY, and it is a stand-in for something that does not exist yet.
		//
		// Nothing consumes this offset. In Lyra the turn-in-place ANIMATION consumes it through a
		// yaw curve, and there is no such curve, no ABP, and no packet that authors one. Without
		// a consumer the offset only ever grows while stationary: pan the camera 200 degrees
		// standing still and it pins to the clamp and STAYS there -- the body twisted 120 degrees
		// from the camera until the player happens to take a step.
		//
		// So: once the turn is over (the camera has stopped), walk it back. This is not
		// turn-in-place -- it is the floor that stops a missing system reading as a broken one.
		// The real fix is `contract_gap BP82-4`.
		//
		// GATED ON THE SNAPSHOT CHANGING, NOT ON A PER-FRAME DELTA, and the difference decides
		// whether remote players turn in place at all. `Snapshot.WorldRotation` comes from the
		// actor, and for a SIMULATED PROXY that is a step function refreshed at the net update
		// rate -- CMC smoothing smooths the mesh offset, not the actor rotation. So at 60 fps
		// against a 20 Hz update the per-frame yaw delta is exactly 0.0 on two frames in three,
		// and a "has the camera stopped?" test built on it answers YES two frames out of three
		// while the player is mid-turn. Recovery then eats ~60 deg/s of a 90 deg/s turn, and any
		// remote player turning slower than that never accumulates an offset at all -- turn in
		// place works for the owner and silently does not exist for everyone watching him.
		if (!bSnapshotRotationChanged)
		{
			RootYawOffset =
				FMath::FInterpConstantTo(RootYawOffset, 0.f, DeltaSeconds, RootYawOffsetIdleRecoverySpeed);
		}
	}

	// ------------------------------------------------------------------ lean
	LeanAngle = FMath::Clamp(YawDeltaSpeed * LeanScale, -LeanMaxAngle, LeanMaxAngle);

	// ------------------------------------------------------------------ sway
	// Springs chase the turn RATE on both axes, so the weapon lags the camera and settles
	// instead of being welded to it. Amendment A's "further than Lyra" lives here; what is
	// missing is only the custom NODE, not the computation.
	//
	// DEFECT FIXED HERE, and it is worth naming because it would have shipped looking fine.
	// The pitch spring was originally fed `AimPitch` -- an ANGLE -- while the yaw spring was fed
	// `YawDeltaSpeed`, a RATE. The local was even named `PitchRate` while holding an angle. The
	// symptom is not a wobble but a PERMANENT offset: hold the camera at 30 degrees up and the
	// spring settles to a constant tilt and stays there, because a constant angle is a constant
	// target. Sway is a response to MOTION; a still camera must produce zero sway on both axes.
	// Same first-frame guard the yaw path already had, and for the identical reason: a delta
	// against a zeroed previous is a teleport. Spawning while looking 30 degrees down would read
	// as ~1800 deg/s and slam the sway spring to its rail for a third of a second.
	//
	// Its OWN flag, not `bHasPreviousFrame`: that one is already set true by the deltas block
	// above, in this same pass, so reusing it here would guard nothing on the only frame that
	// needed guarding.
	const float PitchDelta =
		bHasPreviousAimPitch ? FRotator::NormalizeAxis(AimPitch - PreviousAimPitch) : 0.f;
	const float PitchRate = DeltaSeconds > 0.f ? PitchDelta / DeltaSeconds : 0.f;
	PreviousAimPitch = AimPitch;
	bHasPreviousAimPitch = true;

	SwayYawSpring.Step(FMath::Clamp(-YawDeltaSpeed * SwayYawScale, -SwayMaxAngle, SwayMaxAngle),
		SwayStiffness, SwayDamping, Step);

	SwayPitchSpring.Step(FMath::Clamp(-PitchRate * SwayPitchScale, -SwayMaxAngle, SwayMaxAngle),
		SwayStiffness, SwayDamping, Step);

	SwayRotation = FRotator(SwayPitchSpring.Value, SwayYawSpring.Value, 0.f);

	// ------------------------------------------------------------------ additive weights
	// Sway is suppressed while ADS: a scoped weapon that swims around the screen is unusable,
	// and this is the one place presentation defers to readability.
	ApplySwayAlpha = bIsADS ? 0.f : 1.f;
	ApplyCrouchAlpha = bIsCrouched ? 1.f : 0.f;

	// The upper body stops accepting additives while a montage owns it -- otherwise a reload
	// gets lean and sway layered on top of an animation that was authored complete.
	UpperBodyAdditiveWeight = (bIsReloading || bIsSwapping || bIsMeleeing) ? 0.f : 1.f;
}

void UBRAnimInstance::HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	ForwardNotifyAsGameplayEvent(NotifyName, /*bIsEnd=*/false);
}

void UBRAnimInstance::HandleMontageNotifyEnd(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	ForwardNotifyAsGameplayEvent(NotifyName, /*bIsEnd=*/true);
}

UAbilitySystemComponent* UBRAnimInstance::ResolveAbilitySystem() const
{
	const IAbilitySystemInterface* AbilityInterface = Cast<const IAbilitySystemInterface>(GetOwningActor());
	return AbilityInterface ? AbilityInterface->GetAbilitySystemComponent() : nullptr;
}

bool UBRAnimInstance::IsGameplayEventSource() const
{
	// FOLLOW GAS. DO NOT GUESS AT IT.
	//
	// This used to be `GetOwningComponent() == Character->GetMesh()`, on the reasoning that the
	// third-person mesh is the one that exists everywhere -- and that reasoning was fine while
	// being completely wrong about the thing that matters, because **nothing authored on a
	// montage decides which mesh it plays on.** The ability does, through GAS:
	//
	//   UAbilityTask_PlayMontageAndWait::Activate -> ActorInfo->GetAnimInstance()
	//   FGameplayAbilityActorInfo::GetAnimInstance -> SkeletalMeshComponent->GetAnimInstance()
	//   FGameplayAbilityActorInfo::InitFromActor   -> FindComponentByClass<USkeletalMeshComponent>()
	//
	// and `AActor::OwnedComponents` is a **TSet** -- hash order, not declaration order. A pawn
	// with two skeletal meshes gets whichever one the set hands over first. So the previous gate
	// silently dropped the ENTIRE law-4 seam whenever GAS happened to pick the 1P mesh: the
	// reload's commit notify fired on an instance the gate rejected, on every machine including
	// the server, and the ability's WaitGameplayEvent simply never fired. Ammo never moves.
	// Worse, which mesh you get can differ between PIE and a packaged build.
	//
	// So the gate asks GAS which mesh it is using and matches that. Whichever mesh plays the
	// montage is the one that speaks, exactly one instance per machine, and it stays correct
	// even if the resolution changes -- because it is no longer an assumption.
	const UAbilitySystemComponent* ASC = BoundASC.IsValid() ? BoundASC.Get() : ResolveAbilitySystem();
	if (!ASC || !ASC->AbilityActorInfo.IsValid())
	{
		return false;
	}

	return GetOwningComponent() == ASC->AbilityActorInfo->SkeletalMeshComponent.Get();
}

void UBRAnimInstance::ForwardNotifyAsGameplayEvent(FName NotifyName, bool bIsEnd)
{
	// The law-4 seam, and the ONLY thing this class does that reaches gameplay: it announces
	// that a moment arrived. It does not decide what the moment means.
	//
	// TWO MAPS, and the reason is a property of the engine rather than a style choice.
	// `AnimNotify_PlayMontageNotifyWindow` broadcasts the SAME `NotifyName` to the begin and the
	// end delegate. With one shared map a window's close re-emits its open tag, so
	// `Event.Melee.WindowEnd` is never sent by any name -- the trace window opens and is never
	// told to close. Windowed notifies are named for the WINDOW ("MeleeWindow"), and which edge
	// it is decides the tag.
	static const TMap<FName, FGameplayTag> BeginEvents = {
		{ FName("MeleeWindow"),  BRGameplayTags::Event_Melee_WindowBegin },
		{ FName("ReloadCommit"), BRGameplayTags::Event_Weapon_ReloadCommit },
		{ FName("SwapCommit"),   BRGameplayTags::Event_Weapon_SwapCommit },
	};

	// End-only. A point notify (`PlayMontageNotify`) fires begin and never end, so
	// `ReloadCommit` and `SwapCommit` correctly appear in neither this map nor twice.
	static const TMap<FName, FGameplayTag> EndEvents = {
		{ FName("MeleeWindow"), BRGameplayTags::Event_Melee_WindowEnd },
	};

	// MIS-AUTHORING DETECTOR, and it catches the direction that fails LOUD so the quiet one at
	// least has a companion. A seam name reaching the END delegate while living only in
	// `BeginEvents` means it was authored as a `…NotifyWindow` when it should be a point notify.
	// The reverse mistake -- `MeleeWindow` authored as a point notify -- cannot be detected here
	// at all: the End simply never arrives, and an absence has no callback. That one is
	// `contract_gap BP82-7`, because it silently restores the round-1 high (a trace window that
	// opens and is never closed) from one wrong dropdown selection in a `.uasset`.
	ensureMsgf(!(bIsEnd && !EndEvents.Contains(NotifyName) && BeginEvents.Contains(NotifyName)),
		TEXT("UBRAnimInstance: seam notify '%s' arrived on the END delegate but is a begin-only "
			 "event. It was authored as PlayMontageNotifyWindow and must be PlayMontageNotify."),
		*NotifyName.ToString());

	const FGameplayTag* EventTag = (bIsEnd ? EndEvents : BeginEvents).Find(NotifyName);
	if (!EventTag)
	{
		// An unmapped notify is not an error: a montage's other `PlayMontageNotify`s are
		// presentation cues that have no business raising a gameplay event.
		//
		// Worth knowing, because it is not obvious and nothing else says it: these delegates fire
		// ONLY for `AnimNotify_PlayMontageNotify` and `…NotifyWindow`. An ordinary anim notify --
		// a footstep, a shell eject, a custom notify class -- never reaches here at all. So a
		// gameplay-bearing notify MUST be authored as one of those two classes or the seam is
		// silent, and silence is indistinguishable from "the ability did not care".
		return;
	}

	SendSeamGameplayEvent(*EventTag);
}

void UBRAnimInstance::SendSeamGameplayEvent(FGameplayTag EventTag)
{
	if (!EventTag.IsValid())
	{
		return;
	}

	// One mesh speaks for the pawn. Both AnimInstances resolve to the SAME pawn and the same
	// ASC, so without this every event lands twice -- a doubling the per-machine netcode gate
	// below is structurally unable to see.
	if (!IsGameplayEventSource())
	{
		return;
	}

	APawn* Pawn = TryGetPawnOwner();
	if (!Pawn)
	{
		return;
	}

	// THE NETCODE GATE. Montages play on EVERY machine, simulated proxies included. Forwarding
	// unconditionally raises a reload-commit event on each observer's copy of a REMOTE player --
	// on a machine with no authority over that pawn. Abilities run on the authority and on the
	// predicting owner; nowhere else. This is invisible in PIE with one player and wrong the
	// moment there are two, which is exactly the class of bug law 7's rungs exist to catch.
	if (!Pawn->HasAuthority() && !Pawn->IsLocallyControlled())
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Pawn;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Pawn, EventTag, Payload);
}

void UBRAnimInstance::NativeUninitializeAnimation()
{
	UnbindAbilitySystem();

	OnPlayMontageNotifyBegin.RemoveDynamic(this, &UBRAnimInstance::HandleMontageNotifyBegin);
	OnPlayMontageNotifyEnd.RemoveDynamic(this, &UBRAnimInstance::HandleMontageNotifyEnd);

	Super::NativeUninitializeAnimation();
}

void UBRAnimInstance::NotifyWeaponFired()
{
	// Deliberately just a stamp. Nothing about ammo, cost, or whether the shot was legal --
	// that is the ability's business on the authority (law 4).
	bFirePending.store(true);
}

const TArray<UBRAnimInstance::FBRTagBinding>& UBRAnimInstance::GetTagBindings() const
{
	// The whole tag->bool wiring, in code, in one place, greppable and diffable. This is the
	// table `FGameplayTagBlueprintPropertyMap` would have buried in the ABP asset.
	//
	// R23: `State.Weapon.ADS` and `State.Weapon.Firing` are ABSENT from this table because they
	// are absent from `Core/BRGameplayTags.h`, and `Core/` is CLOSED. They are filed as
	// contract_gaps against BP93. When BP93 declares them, this table gains two lines and
	// nothing else in this class changes -- which is the test of whether the seam was drawn right.
	static const TArray<FBRTagBinding> Bindings = {
		{ BRGameplayTags::State_Movement_Sprinting,     &FBRAnimTagState::bSprinting },
		{ BRGameplayTags::State_Movement_Grappling,     &FBRAnimTagState::bGrappling },
		{ BRGameplayTags::State_Weapon_Reloading,       &FBRAnimTagState::bReloading },
		{ BRGameplayTags::State_Weapon_Swapping,        &FBRAnimTagState::bSwapping },
		{ BRGameplayTags::State_Combat_Meleeing,        &FBRAnimTagState::bMeleeing },
		{ BRGameplayTags::State_Combat_ThrowingGrenade, &FBRAnimTagState::bThrowingGrenade },
		{ BRGameplayTags::State_Dead,                   &FBRAnimTagState::bDead },
	};

	return Bindings;
}

void UBRAnimInstance::BindAbilitySystem()
{
	const AActor* OwnerActor = GetOwningActor();
	if (!OwnerActor)
	{
		return;
	}

	const IAbilitySystemInterface* AbilityInterface = Cast<const IAbilitySystemInterface>(OwnerActor);
	if (!AbilityInterface)
	{
		return;
	}

	UAbilitySystemComponent* ASC = AbilityInterface->GetAbilitySystemComponent();
	if (!ASC || BoundASC.Get() == ASC)
	{
		return;
	}

	// Respawn re-points the ASC, so drop the old registration before taking a new one. NOT for
	// safety -- `AddUObject` binds weakly and a destroyed instance is skipped, never written --
	// but for handle hygiene: leaving stale entries on the old ASC's delegate list means this
	// instance keeps receiving a dead pawn's tag changes for as long as that ASC lives.
	UnbindAbilitySystem();

	for (const FBRTagBinding& Binding : GetTagBindings())
	{
		TagHandles.Add(
			ASC->RegisterGameplayTagEvent(Binding.Tag, EGameplayTagEventType::NewOrRemoved)
			   .AddUObject(this, &UBRAnimInstance::OnStateTagChanged));

		// Seed from the CURRENT count. A callback only fires on CHANGE, so binding to an ASC
		// that is already sprinting leaves the bool false until the player stops -- state that
		// is wrong exactly when it is least visible in testing and most visible in a match.
		TagState.*(Binding.Field) = ASC->HasMatchingGameplayTag(Binding.Tag);
	}

	BoundASC = ASC;
}

void UBRAnimInstance::UnbindAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		const TArray<FBRTagBinding>& Bindings = GetTagBindings();
		for (int32 Index = 0; Index < TagHandles.Num() && Index < Bindings.Num(); ++Index)
		{
			ASC->RegisterGameplayTagEvent(Bindings[Index].Tag, EGameplayTagEventType::NewOrRemoved)
			   .Remove(TagHandles[Index]);
		}
	}

	TagHandles.Reset();
	BoundASC.Reset();

	// CLEAR THE STATE, not just the handles. Without this, an observing client watching a player
	// who DISCONNECTS mid-reload keeps `bReloading` true forever: the PlayerState dies, the
	// rebind retry returns early at the null-ASC check, and nothing ever writes the flag false
	// again. The abandoned pawn reloads for eternity with its upper body additive pinned to zero.
	// Respawn was always clean because a rebind reseeds all seven; ASC LOSS was not.
	TagState = FBRAnimTagState{};
}

void UBRAnimInstance::OnStateTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Linear over seven entries, on a tag change rather than per frame. A map here would be a
	// hash of a struct to save six comparisons that happen when a player starts sprinting.
	for (const FBRTagBinding& Binding : GetTagBindings())
	{
		if (Binding.Tag == Tag)
		{
			// Writes the PRIVATE game-thread copy. The worker publishes it into the graph-read
			// bool next pass, so nothing the graph reads has two writers.
			TagState.*(Binding.Field) = NewCount > 0;
			return;
		}
	}
}
