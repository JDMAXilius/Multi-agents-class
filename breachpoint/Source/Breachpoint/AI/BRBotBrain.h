// Breachpoint. Layer 1 — the ambition layer. Pure, headless, seeded, world-free.

#pragma once

#include "AI/BRBotFacts.h"
#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "BRBotBrain.generated.h"

class UDataTable;
class UStateTree;

/**
 * UBRBotBrain — LAYER 1 of BREACHPOINT-AI-BOTS.md: the GOAP-style ambition layer.
 *
 * WHAT IT DOES: given facts, an ambition table and a tier's tuning scalars, it answers
 * "what do I want" (an Ambition) and "what are the next few things that get me there"
 * (a plan of <= 3 steps, ruling R10). It answers ONLY when an event asks it to.
 *
 * WHAT IT IS: a plain UObject with no world, no owner, no timers, no tick, no engine
 * singletons, no wall-clock, no unseeded randomness. `NewObject<UBRBotBrain>()` in a
 * headless spec, feed it a fact struct and an event, and it produces a decision — that is
 * the whole test surface of `Breachpoint.Bots.Brain`, and it is why the determinism claim
 * is provable rather than asserted.
 *
 * WHAT IT DOES NOT DO — the iron rule of the discipline: it never touches the world. It
 * cannot move a pawn, apply an effect, spawn anything, or read an attribute. It emits an
 * intent; ABRBotController turns that intent into StateTree state; the StateTree presses
 * InputTags; GAS does everything that actually happens. Every arrow points one way.
 *
 * DETERMINISM, CONCRETELY (AI-BOTS §5):
 *   - the ONLY entropy source in this class is `Stream`, an FRandomStream seeded once in
 *     Initialize() from a seed the caller supplies (match seed XOR bot slot index);
 *   - `FMath::RandRange` / `FMath::FRand` / `FMath::Rand` appear nowhere in AI/ and the
 *     pre-tool hook blocks them, so this is enforced, not promised;
 *   - the only clock is FBRBotFacts::NowSeconds, which the caller supplies from the
 *     replicated match clock;
 *   - scoring iterates `AmbitionDefs`, a TArray in table order — never a TMap, whose
 *     iteration order depends on hashing and would make ties resolve differently between
 *     runs of the same seed.
 *   Same seed + same tuning scalars + same (event, facts) trace => identical decision trace.
 */

// =============================================================================================
// The vocabulary: ambitions, step verbs, anchors
// =============================================================================================

/**
 * The six ambitions of AI-BOTS §2.
 *
 * The enumerator names ARE the row names of `Content/Data/DT_BotAmbitions.csv` (same
 * convention as EBRWeaponFireMode's relationship to DT_Weapons: the table's text is matched
 * against the enumerator name, so renaming one here breaks the table, not the build). The
 * enum exists so the StateTree can compare an ambition in one instruction and so a decision
 * trace is diffable; the table remains the source of truth for what each ambition is WORTH.
 */
UENUM()
enum class EBRBotAmbition : uint8
{
	/** No ambition scored above zero, or the bot is dead. The StateTree idles. */
	None,
	/** Own the power weapon's timer and its pad. The tier fantasy Veteran expresses (R12). */
	ControlRocket,
	/** Kill the thing in front of me. */
	WinThisFight,
	/** Break off, get shields back. The Halo shield-crack read (R12). */
	Survive,
	/** Occupy an authored landmark and hold it. */
	TakeGround,
	/** Go get a weapon I do not have. */
	HuntWeapon,
	/** Warmup only. Move through the arena legibly, fight nothing. */
	Roam,

	Count UMETA(Hidden)
};

/** Text form of an ambition — used by decision traces, callouts and spec failure messages. */
BREACHPOINT_API const TCHAR* LexToString(EBRBotAmbition Ambition);

/** Row name (DT_BotAmbitions key) -> ambition. Returns None for an unknown row. */
BREACHPOINT_API EBRBotAmbition BRBotAmbitionFromRowName(FName RowName);

