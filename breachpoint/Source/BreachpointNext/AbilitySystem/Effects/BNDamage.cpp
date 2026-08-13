#include "AbilitySystem/Effects/BNDamage.h"

#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "BreachpointNext.h"
#include "Data/BNDataRows.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

namespace
{
	// The mannequin's head bone. The row's multiplier is the only damage RULE that exists.
	const FName BNHeadBone(TEXT("head"));
}

UBNGE_Damage::UBNGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat Magnitude;
	Magnitude.DataName = BNSetByCaller::Damage;

	FGameplayModifierInfo Modifier;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.Attribute = UBNAttributeSet::GetIncomingDamageAttribute();
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	Modifiers.Add(Modifier);
}

void BNDamage::ApplyDamage(AActor* Instigator, AActor* Target, float Amount, const FHitResult& Hit)
{
	if (!IsValid(Target) || Amount <= 0.f)
	{
		return;
	}

	if (!Target->HasAuthority())
	{
		UE_LOG(LogBN, Error, TEXT("BNDamage: REFUSED — %s -> %s, %.1f asked for on a non-authority machine. Damage is the server's alone."),
			*GetNameSafe(Instigator), *GetNameSafe(Target), Amount);
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!TargetASC)
	{
		return;
	}

	// The SOURCE builds the spec when it has an ASC, so the context carries a real instigator for
	// the killfeed and the scoring a later roadmap will hang off it; self-damage falls back to the
	// target's own ASC, which is the cheat manager's case.
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Instigator);
	UAbilitySystemComponent* SpecSource = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = SpecSource->MakeEffectContext();
	if (IsValid(Instigator))
	{
		Context.AddInstigator(Instigator, Instigator);
	}
	if (Hit.bBlockingHit)
	{
		Context.AddHitResult(Hit);
	}

	const FGameplayEffectSpecHandle Spec = SpecSource->MakeOutgoingSpec(UBNGE_Damage::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(BNSetByCaller::Damage, Amount);
	TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void BNDamage::ApplyWeaponDamage(AActor* Instigator, const FBNWeaponRow& Row, const FHitResult& Hit)
{
	const float Amount = Row.Damage * (Hit.BoneName == BNHeadBone ? Row.HeadshotMultiplier : 1.f);
	ApplyDamage(Instigator, Hit.GetActor(), Amount, Hit);
}
