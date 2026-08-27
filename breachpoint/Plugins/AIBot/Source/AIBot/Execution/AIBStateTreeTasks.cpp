#include "Execution/AIBStateTreeTasks.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/PawnMovementComponent.h"

#include "AIBotModule.h"
#include "Brain/AIBAmbitionEngine.h"
#include "Core/AIBBotController.h"
#include "Core/AIBTags.h"
#include "Interfaces/AIBAvatarInterface.h"
#include "Interfaces/AIBWorldQuery.h"
#include "NavigationSystem.h"
#include "Perception/AIBSensorium.h"
#include "Skills/AIBAimPolicy.h"
#include "Skills/AIBGrenadePolicy.h"
#include "Skills/AIBMeleePolicy.h"
#include "Skills/AIBMovementPolicy.h"
#include "StateTreeExecutionContext.h"
#include "Team/AIBTeamCoordinator.h"

namespace
{
	/** The controller every node runs on: the bound instance data when the editor set one,
	 *  else the execution context's OWNER — the AIController the StateTreeAIComponent lives
	 *  on. The fallback is what lets a CODE-authored tree run at all: property bindings are
	 *  editor-only graph data (the host's proven pattern, same reasoning verbatim). */
	AAIBBotController* ResolveBot(const FStateTreeExecutionContext& Context, AAIController* BoundController)
	{
		if (AAIBBotController* Bound = Cast<AAIBBotController>(BoundController))
		{
			return Bound;
		}
		return Cast<AAIBBotController>(Context.GetOwner());
	}

	/** Turns the CONTROL rotation toward a point at a bounded rate, and the body with it.
	 *  Exists because the controller has no tick, so the engine's focus-driven turn never
	 *  runs — the host walked into both halves of that bug (bullets into walls, bodies
	 *  sliding sideways) before writing this shape. */
	void SteerControlRotation(AAIController& Controller, const FVector& WorldPoint, float DegreesPerSecond, float DeltaTime)
	{
		APawn* Pawn = Controller.GetPawn();
		if (!Pawn)
		{
			return;
		}
		const FVector ToPoint = WorldPoint - Pawn->GetPawnViewLocation();
		if (ToPoint.IsNearlyZero())
		{
			return;
		}
		const FRotator Desired = ToPoint.Rotation();
		// NO SNAP PATH (F4): zero-or-negative is an authoring accident, not a request
		// for instant aim — it falls back to the default rate. F1 has one clamp site
		// for time; this is the one bound site for turn.
		const float Rate = DegreesPerSecond > 0.f ? DegreesPerSecond : 360.f;
		const FRotator Stepped = FMath::RInterpConstantTo(Controller.GetControlRotation(), Desired, DeltaTime, Rate);
		Controller.SetControlRotation(Stepped);
		Pawn->FaceRotation(Stepped, DeltaTime);
	}

