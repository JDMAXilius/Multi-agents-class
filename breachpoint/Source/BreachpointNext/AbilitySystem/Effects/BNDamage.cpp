#include "AbilitySystem/Effects/BNDamage.h"

#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "BreachpointNext.h"
#include "Data/BNDataRows.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

namespace
{
	/**
	 * Bone -> body section, by SUBSTRING rather than an exact name.
	 *
	 * The old rule was one exact match on `head`, which had two faults: it could express nothing
	 * but headshots, and it failed silently the day a skeleton spelled the bone differently. This
	 * is Zorans' bone→section→multiplier shape (ZoransCharacterBase.cpp:157) with the section list
	 * cut to what a Manny actually has. Substring matching covers `neck_01`, `spine_03`,
	 * `upperarm_l` and every numbered variant without listing them.
	 *
	 * NECK COUNTS AS HEAD — the convention in every shooter that has the distinction, and the
	 * alternative (a neck shot doing body damage while the crosshair was on the throat) reads as
	 * a bug to the player.
	 *
	 * An UNRECOGNISED bone resolves to Torso, never to a free hit: an unknown bone is far more
	 * likely to be a chest attachment than a reason to do no damage.
	 */
	enum class EBNBodySection : uint8 { Head, Torso, Arm, Leg };

	EBNBodySection BNSectionForBone(const FName BoneName)
	{
		if (BoneName.IsNone())
		{
			return EBNBodySection::Torso;
		}
		const FString Bone = BoneName.ToString().ToLower();

		if (Bone.Contains(TEXT("head")) || Bone.Contains(TEXT("neck"))) { return EBNBodySection::Head; }
		if (Bone.Contains(TEXT("thigh")) || Bone.Contains(TEXT("calf")) ||
			Bone.Contains(TEXT("foot")) || Bone.Contains(TEXT("ball")))  { return EBNBodySection::Leg; }
		if (Bone.Contains(TEXT("arm")) || Bone.Contains(TEXT("hand")))   { return EBNBodySection::Arm; }
		return EBNBodySection::Torso;
	}

	const TCHAR* BNSectionName(EBNBodySection Section)
	{
		switch (Section)
		{
		case EBNBodySection::Head: return TEXT("head");
		case EBNBodySection::Arm:  return TEXT("arm");
		case EBNBodySection::Leg:  return TEXT("leg");
		default:                   return TEXT("torso");
		}
	}

	float BNSectionMultiplier(const FBNWeaponRow& Row, EBNBodySection Section)
	{
		switch (Section)
		{
		case EBNBodySection::Head: return Row.HeadshotMultiplier;
		case EBNBodySection::Arm:  return Row.ArmMultiplier;
		case EBNBodySection::Leg:  return Row.LegMultiplier;
		default:                   return Row.TorsoMultiplier;
		}
	}

	/**
	 * Distance falloff, Lyra's concept without Lyra's curve asset (LyraRangedWeaponInstance.cpp:125).
	 * Disabled unless the row opts in, so this function returns 1.0 for every weapon shipped today.
	 *
	 * The distance is measured from the shot's OWN origin — `TraceStart` on the server's hit — and
	 * not from the shooter's actor location: on a listen server those differ by the camera offset,
	 * and a falloff that disagrees with the shot it is scaling is worse than no falloff at all.
	 */
	float BNFalloffMultiplier(const FBNWeaponRow& Row, const FHitResult& Hit, float& OutDistance)
	{
		OutDistance = FVector::Dist(Hit.TraceStart, Hit.ImpactPoint);

		// End <= Start means "no falloff configured" — including the default 0/0.
		if (Row.FalloffEndDistance <= Row.FalloffStartDistance)
		{
			return 1.f;
		}
		if (OutDistance <= Row.FalloffStartDistance)
		{
			return 1.f;
		}
		const float MinMultiplier = FMath::Clamp(Row.FalloffMinMultiplier, 0.f, 1.f);
		if (OutDistance >= Row.FalloffEndDistance)
		{
			return MinMultiplier;
		}
		const float Alpha = (OutDistance - Row.FalloffStartDistance) / (Row.FalloffEndDistance - Row.FalloffStartDistance);
		return FMath::Lerp(1.f, MinMultiplier, Alpha);
	}
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

namespace
{
	/** See GetApplyingSourceName's comment: the cause in flight, game thread + authority only. */
	FName GBNApplyingSourceName = NAME_None;
}

FName BNDamage::GetApplyingSourceName()
{
	return GBNApplyingSourceName;
}

void BNDamage::ApplyDamage(AActor* Instigator, AActor* Target, float Amount, const FHitResult& Hit, FName SourceName)
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

	// SAVE and RESTORE, not set and clear: damage applied from inside a damage reaction — a death
	// that detonates something the victim carried — must not leave the outer call's cause blank
	// for the rest of its own execution.
	const FName PreviousSource = GBNApplyingSourceName;
	GBNApplyingSourceName = SourceName;
	TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	GBNApplyingSourceName = PreviousSource;
}

// The weapon-shaped front of the door: WHERE the shot landed and HOW FAR it travelled are damage
// RULES, so they live here — behind the one door — and no caller ever multiplies a damage value.
// Both rules are inert until a row opts in, so this function's answer today is what it was before
// them: Damage, doubled on a head hit.
void BNDamage::ApplyWeaponDamage(AActor* Instigator, FName RowName, const FBNWeaponRow& Row, const FHitResult& Hit)
{
	const EBNBodySection Section = BNSectionForBone(Hit.BoneName);
	const float SectionMultiplier = BNSectionMultiplier(Row, Section);

	float Distance = 0.f;
	const float FalloffMultiplier = BNFalloffMultiplier(Row, Hit, Distance);

	const float Amount = Row.Damage * SectionMultiplier * FalloffMultiplier;

	// Verbose, because it fires on every landed bullet — but when a number ever looks wrong, this
	// is the line that says which of the three factors produced it instead of leaving the reader
	// to reverse-engineer a total. BoneName 'None' here means the trace resolved on something with
	// no skeleton, and a headshot could never fire.
	UE_LOG(LogBN, Verbose,
		TEXT("BNDamage: %.1f base x%.2f %s (bone '%s') x%.2f falloff @ %.0fuu = %.1f"),
		Row.Damage, SectionMultiplier, BNSectionName(Section), *Hit.BoneName.ToString(),
		FalloffMultiplier, Distance, Amount);

	ApplyDamage(Instigator, Hit.GetActor(), Amount, Hit, RowName);
}
