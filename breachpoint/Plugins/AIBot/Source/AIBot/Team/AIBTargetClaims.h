#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"

/**
 * PHASE 12 (AIB23) — TARGET CLAIMS, the headless core. FAIRPLAY amendment 2 Sep 2026:
 * agents ARE claimable, capped at AIB::TargetClaimCap per target per alliance, and a claim
 * stays NEGATIVE-ONLY teammate intent — it may lower the score of a target the reader
 * already believes in; it is never enumerable, never carries a position, a claimant
 * location, or a health read. The read surface is a per-target COUNT and ORDINAL.
 *
 * Same shape as FAIBClaimsBoard (time as a parameter, alliance as an injected predicate,
 * actors as opaque weak handles): grant/renew/deny; renew per think; release by NON-renewal
 * at TTL, on Engage exit past a minimum hold, on target death via the injected liveness,
 * and ReleaseAll on unpossess/EndPlay. UNGATED by Teamwork tier — a non-claiming bot
 * would reopen the pile. Every release is returned to the shell with its reason so the
 * metric line can name it.
 */
enum class EAIBTargetClaimResult : uint8 { Granted, Renewed, Denied };
enum class EAIBTargetClaimRelease : uint8 { Ttl, Exit, Death, Unpossess };

struct AIBOT_API FAIBTargetClaim
{
	TWeakObjectPtr<const AActor> Target;
	FObjectKey Claimant;
	TWeakObjectPtr<const AActor> ClaimantPawn;
	double GrantedAtSeconds = 0.0;
	double ExpiresAtSeconds = 0.0;
	/** Log-only names captured at grant: a corpse has no name to give at release. */
	FString ClaimantName;
	FString TargetName;

	bool IsLive(double Now) const { return Now < ExpiresAtSeconds && Target.IsValid(); }
};

struct AIBOT_API FAIBReleasedTargetClaim
{
	FString ClaimantName;
	FString TargetName;
	EAIBTargetClaimRelease Reason = EAIBTargetClaimRelease::Ttl;
};

struct AIBOT_API FAIBTargetClaims
{
	using FAreAllies = TFunctionRef<bool(const AActor*, const AActor*)>;

	/** Grant, renew (same claimant), or deny at the cap. Only ALLIED live claims count
	 *  toward the cap — each alliance runs its own book. OutHolders = allied live claims
	 *  on Target after the call, the asker's included when held (the line's k). */
	EAIBTargetClaimResult TryClaim(FObjectKey Claimant, const AActor* ClaimantPawn, const AActor* Target,
		double Now, float TtlSeconds, FAreAllies AreAllies, int32& OutHolders,
		const FString& ClaimantName = FString(), const FString& TargetName = FString());

	/** Allied OTHERS holding Target now (self excluded) — the AlliesOnTarget term's source. */
	int32 CountAlliesOn(FObjectKey Asker, const AActor* AskerPawn, const AActor* Target,
		double Now, FAreAllies AreAllies) const;

	/** The asker's rank among allied holders by grant order (0 = first), INDEX_NONE when
	 *  it holds nothing — the ring-spread seed. */
	int32 Ordinal(FObjectKey Asker, const AActor* AskerPawn, const AActor* Target,
		double Now, FAreAllies AreAllies) const;

	bool Holds(FObjectKey Claimant, const AActor* Target, double Now) const;

	/** Engage exit: the claimant's claims held at least MinHoldSeconds release now (Exit);
	 *  younger ones stay and lapse by TTL — the hysteresis against a blink. */
	void ReleaseOnExit(FObjectKey Claimant, double Now, float MinHoldSeconds,
		TArray<FAIBReleasedTargetClaim>& OutReleased);

	void ReleaseAll(FObjectKey Claimant, TArray<FAIBReleasedTargetClaim>& OutReleased);

	/** Expired -> Ttl; a target the predicate no longer calls a live enemy of the claimant
	 *  (or a destroyed one) -> Death. Called by every mutating path. */
	void Prune(double Now, TFunctionRef<bool(const AActor* ClaimantPawn, const AActor* Target)> IsLiveEnemy,
		TArray<FAIBReleasedTargetClaim>& OutReleased);

	int32 NumLive(double Now) const;

	TArray<FAIBTargetClaim> Claims;
};