	/** True when Point sits close enough to walkable ground to stand on, with the standing
	 *  spot written to OutPoint. The extent is vertically generous on purpose: a goal is far
	 *  more often slightly ABOVE or BELOW a floor than beside one, which is the ordinary case
	 *  on a multi-level arena. Callers that have a better answer than a refused path check
	 *  the bool; callers that do not just let the mover refuse. */
	bool ProjectToNav(UWorld* World, const FVector& Point, FVector& OutPoint)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		FNavLocation Projected;
		if (NavSys && NavSys->ProjectPointToNavigation(Point, Projected, FVector(300.f, 300.f, 400.f)))
		{
			OutPoint = Projected.Location;
			return true;
		}
		return false;
	}

	/** WHY a move was refused, in the three facts that separate causes a bare "refused"
	 *  cannot. Off-mesh and unreachable are different defects with different fixes, and a
	 *  log that says only "refused" sends the reader guessing — which is how a half-fix
	 *  gets shipped and measured as a win.
	 *
	 *    self=NO   the BOT is off the mesh. Nothing it asks for will ever path; this is a
	 *              spawn or placement bug and the goal is innocent.
	 *    goal=NO   an off-mesh goal — the class already fixed once. Seeing it again means a
	 *              site was missed, not that the fix failed.
	 *    both yes  genuine unreachability: separate islands, or a navlink that never
	 *              generated. Distance says which geometry to go and look at.
	 *
	 *  Built only on the failure path, so its cost never touches a bot that is moving. */
	FString DescribeMoveFailure(const AAIBBotController& Bot, const FVector& Goal)
	{
		const APawn* Pawn = Bot.GetPawn();
		const FVector Self = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		FVector Ignored;
		const bool bSelfOnNav = Pawn && ProjectToNav(Bot.GetWorld(), Self, Ignored);
		const bool bGoalOnNav = ProjectToNav(Bot.GetWorld(), Goal, Ignored);

		// AIB9 step 2/3, the WHERE and the MOMENT — appended only for self=NO, because
		// that is the case whose causes the ticket must separate (fresh spawn, mid-fall,
		// post-knockback, steady state) and the only one where the bot's own position is
		// the evidence. Exact format is load-bearing: the metrics harness transcribes it.
		FString OffMeshMoment;
		if (Pawn && !bSelfOnNav)
		{
			const UWorld* World = Bot.GetWorld();
			const double Now = World ? World->GetTimeSeconds() : 0.0;
			const UPawnMovementComponent* MoveComp = Pawn->GetMovementComponent();
			const double LastHitAt = Bot.GetLastDamageTakenAtSeconds();
			OffMeshMoment = FString::Printf(
				TEXT(" | off-mesh self at (%.0f, %.0f, %.0f) age=%.1fs falling=%s velZ=%.0f lastHit=%s"),
				Self.X, Self.Y, Self.Z,
				Now - Bot.GetPossessedAtSeconds(),
				(MoveComp && MoveComp->IsFalling()) ? TEXT("yes") : TEXT("no"),
				Pawn->GetVelocity().Z,
				LastHitAt > 0.0 ? *FString::Printf(TEXT("%.1fs"), Now - LastHitAt) : TEXT("never"));
		}

		// HEIGHTS, not just distance. "Both on the mesh and still refused" has two very
		// different shapes: two points on ONE floor with a wall between them, and two
		// points on DIFFERENT floors with no way up. Distance alone cannot tell them
		// apart, and the fix for each is nothing like the fix for the other.
		return FString::Printf(TEXT("self=%s goal=%s dist=%.0fuu selfZ=%.0f goalZ=%.0f dz=%.0f%s"),
			bSelfOnNav ? TEXT("yes") : TEXT("NO"),
			bGoalOnNav ? TEXT("yes") : TEXT("NO"),
			Pawn ? FVector::Dist(Self, Goal) : -1.f,
			Self.Z, Goal.Z, Goal.Z - Self.Z,
			*OffMeshMoment);
	}

	/** EVERY goal this file hands the mover is a REMEMBERED or DERIVED world point — a
	 *  sighting recalled from memory, a belief, or plain arithmetic off the pawn's own
	 *  location. None of those is guaranteed to be somewhere a body can stand, and
	 *  MoveToLocation REFUSES an off-navmesh goal outright. Unprojected, the flee goal
	 *  (pawn + away * distance) refused 33,095 times in one four-minute match against 124
	 *  Retreat ambitions: the brain decided correctly and the body never moved.
	 *
	 *  So projection happens HERE, at the single door to the mover, rather than at each
	 *  call site — four of the five sites pass a remembered point, and fixing only the two
	 *  that log loudly would leave the other two refusing in silence.
	 *
	 *  A FAILED projection is deliberately NOT swallowed: the original point goes through
	 *  and the mover refuses it, so a genuinely unreachable goal still fails loudly (F7)
	 *  instead of being quietly redirected somewhere the caller never asked for.
	 *
	 *  The extent is the vertical-generous box the sibling framework settled on: wide
	 *  enough to catch a goal beside a walkable surface, tall enough to catch one above or
	 *  below a floor, which is the common case on a multi-level arena. */
	EPathFollowingRequestResult::Type MoveToNavPoint(AAIBBotController& Bot, const FVector& Goal, float AcceptanceUU)
	{
		FVector Target;
		if (!ProjectToNav(Bot.GetWorld(), Goal, Target))
		{
			Target = Goal;
		}
		return Bot.MoveToLocation(Target, AcceptanceUU);
	}

	bool IsWithin(const AAIController& Controller, const FVector& Point, float RadiusUU)
	{
		const APawn* Pawn = Controller.GetPawn();
		return Pawn && FVector::Dist(Pawn->GetActorLocation(), Point) <= RadiusUU;
	}

	/** THE DISTANCE A TASK MAY ACT ON — this frame's, not the think's.
	 *
	 *  FAIBFacts::DistToTargetUU is rebuilt every 0.1s, which is right for WANTING (a want
	 *  that flickers at frame rate is not a want) and wrong for a LUNGE: at sprint speed a
	 *  bot covers ~90uu inside one stale think, and a melee reach is ~120. So the executor
	 *  side — which is allowed a world, unlike Brain/ — measures it now.
	 *
	 *  It is still the BELIEF, never the live actor: same F2-B rule the aim obeys, so a bot
	 *  cannot stab or grenade a position it has not honestly seen. */
	bool LiveDistanceToBelief(const AAIBBotController& Bot, float& OutDistanceUU)
	{
		const APawn* Pawn = Bot.GetPawn();
		if (!Pawn || !Bot.GetSensorium().HasVisibleTarget())
		{
			return false;
		}
		OutDistanceUU = FVector::Dist(Pawn->GetActorLocation(), Bot.GetSensorium().GetLastSeenLocation());
		return true;
	}

	/** SPRINT is released inside this multiple of a mover's arrival radius. The host's own
	 *  rule, transcribed with its reasoning: a bot that sprints into its firing position
	 *  arrives unable to shoot, because the sprint state holds for as long as the key is
	 *  down. Un-sprinted bots are also a competitive fact, not polish — sprint multiplies
	 *  move speed, so a sprinting human simply outran every bot, always. */
	constexpr float SprintBeyondRadiusFactor = 1.5f;

	/** The wedge watchdog: less ground than this gained for this long, with a goal still
	 *  ahead, and the bot spends ONE jump — a lip, a crate, a step is exactly the shape a
	 *  jump clears, and it costs nothing to find out. */
	constexpr float WedgeProgressUU = 50.f;
	constexpr float WedgeStallSeconds = 1.5f;

	/** RELOAD below a quarter magazine, re-tapped no faster than this. The magazine only
	 *  refills on the weapon's own notify, so a refused reload (frozen, dead, no montage)
	 *  would otherwise be re-pressed at tick rate forever. */
	constexpr float ReloadAtMagazineFraction = 0.25f;
	constexpr float ReloadRetrySeconds = 1.0f;

	/** MELEE commits inside this fraction of the HELD WEAPON's own reach — the host's rule,
	 *  and below 1 for the host's reason: a swing at the exact edge of the reach turns one
	 *  backward step into a whiff. The reach is never restated here; it comes through the
	 *  door, so retuning the weapon retunes the bot. */
	constexpr float MeleeCommitFraction = 0.8f;
	constexpr float MeleeRetrySeconds = 1.5f;

	/** THE MUZZLE GATE (W-REVIEW P4+5 H4). A burst may only BEGIN with the control
	 *  rotation already near the aim line — a fresh acquisition behind the bot otherwise
	 *  held the trigger through a 175-degree swing, hosing the arena for the half second
	 *  the turn takes. A burst already running BREAKS on a large swing (a mid-burst
	 *  target switch), on the same reasoning. Start tight, break loose: the gap between
	 *  the two is what keeps ordinary tracking from stuttering the trigger. */
	constexpr float BurstStartAlignDegrees = 10.f;
	constexpr float BurstBreakAlignDegrees = 30.f;

	// The GRENADE band lived here as honest defaults until Phase 4's integration: the
	// judgement is now FAIBGrenadePolicy's per-level recognition ladder (its own bands,
	// anchored inside the sight envelope). Only the throttle below stays task-side.

	/** THE COOLDOWN IS NOT OPTIONAL, and it is per BOT for a reason the host learned in
	 *  the world rather than on paper: eight bots that each throw the instant a band opens
	 *  throw eight grenades in one second, and the arena stops being a fight. Longer than
	 *  any host ability cooldown we could ask about, so the press is never a dead one — the
	 *  module must not know the host's cooldown tag, and this makes not knowing it safe. */
	constexpr float GrenadeCooldownSeconds = 8.f;

	/** WEAPON SWAP. The verb is a CYCLE, not a selection: press until the avatar says the
	 *  hand is right, exactly as a human rolls the mouse wheel past a slot they do not want
	 *  — which is also how a bot walks past an empty holster slot without anything in the
	 *  host's equipment needing to change for it. The pause is what lets the equip actually
	 *  take (pressing again next frame cancels it), and the cap is what stops a bot whose
	 *  whole loadout is dry from cycling for the rest of the match. Five presses is one full
	 *  lap of the audited host's five-slot carry: enough to reach any slot, once. */
	constexpr float SwapSeconds = 0.6f;
	constexpr int32 MaxSwapPresses = 5;

	/** Sprint is a HOLD: press the rising edge, release the falling one, re-press nothing.
	 *  A leaked hold rides the persistent ASC into the next life (the host's own leak). */
	void SetSprint(IAIBAvatarInterface& Avatar, bool& bHeld, bool bWant)
	{
		if (bWant == bHeld)
		{
			return;
		}
		bHeld = bWant;
		if (bWant)
		{
			Avatar.PressVerb(AIBTags::Verb_Sprint);
		}
		else
		{
			Avatar.ReleaseVerb(AIBTags::Verb_Sprint);
		}
	}

	/** Crouch is a TOGGLE — one tap flips it — so this compares against the avatar's REAL
	 *  state through the door, never a private mirror, and never asks for a crouch while
	 *  falling: mid-air the toggle only ever UNcrouches, so the press would do the exact
	 *  opposite of what the caller wanted. Both rules are the host's, both were bugs first. */
	void SetCrouch(IAIBAvatarInterface& Avatar, bool bWant)
	{
		if (bWant && !Avatar.IsGrounded())
		{
			return;
		}
		if (Avatar.IsCrouched() == bWant)
		{
			return;
		}
		Avatar.PressVerb(AIBTags::Verb_Crouch);
		Avatar.ReleaseVerb(AIBTags::Verb_Crouch);
	}

	/** One call per mover tick: hold sprint while there is ground to cover, and spend one
	 *  jump when a path that reports Moving stops producing any. NAVLINKS ARE NOT THIS
	 *  FUNCTION'S JOB and need no AI work at all — drop and climb traversal is a property
	 *  of the pawn and the navmesh (the host character's bUseAccelerationForPaths plus the
	 *  project's generated nav links), inherited the moment a bot paths. This is only the
	 *  wedge case, which no link covers. */
	void TickLocomotion(AAIBBotController& Bot, FAIBLocomotionState& State,
		const FVector& Goal, float ArriveRadiusUU, float DeltaTime)
	{
		IAIBAvatarInterface* Avatar = Bot.GetAvatar();
		const APawn* Pawn = Bot.GetPawn();
		if (!Avatar || !Pawn)
		{
			return;
		}
		const FVector Here = Pawn->GetActorLocation();
		const float ToGoal = FVector::Dist(Here, Goal);
		SetSprint(*Avatar, State.bSprintHeld, ToGoal > ArriveRadiusUU * SprintBeyondRadiusFactor);

		if (!State.bHasBestPoint || FVector::Dist(Here, State.BestPoint) > WedgeProgressUU)
		{
			State.BestPoint = Here;
			State.bHasBestPoint = true;
			State.StallSeconds = 0.f;
			State.bTriedWedgeJump = false; // moving again: the next wedge gets its own jump
			return;
		}
		if (ToGoal <= ArriveRadiusUU)
		{
			return; // standing AT the goal is station-keeping, not being stuck
		}
		State.StallSeconds += DeltaTime;
		if (!State.bTriedWedgeJump && State.StallSeconds >= WedgeStallSeconds && Avatar->IsGrounded())
		{
			State.bTriedWedgeJump = true;
			Avatar->PressVerb(AIBTags::Verb_Jump);
			Avatar->ReleaseVerb(AIBTags::Verb_Jump);

			// AND RE-ISSUE THE MOVE. The jump alone was half the manoeuvre and the half that
			// does nothing on its own: path following has already gone Idle at the lip the bot
			// stalled on, so the leap lands on the step and the bot then STANDS there against a
			// dead request until the no-progress timer calls it "cannot reach". A body that
			// jumped onto a stair and did not ask for a path again has not climbed anything.
			//
			// The sibling framework has done exactly this since R9.5 ("stopped short": jump,
			// then MoveToActor again) and its bots cross tiers; this one jumped, landed, and
			// gave up. Measured before this line existed: across 90 PIE samples not one pawn
			// was ever seen at an intermediate height on either new stair flight — zero
			// climbs, while pawns sat happily on the decks above and below.
			const EPathFollowingRequestResult::Type Again = MoveToNavPoint(Bot, Goal, ArriveRadiusUU);
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s jumped to clear whatever it is wedged on — move re-issued (%s)."),
				*Bot.GetName(),
				Again == EPathFollowingRequestResult::Failed ? TEXT("REFUSED") : TEXT("accepted"));
		}
	}

	/** Every mover's ExitState. A sprint carried out of a branch is the host's own leak:
	 *  the bot arrives in its firing position still holding the speed state. */
	void ReleaseLocomotion(AAIBBotController& Bot, FAIBLocomotionState& State)
	{
		if (IAIBAvatarInterface* Avatar = Bot.GetAvatar())
		{
			SetSprint(*Avatar, State.bSprintHeld, false);
		}
	}
}

////////////////////////////////////////////////////////////////////

