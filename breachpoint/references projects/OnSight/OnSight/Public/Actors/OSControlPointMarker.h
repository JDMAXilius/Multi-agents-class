#pragma once
// Lightweight level marker. In Domination: spawns AOSControlPoint at this location.
// In other modes: inert, zero cost. Server-only, no replication, no tick.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OSControlPointMarker.generated.h"

class AOSControlPoint;

UCLASS()
class ONSIGHT_API AOSControlPointMarker : public AActor
{
	GENERATED_BODY()

public:
	AOSControlPointMarker();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Label to assign to the spawned control point ("A", "B", "C", etc.) */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Control Point")
	FText PointLabel;

	/** Control point class to spawn. Must be set in Blueprint (C++ base has no colliders/mesh). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Control Point")
	TSubclassOf<AOSControlPoint> ControlPointClass;

private:
	UPROPERTY()
	TObjectPtr<AOSControlPoint> SpawnedPoint;
};
