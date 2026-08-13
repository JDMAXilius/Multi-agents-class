#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Core/BNGameplayTags.h"
#include "BreachpointNext.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

void UBNAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, MaxShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, SprintSpeedMultiplier, COND_None, REPNOTIFY_Always);
}

void UBNAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamped at BOTH ends now. The floor was always here; the ceiling could not exist until
	// MaxHealth/MaxShield did, which is why a recharge would previously have climbed forever.
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxShield());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxShieldAttribute())
	{
		// A max of zero would divide the UI by zero and pin the pool shut.
		NewValue = FMath::Max(NewValue, UE_KINDA_SMALL_NUMBER);
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

	// Slam the recharge window shut. This is the ONLY thing that stops the shield coming back, and
	// it lives here rather than in the damage door because it must fire for every point of damage
	// this set ever drains, whatever applied it. Re-applying refreshes the duration, so sustained
	// fire holds the shield down for as long as it keeps landing.
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UBNGE_RecentDamage::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			// Tag on the SPEC, not the CDO — the construction-order rule again.
			Spec.Data->DynamicGrantedTags.AddTag(BNTags::State_Combat_RecentDamage);
			Spec.Data->SetSetByCallerMagnitude(BNSetByCaller::RecentDamageWindow, ShieldRechargeDelay);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
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

void UBNAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBNAttributeSet, MaxHealth, OldMaxHealth);
}

void UBNAttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBNAttributeSet, MaxShield, OldMaxShield);
}

void UBNAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBNAttributeSet, MoveSpeed, OldMoveSpeed);
}

void UBNAttributeSet::OnRep_SprintSpeedMultiplier(const FGameplayAttributeData& OldSprintSpeedMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBNAttributeSet, SprintSpeedMultiplier, OldSprintSpeedMultiplier);
}
