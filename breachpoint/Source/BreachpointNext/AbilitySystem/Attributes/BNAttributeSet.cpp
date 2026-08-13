#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "BreachpointNext.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

void UBNAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, SprintSpeedMultiplier, COND_None, REPNOTIFY_Always);
}

void UBNAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute() || Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMoveSpeedAttribute() || Attribute == GetSprintSpeedMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, UE_KINDA_SMALL_NUMBER);
	}
}

// Instant GEs execute on the AUTHORITY only, so this whole path is server-side; what reaches the
// other machines is the replicated Health/Shield the drain leaves behind.
void UBNAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute != GetIncomingDamageAttribute())
	{
		return;
	}

	const float Damage = GetIncomingDamage();
	SetIncomingDamage(0.f);
	if (Damage <= 0.f)
	{
		return;
	}

	const float ShieldBefore = GetShield();
	const float HealthBefore = GetHealth();

	// Shield first, then the remainder into Health. Neither goes below zero. That is the entire
	// rule set — armour, damage types and falloff are the later pipeline's, not this wave's.
	const float FromShield = FMath::Min(ShieldBefore, Damage);
	SetShield(FMath::Max(0.f, ShieldBefore - FromShield));
	if (Damage > FromShield)
	{
		SetHealth(FMath::Max(0.f, HealthBefore - (Damage - FromShield)));
	}

	// THE TEST. The roadmap asks for exactly this line and nothing else logs damage.
	UE_LOG(LogBN, Log, TEXT("BNDamage: %s -> %s, %.1f | shield %.0f -> %.0f | health %.0f -> %.0f"),
		*GetNameSafe(Data.EffectSpec.GetEffectContext().GetOriginalInstigator()),
		*GetNameSafe(Data.Target.GetAvatarActor()),
		Damage, ShieldBefore, GetShield(), HealthBefore, GetHealth());
}

void UBNAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBNAttributeSet, Health, OldHealth);
}

void UBNAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBNAttributeSet, Shield, OldShield);
}

void UBNAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBNAttributeSet, MoveSpeed, OldMoveSpeed);
}

void UBNAttributeSet::OnRep_SprintSpeedMultiplier(const FGameplayAttributeData& OldSprintSpeedMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBNAttributeSet, SprintSpeedMultiplier, OldSprintSpeedMultiplier);
}
