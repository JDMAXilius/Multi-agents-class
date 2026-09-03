#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AIBLaneVolume.generated.h"

class UBoxComponent;
class UNavModifierComponent;

/** Phase 14 — a script-placed lane painter (W-AUDIT: not ANavModifierVolume, a Brush the
 *  MCP cannot place). A box + a nav modifier whose area class is the lane's
 *  UAIBNavArea_Lane<N>; the blockout generator spawns one per lane folder, sets LaneId
 *  and the box extent, and tags it for its idempotent clear. Server-side navmesh only:
 *  nothing here replicates, nothing ticks. */
UCLASS()
class AIBOT_API AAIBLaneVolume : public AActor
{
	GENERATED_BODY()

public:
	AAIBLaneVolume();

	/** 1..AIB::MaxRouteLanes; the folder ordinal. Applied in OnConstruction (editor edits
	 *  and script spawns both pass through it) — SetLane is the script's one-call form. */
	UPROPERTY(EditAnywhere, Category = "AIBot", meta = (ClampMin = "1", ClampMax = "6"))
	int32 LaneId = 1;

	void SetLane(int32 InLaneId, const FVector& HalfExtentUU);

	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UNavModifierComponent> Modifier;
};
