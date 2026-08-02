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
		return;
	}

	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FBRAbilitySetEntry& Entry : Abilities)
	{
		if (Entry.Ability.IsNull())
		{
			continue;
		}

		const bool bWasResident = Entry.Ability.IsValid();
		UClass* AbilityClass = Entry.Ability.LoadSynchronous();
		if (!AbilityClass)
		{
			ensureAlwaysMsgf(false, TEXT("BRAbilitySet '%s': ability '%s' failed to load; that verb will be granted nothing."),
				*GetName(), *Entry.Ability.ToSoftObjectPath().ToString());
			continue;
		}

		ensureAlwaysMsgf(Entry.InputTag.IsValid(), TEXT("BRAbilitySet '%s': ability '%s' has no InputTag, so no keypress can ever reach it."),
			*GetName(), *GetNameSafe(AbilityClass));

		FGameplayAbilitySpec Spec(AbilityClass, Entry.AbilityLevel, INDEX_NONE, SourceObject);

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
			continue;
		}

		UClass* EffectClass = Entry.Effect.LoadSynchronous();
		if (!EffectClass)
		{
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
