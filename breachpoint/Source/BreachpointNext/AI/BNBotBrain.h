#pragma once

#include "CoreMinimal.h"
#include "Data/BNDataRows.h"
#include "UObject/Object.h"
#include "BNBotBrain.generated.h"

UENUM(BlueprintType)
enum class EBNBotAmbition : uint8
{
	Fight,
	Survive,
	Roam
};

/** The facts the controller distills from the world. A value struct on purpose: the brain reads
 *  ONLY this plus the time its caller passes in — never an actor, a world, or a clock. */
USTRUCT()
struct FBNBotFacts
{
	GENERATED_BODY()

	bool bHasTarget = false;
	float HealthNorm = 1.f;
	float DistToTargetNorm = 1.f;
};

/** The bot's WANT, above the tree. Headless by contract: no GetWorld, no clock, no actors — the
 *  caller hands in facts, resolved rows and the current world seconds, which is what makes this
 *  unit-testable and deterministic later (R8). Holds only the ambition, its commit window, and
 *  the scoring math. Server-only by residence: it lives on the AIController. */
UCLASS()
class BREACHPOINTNEXT_API UBNBotBrain : public UObject
{
	GENERATED_BODY()

public:
	/** The C++ fallback rows (G3 3.1) — what drives when DT_BNBotAmbitions has not landed.
	 *  The table OVERRIDES these, never duplicates them. */
	static FBNBotAmbitionRow DefaultRow(EBNBotAmbition Ambition);

	/** The row-name / log-name literal for an ambition: Fight, Survive, Roam. */
	static FName AmbitionRowName(EBNBotAmbition Ambition);

	/** Scores the three ambitions against Facts and applies the commit window against NowSeconds.
	 *  Returns true only when the held ambition CHANGED — the caller's one log gate. */
	bool Rescore(const FBNBotFacts& Facts, const FBNBotAmbitionRow& FightRow,
		const FBNBotAmbitionRow& SurviveRow, const FBNBotAmbitionRow& RoamRow, double NowSeconds);

	EBNBotAmbition GetAmbition() const { return CurrentAmbition; }
	float GetUtility() const { return CurrentUtility; }
	const FString& GetWinningConsideration() const { return WinningConsideration; }

private:
	EBNBotAmbition CurrentAmbition = EBNBotAmbition::Roam;
	float CurrentUtility = 0.f;

	/** World seconds when the current commit ends. Negative so the first Rescore always selects. */
	double CommitEndSeconds = -1.0;

	FString WinningConsideration;
};
