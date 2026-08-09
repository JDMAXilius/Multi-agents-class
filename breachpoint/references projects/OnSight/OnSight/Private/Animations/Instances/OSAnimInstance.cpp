#include "Animations/Instances/OSAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "KismetAnimationLibrary.h"

#include "Characters/OSCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Data/OSGameplayTags.h"
#include "Misc/DataValidation.h"

UOSAnimInstance::UOSAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// Binds the GameplayTagPropertyMap to ASC for event-driven Blueprint variable updates,
// then registers tag-change delegates for all C++ tag booleans.
void UOSAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	check(ASC);

	// Clean up any prior registration (handles re-init during respawn)
	UnregisterTagDelegates();

	GameplayTagPropertyMap.Initialize(this, ASC);
	RegisterTagDelegates(ASC);
}

void UOSAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Character = Cast<AOSCharacter>(TryGetPawnOwner());

	if (IsValid(Character))
	{
		MovementComponent = Character->GetCharacterMovement();
		PreviousWorldLocation = Character->GetActorLocation();
		PreviousYaw = Character->GetActorRotation().Yaw;

		// Normalize skeleton forward direction (prevents designer misconfiguration)
		CachedSkeletonForwardCS = SkeletonForwardCS.GetSafeNormal();
		if (CachedSkeletonForwardCS.IsNearlyZero())
		{
			CachedSkeletonForwardCS = FVector(0.f, -1.f, 0.f); // Mannequin default
		}

		// Cache bone indices for stance system
		if (const USkeletalMeshComponent* Mesh = GetSkelMeshComponent())
		{
			LeftFootBoneIndex = Mesh->GetBoneIndex(LeftFootBoneName);
			RightFootBoneIndex = Mesh->GetBoneIndex(RightFootBoneName);

			if (LeftFootBoneIndex == INDEX_NONE)
			{
				UE_LOG(LogAnimation, Warning, TEXT("%s: Left foot bone '%s' not found. Stance bone fallback disabled."),
					*GetName(), *LeftFootBoneName.ToString());
			}
			if (RightFootBoneIndex == INDEX_NONE)
			{
				UE_LOG(LogAnimation, Warning, TEXT("%s: Right foot bone '%s' not found. Stance bone fallback disabled."),
					*GetName(), *RightFootBoneName.ToString());
			}

			// Knee = parent of foot bone (for pole direction)
			if (LeftFootBoneIndex != INDEX_NONE)
			{
				const FName ParentName = Mesh->GetParentBone(LeftFootBoneName);
				LeftKneeBoneIndex = (ParentName != NAME_None) ? Mesh->GetBoneIndex(ParentName) : INDEX_NONE;
			}
			if (RightFootBoneIndex != INDEX_NONE)
			{
				const FName ParentName = Mesh->GetParentBone(RightFootBoneName);
				RightKneeBoneIndex = (ParentName != NAME_None) ? Mesh->GetBoneIndex(ParentName) : INDEX_NONE;
			}

			// Initialize previous foot locations (prevents first-frame speed spike)
			const TArray<FTransform>& CSTransforms = Mesh->GetComponentSpaceTransforms();
			if (CSTransforms.IsValidIndex(LeftFootBoneIndex))
				PreviousLeftFootLocationCS = CSTransforms[LeftFootBoneIndex].GetLocation();
			if (CSTransforms.IsValidIndex(RightFootBoneIndex))
				PreviousRightFootLocationCS = CSTransforms[RightFootBoneIndex].GetLocation();
		}

		// Prefer pawn interface; Lyra-style fallback if ASC not wired yet (replication ordering).
		UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
		if (!ASC)
		{
			ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
		}
		if (ASC)
		{
			InitializeWithAbilitySystem(ASC);
		}
	}
}

void UOSAnimInstance::NativeUninitializeAnimation()
{
	UnregisterTagDelegates();
	Super::NativeUninitializeAnimation();
}

#if WITH_EDITOR
EDataValidationResult UOSAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	GameplayTagPropertyMap.IsDataValid(this, Context);

	// Warn if foot bone names don't resolve on the skeleton.
	// Guard: GetOuter() may be a UPackage during editor asset validation (not a mesh component).
	// GetOwningComponent() and GetSkelMeshComponent() both use CastChecked which fatals on UPackage.
	if (const USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(GetOuter()))
	{
		if (Mesh->GetSkeletalMeshAsset())
		{
			const FReferenceSkeleton& RefSkel = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
			if (RefSkel.FindBoneIndex(LeftFootBoneName) == INDEX_NONE)
				Context.AddWarning(FText::FromString(FString::Printf(
					TEXT("Left foot bone '%s' not found on skeleton. Stance bone fallback disabled."),
					*LeftFootBoneName.ToString())));
			if (RefSkel.FindBoneIndex(RightFootBoneName) == INDEX_NONE)
				Context.AddWarning(FText::FromString(FString::Printf(
					TEXT("Right foot bone '%s' not found on skeleton. Stance bone fallback disabled."),
					*RightFootBoneName.ToString())));
		}
	}

	return ((Context.GetNumErrors() > 0) ? EDataValidationResult::Invalid : EDataValidationResult::Valid);
}
#endif // WITH_EDITOR

