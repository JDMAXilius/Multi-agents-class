#pragma once

#include "CoreMinimal.h"
#include "Brain/AIBAmbition.h"
#include "Core/AIBTypes.h"
#include "UObject/Object.h"
#include "AIBAmbitionEngine.generated.h"

/**
 * The arbitration layer — Halo's Utility AI, worldless. Registered ambitions are scored
 * against FAIBFacts; the highest wins and becomes the executor's active branch. All of
 * it is pure math on the facts, which is what makes AIBot.Sim.AmbitionEngine possible.
 *
 * Three anti-pathology mechanisms, each a named aib-critic attack:
 *  - HYSTERESIS: the incumbent's score is multiplied by SwitchCostFactor, so a marginal
 *    challenger does not flicker the bot between two wants at the boundary.
 *  - COMMIT: an ambition that TAKES the win holds for its CommitSeconds — one-shot on
 *    entry, not re-armed by repeat wins (a long-lived incumbent runs on hysteresis
 *    alone after its window). Rescoring continues inside the window, and TWO releases
 *    exist besides expiry: a hard interrupt, and THE VETO — an incumbent whose fresh
 *    score is zero has declared itself impossible, and a commit must not hold a bot to
 *    chasing a corpse (W-REVIEW P2 H-2). A spec replaced mid-commit keeps the old
 *    window (re-registration swaps the recipe, not the clock).
 *  - HARD INTERRUPTS, exactly two, engine-owned, BOTH EDGES: a blast BECOMING imminent
 *    (inside BlastInterruptSeconds), and vitals CROSSING below HealthCliffNorm. Edges,
 *    never states — a state-shaped interrupt disables the commit for its whole
 *    duration and the bot re-picks at think rate under a grenade, dithering through
 *    the most legible moment in the match (W-REVIEW P2 M5). An interrupt VOIDS the
 *    commit; it does not pick the winner — scoring still does. (343 published the
 *    ambitions; commit/interrupt shapes are OUR design.)
 *
 * Mode ambitions join their objective facts BY TAG: each spec is scored against the
 * FAIBObjectiveFact whose AmbitionTag matches, so CTF's capture/return/defend receive
 * distinct inputs (W-REVIEW F-6.6).
 */
UCLASS()
class AIBOT_API UAIBAmbitionEngine : public UObject
{
	GENERATED_BODY()

public:
	void RegisterAmbition(const FAIBAmbitionSpec& Spec);
	void ClearAmbitions();
	int32 NumAmbitions() const { return Ambitions.Num(); }

	/** Score everything, honour commit/hysteresis/interrupts, return the winner.
	 *  An empty registry returns the empty tag — a bot with no wants stands still,
	 *  loudly the executor's problem, never a crash. */
	FGameplayTag Rescore(const FAIBFacts& Facts, double NowSeconds);

	FGameplayTag GetCurrent() const { return CurrentTag; }

	/** The incumbent's FRESH pre-hysteresis score from the last rescore — never stale
	 *  across a commit hold, never inflated (W-REVIEW P2 L-3/L2). */
	float GetCurrentScore() const { return CurrentScore; }
	double GetCommitEndSeconds() const { return CommitEndSeconds; }

	/** Runner-up's tag+score from the last rescore, for the instrument (empty when
	 *  fewer than two ambitions scored). */
	const FAIBScoredAmbition& GetLastRunnerUp() const { return LastRunnerUp; }
	bool WasLastRescoreInterrupted() const { return bLastRescoreInterrupted; }

	/** Clears arbitration state — winner, commit, cliff baseline, scoreboard — and
	 *  KEEPS the registry. The brain must die with the body: called at unpossession so
	 *  a respawned bot cannot resume a dead life's commit, and so an absolute-time
	 *  CommitEnd cannot survive into a new world's clock as a permanent commit
	 *  (W-REVIEW P2 M-1, found independently by two passes). */
	void ResetArbitration();

	/** Last rescore's full scoreboard, winner included — for the debugger and specs. */
	const TArray<FAIBScoredAmbition>& GetLastScores() const { return LastScores; }

	/** The default core set — Engage / Retreat / Search / Seek / Roam — with
	 *  C++-authored curves. Phase 8's tables retune these; they never redefine them. */
	static void BuildDefaultCoreAmbitions(TArray<FAIBAmbitionSpec>& OutSpecs);

	// -- the knobs. C++ defaults are truth; Phase 8 moves them onto tier data —
	// EditAnywhere here reached no surface and promised a tuning path that did not
	// exist (W-REVIEW P2 finding D) --------------------------------------------------
	UPROPERTY()
	float SwitchCostFactor = 1.15f;

	UPROPERTY()
	float BlastInterruptSeconds = 2.5f;

	UPROPERTY()
	float HealthCliffNorm = 0.35f;

private:
	bool IsHardInterrupt(const FAIBFacts& Facts) const;
	const FAIBObjectiveFact* MatchObjective(const FAIBFacts& Facts, const FGameplayTag& Tag) const;

	UPROPERTY()
	TArray<FAIBAmbitionSpec> Ambitions;

	FGameplayTag CurrentTag;
	float CurrentScore = 0.f;
	double CommitEndSeconds = -1.0;
	float LastRescoreHealthNorm = -1.f;   // for the cliff's "crossed since last" edge
	bool bBlastWasImminent = false;       // for the blast's rising edge
	bool bLastRescoreInterrupted = false;

	TArray<FAIBScoredAmbition> LastScores;
	FAIBScoredAmbition LastRunnerUp;
};
