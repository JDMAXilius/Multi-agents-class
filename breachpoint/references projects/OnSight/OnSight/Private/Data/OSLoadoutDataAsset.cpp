#include "Data/OSLoadoutDataAsset.h"

#include "AbilitySystemComponent.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "GAS/Effects/GE_OSPlayerDefaultAttributes.h"
#include "GAS/Effects/OSGameplayEffect.h"

namespace
{
static void GrantLoadoutAbilityEntry(UAbilitySystemComponent* ASC, const FOSLoadoutAbilityEntry& Entry, TArray<FGameplayAbilitySpecHandle>& OutHandles, const int32 SpecLevel)
{
	if (!ASC || !Entry.AbilityClass)
	{
		return;
	}

	FGameplayAbilitySpec Spec(Entry.AbilityClass, SpecLevel);
	if (Entry.InputTag.IsValid())
	{
		Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
	}

	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
	if (Handle.IsValid())
	{
		OutHandles.Add(Handle);
	}
}

static void GrantLoadoutAbilityClass(UAbilitySystemComponent* ASC, TSubclassOf<UOSGameplayAbility> AbilityClass, TArray<FGameplayAbilitySpecHandle>& OutHandles, const int32 SpecLevel)
{
	if (!ASC || !AbilityClass)
	{
		return;
	}

	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, SpecLevel));
	if (Handle.IsValid())
	{
		OutHandles.Add(Handle);
	}
}
} // namespace

UOSLoadoutDataAsset::UOSLoadoutDataAsset()
{
	// Deterministic baseline: matches AOSCharacter::ResetAttributes fallback and GE_OSPlayerDefaultAttributes (UOSAttributeSet).
	if (!Definition.InitAttributeEffect)
	{
		Definition.InitAttributeEffect = UGE_OSPlayerDefaultAttributes::StaticClass();
	}
}

void UOSLoadoutDataAsset::GiveToAbilitySystemComponent(
	UAbilitySystemComponent* ASC,
	AActor* AvatarActor,
	AActor* EffectContextFallback,
	TArray<FGameplayAbilitySpecHandle>& OutGrantedAbilities,
	TArray<FActiveGameplayEffectHandle>& OutGrantedEffects,
	FGameplayTagContainer& OutCachedLooseTags,
	const float ApplyLevel) const
{
	Definition.GiveToAbilitySystemComponent(ASC, AvatarActor, EffectContextFallback, OutGrantedAbilities, OutGrantedEffects, OutCachedLooseTags, ApplyLevel);
}

void FOSGameplayLoadoutDefinition::GiveToAbilitySystemComponent(
	UAbilitySystemComponent* ASC,
	AActor* AvatarActor,
	AActor* EffectContextFallback,
	TArray<FGameplayAbilitySpecHandle>& OutGrantedAbilities,
	TArray<FActiveGameplayEffectHandle>& OutGrantedEffects,
	FGameplayTagContainer& OutCachedLooseTags,
	const float ApplyLevel) const
{
	if (!ASC)
	{
		return;
	}

	const float LevelScale = ApplyLevel > 0.f ? ApplyLevel : 1.f;
	const int32 AbilitySpecLevel = FMath::Max(1, FMath::RoundToInt(LevelScale));
	AActor* const SourceObject = AvatarActor ? AvatarActor : EffectContextFallback;

	TSubclassOf<UOSGameplayEffect> AttributeEffect = InitAttributeEffect;
	if (!IsValid(AttributeEffect))
	{
		AttributeEffect = UGE_OSPlayerDefaultAttributes::StaticClass();
	}

	if (IsValid(AttributeEffect))
	{
		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		Ctx.AddSourceObject(SourceObject);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(AttributeEffect, LevelScale, Ctx);
		if (Spec.IsValid() && Spec.Data.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	for (const FOSLoadoutEffectEntry& EffectEntry : GrantedGameplayEffects)
	{
		const TSubclassOf<UOSGameplayEffect> EffectClass = EffectEntry.EffectClass;
		if (!IsValid(EffectClass))
		{
			continue;
		}

		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		Ctx.AddSourceObject(SourceObject);
		const float EffectLevel = (EffectEntry.Level > 0.f ? EffectEntry.Level : 1.f) * LevelScale;
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, EffectLevel, Ctx);
		if (Spec.IsValid() && Spec.Data.IsValid())
		{
			const FActiveGameplayEffectHandle AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			if (AppliedHandle.IsValid())
			{
				OutGrantedEffects.Add(AppliedHandle);
			}
		}
	}

	if (GrantedGameplayTags.Num() > 0)
	{
		ASC->AddLooseGameplayTags(GrantedGameplayTags);
		OutCachedLooseTags = GrantedGameplayTags;
	}

	for (const FOSLoadoutAbilityEntry& AbilityEntry : GrantedAbilities)
	{
		GrantLoadoutAbilityEntry(ASC, AbilityEntry, OutGrantedAbilities, AbilitySpecLevel);
	}

	GrantLoadoutAbilityClass(ASC, MobilityAction, OutGrantedAbilities, AbilitySpecLevel);
	GrantLoadoutAbilityClass(ASC, GrabAction, OutGrantedAbilities, AbilitySpecLevel);
	GrantLoadoutAbilityClass(ASC, BlockAction, OutGrantedAbilities, AbilitySpecLevel);
	GrantLoadoutAbilityClass(ASC, OffensiveLightAction, OutGrantedAbilities, AbilitySpecLevel);
	GrantLoadoutAbilityClass(ASC, OffensiveHeavyAction, OutGrantedAbilities, AbilitySpecLevel);
	GrantLoadoutAbilityClass(ASC, JumpAction, OutGrantedAbilities, AbilitySpecLevel);
	GrantLoadoutAbilityClass(ASC, ClassAbility1, OutGrantedAbilities, AbilitySpecLevel);
	GrantLoadoutAbilityClass(ASC, ClassAbility2, OutGrantedAbilities, AbilitySpecLevel);
	GrantLoadoutAbilityClass(ASC, ClassAbility3, OutGrantedAbilities, AbilitySpecLevel);
	GrantLoadoutAbilityClass(ASC, ClassAbility4, OutGrantedAbilities, AbilitySpecLevel);
}
