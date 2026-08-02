#include "AbilitySystem/BRAttributeSet.h"

#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"

namespace
{
	TSet<FName>& GetUninitializedShieldsReportLedger()
	{
		static TSet<FName> Ledger;
		return Ledger;
	}
}

UBRAttributeSet::UBRAttributeSet()
{
}

void UBRAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, Shields, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, MaxShields, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, Grenades, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, MaxGrenades, COND_OwnerOnly, REPNOTIFY_Always);

	// COND_None, not OwnerOnly: a simulated proxy's CMC needs the same speed the server used or
	// its local prediction of another player's movement drifts.
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, MoveSpeedBase, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBRAttributeSet, SprintSpeedMultiplier, COND_None, REPNOTIFY_Always);
}

void UBRAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamped, not merely non-negative: a runaway multiplier from a stacking buff is a pawn that
	// outruns its own replication, and zero base speed is a pawn frozen with no error anywhere.
	if (Attribute == GetMoveSpeedBaseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 5000.f);
	}
	else if (Attribute == GetSprintSpeedMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 5.f);
	}

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetShieldsAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxShields());
	}
	else if (Attribute == GetGrenadesAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxGrenades());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxShieldsAttribute() || Attribute == GetMaxGrenadesAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

void UBRAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float RawDamage = GetIncomingDamage();
		SetIncomingDamage(0.f);

		if (RawDamage < 0.f)
		{
			return;
		}

		if (RawDamage == 0.f)
		{
			return;
		}

		const float OverflowToHealth = ApplyIncomingDamageShieldsFirst(RawDamage);

		if (UBRAbilitySystemComponent* BRASC = Cast<UBRAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
		{
			BRASC->ApplyRecentDamageGate();
		}

		UpdateShieldsBrokenState();
		CheckForDeath(Data);
		return;
	}

	if (Data.EvaluatedData.Attribute == GetShieldsAttribute())
	{
		UpdateShieldsBrokenState();
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		CheckForDeath(Data);
		return;
	}
}

float UBRAttributeSet::ApplyIncomingDamageShieldsFirst(float RawDamage)
{
	const float ShieldsBefore = GetShields();
	const float AbsorbedByShields = FMath::Min(ShieldsBefore, RawDamage);
	const float OverflowToHealth = RawDamage - AbsorbedByShields;

	SetShields(ShieldsBefore - AbsorbedByShields);

	if (OverflowToHealth > 0.f)
	{
		SetHealth(FMath::Max(GetHealth() - OverflowToHealth, 0.f));
	}

	return OverflowToHealth;
}

void UBRAttributeSet::UpdateShieldsBrokenState()
{
	UBRAbilitySystemComponent* BRASC = Cast<UBRAbilitySystemComponent>(GetOwningAbilitySystemComponent());
	if (!BRASC)
	{
		return;
	}

	if (GetMaxShields() <= 0.f)
	{
		const AActor* OwningActor = GetOwningActor();
		const FName LedgerKey = OwningActor ? FName(*OwningActor->GetPathName()) : FName(TEXT("<no owning actor>"));

		TSet<FName>& Ledger = GetUninitializedShieldsReportLedger();
		if (!Ledger.Contains(LedgerKey))
		{
			Ledger.Add(LedgerKey);
		}

		return;
	}

	BRASC->SetShieldsBrokenState(GetShields() <= 0.f);
}

void UBRAttributeSet::CheckForDeath(const FGameplayEffectModCallbackData& Data)
{
	if (GetHealth() > 0.f)
	{
		bDeathReported = false;
		return;
	}

	if (GetMaxHealth() <= 0.f)
	{
		return;
	}

	if (bDeathReported)
	{
		return;
	}

	bDeathReported = true;

	AActor* Victim = GetOwningActor();
	const FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	AActor* Instigator = Context.GetOriginalInstigator();
	AActor* Causer = Context.GetEffectCauser();

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		FGameplayEventData Payload;
		Payload.EventTag = BRGameplayTags::Event_Death;
		Payload.Instigator = Instigator;
		Payload.Target = Victim;
		Payload.ContextHandle = Context;
		Payload.EventMagnitude = Data.EvaluatedData.Magnitude;

		ASC->HandleGameplayEvent(BRGameplayTags::Event_Death, &Payload);
	}

	OnDeath.Broadcast(Victim, Instigator, Causer, Data.EffectSpec);
}

void UBRAttributeSet::OnRep_Shields(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, Shields, OldValue);
}

void UBRAttributeSet::OnRep_MaxShields(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, MaxShields, OldValue);
}

void UBRAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, Health, OldValue);
}

void UBRAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, MaxHealth, OldValue);
}

void UBRAttributeSet::OnRep_Grenades(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, Grenades, OldValue);
}

void UBRAttributeSet::OnRep_MaxGrenades(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, MaxGrenades, OldValue);
}

void UBRAttributeSet::OnRep_MoveSpeedBase(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, MoveSpeedBase, OldValue);
}

void UBRAttributeSet::OnRep_SprintSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBRAttributeSet, SprintSpeedMultiplier, OldValue);
}