// Game thread: per-foot ground traces (physics scene access requires game thread).
void UOSAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	UpdateFootGroundTraces();
}

// Worker thread: movement + locomotion + stance + stride + lean.
void UOSAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	UpdateMovementData();
	UpdateStance(DeltaSeconds);
	UpdateLocomotionData(DeltaSeconds);
	UpdateStride();
	UpdateBodyLean(DeltaSeconds);
}

// ========================================
// MOVEMENT (worker thread — CMC reads are thread-safe)
// ========================================

void UOSAnimInstance::UpdateMovementData()
{
	if (!IsValid(Character) || !IsValid(MovementComponent))
	{
		return;
	}

	const FVector Vel = Character->GetVelocity();
	Speed = Vel.Size2D();
	Velocity = Vel;
	bIsInAir = MovementComponent->IsFalling();
	// Velocity threshold instead of raw acceleration — GetCurrentAcceleration
	// can retain residual values after root motion ends, causing foot shuffle.
	bIsAccelerating = Speed > 5.f && MovementComponent->GetCurrentAcceleration().SizeSquared2D() > 0.f;
	MovementRotation = Character->GetActorRotation();

	// --- Jump / Airborne ---
	FallSpeed = Vel.Z;
	if (bIsInAir && Vel.Z > 0.f)
	{
		// Rising: time to apex = Vz / gravity
		const float Gravity = FMath::Abs(MovementComponent->GetGravityZ());
		TimeToJumpApex = (Gravity > UE_SMALL_NUMBER) ? (Vel.Z / Gravity) : 0.f;
	}
	else
	{
		TimeToJumpApex = 0.f;
	}

	// Ground distance from feet (not capsule center) — for distance-matched landings.
	if (bIsInAir && Character->GetWorld())
	{
		const float HalfHeight = Character->GetSimpleCollisionHalfHeight();
		const FVector FeetLocation = Character->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
		const FVector End = FeetLocation - FVector(0.f, 0.f, MaxGroundTraceRange);
		FHitResult Hit;
		if (Character->GetWorld()->LineTraceSingleByChannel(Hit, FeetLocation, End, ECC_WorldStatic))
		{
			DistanceToGround = Hit.Distance;
		}
		else
		{
			DistanceToGround = MaxGroundTraceRange;
		}
	}
	else
	{
		DistanceToGround = 0.f;
	}
}

// ========================================
// FOOT GROUND TRACES (game thread — physics scene access)
// ========================================

void UOSAnimInstance::UpdateFootGroundTraces()
{
	// Skip when airborne, IK disabled, or stationary (reuse last result)
	if (bIsInAir || Stance.FootIKAlpha < 0.01f || !IsValid(Character))
	{
		bCachedLeftGroundValid = false;
		bCachedRightGroundValid = false;
		return;
	}
	if (Speed < 5.f && bCachedLeftGroundValid && bCachedRightGroundValid)
	{
		return; // Stationary on known ground — reuse cached traces
	}

	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Mesh) return;

	const FTransform& ComponentToWorld = Mesh->GetComponentTransform();
	const TArray<FTransform>& CSTransforms = Mesh->GetComponentSpaceTransforms();
	const FVector TraceDown = FVector(0.f, 0.f, -FootTraceLength);
	const ECollisionChannel Channel = FootTraceChannel.GetValue();
	UWorld* World = Character->GetWorld();
	if (!World) return;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FootGroundTrace), false, Character);
	FCollisionResponseParams ResponseParams;
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		// Match CMC collision/query setup (AdvancedGASSystem GASAnimInstance pattern).
		Move->InitCollisionParams(Params, ResponseParams);
	}

	// Left foot trace
	if (CSTransforms.IsValidIndex(LeftFootBoneIndex))
	{
		const FVector FootWS = ComponentToWorld.TransformPosition(CSTransforms[LeftFootBoneIndex].GetLocation());
		bCachedLeftGroundValid = World->LineTraceSingleByChannel(
			CachedLeftGroundHit, FootWS, FootWS + TraceDown, Channel, Params, ResponseParams);
	}

	// Right foot trace
	if (CSTransforms.IsValidIndex(RightFootBoneIndex))
	{
		const FVector FootWS = ComponentToWorld.TransformPosition(CSTransforms[RightFootBoneIndex].GetLocation());
		bCachedRightGroundValid = World->LineTraceSingleByChannel(
			CachedRightGroundHit, FootWS, FootWS + TraceDown, Channel, Params, ResponseParams);
	}
}

