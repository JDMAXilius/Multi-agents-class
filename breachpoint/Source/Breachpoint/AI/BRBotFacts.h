// Breachpoint. The bot blackboard: one typed fact struct, normalized at the boundary.

#pragma once

#include "CoreMinimal.h"

#include "BRBotFacts.generated.h"

/**
 * BRBotFacts — the ONE typed struct layer 1 scores against (BREACHPOINT-AI-BOTS.md §2, §6:
 * "no blackboard soup: facts live in one typed struct the brain consumes").
 *
 * THREE PROPERTIES THAT ARE LOAD-BEARING, not style:
 *
 *  1. NOTHING PRIVILEGED. Every field is something this bot could legitimately perceive:
 *     its own ASC state (which a human reads off their own HUD), a target it has actually
 *     perceived through the sight sense, and match state the scoreboard already replicates
 *     to every client. There is deliberately no "enemy health the server knows", no
 *     "position of an unseen player", no aim vector handed down from the engine. If a fact
 *     could not appear on a human's screen, it does not belong in this struct — that check
 *     is the critic's REFUTER line for BP08, and it is meant to be mechanical.
 *
 *  2. NORMALIZED AT THE BOUNDARY. Everything a consideration can score is a float in
 *     [0, 1], produced by ABRBotController from world units BEFORE the struct is handed to
 *     the brain. The consequence is the point: UBRBotBrain contains no metres, no
 *     centimetres, no seconds-to-normalized conversion and therefore no arena constants.
 *     A pure struct in, a decision out.
 *
 *  3. WORLD-FREE. No AActor*, no UObject*, no FVector into the level. A target is an
 *     opaque integer key (its PlayerState's player id). That is what lets
 *     `Breachpoint.Bots.Brain` build a fact struct by hand in a headless spec and pin the
 *     exact decision it produces — the determinism law's precondition.
 *
 * WHO FILLS IT: ABRBotController, on the events named in EBRBotEventType, from ASC queries
 * and perception callbacks. Nobody else writes facts; the brain only reads them.
 */

// =============================================================================================
// Preconditions — GOAP preconditions and GAS state are THE SAME FACTS (AI-BOTS §2)
// =============================================================================================

/**
 * The boolean half of the world-model, as a bitmask.
 *
 * Every bit is answered by an ASC query on the bot's own PlayerState ASC (CanActivateAbility,
 * tag present, cooldown clear, ammo attribute) or by a perception callback. The brain never
 * performs the query — it consumes the answer — which is how the planner keeps GOAP's
 * precondition model without building the "parallel world-model" AI-BOTS §2 forbids.
 *
 * Bits, not bools, for one reason: a plan step stores its requirements as a mask and
 * contradiction detection is then a single AND. Replan-on-contradiction has to be cheap
 * enough that it can happen on every event without anyone being tempted to batch it.
 *
 * The enumerators are BIT INDICES, not bit values, so the enum stays uint8 and can appear
 * in a UPROPERTY (UHT's rule) while the masks stay a plain uint32. BRBotBit() below is the
 * only place the conversion happens.
 */
UENUM()
enum class EBRBotPrecondition : uint8
{
	/** A perceived enemy is currently visible (sight sense reports it as sensed right now). */
	TargetVisible = 0,
	/** A target was perceived recently and its last-known position is still fresh. */
	TargetKnown,
	/** Target is inside the melee ability's range, per that ability's own range data. */
	MeleeInRange,

	/** This bot currently holds the power weapon. */
	HoldingPowerWeapon,
	/** The power weapon is on its pad, or its spawn window is open. */
	PowerWeaponAvailable,

	/** Own State.Shields.Broken is present on the ASC. */
	MyShieldsBroken,
	/** The target was OBSERVED losing its shields (a cue/callout a human also sees). */
	TargetShieldsBroken,

