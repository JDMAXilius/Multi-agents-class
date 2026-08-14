#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Core/BNGameplayTags.h"
#include "BreachpointNext.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

void UBNAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// THE MAXES FIRST, and for the same reason the init GE sets them first: properties replicate in
	// registration order, so a joining client that received Health before MaxHealth would clamp it
	// against a max still at zero and render a live player as dead. PreAttributeChange's zero-max
	// guard is the belt to this braces — order is not a hard guarantee across bunches.
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, MaxShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, SprintSpeedMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBNAttributeSet, ADSSpeedMultiplier, COND_None, REPNOTIFY_Always);
}

void UBNAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamped at BOTH ends now. The floor was always here; the ceiling could not exist until
	// MaxHealth/MaxShield did, which is why a recharge would previously have climbed forever.
	// A max of zero means "not initialised yet", NOT "this pool is empty". Clamping against it
	// would zero a live player: on a joining client MaxHealth may not have arrived when Health
	// does, and on the server the init GE's first modifier lands before its second. Floor only
	// until a real ceiling exists.
	if (Attribute == GetHealthAttribute())
	{
		const float Max = GetMaxHealth();
		NewValue = Max > 0.f ? FMath::Clamp(NewValue, 0.f, Max) : FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetShieldAttribute())
	{
		const float Max = GetMaxShield();
		NewValue = Max > 0.f ? FMath::Clamp(NewValue, 0.f, Max) : FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		// Health's max may never be zero — everyone would spawn dead against it.
		NewValue = FMath::Max(NewValue, UE_KINDA_SMALL_NUMBER);
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		// ZERO IS LEGAL HERE, and the difference from MaxHealth is load-bearing: MaxShield=0 is
		// the shields-off configuration, and every shield gate asks GetMaxShield() > 0. The critic
		// caught the epsilon floor turning that deliberate 0 into 1e-4 — which made HasShieldPool()
		// true, applied the recharge at every spawn, a RecentDamage GE per bullet, and raised
		// State.Shields.Broken on a pool that was supposed to not exist. Floor at zero, not above it.
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMoveSpeedAttribute() || Attribute == GetSprintSpeedMultiplierAttribute() || Attribute == GetADSSpeedMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, UE_KINDA_SMALL_NUMBER);
	}
}

// The BASE value's clamp — the critic's catch. PreAttributeChange clamps only the CURRENT value;
// the periodic recharge is an Additive on Shield's BASE, so at full shield the base climbed +10
// every 0.1s forever, dirtying the replicated attribute every period for every idle player —
// constant churn to all clients doing nothing. Same rules as the current-value clamp, one layer down.
void UBNAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		const float Max = GetMaxHealth();
		NewValue = Max > 0.f ? FMath::Clamp(NewValue, 0.f, Max) : FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetShieldAttribute())
	{
		const float Max = GetMaxShield();
		NewValue = Max > 0.f ? FMath::Clamp(NewValue, 0.f, Max) : FMath::Max(NewValue, 0.f);
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

	// THE CAPTURE, before the drain. This is the one point every damage passes through, and the
	// spec's context is where the door put the instigator and the server-validated hit. Refreshed
	// on every landed hit — not just the lethal one — because the hit-reaction packet needs the
	// latest hit's direction, while kill credit only ever reads the final state.
	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetEffectContext();
	LastDamage.Instigator = Context.GetOriginalInstigator();
	LastDamage.Hit = Context.GetHitResult() ? *Context.GetHitResult() : FHitResult();
	LastDamage.Amount = Damage;

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
	//
	// The shields-off skip is GONE, on the schedule its own comment set: "IF State.Combat.
	// RecentDamage ever gains a second reader, this gate has to go with it." Descope is that
	// reader — UBNGA_ADS cancels itself when this tag arrives, shields or no shields — so the
	// window now applies on every landed hit unconditionally.
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (ASC)
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

void UBNAttributeSet::OnRep_ADSSpeedMultiplier(const FGameplayAttributeData& OldADSSpeedMultiplier)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBNAttributeSet, ADSSpeedMultiplier, OldADSSpeedMultiplier);
}