// ========================================
// STANCE (worker thread — bone reads + derived state)
// ========================================

void UOSAnimInstance::UpdateStance(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f) return;

	// --- Airborne early-out ---
	if (bIsInAir)
	{
		Stance.LeftFoot.PlantWeight = 0.f;
		Stance.RightFoot.PlantWeight = 0.f;
		Stance.StanceFoot = EOSStanceFoot::Neither;
		Stance.bLeftFootDown = false;
		Stance.bRightFootDown = false;
		Stance.FootIKAlpha = FMath::FInterpTo(Stance.FootIKAlpha, 0.f, DeltaSeconds, FootIKBlendSpeed);
		Stance.LeftFoot.IKAlpha = Stance.FootIKAlpha * Stance.LeftFoot.PlantWeight;
		Stance.RightFoot.IKAlpha = Stance.FootIKAlpha * Stance.RightFoot.PlantWeight;
		// Cache foot locations to prevent speed spike on landing
		const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
		if (Mesh)
		{
			const TArray<FTransform>& CS = Mesh->GetComponentSpaceTransforms();
			if (CS.IsValidIndex(LeftFootBoneIndex))
				PreviousLeftFootLocationCS = CS[LeftFootBoneIndex].GetLocation();
			if (CS.IsValidIndex(RightFootBoneIndex))
				PreviousRightFootLocationCS = CS[RightFootBoneIndex].GetLocation();
		}
		return;
	}

	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Mesh) return;

	const TArray<FTransform>& CSTransforms = Mesh->GetComponentSpaceTransforms();
	if (!CSTransforms.IsValidIndex(LeftFootBoneIndex) || !CSTransforms.IsValidIndex(RightFootBoneIndex))
		return;

	// Shared data for both feet
	static const FName FootLCurve(TEXT("Foot_L"));
	static const FName FootRCurve(TEXT("Foot_R"));
	static const FName SpeedLCurve(TEXT("Speed_L"));
	static const FName SpeedRCurve(TEXT("Speed_R"));

	const float LeftPlantCurve = GetCurveValue(FootLCurve);
	const float RightPlantCurve = GetCurveValue(FootRCurve);
	const bool bPlantCurvesActive = FMath::Abs(LeftPlantCurve) > UE_KINDA_SMALL_NUMBER
	                             || FMath::Abs(RightPlantCurve) > UE_KINDA_SMALL_NUMBER;

	const FVector RootLocationCS = CSTransforms[0].GetLocation();
	const FTransform& ComponentToWorld = Mesh->GetComponentTransform();

	// Per-foot computation (shared helper eliminates duplication)
	ComputeFootState(Stance.LeftFoot, LeftFootBoneIndex, LeftKneeBoneIndex,
		LeftPlantCurve, bPlantCurvesActive, GetCurveValue(SpeedLCurve),
		CachedLeftGroundHit, bCachedLeftGroundValid,
		PreviousLeftFootLocationCS, CSTransforms, RootLocationCS, ComponentToWorld, DeltaSeconds);

	ComputeFootState(Stance.RightFoot, RightFootBoneIndex, RightKneeBoneIndex,
		RightPlantCurve, bPlantCurvesActive, GetCurveValue(SpeedRCurve),
		CachedRightGroundHit, bCachedRightGroundValid,
		PreviousRightFootLocationCS, CSTransforms, RootLocationCS, ComponentToWorld, DeltaSeconds);

	ComputeDerivedStanceState(DeltaSeconds);
}

