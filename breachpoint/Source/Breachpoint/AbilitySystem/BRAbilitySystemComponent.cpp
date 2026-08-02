#include "AbilitySystem/BRAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEffect.h"
#include "GameplayPrediction.h"

#include "AbilitySystem/Abilities/BRGameplayAbility.h"
#include "AbilitySystem/BRAttributeSet.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"

namespace
{
	const FName FighterMaxGrenadesCurve(TEXT("Fighter.MaxGrenades"));

	TSet<FName>& GetUnmatchedInputTagLedger()
	{
		static TSet<FName> Ledger;
		return Ledger;
	}
}

UBRAbilitySystemComponent::UBRAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RecentDamageEffectClass = UBRGE_RecentDamage::StaticClass();
	ShieldsBrokenEffectClass = UBRGE_ShieldsBroken::StaticClass();
	InitStatsEffectClass = UBRGE_InitStats::StaticClass();
	ShieldRegenEffectClass = UBRGE_Regen::StaticClass();
	DeathEffectClass = UBRGE_Death::StaticClass();

	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	SetIsReplicatedByDefault(true);
}

void UBRAbilitySystemComponent::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	const int32 NumBeforeAdd = HeldInputTags.Num();
	HeldInputTags.AddUnique(InputTag);
	if (HeldInputTags.Num() == NumBeforeAdd)
	{
		return;
	}

	int32 MatchedSpecs = 0;

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		++MatchedSpecs;
		Spec.InputPressed = true;

		if (Spec.IsActive())
		{
			if (Spec.Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
			{
				ServerSetInputPressed(Spec.Handle);
			}

			AbilitySpecInputPressed(Spec);
			InvokeInputEventForSpec(Spec, EAbilityGenericReplicatedEvent::InputPressed);
		}
		else
		{
			TryActivateAbility(Spec.Handle);
		}
	}

	if (MatchedSpecs > 0)
	{
		return;
	}

	const FName LedgerKey(*FString::Printf(TEXT("%s|%s"), *GetNameSafe(GetOwner()), *InputTag.ToString()));
	TSet<FName>& Ledger = GetUnmatchedInputTagLedger();

	// The ledger dedups per owner+tag so each distinct dead verb reports once rather than on
	// every keypress; ensureAlways because one site serves every tag.
	if (!Ledger.Contains(LedgerKey))
	{
		Ledger.Add(LedgerKey);
		ensureAlwaysMsgf(false, TEXT("BRAbilitySystemComponent on '%s': input tag '%s' reached the ASC but no granted ability spec carries it. Either the startup loadout was never granted, or no entry in the ability set names this InputTag."),
			*GetNameSafe(GetOwner()), *InputTag.ToString());
	}
}

void UBRAbilitySystemComponent::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	if (HeldInputTags.Remove(InputTag) == 0)
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		Spec.InputPressed = false;

		if (Spec.IsActive())
		{
			if (Spec.Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
			{
				ServerSetInputReleased(Spec.Handle);
			}

			AbilitySpecInputReleased(Spec);
			InvokeInputEventForSpec(Spec, EAbilityGenericReplicatedEvent::InputReleased);
		}
	}
}

void UBRAbilitySystemComponent::InvokeInputEventForSpec(const FGameplayAbilitySpec& Spec, EAbilityGenericReplicatedEvent::Type EventType)
{
	const TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
	if (Instances.IsEmpty() || !Instances.Last())
	{
		return;
	}

	InvokeReplicatedEvent(EventType, Spec.Handle, Instances.Last()->GetCurrentActivationInfoRef().GetActivationPredictionKey());
}

void UBRAbilitySystemComponent::ClearAbilityInput()
{
	if (HeldInputTags.IsEmpty())
	{
		return;
	}

	const TArray<FGameplayTag> TagsToRelease = HeldInputTags;

	for (const FGameplayTag& Tag : TagsToRelease)
	{
		AbilityInputTagReleased(Tag);
	}

	HeldInputTags.Reset();
}

void UBRAbilitySystemComponent::ExecuteInPredictionWindow(TFunctionRef<void()> Work)
{
	FScopedPredictionWindow PredictionWindow(this, true);
	Work();
}

bool UBRAbilitySystemComponent::BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle AbilityHandle, bool bEndAbilityImmediately)
{
	if (!AbilityHandle.IsValid())
	{
		return false;
	}

	FScopedServerAbilityRPCBatcher Batcher(this, AbilityHandle);

	const bool bActivated = TryActivateAbility(AbilityHandle, true);

	if (bActivated && bEndAbilityImmediately)
	{
		if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle))
		{
			if (UBRGameplayAbility* BRAbility = Cast<UBRGameplayAbility>(Spec->GetPrimaryInstance()))
			{
				BRAbility->ExternalEndAbility();
			}
		}
	}

	return bActivated;
}