/**
 * What a single plan step asks the spine to do.
 *
 * These are VERBS, not abilities. `Engage` does not mean "fire" — it means "the Engage
 * state owns this step", and Engage's internal priority selector decides which InputTag
 * gets pressed. That separation is R9: the ambition layer never names a button.
 */
UENUM()
enum class EBRBotStepVerb : uint8
{
	/** Nothing to do. */
	Idle,
	/** Get to the anchor, path chosen by EQS over the manifest's authored vocabulary. */
	MoveTo,
	/** Stay put and keep a sightline onto the anchor. */
	Hold,
	/** Fight the current target (Engage state; BT-shaped selector inside). */
	Engage,
	/** Force a target out of cover (grenade / splash), then re-engage. */
	Flush,
	/** Move to better ground while still in the fight. */
	Reposition,
	/** Break contact toward cover away from the threat. */
	Retreat,
	/** Take the power weapon, or deny it if someone else is on the pad. */
	PickupOrContest,
	/** Move to a living teammate. */
	Regroup,

	Count UMETA(Hidden)
};

BREACHPOINT_API const TCHAR* LexToString(EBRBotStepVerb Verb);

/**
 * WHERE a step happens, in the arena's authored vocabulary (AI-BOTS §4, Halo lesson 2).
 *
 * Anchors are named categories, not coordinates: the brain says "the rocket pad", and EQS
 * resolves it against `arena_manifest.json`'s landmarks/cover/perches at execution time.
 * A coordinate in this enum's place would be nav divination and would also drag world units
 * into a world-free class.
 */
UENUM()
enum class EBRBotAnchor : uint8
{
	None,
	/** The perceived target's last known position. */
	Target,
	/** Cover scored against the current threat direction (manifest `cover[].height_class`). */
	Cover,
	/** The power weapon pad landmark ("The Core"). */
	RocketPad,
	/** A firing point with a sightline onto the current anchor, inside the 35 m cap. */
	FiringPoint,
	/** A grapple perch from `landmarks[]`. */
	Perch,
	/** A living teammate. */
	Teammate,
	/** Any contested landmark that is not the rocket pad. */
	Landmark,

	Count UMETA(Hidden)
};

BREACHPOINT_API const TCHAR* LexToString(EBRBotAnchor Anchor);

// =============================================================================================
// Table views — the runtime shape of DT_BotAmbitions / DT_BotTuning
// =============================================================================================

/**
 * One consideration reference on an ambition row: which coded evaluator, and how much it
 * matters. The evaluator is CODE (a field of FBRBotFacts, resolved by name); the weight is
 * DATA. That split is the data-is-not-code law applied to utility scoring: adding a
 * consideration is a code change and a review; re-weighting one is a CSV diff.
 */
USTRUCT()
struct BREACHPOINT_API FBRBotConsiderationDef
{
	GENERATED_BODY()

	/** A key of FBRBotFacts::ScalarRegistry() — e.g. `rocket_timer_norm`. */
	UPROPERTY()
	FName Consideration;

	/**
	 * Influence in [0, 1]. 0 means "this consideration cannot change the score" (the
	 * neutral element of the multiplicative combine), 1 means "a zero fact zeroes this
	 * ambition". Anything outside the range is a table error, not a strong opinion.
	 */
	UPROPERTY()
	float Weight = 0.f;

	/** Score on (1 - fact) instead of the fact. `my_shield_norm` inverted IS "I am hurt". */
	UPROPERTY()
	bool bInvert = false;
};

/**
 * The runtime view of ONE row of `Content/Data/DT_BotAmbitions.csv`.
 *
 * WHY THIS IS NOT THE ROW STRUCT: law 3 puts every `DT_` row struct in
 * `Source/Breachpoint/Data/BRDataRows.h`, and BP08 does not own that header. So the row
 * struct `FBRBotAmbitionRow` belongs there (its required shape is spelled out on
 * ReadAmbitionDefs() below), and this is the pure, world-free, asset-free struct the brain
 * actually scores — which is also what lets a headless spec build ambitions in C++ with no
 * DataTable asset at all. The CSV remains the single source of truth for every number.
 */
