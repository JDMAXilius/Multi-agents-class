// Fill out your copyright notice in the Description page of Project Settings.


#include "Utilities/BlueprintLibrary/OSGASBlueprintLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagsManager.h"
#include "Data/OSHitDamageContext.h"

bool UOSGASBlueprintLibrary::GetOSHitInfo(const FGameplayEffectContextHandle& Context, FOSHitNetInfo& OutInfo)
{
	const FOSGameplayEffectContext* OSCtx = nullptr;
	if (TryGetOSGameplayEffectContext(Context, OSCtx))
	{
		OutInfo = OSCtx->HitInfo;
		return true;
	}
	return false;
}

bool UOSGASBlueprintLibrary::GetOSClashInfo(const FGameplayEffectContextHandle& Context, FOSClashInfo& OutInfo)
{
	const FOSGameplayEffectContext* OSCtx = nullptr;
	if (TryGetOSGameplayEffectContext(Context, OSCtx))
	{
		OutInfo = OSCtx->ClashInfo;
		return true;
	}
	return false;
}

bool UOSGASBlueprintLibrary::GetOSCueSocketName( const FGameplayEffectContextHandle& Context, FName& OutInfo )
{
	const FOSGameplayEffectContext* OSCtx = nullptr;
	if (TryGetOSGameplayEffectContext(Context, OSCtx))
	{
		OutInfo = OSCtx->CueSocketName;
		return true;
	}
	return false;
}

bool UOSGASBlueprintLibrary::HasGameplayTagByString(const AActor* Actor, const FString& TagString, bool bExactMatch)
{
	if (!Actor || TagString.IsEmpty()) return false;

	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagString), /*ErrorIfNotFound=*/false);
	if (!Tag.IsValid()) return false;

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return false;

	if (bExactMatch)
	{
		FGameplayTagContainer Owned;
		ASC->GetOwnedGameplayTags(Owned);
		return Owned.HasTagExact(Tag);
	}
	return ASC->HasMatchingGameplayTag(Tag);
}

bool UOSGASBlueprintLibrary::HasAbilityOfClass(const AActor* Actor, TSubclassOf<UGameplayAbility> AbilityClass)
{
	return FindAbilityOfClass(Actor, AbilityClass) != nullptr;
}

FGameplayTag UOSGASBlueprintLibrary::FindGameplayTagFromString(const FString& TagString)
{
	if (TagString.IsEmpty()) return FGameplayTag();
	return UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagString), /*ErrorIfNotFound=*/false);
}

UGameplayAbility* UOSGASBlueprintLibrary::FindAbilityOfClass(const AActor* Actor, TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!Actor || !AbilityClass) return nullptr;

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return nullptr;

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetClass()->IsChildOf(AbilityClass))
		{
			return Spec.Ability;
		}
	}
	return nullptr;
}

// --- Attributes ---

float UOSGASBlueprintLibrary::GetAttributeValue(const AActor* Actor, FGameplayAttribute Attribute)
{
	if (!Actor || !Attribute.IsValid()) return 0.f;
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC || !ASC->HasAttributeSetForAttribute(Attribute)) return 0.f;
	return ASC->GetNumericAttribute(Attribute);
}

float UOSGASBlueprintLibrary::GetAttributeBaseValue(const AActor* Actor, FGameplayAttribute Attribute)
{
	if (!Actor || !Attribute.IsValid()) return 0.f;
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC || !ASC->HasAttributeSetForAttribute(Attribute)) return 0.f;
	return ASC->GetNumericAttributeBase(Attribute);
}

// --- Effects ---

bool UOSGASBlueprintLibrary::HasActiveEffectWithTag(const AActor* Actor, FGameplayTag EffectTag)
{
	if (!Actor || !EffectTag.IsValid()) return false;
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return false;

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(EffectTag));
	return ASC->GetActiveEffects(Query).Num() > 0;
}

