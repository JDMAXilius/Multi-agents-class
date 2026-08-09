#pragma once
// Level-placed spawn volume. Players on the matching team spawn at a random point within the box.
// Server-only — no replication, no tick. Queried by AOSGameMode::FindBestSpawnZone on demand.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OSTeamSpawnZone.generated.h"

class UBoxComponent;

UCLASS()
class ONSIGHT_API AOSTeamSpawnZone : public AActor
{
	GENERATED_BODY()

public:
	AOSTeamSpawnZone();

	bool ServesTeam(int32 InTeamIndex) const;
	FTransform GetSpawnTransform() const;
	float ScoreForPlayer(AController* Player) const;
	int32 GetTeamIndex() const { return TeamIndex; }

protected:
	/** Team index this zone serves. 0 = Team A, 1 = Team B. -1 = any team. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn")
	int32 TeamIndex = 0;

	/** Volume that defines the spawn area. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn")
	TObjectPtr<UBoxComponent> SpawnVolume;

	/** Desired minimum distance from enemy players when scoring zones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "0"))
	float PreferredEnemyDistance = 1000.f;
};