USTRUCT()
struct BREACHPOINT_API FBRBotAmbitionDef
{
	GENERATED_BODY()

	/** The DT_BotAmbitions row key, kept for diagnostics and trace readability. */
	UPROPERTY()
	FName RowName;

	/** Resolved from RowName. None means the table names an ambition C++ does not implement. */
	UPROPERTY()
	EBRBotAmbition Ambition = EBRBotAmbition::None;

	/** CSV: base_utility. The unmodified worth of this ambition before any fact is read. */
	UPROPERTY()
	float BaseUtility = 0.f;

	/** CSV: considerations. Combined multiplicatively, in table order. */
	UPROPERTY()
	TArray<FBRBotConsiderationDef> Considerations;

	/**
	 * CSV: personality_scalar. Names the DT_BotTuning column that shapes this ambition for
	 * this tier (`rocket_contest`, `push_threshold`, `cover_preference`), or NAME_None.
	 * This one column is how "Veteran times the rocket, Recruit over-commits" is a data
	 * statement rather than three trees.
	 */
	UPROPERTY()
	FName PersonalityScalar;

	/** Preconditions without which this ambition cannot be scored at all (bitmask). */
	UPROPERTY()
	uint32 RequiresMask = 0;

	/** Preconditions any one of which forbids this ambition (bitmask). */
	UPROPERTY()
	uint32 ForbidsMask = 0;
};

/**
 * The runtime view of ONE row of `Content/Data/DT_BotTuning.csv` — a tier's personality.
 *
 * Same reasoning as FBRBotAmbitionDef: `FBRBotTuningRow` lives in BRDataRows.h (BP08 does
 * not own it); this is the pure struct the brain and controller consume.
 *
 * EVERY DEFAULT BELOW IS A SENTINEL, NOT A TUNING VALUE. An unconfigured bot must be
 * visibly inert and complain, never quietly playable at some accidental difficulty — a
 * plausible default is how an untuned scalar ships. The one exception is ReactionMs, whose
 * default IS the law floor.
 */
USTRUCT()
struct BREACHPOINT_API FBRBotTierScalars
{
	GENERATED_BODY()

	/** DT_BotTuning row key: Recruit / Regular / Veteran. */
	UPROPERTY()
	FName TierName;

	/**
	 * CSV: reaction_ms. Milliseconds between a perception event and the brain being told.
	 *
	 * RULING R11 IS ENFORCED IN THE ACCESSOR, not at import and not by convention: no tier,
	 * no scalar combination, and no future caller can produce a sub-200 ms reaction, because
	 * the only way to read this value clamps it. `Breachpoint.Bots.Brain` pins that a row
	 * carrying 50 still yields 200.
	 */
	UPROPERTY()
	int32 ReactionMs = 0;

	/** CSV: reaction_quantum_ms. Reaction delays are quantized to this step (see QuantizeMs). */
	UPROPERTY()
	int32 ReactionQuantumMs = 0;

	/** CSV: reaction_jitter_ms. Seeded jitter added before quantization. 0 = none. */
	UPROPERTY()
	int32 ReactionJitterMs = 0;

	/** CSV: accuracy_pct. Fraction of shots aimed true; the complement becomes aim error. */
	UPROPERTY()
	float AccuracyPct = 0.f;

	/** CSV: aim_error_deg. Maximum cone half-angle applied BEFORE the fire ability activates. */
	UPROPERTY()
	float AimErrorDeg = 0.f;

	/** CSV: switch_margin. Fractional score margin a challenger must beat the incumbent by. */
	UPROPERTY()
	float SwitchMargin = 0.f;

	/** CSV: commit_window_ms. Minimum time an ambition is held before it may be replaced. */
	UPROPERTY()
	int32 CommitWindowMs = 0;

	/** CSV: commit_jitter_ms. Seeded jitter on the commit window, so a squad does not think in lockstep. */
	UPROPERTY()
	int32 CommitJitterMs = 0;