void UOSAnimInstance::ComputeFootState(
	FOSFootState& OutFoot, int32 FootBoneIndex, int32 KneeBoneIndex,
	float PlantCurve, bool bPlantCurvesActive, float SpeedCurve,
	const FHitResult& CachedGroundHit, bool bGroundHitValid,
	FVector& InOutPreviousLocationCS,
	const TArray<FTransform>& CSTransforms, const FVector& RootLocationCS,
	const FTransform& ComponentToWorld, float DeltaSeconds)
{
	const FTransform& FootTransform = CSTransforms[FootBoneIndex];
	const FVector FootLocationCS = FootTransform.GetLocation();

	// Position relative to root
	OutFoot.Height = FootLocationCS.Z - RootLocationCS.Z;
	OutFoot.ForwardOffset = FVector::DotProduct(FootLocationCS - RootLocationCS, CachedSkeletonForwardCS);
	OutFoot.BoneRotationCS = FootTransform.Rotator();

	// PlantWeight: curves preferred, bone height fallback
	OutFoot.PlantWeight = bPlantCurvesActive
		? FMath::Clamp(PlantCurve, 0.f, 1.f)
		: FMath::Clamp(1.f - (FMath::Abs(OutFoot.Height) / MaxFootLiftHeight), 0.f, 1.f);

	// Speed: curve preferred, bone delta fallback
	OutFoot.Speed = (FMath::Abs(SpeedCurve) > UE_KINDA_SMALL_NUMBER)
		? SpeedCurve
		: FVector::Dist(FootLocationCS, InOutPreviousLocationCS) / DeltaSeconds;

	// Speed-based plant boost: slow foot = more planted (height alone can miss contact phase)
	const float SpeedPlantFactor = FMath::Clamp(1.f - (OutFoot.Speed / FootPlantSpeedThreshold), 0.f, 1.f);
	OutFoot.PlantWeight = FMath::Max(OutFoot.PlantWeight, SpeedPlantFactor);

	// Ground data from cached traces (written by UpdateFootGroundTraces on game thread)
	if (bGroundHitValid)
	{
		OutFoot.GroundNormal = CachedGroundHit.ImpactNormal;
		OutFoot.GroundDistance = CachedGroundHit.Distance;
		OutFoot.IKTargetCS = ComponentToWorld.InverseTransformPosition(CachedGroundHit.ImpactPoint);
		const FVector FootUpCS = FootTransform.GetRotation().GetUpVector();
		const FVector GroundNormalCS = ComponentToWorld.InverseTransformVectorNoScale(OutFoot.GroundNormal);
		OutFoot.GroundAlignmentDelta = FQuat::FindBetweenNormals(FootUpCS, GroundNormalCS).Rotator();
	}
	else
	{
		OutFoot.GroundNormal = FVector::UpVector;
		OutFoot.GroundDistance = 0.f;
		OutFoot.IKTargetCS = FootLocationCS;
		OutFoot.GroundAlignmentDelta = FRotator::ZeroRotator;
	}

	// Pole direction: knee bone forward, adjusted for slope
	if (CSTransforms.IsValidIndex(KneeBoneIndex))
	{
		const FTransform& KneeTransform = CSTransforms[KneeBoneIndex];
		FVector KneeForward = KneeTransform.GetRotation().GetForwardVector();
		const FVector GroundNormalCS = ComponentToWorld.InverseTransformVectorNoScale(OutFoot.GroundNormal);
		KneeForward = FVector::VectorPlaneProject(KneeForward, GroundNormalCS).GetSafeNormal();
		if (!KneeForward.IsNearlyZero())
			OutFoot.PoleDirectionCS = KneeForward;
		OutFoot.PoleTargetCS = KneeTransform.GetLocation() + OutFoot.PoleDirectionCS * OutFoot.PoleOffset;
	}

	InOutPreviousLocationCS = FootLocationCS;
}

void UOSAnimInstance::ComputeDerivedStanceState(float DeltaSeconds)
{
	// Convenience booleans from plant weight
	Stance.bLeftFootDown = Stance.LeftFoot.PlantWeight > PlantWeightThreshold;
	Stance.bRightFootDown = Stance.RightFoot.PlantWeight > PlantWeightThreshold;

	// Stance foot enum
	if (Stance.bLeftFootDown && Stance.bRightFootDown)
		Stance.StanceFoot = EOSStanceFoot::Both;
	else if (Stance.bLeftFootDown)
		Stance.StanceFoot = EOSStanceFoot::Left;
	else if (Stance.bRightFootDown)
		Stance.StanceFoot = EOSStanceFoot::Right;
	else
		Stance.StanceFoot = EOSStanceFoot::Neither;

	// Forward foot: ForwardOffset-based with hysteresis. Only updates while accelerating.
	const float OffsetDelta = Stance.LeftFoot.ForwardOffset - Stance.RightFoot.ForwardOffset;
	if (bIsAccelerating && FMath::Abs(OffsetDelta) > ForwardFootHysteresis)
	{
		const bool bNewLeftForward = OffsetDelta > 0.f;
		if (bNewLeftForward != Stance.bLeftFootForward)
			Stance.bLeftFootForward = bNewLeftForward;
		LastMovingPlantDelta = OffsetDelta;
	}

	// Stop latch
	if (bWasAccelerating && !bIsAccelerating)
		Stance.bLeftFootForwardAtStop = LastMovingPlantDelta > 0.f;
	bWasAccelerating = bIsAccelerating;

	// Feet crossing: close ForwardOffsets + at least one foot in swing
	Stance.bFeetCrossing = FMath::Abs(OffsetDelta) < FootCrossingThreshold
		&& (Stance.LeftFoot.PlantWeight < PlantWeightThreshold
		 || Stance.RightFoot.PlantWeight < PlantWeightThreshold);

	// Stride phase: curve preferred, contact-event accumulation fallback
	static const FName StridePhaseCurve(TEXT("Stride_Phase"));
	const float StridePhaseCurveVal = GetCurveValue(StridePhaseCurve);
	if (FMath::Abs(StridePhaseCurveVal) > UE_KINDA_SMALL_NUMBER)
	{
		Stance.StridePhase = FMath::Clamp(StridePhaseCurveVal, 0.f, 1.f);
	}
	else if (bIsAccelerating)
	{
		// Contact-event accumulation: only while moving to avoid idle noise
		TimeSinceLastContact += DeltaSeconds;
		const bool bLeftContact = !bPreviousLeftFootDown && Stance.bLeftFootDown;
		const bool bRightContact = !bPreviousRightFootDown && Stance.bRightFootDown;
		if (bLeftContact || bRightContact)
		{
			// Adapt half-stride estimate from actual contact interval
			if (TimeSinceLastContact > 0.05f) // Ignore spurious rapid contacts
				DynamicHalfStrideTime = FMath::FInterpTo(DynamicHalfStrideTime, TimeSinceLastContact, 1.f, 5.f);
			LastContactPhase = bLeftContact ? 0.f : 0.5f;
			TimeSinceLastContact = 0.f;
		}
		const float EffectiveHalfStride = FMath::Max(DynamicHalfStrideTime, 0.05f);
		const float PhaseProgress = FMath::Clamp(TimeSinceLastContact / EffectiveHalfStride, 0.f, 1.f) * 0.5f;
		Stance.StridePhase = FMath::Fmod(LastContactPhase + PhaseProgress, 1.f);
	}

	bPreviousLeftFootDown = Stance.bLeftFootDown;
	bPreviousRightFootDown = Stance.bRightFootDown;

	// IK policy
	ComputeFootIKPolicy(DeltaSeconds);
}

