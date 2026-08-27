#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/AIBWorldQuery.h"
#include "BNAIBWorldQuery.generated.h"

class ABNHillPoint;

/** The rally geometry (TEAMS, 27 Aug), shared by the POI publisher (each ally POI's
 *  ReachRadiusUU) and the mode's urgency (which reads ZERO inside Near) — one pair of
 *  numbers, or the mover and the want disagree about what "with the team" means.
 *  Near 600: close enough to fight together, far enough not to stack in one doorway.
 *  Far 3000: past this, being alone is the loudest a regroup ever gets. */
namespace BNAIB
{
	inline constexpr float RallyNearUU = 600.f;
	inline constexpr float RallyFarUU = 3000.f;
}

/**
 * BN's answers to the AIBot module's world questions (Phase 6, teams landed BN15):
 * QueryPointsOfInterest, AreEnemies and CountNearbyAllies are implemented;
 * QueryVisibleEnemies REFUSES rather than half-answer.
 *
 * - QueryVisibleEnemies returns EMPTY on purpose. A useful implementation must run the
 *   sensorium's own maturation rules (200ms floor, occlusion, the belief ladder) or it
 *   is the 25 Aug wallhack with a subsystem's name on it. Until a matured feed exists,
 *   an honest empty keeps the controller's own perception the only acquisition path.
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

	/** Living same-team pawns within Radius of Asker, Asker excluded (HUD-grade: radar
	 *  shows teammates). A NoTeam asker counts ZERO — in FFA nobody has allies, which
	 *  was the honest pre-teams answer and still is. */
	virtual int32 CountNearbyAllies(const AActor* Asker, float Radius) const override;

	/** Enemies = both alive AND not same-team, teams read off the PlayerStates with the
	 *  NoTeam guard (NoTeam is nobody's friend — FFA byte-identical). Teams changed THIS
	 *  function and nothing inside the AIBot module — the hostility door doing its job. */
	virtual bool AreEnemies(const AActor* A, const AActor* B) const override;

	/** Allies = same-team, PERIOD — no alive door on purpose: alliance outlives the
	 *  body (a dead teammate is still a teammate, a dead enemy still an enemy), which
	 *  is what keeps corpses Hostile in the module's attitude and keeps a dead enemy's
	 *  claim from binding across teams (BN15 W-REVIEW). FFA (everyone NoTeam) answers
	 *  false for every pair — nobody has allies, the pre-teams truth. */
	virtual bool AreAllies(const AActor* A, const AActor* B) const override;

private:
	TArray<TWeakObjectPtr<ABNHillPoint>> Hills;
};
