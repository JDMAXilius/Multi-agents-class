#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"

UBNGE_InitAttributes::UBNGE_InitAttributes()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// ORDER IS LOAD-BEARING: the maxes go FIRST. Modifiers apply in order and PreAttributeChange now
	// clamps Health to MaxHealth — so setting Health before MaxHealth clamps it against a max that
	// is still the default 0, and every player would spawn dead.
	{
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetMaxHealthAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(100.f);
		Modifiers.Add(Modifier);
	}
	// SHIELDS ARE OFF, on purpose (founder, 13 Aug 2026): "I want to see the health perfectly working
	// first." A shield in front of health means every health test spends its first hits chewing
	// through something else, and a wrong number in either pool looks identical from the outside.
	// Zero here makes damage land on health directly and makes the log line read plainly.
	//
	// This is a PAUSE, not a removal. The whole dance is built and stays built — MaxShield, the
	// recharge GE, the RecentDamage window, State.Shields.Broken. Restoring it is this one number
	// back to 100.f, nothing else: everything downstream already gates itself on MaxShield > 0.
	{
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetMaxShieldAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(0.f);
		Modifiers.Add(Modifier);
	}
	{
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetHealthAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(100.f);
		Modifiers.Add(Modifier);
	}
	{
		// Matches MaxShield above. Restore to 100.f in the same edit that restores the max.
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetShieldAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(0.f);
		Modifiers.Add(Modifier);
	}
	// R7.4 — the pouch. TWO, the arena-shooter convention this design is built on, and the max
	// goes first for the same order reason the health pair does. Respawn re-applies this whole GE,
	// so a fresh life is a full pouch with no separate resupply path to keep in sync.
	// Set MaxGrenades to 0 here and grenades are OFF: the cost refuses every throw and the HUD
	// hides the slot — the same switch MaxShield already is for shields.
	{
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetMaxGrenadesAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(2.f);
		Modifiers.Add(Modifier);
	}
	{
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetGrenadesAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(2.f);
		Modifiers.Add(Modifier);
	}
	{
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetMoveSpeedAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(600.f);
		Modifiers.Add(Modifier);
	}
	{
		// 250/600 in the founder's working reference (MyCharacter.h:300,297), as the ratio.
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetADSSpeedMultiplierAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(0.4167f);
		Modifiers.Add(Modifier);
	}
	{
		// 900/600 in the founder's working reference (MyCharacter.h:297,303), expressed as the ratio.
		FGameplayModifierInfo Modifier;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.Attribute = UBNAttributeSet::GetSprintSpeedMultiplierAttribute();
		Modifier.ModifierMagnitude = FScalableFloat(1.5f);
		Modifiers.Add(Modifier);
	}
}

UBNGE_GrenadeCost::UBNGE_GrenadeCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.Attribute = UBNAttributeSet::GetGrenadesAttribute();
	Modifier.ModifierMagnitude = FScalableFloat(-1.f);
	Modifiers.Add(Modifier);
}

UBNGE_State::UBNGE_State()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

UBNGE_Sprint::UBNGE_Sprint()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FAttributeBasedFloat FromMultiplier;
	FromMultiplier.Coefficient = FScalableFloat(1.f);
	FromMultiplier.AttributeCalculationType = EAttributeBasedFloatCalculationType::AttributeMagnitude;
	// Non-snapshot: a later change to the multiplier re-evaluates a sprint already running.
	FromMultiplier.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(
		UBNAttributeSet::GetSprintSpeedMultiplierAttribute(),
		EGameplayEffectAttributeCaptureSource::Target,
		/*bSnapshot=*/false);

	FGameplayModifierInfo Modifier;
	Modifier.ModifierOp = EGameplayModOp::MultiplyAdditive;
	Modifier.Attribute = UBNAttributeSet::GetMoveSpeedAttribute();
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FromMultiplier);
	Modifiers.Add(Modifier);
}

UBNGE_FireCooldown::UBNGE_FireCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat Duration;
	Duration.DataName = BNSetByCaller::FireDelay;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
}

UBNGE_ADS::UBNGE_ADS()
{
	// Sprint's constructor with one attribute swapped — see UBNGE_Sprint above for why the
	// magnitude is captured non-snapshot from an attribute rather than written as a number.
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FAttributeBasedFloat FromMultiplier;
	FromMultiplier.Coefficient = FScalableFloat(1.f);
	FromMultiplier.AttributeCalculationType = EAttributeBasedFloatCalculationType::AttributeMagnitude;
	FromMultiplier.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(
		UBNAttributeSet::GetADSSpeedMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Target, /*bSnapshot=*/false);

	FGameplayModifierInfo Modifier;
	// MultiplyAdditive, the 5.8 name — copied from UBNGE_Sprint, whose op this must match so the
	// two multipliers aggregate the same way if both ever apply.
	Modifier.ModifierOp = EGameplayModOp::MultiplyAdditive;
	Modifier.Attribute = UBNAttributeSet::GetMoveSpeedAttribute();
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FromMultiplier);
	Modifiers.Add(Modifier);
}

UBNGE_GrenadeCooldown::UBNGE_GrenadeCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat Duration;
	Duration.DataName = BNSetByCaller::GrenadeCooldown;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);

	// The tag rides the SPEC rather than being set here, for the construction-order reason
	// UBNGE_State documents: native tags are not guaranteed registered while CDOs are built.
}

UBNGE_RecentDamage::UBNGE_RecentDamage()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat Duration;
	Duration.DataName = BNSetByCaller::RecentDamageWindow;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
}

UBNGE_ShieldRecharge::UBNGE_ShieldRecharge()
{
	// Infinite and always applied; the tag requirement below is what turns it on and off, so
	// nothing ever applies or removes this GE in response to being shot.
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Per period, not per second — the period is set alongside this in the ini-facing constants
	// below, and the two are read together as "this much shield every this long".
	Period = FScalableFloat(BNShield::RechargePeriod);

	FGameplayModifierInfo Modifier;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.Attribute = UBNAttributeSet::GetShieldAttribute();
	Modifier.ModifierMagnitude = FScalableFloat(BNShield::RechargePerPeriod);
	Modifiers.Add(Modifier);
}