	/** CSV: rocket_contest. Personality scalar; named by an ambition row's personality_scalar. */
	UPROPERTY()
	float RocketContest = 0.f;

	/** CSV: push_threshold. Personality scalar. */
	UPROPERTY()
	float PushThreshold = 0.f;

	/** CSV: cover_preference. Personality scalar. */
	UPROPERTY()
	float CoverPreference = 0.f;

	/** CSV: sight_radius_m / sight_fov_deg. Perception geometry; the same information a human's screen gives. */
	UPROPERTY()
	float SightRadius_m = 0.f;
	UPROPERTY()
	float SightFov_deg = 0.f;

	/** CSV: target_memory_s. How long a lost target stays TargetKnown. */
	UPROPERTY()
	float TargetMemory_s = 0.f;

	/**
	 * CSV: engage_update_ms. How often the Engage selector re-runs while a fight is on.
	 *
	 * This is a TIMER INTERVAL, not a tick: it is the rate at which a bot re-aims and
	 * re-picks a branch, and it is data so that a tier can literally think slower. Zero
	 * means "never re-run", which is the correct inert behaviour for an unconfigured tier.
	 */
	UPROPERTY()
	int32 EngageUpdateMs = 0;

	/** CSV: StateTreeSoftPath. SOFT ref to ST_Bot (law 3: no hard asset refs in C++). */
	UPROPERTY()
	TSoftObjectPtr<UStateTree> StateTreeSoftPath;

	/** RULING R11's floor. A law constant, not a tuning number: it is never tuned, by definition. */
	static constexpr int32 ReactionFloorMs = 200;

	/**
	 * The ONLY legal way to read reaction time. Clamped at the floor, always.
	 * @param Jitter  seeded jitter in [0, ReactionJitterMs], drawn by the caller from ITS stream.
	 */
	int32 GetReactionMsClamped(int32 Jitter = 0) const
	{
		const int32 Raw = FMath::Max(ReactionMs, ReactionFloorMs) + FMath::Max(Jitter, 0);
		return QuantizeMs(Raw, ReactionQuantumMs);
	}

	/**
	 * Round UP to the quantum. Quantization is what makes a reaction reproducible across
	 * machines whose frame boundaries differ; rounding up is what keeps it from ever
	 * dropping below the floor.
	 */
	static int32 QuantizeMs(int32 Value, int32 Quantum)
	{
		if (Quantum <= 0)
		{
			return Value;
		}
		return ((Value + Quantum - 1) / Quantum) * Quantum;
	}

	/** Personality scalar by CSV column name, for FBRBotAmbitionDef::PersonalityScalar. */
	bool ResolvePersonalityScalar(FName ScalarName, float& OutValue) const;

	/** Structural invariants. Not balance: nothing here pins a tuning value. */
	bool ValidateSchema(FString& OutError) const;
};

// =============================================================================================
// Plans — <= 3 steps, ruling R10
// =============================================================================================

/**
 * One step. A verb, a place, and the conditions under which it is still a sane thing to do.
 *
 * The precondition masks are the contradiction detector: after every event the brain
 * re-tests the CURRENT step against the new facts, and a step whose requirements have
 * evaporated forces a replan. AI-BOTS §3: "a surviving contradiction is a bug, not
 * adaptation."
 */
USTRUCT()
struct BREACHPOINT_API FBRBotPlanStep
{
	GENERATED_BODY()

	UPROPERTY()
	EBRBotStepVerb Verb = EBRBotStepVerb::Idle;

	UPROPERTY()
	EBRBotAnchor Anchor = EBRBotAnchor::None;

	/** EBRBotPrecondition bits that must ALL hold for this step to remain valid. */
	UPROPERTY()
	uint32 RequiresMask = 0;

	/** EBRBotPrecondition bits, ANY of which invalidates this step. */
	UPROPERTY()
	uint32 ForbidsMask = 0;

	bool IsValidUnder(const FBRBotFacts& Facts) const
	{
		return Facts.HasAll(RequiresMask) && Facts.HasNone(ForbidsMask);
	}
};