float UOSGASBlueprintLibrary::GetActiveEffectTimeRemaining(const AActor* Actor, FGameplayTag EffectTag)
{
	if (!Actor || !EffectTag.IsValid()) return 0.f;
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return 0.f;

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(EffectTag));
	const TArray<float> Remaining = ASC->GetActiveEffectsTimeRemaining(Query);

	float Max = 0.f;
	for (const float T : Remaining)
	{
		if (T < 0.f) return -1.f; // infinite duration wins
		if (T > Max) Max = T;
	}
	return Max;
}

int32 UOSGASBlueprintLibrary::RemoveActiveEffectsWithTag(AActor* Actor, FGameplayTag EffectTag, int32 StacksToRemove)
{
	if (!Actor || !EffectTag.IsValid()) return 0;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return 0;

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(EffectTag));
	return ASC->RemoveActiveEffects(Query, StacksToRemove);
}

// --- Abilities ---

bool UOSGASBlueprintLibrary::TryActivateAbilityByTag(AActor* Actor, FGameplayTagContainer TagContainer, bool bAllowRemoteActivation)
{
	if (!Actor || TagContainer.IsEmpty()) return false;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return false;
	return ASC->TryActivateAbilitiesByTag(TagContainer, bAllowRemoteActivation);
}

void UOSGASBlueprintLibrary::CancelAbilitiesByTag(AActor* Actor, FGameplayTagContainer TagContainer)
{
	if (!Actor || TagContainer.IsEmpty()) return;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return;
	ASC->CancelAbilities(&TagContainer, nullptr, nullptr);
}

TArray<UGameplayAbility*> UOSGASBlueprintLibrary::GetActiveAbilitiesOfClass(const AActor* Actor, TSubclassOf<UGameplayAbility> AbilityClass)
{
	TArray<UGameplayAbility*> Out;
	if (!Actor || !AbilityClass) return Out;

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return Out;

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.Ability->GetClass()->IsChildOf(AbilityClass)) continue;

		const TArray<UGameplayAbility*>& Instances = Spec.GetAbilityInstances();
		if (Instances.Num() > 0)
		{
			for (UGameplayAbility* Inst : Instances)
			{
				if (Inst && Inst->IsActive()) Out.AddUnique(Inst);
			}
		}
		else if (Spec.IsActive())
		{
			Out.AddUnique(Spec.Ability);
		}
	}
	return Out;
}

bool UOSGASBlueprintLibrary::IsAbilityActive(const AActor* Actor, TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!Actor || !AbilityClass) return false;

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return false;

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.Ability->GetClass()->IsChildOf(AbilityClass)) continue;
		if (Spec.IsActive()) return true;
	}
	return false;
}

// --- Tags ---

FGameplayTagContainer UOSGASBlueprintLibrary::GetOwnedGameplayTags(const AActor* Actor)
{
	FGameplayTagContainer Out;
	if (!Actor) return Out;

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return Out;

	ASC->GetOwnedGameplayTags(Out);
	return Out;
}

void UOSGASBlueprintLibrary::AddLocalLooseGameplayTag(AActor* Actor, FGameplayTag Tag)
{
	if (!Actor || !Tag.IsValid()) return;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return;
	ASC->AddLooseGameplayTag(Tag);
}

void UOSGASBlueprintLibrary::RemoveLocalLooseGameplayTag(AActor* Actor, FGameplayTag Tag)
{
	if (!Actor || !Tag.IsValid()) return;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC) return;
	ASC->RemoveLooseGameplayTag(Tag);
}

// --- Context ---

bool UOSGASBlueprintLibrary::GetContextAttackType(const FGameplayEffectContextHandle& Context, EOSAttackType& OutAttackType)
{
	const FOSGameplayEffectContext* OSCtx = nullptr;
	if (TryGetOSGameplayEffectContext(Context, OSCtx))
	{
		OutAttackType = OSCtx->HitInfo.AttackInfo.Type;
		return true;
	}
	return false;
}

AActor* UOSGASBlueprintLibrary::GetContextInstigator(const FGameplayEffectContextHandle& Context)
{
	return Context.IsValid() ? Context.GetInstigator() : nullptr;
}

AActor* UOSGASBlueprintLibrary::GetContextEffectCauser(const FGameplayEffectContextHandle& Context)
{
	return Context.IsValid() ? Context.GetEffectCauser() : nullptr;
}
