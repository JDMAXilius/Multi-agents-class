#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AIBTypes.generated.h"

/** Module constants — each a law with ONE enforcement site, named in FAIRPLAY.md. */
namespace AIB
{
	/** F1: the floor under every reaction. Tiers draw ABOVE it; the reaction clock is
	 *  the single clamp site. Note 0.20f is not exactly representable — the clamp's
	 *  correctness rests on PopMatured's <= being lawful at the boundary, not on float
	 *  luck (W-REVIEW 25 Aug). */
	inline constexpr float MinReactionSeconds = 0.20f;

	/** F5: the ceiling over every memory. GetFresh clamps caller windows here — the
	 *  single site — so FLT_MAX cannot lawfully compile into infinite memory. */
	inline constexpr float MaxMemorySeconds = 20.f;

	/** The default tier's memory-fresh window. ONE definition (W-REVIEW P3): the tier
	 *  row's C++ default and every task-side fallback reference this — a hand-typed 16
	 *  in two folders is how a Phase-8 Novice tier silently keeps a Marine memory. */
	inline constexpr float DefaultMemoryFreshSeconds = 16.f;

	/** Radius for the builder's world-query asks (POIs, ally count). Objectives are
	 *  HUD-grade so a fair provider may ignore it; it exists so an envelope-bounded
	 *  provider has a bound to honour. */
	inline constexpr float ObjectiveQueryRadiusUU = 10000.f;

	/** THE FUSE-NOISE ENVELOPE (FAIRPLAY amendment, 26 Aug 2026 — closing the open
	 *  blast-fuse ruling): a bot's belief of WHEN a blast detonates is the true fuse
	 *  plus ONE draw from [-Early, +Late], made when the warning enters the reaction
	 *  clock and stored — never recomputed, so two asks about one blast always agree
	 *  (the aim policy's anti-dice-roll law, applied to the ear). Asymmetric on
	 *  purpose: erring EARLY (dodging sooner than needed) is the panicky-human shape
	 *  and free; erring LATE occasionally eats the blast, which is what makes the
	 *  fuse-perfect T-2.5s dodge stop being a tell. */
	inline constexpr float BlastFuseNoiseEarlySeconds = 0.35f;
	inline constexpr float BlastFuseNoiseLateSeconds = 0.15f;

	/** Phase 7: a claim's lease. Renewed each think while the claimant still pursues;
	 *  drift releases by NON-renewal at this horizon — the minimum hold, sized at the
	 *  longest pickup commit window so board transitions never outpace the engine's
	 *  anti-dither machinery. Short enough that a missed release path shadows a slot
	 *  for seconds, not a fight (W-AUDIT P7 ruling, 26 Aug 2026). */
	inline constexpr float ClaimTtlSeconds = 5.f;

	/** The reaction clock's queue cap (drop-oldest). An unpossessed bot must not grow
	 *  an unbounded stimulus backlog for the rest of a match (W-REVIEW F-1.2). */
	inline constexpr int32 MaxPendingStimuli = 64;

	/** The default Engage appetite band, NAMED and sourced: full appetite inside
	 *  EngageFullAppetiteUU, fading toward EngageFadeEndUU == the default tier's
	 *  LoseSightRadius — the band must live INSIDE the sight envelope or the
	 *  consideration is provably inert (W-REVIEW P2 C3: the first band started AT the
	 *  envelope's edge and evaluated to 1.0 on every think, forever). */
	inline constexpr float EngageFullAppetiteUU = 400.f;
	inline constexpr float EngageFadeEndUU = 1500.f;
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

/** Competence rungs, novice -> expert. Levels gate CAPABILITIES, not just numbers. */
UENUM()
enum class EAIBCompetence : uint8
{
	Novice,
	Trained,
	Skilled,
	Expert
};

/** One mode objective as the brain may know it (HUD-grade). Keyed by tag so N mode
 *  ambitions get N distinct consideration inputs — a single flattened triple made
 *  CTF's capture/return/defend inseparable (W-REVIEW F-6.6). */
USTRUCT()
struct AIBOT_API FAIBObjectiveFact
{
	GENERATED_BODY()

	FGameplayTag AmbitionTag;
	float DistanceUU = -1.f;       // raw uu; <0 = unknown
	float Urgency = 0.f;           // CLAMPED 0..1 by the facts builder — the one site.
	                               // 0 = the mode does not care, 1 = drop everything.

