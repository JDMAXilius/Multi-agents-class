#include "Animation/BNAnimInstance.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "BoneIndices.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "UObject/UnrealType.h"

namespace
{
	const FName BN_TurnYawWeightCurve(TEXT("TurnYawWeight"));
	const FName BN_RemainingTurnYawCurve(TEXT("RemainingTurnYaw"));

	/** The source's SelectCardinalDirectionFromAngle: the held direction's dead zone doubles so a
	 *  jog along a 45-degree seam does not flicker between cardinals. */
	EBNCardinal SelectCardinalFromAngle(double Angle, double DeadZone, EBNCardinal Current, bool bUseCurrent)
	{
		double FwdDeadZone = DeadZone;
		double BwdDeadZone = DeadZone;
		if (bUseCurrent)
		{
			if (Current == EBNCardinal::Forward)
			{
				FwdDeadZone *= 2.0;
			}
			else if (Current == EBNCardinal::Backward)
			{
				BwdDeadZone *= 2.0;
			}
		}

		const double AbsAngle = FMath::Abs(Angle);
		if (AbsAngle <= 45.0 + FwdDeadZone)
		{
			return EBNCardinal::Forward;
		}
		if (AbsAngle >= 135.0 - BwdDeadZone)
		{
			return EBNCardinal::Backward;
		}
		return Angle > 0.0 ? EBNCardinal::Right : EBNCardinal::Left;
	}

	EBNCardinal OppositeCardinal(EBNCardinal In)
	{
		switch (In)
		{
		case EBNCardinal::Forward:  return EBNCardinal::Backward;
		case EBNCardinal::Backward: return EBNCardinal::Forward;
		case EBNCardinal::Left:     return EBNCardinal::Right;
		default:                    return EBNCardinal::Left;
		}
	}

	const FStructProperty* FindRotatorProperty(const UClass* Cls, const TCHAR* Name)
	{
		const FStructProperty* Prop = FindFProperty<FStructProperty>(Cls, FName(Name));
		return (Prop && Prop->Struct == TBaseStructure<FRotator>::Get()) ? Prop : nullptr;
	}

	/** A BP "float" is double-width in UE5, but an absorbed or legacy property can still be a true
	 *  float — FNumericProperty covers both. It also covers the integer types, which would assert
	 *  on a floating-point write, so the kind is checked rather than assumed. */
	const FNumericProperty* FindFloatingPointProperty(const UClass* Cls, const TCHAR* Name)
	{
		const FNumericProperty* Prop = FindFProperty<FNumericProperty>(Cls, FName(Name));
		return (Prop && Prop->IsFloatingPoint()) ? Prop : nullptr;
	}

	/** Measured on ABP_Mannequin_Base's AimSpineWeights_UE5. Each bone takes a fraction so the
	 *  accumulated chain equals the look angle — the same distribution the template's
	 *  Transform(Modify)Bone alphas use. */
	struct FBNAimBone
	{
		const TCHAR* Name;
		float Weight;
	};

	const FBNAimBone BNAimBones[] = {
		{ TEXT("spine_01"), 0.15f },
		{ TEXT("spine_02"), 0.10f },
		{ TEXT("spine_03"), 0.10f },
		{ TEXT("spine_04"), 0.10f },
		{ TEXT("spine_05"), 0.10f },
		{ TEXT("neck_01"),  0.15f },
		{ TEXT("neck_02"),  0.20f },
		{ TEXT("head"),     0.10f },
	};
}

struct FBNAnimInstanceProxy : public FAnimInstanceProxy
{
	FBNAnimInstanceProxy() = default;
	explicit FBNAnimInstanceProxy(UAnimInstance* Instance)
		: FAnimInstanceProxy(Instance)
	{
	}

	virtual void PreEvaluateAnimation(UAnimInstance* InAnimInstance) override
	{
		FAnimInstanceProxy::PreEvaluateAnimation(InAnimInstance);

		const UBNAnimInstance* Anim = Cast<UBNAnimInstance>(InAnimInstance);
		if (!Anim)
		{
			bApplyAim = false;
			return;
		}

		// Copied after NativeThreadSafeUpdateAnimation has published this frame's aim. Evaluate
		// must not touch the UObject; these members are the only values the pose apply reads.
		AimPitch = Anim->AimPitch;
		AimYaw = Anim->AimYaw;
		AimPitchAxis = Anim->AimPitchAxis;
		AimYawAxis = Anim->AimYawAxis;
		bApplyAim = Anim->Character != nullptr && !Anim->bTagDead;
	}

