// Breachpoint. The grant unit: soft ability classes + InputTags, granted and revoked as a set.

#include "AbilitySystem/BRAbilitySet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "Core/BRCore.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void UBRAbilitySet::GiveToAbilitySystem(UAbilitySystemComponent* ASC, UObject* SourceObject, TArray<FGameplayAbilitySpecHandle>& OutAbilityHandles, TArray<FActiveGameplayEffectHandle>& OutEffectHandles) const
{
	if (!ASC)
	{
		UE_LOG(LogBRCombat, Error, TEXT("UBRAbilitySet '%s': GiveToAbilitySystem with a null ASC; nothing granted."), *GetName());
		return;
	}

	if (!ASC->IsOwnerActorAuthoritative())
	{
		// REFUSED, not "granted locally". A client-side grant creates a spec the server has never
		// heard of; it activates predictively, is never confirmed, and the failure surfaces as an
		// ability that "sometimes does not fire".
		UE_LOG(LogBRCombat, Error, TEXT("UBRAbilitySet '%s': GiveToAbilitySystem called without authority on '%s' — REFUSED. Ability sets are granted on the server."),
			*GetName(), *GetNameSafe(ASC->GetOwner()));
		return;
	}

	for (const FBRAbilitySetEntry& Entry : Abilities)
	{
		if (Entry.Ability.IsNull())
		{
			UE_LOG(LogBRCombat, Error, TEXT("UBRAbilitySet '%s': an ability row has no class; skipped. (Editor validation reports this row.)"), *GetName());
			continue;
		}

		const bool bWasResident = Entry.Ability.IsValid();
		UClass* AbilityClass = Entry.Ability.LoadSynchronous();
		if (!AbilityClass)
		{
			UE_LOG(LogBRCombat, Error, TEXT("UBRAbilitySet '%s': ability class '%s' failed to load; skipped."), *GetName(), *Entry.Ability.ToString());
			continue;
		}

		if (!bWasResident)
		{
			// Named, so an equip hitch has a cause. A set whose classes are always cold is a
			// preloading problem, not a reason to hold hard references.
			UE_LOG(LogBRCombat, Warning, TEXT("UBRAbilitySet '%s': synchronously loaded ability class '%s' at grant time."), *GetName(), *AbilityClass->GetName());
		}

		FGameplayAbilitySpec Spec(AbilityClass, Entry.AbilityLevel, /*InputID=*/INDEX_NONE, SourceObject);

		// THE BINDING. The InputTag lives on the spec's dynamic source tags, which is exactly where
		// UBRAbilitySystemComponent::AbilityInputTagPressed looks and where GAS replicates it from.
		// An empty tag is legal (a passive) and is not stamped, because an invalid tag in that
		// container would match an invalid press.
		if (Entry.InputTag.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		}

		OutAbilityHandles.Add(ASC->GiveAbility(Spec));
	}

	for (const FBRAbilitySetEffectEntry& Entry : Effects)
	{
		if (Entry.Effect.IsNull())
		{
			UE_LOG(LogBRCombat, Error, TEXT("UBRAbilitySet '%s': an effect row has no class; skipped."), *GetName());
			continue;
		}

		UClass* EffectClass = Entry.Effect.LoadSynchronous();
		if (!EffectClass)
		{
			UE_LOG(LogBRCombat, Error, TEXT("UBRAbilitySet '%s': effect class '%s' failed to load; skipped."), *GetName(), *Entry.Effect.ToString());
			continue;
		}

		const UGameplayEffect* EffectCDO = EffectClass->GetDefaultObject<UGameplayEffect>();
		if (!EffectCDO)
		{
			continue;
		}

		OutEffectHandles.Add(ASC->ApplyGameplayEffectToSelf(EffectCDO, Entry.EffectLevel, ASC->MakeEffectContext()));
	}
}

void UBRAbilitySet::TakeFromAbilitySystem(UAbilitySystemComponent* ASC, TArray<FGameplayAbilitySpecHandle>& AbilityHandles, TArray<FActiveGameplayEffectHandle>& EffectHandles)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		// The arrays are NOT cleared on refusal: dropping the handles on a non-authority would lose
		// the only record of what the server granted, and the next grant would stack on top of it.
		return;
	}

	for (const FActiveGameplayEffectHandle& Handle : EffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	EffectHandles.Reset();

	for (const FGameplayAbilitySpecHandle& Handle : AbilityHandles)
	{
		if (Handle.IsValid())
		{
			// ClearAbility, not ClearAbilityInput: this removes the grant. An ability mid-activation
			// is ended by the engine as part of this.
			ASC->ClearAbility(Handle);
		}
	}
	AbilityHandles.Reset();
}

#if WITH_EDITOR
EDataValidationResult UBRAbilitySet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TSet<FGameplayTag> SeenInputTags;

	for (int32 Index = 0; Index < Abilities.Num(); ++Index)
	{
		const FBRAbilitySetEntry& Entry = Abilities[Index];

		if (Entry.Ability.IsNull())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Ability row %d has no class — it would be silently skipped at grant time."), Index)));
			Result = EDataValidationResult::Invalid;
		}

		if (Entry.InputTag.IsValid())
		{
			if (SeenInputTags.Contains(Entry.InputTag))
			{
				// Two abilities on one InputTag both activate on the same press. Occasionally that
				// is intended; far more often it is a copy-paste, and it is invisible at runtime.
				Context.AddWarning(FText::FromString(FString::Printf(TEXT("InputTag '%s' appears on more than one ability in this set; every one of them will activate on the same press."), *Entry.InputTag.ToString())));
			}
			SeenInputTags.Add(Entry.InputTag);
		}

		if (Entry.AbilityLevel < 1)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Ability row %d has level %d; levels start at 1."), Index, Entry.AbilityLevel)));
			Result = EDataValidationResult::Invalid;
		}
	}

	for (int32 Index = 0; Index < Effects.Num(); ++Index)
	{
		if (Effects[Index].Effect.IsNull())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Effect row %d has no class."), Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
