#include "AbilitySystem/Abilities/BRGA_Sprint.h"

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
}

void UBRGA_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UBRCharacterMovementComponent* Movement = GetBRCharacterMovement(ActorInfo))
	{
		Movement->SetSprintIntent(false);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
