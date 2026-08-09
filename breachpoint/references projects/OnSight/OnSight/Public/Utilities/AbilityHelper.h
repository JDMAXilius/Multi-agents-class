#pragma once
// Helpers: resolve ability C++ vs BP class, apply FOSGameplayEffectWrapper to a spec. Used by cues and abilities.
#include "AbilitySystemGlobals.h"
#include "Data/OSChooserData.h"
#include "Data/OSAbilityCostAndEffects.h"
#include "Data/OSGameplayTags.h"
#include "Gas/Attributes/OSAttributeSet.h"
#include "GAS/Abilities/OSGameplayAbility.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Characters/OSCharacter.h"
#include "Data/OSHitDamageContext.h"

namespace AbilityHelper
{

	/** Walks up from abilityClass to find native C++ base; returns {NativeClass, BPClass} for cue/chooser lookups. */
	static FAbilityClassMapping ResolveAbilityClass(TSubclassOf<UOSGameplayAbility> abilityClass)
	{
		UClass* BPClass     = abilityClass;
		UClass* NativeClass = abilityClass;

		while (NativeClass &&
			   !NativeClass->HasAnyClassFlags(CLASS_Native))
		{
			NativeClass = NativeClass->GetSuperClass();
		}

		// If no native found (pure C++ case), just use the class itself
		if (!NativeClass)
		{
			NativeClass = BPClass;
		}
		
		return {NativeClass, BPClass};
	}

	/** Builds spec from EffectWrapper.Effect, sets SetByCaller Data.Damage when potency enabled; applies to Source and/or Target per wrapper flags. */
	static void ApplyEffect(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, const FGameplayEffectContextHandle& Context, const FOSGameplayEffectWrapper& EffectWrapper)
	{
		if (!EffectWrapper.IsValid() || !SourceASC) return; 
		auto EffectClass = EffectWrapper.Effect;

		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
		if (!SpecHandle.IsValid()) return;
		
		const auto& Tags = FOSGameplayTags::Get();
		
		if (EffectWrapper.Potency.bEnabled)
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Damage, EffectWrapper.Potency.Value);

		if (EffectWrapper.Duration.bEnabled)
			SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_EffectDuration, EffectWrapper.Duration.Value);

		if (EffectWrapper.Period.bEnabled)
			SpecHandle.Data->Period = EffectWrapper.Period.Value;
		
		if (EffectWrapper.bApplyToOther && TargetASC) TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		if (EffectWrapper.bApplyToSelf && SourceASC)  SourceASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	// Overload for ApplyEffect that takes in Actors instead.
	static void ApplyEffect(AActor* SourceActor, AActor* TargetActor, const FGameplayEffectContextHandle& Context, const FOSGameplayEffectWrapper& EffectWrapper)
	{
		if (!SourceActor || !EffectWrapper.IsValid()) return; 
		UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(SourceActor);
		if (!SourceASC) return;
		auto TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
		
		ApplyEffect(SourceASC, TargetASC, Context, EffectWrapper);
	}
	
	static FGameplayEffectContextHandle BuildEffectContext(UAbilitySystemComponent* SourceASC, AOSCharacter* Source, AOSCharacter* Target, EOSAttackType AttackType)
	{
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddSourceObject(Source);

		FOSGameplayEffectContext* OSCtx = nullptr;
		if (TryGetOSGameplayEffectContext(Context, OSCtx))
		{
			OSCtx->HitInfo.AttackInfo.Type = AttackType;
			OSCtx->HitInfo.HitLocation = Target ? Target->GetActorLocation() : FVector::ZeroVector;

			if (APlayerState* PS = Source ? Source->GetPlayerState<APlayerState>() : nullptr)
				OSCtx->HitInfo.AttackInfo.InstigatorPlayerStateUniqueId = PS->GetUniqueId();

			if (APlayerState* PS = Target ? Target->GetPlayerState<APlayerState>() : nullptr)
				OSCtx->HitInfo.AttackInfo.VictimPlayerStateUniqueId = PS->GetUniqueId();
		}

		return Context;
	}
	
	static FGameplayEffectContextHandle BuildEffectContext(AOSCharacter* Source, AOSCharacter* Target, EOSAttackType AttackType)
	{
		if (!Source) return FGameplayEffectContextHandle();
		UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source);
		if (!SourceASC) return FGameplayEffectContextHandle();
		return BuildEffectContext(SourceASC, Source, Target, AttackType);
	}
	
	/** Resolves ASC from Actor: IAbilitySystemInterface first, else AbilitySystemGlobals fallback. Returns UOSAbilitySystemComponent*. */
	static UOSAbilitySystemComponent* GetOSASCFromActor(const AActor* Actor)
	{
		if (!Actor) return nullptr;
		
		if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
		{
			auto asc = ASI->GetAbilitySystemComponent();
			auto osASC = asc ? Cast<UOSAbilitySystemComponent>(asc) : nullptr;
			return osASC;
		}
		
		auto asc =  UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
		auto osASC = asc ? Cast<UOSAbilitySystemComponent>(asc) : nullptr;
		return osASC;
	}

	/** Maps Health/Aura/Stamina attribute to the corresponding Data.Cost.* tag for ability cost checks. */
	static FGameplayTag MapAttributeToCostTag(const FGameplayAttribute& Attr)
	{
		const auto& Tags = FOSGameplayTags::Get();

		if (Attr == UOSAttributeSet::GetAuraAttribute())
			return Tags.Data_Cost_Aura;

		if (Attr == UOSAttributeSet::GetStaminaAttribute())
			return Tags.Data_Cost_Stamina;

		if (Attr == UOSAttributeSet::GetHealthAttribute())
			return Tags.Data_Cost_Health;

		return FGameplayTag(); // invalid
	}
}