bool FAIBAmbitionGateCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const UAIBAmbitionEngine* Engine = Bot ? Bot->GetAmbitionEngine() : nullptr;
	if (!Engine)
	{
		return false;
	}
	// The IsValid guard stays OUTSIDE Matches: an invalid current want must never match
	// any branch (invalid == invalid is true, and MatchesTag on invalid is undefined
	// comfort) — the Fallback branch is where "no want" lands, by design.
	const FGameplayTag Current = Engine->GetCurrent();
	return Current.IsValid() && Matches(Current);
}

bool FAIBAmbitionGateCondition::Matches(const FGameplayTag& Current) const
{
	// EXACT equality is the default — the 1:1 arbitration mirror.
	const FGameplayTag BranchTag = GetBranchTag();
	return BranchTag.IsValid() && Current == BranchTag;
}

// Each branch's identity is a virtual, not a node parameter — the compiled authoring
// surface sets nothing on the nodes it adds, so per-branch data lives in the TYPE.
FGameplayTag FAIBAmbitionGateCondition::GetBranchTag() const     { return FGameplayTag(); }
FGameplayTag FAIBGateEngageCondition::GetBranchTag() const       { return AIBTags::Ambition_Engage; }
FGameplayTag FAIBGateRetreatCondition::GetBranchTag() const      { return AIBTags::Ambition_Retreat; }
FGameplayTag FAIBGateSearchCondition::GetBranchTag() const       { return AIBTags::Ambition_Search; }
FGameplayTag FAIBGateSeekCondition::GetBranchTag() const   { return AIBTags::Ambition_Seek; }
FGameplayTag FAIBGateRoamCondition::GetBranchTag() const         { return AIBTags::Ambition_Roam; }
FGameplayTag FAIBGateModeCondition::GetBranchTag() const         { return AIBTags::Ambition_Mode; }

bool FAIBGateModeCondition::Matches(const FGameplayTag& Current) const
{
	// A host's mode want is a CHILD tag (AIBot.Ambition.Mode.Hold); exact == can never
	// equal it, which was the statue-beside-the-objective defect (W-AUDIT P6 f.5).
	return Current.MatchesTag(AIBTags::Ambition_Mode);
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBAmbitionSentinelTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const UAIBAmbitionEngine* Engine = Bot ? Bot->GetAmbitionEngine() : nullptr;
	if (!Engine)
	{
		return EStateTreeRunStatus::Failed;
	}
	InstanceData.AmbitionAtEnter = Engine->GetCurrent();
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBAmbitionSentinelTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const UAIBAmbitionEngine* Engine = Bot ? Bot->GetAmbitionEngine() : nullptr;
	if (!Engine)
	{
		return EStateTreeRunStatus::Failed;
	}
	// SUCCEEDED, not failed: the want moved on, nothing broke. Root re-selects the
	// branch the brain now wants — this is the executor mirroring arbitration.
	return Engine->GetCurrent() == InstanceData.AmbitionAtEnter
		? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBFaceBeliefTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	return (Bot && Bot->GetSensorium().HasVisibleTarget())
		? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FAIBFaceBeliefTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn || !Bot->GetSensorium().HasVisibleTarget())
	{
		return EStateTreeRunStatus::Failed;
	}
	// THE BELIEF, never the live actor: during the juke window this is the frozen
	// last-seen spot — a bot aiming through the pillar is the bug this line bans.
	//
	// PHASE 4 INTEGRATION — F4 finally executes: the aim point is the belief displaced
	// by the level's HELD, DECAYING angular error (drawn in the cone, settled over the
	// correct time, redrawn on cadence, fully reset on a target switch). The bot aims
	// where it believes MINUS how good its hands are — never a perfect solution.
	const AActor* Target = Bot->GetSensorium().GetVisibleTarget();
	const FVector AimPoint = FAIBAimPolicy::StepAimPoint(
		Bot->GetAimState(), Pawn->GetPawnViewLocation(),
		Bot->GetSensorium().GetLastSeenLocation(),
		Target ? Target->GetUniqueID() : 0,
		Bot->GetSkillProfile().Level(EAIBSkill::Aim),
		Bot->GetPolicyRandom(), Bot->GetWorld()->GetTimeSeconds());
	SteerControlRotation(*Bot, AimPoint, InstanceData.TurnDegreesPerSecond, DeltaTime);
	return EStateTreeRunStatus::Running;
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBMoveNearBeliefTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot || !Bot->GetSensorium().HasVisibleTarget())
	{
		return EStateTreeRunStatus::Failed;
	}
	InstanceData.LastGoal = Bot->GetSensorium().GetLastSeenLocation();
	InstanceData.RepathCooldown = 0.f;
	// THE ENTRY MIRRORS THE TICK'S YIELD (BN22 W-REVIEW M3): Engage re-enters on every
	// belief blink, and without this gate each re-entry issued a close-to-350 request
	// that ran at full speed until the current strafe leg expired — the beeline the
	// fight-range hand-off exists to kill, surviving at exactly the moments fights
	// blink most. Inside the fight range, footwork owns the legs from the first frame.
	if (IsWithin(*Bot, InstanceData.LastGoal, InstanceData.FightRangeUU))
	{
		return EStateTreeRunStatus::Running;
	}
	// Already in position: station-keep from here (issuing a move to where we stand
	// would complete instantly and thrash the branch — the never-succeed contract).
	if (!IsWithin(*Bot, InstanceData.LastGoal, InstanceData.AcceptanceRadiusUU))
	{
		if (MoveToNavPoint(*Bot, InstanceData.LastGoal, InstanceData.AcceptanceRadiusUU)
			== EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s could not path to the belief — closing refused (F7). %s"), *Bot->GetName(), *DescribeMoveFailure(*Bot, InstanceData.LastGoal));
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.RepathCooldown = InstanceData.RepathIntervalSeconds;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBMoveNearBeliefTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot || !Bot->GetSensorium().HasVisibleTarget())
	{
		return EStateTreeRunStatus::Failed;
	}

	// FOOTWORK OWNS THE FIGHT RANGE (founder, 27 Aug — see the instance data's comment):
	// target visible inside FightRangeUU, this mover stands down and the strafe task has
	// the legs. The sprint hand-off matters: TickLocomotion below is what RELEASES a
	// held sprint, so yielding must release it explicitly or the bot strafes at sprint
	// speed with the key stuck down (ReleaseLocomotion is state-guarded — free per tick).
	const FVector Belief = Bot->GetSensorium().GetLastSeenLocation();
	if (IsWithin(*Bot, Belief, InstanceData.FightRangeUU))
	{
		ReleaseLocomotion(*Bot, InstanceData.Locomotion);
		return EStateTreeRunStatus::Running;
	}

	// Station-keeping: chase the belief's drift, never complete. Firing runs beside
	// this task; the sentinel or a visibility loss is what ends the branch. Out of
	// position for ANY reason — belief drift or the BOT displaced (knockback, a pad) —
	// re-closes on the repath cadence (W-REVIEW P3 M3).
	InstanceData.RepathCooldown -= DeltaTime;
	if (!IsWithin(*Bot, Belief, InstanceData.AcceptanceRadiusUU)
		&& InstanceData.RepathCooldown <= 0.f)
	{
		InstanceData.LastGoal = Belief;
		InstanceData.RepathCooldown = InstanceData.RepathIntervalSeconds;
		if (MoveToNavPoint(*Bot, Belief, InstanceData.AcceptanceRadiusUU)
			== EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s could not path to the belief — closing refused (F7). %s"), *Bot->GetName(), *DescribeMoveFailure(*Bot, Belief));
			return EStateTreeRunStatus::Failed;
		}
	}

	// Close FAST, arrive WALKING. There is no blind case to test here — this task fails
	// without a held belief — so distance is the whole rule.
	TickLocomotion(*Bot, InstanceData.Locomotion, Belief, InstanceData.AcceptanceRadiusUU, DeltaTime);
	return EStateTreeRunStatus::Running;
}

void FAIBMoveNearBeliefTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		ReleaseLocomotion(*Bot, InstanceData.Locomotion);
		Bot->StopMovement();
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBFireWhenAbleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bHolding = false;
	InstanceData.PhaseSecondsLeft = 0.f;
	// A fresh engagement gets a fresh swap budget: the cap exists to stop an endless spin
	// inside ONE fight, not to make a bot that gave up once give up for the match.
	InstanceData.SwapPresses = 0;
	InstanceData.SwapCooldownLeft = 0.f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBFireWhenAbleTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	IAIBAvatarInterface* Avatar = Bot ? Bot->GetAvatar() : nullptr;
	if (!Bot || !Avatar)
	{
		// The avatar door closed mid-hold (the adapter died): the trigger cannot be
		// released through a door that no longer exists — say so at Warning, because a
		// silently stranded held verb is undiagnosable (W-REVIEW P3). The controller's
		// unpossess/EndPlay release is the belt on the host side.
		if (InstanceData.bHolding)
		{
			UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s lost its avatar door while holding fire — release could not be delivered."),
				Bot ? *Bot->GetName() : TEXT("<no controller>"));
			InstanceData.bHolding = false;
		}
		return EStateTreeRunStatus::Failed;
	}

	// A CORPSE PRESSES NOTHING. The controller holds the body until respawn (the host's
	// own lifecycle), and every press below this line was gated on facts — ammo, range,
	// visibility — none of which say "alive". A dead bot with a low magazine re-pressed
	// reload every retry window into an ASC whose weapon grants die with the body: the
	// leading suspect for BN20's 308 no-grant warnings, and wrong on its own terms
	// regardless (the fire gate's own State_Dead read shows the host already refuses
	// dead verbs — this stops asking). Held trigger released first, or the corpse keeps
	// firing until the actor is destroyed.
	if (!Avatar->IsAlive())
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		if (InstanceData.bAimHeld)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
		}
		return EStateTreeRunStatus::Running;
	}

	// The cached facts are the one info door: matured visibility + the assembled
	// can-fight answer, never raw avatar reads scattered through tasks.
	const FAIBFacts& Facts = Bot->GetLastFacts();

	// RELOAD, AND CROUCH WHILE THE HANDS ARE BUSY. The most dangerous seconds a bot has
	// are the ones it cannot shoot back in, and a reload is the only moment it chooses to
	// spend them — so it spends them small, and legibly: a crouched bot with its hands
	// busy reads as "reloading" from across the arena. No reserve is a different problem
	// (the weapon swap), and it is NOT handled here — see the ticket Log for why.
	// Every countdown throttle decays here, ABOVE the reload's early return: a cooldown
	// that froze while the bot reloaded would fire the instant the magazine landed. (The
	// melee throttle is an ABSOLUTE deadline on controller state now — nothing to decay.)
	InstanceData.ReloadCooldownLeft = FMath::Max(0.f, InstanceData.ReloadCooldownLeft - DeltaTime);
	InstanceData.SwapCooldownLeft = FMath::Max(0.f, InstanceData.SwapCooldownLeft - DeltaTime);
	InstanceData.ReAimCooldownLeft = FMath::Max(0.f, InstanceData.ReAimCooldownLeft - DeltaTime);
	if (Facts.bHasReserveAmmo && Facts.AmmoNorm <= ReloadAtMagazineFraction)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		// Hands busy = sights down: a human drops out of ADS to reload, and holding the
		// aim through the magazine change is a speed penalty bought for nothing.
		if (InstanceData.bAimHeld)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
		}
		SetCrouch(*Avatar, true);
		InstanceData.bCrouchedToReload = true;
		if (InstanceData.ReloadCooldownLeft <= 0.f)
		{
			// One tap, like the human's R: the press activates, the release clears the
			// held flag so the next reload is a fresh press rather than an input the
			// ability system still believes is down.
			Avatar->PressVerb(AIBTags::Verb_Reload);
			Avatar->ReleaseVerb(AIBTags::Verb_Reload);
			InstanceData.ReloadCooldownLeft = ReloadRetrySeconds;
		}
		return EStateTreeRunStatus::Running;
	}
	if (InstanceData.bCrouchedToReload)
	{
		SetCrouch(*Avatar, false);
		InstanceData.bCrouchedToReload = false;
	}

	// THIS FRAME'S distance to the belief — the reason the three checks below can exist at
	// all. Everything past here that names a range uses it; nothing past here reads the
	// think-rate DistToTargetUU, which is a want's number, not a swing's. (One deliberate
	// exception below: the grenade CALL reads the fact snapshot, because a throw is a
	// decision, not a swing — the policy's one-info-door law.)
	float DistanceUU = 0.f;
	const bool bHasDistance = LiveDistanceToBelief(*Bot, DistanceUU);
	const double Now = Bot->GetWorld()->GetTimeSeconds();

	// -- MELEE: they are close enough to touch, AND the bot has READ that ---------------
	// Two gates, two owners. The REACH is the held weapon's, through the door — a knife
	// commits from further than a rifle butt, and this code never learns which is which.
	// The RECOGNITION is the level's (Phase 4): the policy's clock starts when the range
	// picture becomes continuously true and answers after the level's delay — an Expert
	// reads the closing fight early and is already swinging on arrival, a Novice only
	// realises at point-blank and takes a beat more (the R11 reasoning, applied to the
	// knife). Stepped EVERY tick so the continuous-range reset law holds.
	const bool bMeleeRecognised = FAIBMeleePolicy::ShouldMelee(
		Bot->GetMeleeState(), bHasDistance ? DistanceUU : -1.f, Facts.bTargetVisible,
		Bot->GetSkillProfile().Level(EAIBSkill::Melee), Now);
	const float MeleeRangeUU = Avatar->GetMeleeRangeUU();
	if (bMeleeRecognised && bHasDistance && MeleeRangeUU > 0.f
		&& Now >= Bot->GetMeleeState().NextSwingAtSeconds
		&& DistanceUU <= MeleeRangeUU * MeleeCommitFraction)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		if (InstanceData.bAimHeld) // nobody knifes through a scope
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
		}
		Avatar->PressVerb(AIBTags::Verb_Melee);
		Avatar->ReleaseVerb(AIBTags::Verb_Melee);
		// Controller-owned absolute deadline — a belief blink re-entering this state must
		// find the throttle still standing (W-REVIEW P4+5 H2).
		Bot->GetMeleeState().NextSwingAtSeconds = Now + MeleeRetrySeconds;
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s swung at %.0fuu (reach %.0f)."),
			*Bot->GetName(), DistanceUU, MeleeRangeUU);
		return EStateTreeRunStatus::Running;
	}

	// -- SWAP: hold the right thing for this range ------------------------------------
	// The avatar answers; this presses. Nothing here knows what a weapon is, what one is
	// worth, or that the host's carry contains a slot holding nothing — pressing until
	// the answer is yes walks past that slot the same way a mouse wheel does, which is
	// why the host's equipment code needed no change to make this work.
	if (bHasDistance && !Avatar->IsBestWeaponForRange(DistanceUU)
		&& InstanceData.SwapPresses < MaxSwapPresses)
	{
		if (InstanceData.SwapCooldownLeft <= 0.f)
		{
			if (InstanceData.bHolding)
			{
				Avatar->ReleaseVerb(AIBTags::Verb_Fire);
				InstanceData.bHolding = false;
				InstanceData.PhaseSecondsLeft = 0.f;
			}
			Avatar->PressVerb(AIBTags::Verb_WeaponNext);
			Avatar->ReleaseVerb(AIBTags::Verb_WeaponNext);
			++InstanceData.SwapPresses;
			InstanceData.SwapCooldownLeft = SwapSeconds;
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s cycling weapons for %.0fuu (press %d/%d)."),
				*Bot->GetName(), DistanceUU, InstanceData.SwapPresses, MaxSwapPresses);
		}
		// Do NOT fire mid-cycle: the hand may be empty or holding the wrong answer, and a
		// burst pressed into an equip montage is a burst that never leaves the barrel.
		return EStateTreeRunStatus::Running;
	}
	if (bHasDistance && InstanceData.SwapPresses > 0 && Avatar->IsBestWeaponForRange(DistanceUU))
	{
		InstanceData.SwapPresses = 0; // settled: the next range change gets a full budget
	}

	// -- GRENADE: the level's RECOGNITION, the pouch, the cooldown ---------------------
	// PHASE 4 INTEGRATION: the fixed band became the policy's per-level recognition
	// ladder — a Novice never throws deliberately, Trained sees the opener, Skilled the
	// finisher (on damage-DEALT history, never enemy vitals — F3), Expert the area
	// denial. The CALL reads the fact snapshot (a throw is a decision; one info door)
	// on the level's consider cadence; the throw still rides the CONTROLLER's cooldown,
	// because StateTree re-initialises instance data on every state ENTRY and Engage
	// re-enters whenever a belief blinks — a per-task countdown would reset roughly
	// once a second and throttle nothing at all (see AAIBBotController::CanThrowGrenade).
	const EAIBGrenadeCall GrenadeCall = FAIBGrenadePolicy::Consider(
		Bot->GetGrenadeState(), Facts,
		Bot->GetSkillProfile().Level(EAIBSkill::Grenade),
		Bot->GetPolicyRandom(), Now);
	if (GrenadeCall != EAIBGrenadeCall::None && !Bot->CanThrowGrenade())
	{
		// The recognition fired and the throttle ate it — across one 8s cooldown an
		// Expert burns several of these, and a silent burn is undiagnosable (F7,
		// W-REVIEW P4+5 M5). Verbose: it is cadence, not an error.
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s recognised a grenade moment (call %d) inside the throw cooldown."),
			*Bot->GetName(), static_cast<int32>(GrenadeCall));
	}
	if (GrenadeCall != EAIBGrenadeCall::None && Bot->CanThrowGrenade())
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		if (InstanceData.bAimHeld) // the throw drops the sights, like the human's
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
		}
		Avatar->PressVerb(AIBTags::Verb_Grenade);
		Avatar->ReleaseVerb(AIBTags::Verb_Grenade);
		Bot->NoteGrenadeThrown(GrenadeCooldownSeconds);
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s threw (call %d) at %.0fuu of belief (%d left in the pouch)."),
			*Bot->GetName(), static_cast<int32>(GrenadeCall), Facts.DistToTargetUU, Facts.GrenadeCount);
		return EStateTreeRunStatus::Running;
	}

	// The live sensorium check closes the destroyed-target frame: a corpse whose weak
	// handle just went null must not be burst at for the rest of the fact snapshot's
	// life (W-REVIEW P3 M1 — the facts refresh at think cadence, this task at tick).
	const bool bMayFire = Facts.bTargetVisible && Facts.bWeaponCanFight
		&& Bot->GetSensorium().HasVisibleTarget();

	if (!bMayFire)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		if (InstanceData.bAimHeld) // nothing to aim AT — sights down with the trigger
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
		}
		return EStateTreeRunStatus::Running; // stay: visibility may return next pump
	}

	// -- ADS: the sights come up for the mid-range fight (founder, 27 Aug) --------------
	// Aim skill gates it (a Novice hip-fires — the ladder's shape everywhere else);
	// the band is [AimRangeUU, AimMaxRangeUU] with hysteresis so the boundary never
	// flaps; the HOST owns everything aiming means (spread, the speed penalty, and the
	// DESCOPE — a landed hit cancels the aim behind this task's back, which is why the
	// read is Avatar->IsAiming() and never our own flag, and why the re-press waits
	// ReAimSeconds: the descope must be FELT, not instantly erased). The press is
	// release-then-press on purpose — after a descope the old press is still logically
	// down, and the ability activates on the press EDGE.
	if (bHasDistance)
	{
		constexpr float AimHysteresisUU = 80.f;
		const bool bSkillAims = Bot->GetSkillProfile().Level(EAIBSkill::Aim) >= EAIBCompetence::Trained;
		const float InEdge = InstanceData.bAimHeld ? InstanceData.AimRangeUU - AimHysteresisUU : InstanceData.AimRangeUU;
		const float OutEdge = InstanceData.bAimHeld ? InstanceData.AimMaxRangeUU + AimHysteresisUU : InstanceData.AimMaxRangeUU;
		const bool bWantAim = bSkillAims && DistanceUU >= InEdge && DistanceUU <= OutEdge;
		if (bWantAim && !Avatar->IsAiming() && InstanceData.ReAimCooldownLeft <= 0.f)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			Avatar->PressVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = true;
			InstanceData.ReAimCooldownLeft = InstanceData.ReAimSeconds;
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s aimed in at %.0fuu."), *Bot->GetName(), DistanceUU);
		}
		else if (!bWantAim && InstanceData.bAimHeld)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s let the aim go at %.0fuu."), *Bot->GetName(), DistanceUU);
		}
	}

	// The muzzle gate's measured quantity: degrees between where the control rotation
	// points NOW and the line to the belief. FaceBelief is turning the rotation at its
	// rate limit; this only asks whether it has arrived.
	const APawn* FiringPawn = Bot->GetPawn();
	const FVector ToAimLine = Bot->GetSensorium().GetLastSeenLocation()
		- (FiringPawn ? FiringPawn->GetPawnViewLocation() : FVector::ZeroVector);
	float MuzzleOffDegrees = 0.f;
	if (ToAimLine.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		const float AlignDot = FMath::Clamp(FVector::DotProduct(
			Bot->GetControlRotation().Vector(), ToAimLine.GetSafeNormal()), -1.f, 1.f);
		MuzzleOffDegrees = FMath::RadiansToDegrees(FMath::Acos(AlignDot));
	}

	// A burst in progress breaks on a large swing — the trigger must not ride a turn.
	if (InstanceData.bHolding && MuzzleOffDegrees > BurstBreakAlignDegrees)
	{
		Avatar->ReleaseVerb(AIBTags::Verb_Fire);
		InstanceData.bHolding = false;
		InstanceData.PhaseSecondsLeft = InstanceData.BetweenBurstsSeconds;
		return EStateTreeRunStatus::Running;
	}

	InstanceData.PhaseSecondsLeft -= DeltaTime;
	if (InstanceData.PhaseSecondsLeft <= 0.f)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = InstanceData.BetweenBurstsSeconds;
		}
		else if (MuzzleOffDegrees <= BurstStartAlignDegrees)
		{
			// The muzzle has arrived: this burst starts on the aim line, never across it.
			Avatar->PressVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = true;
			InstanceData.PhaseSecondsLeft = InstanceData.BurstSeconds;
		}
		// else: hold the trigger up this tick — the turn is still in flight, and the
		// phase clock stays expired so the burst begins the tick alignment lands.
	}
	return EStateTreeRunStatus::Running;
}

void FAIBFireWhenAbleTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// ALWAYS release: a held verb on the host's persistent verb sink outlives the body.
	// A skipped release is LOUD (W-REVIEW P3) — clearing bHolding must never silently
	// erase the record that the trigger is still down somewhere.
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (IAIBAvatarInterface* Avatar = Bot ? Bot->GetAvatar() : nullptr)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
		}
		if (InstanceData.bAimHeld)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
		}
		if (InstanceData.bCrouchedToReload)
		{
			// Stand back up on the way out, or the bot carries the reload's crouch — and
			// its speed penalty — into whatever it wanted instead.
			SetCrouch(*Avatar, false);
		}
	}
	else if (InstanceData.bHolding || InstanceData.bCrouchedToReload)
	{
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s exited a fire state holding a verb with no avatar door — release not delivered."),
			Bot ? *Bot->GetName() : TEXT("<no controller>"));
	}
	InstanceData.bHolding = false;
	InstanceData.bCrouchedToReload = false;

	// Leaving the fight breaks the melee CONTINUITY clock: ShouldMelee's "range stayed
	// true" reset only runs on ticks that happen, so a branch gap would otherwise splice
	// two separate approaches into one paid delay (W-REVIEW P4+5 H2's second half). The
	// swing deadline above deliberately survives — that one is the throttle.
	if (Bot)
	{
		Bot->GetMeleeState().InRangeSinceSeconds = -1.0;
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBFleeFromBeliefTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Away from the freshest threat knowledge we hold: the visible belief, else memory.
	// With NOTHING held — hurt, threat unknown — Retreat still needs an executable exit
	// (W-REVIEW P3 H1): reposition to a random reachable point. A hurt bot that
	// relocates reads as falling back; a hurt bot frozen mid-arena reads as broken, and
	// the hysteresis defends the freeze.
	FVector ThreatPoint;
	bool bHasThreatPoint = Bot->GetSensorium().HasVisibleTarget();
	if (bHasThreatPoint)
	{
		ThreatPoint = Bot->GetSensorium().GetLastSeenLocation();
	}
	else
	{
		const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
		bHasThreatPoint = Bot->GetSensorium().Memory().GetFresh(
			Bot->GetWorld()->GetTimeSeconds(), Window > 0.f ? Window : AIB::DefaultMemoryFreshSeconds, ThreatPoint);
	}

	FVector Goal;
	if (bHasThreatPoint)
	{
		const FVector Away = (Pawn->GetActorLocation() - ThreatPoint).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			// Standing exactly ON the threat point: direction is meaningless — fall
			// through to the reposition draw rather than failing silently forever.
			bHasThreatPoint = false;
		}
		else
		{
			const FVector Directed = Pawn->GetActorLocation() + Away * InstanceData.FleeDistanceUU;
			// "Straight away from the threat" is a DIRECTION, not a promise that anything is
			// standable there — off a ledge, through a wall, past the arena edge. When the
			// directed point will not project, take the SAME exit the zero-direction case
			// takes rather than handing the mover a goal it must refuse: a reachable point
			// somewhere else still breaks contact, and refusing does not move the bot at all.
			// Measured: unprojected, this refused 267 times per Retreat ambition.
			if (!ProjectToNav(Bot->GetWorld(), Directed, Goal))
			{
				bHasThreatPoint = false;
			}
		}
	}
	if (!bHasThreatPoint)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Bot->GetWorld());
		FNavLocation Reposition;
		if (!NavSys || !NavSys->GetRandomReachablePointInRadius(
			Pawn->GetActorLocation(), InstanceData.FleeDistanceUU, Reposition))
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s wants Retreat with no threat point and no reachable reposition — standing (F7)."), *Bot->GetName());
			return EStateTreeRunStatus::Failed;
		}
		Goal = Reposition.Location;
	}

	InstanceData.FleeGoal = Goal;
	InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), Goal);
	InstanceData.SecondsWithoutProgress = 0.f;
	if (MoveToNavPoint(*Bot, InstanceData.FleeGoal, 150.f) == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s flee path REFUSED — failing loudly, not standing (F7). %s"), *Bot->GetName(), *DescribeMoveFailure(*Bot, InstanceData.FleeGoal));
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBFleeFromBeliefTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (IsWithin(*Bot, InstanceData.FleeGoal, 200.f))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// A partial path that stalled short of the goal must not read as "fleeing" forever
	// (W-REVIEW P3 H3): no closer approach for the window = the path is dead, say so.
	const float DistNow = FVector::Dist(Pawn->GetActorLocation(), InstanceData.FleeGoal);
	if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
	{
		InstanceData.ClosestSoFarUU = DistNow;
		InstanceData.SecondsWithoutProgress = 0.f;
	}
	else if ((InstanceData.SecondsWithoutProgress += DeltaTime) >= InstanceData.GiveUpAfterNoProgressSeconds)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s flee stalled %.1fs short of its goal — giving up loudly (F7)."),
			*Bot->GetName(), InstanceData.GiveUpAfterNoProgressSeconds);
		return EStateTreeRunStatus::Failed;
	}
	// A bot that WALKS away is not fleeing. Same helper as every other mover, so the
	// hold is released on the way out and a wedge still costs one jump, not the match.
	TickLocomotion(*Bot, InstanceData.Locomotion, InstanceData.FleeGoal, 200.f, DeltaTime);
	return EStateTreeRunStatus::Running;
}

void FAIBFleeFromBeliefTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		ReleaseLocomotion(*Bot, InstanceData.Locomotion);
		Bot->StopMovement();
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBMoveToLastKnownTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
	FVector LastKnown;
	if (!Bot->GetSensorium().Memory().GetFresh(
		Bot->GetWorld()->GetTimeSeconds(), Window > 0.f ? Window : AIB::DefaultMemoryFreshSeconds, LastKnown))
	{
		return EStateTreeRunStatus::Failed; // stale: Root re-selects
	}
	InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), LastKnown);
	InstanceData.SecondsWithoutProgress = 0.f;
	if (MoveToNavPoint(*Bot, LastKnown, InstanceData.AcceptanceRadiusUU)
		== EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s cannot path to the last-known spot — search fails loudly (F7). %s"), *Bot->GetName(), *DescribeMoveFailure(*Bot, LastKnown));
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBMoveToLastKnownTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}
	// Seeing someone ends searching — arbitration will already be swinging to Engage.
	if (Bot->GetSensorium().HasVisibleTarget())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
	FVector LastKnown;
	if (!Bot->GetSensorium().Memory().GetFresh(
		Bot->GetWorld()->GetTimeSeconds(), Window > 0.f ? Window : AIB::DefaultMemoryFreshSeconds, LastKnown))
	{
		return EStateTreeRunStatus::Failed;
	}
	// Crossing ground toward a place someone WAS is the cheapest sprint a bot ever
	// takes: nothing to lose sight of and no burst to interrupt. It drops to a walk on
	// arrival, which is also when SweepLook's hunt starts mattering.
	TickLocomotion(*Bot, InstanceData.Locomotion, LastKnown, InstanceData.AcceptanceRadiusUU, DeltaTime);

	// Arrived: STAND at the post and let SweepLook hunt. The branch ends when the
	// memory stales (above), someone appears (Succeeded), or the want moves on. Short
	// of the post, no-progress means the spot is unreachable (a catwalk memory, a nav
	// hole): fail LOUDLY instead of "searching" motionless for the memory window (H3).
	const APawn* Pawn = Bot->GetPawn();
	if (Pawn && !IsWithin(*Bot, LastKnown, InstanceData.AcceptanceRadiusUU))
	{
		const float DistNow = FVector::Dist(Pawn->GetActorLocation(), LastKnown);
		if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
		{
			InstanceData.ClosestSoFarUU = DistNow;
			InstanceData.SecondsWithoutProgress = 0.f;
		}
		else if ((InstanceData.SecondsWithoutProgress += DeltaTime) >= InstanceData.GiveUpAfterNoProgressSeconds)
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s cannot reach the last-known spot — giving up the search loudly (F7)."), *Bot->GetName());
			return EStateTreeRunStatus::Failed;
		}
	}
	return EStateTreeRunStatus::Running;
}

void FAIBMoveToLastKnownTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		ReleaseLocomotion(*Bot, InstanceData.Locomotion);
		Bot->StopMovement();
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBSweepLookTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBSweepLookTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	// AREA DENIAL'S CALLER (the P4+5 review's dormant-Expert finding, closed): the
	// searching look is exactly where denial lives — target NOT visible, memory fresh —
	// and this task already owns the control rotation here, which is why the consult
	// is folded in rather than shipped as a new node (FireWhenAble's own pinned-node
	// rationale). The policy decides on its cadence through the one info door; this
	// only faces the remembered spot and presses. The throw must not ride the sweep's
	// arbitrary heading — the reviewers' explicit condition — so the press waits for
	// alignment, at the burst gate's own threshold.
	if (IAIBAvatarInterface* Avatar = Bot->GetAvatar())
	{
		const double Now = Bot->GetWorld()->GetTimeSeconds();
		const EAIBGrenadeCall DenialCall = FAIBGrenadePolicy::Consider(
			Bot->GetGrenadeState(), Bot->GetLastFacts(),
			Bot->GetSkillProfile().Level(EAIBSkill::Grenade),
			Bot->GetPolicyRandom(), Now);
		if (DenialCall == EAIBGrenadeCall::AreaDenial && Bot->CanThrowGrenade())
		{
			const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
			FVector Remembered;
			if (Bot->GetSensorium().Memory().GetFresh(Now,
				Window > 0.f ? Window : AIB::MaxMemorySeconds, Remembered))
			{
				SteerControlRotation(*Bot, Remembered, 360.f, DeltaTime);
				const FVector ToSpot = Remembered - Pawn->GetPawnViewLocation();
				if (ToSpot.SizeSquared() > KINDA_SMALL_NUMBER)
				{
					const float OffDot = FMath::Clamp(FVector::DotProduct(
						Bot->GetControlRotation().Vector(), ToSpot.GetSafeNormal()), -1.f, 1.f);
					if (FMath::RadiansToDegrees(FMath::Acos(OffDot)) <= BurstStartAlignDegrees)
					{
						Avatar->PressVerb(AIBTags::Verb_Grenade);
						Avatar->ReleaseVerb(AIBTags::Verb_Grenade);
						Bot->NoteGrenadeThrown(GrenadeCooldownSeconds);
						UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s denied the remembered spot with a grenade."),
							*Bot->GetName());
					}
				}
				// Denial owns the look until it resolves — the sweep resumes next
				// tick the call goes quiet (thrown, throttled, or memory faded).
				return EStateTreeRunStatus::Running;
			}
		}
	}

	FRotator Swept = Bot->GetControlRotation();
	Swept.Yaw += InstanceData.SweepDegreesPerSecond * DeltaTime;
	Swept.Normalize();
	Bot->SetControlRotation(Swept);
	Pawn->FaceRotation(Swept, DeltaTime);
	return EStateTreeRunStatus::Running;
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBMoveToPOITask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bHasGoal = false;

	// SOMEWHERE TO BE, in preference order. The belief comes from the memory's fresh
	// window — F5-lawful, never a live actor read — so "move toward the current target"
	// means the last place this bot honestly knew of, not where the target is now.
	if (ShouldMoveToBeliefFirst())
	{
		const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
		FVector Believed;
		if (Bot->GetSensorium().Memory().GetFresh(Bot->GetWorld()->GetTimeSeconds(),
			Window > 0.f ? Window : AIB::MaxMemorySeconds, Believed))
		{
			InstanceData.Goal = Believed;
			InstanceData.bHasGoal = true;
		}
	}

	// A world-query provider is Phase 6's registration, and its POIs land here between
	// the belief and the wander. Until one exists, a mover that may wander does so and
	// every other FAILS loudly (F7) rather than improvising.
	if (!InstanceData.bHasGoal && ShouldWanderWithoutProvider())
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Bot->GetWorld());
		FNavLocation Wander;
		if (NavSys && NavSys->GetRandomReachablePointInRadius(
			Pawn->GetActorLocation(), InstanceData.WanderRadiusUU, Wander))
		{
			InstanceData.Goal = Wander.Location;
			InstanceData.bHasGoal = true;
		}
	}

	if (!InstanceData.bHasGoal)
	{
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s has no POI provider for kind %s — branch fails."),
			*Bot->GetName(), *GetPOIKind().ToString());
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
	InstanceData.SecondsWithoutProgress = 0.f;
	if (MoveToNavPoint(*Bot, InstanceData.Goal, InstanceData.AcceptanceRadiusUU)
		== EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s POI path refused — branch fails (F7). %s"), *Bot->GetName(), *DescribeMoveFailure(*Bot, InstanceData.Goal));
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBMoveToPOITask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn || !InstanceData.bHasGoal)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (IsWithin(*Bot, InstanceData.Goal, InstanceData.AcceptanceRadiusUU))
	{
		return EStateTreeRunStatus::Succeeded;
	}
	// An aborted or stalled walk must not read as a healthy roam forever (W-REVIEW P3
	// H3) — the wedge jump above gets its chance first (1.5s) and this is the give-up.
	const float DistNow = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
	if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
	{
		InstanceData.ClosestSoFarUU = DistNow;
		InstanceData.SecondsWithoutProgress = 0.f;
	}
	else if ((InstanceData.SecondsWithoutProgress += DeltaTime) >= InstanceData.GiveUpAfterNoProgressSeconds)
	{
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s POI walk stalled — giving up (F7)."), *Bot->GetName());
		return EStateTreeRunStatus::Failed;
	}
	// Crossing the arena with nothing to fight is the one time speed costs a bot
	// nothing — and it is what stops a roaming bot reading as a patrolling tourist.
	TickLocomotion(*Bot, InstanceData.Locomotion, InstanceData.Goal,
		InstanceData.AcceptanceRadiusUU, DeltaTime);
	return EStateTreeRunStatus::Running;
}

