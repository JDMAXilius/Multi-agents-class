// Breachpoint. Layer 1 implementation. Nothing in this file may touch the world.

#include "AI/BRBotBrain.h"

#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogBRBotBrain, Log, All);

// =============================================================================================
// Vocabulary <-> text. The table's row names are matched against these spellings.
// =============================================================================================

const TCHAR* LexToString(EBRBotAmbition Ambition)
{
	switch (Ambition)
	{
	case EBRBotAmbition::ControlRocket: return TEXT("ControlRocket");
	case EBRBotAmbition::WinThisFight:  return TEXT("WinThisFight");
	case EBRBotAmbition::Survive:       return TEXT("Survive");
	case EBRBotAmbition::TakeGround:    return TEXT("TakeGround");
	case EBRBotAmbition::HuntWeapon:    return TEXT("HuntWeapon");
	case EBRBotAmbition::Roam:          return TEXT("Roam");
	default:                            return TEXT("None");
	}
}

EBRBotAmbition BRBotAmbitionFromRowName(FName RowName)
{
	for (uint8 Index = 0; Index < static_cast<uint8>(EBRBotAmbition::Count); ++Index)
	{
		const EBRBotAmbition Candidate = static_cast<EBRBotAmbition>(Index);
		if (RowName == FName(LexToString(Candidate)))
		{
			return Candidate;
		}
	}
	return EBRBotAmbition::None;
}

const TCHAR* LexToString(EBRBotStepVerb Verb)
{
	switch (Verb)
	{
	case EBRBotStepVerb::MoveTo:          return TEXT("MoveTo");
	case EBRBotStepVerb::Hold:            return TEXT("Hold");
	case EBRBotStepVerb::Engage:          return TEXT("Engage");
	case EBRBotStepVerb::Flush:           return TEXT("Flush");
	case EBRBotStepVerb::Reposition:      return TEXT("Reposition");
	case EBRBotStepVerb::Retreat:         return TEXT("Retreat");
	case EBRBotStepVerb::PickupOrContest: return TEXT("PickupOrContest");
	case EBRBotStepVerb::Regroup:         return TEXT("Regroup");
	default:                              return TEXT("Idle");
	}
}

const TCHAR* LexToString(EBRBotAnchor Anchor)
{
	switch (Anchor)
	{
	case EBRBotAnchor::Target:      return TEXT("Target");
	case EBRBotAnchor::Cover:       return TEXT("Cover");
	case EBRBotAnchor::RocketPad:   return TEXT("RocketPad");
	case EBRBotAnchor::FiringPoint: return TEXT("FiringPoint");
	case EBRBotAnchor::Perch:       return TEXT("Perch");
	case EBRBotAnchor::Teammate:    return TEXT("Teammate");
	case EBRBotAnchor::Landmark:    return TEXT("Landmark");
	default:                        return TEXT("None");
	}
}

FString FBRBotDecision::ToTraceString() const
{
	// One line per decision, fixed field order and fixed float precision: a trace is diffed
	// between two runs of the same seed, so its FORMAT is part of the determinism contract.
	return FString::Printf(
		TEXT("t=%.3f cause=%d ambition=%s%s step=%s@%s util=%.4f%s"),
		AtSeconds,
		static_cast<int32>(Cause),
		LexToString(Ambition),
		bAmbitionChanged ? TEXT("*") : TEXT(""),
		LexToString(Step.Verb),
		LexToString(Step.Anchor),
		Utility,
		bReplanned ? TEXT(" [replan]") : TEXT(""));
}

// =============================================================================================
// FBRBotTierScalars
// =============================================================================================

bool FBRBotTierScalars::ResolvePersonalityScalar(FName ScalarName, float& OutValue) const
{
	// The three personality columns AI-BOTS §3 names. A row asking for anything else is a
	// table error the caller reports by name — never a silent 1.0, which would make an
	// untuned tier play like a tuned one.
	static const FName NameRocketContest(TEXT("rocket_contest"));
	static const FName NamePushThreshold(TEXT("push_threshold"));
	static const FName NameCoverPreference(TEXT("cover_preference"));

	if (ScalarName == NameRocketContest)   { OutValue = RocketContest;   return true; }
	if (ScalarName == NamePushThreshold)   { OutValue = PushThreshold;   return true; }
	if (ScalarName == NameCoverPreference) { OutValue = CoverPreference; return true; }
	return false;
}

