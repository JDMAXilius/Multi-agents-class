#include "Animation/BNAnimInstance.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ReflectedTypeAccessors.h"

namespace
{
	// AnimEnum_CardinalDirection / AnimEnum_RootYawOffsetMode, by enumerator order.
	constexpr uint8 BN_CardinalForward = 0;
	constexpr uint8 BN_CardinalBackward = 1;
	constexpr uint8 BN_CardinalLeft = 2;
	constexpr uint8 BN_CardinalRight = 3;

	constexpr uint8 BN_YawModeBlendOut = 0;
	constexpr uint8 BN_YawModeAccumulate = 2;

	const FName BN_TurnYawWeightCurve(TEXT("TurnYawWeight"));
	const FName BN_RemainingTurnYawCurve(TEXT("RemainingTurnYaw"));

	// The source's SelectCardinalDirectionFromAngle: the held direction's dead zone doubles
	// so a jog along a 45-degree seam does not flicker between cardinals.
	uint8 SelectCardinalDirectionFromAngle(double Angle, double DeadZone, uint8 CurrentDirection, bool bUseCurrentDirection)
	{
		const double AbsAngle = FMath::Abs(Angle);
		double FwdDeadZone = DeadZone;
		double BwdDeadZone = DeadZone;
		if (bUseCurrentDirection)
		{
			if (CurrentDirection == BN_CardinalForward)
			{
				FwdDeadZone *= 2.0;
			}
			else if (CurrentDirection == BN_CardinalBackward)
			{
				BwdDeadZone *= 2.0;
			}
		}
		if (AbsAngle <= 45.0 + FwdDeadZone)
		{
			return BN_CardinalForward;
		}
		if (AbsAngle >= 135.0 - BwdDeadZone)
		{
			return BN_CardinalBackward;
		}
		return Angle > 0.0 ? BN_CardinalRight : BN_CardinalLeft;
	}

	uint8 GetOppositeCardinalDirection(uint8 In)
	{
		switch (In)
		{
		case BN_CardinalForward:  return BN_CardinalBackward;
		case BN_CardinalBackward: return BN_CardinalForward;
		case BN_CardinalLeft:     return BN_CardinalRight;
		default:                  return BN_CardinalLeft;
		}
	}
}

void UBNAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Character = Cast<ABNCharacter>(TryGetPawnOwner());
	MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;

	ResolveAbilitySystem();
}

void UBNAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!Character)
	{
		Character = Cast<ABNCharacter>(TryGetPawnOwner());
		MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	}
	if (!Character || !MovementComponent)
	{
		return;
	}

	if (!AbilitySystem)
	{
		ResolveAbilitySystem();
	}

	SnapWorldVelocity = MovementComponent->Velocity;
	SnapWorldAcceleration = MovementComponent->GetCurrentAcceleration();
	SnapWorldLocation = Character->GetActorLocation();
	SnapRotation = Character->GetActorRotation();
	SnapGravityZ = MovementComponent->GetGravityZ();
	// Replicated on every machine (RemoteViewPitch on proxies) — the source read it too.
	SnapBaseAimPitch = Character->GetBaseAimRotation().Pitch;
	SnapGroundDistance = ComputeGroundDistance();
	bSnapAnyMontagePlaying = IsAnyMontagePlaying();
	bSnapFalling = MovementComponent->IsFalling();
	bSnapInAirTag = bTagInAir;
	// Tag OR CMC, the IsFalling pattern: the owning client's CMC predicts bIsCrouched
	// instantly, so the pose doesn't wait ~½RTT for the replicated tag.
	bSnapCrouching = bTagCrouching || Character->bIsCrouched;
	bSnapJumping = bTagJumping;
	bSnapSprinting = bTagSprinting;
	// Locally controlled AND player: the listen host's own pawn and each client's own pawn
	// pose FPS; bots and every remotely-viewed character take the third-person branch.
	bSnapFPSMode = Character->IsLocallyControlled() && Character->IsPlayerControlled();
	bSnapLeanLeft = bTagLeanLeft;
	bSnapLeanRight = bTagLeanRight;
	bSnapADS = bTagADS;

	// ---- ADS camera, owner only. The lens is the local player's alone — proxies get their ADS
	// look from the pose. Game thread on purpose: this touches a UObject and is not graph-facing.
	if (Character->IsLocallyControlled() && Character->IsPlayerControlled())
	{
		if (UCameraComponent* Camera = Character->GetFirstPersonCamera())
		{
			if (DefaultFOV <= 0.f)
			{
				DefaultFOV = Camera->FieldOfView;
			}
			const float TargetFOV = bTagADS ? ADSFOV : DefaultFOV;
			Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaSeconds, ADSFOVInterpSpeed));
		}
	}
	// The weapon seam, not the local pawn: it answers off the character's own weapon state, so
	// server, owner and sim proxies all reach the same pose set for the character they render.
	bSnapUnarmed = Character->GetCurrentWeaponAnimLayer() == nullptr;

	// The character owns layer linking; this pass only watches the mesh for the swap edge.
	// No layer yet (e.g. a sim proxy whose anim instance didn't exist at BeginPlay): re-trigger
	// the character's InitializeAnimLayer — it guards and links once per class — then re-scan.
	UClass* CurrentLayerClass = nullptr;
	// const on purpose: the mutable GetLinkedAnimInstances() overload is private to
	// USkeletalMeshComponent - only the const one is public, and reading is all this wants.
	if (const USkeletalMeshComponent* MeshComp = GetOwningComponent())
	{
		for (int32 Pass = 0; Pass < 2 && !CurrentLayerClass; ++Pass)
		{
			for (UAnimInstance* Linked : MeshComp->GetLinkedAnimInstances())
			{
				if (Linked)
				{
					CurrentLayerClass = Linked->GetClass();
					break;
				}
			}
			if (!CurrentLayerClass && Pass == 0)
			{
				Character->InitializeAnimLayer();
			}
		}
	}
	bSnapLayerChanged = CurrentLayerClass != ObservedLayerClass;
	ObservedLayerClass = CurrentLayerClass;
}

void UBNAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// ABP_Mannequin_Base is SHARED: the template's own characters run this class too (E1/D3),
	// and their pawns are not ABNCharacter — the game-thread pass never fills a snapshot for
	// them. Without this guard the pass below writes every published variable from zeroed
	// snapshots each frame, stomping what the template's interface events wrote — which is
	// exactly what broke BP_FPSCharacter. Not a BN pawn: the C++ publisher stands down whole.
	if (!Character)
	{
		return;
	}

	const FVector WorldVelocity2D(SnapWorldVelocity.X, SnapWorldVelocity.Y, 0.0);
	const FVector WorldAcceleration2D(SnapWorldAcceleration.X, SnapWorldAcceleration.Y, 0.0);

	// ---- location
	DisplacementSinceLastUpdate = bNativeFirstUpdate ? 0.0 : FVector::Dist2D(SnapWorldLocation, PreviousWorldLocation);
	DisplacementSpeed = (!bNativeFirstUpdate && DeltaSeconds > 0.f) ? DisplacementSinceLastUpdate / DeltaSeconds : 0.0;
	PreviousWorldLocation = SnapWorldLocation;

	// ---- rotation
	NativeYawDeltaSinceLastUpdate = bNativeFirstUpdate ? 0.0 : FRotator::NormalizeAxis(SnapRotation.Yaw - PreviousYaw);
	NativeYawDeltaSpeed = DeltaSeconds > 0.f ? NativeYawDeltaSinceLastUpdate / DeltaSeconds : 0.0;
	PreviousYaw = SnapRotation.Yaw;
	// The source's lean factors; crouched leans less.
	AdditiveLeanAngle = NativeYawDeltaSpeed * (bSnapCrouching ? 0.025 : 0.0375);

	// ---- velocity
	const bool bWasMovingLastUpdate = bNativeWasMoving;
	LocalVelocity2D = SnapRotation.UnrotateVector(WorldVelocity2D);
	LocalVelocityDirectionAngle = CalculateDirection(WorldVelocity2D, SnapRotation);
	LocalVelocityDirectionAngleWithOffset = FRotator::NormalizeAxis(LocalVelocityDirectionAngle - NativeRootYawOffset);
	NativeLocalVelocityDirection = SelectCardinalDirectionFromAngle(
		LocalVelocityDirectionAngleWithOffset, CardinalDirectionDeadZone, NativeLocalVelocityDirection, bWasMovingLastUpdate);
	NativeLocalVelocityDirectionNoOffset = SelectCardinalDirectionFromAngle(
		LocalVelocityDirectionAngle, CardinalDirectionDeadZone, NativeLocalVelocityDirectionNoOffset, bWasMovingLastUpdate);
	HasVelocity = !FMath::IsNearlyZero(WorldVelocity2D.SizeSquared());
	bNativeWasMoving = HasVelocity;

	// ---- acceleration
	LocalAcceleration2D = SnapRotation.UnrotateVector(WorldAcceleration2D);
	HasAcceleration = !WorldAcceleration2D.IsNearlyZero();
	NativePivotDirection2D = FMath::Lerp(NativePivotDirection2D, WorldAcceleration2D.GetSafeNormal(), 0.5).GetSafeNormal();
	// The pivot cardinal is where the player came FROM: opposite of where acceleration points.
	NativeCardinalFromAcceleration = GetOppositeCardinalDirection(SelectCardinalDirectionFromAngle(
		CalculateDirection(NativePivotDirection2D, SnapRotation), CardinalDirectionDeadZone, NativeCardinalFromAcceleration, false));

	// ---- wall heuristic: pushing hard, barely moving, accel roughly perpendicular to travel.
	IsRunningIntoWall =
		LocalAcceleration2D.Size2D() > 0.1 &&
		LocalVelocity2D.Size2D() < 200.0 &&
		FMath::IsWithinInclusive(
			FVector::DotProduct(LocalAcceleration2D.GetSafeNormal(), LocalVelocity2D.GetSafeNormal()), -0.6, 0.6);

	// ---- character state (tag-driven; states stay tags)
	// Tag OR CMC: walking off a ledge activates no jump ability, so the InAir tag alone
	// would miss it — the movement component covers that half.
	IsFalling = bSnapInAirTag || bSnapFalling;
	IsOnGround = !IsFalling;
	isCrouching = bSnapCrouching;
	IsJumping = bSnapJumping;
	bUnarmed = bSnapUnarmed;
	bSprinting = bSnapSprinting;
	if (bNativeOwnsAimSurface)
	{
		bFPSMode = bSnapFPSMode;
	}
	GameplayTag_IsADS = bSnapADS;

	bNativeADSStateChange = !bNativeFirstUpdate && bSnapADS != bWasADSLastUpdate;
	bWasADSLastUpdate = bSnapADS;

	bNativeCrouchStateChange = !bNativeFirstUpdate && bSnapCrouching != bWasCrouchingLastUpdate;
	bWasCrouchingLastUpdate = bSnapCrouching;
	ApplyCrouchAlpha = isCrouching ? 1.0 : 0.0;

	GroundDistance = SnapGroundDistance;

	// ---- jump apex: derived, never a countdown — -Vz/g, zero once descending.
	TimeToJumpApex = (IsFalling && SnapWorldVelocity.Z > 0.0 && SnapGravityZ < 0.0)
		? -SnapWorldVelocity.Z / SnapGravityZ
		: 0.0;

	// ---- blend weights
	NativeUpperbodyAdditiveWeight = (bSnapAnyMontagePlaying && IsOnGround)
		? 1.0
		: FMath::FInterpTo(NativeUpperbodyAdditiveWeight, 0.0, static_cast<double>(DeltaSeconds), 6.0);

	// ---- root yaw offset / turn in place
	if (bNativeFirstUpdate)
	{
		NativeRootYawOffset = 0.0;
		NativeTurnYawCurveValue = 0.0;
		PreviousTurnYawCurveValue = 0.0;
	}
	// In the source the graph's state functions drive the mode; idle accumulates and
	// everything else blends out, which is their net effect.
	NativeRootYawOffsetMode = (HasVelocity || HasAcceleration || IsFalling) ? BN_YawModeBlendOut : BN_YawModeAccumulate;
	if (NativeRootYawOffsetMode == BN_YawModeAccumulate)
	{
		SetRootYawOffset(NativeRootYawOffset - NativeYawDeltaSinceLastUpdate);
	}
	else
	{
		SetRootYawOffset(UKismetMathLibrary::FloatSpringInterp(
			static_cast<float>(NativeRootYawOffset), 0.f, RootYawOffsetSpringState,
			/*Stiffness*/ 80.f, /*CriticalDampingFactor*/ 1.f, DeltaSeconds, /*Mass*/ 1.f));
	}
	ProcessTurnYawCurve();

	// ---- aim. The delta an aim offset wants: the view's pitch against the actor's, which the
	// CMC holds at zero. NormalizeAxis is load-bearing on a simulated proxy — GetBaseAimRotation
	// decompresses RemoteViewPitch into 0..360 there, so +-90 only survives normalized.
	// Nothing here reads locally-controlled-only state, which is why every machine agrees.
	if (bNativeOwnsAimSurface)
	{
		const double AimPitchDelta = FRotator::NormalizeAxis(SnapBaseAimPitch - SnapRotation.Pitch);
		AimPitch = AimPitchDelta;
		Pitch = FMath::Clamp(AimPitchDelta, AimPitchClamp.X, AimPitchClamp.Y);
		// The scalar above is what the asset's graph reads; THIS is what the aim layers read. The
		// layers bind to GetMainAnimBPThreadSafe.PitchRotator, so leaving it to a Blueprint hop meant
		// the whole spine chain applied a zero rotator — the arms and weapon held still while the
		// camera, which takes control rotation directly, moved. Writing it here closes that gap.
		PitchRotator = BNMakeAxisRotator(AimPitchAxis, Pitch);

		// ---- lean. The State.Lean.* tags are the input and they replicate (Mixed carries GE-granted
		// tags to simulated proxies), so a proxy leans the way its owner does. Both sides held cancel.
		const double TargetLeaning = (bSnapLeanRight ? 1.0 : 0.0) - (bSnapLeanLeft ? 1.0 : 0.0);
		NativeCurrLeaning = FMath::FInterpTo(NativeCurrLeaning, TargetLeaning, static_cast<double>(DeltaSeconds), LeanInterpSpeed);
		const double LeanDegrees = NativeCurrLeaning * LeanAngle;
		LeanRotation = BNMakeAxisRotator(LeanAxis, LeanDegrees);
		// The head's counter-tilt: the body tips, the gaze stays level. Same axis, opposite sign —
		// the asset's LeanSpineWeights head entry is literally named "Opposite Angle Weight".
		LeanOppRotation = BNMakeAxisRotator(LeanAxis, -LeanDegrees);
	}

	// ---- linked layer edge
	bNativeLinkedLayerChanged = !bNativeFirstUpdate && bSnapLayerChanged;

	// ---- gated publish: while the event graph is live it owns these RMW/edge outputs and
	// repeats the same math on the shared fields; publishing both would double-count.
	// Shared IsFirstUpdate is NEVER written from native — the graph keeps its own until cleared.
	// The enum-typed outputs (cardinals + RootYawOffsetMode) have no shared property at all:
	// they get UENUMs + a graph retype at clear-day, and publish from the private members then.
	if (bNativeOwnsTurnState)
	{
		RootYawOffset = NativeRootYawOffset;
		AimYaw = -NativeRootYawOffset;
		TurnYawCurveValue = NativeTurnYawCurveValue;
		YawDeltaSinceLastUpdate = NativeYawDeltaSinceLastUpdate;
		YawDeltaSpeed = NativeYawDeltaSpeed;
		CrouchStateChange = bNativeCrouchStateChange;
		ADSStateChanged = bNativeADSStateChange;
		LinkedLayerChanged = bNativeLinkedLayerChanged;
		PivotDirection2D = NativePivotDirection2D;
		UpperbodyDynamicAdditiveWeight = NativeUpperbodyAdditiveWeight;
	}

	bNativeFirstUpdate = false;
}