// ========================================
// FOOT IK POLICY
// ========================================

void UOSAnimInstance::ComputeFootIKPolicy(float DeltaSeconds)
{
	// Note: Add GameplayTag_IsCharging here if charged attacks are re-enabled
	const bool bDisableFootIK = bIsInAir
		|| GameplayTag_IsAttacking
		|| GameplayTag_IsDodging
		|| GameplayTag_IsMantling
		|| GameplayTag_IsGrabbed
		|| GameplayTag_IsBeingGrabbed
		|| GameplayTag_IsGrabbing
		|| GameplayTag_IsDead
		|| GameplayTag_IsHitReacting
		|| GameplayTag_IsKnockedDown
		|| GameplayTag_IsStunned
		|| GameplayTag_IsRecoiling;

	const float TargetIKAlpha = bDisableFootIK ? 0.f : 1.f;
	Stance.FootIKAlpha = FMath::FInterpTo(Stance.FootIKAlpha, TargetIKAlpha, DeltaSeconds, FootIKBlendSpeed);
	// Per-foot: master alpha * plant weight. Planted foot gets full IK, swing foot gets none.
	// During disable states, FootIKAlpha → 0 kills both regardless of plant state.
	Stance.LeftFoot.IKAlpha = Stance.FootIKAlpha * Stance.LeftFoot.PlantWeight;
	Stance.RightFoot.IKAlpha = Stance.FootIKAlpha * Stance.RightFoot.PlantWeight;

	// Per-foot lock hints: during blocking, lock rear foot (planted stance)
	Stance.LeftFoot.LockHint = 0.f;
	Stance.RightFoot.LockHint = 0.f;
	if (GameplayTag_IsBlocking)
	{
		if (Stance.bLeftFootForward)
			Stance.RightFoot.LockHint = 1.f;
		else
			Stance.LeftFoot.LockHint = 1.f;
	}
}

// ========================================
// LOCOMOTION (worker thread — derived from cached movement data)
// ========================================

