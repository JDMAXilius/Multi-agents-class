#include "FPS/BRAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

#include "Core/BRGameplayTags.h"

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

	Snapshot.WorldLocation = Pawn->GetActorLocation();
	Snapshot.WorldRotation = Pawn->GetActorRotation();
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
		}
	}

	Snapshot.bValid = true;
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

	// ------------------------------------------------------------------ fire stamp
	if (bFirePending.exchange(false))
	{
		TimeSinceFired = 0.f;
	}
	// Saturate rather than accumulate forever. `9999` is the pack's own sentinel for "not
	// recently"; past it the value carries no information and float precision only degrades.
	TimeSinceFired = FMath::Min(TimeSinceFired + DeltaSeconds, BR_NeverFired);

	// ------------------------------------------------------------------ locomotion
	bIsOnGround = Snapshot.bIsOnGround;
	bIsFalling = Snapshot.bIsFalling;
	bIsCrouched = Snapshot.bIsCrouched;

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

	if (bHasVelocity)
	{
		RootYawOffset = FMath::FInterpConstantTo(RootYawOffset, 0.f, DeltaSeconds, RootYawOffsetBleedSpeed);
	}
	else
	{
		RootYawOffset = FMath::Clamp(RootYawOffset - YawDeltaSinceLastUpdate, ClampMin, ClampMax);
	}

	// ------------------------------------------------------------------ lean
	LeanAngle = FMath::Clamp(YawDeltaSpeed * LeanScale, -LeanMaxAngle, LeanMaxAngle);

	// ------------------------------------------------------------------ sway
	// Springs chase the turn rate, so the weapon lags the camera and settles instead of being
	// welded to it. Amendment A's "further than Lyra" lives in these four lines; what it is
	// missing is only the custom NODE, not the computation.
	SwayYawSpring.Step(FMath::Clamp(-YawDeltaSpeed * SwayYawScale, -SwayMaxAngle, SwayMaxAngle),
		SwayStiffness, SwayDamping, DeltaSeconds);

	const float PitchRate = DeltaSeconds > 0.f ? AimPitch : 0.f;
	SwayPitchSpring.Step(FMath::Clamp(-PitchRate * SwayPitchScale, -SwayMaxAngle, SwayMaxAngle),
		SwayStiffness, SwayDamping, DeltaSeconds);

	SwayRotation = FRotator(SwayPitchSpring.Value, SwayYawSpring.Value, 0.f);
	SwayLocation = FVector::ZeroVector;
}

void UBRAnimInstance::NativeUninitializeAnimation()
{
	UnbindAbilitySystem();

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
		{ BRGameplayTags::State_Movement_Sprinting,   &UBRAnimInstance::bIsSprinting },
		{ BRGameplayTags::State_Movement_Grappling,   &UBRAnimInstance::bIsGrappling },
		{ BRGameplayTags::State_Weapon_Reloading,     &UBRAnimInstance::bIsReloading },
		{ BRGameplayTags::State_Weapon_Swapping,      &UBRAnimInstance::bIsSwapping },
		{ BRGameplayTags::State_Combat_Meleeing,      &UBRAnimInstance::bIsMeleeing },
		{ BRGameplayTags::State_Combat_ThrowingGrenade, &UBRAnimInstance::bIsThrowingGrenade },
		{ BRGameplayTags::State_Dead,                 &UBRAnimInstance::bIsDead },
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

	// Respawn re-points the ASC. Binding the new one without dropping the old leaves a callback
	// writing into a dead instance's bools -- and the first symptom is a corpse that reloads.
	UnbindAbilitySystem();

	for (const FBRTagBinding& Binding : GetTagBindings())
	{
		TagHandles.Add(
			ASC->RegisterGameplayTagEvent(Binding.Tag, EGameplayTagEventType::NewOrRemoved)
			   .AddUObject(this, &UBRAnimInstance::OnStateTagChanged));

		// Seed from the CURRENT count. A callback only fires on CHANGE, so binding to an ASC
		// that is already sprinting leaves the bool false until the player stops -- state that
		// is wrong exactly when it is least visible in testing and most visible in a match.
		this->*(Binding.Field) = ASC->HasMatchingGameplayTag(Binding.Tag);
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
}

void UBRAnimInstance::OnStateTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Linear over seven entries, on a tag change rather than per frame. A map here would be a
	// hash of a struct to save six comparisons that happen when a player starts sprinting.
	for (const FBRTagBinding& Binding : GetTagBindings())
	{
		if (Binding.Tag == Tag)
		{
			this->*(Binding.Field) = NewCount > 0;
			return;
		}
	}
}
