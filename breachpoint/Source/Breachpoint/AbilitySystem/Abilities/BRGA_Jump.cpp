#include "AbilitySystem/Abilities/BRGA_Jump.h"

#include "GameFramework/Character.h"

#include "Core/BRGameplayTags.h"

UBRGA_Jump::UBRGA_Jump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Held, not pressed: ACharacter::Jump holds bPressedJump for as long as the key is down and
	// StopJumping cuts the arc. A press-only policy would end the ability on the same frame it
	// began and every jump would be the shortest one the CMC can produce.
	ActivationPolicy = EBRAbilityActivationPolicy::WhileInputHeld;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Jump);
	SetAssetTags(AssetTags);

	// Every other verb cancels sprint; jump was the sole exception. Leaving the ground cancels
	// it anyway through BRGA_Sprint's movement-mode handler, but the intent belongs stated here
	// rather than resting on another ability's implementation detail.
	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);
}

ACharacter* UBRGA_Jump::GetAvatarCharacter(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
}

bool UBRGA_Jump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// The gate belongs here rather than in ActivateAbility: this ability is LocalPredicted, so
	// activating a jump the CMC will refuse would open a prediction window the server rejects,
	// and the client would visibly rubber-band on every jump attempt made mid-air.
	const ACharacter* Character = GetAvatarCharacter(ActorInfo);
	return Character && Character->CanJump();
}

void UBRGA_Jump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		return;
	}

	ACharacter* Character = GetAvatarCharacter(ActorInfo);
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Character->Jump();
}

void UBRGA_Jump::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Runs on release, on cancel, and on death. All three want the arc cut.
	if (ACharacter* Character = GetAvatarCharacter(ActorInfo))
	{
		Character->StopJumping();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