/** The hard ceiling of ruling R10. Not a tuning number — a design ruling, enforced in code. */
inline constexpr int32 BRBotMaxPlanSteps = 3;

/**
 * A plan: an ambition's <= 3 steps and a cursor. Disposable by construction — the brain
 * throws the whole thing away and rebuilds it rather than editing it, because a
 * partially-edited plan is how a contradicted step survives.
 */
USTRUCT()
struct BREACHPOINT_API FBRBotPlan
{
	GENERATED_BODY()

	UPROPERTY()
	EBRBotAmbition Ambition = EBRBotAmbition::None;

	UPROPERTY()
	TArray<FBRBotPlanStep> Steps;

	/** Index into Steps; INDEX_NONE when the plan is spent. */
	UPROPERTY()
	int32 CurrentStep = INDEX_NONE;

	bool IsValid() const { return Steps.Num() > 0 && Steps.IsValidIndex(CurrentStep); }

	const FBRBotPlanStep& GetCurrentStep() const
	{
		static const FBRBotPlanStep IdleStep;
		return Steps.IsValidIndex(CurrentStep) ? Steps[CurrentStep] : IdleStep;
	}

	void Reset()
	{
		Ambition = EBRBotAmbition::None;
		Steps.Reset();
		CurrentStep = INDEX_NONE;
	}
};

/**
 * What one Think() produced. The unit `Breachpoint.Bots.Brain` pins, and the unit the
 * soak logs — a trace is a list of these, and two runs of the same seed must produce
 * identical lists.
 */
USTRUCT()
struct BREACHPOINT_API FBRBotDecision
{
	GENERATED_BODY()

	/** Which event caused this decision. */
	UPROPERTY()
	EBRBotEventType Cause = EBRBotEventType::Spawned;

	UPROPERTY()
	EBRBotAmbition Ambition = EBRBotAmbition::None;

	/** True when this decision replaced the previous ambition (hysteresis was overcome). */
	UPROPERTY()
	bool bAmbitionChanged = false;

	/** True when the plan was rebuilt (ambition change, contradiction, or plan spent). */
	UPROPERTY()
	bool bReplanned = false;

	UPROPERTY()
	FBRBotPlanStep Step;

	/** The winning utility. Logged so a balance question has a number attached to it. */
	UPROPERTY()
	float Utility = 0.f;

	/** Match time this decision was taken, from FBRBotFacts::NowSeconds. */
	UPROPERTY()
	float AtSeconds = 0.f;

	FString ToTraceString() const;
};

// =============================================================================================
// The brain
// =============================================================================================

UCLASS()
class BREACHPOINT_API UBRBotBrain : public UObject
{
	GENERATED_BODY()

public:
	// -- Configuration ------------------------------------------------------------------

	/**
	 * Seed the brain and hand it its data. Call ONCE, at match start (or on possession).
	 *
	 * @param InSeed     the bot's deterministic seed. The manager derives it from the match
	 *                   seed and the bot's slot index, so a match replays identically and
	 *                   two bots in the same match do not think in lockstep.
	 * @param InScalars  this bot's tier row, already read out of DT_BotTuning.
	 * @param InAmbitions the ambition table, already read out of DT_BotAmbitions. Copied,
	 *                   and copied in TABLE ORDER: ties break by table order, so the CSV's
	 *                   row order is part of the deterministic contract.
	 */
	void Initialize(int32 InSeed, const FBRBotTierScalars& InScalars, const TArray<FBRBotAmbitionDef>& InAmbitions);

	bool IsInitialized() const { return bInitialized; }

	// -- The one entry point ------------------------------------------------------------

	/**
	 * Rescore, maybe replan, and return the intent.
	 *
	 * THE ONLY WAY THE BRAIN EVER RUNS. There is no Tick, no timer, and no "update" — an
	 * event arrives or nothing happens (AI-BOTS §3). Pure with respect to the world:
	 * everything it reads is in Facts, everything it changes is its own state.
	 *
	 * @param Event  what happened (and to whom).
	 * @param Facts  the snapshot true at that moment, filled by ABRBotController.
	 * @return the decision. Also retrievable afterwards via GetLastDecision().
	 */
	FBRBotDecision Think(const FBRBotEvent& Event, const FBRBotFacts& Facts);

