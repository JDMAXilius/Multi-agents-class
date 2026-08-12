#include "Match/BNPlayerState.h"
#include "AbilitySystem/BNAbilitySet.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "AbilitySystem/Abilities/BNMovementAbilities.h"
#include "Core/BNGameplayTags.h"

ABNPlayerState::ABNPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UBNAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UBNAttributeSet>(TEXT("AttributeSet"));

	InitEffect = UBNGE_InitAttributes::StaticClass();

	SetNetUpdateFrequency(100.f);
}

UAbilitySystemComponent* ABNPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABNPlayerState::GrantDefaults()
{
	if (bDefaultsGranted || !AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}
	bDefaultsGranted = true;

	if (InitEffect)
	{
		const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(InitEffect, 1.f, AbilitySystemComponent->MakeEffectContext());
		if (SpecHandle.IsValid())
		{
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	if (DefaultAbilitySet)
	{
		FBNAbilitySetHandles Handles;
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, Handles);
	}
	else
	{
		FGameplayAbilitySpec JumpSpec(UBNGA_Jump::StaticClass(), 1);
		JumpSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Jump);
		AbilitySystemComponent->GiveAbility(JumpSpec);

		FGameplayAbilitySpec CrouchSpec(UBNGA_Crouch::StaticClass(), 1);
		CrouchSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Crouch);
		AbilitySystemComponent->GiveAbility(CrouchSpec);
	}
}
