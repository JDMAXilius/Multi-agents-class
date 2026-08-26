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
 *  - COMMIT: a newly won ambition holds for its CommitSeconds. Rescoring continues (the
 *    introspection rows stay live) but the winner does not change inside the window.
 *  - HARD INTERRUPTS, exactly two, engine-owned: an incoming blast inside
 *    BlastInterruptSeconds, and vitals crossing below HealthCliffNorm since the last
 *    rescore. An interrupt VOIDS the commit window; it does not pick the winner —
 *    scoring still does, which is why an Evade-shaped ambition must out-score, not
 *    bypass. (343 published the ambitions; commit/interrupt shapes are OUR design.)
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
	float GetCurrentScore() const { return CurrentScore; }
	double GetCommitEndSeconds() const { return CommitEndSeconds; }

	/** Last rescore's full scoreboard, winner included — for the debugger and specs. */
	const TArray<FAIBScoredAmbition>& GetLastScores() const { return LastScores; }

	/** The default core set — Engage / Retreat / Search / SeekWeapon / Roam — with
	 *  C++-authored curves. Phase 8's tables retune these; they never redefine them. */
	static void BuildDefaultCoreAmbitions(TArray<FAIBAmbitionSpec>& OutSpecs);

	// -- the knobs (engine-wide; per-ambition commit lives on the spec) -------------
	UPROPERTY(EditAnywhere, Category = "Engine")
	float SwitchCostFactor = 1.15f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float BlastInterruptSeconds = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Engine")
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

	TArray<FAIBScoredAmbition> LastScores;
};