bool FBRBotTierScalars::ValidateSchema(FString& OutError) const
{
	// Structural only. Nothing here pins a balance value: balance is a CSV diff and must
	// never have to fight a C++ assert (the FBRWeaponRow precedent).
	if (ReactionMs < ReactionFloorMs)
	{
		// NOT an error — GetReactionMsClamped() makes it harmless. It is worth saying out
		// loud, because a row carrying 50 means someone believed it would take effect.
		UE_LOG(LogBRBotBrain, Warning,
			TEXT("Tier '%s' declares reaction_ms=%d; ruling R11 clamps it to %d."),
			*TierName.ToString(), ReactionMs, ReactionFloorMs);
	}
	if (ReactionQuantumMs < 0 || ReactionJitterMs < 0 || CommitWindowMs < 0 || CommitJitterMs < 0)
	{
		OutError = TEXT("A millisecond column is negative");
		return false;
	}
	if (AccuracyPct < 0.f || AccuracyPct > 1.f)
	{
		OutError = TEXT("accuracy_pct must be a fraction in [0,1]");
		return false;
	}
	if (AimErrorDeg < 0.f)
	{
		OutError = TEXT("aim_error_deg cannot be negative");
		return false;
	}
	if (SwitchMargin < 0.f)
	{
		OutError = TEXT("switch_margin cannot be negative (a negative margin is ambition thrash)");
		return false;
	}
	if (SightRadius_m < 0.f || SightFov_deg < 0.f || SightFov_deg > 360.f || TargetMemory_s < 0.f)
	{
		OutError = TEXT("A perception column is out of range");
		return false;
	}
	return true;
}

// =============================================================================================
// The authored plan chains — ruling R10's "bounded lookup, not free search"
// =============================================================================================

namespace
{
	using EPre = EBRBotPrecondition;

	inline constexpr uint32 Bits() { return BRBotNoPreconditions; }
	template <typename... TRest>
	inline constexpr uint32 Bits(EPre First, TRest... Rest)
	{
		return BRBotBit(First) | Bits(Rest...);
	}

	/**
	 * One authored chain: an ambition, the facts under which this variant is the right
	 * shape, and up to three steps.
	 *
	 * WHY THIS IS CODE AND NOT DATA: a chain contains no numbers. It is verbs and places —
	 * the STRUCTURE of a behaviour, which is exactly what review and the legibility check
	 * (R12) argue about. The numbers that decide WHICH ambition wins, and how hard a tier
	 * pursues it, are all in DT_BotAmbitions/DT_BotTuning, and none of them appear here. If
	 * chains ever need to be authorable without a compile, they move to a table wholesale —
	 * that is a design change, not a place to start sprinkling literals.
	 *
	 * ORDER IS THE CONTRACT: the first chain whose gate passes wins. Reordering this array
	 * changes bot behaviour and must be reviewed as a behaviour change.
	 */
	struct FBRBotPlanChain
	{
		EBRBotAmbition Ambition;
		uint32 RequiresMask;
		uint32 ForbidsMask;
		FBRBotPlanStep Steps[BRBotMaxPlanSteps];
		int32 NumSteps;
	};

	FBRBotPlanStep MakeStep(EBRBotStepVerb Verb, EBRBotAnchor Anchor, uint32 Requires = 0, uint32 Forbids = 0)
	{
		FBRBotPlanStep Step;
		Step.Verb = Verb;
		Step.Anchor = Anchor;
		Step.RequiresMask = Requires;
		Step.ForbidsMask = Forbids;
		return Step;
	}

