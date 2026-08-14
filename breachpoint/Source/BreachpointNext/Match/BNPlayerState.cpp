#include "Match/BNPlayerState.h"
#include "AbilitySystem/BNAbilitySet.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "AbilitySystem/Abilities/BNGA_Death.h"
#include "AbilitySystem/Abilities/BNGA_Equip.h"
#include "AbilitySystem/Abilities/BNGA_Grenade.h"
#include "AbilitySystem/Abilities/BNGA_Melee.h"
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

void ABNPlayerState::ApplyInitAttributes()
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative() || !InitEffect)
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(InitEffect, 1.f, AbilitySystemComponent->MakeEffectContext());
	if (SpecHandle.IsValid())
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void ABNPlayerState::BroadcastDeath()
{
	// Authority only: the GameMode is the subscriber and only exists there, and a client
	// broadcasting a death would be a second source of truth for the one thing that is entirely
	// the server's.
	if (AbilitySystemComponent && AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		OnPlayerDeath.Broadcast(this);
	}
}

void ABNPlayerState::GrantDefaults()
{
	if (bDefaultsGranted || !AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}
	bDefaultsGranted = true;

	ApplyInitAttributes();

	// Granted OUTSIDE the set/fallback split and with no input tag: death is not a key and not a
	// weapon's verb. UBNHealthComponent's verdict activates it by class, on the authority.
	AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UBNGA_Death::StaticClass(), 1));

	// Body verbs, granted OUTSIDE the set/fallback split on purpose: melee and grenade must exist
	// whether or not DefaultAbilitySet is configured, must survive every weapon swap, and must not
	// wait on an editor edit to two DA_BNAbilitySet assets to become reachable. Melee still reads
	// its montage, damage and reach from the CURRENT weapon's row.
	// If either later joins DefaultAbilitySet, delete it here — two grants means two specs.
	FGameplayAbilitySpec MeleeSpec(UBNGA_Melee::StaticClass(), 1);
	MeleeSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Melee);
	AbilitySystemComponent->GiveAbility(MeleeSpec);

	FGameplayAbilitySpec GrenadeSpec(UBNGA_Grenade::StaticClass(), 1);
	GrenadeSpec.GetDynamicSpecSourceTags().AddTag(BNTags::Input_Grenade);
	AbilitySystemComponent->GiveAbility(GrenadeSpec);

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
