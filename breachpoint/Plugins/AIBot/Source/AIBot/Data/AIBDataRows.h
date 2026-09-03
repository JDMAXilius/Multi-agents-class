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

	// -- the searching look (AIB22, law F9: motion is the default) ------------------
	/** Half-width of the pan around the TRAVEL heading while walking (±). The sweep never
	 *  owns the yaw on the move; it rides the mover's own facing as an offset. */
	UPROPERTY(EditAnywhere, Category = "Search")
	float SweepArcDegrees = 60.f;

	/** Ceiling on the stationary full-circle sweep per still spell, accumulated on the
	 *  CONTROLLER (a StateTree recreates task instance data on every re-entry). Past it
	 *  the sweep releases the yaw and a search at its post ends the want. 0 = never stand
	 *  to sweep. Spartan-only project: every tier carries the same value. */
	UPROPERTY(EditAnywhere, Category = "Search")
	float SweepMaxSeconds = 2.f;

	// -- the island fact and the way off it (AIB22 5(B), law F9) --------------------
	/** Consecutive wander draws with no FULL path before the bot concludes it stands on an
	 *  island (counted on the CONTROLLER across branch re-entries). One full draw clears. */
	UPROPERTY(EditAnywhere, Category = "Roam")
	int32 IslandLatchDraws = 3;

	/** The walk-to-the-lip goal sits this far INSIDE the navmesh boundary (agent radius
	 *  is 35): on-mesh, so the walk there is an ordinary path. */
	UPROPERTY(EditAnywhere, Category = "Roam")
	float IslandLipStandoffUU = 60.f;

	/** How far BEYOND the boundary the landing probe (and the step-off's straight-line
	 *  target) sits. The probe only asks "is there navmesh lower than the lip out there";
	 *  the fall is the move, the landing point is never projected. */
	UPROPERTY(EditAnywhere, Category = "Roam")
	float IslandLipProbeUU = 150.f;

	/** A boundary counts as a LIP (not a wall or a step) when the navmesh beyond it is
	 *  at least this far below; the same number says a landing left the island. */
	UPROPERTY(EditAnywhere, Category = "Roam")
	float IslandMinDropUU = 120.f;

	/** After an Egress FAILURE (no lip, a walk short of it, a body that never left) wander
	 *  draws do not latch for this long (W-REVIEW H1): the bot walks its longest partial
	 *  draws instead of standing in a latch/fail/latch oscillator. */
	UPROPERTY(EditAnywhere, Category = "Roam")
	float EgressCooldownSeconds = 5.f;

	/** A latch older than this reads as unlatched and clears (W-REVIEW M3): the island
	 *  fact was measured where the feet were then, and a bot that chased up a tier since
	 *  must not cash a culvert's latch out as a step off T2. */
	UPROPERTY(EditAnywhere, Category = "Roam")
	float LatchMaxAgeSeconds = 10.f;

	// -- route variety (Phase 14) ---------------------------------------------------
	/** Half-width of the per-life lane weight draw: each lane's travel cost is
	 *  1 ± this before normalisation (the cheapest lane always costs exactly 1). 0 = every
	 *  bot takes the shortest corridor (the conga line); clamped at read to 0.9. Spartan-
	 *  only project: every tier carries the same value. */
	UPROPERTY(EditAnywhere, Category = "Route")
	float RouteLaneWeightSpread = 0.3f;

	// -- separation (Phase 13, AIB24): Detour Crowd owns steering during moves --------
	/** Detour Crowd's separation weight, applied on the crowd follower at possession.
	 *  Below the crowd's own 2.0: with the ini's SeparationDirClamp a pair still parts in
	 *  a doorway without braking for a teammate behind it (the conga). */
	UPROPERTY(EditAnywhere, Category = "Separation")
	float CrowdSeparationWeight = 1.5f;

	/** A teammate inside this radius (two 35uu capsules plus slack) is an OVERLAP for the
	 *  metric and a BODY — not geometry — for the wedge watchdog. HUD-grade: the count
	 *  comes through IAIBWorldQuery::CountNearbyAllies, never positions. */
	UPROPERTY(EditAnywhere, Category = "Separation")
	float TeammateYieldRadiusUU = 80.f;

	/** How long a wedged bot YIELDS to a teammate inside that radius — sprint released,
	 *  the stall clock paused, the crowd's separation left to steer — before the stall
	 *  counts again. One window per wedge; moving 50uu re-arms it. */
	UPROPERTY(EditAnywhere, Category = "Separation")
	float TeammateYieldSeconds = 1.f;

	/** The hill hold's footwork ring as a fraction of the objective's reach radius: strafe
	 *  legs orbit the objective centre at most this far out, so a 55-degree chord's inward
	 *  dip never carries the body off the objective (AIB22 LOW-7: a hold is footwork with
	 *  planted legs, not a statue). */
	UPROPERTY(EditAnywhere, Category = "Separation")
	float HillStrafeRadiusFraction = 0.6f;

	// -- the team mind (AIB23, Phase 12). Awareness is never tier-gated (roadmap §5.2):
	//    every row carries the same values; they are data, not difficulty. ------------
	/** A target claim younger than this is NOT released when Engage exits — it lapses by
	 *  TTL instead — so an Engage that blinks out and back cannot free the slot for a
	 *  teammate and re-take it (the claim/re-claim thrash). */
	UPROPERTY(EditAnywhere, Category = "Team")
	float ClaimMinHoldSeconds = 2.f;

	/** A bot takes at most one team report per target per this interval: a teammate's
	 *  live sight is re-published every pump, and each report is a stimulus on the
	 *  reaction clock (queue capped at AIB::MaxPendingStimuli). */
	UPROPERTY(EditAnywhere, Category = "Team")
	float TeamReportIntervalSeconds = 1.f;

	/** A ledger entry not re-published within this is STALE — the reporter lost sight or
	 *  died — and is never relayed (FAIRPLAY 2 Sep, condition 1: current sight only). */
	UPROPERTY(EditAnywhere, Category = "Team")
	float TeamReportStaleSeconds = 0.5f;

	/** The visit heat grid's cell edge (a cube). Coarse on purpose: exploration reads
	 *  "this corner is cold", not "this square metre". */
	UPROPERTY(EditAnywhere, Category = "Team")
	float VisitHeatCellUU = 500.f;

	/** Heat = exp(−age/this) of the team's freshest footstep in the cell; a cell nobody
	 *  visited reads 0 and wins every wander draw. */
	UPROPERTY(EditAnywhere, Category = "Team")
	float VisitHeatDecaySeconds = 30.f;

	/** Wander draws this many navigable points per attempt and walks the COLDEST (or the
	 *  one nearest a fresh heard fight — AIB17's bias outranks exploration). */
	UPROPERTY(EditAnywhere, Category = "Team")
	int32 VisitHeatDrawSamples = 3;

	// -- Engage tactics (AIB26 / Phase 15: Push · Flank · Hold on the second engine) ----
	/** Flank's commit — the LONGEST of the three, because a flank abandoned halfway is a
	 *  bot standing in the open between two positions. "MinDwell" is this plus the
	 *  engine's SwitchCostFactor; there is no third knob (W-AUDIT P15). */
	UPROPERTY(EditAnywhere, Category = "Tactics")
	float FlankCommitSeconds = 3.5f;

	/** Ring radius, around the midpoint between the feet and the belief, on which the
	 *  eight flank candidates are sampled. */
	UPROPERTY(EditAnywhere, Category = "Tactics")
	float FlankRadiusUU = 700.f;

	/** A flank route (path to the point + point to the belief) longer than this times
	 *  the direct distance is a visible stupid detour: the candidate is dropped, and with
	 *  no candidate left Flank scores 0 (the VETO). Load-bearing (W-AUDIT P14). */
	UPROPERTY(EditAnywhere, Category = "Tactics")
	float FlankMaxDetourFactor = 1.5f;

	/** Hold is a NAMED stillness tactic (law F9) and therefore BOUNDED: past this the
	 *  hold ends and the tactic is suppressed, so a bot cannot hold a ledge all match. */
	UPROPERTY(EditAnywhere, Category = "Tactics")
	float HoldMaxSeconds = 4.f;
};