	const TArray<FBRBotPlanChain>& PlanLibrary()
	{
		static const TArray<FBRBotPlanChain> Library = []()
		{
			TArray<FBRBotPlanChain> Chains;

			// --- ControlRocket -------------------------------------------------------
			// Already holding it: the want becomes "use it from somewhere good".
			Chains.Add({ EBRBotAmbition::ControlRocket,
				Bits(EPre::HoldingPowerWeapon), Bits(EPre::SelfDead),
				{ MakeStep(EBRBotStepVerb::Reposition, EBRBotAnchor::Perch),
				  MakeStep(EBRBotStepVerb::Engage, EBRBotAnchor::Target, Bits(EPre::TargetKnown)),
				  {} }, 2 });

			// AI-BOTS §3's worked example, verbatim: approach with cover, hold the
			// sightline, take or deny. This is the T-10 s beat a playtester must be able
			// to NAME from behaviour alone (R12 / BP08 done-when box 5).
			Chains.Add({ EBRBotAmbition::ControlRocket,
				Bits(EPre::PowerWeaponAvailable), Bits(EPre::SelfDead, EPre::MyShieldsBroken),
				{ MakeStep(EBRBotStepVerb::MoveTo, EBRBotAnchor::RocketPad, 0, Bits(EPre::MyShieldsBroken)),
				  MakeStep(EBRBotStepVerb::Hold, EBRBotAnchor::RocketPad),
				  MakeStep(EBRBotStepVerb::PickupOrContest, EBRBotAnchor::RocketPad) }, 3 });

			// Shields cracked on the way in: contest from cover instead of walking onto the pad.
			Chains.Add({ EBRBotAmbition::ControlRocket,
				Bits(EPre::PowerWeaponAvailable, EPre::MyShieldsBroken), Bits(EPre::SelfDead),
				{ MakeStep(EBRBotStepVerb::MoveTo, EBRBotAnchor::Cover),
				  MakeStep(EBRBotStepVerb::Hold, EBRBotAnchor::RocketPad) }, 2 });

			// --- WinThisFight --------------------------------------------------------
			// Target is behind cover and the grenade is up: flush, then push. The Flush
			// state exists for exactly this and nothing else.
			Chains.Add({ EBRBotAmbition::WinThisFight,
				Bits(EPre::TargetKnown, EPre::GrenadeReady), Bits(EPre::SelfDead, EPre::TargetVisible),
				{ MakeStep(EBRBotStepVerb::Flush, EBRBotAnchor::Target, Bits(EPre::GrenadeReady)),
				  MakeStep(EBRBotStepVerb::Engage, EBRBotAnchor::Target, Bits(EPre::TargetKnown)),
				  {} }, 2 });

			Chains.Add({ EBRBotAmbition::WinThisFight,
				Bits(EPre::TargetVisible), Bits(EPre::SelfDead),
				{ MakeStep(EBRBotStepVerb::Engage, EBRBotAnchor::Target, Bits(EPre::TargetKnown)),
				  {}, {} }, 1 });

			// Heard/knew, cannot see: take a firing point onto the last known place first.
			Chains.Add({ EBRBotAmbition::WinThisFight,
				Bits(EPre::TargetKnown), Bits(EPre::SelfDead),
				{ MakeStep(EBRBotStepVerb::MoveTo, EBRBotAnchor::FiringPoint),
				  MakeStep(EBRBotStepVerb::Engage, EBRBotAnchor::Target, Bits(EPre::TargetKnown)),
				  {} }, 2 });

			// --- Survive -------------------------------------------------------------
			// THE Halo break-off (R12): shields crack -> visible disengage toward cover.
			// The Retreat step FORBIDS nothing and REQUIRES nothing, so it cannot be
			// contradicted into a livelock; it ends when the spine reports it done.
			Chains.Add({ EBRBotAmbition::Survive,
				Bits(EPre::TeammateNearby), Bits(EPre::SelfDead),
				{ MakeStep(EBRBotStepVerb::Retreat, EBRBotAnchor::Cover),
				  MakeStep(EBRBotStepVerb::Regroup, EBRBotAnchor::Teammate),
				  {} }, 2 });

			Chains.Add({ EBRBotAmbition::Survive,
				0, Bits(EPre::SelfDead),
				{ MakeStep(EBRBotStepVerb::Retreat, EBRBotAnchor::Cover),
				  MakeStep(EBRBotStepVerb::Hold, EBRBotAnchor::Cover),
				  {} }, 2 });

			// --- TakeGround ----------------------------------------------------------
			Chains.Add({ EBRBotAmbition::TakeGround,
				0, Bits(EPre::SelfDead),
				{ MakeStep(EBRBotStepVerb::MoveTo, EBRBotAnchor::Landmark),
				  MakeStep(EBRBotStepVerb::Hold, EBRBotAnchor::Landmark),
				  {} }, 2 });

			// --- HuntWeapon ----------------------------------------------------------
			Chains.Add({ EBRBotAmbition::HuntWeapon,
				0, Bits(EPre::SelfDead, EPre::HoldingPowerWeapon),
				{ MakeStep(EBRBotStepVerb::MoveTo, EBRBotAnchor::RocketPad),
				  MakeStep(EBRBotStepVerb::PickupOrContest, EBRBotAnchor::RocketPad),
				  {} }, 2 });

			// --- Roam (warmup only; the ambition row's requires mask enforces the phase) --
			Chains.Add({ EBRBotAmbition::Roam,
				0, Bits(EPre::SelfDead),
				{ MakeStep(EBRBotStepVerb::MoveTo, EBRBotAnchor::Landmark), {}, {} }, 1 });

			return Chains;
		}();

		return Library;
	}
} // namespace