void FAIBMoveToPOITask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		ReleaseLocomotion(*Bot, InstanceData.Locomotion);
		Bot->StopMovement();
	}
}

FGameplayTag FAIBMoveToPOITask::GetPOIKind() const           { return FGameplayTag(); }
bool FAIBMoveToPOITask::ShouldWanderWithoutProvider() const  { return false; }

////////////////////////////////////////////////////////////////////

namespace
{
	/** The objective pick, callable from Tick as well as EnterState (BN22 W-REVIEW H1:
	 *  a goal snapshotted once was written for a hill that never moves, and Rally POIs
	 *  are PAWNS — a bot "arrived" at a teammate's abandoned spot stood there forever
	 *  while the live urgency kept the want winning). Worth ties break NEAREST (L2:
	 *  uniform-Worth ally POIs picked in iterator order sent a bot across the map past
	 *  a 700uu teammate). Returns false when no POI of the kind survives the filters. */
	bool PickObjectiveGoal(AAIBBotController& Bot, const APawn& Pawn,
		FAIBMoveToObjectiveTaskInstanceData& InstanceData)
	{
		InstanceData.bHasGoal = false;
		const FGameplayTag Kind = Bot.GetObjectiveKindForCurrentAmbition();
		IAIBWorldQuery* Query = Bot.GetWorldQuery();
		if (!Query)
		{
			return false;
		}

		// Phase 7: the scoring seam has a task-side mirror — a claim honoured at the
		// want but not at the pick is a bot that walks to a claimed slot anyway
		// (multi-slot kinds keep the want alive while one slot is spoken for). Self
		// passes through IsClaimedByOtherTeammate by definition; the gate matches the
		// builder's (a Novice's board is nobody's business here either — the
		// coordinator read below is what the builder's Teamwork gate already allowed
		// or refused at scoring time, mirrored on the same profile read).
		const UAIBTeamCoordinator* Coordinator =
			Bot.GetSkillProfile().Level(EAIBSkill::Teamwork) >= EAIBCompetence::Trained
				? (Bot.GetWorld() ? Bot.GetWorld()->GetSubsystem<UAIBTeamCoordinator>() : nullptr)
				: nullptr;

		TArray<FAIBPointOfInterest> Points;
		Query->QueryPointsOfInterest(&Pawn, AIB::ObjectiveQueryRadiusUU, Points);
		float BestWorth = -1.f;
		float BestDistSq = TNumericLimits<float>::Max();
		for (const FAIBPointOfInterest& Point : Points)
		{
			if (Kind.IsValid() && Point.Kind != Kind)
			{
				continue;
			}
			if (Coordinator && Point.bClaimableSlot
				&& Coordinator->IsClaimedByOtherTeammate(Bot, Point))
			{
				continue;
			}
			const float DistSq = static_cast<float>(FVector::DistSquared(Pawn.GetActorLocation(), Point.Location));
			if (Point.Worth > BestWorth || (Point.Worth == BestWorth && DistSq < BestDistSq))
			{
				BestWorth = Point.Worth;
				BestDistSq = DistSq;
				InstanceData.Goal = Point.Location;
				InstanceData.bHasGoal = true;
				// Never SHRINK the arrival test below the mover's own floor — a provider
				// publishing 0 (a point objective) must not make arrival impossible.
				InstanceData.GoalReachUU = FMath::Max(InstanceData.AcceptanceRadiusUU, Point.ReachRadiusUU);
			}
		}
		return InstanceData.bHasGoal;
	}
}

EStateTreeRunStatus FAIBMoveToObjectiveTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	// The kind join: the CURRENT ambition's ObjectiveKind from the controller's cached
	// mode set (data, never a serialized node parameter), then the best matching POI
	// the world query offers. A servable want with no POI is the provider
	// under-delivering — loud (F7), and the 1s failed-delay keeps it cheap.
	PickObjectiveGoal(*Bot, *Pawn, InstanceData);
	InstanceData.RepollCooldown = InstanceData.RepollIntervalSeconds;

	if (!InstanceData.bHasGoal)
	{
		const FGameplayTag Kind = Bot->GetObjectiveKindForCurrentAmbition();
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s won a mode want but %s offered no POI of kind %s — branch fails (F7)."),
			*Bot->GetName(), Bot->GetWorldQuery() ? TEXT("the world query") : TEXT("NO world query is registered and it"),
			Kind.IsValid() ? *Kind.ToString() : TEXT("<any>"));
		// The deadlock of record: with the hill registered 2ms after possession, seven
		// bots won this want, landed here, and never made another decision for four
		// minutes. Reporting it is what lets the want go quiet and the bot get on with
		// the match.
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
	InstanceData.SecondsWithoutProgress = 0.f;
	if (!IsWithin(*Bot, InstanceData.Goal, InstanceData.GoalReachUU)
		&& MoveToNavPoint(*Bot, InstanceData.Goal, InstanceData.GoalReachUU)
			== EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s cannot path to the objective — failing loudly (F7). %s"),
			*Bot->GetName(), *DescribeMoveFailure(*Bot, InstanceData.Goal));
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBMoveToObjectiveTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn || !InstanceData.bHasGoal)
	{
		return EStateTreeRunStatus::Failed;
	}

	// THE GOAL FOLLOWS THE POI (BN22 W-REVIEW H1). The snapshot was correct for a hill;
	// a Rally POI is a pawn, and standing at a teammate's ABANDONED spot while the live
	// urgency keeps the want winning was a 10-60s statue — the isolation collapse this
	// want exists to fix, re-created as standing. Re-pick on the repath cadence: the
	// hill re-picks itself (byte-identical goal, nothing resets), a moved ally re-aims
	// the walk. Progress tracking resets ONLY when the goal actually moved — a re-poll
	// that reset it every time would neuter the no-progress give-up law. A pick that
	// comes back empty means the provider stopped offering (the last ally died): fail
	// loudly, the F7 shape, and suppression rests the want.
	InstanceData.RepollCooldown -= DeltaTime;
	if (InstanceData.RepollCooldown <= 0.f)
	{
		InstanceData.RepollCooldown = InstanceData.RepollIntervalSeconds;
		const FVector OldGoal = InstanceData.Goal;
		if (!PickObjectiveGoal(*Bot, *Pawn, InstanceData))
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s's objective POI is gone — the branch fails with it (F7)."),
				*Bot->GetName());
			Bot->NoteCurrentAmbitionFailed();
			return EStateTreeRunStatus::Failed;
		}
		if (FVector::DistSquared(OldGoal, InstanceData.Goal) > FMath::Square(50.f))
		{
			InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
			InstanceData.SecondsWithoutProgress = 0.f;
			if (!IsWithin(*Bot, InstanceData.Goal, InstanceData.GoalReachUU))
			{
				MoveToNavPoint(*Bot, InstanceData.Goal, InstanceData.GoalReachUU);
			}
		}
	}

	// ON the objective: STAND — a hill is held by being there. SweepLook rides beside;
	// the sentinel or the want's own decay ends the branch, never arrival.
	if (IsWithin(*Bot, InstanceData.Goal, InstanceData.GoalReachUU))
	{
		return EStateTreeRunStatus::Running;
	}

	// Short of it: sprint the crossing, wedge-jump if stuck, give up LOUDLY if the
	// post is unreachable (the movers' shared no-progress law).
	TickLocomotion(*Bot, InstanceData.Locomotion, InstanceData.Goal, InstanceData.GoalReachUU, DeltaTime);
	const float DistNow = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
	if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
	{
		InstanceData.ClosestSoFarUU = DistNow;
		InstanceData.SecondsWithoutProgress = 0.f;
	}
	else if ((InstanceData.SecondsWithoutProgress += DeltaTime) >= InstanceData.GiveUpAfterNoProgressSeconds)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s cannot reach the objective — giving up loudly (F7). %s"),
			*Bot->GetName(), *DescribeMoveFailure(*Bot, InstanceData.Goal));
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

void FAIBMoveToObjectiveTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		ReleaseLocomotion(*Bot, InstanceData.Locomotion);
		Bot->StopMovement();
	}
}

////////////////////////////////////////////////////////////////////

namespace
{
	/** AIB10's spell instrument, closing half: one line with the DURATION when a gate-hold
	 *  spell ends, whatever ends it — re-entering the circle or losing the target. The
	 *  harness sums the seconds; the reason says which door closed the spell. */
	void EndStrafeGateSpell(AAIBBotController& Bot, const TCHAR* Reason)
	{
		FAIBMovementState& MovementState = Bot.GetMovementState();
		if (!MovementState.bStrafeOutsideGate)
		{
			return;
		}
		MovementState.bStrafeOutsideGate = false;
		const double OutsideSeconds =
			Bot.GetWorld()->GetTimeSeconds() - MovementState.StrafeOutsideSinceSeconds;
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s strafe opportunity back — %.1fs outside (%s)."),
			*Bot.GetName(), OutsideSeconds, Reason);
	}
}