FString UBNAnimInstance::DescribeAimState() const
{
	// Every link in the aim chain on one line, in the order it flows. A zero anywhere names the
	// broken link: no BaseAim = the view is not reaching the pawn; Pitch fine but PitchRotator
	// zero = the publish; both fine and no motion = the ABP's binding or the bFPSMode gate.
	static const UEnum* AxisEnum = StaticEnum<EBNSpineAxis>();
	const auto AxisName = [](const UEnum* Enum, EBNSpineAxis Axis)
	{
		// Guarded: this is a debug string, and a debug string must never be the thing that crashes
		// the session it was added to diagnose.
		return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Axis)) : FString(TEXT("?"));
	};
	return FString::Printf(
		TEXT("BNAim | BaseAimPitch %.1f  ActorPitch %.1f  ->  AimPitch %.1f  Pitch %.1f  ")
		TEXT("PitchRotator (P %.1f Y %.1f R %.1f) axis %s | bFPSMode %s  bUnarmed %s | ")
		TEXT("Lean curr %.2f  LeanRotation (P %.1f Y %.1f R %.1f) axis %s | Layer %s"),
		SnapBaseAimPitch, SnapRotation.Pitch, AimPitch, Pitch,
		PitchRotator.Pitch, PitchRotator.Yaw, PitchRotator.Roll,
		*AxisName(AxisEnum, AimPitchAxis),
		bFPSMode ? TEXT("true") : TEXT("false"),
		bUnarmed ? TEXT("true") : TEXT("false"),
		NativeCurrLeaning,
		LeanRotation.Pitch, LeanRotation.Yaw, LeanRotation.Roll,
		*AxisName(AxisEnum, LeanAxis),
		*GetNameSafe(ObservedLayerClass));
}