// =============================================================================================
// UBRBotBrain
// =============================================================================================

void UBRBotBrain::Initialize(int32 InSeed, const FBRBotTierScalars& InScalars, const TArray<FBRBotAmbitionDef>& InAmbitions)
{
	Scalars = InScalars;

	// Copied in table order and kept in an ARRAY: ties break by table order, and a TMap's
	// iteration order depends on hashing, so it could resolve a tie differently between two
	// runs of the same seed. That is the whole reason this is not a map.
	AmbitionDefs = InAmbitions;

	ActivePlan.Reset();
	LastDecision = FBRBotDecision();
	LastUtilities.Reset();
	AmbitionAdoptedAtSeconds = 0.f;
	CurrentCommitWindowSeconds = 0.f;

	// THE seed. One stream per bot, initialised once, never re-seeded — re-seeding
	// mid-match would make a trace depend on WHEN it was re-seeded, i.e. on the frame rate.
	Stream.Initialize(InSeed);

	FString ScalarError;
	if (!Scalars.ValidateSchema(ScalarError))
	{
		UE_LOG(LogBRBotBrain, Error, TEXT("Tier '%s' failed schema validation: %s"),
			*Scalars.TierName.ToString(), *ScalarError);
	}

	bInitialized = true;
}

float UBRBotBrain::ScoreAmbition(const FBRBotAmbitionDef& Def, const FBRBotFacts& Facts) const
{
	// Gate first: an ambition whose world-conditions are absent scores nothing at all. This
	// is where "Roam is warmup-only" and "ControlRocket needs an open window" live, as data.
	if (!Facts.HasAll(Def.RequiresMask) || !Facts.HasNone(Def.ForbidsMask))
	{
		return 0.f;
	}

	float Utility = Def.BaseUtility;

	// Multiplicative combine, in TABLE ORDER. Float multiplication is not associative, so
	// the order is part of the deterministic contract, not an implementation detail.
	for (const FBRBotConsiderationDef& Consideration : Def.Considerations)
	{
		float Value = 0.f;
		if (!Facts.ResolveScalar(Consideration.Consideration, Value))
		{
			// Named a fact that does not exist. Loud, and scores the ambition to zero: a
			// silently-ignored consideration is a balance bug that survives every review.
			UE_LOG(LogBRBotBrain, Error,
				TEXT("Ambition row '%s' names unknown consideration '%s' — scoring it 0."),
				*Def.RowName.ToString(), *Consideration.Consideration.ToString());
			return 0.f;
		}

		if (Consideration.bInvert)
		{
			Value = 1.f - Value;
		}

		// Weight is INFLUENCE: 0 => this consideration cannot move the score (neutral
		// element), 1 => a zero fact annihilates the ambition. 0 and 1 here are the algebra's
		// identity and annihilator, not tuning values.
		const float Weight = FMath::Clamp(Consideration.Weight, 0.f, 1.f);
		Utility *= (1.f - Weight * (1.f - FMath::Clamp(Value, 0.f, 1.f)));
	}

	// Personality: one named DT_BotTuning column shapes this ambition for this tier. This is
	// the entire mechanism behind "three difficulty tiers from ONE brain via data scalars".
	if (!Def.PersonalityScalar.IsNone())
	{
		float ScalarValue = 0.f;
		if (Scalars.ResolvePersonalityScalar(Def.PersonalityScalar, ScalarValue))
		{
			Utility *= ScalarValue;
		}
		else
		{
			UE_LOG(LogBRBotBrain, Error,
				TEXT("Ambition row '%s' names unknown personality scalar '%s' — scoring it 0."),
				*Def.RowName.ToString(), *Def.PersonalityScalar.ToString());
			return 0.f;
		}
	}

	return Utility;
}

