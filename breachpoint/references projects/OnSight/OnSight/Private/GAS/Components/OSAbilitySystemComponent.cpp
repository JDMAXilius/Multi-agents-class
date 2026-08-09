// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "AbilitySystemGlobals.h"

UOSAbilitySystemComponent::UOSAbilitySystemComponent()
{
	// Replication is set on the owning actor (PlayerState/Character), not here
	// SetIsReplicated(true) should be called in the owning actor's constructor
	
	// ASC should not tick by default - it's a performance optimization
	// Only enable ticking if you have specific needs (e.g., custom prediction logic)
	PrimaryComponentTick.bCanEverTick = false;
}

void UOSAbilitySystemComponent::BufferInput(EOSBufferedInputType Type)
{
	if (Type == EOSBufferedInputType::None)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	BufferedInput.Type = Type;
	BufferedInput.TimePressedSeconds = World->GetTimeSeconds();
}

bool UOSAbilitySystemComponent::ConsumeBufferedInput(EOSBufferedInputType& OutType)
{
	OutType = EOSBufferedInputType::None;

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (BufferedInput.Type == EOSBufferedInputType::None)
	{
		return false;
	}

	const float Age = World->GetTimeSeconds() - BufferedInput.TimePressedSeconds;
	if (Age > BufferedInputMaxAgeSeconds)
	{
		BufferedInput.Clear();
		return false;
	}

	OutType = BufferedInput.Type;
	BufferedInput.Clear();
	return true;
}

void UOSAbilitySystemComponent::ClearBufferedInput()
{
	BufferedInput.Clear();
}

bool UOSAbilitySystemComponent::HasBufferedInput(EOSBufferedInputType Type) const
{
	if (Type == EOSBufferedInputType::None || BufferedInput.Type != Type)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const float Age = World->GetTimeSeconds() - BufferedInput.TimePressedSeconds;
	return Age <= BufferedInputMaxAgeSeconds;
}

void UOSAbilitySystemComponent::RemoveTagChildren( FGameplayTag parentTag )
{
	FGameplayTagContainer ownedTags;
	GetOwnedGameplayTags(ownedTags);
	
	FGameplayTagContainer toRemove;
	for (const auto& tag : ownedTags)
		if (tag.MatchesTag(parentTag))
			toRemove.AddTag(tag);

	for (const auto& tag : toRemove)
		RemoveReplicatedLooseGameplayTag(tag);
}

bool UOSAbilitySystemComponent::GetAbilitySpecByTag(const FGameplayTag& AbilityTag, FGameplayAbilitySpec& OutSpec) const
{
	// Search through all granted abilities
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability)
		{
			const FGameplayTagContainer& AssetTags = Spec.Ability->GetAssetTags();
			if (AssetTags.HasTagExact(AbilityTag))
			{
				OutSpec = Spec;
				return true;
			}
		}
	}
	return false;
}

bool UOSAbilitySystemComponent::HasAbilityWithTag(const FGameplayTag& AbilityTag) const
{
	// Search through all granted abilities
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability)
		{
			const FGameplayTagContainer& AssetTags = Spec.Ability->GetAssetTags();
			if (AssetTags.HasTagExact(AbilityTag))
			{
				return true;
			}
		}
	}
	return false;
}

void UOSAbilitySystemComponent::CancelAllAbilities()
{
	// Cancel all active abilities (pass nullptr to cancel all)
	CancelAbilities(nullptr, nullptr, nullptr);
}

void UOSAbilitySystemComponent::CancelAbilitiesWithTags(const FGameplayTagContainer& CancelTags)
{
	// Cancel abilities matching the provided tags
	CancelAbilities(&CancelTags, nullptr, nullptr);
}

void UOSAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);
	
	// Broadcast delegate for UI, debugging, etc.
	OnAbilityGranted.Broadcast(AbilitySpec);
}

void UOSAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnRemoveAbility(AbilitySpec);
	
	// Broadcast delegate for UI, debugging, etc.
	OnAbilityRemoved.Broadcast(AbilitySpec);
}

// Note: OnAbilityRemoved delegate can be manually broadcast when abilities are removed
// For example, in SetRemoveAbilityOnEnd or when explicitly removing abilities
