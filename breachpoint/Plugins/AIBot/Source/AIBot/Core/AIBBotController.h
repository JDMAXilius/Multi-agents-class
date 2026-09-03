#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Brain/AIBConfidenceModel.h"
#include "Core/AIBRouteBias.h"
#include "Core/AIBTypes.h"
#include "GameplayTagContainer.h"
#include "Interfaces/AIBAmbitionProvider.h"
#include "Perception/AIBSensorium.h"
#include "Skills/AIBAimPolicy.h"
#include "Skills/AIBGrenadePolicy.h"
#include "Skills/AIBMeleePolicy.h"
#include "Skills/AIBMovementPolicy.h"
#include "Skills/AIBSkillProfile.h"
#include "AIBBotController.generated.h"

class IAIBAvatarInterface;
class IAIBExecutor;
class IAIBWorldQuery;
class UAIBAmbitionEngine;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UStateTree;
class UStateTreeAIComponent;
struct FAIStimulus;

/** LAW F9's named stillness (AIB22). A task that legitimately plants the body SETS its
 *  flag while it holds and CLEARS it when it stops; the idle instrument reports whether
 *  ANY was up during a still spell. Flags, not a single value: three independent
 *  holders share one fight (Hold + Reload + a planted strafe leg), and one enum slot
 *  would let the first to clear erase the others. */
enum class EAIBStillTactic : uint8
{
	None       = 0,
	Hold       = 1 << 0,
	Reload     = 1 << 1,
	StrafeHold = 1 << 2,
	/** AIB22 M5: the budgeted stationary sweep at a search post. Up only while the
	 *  budget is being SPENT, so the idle gate excludes exactly the seconds the
	 *  `sweep over` line reports. */
	Sweep      = 1 << 3,
	/** AIB22 W-REVIEW #3 H2: a CONFIRMED island with no policy-legal lip — a MAP defect
	 *  the verifier must see, not a silent spin. Up for the Egress cooldown, during
	 *  which Wander draws nothing; Think mirrors it from the latch each sample, so it
	 *  drops when the cooldown lapses or any full-path move completes. */
	Stranded   = 1 << 4,
	/** Phase 13 (AIB24): a wedged mover with a TEAMMATE inside TeammateYieldRadiusUU waits
	 *  one bounded window for the crowd's separation to part them — a body, not geometry,
	 *  so no jump and no re-issued move. Up for TeammateYieldSeconds at most. */
	Yield      = 1 << 5,
	/** Phase 13 W-REVIEW M6: a live move request the crowd has braked to zero velocity —
	 *  the crowd is doing the stepping, not the bot standing. Mirrored each Think. */
	Crowd      = 1 << 6,
};

/** AIB22 H1/F9 — the STATIONARY sweep's budget. Held by the controller, never by the
 *  task: a StateTree recreates task instance data on every completion transition, so a
 *  budget in SweepLook's scratch refilled each time Search re-entered (the grenade-
 *  cooldown lesson, again). It bounds a still SPELL — Think clears it on the first sample
 *  the body moves — so arriving somewhere new earns a new look, re-entering does not.
 *  Worldless on purpose: the spec drives it with plain seconds. */
struct AIBOT_API FAIBSweepBudget
{
	float SpentSeconds = 0.f;
	/** AIB22 fix #4 R4: the refill is keyed on the BOT, never on the post. Where the body
	 *  stood at the last refill (valid while bHasRefill); ArriveAt with the body 1.5x
	 *  RadiusUU or farther from it refills (the mover's own at-post band — W-REVIEW #3
	 *  M4), closer does not — a post that moves under a standing bot, a nudge at the same
	 *  post, or a re-entry refill nothing; walking somewhere new and stopping does. */
	FVector LastRefillLocation = FVector::ZeroVector;
	bool bHasRefill = false;

	bool HasBudget(float MaxSeconds) const { return SpentSeconds < MaxSeconds; }
	void Spend(float DeltaTime) { SpentSeconds += FMath::Max(DeltaTime, 0.f); }
	/** Possession: a fresh body has looked at nothing yet and stood nowhere. */
	void Reset() { SpentSeconds = 0.f; bHasRefill = false; }
	/** The displacement refill. True when the body stands somewhere new (and refilled). */
	bool ArriveAt(const FVector& BotLocation, float RadiusUU)
	{
		if (bHasRefill && FVector::DistSquared(BotLocation, LastRefillLocation) <= FMath::Square(RadiusUU * 1.5f))
		{
			return false;
		}
		LastRefillLocation = BotLocation;
		bHasRefill = true;
		SpentSeconds = 0.f;
		return true;
	}
};

/** Every mover's locomotion scratch: the sprint HOLD's edge state and the stall watchdog
 *  (measures only — traversal presses live on UAIBPathFollowingComponent since AIB22;
 *  fix #4 R9: the watchdog never hops). ON THE CONTROLLER since fix #4 R3: in a task's
 *  instance data it was recreated on every branch re-entry, so a body wedged against a
 *  storey had its give-up window reset by every acquire/SWITCHED flap through Engage —
 *  measured 21.9s and 46.5s stalls. One body, one stall clock; only real progress
 *  (WedgeProgressUU of ground gained) resets it. Never authored, never serialized. */
struct AIBOT_API FAIBLocomotionState
{
	bool bSprintHeld = false;
	/** The one diagnosis line per stall has printed (was bTriedWedgeJump — nothing jumps). */
	bool bDiagnosed = false;
	bool bHasBestPoint = false;
	float StallSeconds = 0.f;
	FVector BestPoint = FVector::ZeroVector;
	/** World seconds when the stall clock last restarted — `jumped=` on the stall line
	 *  reads whether the path follower pressed a link jump since then. */
	double StallStartedAtSeconds = 0.0;
	/** World seconds when the mover's GOAL last changed. The storey read (R3) needs the
	 *  stall to be WedgeStallSeconds old against THIS goal: a fresh goal up a ramp, handed
	 *  to a body still standing from its last stop, is not a stall against a storey. The
	 *  give-up window (StallSeconds) does not reset with it. */
	double GoalSetAtSeconds = 0.0;