	/** Phase 7: every claimable SLOT serving this want is spoken for by a non-enemy
	 *  other. PRESENT-zero, never absence — a removed fact gets resurrected by
	 *  ValueWhenUnknown, and an epsilon survives the engine's zero-score veto; both
	 *  leave the race loser walking a dead route for its whole commit (W-AUDIT P7).
	 *  Stays false whenever any matching ZONE POI exists: zones are not arbitrated. */
	bool bClaimedElsewhere = false;
};

/**
 * Everything the brain is allowed to know, in one worldless struct. Built once per think
 * by the facts builder (Core/ — the world-touching side); consumed by Brain/ and Skills/,
 * which by module law never see a UWorld. Every field is sensorium-matured or HUD-grade
 * (FAIRPLAY F3).
 *
 * SCALE LAW (W-REVIEW F-6.1/6.2): distances and heights are RAW uu; ages are RAW seconds;
 * counts are RAW ints. Normalisation is the response curve's job, against constants the
 * curve names — never a tier-varying divisor baked invisibly into a fact ("0..1 over the
 * perception envelope" made the same 900uu mean different metres per tier, and made
 * absolute weapon falloff inexpressible: the shotgun-at-every-range bug, re-armed).
 * Unknowns are explicit flags, never confident defaults (F-6.10): an unknowable health
 * must not read as full health and make a broken adapter fight to the death.
 */
USTRUCT()
struct AIBOT_API FAIBFacts
{
	GENERATED_BODY()

	// -- self ----------------------------------------------------------------------
	bool bVitalsKnown = false;         // false => HealthNorm is meaningless, score "unknown"
	float HealthNorm = 0.f;            // 0..1 of max, valid only when bVitalsKnown
	float AmmoNorm = 0.f;              // magazine fraction of the held weapon
	bool bHasReserveAmmo = false;
	bool bWeaponCanFight = false;      // the avatar door's assembled answer (F-6.4) —
	                                   // the honest gate on whether this bot can FIGHT,
	                                   // which is a different question from where it
	                                   // should GO (the 25 Aug conflation)
	int32 GrenadeCount = 0;
	bool bGrounded = true;

	// -- the target, as perceived (not as it is) -----------------------------------
	bool bHasTarget = false;
	bool bTargetVisible = false;       // matured sight — a HELD BELIEF during a pending
	                                   // loss (the juke window); pair with
	                                   // bTargetFactsFromMemory to tell live from stale
	bool bTargetAlive = true;
	bool bTargetFactsFromMemory = false; // true => position facts are last-SEEN beliefs,
	                                     // not live reads (the F3 audit marker) — and it
	                                     // is a SELECTOR, so curves can read it too
	bool bTargetVitalsKnown = false;     // TargetHealthNorm/bTargetAlive were actually
	                                     // read this think; false => they are unknowable,
	                                     // never confident defaults (W-REVIEW P2 M3)
	float TargetHealthNorm = 1.f;
	float DistToTargetUU = -1.f;       // raw uu; <0 = unknown
	float HeightAdvantageUU = 0.f;     // raw signed uu, +up (Halo's vertical consideration)

	// -- memory (F5) ----------------------------------------------------------------
	bool bHasMemory = false;           // the -1 sentinel is OUT of band now (F-6.9)
	float LastKnownAgeSeconds = 0.f;   // valid only when bHasMemory
	float MemoryFreshWindowSeconds = 0.f; // the tier's window, so a worldless
	                                      // consideration can normalise age against it

	// -- the incoming blast (the hard interrupt's facts — F-6.3) ---------------------
	bool bIncomingBlast = false;
	float BlastSecondsToDetonation = 0.f;
	FVector BlastCenterRelative = FVector::ZeroVector; // center minus self, so the dodge
	                                                   // consideration needs no world
	float BlastRadius = 0.f;

	// -- the fight so far (confidence inputs; our design, flagged as ours) -----------
	bool bDamageHistoryKnown = false;  // SOURCE (Phase 5): the controller's damage ledger,
	                                   // fed by the host's one-per-hit damage seam through
	                                   // NoteDamageTaken/NoteDamageDealt. A host that never
	                                   // calls them leaves this false and the curves'
	                                   // ValueWhenUnknown governs — never a dead 0
	float RecentDamageTakenNorm = 0.f; // decayed window, fraction of max health, MAY exceed 1
	float RecentDamageDealtNorm = 0.f;
	/** Phase 6: true only when a bounded, honest crowd read EXISTS. Enemies have no
	 *  fair source yet (an unmatured feed was refused — W-AUDIT P6), so this stays
	 *  false and every outnumbered consumer scores unknown, never "confidently alone"
	 *  (the F-6.10 shape the audit caught these two fields in). */
	bool bCrowdKnown = false;
	int32 NearbyAllies = 0;
	int32 NearbyEnemies = 0;           // "am I outnumbered" — the textbook confidence input

	// -- the judgment (Phase 5): the confidence model's output, written by the
	//    controller AFTER the builder runs and BEFORE the brain scores — the one fact
	//    that is computed, not perceived, and it is flagged like every other unknowable
	bool bConfidenceKnown = false;
	float ConfidenceNorm = 0.5f;       // 0 = this fight is lost, 1 = press it

	// -- the mode (HUD-grade; one entry per mode ambition) ---------------------------
	TArray<FAIBObjectiveFact> Objectives;
};