void UBNAnimInstance::SetRootYawOffset(double InRootYawOffset)
{
	if (!bEnableRootYawOffset)
	{
		NativeRootYawOffset = 0.0;
		return;
	}

	const FVector2D Clamp = bSnapCrouching ? RootYawOffsetAngleClampCrouched : RootYawOffsetAngleClamp;
	const double Normalized = FRotator::NormalizeAxis(InRootYawOffset);
	NativeRootYawOffset = Clamp.X < Clamp.Y ? FMath::Clamp(Normalized, Clamp.X, Clamp.Y) : Normalized;
}

void UBNAnimInstance::ProcessTurnYawCurve()
{
	PreviousTurnYawCurveValue = NativeTurnYawCurveValue;

	const double TurnYawWeight = GetCurveValue(BN_TurnYawWeightCurve);
	if (FMath::IsNearlyZero(TurnYawWeight))
	{
		NativeTurnYawCurveValue = 0.0;
		PreviousTurnYawCurveValue = 0.0;
		return;
	}

	// The turn animation's RemainingTurnYaw curve is normalized by its weight; the offset
	// shrinks by exactly the yaw the animation consumed this frame.
	NativeTurnYawCurveValue = GetCurveValue(BN_RemainingTurnYawCurve) / TurnYawWeight;
	if (PreviousTurnYawCurveValue != 0.0)
	{
		SetRootYawOffset(NativeRootYawOffset - (NativeTurnYawCurveValue - PreviousTurnYawCurveValue));
	}
}