	/** AIB22 `stuck_seconds` bookkeeping — reads the watchdog's clocks, never moves them.
	 *  An episode is OPEN once StallSeconds has run StallReportSeconds past what the last
	 *  line reported; Goal is the mover's current target, for the line. */
	bool bStallOpen = false;
	float StallReportedSeconds = 0.f;
	FVector Goal = FVector::ZeroVector;

	/** Phase 13: world seconds until which this wedge YIELDS to a teammate (0 = not
	 *  yielding). While it runs sprint is released and the abandon VERDICT waits; the
	 *  stall clock keeps running (W-REVIEW H2) and the crowd's separation does the
	 *  stepping. ONE yield per wedge: bYielded latches until real progress clears it. */
	double YieldUntilSeconds = 0.0;
	bool bYielded = false;

	bool IsYielding(double NowSeconds) const { return YieldUntilSeconds > 0.0 && NowSeconds < YieldUntilSeconds; }
	/** Arms the one window this wedge gets (H1: the caller arms only at a REAL wedge —
	 *  the stall clock at WedgeStallSeconds). False when this wedge already yielded. */
	bool TryArmYield(double NowSeconds, float WindowSeconds)
	{
		if (bYielded)
		{
			return false;
		}
		bYielded = true;
		YieldUntilSeconds = NowSeconds + FMath::Max(WindowSeconds, 0.f);
		return true;
	}
	/** Real progress (WedgeProgressUU gained): the wedge is over, the next one may yield. */
	void NoteProgress() { bYielded = false; YieldUntilSeconds = 0.0; }

	/** AIB22 F5-1(a): the goal the watchdog last ABANDONED, refused for the ambition's
	 *  suppression window. The verdict used to leave the clock running, so a branch that
	 *  re-issued the same goal next frame got the verdict again — 107k `stall abandoned`
	 *  lines from ONE clock. Now the verdict consumes the clock (a different goal runs a
	 *  fresh one) and the same goal is refused up front, silently. Worldless, spec-driven. */
	bool bHasAbandonedGoal = false;
	FVector AbandonedGoal = FVector::ZeroVector;
	double AbandonRefuseUntilSeconds = -1.0;

	void NoteAbandoned(const FVector& InGoal, double NowSeconds, float WindowSeconds)
	{
		bHasAbandonedGoal = true;
		AbandonedGoal = InGoal;
		AbandonRefuseUntilSeconds = NowSeconds + FMath::Max(WindowSeconds, 0.f);
	}
	/** True while InGoal is the abandoned goal (within SameGoalUU) and the window is live. */
	bool RefusesGoal(const FVector& InGoal, double NowSeconds, float SameGoalUU) const
	{
		return bHasAbandonedGoal && NowSeconds < AbandonRefuseUntilSeconds
			&& AbandonedGoal.Equals(InGoal, SameGoalUU);
	}
};

/** AIB22 fix #4 R1: one confirmation anchor — a point that is connected ground IF the bot
 *  is not on an island. The gate tests the feet against the whole list. */
struct FAIBIslandAnchor
{
	FVector Location = FVector::ZeroVector;
	const TCHAR* Name = TEXT("?");
};

/** AIB22 5(B) — THE ISLAND FACT, as a latch. Roam's wander draws a NAVIGABLE point and
 *  prefers a FULL path; a partial path is what an island looks like from the inside
 *  (islands do not refuse, they deliver you to the edge — the audit's corrected premise).
 *  IslandLatchDraws consecutive bad draws latch bOnIsland. The latch is a HYPOTHESIS
 *  (W-REVIEW H2): the Egress gate confirms it against the ANCHOR LIST ONCE per latch
 *  (Confirm — island iff no anchor has a full path, fix #4 R1) and caches the answer
 *  here (W-REVIEW #3 M6). Cleared by: one full draw, any COMPLETED
 *  full-path move (M3), a landing, age past LatchMaxAgeSeconds, or a refuted hypothesis.
 *  An Egress failure clears AND arms a cooldown during which draws do not latch (H1); a
 *  lipless failure on a CONFIRMED island also STRANDS (#3 H2) — no draws at all until the
 *  cooldown lapses or a full-path move completes. Held by the controller for the same
 *  reason as the sweep budget: a StateTree recreates task instance data on every
 *  completion transition. Worldless: the spec drives it with plain seconds and bools. */
struct AIBOT_API FAIBIslandLatch
{
	/** The gate's confirmation, cached per latch (Untested again on every clear/re-latch). */
	enum class EConfirm : uint8 { Untested, Island, Refuted };

	int32 BadDraws = 0;
	bool bOnIsland = false;
	EConfirm Confirmation = EConfirm::Untested;
	/** See Strand / IsStranded. */
	bool bStranded = false;
	/** World seconds at the latch (-1 = not latched): the egress line's stranded clock. */
	double LatchedAtSeconds = -1.0;
	/** World seconds before which draws do not latch (-1 = no cooldown). */
	double NoLatchBeforeSeconds = -1.0;