	/** GA_Grenade passes CanActivateAbility right now (cost + cooldown + tags). */
	GrenadeReady,
	/** GA_Grapple passes CanActivateAbility right now. */
	GrappleReady,
	/** Current weapon has rounds in the magazine. */
	HasAmmo,
	/** Magazine is low enough that the tier's reload policy wants a reload. */
	ReloadNeeded,

	/** Standing in a cover volume from the arena manifest's authored vocabulary. */
	InCover,
	/** At least one living teammate is within the tier's regroup radius. */
	TeammateNearby,
	/** More perceived enemies than known friends in the local fight. */
	Outnumbered,

	/** The rocket contest window is open (match beat: T-10 s or pad occupied). */
	RocketWindowOpen,
	/** Match is in warmup — the only phase in which Roam is a legal ambition. */
	Warmup,
	/** This bot is dead / awaiting respawn. Every plan is void while this bit is set. */
	SelfDead,

	Count UMETA(Hidden)
};

/** Bit index -> bit value. The ONE conversion; masks are uint32 everywhere else. */
inline constexpr uint32 BRBotBit(EBRBotPrecondition Precondition)
{
	return 1u << static_cast<uint32>(Precondition);
}

/** The empty mask, spelled out so no caller writes a bare 0 and wonders what it meant. */
inline constexpr uint32 BRBotNoPreconditions = 0u;

// =============================================================================================
// Events — the ONLY things that cause a rescore (AI-BOTS §3: "event-driven rescoring only")
// =============================================================================================

/**
 * Why the brain is being asked to think.
 *
 * There is no `Timer` enumerator and there must never be one: a periodic rescore is the
 * per-frame gameplay poll rulings R10/R12 reject, and it is also what makes bots read as
 * twitchy rather than intentional. If a behaviour seems to need a timer tick, the missing
 * thing is an event, and adding it is a design change, not a tuning change.
 */
UENUM()
enum class EBRBotEventType : uint8
{
	/** Match start / respawn: there is no incumbent ambition yet. */
	Spawned,

	/** Sight sense acquired an enemy (already delayed by the tier's reaction quantum). */
	TargetPerceived,
	/** The perceived target went stale (left sight for longer than the tier's memory). */
	TargetLost,

	/** This bot took damage. Carries no attacker identity a human would not have. */
	DamageTaken,
	/** Own State.Shields.Broken applied — the Halo break-off beat (R12). */
	SelfShieldsBroken,
	/** The current target was observed losing shields — the push beat. */
	TargetShieldsBroken,
	/** An Event.Kill / Event.Death landed near enough to be observable. */
	CombatantDied,

	/** Match beat: the rocket contest window opened (T-10 s). */
	RocketWindowOpened,
	/** Match beat: someone took the rocket. */
	RocketTaken,
	/** Match beat: the score changed. */
	ScoreChanged,
	/** Match beat: warmup ended, sudden death began, etc. */
	MatchPhaseChanged,

	/** The StateTree finished the current plan step successfully. */
	StepCompleted,
	/** The StateTree could not execute the current step (no path, no EQS result, timeout). */
	StepFailed
};

/**
 * One event, with the facts snapshot that was true when it fired.
 *
 * The snapshot travels WITH the event on purpose. The brain must never reach out for
 * "current" facts, because "current" is a wall-clock concept and the determinism law bans
 * it: replaying a recorded (event, facts) trace through the brain has to reproduce the
 * decision trace exactly, and it can only do that if the facts were captured, not fetched.
 */
USTRUCT()
struct BREACHPOINT_API FBRBotEvent
{
	GENERATED_BODY()

	UPROPERTY()
	EBRBotEventType Type = EBRBotEventType::Spawned;

	/**
	 * Opaque identity of the other combatant involved (PlayerState player id), or
	 * INDEX_NONE. An integer rather than a pointer keeps the brain world-free and keeps a
	 * recorded trace replayable in a headless spec.
	 */
	UPROPERTY()
	int32 SubjectId = INDEX_NONE;
};

// =============================================================================================
// The fact struct
// =============================================================================================

