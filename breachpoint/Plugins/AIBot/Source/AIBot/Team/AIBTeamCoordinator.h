#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Team/AIBClaimsBoard.h"
#include "Team/AIBSightingLedger.h"
#include "Team/AIBTargetClaims.h"
#include "Team/AIBVisitHeat.h"
#include "AIBTeamCoordinator.generated.h"

class AAIBBotController;

/**
 * PHASE 7 — the claims board's shell (contract landed; "~0" amended to EXACTLY 0 by the
 * P7 audit ruling, 26 Aug 2026: an epsilon survives the engine's zero-score veto and the
 * race loser walks a dead route for its whole commit window; the zero arrives as a
 * PRESENT-zero fact, never an absent one, or ValueWhenUnknown resurrects the want).
 *
 * Server-only world subsystem: pickups and provider-declared objective SLOTS are
 * claimed; a slot claimed by a non-enemy other scores exactly 0 for every other
 * Teamwork-competent bot; claims expire (TTL renewed by the claimant's think — drift
 * releases by non-renewal); tier-gated SYMMETRICALLY by Teamwork competence (a Novice
 * neither files nor honours — it behaves like a human who neither calls out nor
 * listens, which is what keeps low tiers legible and honest). Fixes the failure
 * per-agent utility cannot express — five correct bots all converging on one rocket
 * (Halo shipped this bug; we ship the fix, scoped to Teamwork-competent bots: novices
 * packing IS honest novice play).
 *
 * The board itself is FAIBClaimsBoard (headless — the specs run there); this shell only
 * supplies time, the hostility predicate, and the refusals that need a world: authority,
 * and pawn-backed SLOTS (a provider marking a body a slot is a wiring bug).
 *
 * PHASE 12 (AIB23, W-AUDIT deviation adopted): this IS the server-only per-alliance Team
 * Mind — no new subsystem. Three headless members ride beside the slot board: the TARGET
 * CLAIMS (agents claimable, cap AIB::TargetClaimCap, FAIRPLAY amendment 2 Sep — the
 * "never claimable" rule is REPLACED, deliberately), the SIGHTING LEDGER (callouts), and
 * the VISIT HEAT grid (team-only footsteps). Rename to Team Mind in Phase 15.
 */
UCLASS()
class AIBOT_API UAIBTeamCoordinator : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Grant/renew/deny for the claimant's controller. Authority only, slots only,
	 *  never a pawn-backed target. New grants and denials log; renewals stay quiet. */
	bool TryClaim(const AAIBBotController& Claimant, const FAIBPointOfInterest& Target,
		float TtlSeconds);

	/** THE read: suppressed for Asker? Self-claims answer false. With no world query
	 *  registered the hostility fallback treats every pair as enemies, so nothing is
	 *  ever suppressed — inert and honest, the all-hostile-host case by construction. */
	bool IsClaimedByOtherTeammate(const AAIBBotController& Asker,
		const FAIBPointOfInterest& Target) const;

	/** The unpossess/EndPlay belt. */
	void ReleaseAll(const AAIBBotController& Claimant);

	/** The instrument's count (grep-able proof lines are the call sites'). */
	int32 NumLiveClaims() const;

	// -- Phase 12: target claims (ungated by tier). The caller logs GRANTED/DENIED (it
	//    knows what it does next); this shell logs every RELEASED with its reason. -------
	EAIBTargetClaimResult TryClaimTarget(const AAIBBotController& Claimant, const AActor& Target, int32& OutHolders);
	/** Allied OTHERS holding Target (self excluded): the AlliesOnTarget term and the
	 *  saturation fact. A per-target read for a target the asker already believes in. */
	int32 CountAlliesOnTarget(const AAIBBotController& Asker, const AActor& Target) const;
	bool HoldsTargetClaim(const AAIBBotController& Asker, const AActor& Target) const;
	/** 0 = first holder, 1 = second, INDEX_NONE = holds nothing — the ring-spread seed. */
	int32 GetTargetClaimOrdinal(const AAIBBotController& Asker, const AActor& Target) const;
	/** Engage no longer winning: claims held ≥ MinHold release now (reason=exit), the
	 *  rest lapse by TTL. */
	void ReleaseTargetClaimsOnExit(const AAIBBotController& Claimant, float MinHoldSeconds);

	// -- Phase 12: shared sightings (current sight only, original stamp). --------------
	void PublishSighting(const AAIBBotController& Reporter, AActor& Target, const FVector& Where, double SeenAtSeconds);
	void ForEachTeamReport(const AAIBBotController& Asker, float StaleSeconds,
		TFunctionRef<void(const FAIBSighting&)> Visit) const;

	// -- Phase 12: the team visit heat grid. --------------------------------------------
	void StampVisit(const AAIBBotController& Visitor, const FVector& Where, float CellUU, float DecaySeconds);
	float VisitHeatAt(const AAIBBotController& Asker, const FVector& Where, float CellUU, float DecaySeconds) const;

private:
	/** TTL and death releases, logged. Every mutating path calls it. */
	void PruneTargetClaims(double Now);
	void LogReleases(double Now, const TArray<FAIBReleasedTargetClaim>& Released) const;

	FAIBClaimsBoard Board;
	FAIBTargetClaims TargetClaims;
	FAIBSightingLedger Sightings;
	FAIBVisitHeat VisitHeat;
};