	/** True on the ONE draw that latches — the caller logs it once. */
	bool NoteDraw(bool bFullPath, int32 LatchAfterDraws, double NowSeconds)
	{
		if (bFullPath)
		{
			Clear();
			return false;
		}
		if (NowSeconds < NoLatchBeforeSeconds)
		{
			return false; // the Egress cooldown: walk, do not measure
		}
		++BadDraws;
		if (!bOnIsland && BadDraws >= FMath::Max(LatchAfterDraws, 1))
		{
			bOnIsland = true;
			LatchedAtSeconds = NowSeconds;
			Confirmation = EConfirm::Untested;
			return true;
		}
		return false;
	}
	/** THE ANCHOR-LIST DECISION (fix #4 R1), pure so the spec can drive it: one bool per
	 *  anchor, "the feet have a FULL path to it". Island iff NONE does — a spawn pad that
	 *  is itself an island cannot refute through itself while the objective anchor beside
	 *  it still refutes a floor bot. Any full path REFUTES: cleared with the cooldown, the
	 *  verdict cached. An EMPTY list confirms nothing (Untested: the gate acts on the
	 *  latch alone and Egress cannot strand). Returns the refuting anchor's index, or
	 *  INDEX_NONE. */
	int32 Confirm(TConstArrayView<bool> FullPathToAnchor, double NowSeconds, float CooldownSeconds)
	{
		if (FullPathToAnchor.Num() == 0)
		{
			Confirmation = EConfirm::Untested;
			return INDEX_NONE;
		}
		const int32 Refuter = FullPathToAnchor.IndexOfByKey(true);
		if (Refuter != INDEX_NONE)
		{
			ClearWithCooldown(NowSeconds, CooldownSeconds);
			Confirmation = EConfirm::Refuted;
			return Refuter;
		}
		Confirmation = EConfirm::Island;
		return INDEX_NONE;
	}
	/** The gate's read: a latch older than MaxAgeSeconds (0 = ageless) is stale and clears. */
	bool ReadLatched(double NowSeconds, float MaxAgeSeconds)
	{
		if (bOnIsland && MaxAgeSeconds > 0.f && NowSeconds - LatchedAtSeconds > MaxAgeSeconds)
		{
			Clear();
		}
		return bOnIsland;
	}
	/** A completed full-path move, a landing, a full draw: the fact is gone, no cooldown
	 *  — and a stranded bot is stranded no longer. */
	void Clear()
	{
		BadDraws = 0;
		bOnIsland = false;
		LatchedAtSeconds = -1.0;
		Confirmation = EConfirm::Untested;
		bStranded = false;
	}
	/** An Egress failure or a refuted hypothesis: gone, and not re-measured for a while. */
	void ClearWithCooldown(double NowSeconds, float CooldownSeconds)
	{
		Clear();
		NoLatchBeforeSeconds = NowSeconds + FMath::Max(CooldownSeconds, 0.f);
	}
	/** W-REVIEW #3 H2: Egress found no policy-legal lip on a CONFIRMED island. The
	 *  cooldown as above, plus STRANDED for its length: Wander draws nothing. */
	void Strand(double NowSeconds, float CooldownSeconds)
	{
		ClearWithCooldown(NowSeconds, CooldownSeconds);
		bStranded = true;
	}
	/** Stranded until the cooldown lapses (lazily dropped here) or Clear(). */
	bool IsStranded(double NowSeconds)
	{
		if (bStranded && NowSeconds >= NoLatchBeforeSeconds)
		{
			bStranded = false;
		}
		return bStranded;
	}
	/** Possession: everything, the absolute cooldown stamp included. */
	void Reset()
	{
		Clear();
		NoLatchBeforeSeconds = -1.0;
	}
};

/** Phase 13 (AIB24) `teammate_overlap_events`: a teammate inside the capsule sum, as an
 *  EPISODE sampled at think cadence — opens on the first sample with an ally inside,
 *  tracks the peak count, closes on the first without (or with the body). The controller
 *  writes the `teammate overlap over` line only for spells past
 *  AIB::TeammateOverlapReportSeconds. Worldless: the spec drives it with plain seconds. */
struct AIBOT_API FAIBOverlapEpisode
{
	double SinceSeconds = -1.0;
	int32 PeakCount = 0;

	/** One sample. True when this sample CLOSED an episode (Out* filled). */
	bool Note(int32 AlliesInside, double NowSeconds, float& OutSeconds, int32& OutPeak)
	{
		if (AlliesInside > 0)
		{
			if (SinceSeconds < 0.0)
			{
				SinceSeconds = NowSeconds;
				PeakCount = 0;
			}
			PeakCount = FMath::Max(PeakCount, AlliesInside);
			return false;
		}
		return Close(NowSeconds, OutSeconds, OutPeak);
	}
	/** The body-gone close. True when an episode was open. */
	bool Close(double NowSeconds, float& OutSeconds, int32& OutPeak)
	{
		if (SinceSeconds < 0.0)
		{
			return false;
		}
		OutSeconds = static_cast<float>(NowSeconds - SinceSeconds);
		OutPeak = PeakCount;
		Reset();
		return true;
	}
	void Reset()
	{
		SinceSeconds = -1.0;
		PeakCount = 0;
	}
};

/** AIB26 — THE FLANK LATCH. Flank's want may reach zero ONLY through this (the audit's
 *  VETO-bypass rule): a hidden, reachable point off the fight line, found ONCE by the
 *  controller's bounded search and held here until a definitive event — arrival, a
 *  refused or stalled walk, the belief drifting a ring radius from where it was measured,
 *  or the ambition leaving the fight. Never re-traced per think: a per-think trace that
 *  went dark for one tick would veto Flank's own commit and dither the bot through the
 *  manoeuvre. Controller-held for the AIB22 reason (task instance data is recreated on
 *  re-entry). Worldless: the spec drives it with vectors and seconds. */
struct AIBOT_API FAIBFlankLatch
{
	FVector Point = FVector::ZeroVector;
	FVector BeliefAtLatch = FVector::ZeroVector;
	float DetourUU = 0.f;
	double LatchedAtSeconds = -1.0;
	bool bHasPoint = false;
	/** AIB26 W-REVIEW M2: the point was REACHED. A Flank entry that finds this set runs
	 *  silently until the tactic engine hands to Push (arrival zeroed the point term) —
	 *  never a failure strike, which self-suppressed the tactic for 20 s. A new latch
	 *  or any clear drops it. */
	bool bDone = false;

	void Latch(const FVector& InPoint, const FVector& InBelief, float InDetourUU, double NowSeconds)
	{
		Point = InPoint;
		BeliefAtLatch = InBelief;
		DetourUU = InDetourUU;
		LatchedAtSeconds = NowSeconds;
		bHasPoint = true;
		bDone = false;
	}
	void Clear() { bHasPoint = false; bDone = false; LatchedAtSeconds = -1.0; }
	void MarkDone() { Clear(); bDone = true; }
	/** The point was hidden from WHERE THE BELIEF WAS; a belief that moved a ring radius
	 *  has made it a guess, and a guess is cleared, not walked. */
	bool IsStale(const FVector& BeliefNow, float DriftUU) const
	{
		return bHasPoint && FVector::DistSquared(BeliefNow, BeliefAtLatch) > FMath::Square(DriftUU);
	}
};