double UBNAnimInstance::ComputeGroundDistance() const
{
	if (!Character || !MovementComponent)
	{
		return 0.0;
	}
	if (MovementComponent->MovementMode == MOVE_Walking || MovementComponent->MovementMode == MOVE_NavWalking)
	{
		return 0.0;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	UWorld* World = Character->GetWorld();
	if (!Capsule || !World)
	{
		return 0.0;
	}

	constexpr double TraceLength = 100000.0;
	const double HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const FVector TraceStart = Character->GetActorLocation();
	const FVector TraceEnd = TraceStart - FVector(0.0, 0.0, TraceLength + HalfHeight);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BNAnimGroundDistance), false, Character);
	FHitResult Hit;
	World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, Capsule->GetCollisionObjectType(), QueryParams);

	return Hit.bBlockingHit ? FMath::Max(static_cast<double>(Hit.Distance) - HalfHeight, 0.0) : TraceLength;
}

void UBNAnimInstance::NativeUninitializeAnimation()
{
	if (AbilitySystem)
	{
		AbilitySystem->UnregisterGameplayTagEvent(CrouchTagHandle, BNTags::State_Movement_Crouching, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(JumpTagHandle, BNTags::State_Movement_Jumping, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(InAirTagHandle, BNTags::State_Movement_InAir, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(SprintTagHandle, BNTags::State_Movement_Sprinting, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(LeanLeftTagHandle, BNTags::State_Lean_Left, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(LeanRightTagHandle, BNTags::State_Lean_Right, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(ADSTagHandle, BNTags::State_Weapon_ADS, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem = nullptr;
	}
	CrouchTagHandle.Reset();
	JumpTagHandle.Reset();
	InAirTagHandle.Reset();
	SprintTagHandle.Reset();
	LeanLeftTagHandle.Reset();
	LeanRightTagHandle.Reset();
	ADSTagHandle.Reset();

	Super::NativeUninitializeAnimation();
}

void UBNAnimInstance::ResolveAbilitySystem()
{
	if (!Character)
	{
		return;
	}

	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	AbilitySystem = ASC;
	CrouchTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Movement_Crouching, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UBNAnimInstance::OnTagChanged);
	JumpTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Movement_Jumping, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UBNAnimInstance::OnTagChanged);
	InAirTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Movement_InAir, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UBNAnimInstance::OnTagChanged);
	SprintTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Movement_Sprinting, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UBNAnimInstance::OnTagChanged);
	LeanLeftTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Lean_Left, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UBNAnimInstance::OnTagChanged);
	LeanRightTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Lean_Right, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UBNAnimInstance::OnTagChanged);
	ADSTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Weapon_ADS, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UBNAnimInstance::OnTagChanged);

	bTagCrouching = ASC->HasMatchingGameplayTag(BNTags::State_Movement_Crouching);
	bTagJumping = ASC->HasMatchingGameplayTag(BNTags::State_Movement_Jumping);
	bTagInAir = ASC->HasMatchingGameplayTag(BNTags::State_Movement_InAir);
	bTagSprinting = ASC->HasMatchingGameplayTag(BNTags::State_Movement_Sprinting);
	bTagLeanLeft = ASC->HasMatchingGameplayTag(BNTags::State_Lean_Left);
	bTagLeanRight = ASC->HasMatchingGameplayTag(BNTags::State_Lean_Right);
	bTagADS = ASC->HasMatchingGameplayTag(BNTags::State_Weapon_ADS);
}

void UBNAnimInstance::OnTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bActive = NewCount > 0;
	if (Tag == BNTags::State_Movement_Crouching)
	{
		bTagCrouching = bActive;
	}
	else if (Tag == BNTags::State_Movement_Jumping)
	{
		bTagJumping = bActive;
	}
	else if (Tag == BNTags::State_Movement_InAir)
	{
		bTagInAir = bActive;
	}
	else if (Tag == BNTags::State_Movement_Sprinting)
	{
		bTagSprinting = bActive;
	}
	else if (Tag == BNTags::State_Lean_Left)
	{
		bTagLeanLeft = bActive;
	}
	else if (Tag == BNTags::State_Lean_Right)
	{
		bTagLeanRight = bActive;
	}
	else if (Tag == BNTags::State_Weapon_ADS)
	{
		bTagADS = bActive;
	}
}
