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
		// Already held, so this press is swallowed. Legitimate for an auto-repeating trigger,
		// but it is also what a lost release looks like: once a WhileInputHeld tag is stuck in
		// the buffer, every later press of that key is silently dead.
		UE_LOG(LogBRAbility, Log, TEXT("INPUT SWALLOWED: '%s' was already held on '%s'; no release ever cleared it."),
			*InputTag.ToString(), *GetNameSafe(GetOwner()));
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

			UE_LOG(LogBRAbility, Log, TEXT("INPUT TO ACTIVE: '%s' -> %s was already running; press forwarded, not re-activated."),
				*InputTag.ToString(), *GetNameSafe(Spec.Ability->GetClass()));
		}
		else
		{
			// THE RESULT WAS PREVIOUSLY DISCARDED, and that was the blind spot: a refused
			// activation and a missing spec produced exactly the same silence.
			const bool bActivated = TryActivateAbility(Spec.Handle);

			if (bActivated)
			{
				UE_LOG(LogBRAbility, Log, TEXT("INPUT MATCHED: '%s' -> %s, activated."),
					*InputTag.ToString(), *GetNameSafe(Spec.Ability->GetClass()));
			}
			else
			{
				// Re-ask CanActivateAbility purely for its OptionalRelevantTags out-param, which
				// GAS fills with the REASON - the blocking tag, the missing cost, the live
				// cooldown. Naming a list of suspects is not a diagnostic; this names the cause.
				// The call is side-effect free and only runs on the failure path.
				FGameplayTagContainer FailureTags;
				Spec.Ability->CanActivateAbility(Spec.Handle, AbilityActorInfo.Get(), nullptr, nullptr, &FailureTags);

				FGameplayTagContainer OwnedTags;
				GetOwnedGameplayTags(OwnedTags);

				UE_LOG(LogBRAbility, Warning, TEXT("INPUT REFUSED: '%s' -> %s. Reason tags: [%s]. Tags currently on the owner: [%s]."),
					*InputTag.ToString(), *GetNameSafe(Spec.Ability->GetClass()),
					FailureTags.IsEmpty() ? TEXT("none reported") : *FailureTags.ToStringSimple(),
					OwnedTags.IsEmpty() ? TEXT("none") : *OwnedTags.ToStringSimple());
			}
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
	if (Ledger.Contains(LedgerKey))
	{
		return;
	}
	Ledger.Add(LedgerKey);

	// ONLY when nothing at all is granted. An unmatched tag on an ASC that does hold abilities
	// is ordinary: weapon verbs are granted by UBREquipmentComponent when a weapon is equipped,
	// so pressing reload while empty-handed is a legitimate state and not a wiring fault. The
	// unambiguous failure is an ASC with no specs whatsoever, which means the loadout grant
	// never ran.
	ensureAlwaysMsgf(ActivatableAbilities.Items.Num() > 0, TEXT("BRAbilitySystemComponent on '%s': input tag '%s' reached the ASC and it holds NO granted abilities at all. The startup loadout never ran."),
		*GetNameSafe(GetOwner()), *InputTag.ToString());
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

	// MOVEMENT, and the fallback is NOT zero - that was a real hole. These modifiers are Override,
	// so passing zero when a curve row is missing does not mean "leave it alone", it actively
	// writes zero over the constructor's default and the pawn loses its speed. A missing row falls
	// back to the C++ default instead, which is the same number the CSV carries.
	float BaseSpeed = BRAttributeDefaults::MoveSpeedBase;
	float SprintMultiplier = BRAttributeDefaults::SprintSpeedMultiplier;

	float CurveBaseSpeed = 0.f;
	if (BRCombatCurves::Evaluate(BRCombatCurves::Names::MovementBaseSpeed, CurveBaseSpeed) && CurveBaseSpeed > 0.f)
	{
		BaseSpeed = CurveBaseSpeed;
	}

	float CurveSprintMultiplier = 0.f;
	if (BRCombatCurves::Evaluate(BRCombatCurves::Names::MovementSprintSpeedMultiplier, CurveSprintMultiplier) && CurveSprintMultiplier > 0.f)
	{
		SprintMultiplier = CurveSprintMultiplier;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::MoveSpeedBaseName, BaseSpeed);
	SpecHandle.Data->SetSetByCallerMagnitude(UBRGE_InitStats::SprintSpeedMultiplierName, SprintMultiplier);

	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

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