namespace AIBSweep
{
	/** The walking pan: a triangle wave in [-Arc, +Arc] over an unwrapped phase in
	 *  degrees, starting at 0 when Phase == Arc. Continuous, so the mover's facing
	 *  never sees a jump larger than the sweep rate. */
	inline float PanOffsetDegrees(float PhaseDegrees, float ArcDegrees)
	{
		if (ArcDegrees <= 0.f)
		{
			return 0.f;
		}
		const float Period = 4.f * ArcDegrees;
		const float P = FMath::Fmod(FMath::Fmod(PhaseDegrees, Period) + Period, Period);
		return P < 2.f * ArcDegrees ? P - ArcDegrees : 3.f * ArcDegrees - P;
	}
}

/**
 * The HAND. Owns the engine perception (eyes/ears), feeds the sensorium, hosts the brain
 * (Phase 2), runs the executor (Phase 3), presses verbs — and decides nothing itself.
 *
 * Server-only by construction, tickless by law: a think timer pumps the sensorium and,
 * later, the brain. Because there is no tick, the engine's focus-based aim never runs —
 * aim will be stepped explicitly by executor tasks (the seam audit's lesson).
 *
 * Perception is FFA-open (detect everyone; hostility is decided above perception, so
 * teams can land later without touching the senses) — the pattern transcribed from the
 * host's compiled controller, not designed fresh.
 */
UCLASS(Config=Game)
class AIBOT_API AAIBBotController : public AAIController
{
	GENERATED_BODY()

public:
	AAIBBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The avatar door, resolved at possession. Null when the pawn carries no adapter —
	 *  loud (one Error) and the bot stands, never crashes. Validity rides the GC-tracked
	 *  UPROPERTY half of the pair: a host destroying its adapter component mid-life must
	 *  yield null here, never a dangling raw pointer (W-REVIEW P3, two passes). */
	IAIBAvatarInterface* GetAvatar() const { return IsValid(AvatarObject) ? Avatar : nullptr; }

	/** The matured world — the ONLY awareness anything downstream may read. */
	const FAIBSensorium& GetSensorium() const { return Sensorium; }

	/** The arbitration layer; valid while possessing on the authority. */
	UAIBAmbitionEngine* GetAmbitionEngine() const { return AmbitionEngine; }

	/** AIB26: the SECOND engine — Push/Flank/Hold under Engage (Brain/AIBTactic.h). Same
	 *  class, same laws (hysteresis, commit, VETO, suppression); scored right after the
	 *  ambition each Think against the same facts plus the flank latch's objective fact. */
	UAIBAmbitionEngine* GetTacticEngine() const { return TacticEngine; }

	/** AIB26 replay identity. The host's match seed and this bot's STABLE spawn slot
	 *  (GetUniqueID is not stable across runs) key the `decide` line and seed
	 *  DecisionRandom. Until the manager hands them over (AIB25's seam) the index reads
	 *  -1 and the decision stream falls back to the per-life hash — deterministic, not
	 *  replayable across runs. */
	void SetMatchSeed(int32 InSeed) { MatchSeed = InSeed; }
	void SetBotIndex(int32 InIndex) { BotIndex = InIndex; }
	// GetBotIndex / GetLifeSeed: Phase 14's seed triple below (one declaration).

	/** AIB26: see FAIBFlankLatch. The Flank task walks it and clears it on arrival or
	 *  failure (Why is logged); Think clears it when the belief drifts or the fight ends. */
	const FAIBFlankLatch& GetFlankLatch() const { return FlankLatch; }
	void ClearFlankLatch(const TCHAR* Why);
	/** AIB26 M2: the Flank task reached the point — see FAIBFlankLatch::bDone. */
	void MarkFlankDone() { FlankLatch.MarkDone(); }

	/** AIB26: the tactic's mirror of NoteCurrentAmbitionFailed — a tactic child that
	 *  could not run (or a Hold that reached HoldMaxSeconds) rests that tactic on the
	 *  tactic engine so the next one gets its turn. */
	void NoteCurrentTacticFailed(const TCHAR* Why);

	/** AIB26: Hold's clock, controller-held so an Engage flap re-entering Hold cannot
	 *  restart it (the grenade-cooldown lesson). Entered sets it once; the Hold task
	 *  clears it at HoldMaxSeconds (W-REVIEW H1 — one `hold over` per stand) and Think
	 *  clears it the moment the tactic is no longer Hold. -1 = not holding. */
	void NoteHoldEntered(double NowSeconds) { if (HoldSinceSeconds < 0.0) { HoldSinceSeconds = NowSeconds; } }
	void NoteHoldOver() { HoldSinceSeconds = -1.0; }
	double GetHoldSinceSeconds() const { return HoldSinceSeconds; }

	/** AIB23 W-REVIEW H1 — THE TRIGGER'S OWN EYES. True when the bot's eyes have a clear
	 *  line to the BELIEVED point of the held target (the sensorium's belief, never the
	 *  live actor; the target's own body is ignored — the question is the wall). A
	 *  callout may move the feet, never the trigger: FireWhenAble presses only on current
	 *  sight or on this. False without a pawn, a world, or a held target. */
	bool HasLineOfSightToBelief() const;

	/** The execution surface (Phase 3). The executor drives it; nothing else touches it. */
	UStateTreeAIComponent* GetStateTreeComponent() const { return StateTreeComponent; }

	/** Where the compiled behaviour asset lives — ini-set, resolved by the executor at
	 *  possession. Soft by law 3's sibling rule: the module hard-references no asset. */
	const TSoftObjectPtr<UStateTree>& GetBotStateTreePath() const { return BotStateTree; }

	/** The last matured, arbitrated world — what executor tasks read mid-frame. Facts are
	 *  built ONCE per Think (F3: one belief sample per pump); tasks reading this cache
	 *  cannot accidentally re-sample the live world between pumps. */
	const FAIBFacts& GetLastFacts() const { return LastFacts; }

	// -- Phase 4 integration: the skill surface executor tasks consume ---------------
	// The PROFILE answers "how good is this bot at X"; the STATES are per-life policy
	// scratch that must survive branch re-entry (StateTree re-initialises instance data
	// on every state ENTRY — the grenade-cooldown lesson, applied to every policy);
	// the STREAM is per-bot so no two bots dance in lockstep (F-3.7).
	const FAIBSkillProfile& GetSkillProfile() const { return SkillProfile; }

