#include "Match/BNPlayerState.h"
#include "AbilitySystem/BNAbilitySet.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "AbilitySystem/Abilities/BNGA_Equip.h"
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

		// Swap is the INVENTORY's verb, not any weapon's. Granted by the weapon's set it
		// would clear its own spec by running — the next press inside a round trip carries a
		// dead handle and is dropped — and a row with a null AbilitySet would leave no swap
		// spec at all, locking the player to that weapon for the life.
		FGameplayAbilitySpec SwapNextSpec(UBNGA_SwapNext::StaticClass(), 1);
		SwapNextSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Weapon_Next);
		AbilitySystemComponent->GiveAbility(SwapNextSpec);

		FGameplayAbilitySpec SwapPreviousSpec(UBNGA_SwapPrevious::StaticClass(), 1);
		SwapPreviousSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Weapon_Previous);
		AbilitySystemComponent->GiveAbility(SwapPreviousSpec);

		// Character verbs, not weapon verbs: sprint and lean belong to the body and must survive
		// every weapon swap, so they are granted here and never by a weapon's ability set.
		FGameplayAbilitySpec SprintSpec(UBNGA_Sprint::StaticClass(), 1);
		SprintSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Sprint);
		AbilitySystemComponent->GiveAbility(SprintSpec);

		FGameplayAbilitySpec LeanLeftSpec(UBNGA_LeanLeft::StaticClass(), 1);
		LeanLeftSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Lean_Left);
		AbilitySystemComponent->GiveAbility(LeanLeftSpec);

		FGameplayAbilitySpec LeanRightSpec(UBNGA_LeanRight::StaticClass(), 1);
		LeanRightSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Lean_Right);
		AbilitySystemComponent->GiveAbility(LeanRightSpec);
	}
}