	/**
	 * The spine reports that it finished the current step. Advances the cursor; when the
	 * plan is spent this is what causes the next plan to be built.
	 * Convenience wrapper around Think(StepCompleted).
	 */
	FBRBotDecision NotifyStepCompleted(const FBRBotFacts& Facts);

	/** The spine could not execute the step (no path, no EQS result, timeout). Forces a replan. */
	FBRBotDecision NotifyStepFailed(const FBRBotFacts& Facts);

	// -- Read-back (the StateTree's window into layer 1) ---------------------------------

	EBRBotAmbition GetActiveAmbition() const { return ActivePlan.Ambition; }
	const FBRBotPlan& GetActivePlan() const { return ActivePlan; }
	const FBRBotPlanStep& GetCurrentStep() const { return ActivePlan.GetCurrentStep(); }
	const FBRBotDecision& GetLastDecision() const { return LastDecision; }
	const FBRBotTierScalars& GetScalars() const { return Scalars; }

	/**
	 * Draw a seeded reaction delay in milliseconds, clamped at R11's floor and quantized.
	 *
	 * Lives on the brain, not the controller, because it consumes the brain's stream: the
	 * ONE stream is what keeps a bot's whole behaviour reproducible from a single seed. The
	 * controller calls this and sets a timer with the result.
	 */
	int32 DrawReactionDelayMs();

	/** Seeded, quantized commit window for the ambition just adopted. Milliseconds. */
	int32 DrawCommitWindowMs();

	/**
	 * The seeded stream, exposed READ-WRITE for the controller's aim error and for the
	 * Roam anchor pick — the two other places that need entropy. One stream per bot,
	 * shared, so that the whole bot is a function of one seed. Anything that reaches for
	 * FMath::Rand instead is a determinism break, and the hook blocks it.
	 */
	FRandomStream& GetStream() { return Stream; }

	// -- Diagnostics --------------------------------------------------------------------

	/** Utility of every ambition at the last Think(), in table order. Empty in shipping-quiet paths. */
	const TArray<float>& GetLastUtilities() const { return LastUtilities; }

	/** Human-readable trace line for the log and for the soak report. */
	FString DescribeLastDecision() const;

	// -- Table adapters (the seam onto Data/, which BP08 does not own) --------------------

	/**
	 * Read `DT_BotAmbitions` into the pure defs this class scores.
	 *
	 * REQUIRED ROW SHAPE — `FBRBotAmbitionRow : public FTableRowBase` in
	 * `Source/Breachpoint/Data/BRDataRows.h`, one member per CSV column, spelled identically:
	 *
	 *     Name                 (row key)  ControlRocket | WinThisFight | Survive |
	 *                                     TakeGround | HuntWeapon | Roam   -- must match the
	 *                                     EBRBotAmbition enumerator spelling exactly
	 *     base_utility         float
	 *     considerations       TArray<FBRBotConsiderationRow>   -- {consideration:FName,
	 *                                     weight:float, invert:bool}; CSV cell is UE's
	 *                                     parenthesised struct-array form
	 *     personality_scalar   FName      rocket_contest | push_threshold |
	 *                                     cover_preference | (empty)
	 *     requires             FString    '|'-separated EBRBotPrecondition names
	 *     forbids              FString    '|'-separated EBRBotPrecondition names
	 *
	 * @return false with OutError set. TODAY IT ALWAYS RETURNS FALSE, LOUDLY: the row struct
	 *         does not exist yet (BP08 contract_gap 1). It is a stub rather than a
	 *         compile-time reference to a missing type on purpose — three other builders are
	 *         in this tree and a red module would be their problem, not mine. The brain
	 *         itself needs none of this: Initialize() takes defs directly, which is exactly
	 *         how `Breachpoint.Bots.Brain` runs with no asset at all.
	 */
	static bool ReadAmbitionDefs(const UDataTable* AmbitionsTable, TArray<FBRBotAmbitionDef>& OutDefs, FString& OutError);

