#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Team/AIBClaimsBoard.h"
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
 * and pawn-backed targets (agents are never claimable — a shared enemy-assignment board
 * is the coordinated-omniscience F-3.7 bans).
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

private:
	FAIBClaimsBoard Board;
};