EBRBotAmbition UBRBotBrain::SelectAmbition(const FBRBotFacts& Facts, float& OutUtility, bool& bOutChanged)
{
	const EBRBotAmbition Incumbent = ActivePlan.Ambition;

	LastUtilities.SetNumZeroed(AmbitionDefs.Num());

	int32 BestIndex = INDEX_NONE;
	float BestUtility = 0.f;
	float IncumbentUtility = 0.f;

	for (int32 Index = 0; Index < AmbitionDefs.Num(); ++Index)
	{
		const FBRBotAmbitionDef& Def = AmbitionDefs[Index];
		const float Utility = ScoreAmbition(Def, Facts);
		LastUtilities[Index] = Utility;

		if (Def.Ambition == Incumbent)
		{
			IncumbentUtility = Utility;
		}

		// STRICTLY greater: an exact tie keeps the earlier table row. Ties are common with
		// clamped normalized facts, so "who wins a tie" has to be defined or a trace is not
		// reproducible.
		if (Utility > BestUtility)
		{
			BestUtility = Utility;
			BestIndex = Index;
		}
	}

	const EBRBotAmbition Challenger = (BestIndex != INDEX_NONE) ? AmbitionDefs[BestIndex].Ambition : EBRBotAmbition::None;

	// No incumbent (spawn, or the last ambition is now impossible): adopt immediately.
	if (Incumbent == EBRBotAmbition::None || IncumbentUtility <= 0.f)
	{
		bOutChanged = (Challenger != Incumbent);
		OutUtility = BestUtility;
		return Challenger;
	}

	if (Challenger == Incumbent)
	{
		bOutChanged = false;
		OutUtility = IncumbentUtility;
		return Incumbent;
	}

	// HYSTERESIS, both halves. The commit window is measured against the facts clock the
	// caller supplied — never a platform clock.
	const float HeldForSeconds = Facts.NowSeconds - AmbitionAdoptedAtSeconds;
	const bool bWindowElapsed = (HeldForSeconds >= CurrentCommitWindowSeconds);
	const bool bBeatsMargin = (BestUtility > IncumbentUtility * (1.f + FMath::Max(Scalars.SwitchMargin, 0.f)));

	if (bWindowElapsed && bBeatsMargin)
	{
		bOutChanged = true;
		OutUtility = BestUtility;
		return Challenger;
	}

	bOutChanged = false;
	OutUtility = IncumbentUtility;
	return Incumbent;
}

