// Fill out your copyright notice in the Description page of Project Settings.

// Reads SetByCaller Data.Damage; if target blocking, drains stamina then applies overflow to Health. Sets GuardBreak meta when stamina breaks.
#include "GAS/Executions/ExecCalc_OSDamage.h"

#include "Data/OSGameplayTags.h"
#include "GAS/Attributes/OSAttributeSet.h"
#include "OSLogCategories.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

namespace OSDamageExecStatics
{
	static FGameplayEffectAttributeCaptureDefinition TargetStaminaDef()
	{
		// Capture current Stamina from target at execution time (not snapshot).
		return FGameplayEffectAttributeCaptureDefinition(UOSAttributeSet::GetStaminaAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
	}
}

UExecCalc_OSDamage::UExecCalc_OSDamage()
{
	RelevantAttributesToCapture.Add(OSDamageExecStatics::TargetStaminaDef());
}

void UExecCalc_OSDamage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	// Team filter (defense-in-depth): block friendly fire at the damage execution level.
	// Primary filtering happens UPSTREAM — targeting (GatherTargetCandidates), attack traces
	// (OSAnimNotifyState_GASAttackTrace), and ApplyDirectDamage all reject friendlies before
	// the GE is even created. This ExecCalc check is a safety net for any code path that
	// bypasses those upstream filters (future abilities, BP-only damage, etc.).
	//
	// NOTE: Even when this returns early, the GE's dummy modifier (0 damage) still "succeeds"
	// and fires GameplayCue.Sound.Hit. That's why upstream filtering is primary — it prevents
	// the GE from being applied at all, which prevents the cue entirely.
	{
		const UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
		const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
		if (SourceASC && TargetASC)
		{
			const AActor* SourceActor = SourceASC->GetAvatarActor();
			const AActor* TargetActor = TargetASC->GetAvatarActor();
			if (UOSCombatBlueprintLibrary::AreActorsFriendly(SourceActor, TargetActor))
			{
				return;
			}
		}
	}

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const FOSGameplayTags& Tags = FOSGameplayTags::Get();

	// Incoming damage from GE SetByCaller (Data.Damage); skip if none.
	const float IncomingDamage = FMath::Max(0.f, Spec.GetSetByCallerMagnitude(Tags.Data_Damage, false, 0.f));
	if (IncomingDamage <= 0.f)
	{
		UE_LOG(LogOSCombat, Warning, TEXT("[ExecCalc_OSDamage] Zero damage — SetByCaller tag '%s' not set on GE '%s'. "
			"Check that the ability sets Data_Damage before applying the effect."),
			*Tags.Data_Damage.ToString(), *GetNameSafe(Spec.Def));
		return;
	}

	// Grab damage bypasses block (Vicious Attacks GDD: grabs ignore block).
	// CapturedSourceTags contains attacker's active tags when GE was created —
	// IsGrabbing is present from GA_OSGrab's ActivationOwnedTags.
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	const bool bBypassesBlock = SourceTags && SourceTags->HasTag(Tags.IsGrabbing);
	const bool bIsBlocking = !bBypassesBlock
		&& TargetTags && Tags.IsBlocking.IsValid()
		&& TargetTags->HasTag(Tags.IsBlocking);

	// Capture target's current stamina (live value, not snapshot).
	float CurrentStamina = 0.f;
	{
		FAggregatorEvaluateParameters EvalParams;
		EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
		EvalParams.TargetTags = TargetTags;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(OSDamageExecStatics::TargetStaminaDef(), EvalParams, CurrentStamina);
		CurrentStamina = FMath::Max(0.f, CurrentStamina);
	}

	// Blocking: absorb up to CurrentStamina; rest is overflow. Guard break when overflow > 0.
	float Absorb = 0.f;
	float Overflow = IncomingDamage;
	bool bGuardBreak = false;

	if (bIsBlocking)
	{
		Absorb = FMath::Min(CurrentStamina, IncomingDamage);
		Overflow = IncomingDamage - Absorb;
		bGuardBreak = (Overflow > KINDA_SMALL_NUMBER); // blocking but couldn't absorb full damage
	}

	// Apply stamina drain for absorbed amount.
	if (Absorb > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(UOSAttributeSet::GetStaminaAttribute(), EGameplayModOp::Additive, -Absorb));
	}

	// Apply overflow as Damage meta (AttributeSet consumes into Health).
	if (Overflow > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(UOSAttributeSet::GetDamageAttribute(), EGameplayModOp::Additive, Overflow));
	}

	// Flag guard break via meta attribute (AttributeSet fires gameplay event).
	if (bGuardBreak)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(UOSAttributeSet::GetGuardBreakAttribute(), EGameplayModOp::Additive, 1.f));
	}

	// Recoil: flag attacker for recoil when block fully absorbs (no guard break)
	if (bIsBlocking && !bBypassesBlock && !bGuardBreak)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(
				UOSAttributeSet::GetRecoilAttribute(),
				EGameplayModOp::Additive,
				1.0f));
	}
}

