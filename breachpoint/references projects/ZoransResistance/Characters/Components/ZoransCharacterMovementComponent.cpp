// Copyright Zollpa LLC.


#include "ZoransCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "ZoransResistance/AbilitySystem/Attributes/MovementAttributes.h"

namespace ZoransMovementCVars
{
	// Listen server smoothing
	static int32 UseListenServerSmoothing = 1;
	FAutoConsoleVariableRef CVarNetEnableListenServerSmoothing(
		TEXT("p.UseListenServerSmoothing"),
		UseListenServerSmoothing,
		TEXT("Whether to enable mesh smoothing on listen servers for the local view of remote clients.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);
	
	static int32 UseQueuedAnimEventsOnServer = 1;
	FAutoConsoleVariableRef CVarEnableQueuedAnimEventsOnServer(
		TEXT("a.UseQueuedAnimEventsOnServer"),
		UseQueuedAnimEventsOnServer,
		TEXT("Whether to enable queued anim events on servers. In most cases, when the server is doing a full anim graph update, queued notifies aren't triggered by the server, but this will enable them. Enabling this is recommended in projects using Listen Servers. \n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);
}

UZoransCharacterMovementComponent::UZoransCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	NavAgentProps.bCanCrouch = true;
	MaxAcceleration = 2400.0f;
	BrakingFrictionFactor = 1.0f;
	BrakingFriction = 6.0f;
	GroundFriction = 8.0f;
	BrakingDecelerationWalking = 1400.0f;
	bUseControllerDesiredRotation = false;
	bOrientRotationToMovement = false;
	RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	bAllowPhysicsRotationDuringAnimRootMotion = false;
	GetNavAgentPropertiesRef().bCanCrouch = true;
	bCanWalkOffLedgesWhenCrouching = true;
	SetCrouchedHalfHeight(65.0f);
	JumpZVelocity = StaticJumpStrength;
	AirControl = StaticAirControl;
	FallingLateralFriction = StaticFallingLateralFriction;
}

void UZoransCharacterMovementComponent::BindMovementAttributes(UAbilitySystemComponent* InAbilitySystemComponent)
{
	ensure(InAbilitySystemComponent);

	if (const UAttributeSet* Attributes = InAbilitySystemComponent->GetAttributeSet(UMovementAttributes::StaticClass()))
	{
		MovementAttributes = Cast<UMovementAttributes>(Attributes);

		InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(MovementAttributes->GetCurrentMovementSpeedAttribute()).AddUObject(this, &UZoransCharacterMovementComponent::Internal_OnMovementSpeedChanged);
		InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(MovementAttributes->GetWeightModifierAttribute()).AddUObject(this, &UZoransCharacterMovementComponent::WeightValueChanged);
		InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(MovementAttributes->GetJumpModifierAttribute()).AddUObject(this, &UZoransCharacterMovementComponent::JumpValueChanged);

		Internal_RecalculateMovementValues();
	}
}

void UZoransCharacterMovementComponent::ResetMovementValues()
{
	Internal_RecalculateMovementValues();

	bApplyGravityWhileJumping = true;
	AirControl = StaticAirControl;
	FallingLateralFriction = StaticFallingLateralFriction;
}

UMovementAttributes* UZoransCharacterMovementComponent::GetMovementAttributes()
{
	if (MovementAttributes.Get())
	{
		return MovementAttributes.Get();
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (const UAttributeSet* Attributes = AbilitySystemComponent->GetAttributeSet(UMovementAttributes::StaticClass()))
		{
			MovementAttributes = Cast<UMovementAttributes>(Attributes);

			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(MovementAttributes->GetCurrentMovementSpeedAttribute()).AddUObject(this, &UZoransCharacterMovementComponent::Internal_OnMovementSpeedChanged);
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(MovementAttributes->GetWeightModifierAttribute()).AddUObject(this, &UZoransCharacterMovementComponent::WeightValueChanged);
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(MovementAttributes->GetJumpModifierAttribute()).AddUObject(this, &UZoransCharacterMovementComponent::JumpValueChanged);

			Internal_RecalculateMovementValues();
		}
	}

	return MovementAttributes.Get();
}

void UZoransCharacterMovementComponent::MoveAutonomous(float ClientTimeStamp, const float DeltaTime, const uint8 CompressedFlags, const FVector& NewAccel)
{
	if (!HasValidData())
	{
		return;
	}

	UpdateFromCompressedFlags(CompressedFlags);
	CharacterOwner->CheckJumpInput(DeltaTime);

	Acceleration = ConstrainInputAcceleration(NewAccel);
	Acceleration = Acceleration.GetClampedToMaxSize(GetMaxAcceleration());
	AnalogInputModifier = ComputeAnalogInputModifier();
	
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FQuat OldRotation = UpdatedComponent->GetComponentQuat();
	
	PerformMovement(DeltaTime);

	// Check if data is valid as PerformMovement can mark character for pending kill
	if (!HasValidData())
	{
		return;
	}

	// If not playing root motion, tick animations after physics. We do this here to keep events, notifies, states and transitions in sync with client updates.
	if (CharacterOwner && !CharacterOwner->bClientUpdating && !CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh())
	{
		if (!ZoransMovementCVars::UseQueuedAnimEventsOnServer || CharacterOwner->GetMesh()->ShouldOnlyTickMontages(DeltaTime))
		{
			// If we're not doing a full anim graph update on the server, 
			// trigger events right away, as we could be receiving multiple ServerMoves per frame.
			CharacterOwner->GetMesh()->ConditionallyDispatchQueuedAnimEvents();
		}
	}

	if (CharacterOwner && UpdatedComponent)
	{
		// Smooth local view of remote clients on listen servers
		if (ZoransMovementCVars::UseListenServerSmoothing && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy && IsNetMode(NM_ListenServer))
		{
			SmoothCorrection(OldLocation, OldRotation, UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat());
		}
	}	
}

void UZoransCharacterMovementComponent::Internal_OnMovementSpeedChanged(const FOnAttributeChangeData& Data)
{
	MaxWalkSpeed = CalculateMovementSpeed();

	const float SpeedModifier = MaxWalkSpeed / StaticMovementSpeed;
	const float MaxSmoothUpdateDistance = FMath::Clamp(256.0f * SpeedModifier, 256.0f, 590.0f);
	
	NetworkMaxSmoothUpdateDistance = MaxSmoothUpdateDistance;
	NetworkNoSmoothUpdateDistance = MaxSmoothUpdateDistance * 1.5f;
}

void UZoransCharacterMovementComponent::Internal_RecalculateMovementValues()
{
	if (bUseStaticGravity)
	{
		if (GetMovementAttributes())
		{
			CurrentWeightModifier = MovementAttributes->GetWeightModifier();
			GravityScale = FMath::Clamp<float>(StaticGravityScale + CurrentWeightModifier * 0.5f, 0.2f, StaticGravityScale * 3.f);
			MaxWalkSpeed = CalculateMovementSpeed();

			CurrentJumpModifier = MovementAttributes->GetJumpModifier();
			JumpZVelocity = CalculateJumpZ();

			if (OnMovementSpeedChangedDelegate.IsBound())
			{
				OnMovementSpeedChangedDelegate.Broadcast(MaxWalkSpeed);
			}
		}
	}
}

float UZoransCharacterMovementComponent::CalculateMovementSpeed()
{
	float OutSpeed = 0.f;

	if (GetMovementAttributes())
	{
		const float BaseMovementSpeed = MovementAttributes->GetCurrentMovementSpeed();
		const float WeightPenaltyModifier = (1.f - CurrentWeightModifier) * 0.5f;
		const float RawSpeedPenalty = StaticMovementSpeed * WeightPenaltyModifier;
		const float SpeedPenalty = FMath::Clamp<float>(RawSpeedPenalty, StaticMovementSpeed * -0.5f, 0);

		OutSpeed = BaseMovementSpeed * (StaticMovementSpeed + SpeedPenalty);
	}

	return OutSpeed;
}

void UZoransCharacterMovementComponent::WeightValueChanged(const FOnAttributeChangeData& Data)
{
	const float NewWeightValue = Data.NewValue;
	
	if (!FMath::IsNearlyEqual(NewWeightValue, CurrentWeightModifier, 0.01f))
	{
		CurrentWeightModifier = NewWeightValue;
		JumpZVelocity = CalculateJumpZ();
		
		MaxWalkSpeed = CalculateMovementSpeed();
		GravityScale = FMath::Clamp<float>(StaticGravityScale + CurrentWeightModifier * 0.5f, 0.2f, StaticGravityScale * 3.f);

		if (OnMovementSpeedChangedDelegate.IsBound())
		{
			OnMovementSpeedChangedDelegate.Broadcast(MaxWalkSpeed);
		}
	}
}

void UZoransCharacterMovementComponent::JumpValueChanged(const FOnAttributeChangeData& Data)
{
	const float NewJumpValue = Data.NewValue;
		
	if (!FMath::IsNearlyEqual(NewJumpValue, CurrentJumpModifier, 0.01f))
	{
		CurrentJumpModifier = NewJumpValue;
		JumpZVelocity = CalculateJumpZ();
	}
}

float UZoransCharacterMovementComponent::CalculateJumpZ() const
{
	const float JumpPenaltyModifier = 1.f - CurrentWeightModifier;
	const float JumpPenalty = StaticJumpStrength * JumpPenaltyModifier * 0.15f;
	return (JumpPenaltyModifier > 0.f ? StaticJumpStrength : StaticJumpStrength + JumpPenalty) * CurrentJumpModifier;
}