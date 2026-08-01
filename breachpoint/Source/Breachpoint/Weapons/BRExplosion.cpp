// Breachpoint. OUR radial damage — the one blast rule, shared by the grenade and the rocket.

#include "Weapons/BRExplosion.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/BRCombatCurves.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
}

int32 BRExplosion::ApplyExplosionDamage(
	UAbilitySystemComponent* InstigatorASC,
	AActor* InstigatorActor,
	const FVector& Epicentre,
	float BlastRadiusMetres,
	float BlastCentreDamage,
	FName FalloffCurveName,
	const FGameplayTag& ExplodeCueTag)
{
	if (!InstigatorASC || !InstigatorActor)
	{
		// A projectile that outlives its instigator (disconnect, respawn, travel) lands here.
		// Refusing is right: damage with no source cannot be attributed, and an unattributed kill is
		// a scoring bug that surfaces three systems away.
		UE_LOG(LogBRCombat, Warning, TEXT("BRExplosion::ApplyExplosionDamage refused: the blast has no instigator ASC or actor."));
		return 0;
	}

	UWorld* World = InstigatorActor->GetWorld();
	if (!World)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRExplosion::ApplyExplosionDamage refused: instigator '%s' has no world, so there is nothing to overlap."),
			*GetNameSafe(InstigatorActor));
		return 0;
	}

	if (InstigatorASC->GetOwnerRole() != ROLE_Authority)
	{
		// SERVER ONLY BY CONTRACT (`BRGameplayEffects.h`: the exec calc is server truth). A client
		// running this would apply a GE that replication then contradicts, and every client would
		// compute a slightly different overlap set from its own approximate positions.
		UE_LOG(LogBRCombat, Error, TEXT("BRExplosion::ApplyExplosionDamage called off the authority; refused."));
		return 0;
	}

	if (BlastRadiusMetres <= 0.f || BlastCentreDamage <= 0.f)
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRExplosion::ApplyExplosionDamage refused: radius %.2f m / centre damage %.2f. "
				 "Both come from data and neither may be invented here."),
			BlastRadiusMetres, BlastCentreDamage);
		return 0;
	}

	if (FalloffCurveName.IsNone())
	{
		// The caller owns the curve's NAME (see the header on why it is a parameter). A blast with no
		// named shape is a blast this file would have to invent a shape for, which is the one thing
		// it may never do.
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRExplosion::ApplyExplosionDamage refused: no falloff curve name was supplied, and "
				 "no falloff shape may be invented here (law 3)."));
		return 0;
	}

	const float RadiusUU = BlastRadiusMetres * BRUnits::MetresToUU;

	// ------------------------------------------------------------------------------------------
	// 1. THE OVERLAP. This is what "radial damage" means in this project.
	// ------------------------------------------------------------------------------------------
	//
	// `AActor::TakeDamage`, `UGameplayStatics::ApplyRadialDamage` and `ApplyPointDamage` are BANNED
	// (law 2/3, and a PreToolUse hook blocks them outright). The ban is not a stylistic preference:
	// the engine's radial helper computes its own damage number, applies it through its own event,
	// and lands OUTSIDE `BRDamageExecCalc` — so shields would not absorb it, `GE_RecentDamage`
	// would not gate regen after it, and `Damage.Explosive.Multiplier` would not touch it. Radial
	// damage means gathering the targets ourselves and applying the ONE GE per target. That is all
	// this function is.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(BRExplosionBlast), /*bTraceComplex=*/false);
	World->OverlapMultiByObjectType(
		Overlaps,
		Epicentre,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(RadiusUU),
		OverlapParams);

	// The world blockers for the line-of-sight step, built once. STATIC AND DYNAMIC WORLD ONLY, and
	// that is a stated design position reached structurally rather than by a filter: **a body never
	// shields another body from a blast.** Querying only world object types makes it impossible to
	// change that by accident later — a `Visibility` trace would have silently made teammates into
	// cover, which is a real gameplay rule nobody would have decided to add.
	FCollisionObjectQueryParams BlastBlockers;
	BlastBlockers.AddObjectTypesToQuery(ECC_WorldStatic);
	BlastBlockers.AddObjectTypesToQuery(ECC_WorldDynamic);

	if (ExplodeCueTag.IsValid())
	{
		// A confirmed one-shot on the server path, which is what `gas-purity` §6 asks for: the
		// detonation is not predicted by anyone, so there is nothing to roll back and an `Executed`
		// cue is correct.
		FGameplayCueParameters CueParams;
		CueParams.Location = Epicentre;
		CueParams.RawMagnitude = BlastCentreDamage;
		CueParams.Instigator = InstigatorActor;
		InstigatorASC->ExecuteGameplayCue(ExplodeCueTag, CueParams);
	}
	else
	{
		// *** CUE TAG OWED (BP05): `GameplayCue.Grenade.Explode`, plus its C++ handler class. ***
		// The caller resolves the tag and hands it in; an invalid one means the leaf is still
		// undeclared. Damage still applies — FX and gameplay are separate legs.
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRExplosion: no valid explosion cue tag was supplied, so the explosion is silent "
				 "and invisible. Damage still applies — FX and gameplay are separate legs."));
	}

	// One actor produces many overlap hits (capsule, mesh, hitboxes). Without this set, a target
	// takes one grenade's damage once per collider it happens to own — which is a damage bug whose
	// size depends on art.
	TSet<AActor*> Considered;
	int32 DamagedCount = 0;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Considered.Contains(Target))
		{
			continue;
		}
		Considered.Add(Target);

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!TargetASC)
		{
			// No ASC means it is scenery that happens to sit on the Pawn channel, not a fighter.
			continue;
		}

		// ----------------------------------------------------------------------------------
		// SELF-DAMAGE: the instigator is a target like anyone else. STATED, per BP05 step 5.
		// ----------------------------------------------------------------------------------
		//
		// `BRGA_WeaponFire` drops self-hits (`TargetASC == SourceASC`) because a hitscan shot that
		// hits its own shooter is a bug. An explosion that hits its own thrower is the mechanic:
		// the risk of standing near your own grenade is half of what makes grenade play a decision,
		// and it is the arena convention this game is built on. There is deliberately NO
		// `TargetASC == InstigatorASC` skip here, and its absence is the rule.
		//
		// NOT DECIDED HERE, and named rather than assumed: FRIENDLY FIRE. Nothing in this function
		// asks which team the target is on, because that is a MATCH rule (`DT_MatchRules` /
		// GameMode), not a property of an explosion, and inventing a team check here would put a
		// second owner on it. A blast currently damages teammates. If the match rules say
		// otherwise, the filter belongs where the rule lives, not in this loop.

		// ----------------------------------------------------------------------------------
		// 2. LINE OF SIGHT. Without this, an overlap sphere damages through walls.
		// ----------------------------------------------------------------------------------
		//
		// BP05 step 5's critic case is literally "grenade through walls (overlap vs LOS)". A sphere
		// overlap answers "is it within R" and nothing else; the wall between is invisible to it.
		// Re-deriving reachability from the epicentre is what makes this a BLAST rather than a
		// radius, and it happens on the authority for the same reason the whole function does.
		const FVector TargetCentre = Target->GetActorLocation();
		FHitResult Blocker;
		FCollisionQueryParams LosParams(SCENE_QUERY_STAT(BRExplosionBlastLOS), /*bTraceComplex=*/false);
		LosParams.AddIgnoredActor(Target);
		if (World->LineTraceSingleByObjectType(Blocker, Epicentre, TargetCentre, BlastBlockers, LosParams))
		{
			UE_LOG(LogBRCombat, Verbose,
				TEXT("BRExplosion blast: '%s' is in radius but '%s' blocks the path; no damage."),
				*GetNameSafe(Target), *GetNameSafe(Blocker.GetActor()));
			continue;
		}

		// ----------------------------------------------------------------------------------
		// 3. FALLOFF, from the curve. Not from an expression typed here.
		// ----------------------------------------------------------------------------------
		//
		// Distance is measured to the target's ACTOR CENTRE, not to the nearest point on its
		// capsule. That is a decision: nearest-point would quietly reward crouching and lying down
		// with extra blast damage, and it would make the authored falloff shape mean something
		// different for a tall pawn than a short one. One distance per target, one curve.
		const float Distance = FVector::Dist(Epicentre, TargetCentre);
		const float NormalisedDistance = FMath::Clamp(Distance / RadiusUU, 0.f, 1.f);

		float FalloffScale = 0.f;
		if (!BRCombatCurves::Evaluate(FalloffCurveName, NormalisedDistance, FalloffScale))
		{
			// REFUSE, do not substitute. A linear falloff typed here would be a law-3 violation and
			// a second source of truth that silently outranks the CSV. The grenade already refuses
			// at activation when this curve is missing; reaching here means the table changed
			// mid-match, which is worth its own line.
			UE_LOG(LogBRCombat, Warning,
				TEXT("BRExplosion blast: CT_Combat has no '%s' curve. No falloff shape means no "
					 "damage number this file is allowed to compute; blast aborted."),
				*FalloffCurveName.ToString());
			return DamagedCount;
		}

		const float BaseDamage = BlastCentreDamage * FalloffScale;
		if (BaseDamage <= 0.f)
		{
			// A curve may legitimately author zero at the edge. That is "out of the blast", not an
			// error, and applying a zero-damage GE would still fire the RecentDamage regen gate.
			continue;
		}

		// ----------------------------------------------------------------------------------
		// 4. THE ONE PIPELINE. One GE per target, one type tag, FLAT (R22).
		// ----------------------------------------------------------------------------------
		//
		// `{Damage.Explosive}` — never a nested leaf, and never `Damage.Explosive.Something`.
		// This file does NOT read `Damage.Explosive.Multiplier`: `BRDamageExecCalc` derives that
		// curve name from the tag itself and composes it, so the explosive multiplier is applied
		// exactly once, in the one place that owns the damage rule. Reading it here as well would
		// square it — the D8 headshot defect in a different costume.
		FGameplayTagContainer DamageTags;
		DamageTags.AddTag(BRGameplayTags::Damage_Explosive);

		FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
		Context.AddInstigator(InstigatorActor, InstigatorActor);
		// The epicentre travels on the context so impact cues and any future knockback know which
		// way "away" is, without a second parameter channel.
		Context.AddOrigin(Epicentre);

		const FGameplayEffectSpecHandle Spec = UBRGE_Damage::MakeSpec(InstigatorASC, BaseDamage, DamageTags, Context);
		if (UBRGE_Damage::ApplyToTarget(Spec, InstigatorASC, TargetASC))
		{
			++DamagedCount;
		}
	}

	UE_LOG(LogBRCombat, Verbose,
		TEXT("BRExplosion blast at %s: %d of %d overlapped actors damaged (radius %.2f m)."),
		*Epicentre.ToCompactString(), DamagedCount, Considered.Num(), BlastRadiusMetres);

	return DamagedCount;
}
