#pragma once

#include "CoreMinimal.h"
#include "Core/AIBTypes.h"
#include "Curves/CurveFloat.h"
#include "AIBConsideration.generated.h"

/**
 * Which fact feeds this consideration. One enum, one switch (in the .cpp), one site —
 * a curve author picks a named input, never writes an accessor. Selectors that read an
 * unknowable fact (vitals unknown, no target, no memory) return UNSET, and the
 * consideration falls back to its authored ValueWhenUnknown — unknown is a state to
 * score, never a confident default (W-REVIEW F-6.10).
 */
UENUM()
enum class EAIBFactSelector : uint8
{
	HealthNorm,             // unset unless bVitalsKnown
	AmmoNorm,
	GrenadeCount,
	WeaponCanFight,         // bool as 0/1
	HasReserveAmmo,         // bool as 0/1
	TargetVisible,          // bool as 0/1
	TargetFactsFromMemory,  // bool as 0/1 — the staleness marker, curve-readable
	TargetHealthNorm,       // unset unless bTargetVitalsKnown
	DistToTargetUU,         // unset when negative (unknown)
	HeightAdvantageUU,      // unset without a target (0 would mean "exactly level")
	MemoryFreshness,        // 1 at just-seen -> 0 at the tier window's edge; unset without memory
	BlastSecondsToDetonation, // unset unless bIncomingBlast
	RecentDamageTakenNorm,  // unset until bDamageHistoryKnown
	RecentDamageDealtNorm,  // unset until bDamageHistoryKnown
	NearbyAllies,
	NearbyEnemies,
	Outnumbered,            // enemies minus allies, signed
	ObjectiveUrgency,       // read from the objective fact MATCHED to this ambition's tag
	ObjectiveDistanceUU     // same; unset when the mode supplied no matching entry
};

/**
 * One scoring input: a named fact, mapped from its RAW range onto 0..1, shaped by a
 * response curve, raised to a weight. Worldless; evaluable headless.
 *
 * THE SCALE LAW LIVES HERE: facts arrive in raw units (uu, seconds, counts) and
 * InputMin/InputMax are where a curve author states, visibly, what range they mean —
 * never a tier-varying divisor hidden in the fact (W-REVIEW F-6.1/6.2). The curve is an
 * inline FRuntimeFloatCurve so C++ defaults author it and tables can retune it; output
 * is clamped 0..1. Weight is an exponent: 1 = as authored, 0 = disabled, >1 sharpens.
 */
USTRUCT()
struct AIBOT_API FAIBConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration")
	EAIBFactSelector Selector = EAIBFactSelector::HealthNorm;

	UPROPERTY(EditAnywhere, Category = "Consideration")
	float InputMin = 0.f;

	UPROPERTY(EditAnywhere, Category = "Consideration")
	float InputMax = 1.f;

	UPROPERTY(EditAnywhere, Category = "Consideration")
	FRuntimeFloatCurve Curve;

	UPROPERTY(EditAnywhere, Category = "Consideration")
	float Weight = 1.f;

	/** Scored when the selector reads an unknowable fact. 0.5 = indifferent;
	 *  0 = an unknown vetoes this ambition; 1 = an unknown waves it through. */
	UPROPERTY(EditAnywhere, Category = "Consideration")
	float ValueWhenUnknown = 0.5f;

	/** 0..1. MatchedObjective is the objective fact joined to the scoring ambition's
	 *  tag, null when the mode supplied none — only the two Objective* selectors read
	 *  it (the join that kept CTF's three ambitions separable, W-REVIEW F-6.6). */
	float Evaluate(const FAIBFacts& Facts, const FAIBObjectiveFact* MatchedObjective) const;

	/** A linear 0->1 ramp, the default shape; helpers for C++-authored defaults. */
	void SetLinearCurve(bool bRising = true);
};
