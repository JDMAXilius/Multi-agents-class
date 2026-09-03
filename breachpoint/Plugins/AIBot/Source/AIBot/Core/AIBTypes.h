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

	// ---- TRAVERSAL REACH (founder, 1 Sep) --------------------------------------------
	// "make sure that they are able to go up, down one platform to one platform by just
	// jumping or doing dash... if it's possible to just dash to that platform, that will
	// be something that the AI should be capable of doing."
	//
	// THREE OF THESE FOUR ARE [thin] AND SAY SO. BN32's rig is committed and unrun, and
	// it owns them — see TICKET_BN32's "the four numbers F2 is waiting on". A traversal
	// behaviour built on guessed reach is how the Aquarius blockout got ramps to nowhere,
	// so the chooser is deliberately CONSERVATIVE until they are measured: it refuses a
	// crossing it is not sure of, and a refused crossing is the wander the bot was doing
	// anyway.

	/** [MEASURED, BN32 CDO read] JumpZ 420 against gravity 1 gives a 0.90 m apex — which
	 *  is also why BN_Climb's navlink can only express a 0.70 m climb. */
	inline constexpr float JumpReachUpUU = 90.f;

	/** [thin] Sprint speed x airtime, arithmetic, never observed. BN32 owes the walked
	 *  and the sprinted number; the chooser must take the SMALLER, because a bot that
	 *  decides to jump from a standstill must not fall in the gap. */
	inline constexpr float JumpReachAcrossUU = 260.f;

	/** [thin] A COMMENT in the blast-scatter code says the dash "covers ~500uu in a
	 *  quarter second". A comment is not a measurement, and this is the constant the
	 *  whole gap-crossing behaviour rests on. */
	inline constexpr float DashReachUU = 500.f;

	/** Derived from BN_Drop's JumpMaxDepth in DefaultEngine.ini — a navlink's WILLINGNESS
	 *  to path a drop, which is not the same as a proven survivable fall. BN32 owes the
	 *  height where damage starts and the height that kills. */
	inline constexpr float SafeDropUU = 1000.f;

	/** A crossing shorter than this is not a crossing — the navmesh has it, and a bot
	 *  hopping over every doorstep reads as broken rather than agile. */
	inline constexpr float TraversalMinGapUU = 120.f;

	/** Safety fraction applied to every reach above. The bot commits at 80% of what it
	 *  can do, because landing exactly on a lip is landing in the gap half the time and
	 *  the cost of being wrong is a death, not a delay. */
	inline constexpr float TraversalCommitFraction = 0.80f;

	// ---- TARGET SELECTION (founder, 1 Sep) -------------------------------------------
	// The bot had no opinion about who its enemy was: the sensorium kept one slot and the
	// newest matured sighting overwrote it. These are the weights that give it one. They
	// are deliberately readable as a sentence — seeing beats remembering, close beats far,
	// being shot at beats both when nothing else is pressing, and the enemy you are
	// already fighting keeps the benefit of the doubt.

	/** Currently in sight. The largest single term that is not being-shot-at: a target you
	 *  can see is one you can act on. */
	inline constexpr float TargetVisibleWeight = 1.00f;

	/** Closeness, linear to TargetConsiderRangeUU. The founder's "there's a possible new
	 *  target closer" — and what keeps a bot from ignoring the person in its face. */
	inline constexpr float TargetProximityWeight = 0.85f;

	/** WHO IS SHOOTING ME. Weighted ABOVE visibility on purpose, so an unseen attacker can
	 *  win against nothing — that is the whole complaint ("it's not knowing who is
	 *  shooting him"). It cannot win against a visible enemy at knife range, which is
	 *  correct: 1.20 alone loses to 1.00 + 0.85 + the incumbent's bonus. A bot being shot
	 *  from a rooftop while someone is stabbing it deals with the knife. */
	inline constexpr float TargetThreatWeight = 1.20f;

	/** How fast being-shot-at fades. Short, because it is about the CURRENT fight: after
	 *  ~3 half-lives the shooter is just another remembered enemy. */
	inline constexpr float TargetThreatHalfLifeSeconds = 4.0f;

	/** How recently the target was actually perceived, over the tier's memory window. */
	inline constexpr float TargetFreshnessWeight = 0.45f;

	/** THE PERSISTENCE BONUS — the founder's "I would try to be as persistent as possible,
	 *  realistically, to kill this target." The enemy already being fought carries this,
	 *  so a marginally better option is not a better option. */
	inline constexpr float TargetIncumbentBonus = 0.35f;

	/** And a challenger must beat the incumbent by THIS on top of the bonus. Two separate
	 *  knobs because they answer different questions: the bonus is how much the current
	 *  fight is worth, the margin is how much better a new one has to look before it is
	 *  worth abandoning. Together they are what stops a bot ping-ponging between two
	 *  enemies at similar range, which is the failure mode of every naive "nearest enemy"
	 *  selector. */
	inline constexpr float TargetSwitchMargin = 0.15f;

	/** Beyond this, proximity scores zero. Past the sight envelope on purpose: a
	 *  remembered or shooting enemy further out is still a candidate, just not a near
	 *  one. */
	inline constexpr float TargetConsiderRangeUU = 4000.f;

	/** How long an aimer's yaw claim suppresses facing-travel. Longer than one frame,
	 *  shorter than a think interval: enough that a mover stepped between two aim ticks
	 *  does not twitch the body back toward its path, short enough that the moment aiming
	 *  stops the bot turns and runs properly. */
	inline constexpr double YawClaimHoldSeconds = 0.25;

	/** How fast a bot turns to face where it is walking. Not the aim rate: this is a body
	 *  turning, and a body that snapped to its heading would read as a turret on rails.
	 *  Fast enough that a corner does not produce a visible sidestep. */
	inline constexpr float TravelFacingTurnRateDeg = 420.f;

	/** Below this ground speed the velocity vector is noise, so facing follows the GOAL
	 *  instead — which is also what makes a bot turn to face its destination before it
	 *  starts moving, rather than setting off sideways and correcting. */
	inline constexpr float TravelFacingMinSpeedUU = 60.f;

	/** AIB22 W-REVIEW H2: the scan a bot holding an objective makes — the founder's
	 *  named-hold exception, UNBUDGETED because a hill is held by standing on it, and
	 *  slow because a guard looks about, it does not spin. Search's post sweep is the
	 *  budgeted one (SweepDegreesPerSecond, SweepMaxSeconds). */
	inline constexpr float HoldScanDegreesPerSecond = 40.f;

	/** AIB22 fix #4 (R3/R6): ONE STEP of the host's character — the engine's CMC
	 *  MaxStepHeight default, [thin] not measured on the BN pawn. A stalled goal farther
	 *  than this above or below the feet with no link ahead is a STOREY, not a wedge
	 *  (abandon at once); navmesh farther than this under the feet is OFF-MESH (recover). */
	inline constexpr float StepHeightUU = 45.f;

	/** AIB22 fix #4 (R4): the sweep budget's refill key. The BOT — never the post —
	 *  displaced 1.5x this since the last refill earns a new look; mirrors the search
	 *  post's acceptance (MoveToLastKnown's 150) so "a new post" and "a new look" agree. */
	inline constexpr float SweepRefillRadiusUU = 150.f;

	/** A dry bot reads a melee fight from this much further out (founder, 1 Sep). A
	 *  multiplier rather than an absolute, so it rides the competence ladder instead of
	 *  flattening it — an Expert with an empty hand still reads the fight before a
	 *  Novice with one. RANGE only: no reaction time moves, at any tier. */
	inline constexpr float EmptyHandedMeleeRangeFactor = 2.0f;

	/** The range the swap scorer is asked about when the bot has NO target and therefore
	 *  no distance to reason from. A bot holding the empty holster must still draw
	 *  something while it walks, and "what is best at a typical engagement" is a better
	 *  default than "stay unarmed until someone shoots me". Mid-map, deliberately: it
	 *  should pull the general-purpose weapon, not the specialist at either extreme. */
	inline constexpr float NoTargetSwapRangeUU = 1200.f;

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
	/** How long a bot waits between dashes. A BOT-SIDE guess at the host's cooldown, held
	 *  deliberately LONGER than the host's own (3s at time of writing) so the bot is never the
	 *  thing that discovers the real number by being refused — the module owns no ability
	 *  system and must not learn one. */
	inline constexpr float DashThrottleSeconds = 3.5f;

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
/** AIB17 — the ally-fight note: ONE heard place and its stamp, controller-owned per
 *  life. Deliberately NOT a target memory and NOT a fact (F-4.5: a sense added later
 *  must not silently become vision; F5-C: a friendly must never become a stimulus):
 *  the only consumer is the idle wander's destination draw. Newest-wins; decays. */
struct AIBOT_API FAIBAllyFightMemory
{
	FVector HeardAt = FVector::ZeroVector;
	double HeardAtSeconds = -1.0;

	/** How long a heard fight stays worth walking toward. Past this the fight has
	 *  moved or ended, and biasing a wander at stale noise reads as clairvoyance. */
	static constexpr double FreshSeconds = 8.0;

	bool IsFresh(double Now) const
	{
		return HeardAtSeconds >= 0.0 && (Now - HeardAtSeconds) <= FreshSeconds;
	}
};

USTRUCT()
struct AIBOT_API FAIBFacts
{
	GENERATED_BODY()

	// -- self ----------------------------------------------------------------------
	bool bVitalsKnown = false;         // false => HealthNorm is meaningless, score "unknown"
	float HealthNorm = 0.f;            // 0..1 of max, valid only when bVitalsKnown
	/** 0..1 of max shield, valid only when bVitalsKnown. Rides the SAME known flag as
	 *  HealthNorm because they come from one avatar read — a host that can answer one can
	 *  answer both, and a second flag would only ever disagree by being wrong. */
	float ShieldNorm = 1.f;
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