	virtual bool Evaluate_WithRoot(FPoseContext& Output, FAnimNode_Base* InRootNode) override
	{
		EvaluateAnimationNode_WithRoot(Output, InRootNode);
		if (bApplyAim && InRootNode && InRootNode == GetRootNode())
		{
			ApplyAimToPose(Output);
		}
		return true;
	}

private:
	void ApplyAimToPose(FPoseContext& Output) const
	{
		if (FMath::IsNearlyZero(AimPitch) && FMath::IsNearlyZero(AimYaw))
		{
			return;
		}

		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		for (const FBNAimBone& Entry : BNAimBones)
		{
			const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(FName(Entry.Name));
			if (MeshIndex == INDEX_NONE)
			{
				continue;
			}

			const FCompactPoseBoneIndex Compact = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
			if (!Compact.IsValid())
			{
				continue;
			}

			const FQuat PitchDelta(BNMakeAxisRotator(AimPitchAxis, AimPitch * static_cast<double>(Entry.Weight)));
			const FQuat YawDelta(BNMakeAxisRotator(AimYawAxis, AimYaw * static_cast<double>(Entry.Weight)));
			FTransform& Local = Output.Pose[Compact];
			Local.SetRotation((PitchDelta * YawDelta * Local.GetRotation()).GetNormalized());
		}
	}

	double AimPitch = 0.0;
	double AimYaw = 0.0;
	EBNSpineAxis AimPitchAxis = EBNSpineAxis::Roll;
	EBNSpineAxis AimYawAxis = EBNSpineAxis::Yaw;
	bool bApplyAim = false;
};

void FBNLinkedLayer::Resolve(UAnimInstance& Layer)
{
	UClass* Cls = Layer.GetClass();
	Instance = &Layer;
	ResolvedClass = Cls;

	FPSMode = FindFProperty<FBoolProperty>(Cls, FName(TEXT("bFPSMode")));
	IsADS = FindFProperty<FBoolProperty>(Cls, FName(TEXT("GameplayTag_IsADS")));
	IsADSUpper = FindFProperty<FBoolProperty>(Cls, FName(TEXT("IsADS_Upper")));
	Pitch = FindFloatingPointProperty(Cls, TEXT("Pitch"));
	AimPitch = FindFloatingPointProperty(Cls, TEXT("AimPitch"));
	AimYaw = FindFloatingPointProperty(Cls, TEXT("AimYaw"));
	PitchRotator = FindRotatorProperty(Cls, TEXT("PitchRotator"));
	YawRotator = FindRotatorProperty(Cls, TEXT("YawRotator"));
	LeanRotation = FindRotatorProperty(Cls, TEXT("LeanRotation"));
	LeanOppRotation = FindRotatorProperty(Cls, TEXT("LeanOppRotation"));
}

void UBNAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	SelfIsADSUpper = FindFProperty<FBoolProperty>(GetClass(), FName(TEXT("IsADS_Upper")));

	ResolveOwner();
	ResolveAbilitySystem();
}

void UBNAnimInstance::ResolveOwner()
{
	Character = Cast<ABNCharacter>(TryGetPawnOwner());
	MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	Camera = Character ? Character->GetFirstPersonCamera() : nullptr;
}

void UBNAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!Character)
	{
		ResolveOwner();
	}
	if (!Character || !MovementComponent)
	{
		return;
	}
	if (!AbilitySystem)
	{
		ResolveAbilitySystem();
	}

	Snapshot.WorldVelocity = MovementComponent->Velocity;
	Snapshot.WorldAcceleration = MovementComponent->GetCurrentAcceleration();
	Snapshot.WorldLocation = Character->GetActorLocation();
	Snapshot.ActorRotation = Character->GetActorRotation();
	Snapshot.GravityZ = MovementComponent->GetGravityZ();
	// Replicated on every machine — RemoteViewPitch on proxies. One getter, already normalized.
	const FRotator AimRotation = Character->GetAimRotation();
	Snapshot.BaseAimPitch = AimRotation.Pitch;
	Snapshot.BaseAimYaw = AimRotation.Yaw;
	Snapshot.GroundDistance = ComputeGroundDistance();
	Snapshot.bAnyMontagePlaying = IsAnyMontagePlaying();
	Snapshot.bFalling = MovementComponent->IsFalling();
	Snapshot.bInAirTag = bTagInAir;
	// Tag OR CMC: the owning client's movement component predicts bIsCrouched instantly, so the
	// pose does not wait half a round trip for the replicated tag.
	Snapshot.bCrouching = bTagCrouching || Character->bIsCrouched;
	Snapshot.bJumping = bTagJumping;
	Snapshot.bSprinting = bTagSprinting;
	Snapshot.bLeanLeft = bTagLeanLeft;
	Snapshot.bLeanRight = bTagLeanRight;
	Snapshot.bADS = bTagADS;
	Snapshot.bDead = bTagDead;
	// Locally controlled AND player: the listen host's own pawn and each client's own pawn pose
	// first person; bots and every remotely-viewed character take the third-person branch.
	Snapshot.bLocallyControlled = Character->IsLocallyControlled();
	Snapshot.bFPSMode = Snapshot.bLocallyControlled && Character->IsPlayerControlled();
	// Answers off the character's own weapon state, so server, owner and proxies all reach the
	// same pose set for the character they render.
	Snapshot.bUnarmed = Character->GetCurrentWeaponAnimLayer() == nullptr;

	RefreshLinkedLayers();
	PushAimSurfaceToLinkedLayers();
	UpdateADSFieldOfView(DeltaSeconds);
}