/**
 * FBRBotFacts — everything layer 1 is allowed to know.
 *
 * The float fields whose names end in `Norm` are exactly the set a DT_BotAmbitions
 * consideration may name; the CSV spelling of each is the snake_case form given in the
 * comment above it, and ResolveScalar() below is the only bridge between the two. A
 * consideration naming a field that does not exist here is a table error the brain reports
 * loudly rather than scoring as zero — a silently-zero consideration is a balance bug that
 * survives review.
 */
USTRUCT()
struct BREACHPOINT_API FBRBotFacts
{
	GENERATED_BODY()

	// -- Self ---------------------------------------------------------------------------

	/** CSV: my_shield_norm. Own shield attribute / its max. */
	UPROPERTY() float MyShieldNorm = 1.f;
	/** CSV: my_health_norm. Own health attribute / its max. */
	UPROPERTY() float MyHealthNorm = 1.f;
	/** CSV: my_ammo_norm. Rounds in magazine / MagSize of the equipped weapon. */
	UPROPERTY() float MyAmmoNorm = 1.f;

	// -- The perceived target (all zero when nothing is perceived) -----------------------

	/**
	 * CSV: enemy_shield_norm.
	 *
	 * OBSERVED, not read: this is the bot's belief, updated only by the same shield-break
	 * cue and hit feedback a human sees. It is 1.0 for a freshly seen enemy and is NOT
	 * corrected back down from the server's attribute. That inaccuracy is deliberate and is
	 * the difference between an opponent and an aimbot.
	 */
	UPROPERTY() float TargetShieldNorm = 0.f;

	/** CSV: dist_to_target_norm. 1 = adjacent, 0 = at or beyond the arena's 35 m sightline cap. */
	UPROPERTY() float DistToTargetNorm = 0.f;

	/** CSV: target_exposure_norm. How exposed THIS bot is on the line to the target (1 = fully open). */
	UPROPERTY() float ThreatExposureNorm = 0.f;

	// -- The rocket (the arena's one power fantasy, AI-BOTS §3's worked example) ----------

	/** CSV: rocket_timer_norm. 1 = spawning now / on the pad, 0 = a full respawn cycle away. */
	UPROPERTY() float RocketTimerNorm = 0.f;
	/** CSV: dist_to_rocket_norm. 1 = standing on the pad, 0 = the far corner of the arena. */
	UPROPERTY() float DistToRocketNorm = 0.f;

	// -- The local fight -----------------------------------------------------------------

	/** CSV: teammates_alive_norm. Living teammates / team size. */
	UPROPERTY() float TeammatesAliveNorm = 1.f;
	/** CSV: enemies_alive_norm. Living enemies believed alive / team size. */
	UPROPERTY() float EnemiesAliveNorm = 1.f;
	/** CSV: cover_quality_norm. Quality of the cover currently occupied (0 = in the open). */
	UPROPERTY() float CoverQualityNorm = 0.f;

	// -- Match ----------------------------------------------------------------------------

	/** CSV: score_deficit_norm. 0 = tied or ahead, 1 = one point from losing. */
	UPROPERTY() float ScoreDeficitNorm = 0.f;
	/** CSV: time_remaining_norm. 1 = full clock, 0 = expired. */
	UPROPERTY() float TimeRemainingNorm = 1.f;

	// -- Non-scored: preconditions, identity, and the clock ------------------------------

	/** The EBRBotPrecondition bits currently true. Filled from ASC queries by the controller. */
	UPROPERTY() uint32 PreconditionMask = 0;

	/** Opaque id of the currently perceived target, or INDEX_NONE. */
	UPROPERTY() int32 TargetId = INDEX_NONE;

	/**
	 * SERVER match time in seconds, PASSED IN by the controller (GameState's replicated
	 * clock), never read from a platform clock inside the brain. Hysteresis and the commit
	 * window are measured against this and nothing else — this field is the whole reason
	 * the brain can be replayed (AI-BOTS §5: "no wall-clock").
	 */
	UPROPERTY() float NowSeconds = 0.f;