void UOSAnimInstance::UpdateLocomotionData(float DeltaSeconds)
{
	if (!IsValid(Character) || !IsValid(MovementComponent) || DeltaSeconds <= 0.f) return;

	// --- Locomotion Angle ---
	LocomotionAngle = UKismetAnimationLibrary::CalculateDirection(Velocity, MovementRotation);

	// --- Local Velocity (for stride warping direction) ---
	const FVector LocalVel = MovementRotation.UnrotateVector(Velocity);
	LocalVelocity2D = FVector2D(LocalVel.X, LocalVel.Y);

	// --- Cardinal Direction (with hysteresis) ---
	PreviousLocomotionDirection = LocomotionDirection;
	if (bIsAccelerating)
	{
		LocomotionDirection = CalculateLocomotionDirection(LocomotionAngle);
	}

	// --- Hip Facing (for strafing animation sets) ---
	if (PreviousLocomotionDirection != LocomotionDirection && bIsAccelerating)
	{
		HipFacingDirection = CalculateHipFacing(LocomotionDirection, PreviousLocomotionDirection);
	}

	// --- Yaw Delta (shared by root yaw offset + lean speed) ---
	const float CurrentYaw = MovementRotation.Yaw;
	const float ActorYawDelta = FMath::FindDeltaAngleDegrees(PreviousYaw, CurrentYaw);
	PreviousYaw = CurrentYaw;

	// --- Root Yaw Offset (turn in place) ---
	UpdateRootYawOffset(DeltaSeconds, ActorYawDelta);

	// --- Offset-adjusted angle (accounts for mesh rotation vs actor rotation) ---
	LocomotionAngleWithOffset = FRotator::NormalizeAxis(LocomotionAngle - RootYawOffset);

	// --- Yaw Delta Speed (for turning leans) ---
	YawDeltaSpeed = ActorYawDelta / DeltaSeconds;

	// --- Relative Acceleration (for accel leans) ---
	ComputeRelativeAcceleration(DeltaSeconds);

	// --- Location Delta (for distance-matched starts/stops) ---
	const FVector CurrentLocation = Character->GetActorLocation();
	LocationDelta = FVector::Dist2D(CurrentLocation, PreviousWorldLocation);
	AccumulatedDistance += LocationDelta;
	PreviousWorldLocation = CurrentLocation;

	// --- Gait (tags win over speed threshold) ---
	if (GameplayTag_IsSprinting)
	{
		Gait = EOSGait::Sprint;
	}
	else if (Speed <= WalkSpeedThreshold && Speed > 0.f)
	{
		Gait = EOSGait::Walk;
	}
	else
	{
		Gait = EOSGait::Jog;
	}
}

void UOSAnimInstance::UpdateRootYawOffset(float DeltaSeconds, float ActorYawDelta)
{
	switch (RootYawOffsetMode)
	{
	case EOSRootYawOffsetMode::Accumulate:
		// Counter-rotate mesh: as actor yaw increases, offset decreases to hold mesh facing.
		SetRootYawOffset(RootYawOffset - ActorYawDelta);
		break;

	case EOSRootYawOffsetMode::Hold:
		// Freeze — do nothing. Offset stays where it was when we entered this mode.
		break;

	case EOSRootYawOffsetMode::BlendOut:
	default:
		// Spring-damped blend toward zero. Critically damped (ratio 1.0) = no oscillation.
		{
			float SpringValue = RootYawOffset;
			FMath::SpringDamper(
				SpringValue, RootYawSpringVelocity,
				0.f, 0.f,    // Target value and target rate (both zero)
				DeltaSeconds,
				RootYawBlendOutSpringFrequency,
				1.f);         // DampingRatio 1.0 = critically damped
			SetRootYawOffset(FMath::IsNearlyZero(SpringValue, 0.1f) ? 0.f : SpringValue);
		}
		break;
	}
}

void UOSAnimInstance::SetRootYawOffset(float NewOffset)
{
	PreviousRootYawOffset = RootYawOffset;
	RootYawOffset = FRotator::NormalizeAxis(NewOffset);
}

void UOSAnimInstance::ComputeRelativeAcceleration(float DeltaSeconds)
{
	const FVector2D CurrentVelocity2D(Velocity.X, Velocity.Y);
	const FVector2D AccelVector = (CurrentVelocity2D - PreviousVelocity2D) / DeltaSeconds;
	PreviousVelocity2D = CurrentVelocity2D;

	if (!AccelVector.IsNearlyZero())
	{
		const FVector AccelWorld(AccelVector.X, AccelVector.Y, 0.f);
		const FVector AccelLocal = MovementRotation.UnrotateVector(AccelWorld);

		const float Divisor = bIsAccelerating
			? FMath::Max(MovementComponent->GetMaxAcceleration(), 1.f)
			: FMath::Max(MovementComponent->GetMaxBrakingDeceleration(), 1.f);

		RelativeAcceleration2D = FVector2D(
			FMath::Clamp(AccelLocal.X / Divisor, -1.f, 1.f),
			FMath::Clamp(AccelLocal.Y / Divisor, -1.f, 1.f)
		);
	}
	else
	{
		RelativeAcceleration2D = FVector2D::ZeroVector;
	}
}

EOSLocomotionDirection UOSAnimInstance::CalculateLocomotionDirection(float Angle) const
{
	const float AbsAngle = FMath::Abs(Angle);
	const float Dz = LocomotionSettings.DeadzoneDegrees;

	// Backward first (handles -180/+180 wraparound)
	{
		float Threshold = LocomotionSettings.BackwardMin;
		if (LocomotionDirection == EOSLocomotionDirection::Backward)
			Threshold -= Dz;
		if (AbsAngle >= Threshold)
			return EOSLocomotionDirection::Backward;
	}

	// Forward
	{
		float Threshold = LocomotionSettings.ForwardMax;
		if (LocomotionDirection == EOSLocomotionDirection::Forward)
			Threshold += Dz;
		if (AbsAngle <= Threshold)
			return EOSLocomotionDirection::Forward;
	}

	// Left (negative) vs Right (positive)
	return (Angle < 0.f) ? EOSLocomotionDirection::Left : EOSLocomotionDirection::Right;
}

