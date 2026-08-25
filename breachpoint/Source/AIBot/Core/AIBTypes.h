#pragma once

#include "CoreMinimal.h"
#include "AIBTypes.generated.h"

/** FAIRPLAY F1: the floor under every reaction, a constant no tier can undercut.
 *  Tiers draw their latencies ABOVE this; the sensorium clamps here, once, at the
 *  single point stimuli mature — never trust N call sites to remember a law. */
namespace AIB
{
	inline constexpr float MinReactionSeconds = 0.20f;
}

/** The combat dance: Halo Infinite's five bot skills (Stern, GDC 2022). A tier is a
 *  LEVEL PER SKILL — a vector, never a scalar. Teamwork rides as a sixth, tier-gated. */
UENUM()
enum class EAIBSkill : uint8
{
	Movement,
	Aim,
	Grenade,
	Melee,
	Confidence,
	Teamwork
};

/** Competence rungs, novice -> expert. Levels gate CAPABILITIES, not just numbers:
 *  a Novice grenade skill cannot evade a blast at any tuning value — that is the
 *  design ("you can catch it with one"), not a limitation. */
UENUM()
enum class EAIBCompetence : uint8
{
	Novice,
	Trained,
	Skilled,
	Expert
};

/**
 * Everything the brain is allowed to know, in one worldless struct — the considerations
 * of the utility layer (Halo's published five, plus what confidence needs). Built once
 * per think by the facts builder (world side); consumed by Brain/ and Skills/, which by
 * module law never see a UWorld. Every field is either sensorium-matured or HUD-grade
 * (FAIRPLAY F3) — a fact that could not have reached a human has no business here.
 */
USTRUCT()
struct AIBOT_API FAIBFacts
{
	GENERATED_BODY()

	// -- self --------------------------------------------------------------------
	float HealthNorm = 1.f;            // 0..1 of max
	float AmmoNorm = 1.f;              // magazine fraction of the HELD weapon
	bool bHasReserveAmmo = true;
	bool bGrounded = true;

	// -- the target, as perceived (not as it is) ---------------------------------
	bool bHasTarget = false;
	bool bTargetVisible = false;       // matured line of sight, not a raw trace
	float TargetHealthNorm = 1.f;      // consideration: is the trade winnable?
	float DistToTargetNorm = 1.f;      // 0..1 over the perception envelope
	float HeightAdvantage = 0.f;       // +up in uu; Halo's vertical consideration
	float LastKnownAgeSeconds = -1.f;  // <0 = no memory (F5: memory decays)

	// -- the fight so far (confidence inputs; our design, flagged as ours) -------
	float RecentDamageTakenNorm = 0.f; // decayed window, fraction of max health
	float RecentDamageDealtNorm = 0.f;
	int32 NearbyAllies = 0;

	// -- the mode (HUD-grade knowledge) ------------------------------------------
	bool bObjectiveActive = false;
	float ObjectiveDistNorm = 1.f;
	float ObjectiveUrgency = 0.f;      // 0..1, the provider's own scaling
};