FBRBotPlan UBRBotBrain::BuildPlan(EBRBotAmbition Ambition, const FBRBotFacts& Facts)
{
	FBRBotPlan Plan;
	Plan.Ambition = Ambition;

	if (Ambition == EBRBotAmbition::None)
	{
		return Plan;
	}

	// Bounded lookup: first chain, in declaration order, whose gate passes. No search, no
	// cost function, no open list (R10).
	for (const FBRBotPlanChain& Chain : PlanLibrary())
	{
		if (Chain.Ambition != Ambition)
		{
			continue;
		}
		if (!Facts.HasAll(Chain.RequiresMask) || !Facts.HasNone(Chain.ForbidsMask))
		{
			continue;
		}

		const int32 NumSteps = FMath::Clamp(Chain.NumSteps, 0, BRBotMaxPlanSteps);
		for (int32 Index = 0; Index < NumSteps; ++Index)
		{
			Plan.Steps.Add(Chain.Steps[Index]);
		}
		Plan.CurrentStep = (Plan.Steps.Num() > 0) ? 0 : INDEX_NONE;
		return Plan;
	}

	// An ambition that scored above zero but has no legal chain is a data/design mismatch,
	// not a runtime condition to absorb quietly.
	UE_LOG(LogBRBotBrain, Warning, TEXT("Ambition '%s' won but no authored chain accepted the facts."),
		LexToString(Ambition));
	return Plan;
}

bool UBRBotBrain::IsPlanContradicted(const FBRBotFacts& Facts) const
{
	if (!ActivePlan.IsValid())
	{
		return true;
	}
	return !ActivePlan.GetCurrentStep().IsValidUnder(Facts);
}

FBRBotDecision UBRBotBrain::Think(const FBRBotEvent& Event, const FBRBotFacts& Facts)
{
	FBRBotDecision Decision;
	Decision.Cause = Event.Type;
	Decision.AtSeconds = Facts.NowSeconds;

	if (!bInitialized)
	{
		UE_LOG(LogBRBotBrain, Error, TEXT("Think() before Initialize() — no seed, no tuning. Refusing to decide."));
		LastDecision = Decision;
		return Decision;
	}

	// Dead bots want nothing. Stated once here rather than as a forbids bit on every row.
	if (Facts.Has(EBRBotPrecondition::SelfDead))
	{
		ActivePlan.Reset();
		LastDecision = Decision;
		return Decision;
	}

	// 1. WHAT do I want?
	float Utility = 0.f;
	bool bChanged = false;
	const EBRBotAmbition Ambition = SelectAmbition(Facts, Utility, bChanged);

	// 2. Does the current plan still stand? Three ways it does not: the ambition changed,
	//    the current step's preconditions died, or the spine reported the step unexecutable.
	const bool bStepFailed = (Event.Type == EBRBotEventType::StepFailed);
	const bool bNeedsPlan = bChanged || bStepFailed || IsPlanContradicted(Facts);

	if (bNeedsPlan)
	{
		ActivePlan = BuildPlan(Ambition, Facts);
		Decision.bReplanned = true;

		if (bChanged || AmbitionAdoptedAtSeconds == 0.f)
		{
			// A new ambition starts a new, seeded commit window. Drawing it HERE (once per
			// adoption) and not per event is what keeps the stream draw count a function of
			// the decision trace rather than of the event rate.
			AmbitionAdoptedAtSeconds = Facts.NowSeconds;
			CurrentCommitWindowSeconds = static_cast<float>(DrawCommitWindowMs()) / 1000.f;
		}
	}
	else if (Event.Type == EBRBotEventType::StepCompleted)
	{
		// Advance the cursor. A spent plan replans immediately rather than idling — an idle
		// bot with an unfinished ambition is the livelock the critic looks for.
		++ActivePlan.CurrentStep;
		if (!ActivePlan.Steps.IsValidIndex(ActivePlan.CurrentStep))
		{
			ActivePlan = BuildPlan(Ambition, Facts);
			Decision.bReplanned = true;
		}
	}

	Decision.Ambition = ActivePlan.Ambition;
	Decision.bAmbitionChanged = bChanged;
	Decision.Step = ActivePlan.GetCurrentStep();
	Decision.Utility = Utility;

	LastDecision = Decision;
	return Decision;
}