EOSHipFacing UOSAnimInstance::CalculateHipFacing(
	EOSLocomotionDirection NewDir,
	EOSLocomotionDirection PrevDir) const
{
	// Forward → hips forward. Backward → hips backward.
	// Strafing inherits from previous direction.
	switch (NewDir)
	{
	case EOSLocomotionDirection::Forward:  return EOSHipFacing::Forward;
	case EOSLocomotionDirection::Backward: return EOSHipFacing::Backward;
	case EOSLocomotionDirection::Left:
	case EOSLocomotionDirection::Right:
		return (PrevDir == EOSLocomotionDirection::Backward)
			? EOSHipFacing::Backward : EOSHipFacing::Forward;
	}
	return EOSHipFacing::Forward;
}

// ========================================
// STRIDE WARPING (worker thread)
// ========================================

void UOSAnimInstance::UpdateStride()
{
	// StrideDirection: rotate forward vector by LocomotionAngle (component space).
	// StrideScale is computed by AnimNode_StrideWarping in Graph mode (reads root
	// motion speed from the animation attribute stream internally).
	const float AngleRad = FMath::DegreesToRadians(LocomotionAngle);
	StrideDirection = FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f);
}

// ========================================
// BODY LEAN (worker thread)
// ========================================

void UOSAnimInstance::UpdateBodyLean(float DeltaSeconds)
{
	const float TargetForward = RelativeAcceleration2D.X;
	const float TargetSide = RelativeAcceleration2D.Y;
	const float TargetTurn = FMath::Clamp(YawDeltaSpeed / MaxTurnLeanYawSpeed, -1.f, 1.f);

	BodyLean.ForwardLean = FMath::FInterpTo(BodyLean.ForwardLean, TargetForward, DeltaSeconds, LeanInterpSpeed);
	BodyLean.SideLean = FMath::FInterpTo(BodyLean.SideLean, TargetSide, DeltaSeconds, LeanInterpSpeed);
	BodyLean.TurnLean = FMath::FInterpTo(BodyLean.TurnLean, TargetTurn, DeltaSeconds, LeanInterpSpeed);
}

// ========================================
// TAG DELEGATE SYSTEM
// ========================================

