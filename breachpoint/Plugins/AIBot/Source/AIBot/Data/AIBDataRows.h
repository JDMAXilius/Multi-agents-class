#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/AIBTypes.h"
#include "AIBDataRows.generated.h"

/**
 * A difficulty tier — Halo's model: a VECTOR of competences, one per combat-dance skill,
 * plus the perception envelope. Capability gating lives in the skill policies (a Novice
 * grenade skill cannot evade, at any number); this row only ASSIGNS levels.
 *
 * C++ defaults are the source of truth; DT_AIBTiers mirrors them (one direction of flow —
 * the 13-places lesson). Defaults here are the Marine-shaped baseline; the tier table
 * authored in Phase 8 sets all four tiers.
 */
USTRUCT()
struct AIBOT_API FAIBTierRow : public FTableRowBase
{
	GENERATED_BODY()

	// -- the combat dance, one level per skill ------------------------------------
	UPROPERTY(EditAnywhere, Category = "Skills")
	EAIBCompetence Movement = EAIBCompetence::Trained;

	UPROPERTY(EditAnywhere, Category = "Skills")
	EAIBCompetence Aim = EAIBCompetence::Trained;

	UPROPERTY(EditAnywhere, Category = "Skills")
	EAIBCompetence Grenade = EAIBCompetence::Trained;

	UPROPERTY(EditAnywhere, Category = "Skills")
	EAIBCompetence Melee = EAIBCompetence::Trained;

	UPROPERTY(EditAnywhere, Category = "Skills")
	EAIBCompetence Confidence = EAIBCompetence::Trained;

	UPROPERTY(EditAnywhere, Category = "Skills")
	EAIBCompetence Teamwork = EAIBCompetence::Novice;

	// -- the perception envelope (FAIRPLAY F3's numbers) ---------------------------
	UPROPERTY(EditAnywhere, Category = "Perception")
	float SightRadius = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Perception")
	float LoseSightRadius = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Perception")
	float PeripheralVisionAngleDegrees = 70.f;

	UPROPERTY(EditAnywhere, Category = "Perception")
	float HearingRange = 2200.f;

	// -- the reaction draw; the CLOCK clamps at AIB::MinReactionSeconds (F1), so
	//    these can only slow a tier down — an authored sub-floor number silently
	//    does nothing, which Phase 8's row validator will warn on ----------------
	UPROPERTY(EditAnywhere, Category = "Perception")
	float ReactionSecondsMin = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Perception")
	float ReactionSecondsMax = 0.45f;

	/** F5: how long a last-known position stays worth searching. Clamped at read to
	 *  AIB::MaxMemorySeconds — the ceiling is the module's, the window is the tier's. */
	UPROPERTY(EditAnywhere, Category = "Perception")
	float MemoryFreshSeconds = AIB::DefaultMemoryFreshSeconds;

	/** Engine perception forgets an unseen actor after this age, which is what makes
	 *  the forgotten->loss path fire at all (a MaxAge of 0 = never forget = the
	 *  infinite-sight hole three review passes flagged). */
	UPROPERTY(EditAnywhere, Category = "Perception")
	float SightMaxAgeSeconds = 5.f;
};