	/**
	 * Read one tier row of `DT_BotTuning` into FBRBotTierScalars.
	 *
	 * REQUIRED ROW SHAPE — `FBRBotTuningRow : public FTableRowBase`, columns named exactly
	 * as the `CSV:` comments on FBRBotTierScalars above (reaction_ms, reaction_quantum_ms,
	 * reaction_jitter_ms, accuracy_pct, aim_error_deg, switch_margin, commit_window_ms,
	 * commit_jitter_ms, rocket_contest, push_threshold, cover_preference, sight_radius_m,
	 * sight_fov_deg, target_memory_s, StateTreeSoftPath), plus ARCHITECTURE §3.11's
	 * `OnPostDataImport` clamp of reaction_ms to 200.
	 *
	 * Same stub, same reason, same contract_gap.
	 */
	static bool ReadTierScalars(const UDataTable* TuningTable, FName TierRowName, FBRBotTierScalars& OutScalars, FString& OutError);

private:
	// -- Scoring ------------------------------------------------------------------------

	/**
	 * Utility of one ambition under these facts. Pure.
	 *
	 * base_utility x PROD(considerations) x personality_scalar, exactly as AI-BOTS §3
	 * specifies, with each consideration contributing `1 - weight * (1 - value)`: weight is
	 * INFLUENCE, so a weight of 0 is the neutral element and a table can disable a
	 * consideration without deleting it. The only literals in this function are 0 and 1 —
	 * the identity and annihilator of the algebra, not tuning.
	 *
	 * Returns 0 for an ambition whose requires/forbids masks are not satisfied, which is how
	 * "Roam is warmup-only" and "ControlRocket needs an open window" are expressed without a
	 * branch per ambition.
	 */
	float ScoreAmbition(const FBRBotAmbitionDef& Def, const FBRBotFacts& Facts) const;

	/**
	 * Pick the winner, applying hysteresis.
	 *
	 * The incumbent keeps its ambition unless a challenger beats it by `switch_margin` AND
	 * the seeded commit window has elapsed. Both halves matter: the margin stops flip-flop
	 * between two near-equal scores, the window stops a burst of events (a firefight is a
	 * burst of events) from re-deciding four times in 200 ms. This is a legibility feature
	 * before it is a perf feature — R12: a bot that visibly commits reads as intentional.
	 */
	EBRBotAmbition SelectAmbition(const FBRBotFacts& Facts, float& OutUtility, bool& bOutChanged);

	// -- Planning -----------------------------------------------------------------------

	/**
	 * Build a plan for an ambition: pick the first authored chain whose preconditions hold,
	 * in declaration order. Bounded lookup, never search (ruling R10 — full A* stays
	 * rejected; if this ever wants to be a search, that is a founder decision, not a patch).
	 */
	FBRBotPlan BuildPlan(EBRBotAmbition Ambition, const FBRBotFacts& Facts);

	/** Does the current step still make sense? */
	bool IsPlanContradicted(const FBRBotFacts& Facts) const;

	// -- State ---------------------------------------------------------------------------

	UPROPERTY()
	TArray<FBRBotAmbitionDef> AmbitionDefs;

	UPROPERTY()
	FBRBotTierScalars Scalars;

	UPROPERTY()
	FBRBotPlan ActivePlan;

	UPROPERTY()
	FBRBotDecision LastDecision;

	UPROPERTY()
	TArray<float> LastUtilities;

	/** Match time (facts clock) at which the current ambition was adopted. */
	float AmbitionAdoptedAtSeconds = 0.f;

	/** The seeded commit window in force for the CURRENT ambition, in seconds. */
	float CurrentCommitWindowSeconds = 0.f;

	/** THE one entropy source. Seeded in Initialize(); never re-seeded mid-match. */
	FRandomStream Stream;

	bool bInitialized = false;
};
