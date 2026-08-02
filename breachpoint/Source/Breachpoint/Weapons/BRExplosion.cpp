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
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRExplosion::ApplyExplosionDamage refused: no falloff curve name was supplied, and "
				 "no falloff shape may be invented here (law 3)."));
		return 0;
	}

	const float RadiusUU = BlastRadiusMetres * BRUnits::MetresToUU;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(BRExplosionBlast), false);
	World->OverlapMultiByObjectType(
		Overlaps,
		Epicentre,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(RadiusUU),
		OverlapParams);

	FCollisionObjectQueryParams BlastBlockers;
	BlastBlockers.AddObjectTypesToQuery(ECC_WorldStatic);
	BlastBlockers.AddObjectTypesToQuery(ECC_WorldDynamic);

	if (ExplodeCueTag.IsValid())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Epicentre;
		CueParams.RawMagnitude = BlastCentreDamage;
		CueParams.Instigator = InstigatorActor;
		InstigatorASC->ExecuteGameplayCue(ExplodeCueTag, CueParams);
	}
	else
	{
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRExplosion: no valid explosion cue tag was supplied, so the explosion is silent "
				 "and invisible. Damage still applies — FX and gameplay are separate legs."));
	}

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
			continue;
		}

		const FVector TargetCentre = Target->GetActorLocation();
		FHitResult Blocker;
		FCollisionQueryParams LosParams(SCENE_QUERY_STAT(BRExplosionBlastLOS), false);
		LosParams.AddIgnoredActor(Target);
		if (World->LineTraceSingleByObjectType(Blocker, Epicentre, TargetCentre, BlastBlockers, LosParams))
		{
			continue;
		}

		const float Distance = FVector::Dist(Epicentre, TargetCentre);
		const float NormalisedDistance = FMath::Clamp(Distance / RadiusUU, 0.f, 1.f);

		float FalloffScale = 0.f;
		if (!BRCombatCurves::Evaluate(FalloffCurveName, NormalisedDistance, FalloffScale))
		{
			UE_LOG(LogBRCombat, Warning,
				TEXT("BRExplosion blast: CT_Combat has no '%s' curve. No falloff shape means no "
					 "damage number this file is allowed to compute; blast aborted."),
				*FalloffCurveName.ToString());
			return DamagedCount;
		}

		const float BaseDamage = BlastCentreDamage * FalloffScale;
		if (BaseDamage <= 0.f)
		{
			continue;
		}

		FGameplayTagContainer DamageTags;
		DamageTags.AddTag(BRGameplayTags::Damage_Explosive);

		FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
		Context.AddInstigator(InstigatorActor, InstigatorActor);
		Context.AddOrigin(Epicentre);

		const FGameplayEffectSpecHandle Spec = UBRGE_Damage::MakeSpec(InstigatorASC, BaseDamage, DamageTags, Context);
		if (UBRGE_Damage::ApplyToTarget(Spec, InstigatorASC, TargetASC))
		{
			++DamagedCount;
		}
	}

	return DamagedCount;
}