	/** Phase 8: the RESOLVED tier row — every consumer of a tier number reads this,
	 *  never a function-local default (the two "Phase 8 resolves the real tier"
	 *  markers this replaced). Valid from OnPossess; the defaults row before that. */
	const FAIBTierRow& GetTierRow() const { return ResolvedTier; }

	/** Host-callable (the game mode assigning mixed lobbies): takes effect at the NEXT
	 *  possession — a tier is a per-life resolution, same as every policy state. */
	void SetTierName(FName InTierName) { BotTier = InTierName; }
	FName GetTierName() const { return BotTier; }
	FAIBAimState& GetAimState() { return AimState; }
	FAIBMeleeState& GetMeleeState() { return MeleeState; }
	FAIBGrenadeState& GetGrenadeState() { return GrenadeState; }
	FAIBMovementState& GetMovementState() { return MovementState; }

	/** AIB17 — the ally-fight note the idle wander walks toward. Read-only outside the
	 *  Note boundary; the tap in OnPerceptionUpdated is the one writer. */
	const FAIBAllyFightMemory& GetAllyFightMemory() const { return AllyFightMemory; }

	/** AIB9's moment instrument (both const — the failure describer holds a const ref):
	 *  when this life began, and when it last took damage (0 = never this life). Together
	 *  with the pawn's falling state they separate the off-mesh candidate causes the
	 *  ticket names — fresh spawn, mid-fall, post-knockback, steady-state — at the one
	 *  site that already reports self=NO. */
	double GetPossessedAtSeconds() const { return PossessedAtSeconds; }
	double GetLastDamageTakenAtSeconds() const { return DamageLedger.LastTakenAtSeconds(); }

	/** THE YAW CLAIM (founder, 1 Sep: bots "walking and running in reverse").
	 *
	 *  The adapter-hosted pawns are FPS pawns — bUseControllerRotationYaw true,
	 *  bOrientRotationToMovement false — so the BODY's facing is the CONTROL rotation and
	 *  nothing else. Every task that aims writes it; the movers never did, so a bot
	 *  crossing the map kept facing wherever it last looked and travelled sideways or
	 *  backwards the whole way.
	 *
	 *  The mover now faces its own travel, but only when nothing better has asked for the
	 *  yaw this tick. Rather than each mover guessing whether an aimer is running beside
	 *  it in the branch — which is a question about a tree it cannot see — an aimer
	 *  CLAIMS the yaw as it steers, and facing-travel is the fallback for a tick with no
	 *  claim. Order inside the tick does not matter: whoever claims writes last or the
	 *  mover sees the claim and stands aside, so the aim always wins and combat backpedal
	 *  is untouched. A future task that steers is covered automatically. */
	void NoteYawClaimed(double NowSeconds) { YawClaimedAtSeconds = NowSeconds; }
	bool IsYawClaimed(double NowSeconds) const
	{
		// One think-interval of grace, not one frame: tasks tick on the tree's cadence
		// while the mover may be stepped by a faster one, and a claim that expired
		// between them would let the body twitch back toward its path mid-aim.
		return YawClaimedAtSeconds >= 0.0
			&& (NowSeconds - YawClaimedAtSeconds) <= AIB::YawClaimHoldSeconds;
	}
	FRandomStream& GetPolicyRandom() { return PolicyRandom; }

	/** Phase 14 — THE SEED TRIPLE. BotIndex is the manager's stable spawn slot (-1 with
	 *  no manager, i.e. headless); LifeSeed = HashCombine(HashCombine(MatchSeed, BotIndex),
	 *  LifeIndex) is what every per-life stream hashes off, so two -AIBSeed=N runs draw the
	 *  same first jink, the same latency, the same lanes. Phase 15's `decide` line keys on
	 *  BotIndex, never on a name or a UniqueID. */
	int32 GetBotIndex() const { return BotIndex; }
	uint32 GetLifeSeed() const { return LifeSeed; }
	/** AIB23 M5/L3 + AIB24 M5: this life's seeded ring phase in degrees — the approach
	 *  ring's and the hill ring's slot, off LifeSeed (replay-stable), never a UniqueID. */
	float GetRingPhaseDeg() const { return static_cast<float>(HashCombine(LifeSeed, 4099u) % 360u); }

	/** Phase 14 — see FAIBRouteBias. Drawn once per life; UAIBQueryFilter reads the
	 *  per-lane cost through GetRouteLaneCost on every query this controller issues.
	 *  Route heat (lane C) multiplies in HERE, never in the filter. */
	const FAIBRouteBias& GetRouteBias() const { return RouteBias; }
	float GetRouteLaneCost(int32 LaneId) const { return RouteBias.CostOf(LaneId); }

	/** AIB22 H1/F9 — see FAIBSweepBudget. SweepLook spends it only while the body is
	 *  still; Think resets it the moment the body moves, and a NEW post refills it. */
	FAIBSweepBudget& GetSweepBudget() { return SweepBudget; }

	/** AIB22 5(B) — see FAIBIslandLatch. Wander notes every draw; the Egress gate reads
	 *  and confirms it; Egress's landing, a full draw, or any completed full-path move
	 *  (OnMoveCompleted) clears it. */
	FAIBIslandLatch& GetIslandLatch() { return IslandLatch; }
	const FAIBIslandLatch& GetIslandLatch() const { return IslandLatch; }

	/** THE ISLAND ANCHORS (fix #4 R1, replacing #3 H3's single anchor): in test order, the
	 *  CURRENT want's goal (the mode objective POIs, or Search/Seek's fresh last-known),
	 *  the goal of the last completed full-path move, and every PlayerStart in the level
	 *  (cached at possession). Raw locations; the gate projects them. The island verdict
	 *  is FAIBIslandLatch::Confirm over the whole list — one anchor on the bot's own
	 *  island (a corner spawn pad) can no longer refute by itself. */
	void GetIslandAnchors(TArray<FAIBIslandAnchor>& OutAnchors) const;

	/** Fix #4 R8: Egress calls this right after each move it issues (the lip walk and the
	 *  step-off) — the move now in flight is EGRESS'S OWN, so its completion neither
	 *  clears the latch (the lip walk completing on the island cleared the fact one tick
	 *  before the step-off, the gate went false, ExitState stopped the bot on top) nor
	 *  becomes the island anchor. */
	void MarkEgressMove();