	// -- Helpers (pure, header-only: this unit has no .cpp by design, ARCHITECTURE §3.7) --

	bool Has(EBRBotPrecondition Precondition) const
	{
		return (PreconditionMask & BRBotBit(Precondition)) != 0;
	}

	bool HasAll(uint32 Mask) const { return (PreconditionMask & Mask) == Mask; }
	bool HasNone(uint32 Mask) const { return (PreconditionMask & Mask) == 0; }

	void Set(EBRBotPrecondition Precondition, bool bValue)
	{
		const uint32 BitValue = BRBotBit(Precondition);
		PreconditionMask = bValue ? (PreconditionMask | BitValue) : (PreconditionMask & ~BitValue);
	}

	/**
	 * Resolve a DT_BotAmbitions consideration name to its value.
	 *
	 * THE bridge between the CSV's vocabulary and this struct, and deliberately the only
	 * one. Returns false for an unknown name so the brain can report a bad table row by
	 * name instead of scoring it as zero.
	 */
	bool ResolveScalar(FName ConsiderationName, float& OutValue) const
	{
		if (const float FBRBotFacts::* Member = ScalarRegistry().FindRef(ConsiderationName))
		{
			OutValue = this->*Member;
			return true;
		}
		return false;
	}

	/** Every consideration name a table row may legally use. Order is stable (for diagnostics). */
	static const TMap<FName, float FBRBotFacts::*>& ScalarRegistry()
	{
		// Built once, keyed lookup only — never iterated for scoring, so map ordering can
		// never leak into a decision. (Iteration happens only in validation/diagnostics.)
		static const TMap<FName, float FBRBotFacts::*> Registry = {
			{ FName("my_shield_norm"),       &FBRBotFacts::MyShieldNorm },
			{ FName("my_health_norm"),       &FBRBotFacts::MyHealthNorm },
			{ FName("my_ammo_norm"),         &FBRBotFacts::MyAmmoNorm },
			{ FName("enemy_shield_norm"),    &FBRBotFacts::TargetShieldNorm },
			{ FName("dist_to_target_norm"),  &FBRBotFacts::DistToTargetNorm },
			{ FName("target_exposure_norm"), &FBRBotFacts::ThreatExposureNorm },
			{ FName("rocket_timer_norm"),    &FBRBotFacts::RocketTimerNorm },
			{ FName("dist_to_rocket_norm"),  &FBRBotFacts::DistToRocketNorm },
			{ FName("teammates_alive_norm"), &FBRBotFacts::TeammatesAliveNorm },
			{ FName("enemies_alive_norm"),   &FBRBotFacts::EnemiesAliveNorm },
			{ FName("cover_quality_norm"),   &FBRBotFacts::CoverQualityNorm },
			{ FName("score_deficit_norm"),   &FBRBotFacts::ScoreDeficitNorm },
			{ FName("time_remaining_norm"),  &FBRBotFacts::TimeRemainingNorm }
		};
		return Registry;
	}

	/**
	 * Structural invariant: every scored fact is in [0, 1].
	 *
	 * Pure; call it from `Breachpoint.Bots.Brain` and from the controller's fill path in
	 * development builds. An out-of-range fact does not crash — it silently makes one
	 * ambition dominate forever, which is exactly the class of bug a soak cannot explain.
	 */
	bool ValidateNormalized(FString& OutError) const
	{
		for (const TPair<FName, float FBRBotFacts::*>& Entry : ScalarRegistry())
		{
			const float Value = this->*(Entry.Value);
			if (!FMath::IsFinite(Value) || Value < 0.f || Value > 1.f)
			{
				OutError = FString::Printf(TEXT("Fact '%s' = %f is outside [0,1]"),
					*Entry.Key.ToString(), Value);
				return false;
			}
		}
		return true;
	}
};
