#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/AIBWorldQuery.h"
#include "BNAIBWorldQuery.generated.h"

class ABNHillPoint;

/**
 * BN's answers to the AIBot module's world questions (Phase 6, deliberately narrow):
 * QueryPointsOfInterest and AreEnemies are implemented; the other two REFUSE rather
 * than half-answer.
 *
 * - QueryVisibleEnemies returns EMPTY on purpose. A useful implementation must run the
 *   sensorium's own maturation rules (200ms floor, occlusion, the belief ladder) or it
 *   is the 25 Aug wallhack with a subsystem's name on it. Until a matured feed exists,
 *   an honest empty keeps the controller's own perception the only acquisition path.
 * - CountNearbyAllies returns 0: BN is FFA today, and inventing teammates would feed
 *   the confidence model fiction. The facts builder keeps bCrowdKnown=false to match.
 *
 * A WorldSubsystem, not the GameState: the queries are server-brain plumbing, and
 * nothing about them should ride an actor that replicates.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNAIBWorldQuery : public UWorldSubsystem, public IAIBWorldQuery
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** The GameMode pushes the hill it spawned (or found placed) at match start. The
	 *  subsystem never searches the world for hills — same law as the module's own
	 *  provider resolution: things are handed in, not hunted. */
	void RegisterHill(ABNHillPoint* Hill);

	// -- IAIBWorldQuery ---------------------------------------------------------------
	/** Hills, as HUD-grade knowledge: an objective marker is on every human's screen
	 *  from anywhere on the map, so MaxDistance is deliberately not applied. */
	virtual void QueryPointsOfInterest(const AActor* Asker, float MaxDistance,
		TArray<FAIBPointOfInterest>& OutPoints) const override;

	/** Honest empty — see the class comment. */
	virtual void QueryVisibleEnemies(const AActor* Asker, float Radius,
		TArray<AActor*>& OutEnemies) const override;

	/** FFA: nobody has allies. */
	virtual int32 CountNearbyAllies(const AActor* Asker, float Radius) const override;

	/** FFA: two different living pawns are enemies. Teams change THIS function and
	 *  nothing inside the AIBot module — which is the point of the hostility door. */
	virtual bool AreEnemies(const AActor* A, const AActor* B) const override;

private:
	TArray<TWeakObjectPtr<ABNHillPoint>> Hills;
};
