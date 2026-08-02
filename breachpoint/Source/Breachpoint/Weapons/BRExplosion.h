// Breachpoint. OUR radial damage — the one blast rule, shared by the grenade and the rocket.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;
class UAbilitySystemComponent;

namespace BRExplosion
{
	BREACHPOINT_API int32 ApplyExplosionDamage(
		UAbilitySystemComponent* InstigatorASC,
		AActor* InstigatorActor,
		const FVector& Epicentre,
		float BlastRadiusMetres,
		float BlastCentreDamage,
		FName FalloffCurveName,
		const FGameplayTag& ExplodeCueTag);
}