void UBNAnimInstance::RefreshLinkedLayers()
{
	Snapshot.bLayerChanged = false;

	const USkeletalMeshComponent* MeshComp = GetOwningComponent();
	if (!MeshComp)
	{
		return;
	}

	// The const overload is the only public one, and reading is all this wants. Two passes: a
	// simulated proxy's anim instance may not have existed when the character first linked, and
	// InitializeAnimLayer guards and links once per class, so re-triggering it is cheap.
	UClass* CurrentLayerClass = nullptr;
	for (int32 Pass = 0; Pass < 2 && !CurrentLayerClass; ++Pass)
	{
		for (const UAnimInstance* Linked : MeshComp->GetLinkedAnimInstances())
		{
			if (Linked && Linked != this)
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

	Snapshot.bLayerChanged = CurrentLayerClass != ObservedLayerClass;
	if (!Snapshot.bLayerChanged)
	{
		return;
	}
	ObservedLayerClass = CurrentLayerClass;

	// Property addresses are resolved here and nowhere else: the set only changes on a weapon swap.
	LinkedLayers.Reset();
	for (UAnimInstance* Linked : MeshComp->GetLinkedAnimInstances())
	{
		if (Linked && Linked != this)
		{
			LinkedLayers.AddDefaulted_GetRef().Resolve(*Linked);
		}
	}
}

void UBNAnimInstance::PushAimSurfaceToLinkedLayers()
{
	for (const FBNLinkedLayer& Layer : LinkedLayers)
	{
		UAnimInstance* Instance = Layer.Instance.Get();
		if (!Instance || Instance->GetClass() != Layer.ResolvedClass.Get())
		{
			continue;
		}

		// Lean and ADS reach the layers from here alone: lean's input is BN's own State.Lean.*
		// tags, and ADS is the character's message in the template, which BN speaks as a tag.
		if (Layer.LeanRotation)
		{
			*Layer.LeanRotation->ContainerPtrToValuePtr<FRotator>(Instance) = LeanRotation;
		}
		if (Layer.LeanOppRotation)
		{
			*Layer.LeanOppRotation->ContainerPtrToValuePtr<FRotator>(Instance) = LeanOppRotation;
		}
		if (Layer.IsADS)
		{
			Layer.IsADS->SetPropertyValue_InContainer(Instance, GameplayTag_IsADS);
		}
		if (Layer.IsADSUpper)
		{
			Layer.IsADSUpper->SetPropertyValue_InContainer(Instance, GameplayTag_IsADS);
		}
		if (Layer.FPSMode)
		{
			Layer.FPSMode->SetPropertyValue_InContainer(Instance, bFPSMode);
		}
		if (Layer.Pitch)
		{
			Layer.Pitch->SetFloatingPointPropertyValue(Layer.Pitch->ContainerPtrToValuePtr<void>(Instance), Pitch);
		}
		if (Layer.AimPitch)
		{
			Layer.AimPitch->SetFloatingPointPropertyValue(Layer.AimPitch->ContainerPtrToValuePtr<void>(Instance), AimPitch);
		}
		if (Layer.AimYaw)
		{
			Layer.AimYaw->SetFloatingPointPropertyValue(Layer.AimYaw->ContainerPtrToValuePtr<void>(Instance), AimYaw);
		}
		if (Layer.PitchRotator)
		{
			*Layer.PitchRotator->ContainerPtrToValuePtr<FRotator>(Instance) = PitchRotator;
		}
		if (Layer.YawRotator)
		{
			*Layer.YawRotator->ContainerPtrToValuePtr<FRotator>(Instance) = YawRotator;
		}
	}
}

void UBNAnimInstance::UpdateADSFieldOfView(float DeltaSeconds)
{
	// bFPSMode is already "locally controlled AND player controlled" — the owner-only gate the
	// blend wants. The whole body now lives in FBNADSCameraBlend so UBNLAnimInstance runs the
	// identical blend off the identical numbers.
	ADSCameraBlend.Update(Camera, bTagADS, Snapshot.bFPSMode, DeltaSeconds);
}

void UBNAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// This ABP is SHARED: the template's own characters run this class too, and their pawns are
	// not ABNCharacter, so the game-thread pass never fills a snapshot for them. Without this
	// guard the pass below would publish zeroed values over whatever their graph wrote.
	if (!Character)
	{
		return;
	}

	const FVector WorldVelocity2D(Snapshot.WorldVelocity.X, Snapshot.WorldVelocity.Y, 0.0);
	const FVector WorldAcceleration2D(Snapshot.WorldAcceleration.X, Snapshot.WorldAcceleration.Y, 0.0);

	// ---- location
	DisplacementSinceLastUpdate = bFirstUpdate ? 0.0 : FVector::Dist2D(Snapshot.WorldLocation, PreviousWorldLocation);
	DisplacementSpeed = (!bFirstUpdate && DeltaSeconds > 0.f) ? DisplacementSinceLastUpdate / DeltaSeconds : 0.0;
	PreviousWorldLocation = Snapshot.WorldLocation;

	// ---- rotation
	YawDeltaSinceLastUpdate = bFirstUpdate ? 0.0 : FRotator::NormalizeAxis(Snapshot.ActorRotation.Yaw - PreviousYaw);
	YawDeltaSpeed = DeltaSeconds > 0.f ? YawDeltaSinceLastUpdate / DeltaSeconds : 0.0;
	PreviousYaw = Snapshot.ActorRotation.Yaw;
	// The source's lean factors; crouched leans less.
	AdditiveLeanAngle = YawDeltaSpeed * (Snapshot.bCrouching ? 0.025 : 0.0375);

	// ---- velocity
	LocalVelocity2D = Snapshot.ActorRotation.UnrotateVector(WorldVelocity2D);
	LocalVelocityDirectionAngle = CalculateDirection(WorldVelocity2D, Snapshot.ActorRotation);
	LocalVelocityDirectionAngleWithOffset = FRotator::NormalizeAxis(LocalVelocityDirectionAngle - RootYawOffset);
	LocalVelocityDirection = SelectCardinalFromAngle(
		LocalVelocityDirectionAngleWithOffset, CardinalDirectionDeadZone, LocalVelocityDirection, bWasMovingLastUpdate);
	LocalVelocityDirectionNoOffset = SelectCardinalFromAngle(
		LocalVelocityDirectionAngle, CardinalDirectionDeadZone, LocalVelocityDirectionNoOffset, bWasMovingLastUpdate);
	HasVelocity = !FMath::IsNearlyZero(WorldVelocity2D.SizeSquared());
	bWasMovingLastUpdate = HasVelocity;

	// ---- acceleration
	LocalAcceleration2D = Snapshot.ActorRotation.UnrotateVector(WorldAcceleration2D);
	HasAcceleration = !WorldAcceleration2D.IsNearlyZero();
	PivotDirection2D = FMath::Lerp(PivotDirection2D, WorldAcceleration2D.GetSafeNormal(), 0.5).GetSafeNormal();
	// The pivot cardinal is where the player came FROM: opposite of where acceleration points.
	CardinalFromAcceleration = OppositeCardinal(SelectCardinalFromAngle(
		CalculateDirection(PivotDirection2D, Snapshot.ActorRotation), CardinalDirectionDeadZone, CardinalFromAcceleration, false));

	// ---- wall heuristic: pushing hard, barely moving, accel roughly perpendicular to travel.
	IsRunningIntoWall =
		LocalAcceleration2D.Size2D() > 0.1 &&
		LocalVelocity2D.Size2D() < 200.0 &&
		FMath::IsWithinInclusive(
			FVector::DotProduct(LocalAcceleration2D.GetSafeNormal(), LocalVelocity2D.GetSafeNormal()), -0.6, 0.6);

	// ---- character state. States are tags; the movement component covers what no ability owns.
	// Walking off a ledge activates no jump ability, so the InAir tag alone would miss it.
	IsFalling = Snapshot.bInAirTag || Snapshot.bFalling;
	IsOnGround = !IsFalling;
	isCrouching = Snapshot.bCrouching;
	IsJumping = Snapshot.bJumping;
	bUnarmed = Snapshot.bUnarmed;
	bSprinting = Snapshot.bSprinting;
	bFPSMode = Snapshot.bFPSMode;
	GameplayTag_IsADS = Snapshot.bADS;
	if (SelfIsADSUpper)
	{
		SelfIsADSUpper->SetPropertyValue_InContainer(this, Snapshot.bADS);
	}

	ADSStateChanged = !bFirstUpdate && Snapshot.bADS != bWasADSLastUpdate;
	bWasADSLastUpdate = Snapshot.bADS;
	CrouchStateChange = !bFirstUpdate && Snapshot.bCrouching != bWasCrouchingLastUpdate;
	bWasCrouchingLastUpdate = Snapshot.bCrouching;
	ApplyCrouchAlpha = isCrouching ? 1.0 : 0.0;
	LinkedLayerChanged = !bFirstUpdate && Snapshot.bLayerChanged;
	GroundDistance = Snapshot.GroundDistance;

	// ---- jump apex: derived, never a countdown — -Vz/g, zero once descending.
	TimeToJumpApex = (IsFalling && Snapshot.WorldVelocity.Z > 0.0 && Snapshot.GravityZ < 0.0)
		? -Snapshot.WorldVelocity.Z / Snapshot.GravityZ
		: 0.0;

	// ---- blend weights
	UpperbodyDynamicAdditiveWeight = (Snapshot.bAnyMontagePlaying && IsOnGround)
		? 1.0
		: FMath::FInterpTo(UpperbodyDynamicAdditiveWeight, 0.0, static_cast<double>(DeltaSeconds), 6.0);

	// ---- root yaw offset / turn in place. In the source the graph's state functions drive the
	// mode; idle accumulates and everything else blends out, which is their net effect.
	if (bFirstUpdate)
	{
		RootYawOffset = 0.0;
		TurnYawCurveValue = 0.0;
		PreviousTurnYawCurveValue = 0.0;
	}
	const EBNRootYawOffsetMode YawMode = (HasVelocity || HasAcceleration || IsFalling)
		? EBNRootYawOffsetMode::BlendOut
		: EBNRootYawOffsetMode::Accumulate;
	if (YawMode == EBNRootYawOffsetMode::Accumulate)
	{
		SetRootYawOffset(RootYawOffset - YawDeltaSinceLastUpdate);
	}
	else
	{
		SetRootYawOffset(UKismetMathLibrary::FloatSpringInterp(
			static_cast<float>(RootYawOffset), 0.f, RootYawOffsetSpringState,
			/*Stiffness*/ 80.f, /*CriticalDampingFactor*/ 1.f, DeltaSeconds, /*Mass*/ 1.f));
	}
	ProcessTurnYawCurve();

	if (Snapshot.bDead)
	{
		AimPitch = 0.0;
		Pitch = 0.0;
		AimYaw = 0.0;
		SmoothedAimPitch = 0.0;
		SmoothedAimYaw = 0.0;
		PitchRotator = FRotator::ZeroRotator;
		YawRotator = FRotator::ZeroRotator;
		IsFirstUpdate = bFirstUpdate;
		bFirstUpdate = false;
		return;
	}

	// ---- aim. View relative to the body, on both axes. The capsule yaws with the controller so
	// owner AimYaw sits near zero and side-to-side is the body turning; AimPitch is the half the
	// capsule cannot do. A proxy's actor rotation interpolates, so both axes are live there.
	//
	// NormalizeAxis is load-bearing on a simulated proxy: GetBaseAimRotation decompresses
	// RemoteViewPitch into 0..360 there, so +-90 only survives normalized. This is the same
	// correction the reference spells out as GetFixedAimRotation's 270..360 -> -90..0 remap,
	// expressed as the one operation that also covers the locally-controlled case.
	const double TargetAimPitch = FMath::Clamp(
		FRotator::NormalizeAxis(Snapshot.BaseAimPitch - Snapshot.ActorRotation.Pitch),
		AimPitchClamp.X, AimPitchClamp.Y);
	const double TargetAimYaw = FMath::Clamp(
		FRotator::NormalizeAxis(Snapshot.BaseAimYaw - Snapshot.ActorRotation.Yaw),
		AimYawClamp.X, AimYawClamp.Y);

	// The owner is exact and must never be filtered — its own view is the one thing in this class
	// that carries zero latency, and smoothing it would put lag between the mouse and the gun.
	// A proxy is quantised and arrives at net rate, so it gets chased instead of stepped to.
	if (Snapshot.bLocallyControlled || ProxyAimInterpSpeed <= 0.f || bFirstUpdate)
	{
		SmoothedAimPitch = TargetAimPitch;
		SmoothedAimYaw = TargetAimYaw;
	}
	else
	{
		SmoothedAimPitch = FMath::FInterpTo(SmoothedAimPitch, TargetAimPitch, static_cast<double>(DeltaSeconds), static_cast<double>(ProxyAimInterpSpeed));
		SmoothedAimYaw = FMath::FInterpTo(SmoothedAimYaw, TargetAimYaw, static_cast<double>(DeltaSeconds), static_cast<double>(ProxyAimInterpSpeed));
	}

	AimPitch = SmoothedAimPitch;
	Pitch = SmoothedAimPitch;
	AimYaw = SmoothedAimYaw;
	PitchRotator = BNMakeAxisRotator(AimPitchAxis, SmoothedAimPitch);
	YawRotator = BNMakeAxisRotator(AimYawAxis, SmoothedAimYaw);

	// ---- lean. The tags replicate, so a proxy leans the way its owner does; both sides held
	// cancel.
	const double TargetLeaning = (Snapshot.bLeanRight ? 1.0 : 0.0) - (Snapshot.bLeanLeft ? 1.0 : 0.0);
	CurrentLeaning = FMath::FInterpTo(CurrentLeaning, TargetLeaning, static_cast<double>(DeltaSeconds), LeanInterpSpeed);
	const double LeanDegrees = CurrentLeaning * LeanAngle;
	LeanRotation = BNMakeAxisRotator(LeanAxis, LeanDegrees);
	// The head's counter-tilt: the body tips, the gaze stays level.
	LeanOppRotation = BNMakeAxisRotator(LeanAxis, -LeanDegrees);

	IsFirstUpdate = bFirstUpdate;
	bFirstUpdate = false;
}

void UBNAnimInstance::SetRootYawOffset(double InRootYawOffset)
{
	if (!bEnableRootYawOffset)
	{
		RootYawOffset = 0.0;
		return;
	}

	const FVector2D Clamp = Snapshot.bCrouching ? RootYawOffsetAngleClampCrouched : RootYawOffsetAngleClamp;
	const double Normalized = FRotator::NormalizeAxis(InRootYawOffset);
	RootYawOffset = Clamp.X < Clamp.Y ? FMath::Clamp(Normalized, Clamp.X, Clamp.Y) : Normalized;
}

void UBNAnimInstance::ProcessTurnYawCurve()
{
	PreviousTurnYawCurveValue = TurnYawCurveValue;

	const double TurnYawWeight = GetCurveValue(BN_TurnYawWeightCurve);
	if (FMath::IsNearlyZero(TurnYawWeight))
	{
		TurnYawCurveValue = 0.0;
		PreviousTurnYawCurveValue = 0.0;
		return;
	}

	// The turn animation's RemainingTurnYaw curve is normalized by its weight; the offset shrinks
	// by exactly the yaw the animation consumed this frame.
	TurnYawCurveValue = GetCurveValue(BN_RemainingTurnYawCurve) / TurnYawWeight;
	if (PreviousTurnYawCurveValue != 0.0)
	{
		SetRootYawOffset(RootYawOffset - (TurnYawCurveValue - PreviousTurnYawCurveValue));
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

void UBNAnimInstance::ResolveAbilitySystem()
{
	UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	AbilitySystem = ASC;

	auto Bind = [this, ASC](const FGameplayTag& Tag, FDelegateHandle& Handle, bool& Mirror)
	{
		Handle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UBNAnimInstance::OnTagChanged);
		// Seeded, not assumed: this instance can be created long after the tag was applied.
		Mirror = ASC->HasMatchingGameplayTag(Tag);
	};

	Bind(BNTags::State_Movement_Crouching, CrouchTagHandle, bTagCrouching);
	Bind(BNTags::State_Movement_Jumping, JumpTagHandle, bTagJumping);
	Bind(BNTags::State_Movement_InAir, InAirTagHandle, bTagInAir);
	Bind(BNTags::State_Movement_Sprinting, SprintTagHandle, bTagSprinting);
	Bind(BNTags::State_Lean_Left, LeanLeftTagHandle, bTagLeanLeft);
	Bind(BNTags::State_Lean_Right, LeanRightTagHandle, bTagLeanRight);
	Bind(BNTags::State_Weapon_ADS, ADSTagHandle, bTagADS);
	Bind(BNTags::State_Weapon_Reloading, ReloadTagHandle, bTagReloading);
	Bind(BNTags::State_Weapon_Firing, FiringTagHandle, bTagFiring);
	Bind(BNTags::State_Weapon_Melee, MeleeTagHandle, bTagMelee);
	Bind(BNTags::State_Dead, DeadTagHandle, bTagDead);
}

void UBNAnimInstance::OnTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bActive = NewCount > 0;
	if (Tag == BNTags::State_Movement_Crouching)      { bTagCrouching = bActive; }
	else if (Tag == BNTags::State_Movement_Jumping)   { bTagJumping = bActive; }
	else if (Tag == BNTags::State_Movement_InAir)     { bTagInAir = bActive; }
	else if (Tag == BNTags::State_Movement_Sprinting) { bTagSprinting = bActive; }
	else if (Tag == BNTags::State_Lean_Left)          { bTagLeanLeft = bActive; }
	else if (Tag == BNTags::State_Lean_Right)         { bTagLeanRight = bActive; }
	else if (Tag == BNTags::State_Weapon_ADS)         { bTagADS = bActive; }
	else if (Tag == BNTags::State_Weapon_Reloading)   { bTagReloading = bActive; }
	else if (Tag == BNTags::State_Weapon_Firing)      { bTagFiring = bActive; }
	else if (Tag == BNTags::State_Weapon_Melee)       { bTagMelee = bActive; }
	else if (Tag == BNTags::State_Dead)               { bTagDead = bActive; }
}

void UBNAnimInstance::NativeUninitializeAnimation()
{
	// The ability system component lives on the PlayerState and outlives this instance, so an
	// unregistered binding per respawn accumulates on it forever.
	if (AbilitySystem)
	{
		AbilitySystem->UnregisterGameplayTagEvent(CrouchTagHandle, BNTags::State_Movement_Crouching, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(JumpTagHandle, BNTags::State_Movement_Jumping, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(InAirTagHandle, BNTags::State_Movement_InAir, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(SprintTagHandle, BNTags::State_Movement_Sprinting, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(LeanLeftTagHandle, BNTags::State_Lean_Left, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(LeanRightTagHandle, BNTags::State_Lean_Right, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(ADSTagHandle, BNTags::State_Weapon_ADS, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(ReloadTagHandle, BNTags::State_Weapon_Reloading, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(FiringTagHandle, BNTags::State_Weapon_Firing, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(MeleeTagHandle, BNTags::State_Weapon_Melee, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(DeadTagHandle, BNTags::State_Dead, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem = nullptr;
	}

	CrouchTagHandle.Reset();
	JumpTagHandle.Reset();
	InAirTagHandle.Reset();
	SprintTagHandle.Reset();
	LeanLeftTagHandle.Reset();
	LeanRightTagHandle.Reset();
	ADSTagHandle.Reset();
	ReloadTagHandle.Reset();
	FiringTagHandle.Reset();
	MeleeTagHandle.Reset();
	DeadTagHandle.Reset();
	LinkedLayers.Reset();

	Super::NativeUninitializeAnimation();
}

FAnimInstanceProxy* UBNAnimInstance::CreateAnimInstanceProxy()
{
	return new FBNAnimInstanceProxy(this);
}

void UBNAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
