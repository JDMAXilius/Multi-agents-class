#include "AbilitySystem/Abilities/BRGA_Sprint.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameFramework/Character.h"

#include "Character/BRCharacterMovementComponent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"

UBRGA_Sprint::UBRGA_Sprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EBRAbilityActivationPolicy::WhileInputHeld;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Sprint);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(BRGameplayTags::State_Movement_Sprinting);
}

UBRCharacterMovementComponent* UBRGA_Sprint::GetBRCharacterMovement(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return Character ? Cast<UBRCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}

ACharacter* UBRGA_Sprint::GetAvatarCharacter() const
{
	return CurrentActorInfo ? Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

bool UBRGA_Sprint::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UBRGA_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		return;
	}

	UBRCharacterMovementComponent* Movement = GetBRCharacterMovement(ActorInfo);
	if (!Movement)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Movement->SetSprintIntent(true);

	// LEAVING THE GROUND IS REACTIVE, NOT POLLED. A jump mid-sprint has to drop the sprint on
	// the frame it happens; polling would leave State.Movement.Sprinting granted for up to
	// SprintCheckIntervalSeconds after the sprint has physically stopped, and anything gated on
	// that tag would read the wrong state for a tenth of a second.
	if (ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		Character->MovementModeChangedDelegate.AddDynamic(this, &UBRGA_Sprint::HandleMovementModeChanged);
		bBoundMovementMode = true;
	}

	// Standing still fires no event, so it is the one condition that has to be polled.
	StartSprintCheckLoop();
}

void UBRGA_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Unbind BEFORE clearing the intent: dropping the sprint speed can itself change the
	// movement mode, which would re-enter the handler while the ability is already tearing down.
	if (bBoundMovementMode)
	{
		if (ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
		{
			Character->MovementModeChangedDelegate.RemoveDynamic(this, &UBRGA_Sprint::HandleMovementModeChanged);
		}
		bBoundMovementMode = false;
	}

	if (UBRCharacterMovementComponent* Movement = GetBRCharacterMovement(ActorInfo))
	{
		Movement->SetSprintIntent(false);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBRGA_Sprint::StartSprintCheckLoop()
{
	// A fresh WaitDelay per interval rather than a timer, because an ability task dies with the
	// ability: a cancelled sprint cannot leave a callback pointing at a torn-down instance.
	// Law 4 - this is a delegate chain, not a Tick.
	if (UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, SprintCheckIntervalSeconds))
	{
		WaitTask->OnFinish.AddDynamic(this, &UBRGA_Sprint::OnSprintCheck);
		WaitTask->ReadyForActivation();
	}
}

bool UBRGA_Sprint::IsMovingFastEnough() const
{
	const ACharacter* Character = GetAvatarCharacter();
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return false;
	}

	return Movement->Velocity.SizeSquared2D() > FMath::Square(MinSprintSpeedUU);
}

void UBRGA_Sprint::OnSprintCheck()
{
	if (!IsActive())
	{
		return;
	}

	if (!IsMovingFastEnough())
	{
		CancelSelf();
		return;
	}

	StartSprintCheckLoop();
}

void UBRGA_Sprint::HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	// Anything that is not walking ends it: falling, flying, swimming. The CMC already refuses
	// the speed bonus off the ground (IsSprintIntentValid), so without this the ability would
	// stay active granting a state tag that no longer describes anything.
	if (!Movement->IsMovingOnGround())
	{
		CancelSelf();
	}
}

void UBRGA_Sprint::CancelSelf()
{
	if (IsActive())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}
