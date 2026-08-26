#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BNHillPoint.generated.h"

/**
 * THE HILL (founder ruling, 26 Aug 2026): one volume, sole occupant scores per second,
 * first to the limit wins. The actor is deliberately as dumb as the point-of-interest
 * marker it mirrors — a place and a radius, no tick, no replication: occupancy,
 * scoring, and the contested rule all live on the GameMode's one-second timer, and
 * what a client needs to SEE arrives through the PlayerState scores it already gets.
 * Spawned by the GameMode from ini config (C++-first: no level edit required to run
 * the mode), or placed by hand — both register with the world query the same way.
 */
UCLASS()
class BREACHPOINTNEXT_API ABNHillPoint : public AActor
{
	GENERATED_BODY()

public:
	ABNHillPoint();

	UPROPERTY(EditAnywhere, Category = "BN")
	float Radius = 600.f;
};