	/** Fix #4 R3: the ONE stall clock — see FAIBLocomotionState. */
	FAIBLocomotionState& GetLocomotion() { return Locomotion; }

	/** Fix #4 R9's instrument: the path follower's link press stamp, so the stall line's
	 *  `jumped=` reads whether a LINK jump fired during the stall — the only jump there is. */
	void NoteLinkJumped(double NowSeconds) { LastLinkJumpAtSeconds = NowSeconds; }
	double GetLastLinkJumpAtSeconds() const { return LastLinkJumpAtSeconds; }

	/** W-REVIEW #3 H3: every move funnels through here (MoveToLocation/MoveToActor both
	 *  call it), so this is where a PATHED request's destination is remembered for
	 *  OnMoveCompleted — the follower has Reset() its path before it broadcasts. An
	 *  already-at-goal request had no path and records nothing. */
	virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath = nullptr) override;

	/** ONE-SHOT STOP ON LANDING (W-REVIEW M6). Egress's ExitState mid-fall must not cancel
	 *  the fall, but the unpathed step-off request would otherwise outlive the landing.
	 *  Arms against the move in flight NOW; Think consumes it on the first grounded
	 *  sample and stops the mover only if that same request is still the current one. */
	void ArmStopOnLanding();

	/** SweepLook's pan while WALKING: the yaw offset the movers' facing block adds to the
	 *  travel heading (see TickLocomotion). 0 when nothing sweeps; SweepLook zeroes it on
	 *  exit so no later mover inherits a turned head. Not a yaw claim — the mover still
	 *  owns the facing; the sweep only bends it. */
	void SetTravelPanDegrees(float Degrees) { TravelPanDegrees = Degrees; }
	float GetTravelPanDegrees() const { return TravelPanDegrees; }

	/** AIB22 H1 — END THE WANT. A search that has swept its post and found nothing, or
	 *  gave up short of it after walking, forgets the lead: MemoryFreshness reads 0 on the
	 *  next Think, Search vetoes its own commit, and Roam wins. NOT for a refused path
	 *  (W-REVIEW H1): a refusal is the off-mesh-self case and says nothing about the
	 *  lead — that arms NoteCurrentAmbitionFailed instead. Logs the abandonment once. */
	void ForgetSearchMemory(const TCHAR* Why, float AfterSeconds);

	/** AIB22: see EAIBStillTactic. Idempotent per flag. */
	void SetStillTactic(EAIBStillTactic Tactic, bool bActive)
	{
		const uint8 Bit = static_cast<uint8>(Tactic);
		StillTactics = static_cast<uint8>(bActive ? (StillTactics | Bit) : (StillTactics & ~Bit));
	}
	bool HasStillTactic(EAIBStillTactic Tactic) const { return (StillTactics & static_cast<uint8>(Tactic)) != 0; }

	/** The executor's live leaf state name (NAME_None when nothing runs). */
	FName GetActiveStateName() const;

	// -- Phase 6: the provider doors (pulled from UAIBBotManager at possession) --------
	// Same twin-pointer validity rule as the avatar door: the interface half runs, the
	// GC-tracked half decides whether it still may.
	IAIBAmbitionProvider* GetAmbitionProvider() const { return IsValid(AmbitionProviderObject) ? AmbitionProvider : nullptr; }
	IAIBWorldQuery* GetWorldQuery() const { return IsValid(WorldQueryObject) ? WorldQuery : nullptr; }

	/** The typed join a mode branch's mover needs: the ObjectiveKind of the CURRENT
	 *  ambition, from the mode set cached at the last refresh. Invalid when the current
	 *  want is not a mode want or names no kind. */
	FGameplayTag GetObjectiveKindForCurrentAmbition() const;

	/** THE EXECUTOR'S ONE REPORT BACK (AIB16). A branch that could not run tells the
	 *  brain so, and the engine silences that want long enough for another to have a
	 *  turn. Without it a failing branch keeps its score, keeps winning, and the bot
	 *  stops making decisions entirely — measured: 0 kills against 76.
	 *
	 *  Deliberately the ONLY direction the executor speaks: it reports what happened,
	 *  never what to want next. Choosing stays with the engine. */
	void NoteCurrentAmbitionFailed();

	/** THE POSSESSION OBLIGATION, finally payable (ARCHITECTURE's recorded CTF-in-Slayer
	 *  debt): clear + core + the CURRENT mode's translated ambitions + one immediate
	 *  Think, so the empty-tag window never reaches a tree selection. Also the mid-life
	 *  mode-swap API — the host calls it on a round transition. */
	void RefreshAmbitions();

	/** THE ONE THROTTLE THAT MUST OUTLIVE A BRANCH, and it lives here for a mechanical
	 *  reason worth naming: a StateTree re-initialises a state's instance data from the
	 *  compiled defaults every time that state is ENTERED, so a cooldown kept in a task's
	 *  scratch is silently reset by an Engage branch that flaps — and Engage flaps by
	 *  design (its belief tasks fail on a visibility loss, re-selecting 0.2s later). A
	 *  grenade cooldown reset every second is no cooldown at all, which is exactly the
	 *  "seven grenades in one second" failure. The CONTROLLER outlives every state, so the
	 *  gate is a wall-clock stamp on it. The duration stays with the behaviour that spends
	 *  it — the task passes it in.
	 *
	 *  Fire, reload, melee and swap deliberately do NOT come here: each is refused
	 *  harmlessly by the host's own ability state, and a re-entry costing one extra tap is
	 *  not a fairness problem. A grenade is. */
	bool CanThrowGrenade() const;
	void NoteGrenadeThrown(float CooldownSeconds);

	/** THE DASH's own throttle. The host refuses a dash on cooldown, and a refused verb
	 *  pressed on a timer is the futile-press shape F7 bans — so the bot tracks its own
	 *  window rather than discovering the answer by being told no. */
	bool CanDash() const;
	void NoteDashed(float CooldownSeconds);

	/** The game's projectile warning seam calls this (via the adapter wiring). It NOTES —
	 *  the dodge happens only after the stimulus matures (FAIRPLAY F2). */
	void NoteIncomingBlast(const FVector& Center, float Radius, double DetonateAtSeconds);

	/** THE DAMAGE SEAM (Phase 5). The host's one-per-hit damage site calls these on the
	 *  authority. Taken: the hit's fraction of THIS bot's max health, plus the attacker
	 *  and its location AT THE HIT — which becomes a matured MEMORY through the same
	 *  reaction clock as every sense (being shot makes a bot go and LOOK; a perfect lock
	 *  on someone never seen would be omniscience — the host's own ruling, kept). Dealt:
	 *  the fraction of the VICTIM's max health a hit this bot landed removed. Both feed
	 *  the momentum ledger the confidence model and the damage-history facts read. */
	void NoteDamageTaken(AActor* Attacker, const FVector& AttackerLocation, float FractionOfMaxHealth);
	void NoteDamageDealt(float FractionOfVictimMaxHealth);

	// FFA seam, verbatim from the host's proven pattern: one shared "no team",
	// hostility decided per-pawn. A team system replaces these two overrides.
	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(255); }
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	/** AIB22 W-REVIEW M3: the one place a move's completion is observed. A move that
	 *  reached its goal on a NON-partial path proves the feet are on connected ground
	 *  and clears the island latch (and Stranded), whatever state issued it; a PATHED
	 *  one also becomes the island anchor (#3 H3). */
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	/** Teardown (PIE end, travel) does not promise UnPossess — this is the ordered path
	 *  that stops the executor while the avatar door is still valid (W-REVIEW P3). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void OnPerceptionForgotten(AActor* Actor);

private:
	void Think();

	/** AIB22 `idle_seconds`: emit the open still spell (if any) and clear it. */
	void CloseIdleEpisode(double NowSeconds);

	/** Phase 14 `route` line: the lane sequence the follower's current path crosses,
	 *  logged when it differs from the last one logged this life (a NEW route, not every
	 *  repath on the belief's drift). */
	void LogRouteIfChanged(const FVector& Goal);

	/** Phase 13 `teammate_overlap_events`: emit the open overlap spell (if any, and past
	 *  the report threshold) and clear it. */
	void CloseOverlapEpisode(double NowSeconds);

	/** AIB26: one bounded search for a flank point — eight ring samples around the
	 *  midpoint between the feet and the BELIEF (never the live actor: F2-B), kept when
	 *  nav-projected, reachable on a full path, hidden from the belief's eye line, and
	 *  inside the detour clamp; the shortest detour latches. None found = Flank noted
	 *  failed on the tactic engine (suppression is what throttles the next search). */
	void SearchFlankPoint(const FVector& Belief, double NowSeconds);

	/** AIB26: the tactic layer's Think — latch upkeep, the second rescore, its log line. */
	void ThinkTactic(FGameplayTag Ambition, double NowSeconds);

	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UAIPerceptionComponent> BotPerception;

	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	/** Born in the constructor (components must be); logic start is the executor's call,
	 *  never automatic — the host controller's proven, respawn-idempotent shape. */
	UPROPERTY(VisibleAnywhere, Category = "AIBot")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

	/** Soft path to the compiled tree, from [/Script/AIBot.AIBBotController] in ini.
	 *  UAIBTreeAuthoring builds the asset it names, from the editor. */
	UPROPERTY(Config)
	TSoftObjectPtr<UStateTree> BotStateTree;

	/** Seconds between thinks. Config so the terminal can tune cadence without a
	 *  recompile; the floor law does not live here (the clock owns it). */
	UPROPERTY(Config)
	float ThinkIntervalSeconds = 0.1f;

	/** The trace channel the blast perceivability gate tests (an ECollisionChannel
	 *  value; int because the ini writes a number). Default ECC_Visibility — but a
	 *  channel's MEANING is entirely the host project's collision config (a project
	 *  whose cover blocks weapons yet ignores Visibility turns this gate into a no-op
	 *  through a wall). The HOST decides via ini what channel honestly answers "could
	 *  eyes reach the blast point" (W-REVIEW P3); this module only defaults it. */
	UPROPERTY(Config)
	int32 BlastPerceivabilityChannel = ECC_Visibility;

	/** Phase 8: which tier this bot resolves at possession. Config so one ini line
	 *  runs a whole lobby at a tier; the host's mode can override per-bot through
	 *  SetTierName for mixed lobbies. An unknown name falls back to the defaults row,
	 *  loudly (F7). */
	UPROPERTY(Config)
	FName BotTier = TEXT("Marine");

	/** Phase 8: draw the per-bot overlay (ambition scores, confidence, skill vector,
	 *  stimulus queue) over each bot's head every think. Config: flip in the ini, or
	 *  at runtime from the editor's settings — the eyes-on half of proof 3. */
	UPROPERTY(Config)
	bool bDebugOverlay = false;

	/** The resolved row (see GetTierRow). */
	FAIBTierRow ResolvedTier;

	FAIBSensorium Sensorium;
	FTimerHandle ThinkTimer;

	UPROPERTY()
	TObjectPtr<UAIBAmbitionEngine> AmbitionEngine;

	/** For the one ambition-switch log line per change (the verifier's instrument). */
	FGameplayTag LastLoggedAmbition;

	// -- AIB26: the tactic layer and the replay instrument ---------------------------
	UPROPERTY()
	TObjectPtr<UAIBAmbitionEngine> TacticEngine;
	FGameplayTag LastLoggedTactic;
	FAIBFlankLatch FlankLatch;
	double HoldSinceSeconds = -1.0;
	int32 MatchSeed = 0;
	// BotIndex: Phase 14's member below (INDEX_NONE until the manager assigns it).
	/** Never reset per life: the replay diff sorts on (bot, seq) across a whole match. */
	uint32 DecisionSeq = 0;
	/** The decision stream — today only the flank ring's offset draws from it; every
	 *  draw is counted for the `rng=` field so two runs that consumed the stream
	 *  differently show it on the line. */
	FRandomStream DecisionRandom;
	int32 DecisionRandomDraws = 0;

	/** For the one fairness log line per acquisition (aib-verifier's sample). */
	TWeakObjectPtr<AActor> LastLoggedTarget;

	/** One fact snapshot per Think; see GetLastFacts. */
	FAIBFacts LastFacts;

	/** World seconds before which no grenade may be thrown; see CanThrowGrenade. */
	float NextGrenadeThrowTimeSeconds = 0.f;

	/** Same, for the dash. Deliberately a BOT-SIDE guess at the host's cooldown rather than
	 *  a query: the module owns no ability system and must not learn one. Set slightly long
	 *  so the bot is never the thing that discovers the real number by being refused. */
	float NextDashTimeSeconds = 0.f;

	/** World seconds at OnPossess — the current life's birth stamp (AIB9: an off-mesh
	 *  report inside the first breaths of a life points at the SPAWN, not at play). */
	double PossessedAtSeconds = 0.0;

	/** Counts possessions, and exists ONLY to vary the per-life seeds: re-seeding a
	 *  respawn from the bare controller id replayed a byte-identical draw sequence every
	 *  life — the same first jink, the same reaction latency, a learnable tell that
	 *  reset on death (W-REVIEW P4+5 H5). Deterministic given (bot, life). */
	int32 LifeIndex = 0;

	/** Phase 14: see GetBotIndex / GetLifeSeed / GetRouteBias. The signature is the last
	 *  `route` line's lane sequence; the key is the last corridor walked (AIB25 L6: the
	 *  walk runs only for a corridor that differs). Both reset per life. */
	int32 BotIndex = INDEX_NONE;
	uint32 LifeSeed = 0;
	FAIBRouteBias RouteBias;
	FString LastRouteSignature;
	uint32 LastRouteCorridorKey = 0;

	// Phase 5: momentum + judgment. The ledger is the damage-history facts' source; the
	// model turns facts into ConfidenceNorm at think cadence. Phase 4's profile gets its
	// first consumer here (Confidence level = judgment quality); Phase 8 re-resolves it
	// from the real tier row.
	FAIBDamageLedger DamageLedger;

	/** Last time an aimer took the yaw. -1 = never; reset with the body. */
	double YawClaimedAtSeconds = -1.0;

	/** AIB22 idle instrument: when the current no-input spell began (-1 = none open),
	 *  the tactic flags up NOW, and the set the open spell is attributed to — a spell is
	 *  ONE set; the set changing closes it (W-REVIEW #3 H1). */
	double IdleSinceSeconds = -1.0;
	uint8 StillTactics = 0;
	uint8 IdleTactics = 0;

	/** Phase 13 instruments (see FAIBOverlapEpisode / AIB::PositionSampleSeconds). Both die
	 *  with the body. */
	FAIBOverlapEpisode OverlapEpisode;
	double LastPositionSampleSeconds = -1.0;

	/** AIB22 H1/F9: see GetSweepBudget / SetTravelPanDegrees. Both die with the body. */
	FAIBSweepBudget SweepBudget;
	float TravelPanDegrees = 0.f;
	FAIBIslandLatch IslandLatch;
	FAIBLocomotionState Locomotion;
	double LastLinkJumpAtSeconds = -1.0;
	/** See GetIslandAnchors / MoveTo / MarkEgressMove / ArmStopOnLanding. All of it dies
	 *  with the body. */
	TArray<FVector> PlayerStartLocations;
	FVector PendingMoveGoal = FVector::ZeroVector;
	uint32 PendingMoveRequestId = 0;
	uint32 EgressMoveRequestId = 0;
	FVector LastFullPathGoal = FVector::ZeroVector;
	bool bHasLastFullPathGoal = false;
	bool bStopOnLanding = false;
	uint32 StopOnLandingRequestId = 0;
	/** Fix #4 R7: no arbitration, no tree, no move until the pawn has projected onto the
	 *  navmesh ONCE this life (the t<2s spawn burst refused 95% of Spillway's refusals).
	 *  The executor starts from the first on-nav Think, after that Think's rescore. */
	bool bNavSeen = false;
	bool bWaitingForNavLogged = false;
	bool bExecutorStarted = false;
	FAIBConfidenceState ConfidenceState;
	FAIBSkillProfile SkillProfile;

	// Phase 4 integration: per-life policy scratch (see the accessors' comment) and the
	// execution-side draw stream. One stream for all four skills is deliberate — the
	// F-3.7 hazard is cross-BOT lockstep, not cross-skill; per-skill streams would buy
	// replay granularity nothing downstream reads yet.
	FAIBAimState AimState;
	FAIBMeleeState MeleeState;
	FAIBGrenadeState GrenadeState;
	FAIBMovementState MovementState;

	/** AIB17 — see GetAllyFightMemory. Reset at possession: a fresh life heard nothing. */
	FAIBAllyFightMemory AllyFightMemory;

	/** PHASE 12 — the `target claim DENIED` line is edge-triggered per target (one per
	 *  denial episode, not per think); the report throttle is per target (world seconds
	 *  of the last report taken). Both die with the body. */
	TWeakObjectPtr<AActor> LastDeniedTarget;
	TMap<FObjectKey, double> TeamReportTakenAt;
	FRandomStream PolicyRandom;

	/** The misjudge draws. Its OWN stream, seeded per bot beside the sensorium's:
	 *  sharing would let a confidence redraw shift every later reaction latency, and
	 *  determinism per subsystem is what keeps specs and replays honest. */
	FRandomStream ConfidenceRandom;

	/** True once the host's damage seam has EVER called in — per controller, never reset
	 *  per life: seam wiring is a host property. Until then damage history stays an
	 *  honest UNKNOWN rather than a confident zero. */
	bool bDamageSeamSeen = false;

	IAIBAvatarInterface* Avatar = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> AvatarObject;

	// Phase 6: the provider doors + the mode set the last refresh translated (cached so
	// the executor's kind join reads a list this controller owns, never a live provider
	// walk per tick).
	IAIBAmbitionProvider* AmbitionProvider = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> AmbitionProviderObject;

	IAIBWorldQuery* WorldQuery = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> WorldQueryObject;

	TArray<FAIBModeAmbition> CachedModeAmbitions;

	// The executor door, the avatar door's twin: the interface pointer is what runs, the
	// UObject pointer is what keeps it alive. Today the concrete type is the StateTree
	// executor; a Behavior Tree impl replaces one NewObject line, nothing else.
	IAIBExecutor* Executor = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> ExecutorObject;
};
