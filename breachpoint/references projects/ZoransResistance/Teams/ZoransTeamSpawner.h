// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "ZoransTeamSpawner.generated.h"

UCLASS()
class ZORANSRESISTANCE_API AZoransTeamSpawner : public APlayerStart
{
	GENERATED_BODY()

public:
	AZoransTeamSpawner(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Meta = (ExposeOnSpawn = true))
	int32 TeamIndex = -1;

	/**
	 * Calculates this spawner's "score". The higher the score, the "better" this spawner is.
	 * The actual value has no meaning by itself. It should only be used to compare with other spawners.
	 * @return Spawn score
	 */
	UFUNCTION()
	float GetSpawnScore() const;
};