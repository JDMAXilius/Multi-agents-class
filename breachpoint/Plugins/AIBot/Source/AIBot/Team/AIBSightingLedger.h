#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"

/**
 * PHASE 12 (AIB23) — SHARED SIGHTINGS, the headless core. FAIRPLAY amendment 2 Sep 2026: a
 * team report is a CALLOUT carrying only what a teammate actually sensed — (1) published
 * only from a candidate whose sight is CURRENT at publish time (the shell re-publishes
 * every pump; an entry not re-published within StaleSeconds is never relayed); (2) it
 * carries the ORIGINAL observation stamp, so F5 decay measures from the seeing; (3) the
 * receiver notes it on its own reaction clock and it lands as MEMORY — never sight, never
 * eligibility, never an aim. Not tier-gated (one tier: Spartan).
 */
struct AIBOT_API FAIBSighting
{
	TWeakObjectPtr<AActor> Target;
	FVector Where = FVector::ZeroVector;
	/** The reporter's own LastSeenAtSeconds — never its Now. */
	double SeenAtSeconds = -1.0;
	/** The telling; refreshed every pump the reporter still has current sight. */
	double PublishedAtSeconds = -1.0;
	FObjectKey Reporter;
	TWeakObjectPtr<const AActor> ReporterPawn;
	FString ReporterName;
};

struct AIBOT_API FAIBSightingLedger
{
	using FAreAllies = TFunctionRef<bool(const AActor*, const AActor*)>;

	/** One entry per (reporter, target); newest publish wins. */
	void Publish(FObjectKey Reporter, const AActor* ReporterPawn, const FString& ReporterName,
		AActor* Target, const FVector& Where, double SeenAtSeconds, double Now);

	/** Visits the freshest (by SeenAt) ALLIED report per target from OTHERS, published
	 *  within StaleSeconds of Now. Self is never its own witness. */
	void ForEachReport(FObjectKey Asker, const AActor* AskerPawn, double Now, float StaleSeconds,
		FAreAllies AreAllies, TFunctionRef<void(const FAIBSighting&)> Visit) const;

	/** Drops stale entries and destroyed targets. */
	void Prune(double Now, float StaleSeconds);

	TArray<FAIBSighting> Reports;
};
