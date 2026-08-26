#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/ObjectKey.h"
#include "Interfaces/AIBWorldQuery.h"

/**
 * PHASE 7 — the claims board's HEADLESS core (the Perception/ pattern: the subsystem is
 * a thin shell, the logic runs on the mac in seconds with no editor). Time arrives as a
 * parameter, hostility as an injected predicate; the only world types are weak handles
 * used as identity, never dereferenced for state.
 *
 * WHAT A CLAIM IS (W-AUDIT P7, both scopes): negative-only teammate intent — "this slot
 * is spoken for." The read surface is a per-target boolean. No claimant position, no
 * target roster, no intent detail ever crosses; a claim can only SUPPRESS scoring of a
 * target the reader already knows through its own envelope or HUD-grade queries (the
 * refused-unmatured-feed precedent: a board that TEACHES teammates about targets is a
 * laundering channel, F3).
 *
 * WHAT IS CLAIMABLE: provider-declared SLOTS only (FAIBPointOfInterest::bClaimableSlot).
 * Zones are refused here, structurally. Agents are never claimable — the shell refuses
 * pawn-backed targets before the board sees them, and no provider marks a pawn a slot.
 *
 * SCOPE IS ALLIANCE: a claim binds only pairs the injected predicate calls NOT enemies.
 * Between enemies the board must not exist — suppression across teams would be
 * collusion, worse than unfair. In an all-hostile (FFA) host every pair is enemies, so
 * the board is provably inert by construction, not by a flag.
 *
 * LIFECYCLE (the four belts): TTL expiry (renewed by the claimant's think while its
 * ambition still routes at the target — drift releases by NON-renewal, never instantly,
 * so a flapping claimant cannot flap its teammates' arbitration); immediate release on
 * claimant death (a bound pawn going stale reads as expired); ReleaseAll on
 * unpossess/EndPlay; and lazy pruning at query time — no tick (the module's one
 * per-frame surface stays the executor's, ARCHITECTURE's dated exception).
 */

/** Identity of a claimable slot: the backing actor when one exists, else the typed kind
 *  plus a 100uu grid cell — exact float compare would make two queries of one slot two
 *  different claims, and kind-only would make one claim lock every slot of a kind. */
struct AIBOT_API FAIBClaimKey
{
	TWeakObjectPtr<const AActor> Actor;
	FGameplayTag Kind;
	FIntVector Cell = FIntVector::ZeroValue;

	static FAIBClaimKey From(const FAIBPointOfInterest& Target);
	bool SameSlotAs(const FAIBClaimKey& Other) const;
};

struct AIBOT_API FAIBClaim
{
	FAIBClaimKey Key;

	/** The claimant CONTROLLER as opaque identity — compared, never dereferenced. */
	FObjectKey Claimant;

	/** The claimant's body at claim time: alliance scoping needs a pawn to ask the
	 *  hostility predicate about, and a stale handle is the death belt. bPawnBound
	 *  records whether a pawn was ever attached, so a headless spec's pawnless claim
	 *  is not misread as a death (no unproven null-vs-stale API — the flag is ours). */
	TWeakObjectPtr<const AActor> ClaimantPawn;
	bool bPawnBound = false;

	double ExpiresAtSeconds = 0.0;

	bool IsLive(double Now) const
	{
		return Now < ExpiresAtSeconds && (!bPawnBound || ClaimantPawn.IsValid());
	}
};

struct AIBOT_API FAIBClaimsBoard
{
	/** Grant, renew (same claimant), or deny. Refuses non-slot targets outright.
	 *  A live claim by a NON-enemy other denies; an enemy's claim does not bind —
	 *  each alliance runs its own book on the same slot, and none of them leak.
	 *  Returns true when the claimant holds the slot after the call. */
	bool TryClaim(FObjectKey Claimant, const AActor* ClaimantPawn,
		const FAIBPointOfInterest& Target, double Now, float TtlSeconds,
		TFunctionRef<bool(const AActor*, const AActor*)> AreEnemies);

	/** The one read: is this slot held by a living, unexpired, NON-enemy other?
	 *  Self-claims answer false — a bot must never veto its own route (the engine
	 *  releases a commit whose raw score hits zero; a self-suppressing claim would
	 *  make every claimant veto itself one think after claiming). */
	bool IsClaimedByOther(FObjectKey Asker, const AActor* AskerPawn,
		const FAIBPointOfInterest& Target, double Now,
		TFunctionRef<bool(const AActor*, const AActor*)> AreEnemies) const;

	/** The unpossess/EndPlay belt. */
	void ReleaseAll(FObjectKey Claimant);

	/** Dead entries dropped in passing; called by the mutating paths. */
	void Prune(double Now);

	int32 NumLive(double Now) const;

	TArray<FAIBClaim> Claims;
};
