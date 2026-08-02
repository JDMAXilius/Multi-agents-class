// Breachpoint. The ASC: input-buffered tag activation, prediction-window helpers.
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
	if (!BRGas::IsStageEnabled(EBRGasStage::InputRouted))
	{
		return;
	}

	if (!InputTag.IsValid())
	{
		UE_LOG(LogBRInput, Warning, TEXT("BRAbilitySystemComponent '%s': AbilityInputTagPressed with an INVALID tag; ignored."),
			*GetNameSafe(GetOwner()));
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

	if (!Ledger.Contains(LedgerKey))
	{
		Ledger.Add(LedgerKey);
		UE_LOG(LogBRInput, Warning,
			TEXT("BRAbilitySystemComponent '%s': %s PRESSED and matched NO granted ability (%d ability(ies) granted on this ASC in total). The key is ALIVE and the tag reached the ASC — nothing on this fighter answers to that tag. Look at the GRANT (the equipped weapon's ability set, or the InputTag on its entries), not at the input bindings. Reported once per owner+tag; repeats are Verbose."),
			*GetNameSafe(GetOwner()), *InputTag.ToString(), ActivatableAbilities.Items.Num());
	}
	else
	{
	}
}

void UBRAbilitySystemComponent::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (!BRGas::IsStageEnabled(EBRGasStage::InputRouted))
	{
		return;
	}

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
		UE_LOG(LogBRInput, Warning, TEXT("BRAbilitySystemComponent '%s': active ability '%s' has no instance; input event not invoked. Non-instanced abilities are not supported."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Spec.Ability));
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
			else
			{
				UE_LOG(LogBRCombat, Warning, TEXT("BRAbilitySystemComponent '%s': bEndAbilityImmediately requested for an ability that is not a UBRGameplayAbility; NOT ended. Only our base has a public external end."),
					*GetNameSafe(GetOwner()));
			}
		}
	}

	return bActivated;
}

bool UBRAbilitySystemComponent::ApplyRecentDamageGate()
{
	if (!RecentDamageEffectClass)
	{
		UE_LOG(LogBRCombat, Warning, TEXT("BRAbilitySystemComponent '%s': damage landed but RecentDamageEffectClass is UNSET — State.Combat.RecentDamage was NOT applied and shield regen is UNGATED."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	float DelaySeconds = 0.f;
	if (!BRCombatCurves::Evaluate(BRCombatCurves::Names::ShieldsRegenDelaySeconds, DelaySeconds) || DelaySeconds <= 0.f)
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': CT_Combat has no usable '%s' curve; the RecentDamage gate was NOT applied. Shield regen is UNGATED."),
			*GetNameSafe(GetOwner()), *BRCombatCurves::Names::ShieldsRegenDelaySeconds.ToString());
		return false;
	}

	const FGameplayEffectContextHandle Context = MakeEffectContext();
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(RecentDamageEffectClass, 1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': failed to build a spec for RecentDamageEffectClass '%s'."),
			*GetNameSafe(GetOwner()), *GetNameSafe(RecentDamageEffectClass));
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
		UE_LOG(LogBRCombat, Warning, TEXT("BRAbilitySystemComponent '%s': ShieldsBrokenEffectClass is UNSET — State.Shields.Broken is never applied."), *GetNameSafe(GetOwner()));
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
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': ApplyInitStats without authority — REFUSED. Attribute initialisation is server truth."), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!InitStatsEffectClass)
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': InitStatsEffectClass is UNSET; attributes stay at zero."), *GetNameSafe(GetOwner()));
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
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': CT_Combat is missing '%s' or '%s' (read %.2f / %.2f); GE_InitStats NOT applied and this fighter is UNINITIALISED."),
			*GetNameSafe(GetOwner()), *BRCombatCurves::Names::FighterMaxHealth.ToString(), *BRCombatCurves::Names::FighterMaxShields.ToString(), MaxHealth, MaxShields);
		return false;
	}

	if (!bHaveGrenades || MaxGrenades < 0.f)
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': CT_Combat is missing a usable '%s' (read %.2f); GE_InitStats NOT applied and this fighter is UNINITIALISED. The grenade count is data and is not invented here."),
			*GetNameSafe(GetOwner()), *FighterMaxGrenadesCurve.ToString(), MaxGrenades);
		return false;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(InitStatsEffectClass, 1.f, MakeEffectContext());
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': failed to build a GE_InitStats spec."), *GetNameSafe(GetOwner()));
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
	else
	{
		UE_LOG(LogBRCombat, Error,
			TEXT("BRAbilitySystemComponent '%s': GE_InitStats was applied but this ASC has NO UBRAttributeSet registered — the effect modified NOTHING and every attribute is still zero. The set is a PlayerState subobject; this is a construction problem, not a data one."),
			*GetNameSafe(GetOwner()));
	}

	SetShieldsBrokenState(false);

	if (!ShieldRegenEffectClass)
	{
		UE_LOG(LogBRCombat, Warning, TEXT("BRAbilitySystemComponent '%s': ShieldRegenEffectClass is UNSET; shields will never recharge."), *GetNameSafe(GetOwner()));
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
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': CT_Combat is missing a usable '%s' or '%s' (read %.2f / %.2f); GE_Regen NOT applied and shields will never recharge."),
			*GetNameSafe(GetOwner()), *BRCombatCurves::Names::ShieldsRegenRatePerSecond.ToString(), *BRCombatCurves::Names::ShieldsRegenPeriodSeconds.ToString(), RatePerSecond, PeriodSeconds);
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
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': ApplyDeathEffect without authority — REFUSED."), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!DeathEffectClass)
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRAbilitySystemComponent '%s': DeathEffectClass is UNSET; State.Dead is never applied and NOTHING blocks a dead fighter's abilities."), *GetNameSafe(GetOwner()));
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