EStateTreeRunStatus FAIBStrafeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBStrafeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn || !Bot->GetSensorium().HasVisibleTarget())
	{
		// Not this task's failure — the belief tasks beside it own the state's fate.
		// Keep running and simply stop stepping (the host's own strafe rule). A gate-hold
		// spell in flight ends here: "denied opportunity" only means anything while there
		// is a visible target to strafe against.
		if (Bot)
		{
			EndStrafeGateSpell(*Bot, TEXT("target lost"));
		}
		return EStateTreeRunStatus::Running;
	}

	// The whole visible fight range is footwork's (founder, 27 Aug): outside FightRangeUU
	// the mover owns the legs and this task holds. The log wording below is FROZEN — the
	// harness's strafe_hold regex transcribes it, and "the engaged radius" now names this
	// wider gate.
	const FVector Belief = Bot->GetSensorium().GetLastSeenLocation();
	if (!IsWithin(*Bot, Belief, InstanceData.FightRangeUU))
	{
		// THE SPELL EDGE (AIB10 re-instrument): the first cut logged this hold EVERY TICK
		// while the leg line fired once per leg, and the "182:1 gated out" measurement was
		// those two rates divided — frames over legs, meaningless. One line per SPELL at
		// the edge (the closing line below carries the duration) makes holds and legs
		// commensurate: hold spells vs stepped legs, plus denied-seconds the harness sums.
		// A hold is not a failure, but it must be countable, or "the strafe is too short"
		// has no measurement behind it.
		FAIBMovementState& MovementState = Bot->GetMovementState();
		if (!MovementState.bStrafeOutsideGate)
		{
			MovementState.bStrafeOutsideGate = true;
			MovementState.StrafeOutsideSinceSeconds = Bot->GetWorld()->GetTimeSeconds();
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s strafe held — outside the engaged radius (range %.0fuu > %.0fuu)."),
				*Bot->GetName(), FVector::Dist(Pawn->GetActorLocation(), Belief), InstanceData.FightRangeUU);
		}
		return EStateTreeRunStatus::Running;
	}
	EndStrafeGateSpell(*Bot, TEXT("reentered"));

	// The policy decides the rhythm (per-life state on the controller — a branch blink
	// must not reset the dance); this task actuates ONE step per leg.
	const double Now = Bot->GetWorld()->GetTimeSeconds();
	const EAIBStrafeIntent Intent = FAIBMovementPolicy::StepStrafe(
		Bot->GetMovementState(), Bot->GetSkillProfile().Level(EAIBSkill::Movement),
		Bot->GetPolicyRandom(), Now);
	// The stamp lives on the CONTROLLER's movement state (W-REVIEW P4+5 H1): instance
	// data resets on every Engage re-entry, and a reset stamp re-actuated the same leg
	// once per belief blink — up to five 220uu steps in a leg authored as one.
	FAIBMovementState& MovementState = Bot->GetMovementState();
	if (MovementState.NextDecisionAtSeconds == MovementState.LastActuatedLegStamp)
	{
		return EStateTreeRunStatus::Running; // this leg is already actuated, hold or move
	}
	MovementState.LastActuatedLegStamp = MovementState.NextDecisionAtSeconds;

	// A HOLD IS AN ACTUATION TOO (founder's strafe review, 26 Aug). The policy's hold
	// windows only ever suppressed NEW steps — the previous leg's move request kept
	// walking right through them, so the "plant" the ladder authors (a Novice stands
	// 19 windows in 20) never actually read on a body mid-chord. One StopMovement per
	// hold leg makes the stand real; safe, because inside the engaged circle this task
	// is the only mover (MoveNearBelief stations, it does not move).
	if (Intent == EAIBStrafeIntent::Hold)
	{
		Bot->StopMovement();
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s strafe hold — planted for this leg."), *Bot->GetName());
		return EStateTreeRunStatus::Running;
	}

	// ON AN ARC AROUND THE BELIEF, not perpendicular to it. A perpendicular step always
	// LENGTHENS range — from distance d a lateral L lands at sqrt(d^2 + L^2) — so it
	// walks itself out of this task's own EngagedRadius gate and MoveNearBelief drags it
	// back. That thrash was the "strafe is far too short" report: at d=340 inside a 350
	// gate there are only 83uu of room, and at d=350 there are none at all.
	//
	// Rotating the bot's own bearing about the belief keeps range CONSTANT by
	// construction, so the step can never leave the gate however long it is — and it is
	// what strafing physically is, circling an opponent rather than backing away sideways.
	FVector FromBelief = Pawn->GetActorLocation() - Belief;
	FromBelief.Z = 0.f;
	const float RangeUU = FromBelief.Size();
	if (RangeUU <= KINDA_SMALL_NUMBER)
	{
		return EStateTreeRunStatus::Running; // standing on the belief: no bearing to rotate
	}

	// FILL THE LEG. One step is issued per leg, so a constant distance can only ever suit
	// one rung: at 600uu/s a leg covers 210..1200uu across the ladder, against the old
	// fixed 220 — tuned for Expert's SHORTEST leg, leaving every other rung standing for
	// 60-80% of its own leg. Derive it from the leg actually in flight instead.
	const float LegRemainingSeconds = FMath::Max(0.f,
		static_cast<float>(MovementState.NextDecisionAtSeconds - Bot->GetWorld()->GetTimeSeconds()));
	float StepUU = InstanceData.StepDistanceUU;
	if (const UPawnMovementComponent* MoveComp = Pawn->GetMovementComponent())
	{
		const float Speed = MoveComp->GetMaxSpeed();
		if (Speed > 0.f && LegRemainingSeconds > 0.f)
		{
			StepUU = FMath::Max(InstanceData.StepDistanceUU, Speed * LegRemainingSeconds);
		}
	}

	// Cap the ARC, not the distance: a long leg at close range would otherwise swing the
	// bot most of the way around the target, which reads as orbiting, not footwork.
	// (The arc also retires the review's M2 clamp: range is invariant under an arc step
	// by construction, so the step can never escape the engaged circle at all.)
	const float ArcRadians = FMath::Min(FMath::DegreesToRadians(InstanceData.MaxArcDegrees), StepUU / RangeUU);
	const float Signed = ArcRadians * (Intent == EAIBStrafeIntent::Right ? 1.f : -1.f);
	const FVector Rotated = FromBelief.GetSafeNormal().RotateAngleAxisRad(Signed, FVector::UpVector);

	// THE SPIRAL FIX (founder's strafe review, 26 Aug), rebanded for the fight range.
	// Only the ENDPOINTS of an arc step sit on the range circle — the walk between them
	// is the CHORD, dipping inward by R(1-cos(arc/2)) at midpoint. Legs are TIME-driven
	// and routinely expire mid-chord, so the next leg re-measures range from the dip and
	// keeps it. Over the old NARROW band that compounded into the target's face; over
	// the wide fight range the same ratchet is kept ON PURPOSE as gradual pressure — a
	// jinking bot slowly working closer is the Halo read — and the floor is what stops
	// it at stand-off instead of at melee-accident range.
	const float DesiredRangeUU = FMath::Clamp(RangeUU,
		FMath::Min(InstanceData.StandOffMinUU, InstanceData.FightRangeUU), InstanceData.FightRangeUU);
	const FVector Destination = Belief + Rotated * DesiredRangeUU;

	// Projected onto the navmesh by the move itself: a step into a wall or off a ledge
	// resolves to the nearest legal point instead of failing (the host's proven call).
	if (Bot->MoveToLocation(Destination, /*AcceptanceRadius=*/50.f, /*bStopOnOverlap=*/true,
			/*bUsePathfinding=*/true, /*bProjectDestinationToNavigation=*/true, /*bCanStrafe=*/true)
		== EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s strafe step refused — holding this leg."), *Bot->GetName());
	}
	else
	{
		// The measurement the founder's report needed and nobody had: how far one leg
		// actually carries the bot, and at what range. Range is printed because the arc
		// is supposed to hold it constant — a drifting range means the geometry is wrong.
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s strafe leg — %.0fuu of arc at range %.0fuu (%.0f deg, %.2fs left)."),
			*Bot->GetName(), ArcRadians * RangeUU, RangeUU,
			FMath::RadiansToDegrees(ArcRadians), LegRemainingSeconds);
	}
	return EStateTreeRunStatus::Running;
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBUnservedWantTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const UAIBAmbitionEngine* Engine = Bot ? Bot->GetAmbitionEngine() : nullptr;
	// F7 at default verbosity: the one line that turns "mystery statue" into a named
	// gap. The sentinel beside this task ends the branch when the want changes.
	UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s wants '%s' and NO branch serves it — standing until the want changes (F7)."),
		Bot ? *Bot->GetName() : TEXT("<no controller>"),
		Engine && Engine->GetCurrent().IsValid() ? *Engine->GetCurrent().ToString() : TEXT("<none>"));
	return EStateTreeRunStatus::Running;
}