bool UBRAbilitySystemComponent::ApplyRecentDamageGate()
{
	if (!RecentDamageEffectClass)
	{
		return false;
	}

	float DelaySeconds = 0.f;
	if (!BRCombatCurves::Evaluate(BRCombatCurves::Names::ShieldsRegenDelaySeconds, DelaySeconds) || DelaySeconds <= 0.f)
	{
		return false;
	}

	const FGameplayEffectContextHandle Context = MakeEffectContext();
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(RecentDamageEffectClass, 1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_RecentDamage::DurationSetByCallerName, DelaySeconds);

	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}

bool UBRAbilitySystemComponent::HasActiveEffectOfClass(TSubclassOf<UGameplayEffect> EffectClass) const
{
	if (!EffectClass)
	{
		return false;
	}

	FGameplayEffectQuery Query;
	Query.EffectDefinition = EffectClass;
	return GetActiveEffects(Query).Num() > 0;
}

void UBRAbilitySystemComponent::SetShieldsBrokenState(bool bBroken)
{
	if (!ShieldsBrokenEffectClass)
	{
		return;
	}

	const bool bCurrentlyBroken = HasActiveEffectOfClass(ShieldsBrokenEffectClass);
	if (bCurrentlyBroken == bBroken)
	{
		return;
	}

	if (bBroken)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(ShieldsBrokenEffectClass, 1.f, MakeEffectContext());
		if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
		{
			ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
	else
	{
		RemoveActiveGameplayEffectBySourceEffect(ShieldsBrokenEffectClass, this);
	}
}

bool UBRAbilitySystemComponent::ApplyInitStats()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	if (!InitStatsEffectClass)
	{
		return false;
	}

	float MaxHealth = 0.f;
	float MaxShields = 0.f;
	float MaxGrenades = 0.f;
	const bool bHaveHealth = BRCombatCurves::Evaluate(BRCombatCurves::Names::FighterMaxHealth, MaxHealth);
	const bool bHaveShields = BRCombatCurves::Evaluate(BRCombatCurves::Names::FighterMaxShields, MaxShields);
	const bool bHaveGrenades = BRCombatCurves::Evaluate(FighterMaxGrenadesCurve, MaxGrenades);

	if (!bHaveHealth || !bHaveShields || MaxHealth <= 0.f)
	{
		return false;
	}

	if (!bHaveGrenades || MaxGrenades < 0.f)
	{
		return false;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(InitStatsEffectClass, 1.f, MakeEffectContext());
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::MaxHealthName, MaxHealth);
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::MaxShieldsName, MaxShields);
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::MaxGrenadesName, MaxGrenades);
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::HealthName, MaxHealth);
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::ShieldsName, MaxShields);
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::GrenadesName, MaxGrenades);

	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	if (const UBRAttributeSet* Attributes = GetSet<UBRAttributeSet>())
	{
	}

	SetShieldsBrokenState(false);

	if (!ShieldRegenEffectClass)
	{
		return true;
	}

	if (HasActiveEffectOfClass(ShieldRegenEffectClass))
	{
		return true;
	}

	float RatePerSecond = 0.f;
	float PeriodSeconds = 0.f;
	if (!BRCombatCurves::Evaluate(BRCombatCurves::Names::ShieldsRegenRatePerSecond, RatePerSecond) || RatePerSecond <= 0.f
		|| !BRCombatCurves::Evaluate(BRCombatCurves::Names::ShieldsRegenPeriodSeconds, PeriodSeconds) || PeriodSeconds <= 0.f)
	{
		return true;
	}

	const FGameplayEffectSpecHandle RegenSpec = MakeOutgoingSpec(ShieldRegenEffectClass, 1.f, MakeEffectContext());
	if (RegenSpec.IsValid() && RegenSpec.Data.IsValid())
	{
		RegenSpec.Data->Period = PeriodSeconds;
		RegenSpec.Data->SetSetByCallerMagnitude(BRGameplayTags::SetByCaller_RegenRate, RatePerSecond * PeriodSeconds);
		ApplyGameplayEffectSpecToSelf(*RegenSpec.Data.Get());
	}

	return true;
}

bool UBRAbilitySystemComponent::ApplyDeathEffect()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	if (!DeathEffectClass)
	{
		return false;
	}

	if (HasActiveEffectOfClass(DeathEffectClass))
	{
		return false;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(DeathEffectClass, 1.f, MakeEffectContext());
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	CancelAbilities(nullptr, nullptr, nullptr);

	return true;
}

void UBRAbilitySystemComponent::ClearDeathEffect()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !DeathEffectClass)
	{
		return;
	}

	RemoveActiveGameplayEffectBySourceEffect(DeathEffectClass, this);
}