FBRBotDecision UBRBotBrain::NotifyStepCompleted(const FBRBotFacts& Facts)
{
	FBRBotEvent Event;
	Event.Type = EBRBotEventType::StepCompleted;
	return Think(Event, Facts);
}

FBRBotDecision UBRBotBrain::NotifyStepFailed(const FBRBotFacts& Facts)
{
	FBRBotEvent Event;
	Event.Type = EBRBotEventType::StepFailed;
	return Think(Event, Facts);
}

int32 UBRBotBrain::DrawReactionDelayMs()
{
	// The ONLY randomness: FRandomStream, seeded in Initialize(). FMath::RandRange and
	// FMath::FRand are banned project-wide and blocked by the pre-tool hook.
	const int32 Jitter = (Scalars.ReactionJitterMs > 0) ? Stream.RandRange(0, Scalars.ReactionJitterMs) : 0;

	// Clamped at R11's 200 ms floor inside the accessor — there is no other way to read it.
	return Scalars.GetReactionMsClamped(Jitter);
}

int32 UBRBotBrain::DrawCommitWindowMs()
{
	const int32 Jitter = (Scalars.CommitJitterMs > 0) ? Stream.RandRange(0, Scalars.CommitJitterMs) : 0;

	// Quantized on the same grid as reactions: two bots that adopt an ambition in the same
	// event burst must not be able to drift apart by sub-quantum amounts.
	return FBRBotTierScalars::QuantizeMs(FMath::Max(Scalars.CommitWindowMs, 0) + Jitter, Scalars.ReactionQuantumMs);
}

FString UBRBotBrain::DescribeLastDecision() const
{
	return LastDecision.ToTraceString();
}

// =============================================================================================
// Table adapters — the seam onto Data/, which BP08 does not own. See BP08 contract_gap 1.
// =============================================================================================

bool UBRBotBrain::ReadAmbitionDefs(const UDataTable* AmbitionsTable, TArray<FBRBotAmbitionDef>& OutDefs, FString& OutError)
{
	OutDefs.Reset();

	if (AmbitionsTable == nullptr)
	{
		OutError = TEXT("DT_BotAmbitions is null");
		return false;
	}

	// CONTRACT GAP, stated in code so it cannot be forgotten: `FBRBotAmbitionRow` does not
	// exist in Source/Breachpoint/Data/BRDataRows.h yet, and BP08 does not own that header.
	// The required shape is documented on the declaration of this function. Once the row
	// lands, this body becomes a GetAllRows<FBRBotAmbitionRow>() loop that fills
	// FBRBotAmbitionDef and resolves RowName -> EBRBotAmbition via BRBotAmbitionFromRowName.
	//
	// Returning false loudly, rather than referencing a type that does not compile: three
	// other builders share this module and a red build would be their problem, not mine.
	OutError = TEXT("FBRBotAmbitionRow is not declared in BRDataRows.h yet (BP08 contract_gap 1)");
	UE_LOG(LogBRBotBrain, Error, TEXT("%s"), *OutError);
	return false;
}

bool UBRBotBrain::ReadTierScalars(const UDataTable* TuningTable, FName TierRowName, FBRBotTierScalars& OutScalars, FString& OutError)
{
	OutScalars = FBRBotTierScalars();
	OutScalars.TierName = TierRowName;

	if (TuningTable == nullptr)
	{
		OutError = TEXT("DT_BotTuning is null");
		return false;
	}

	// Same gap, same reason: `FBRBotTuningRow` is named by ARCHITECTURE §3.11 but is not in
	// BRDataRows.h today. Until it is, every bot is UNCONFIGURED — and the sentinel defaults
	// on FBRBotTierScalars make that visibly inert rather than accidentally playable.
	OutError = TEXT("FBRBotTuningRow is not declared in BRDataRows.h yet (BP08 contract_gap 1)");
	UE_LOG(LogBRBotBrain, Error, TEXT("%s"), *OutError);
	return false;
}