// Builds tag→bool mapping, registers ASC delegates, seeds initial values.
void UOSAnimInstance::RegisterTagDelegates(UAbilitySystemComponent* ASC)
{
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	TagBoolMap.Empty();
	TagBoolMap.Reserve(29);

	// State tags
	TagBoolMap.Add(Tags.IsDead,          &GameplayTag_IsDead);
	TagBoolMap.Add(Tags.IsStunned,       &GameplayTag_IsStunned);
	TagBoolMap.Add(Tags.IsHitReacting,   &GameplayTag_IsHitReacting);
	TagBoolMap.Add(Tags.IsKnockedDown,   &GameplayTag_IsKnockedDown);
	TagBoolMap.Add(Tags.IsGrabbing,      &GameplayTag_IsGrabbing);
	TagBoolMap.Add(Tags.IsGrabbed,       &GameplayTag_IsGrabbed);
	TagBoolMap.Add(Tags.IsBeingGrabbed,  &GameplayTag_IsBeingGrabbed);
	TagBoolMap.Add(Tags.IsBlocking,      &GameplayTag_IsBlocking);
	TagBoolMap.Add(Tags.IsCharging,      &GameplayTag_IsCharging);
	TagBoolMap.Add(Tags.IsAttacking,     &GameplayTag_IsAttacking);
	TagBoolMap.Add(Tags.IsSprinting,     &GameplayTag_IsSprinting);
	TagBoolMap.Add(Tags.IsMantling,      &GameplayTag_IsMantling);
	TagBoolMap.Add(Tags.IsDodging,       &GameplayTag_IsDodging);
	TagBoolMap.Add(Tags.IsRecoiling,     &GameplayTag_IsRecoiling);
	TagBoolMap.Add(Tags.IsLockedOn,      &GameplayTag_IsLockedOn);
	TagBoolMap.Add(Tags.IsInAir,         &GameplayTag_IsInAir);
	// Locomotion substates: GEs grant Gameplay.State.Movement.* . AnimBPs / choosers may use the
	// shorthand State.Movement.Airborne | State.Movement.Grounded | State.Movement.Jumping — those
	// redirect to the same tags (DefaultGameplayTags.ini).
	TagBoolMap.Add(Tags.State_Movement_Airborne, &GameplayTag_State_Movement_Airborne);
	TagBoolMap.Add(Tags.State_Movement_Grounded, &GameplayTag_State_Movement_Grounded);
	TagBoolMap.Add(Tags.State_Movement_Jumping, &GameplayTag_State_Movement_Jumping);
	TagBoolMap.Add(Tags.Attack_Committed, &GameplayTag_AttackCommitted);

	// Attack phase tags
	TagBoolMap.Add(Tags.State_Attack_Windup,   &GameplayTag_State_Attack_Windup);
	TagBoolMap.Add(Tags.State_Attack_Active,   &GameplayTag_State_Attack_Active);
	TagBoolMap.Add(Tags.State_Attack_Recovery, &GameplayTag_State_Attack_Recovery);

	// Ability tags
	TagBoolMap.Add(Tags.Ability_Dodge, &GameplayTag_Ability_Dodge);
	TagBoolMap.Add(Tags.Ability_Jump,  &GameplayTag_Ability_Jump);

	// Attribute tags
	TagBoolMap.Add(Tags.IsStaminaBlocked, &GameplayTag_IsStaminaBlocked);

	// Event tags
	TagBoolMap.Add(Tags.Event_GuardBreak, &GameplayTag_Event_GuardBreak);

	// Cue tags
	TagBoolMap.Add(Tags.Cue_Flinch,          &GameplayTag_Cue_Flinch);
	TagBoolMap.Add(Tags.Cue_ClashKnockback,  &GameplayTag_Cue_ClashKnockback);

	// Register delegates and seed initial values
	TagDelegateHandles.Empty();
	TagDelegateHandles.Reserve(TagBoolMap.Num());

	for (auto& [Tag, BoolPtr] : TagBoolMap)
	{
		*BoolPtr = ASC->HasMatchingGameplayTag(Tag);

		FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UOSAnimInstance::OnGameplayTagChanged);
		TagDelegateHandles.Emplace(Tag, Handle);
	}

	CachedASC = ASC;
	RecalculateDerivedState();
}

void UOSAnimInstance::UnregisterTagDelegates()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		for (auto& [Tag, Handle] : TagDelegateHandles)
		{
			ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
				.Remove(Handle);
		}
	}

	TagDelegateHandles.Empty();
	TagBoolMap.Empty();
	CachedASC.Reset();
}

// Fires on game thread when any registered tag transitions through zero.
void UOSAnimInstance::OnGameplayTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (bool* BoolPtr = TagBoolMap.FindRef(Tag))
	{
		*BoolPtr = (NewCount > 0);
	}

	RecalculateDerivedState();
}

void UOSAnimInstance::RecalculateDerivedState()
{
	GameplayTag_IsGrounded = !GameplayTag_IsInAir;
	CombatState = EvaluateCombatState();
	AttackPhase = EvaluateAttackPhase();
	bIsInCombat = CombatState != EOSCombatState::Idle;
}

// ========================================
// DERIVED STATE (reads cached bools only — no ASC access)
// ========================================

EOSCombatState UOSAnimInstance::EvaluateCombatState() const
{
	if (GameplayTag_IsDead)        return EOSCombatState::Dead;
	if (GameplayTag_IsStunned)     return EOSCombatState::Stunned;
	if (GameplayTag_IsHitReacting) return EOSCombatState::HitReacting;
	if (GameplayTag_IsKnockedDown) return EOSCombatState::KnockedDown;
	if (GameplayTag_IsGrabbed || GameplayTag_IsBeingGrabbed) return EOSCombatState::Grabbed;
	if (GameplayTag_IsGrabbing)    return EOSCombatState::Grabbing;
	if (GameplayTag_IsRecoiling)   return EOSCombatState::Recoiling;
	if (GameplayTag_IsBlocking)    return EOSCombatState::Blocking;
	if (GameplayTag_IsCharging)    return EOSCombatState::Charging;
	if (GameplayTag_IsAttacking)   return EOSCombatState::Attacking;
	if (GameplayTag_IsMantling)    return EOSCombatState::Mantling;
	if (GameplayTag_IsDodging)     return EOSCombatState::Dodging;
	// Casting: no driving tag yet — will be added when magic/aura system is implemented
	return EOSCombatState::Idle;
}

EOSAttackPhase UOSAnimInstance::EvaluateAttackPhase() const
{
	if (GameplayTag_State_Attack_Active)   return EOSAttackPhase::Active;
	if (GameplayTag_State_Attack_Recovery) return EOSAttackPhase::Recovery;
	if (GameplayTag_State_Attack_Windup)   return EOSAttackPhase::Windup;
	return EOSAttackPhase::None;
}
