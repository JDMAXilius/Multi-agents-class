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
#include "Skills/AIBWeaponPolicy.h"
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
	void SteerControlRotation(AAIBBotController& Controller, const FVector& WorldPoint, float DegreesPerSecond, float DeltaTime)
	{
		APawn* Pawn = Controller.GetPawn();
		if (!Pawn)
		{
			return;
		}
		// CLAIM THE YAW. Every aimer goes through here, so this one line is what tells the
		// movers to stop facing their travel — see AAIBBotController::NoteYawClaimed.
		if (const UWorld* World = Controller.GetWorld())
		{
			Controller.NoteYawClaimed(World->GetTimeSeconds());
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

	/** AIB19: the traverse aim's turn rate. Faster than combat facing on purpose — the
	 *  anchor is scenery that cannot dodge, and a slow pan here is seconds spent as a
	 *  stationary target at a known doorstep. Still through the ONE bounded steer (F4). */
	constexpr float TraverseAimTurnRate = 540.f;

	/** RELOAD below a quarter magazine, re-tapped no faster than this. The magazine only
	 *  refills on the weapon's own notify, so a refused reload (frozen, dead, no montage)
	 *  would otherwise be re-pressed at tick rate forever. */
	constexpr float ReloadAtMagazineFraction = 0.25f;
	constexpr float ReloadRetrySeconds = 1.0f;
	/** How long a bot may hold the reload crouch before it gives up on the magazine. Longer
	 *  than any real reload in the table (2.2s is the slowest) with room for the retry
	 *  cadence, so a working reload is never interrupted by this. */
	constexpr float ReloadGiveUpSeconds = 6.0f;

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

	/** The host's name for a route, or "?" when the host names none. AIB19's open finding
	 *  is that ~half of grapple attempts fail to reach the standoff and NOBODY CAN SAY
	 *  WHICH ROUTES — every line named only the bot, so the remedy (a manifest fix for a
	 *  bad authored anchor vs a generator retune for a physics shortfall) had nothing to
	 *  branch on. Every traverse line carries this now. */
	FString RouteLabel(const FName RouteId)
	{
		return RouteId.IsNone() ? FString(TEXT("?")) : RouteId.ToString();
	}

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

		// FACE THE WALK (founder, 1 Sep: bots "walking and running in reverse instead of
		// like a human rotating themselves"). This host is an FPS pawn whose body yaw IS
		// the control rotation, and until now NOTHING in a mover wrote it — so a bot
		// crossing the map kept the heading of whatever it last aimed at and moonwalked
		// the whole way. The sibling framework has faced its walk since R9; this module
		// was written without it.
		//
		// Suppressed while an aimer holds the yaw, which is the founder's own exception:
		// "that doesn't mean that it cannot be doing evasive actions in backwards,
		// especially if it is in combat mode". A bot with a target to face keeps facing
		// it and strafes and backpedals exactly as before — the claim, not a guess about
		// which branch is running, is what decides.
		if (!Bot.IsYawClaimed(Bot.GetWorld() ? Bot.GetWorld()->GetTimeSeconds() : 0.0)
			&& ToGoal > ArriveRadiusUU)
		{
			// Velocity when there is real motion, the GOAL when there is not: a bot that
			// has stopped, or is about to set off, should turn toward where it is going
			// BEFORE it starts, rather than leaving sideways and correcting.
			FVector Travel = Pawn->GetVelocity();
			Travel.Z = 0.f;
			if (Travel.Size() < AIB::TravelFacingMinSpeedUU)
			{
				Travel = Goal - Here;
				Travel.Z = 0.f;
			}
			if (!Travel.IsNearlyZero())
			{
				// Yaw only, and LEVEL: a walking body does not pitch. Steering at a point
				// would tilt the head at the floor on a downhill and at the sky on a ramp,
				// which is also where the aim would start from if a target appeared.
				const FRotator Current = Bot.GetControlRotation();
				const FRotator Desired(0.f, Travel.Rotation().Yaw, 0.f);
				const FRotator Stepped = FMath::RInterpConstantTo(
					FRotator(0.f, Current.Yaw, 0.f), Desired, DeltaTime,
					AIB::TravelFacingTurnRateDeg);
				const FRotator Applied(Current.Pitch, Stepped.Yaw, Current.Roll);
				Bot.SetControlRotation(Applied);
				Pawn->FaceRotation(Applied, DeltaTime);
			}
		}

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
FGameplayTag FAIBGateEvadeCondition::GetBranchTag() const        { return AIBTags::Ambition_Evade; }
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
	if (Bot && Bot->GetSensorium().HasVisibleTarget())
	{
		return EStateTreeRunStatus::Running;
	}
	// Nothing to face. In Engage that ends the fight; in Retreat it is a normal moment
	// mid-flight and the flee must go on (see bRequireTarget).
	return InstanceData.bRequireTarget ? EStateTreeRunStatus::Failed : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBFaceBeliefTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn || !Bot->GetSensorium().HasVisibleTarget())
	{
		// No pawn is a real failure either way. No TARGET is only a failure where the
		// branch is built on holding one.
		const bool bFatal = !Pawn || InstanceData.bRequireTarget;
		return bFatal ? EStateTreeRunStatus::Failed : EStateTreeRunStatus::Running;
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
		// AND THE CROUCH. This block already gives back the trigger and the sights; the crouch
		// was the one rented verb it kept, so a corpse stayed squatting and the next life's
		// task instance started believing it was mid-reload (aib-critic L2).
		if (InstanceData.bCrouchedToReload)
		{
			SetCrouch(*Avatar, false);
			InstanceData.bCrouchedToReload = false;
		}
		InstanceData.ReloadWantedSeconds = 0.f;
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
	// HOISTED ABOVE THE RELOAD GATE (aib-critic M3, 28 Aug), for two reasons that turned out
	// to be one. A bot mid-reload would not swing at a rusher inside knife range — it crouched
	// and took the beating, where Halo's answer to a rusher mid-reload is ALWAYS the melee. And
	// worse, the reload's early return skipped FAIBMeleePolicy::ShouldMelee entirely, while that
	// policy's own contract says it must be stepped EVERY tick or its continuous-range reset law
	// does not hold. Stepping it here honours the contract and makes the interrupt possible.
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
	//
	// EMPTY-HANDED is read HERE, above the melee, because it changes two decisions and the
	// melee is the first of them. It means the hand cannot fight AND the pouch can — the
	// dead-end state, not a dry loadout. See the swap block below for why those differ.
	const bool bCanFight = Avatar->CanWeaponFight();
	const bool bHasUsableWeapon = Avatar->HasUsableWeapon();
	const bool bEmptyHanded = !bCanFight && bHasUsableWeapon;
	const bool bMeleeRecognised = FAIBMeleePolicy::ShouldMelee(
		Bot->GetMeleeState(), bHasDistance ? DistanceUU : -1.f, Facts.bTargetVisible,
		Bot->GetSkillProfile().Level(EAIBSkill::Melee), Now, bEmptyHanded);
	const float MeleeRangeUU = Avatar->GetMeleeRangeUU();
	// THE INTERRUPT. Reach and recognition both, so the swing still obeys the level's read
	// (an Expert reads the rush early, a Novice a beat later) — the reload does not get to
	// bypass a capability gate, it only stops OUTRANKING one.
	const bool bMeleeWarranted = bMeleeRecognised && bHasDistance && MeleeRangeUU > 0.f
		&& DistanceUU <= MeleeRangeUU * MeleeCommitFraction;

	const bool bWantsReload = Facts.bHasReserveAmmo && Facts.AmmoNorm <= ReloadAtMagazineFraction;
	InstanceData.ReloadWantedSeconds = bWantsReload ? InstanceData.ReloadWantedSeconds + DeltaTime : 0.f;

	// THE CROUCH MUST NOT OUTLIVE ITS REASON. Crouching is rented against hands that are busy
	// changing a magazine; when the magazine never arrives the rent is never paid and the bot
	// simply squats in the open until it dies. One measured cause was an unresolved weapon row
	// (fixed in the adapter, which now reports such a weapon as FULL) — this is the guard for
	// every cause not yet measured. Standing up and fighting badly beats crouching and not
	// fighting at all.
	if (bWantsReload && InstanceData.ReloadWantedSeconds >= ReloadGiveUpSeconds)
	{
		if (InstanceData.bCrouchedToReload)
		{
			SetCrouch(*Avatar, false);
			InstanceData.bCrouchedToReload = false;
			UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s wanted a reload for %.0fs and never got one (ammo %.2f, reserve %s) — standing up and fighting anyway."),
				*Bot->GetName(), InstanceData.ReloadWantedSeconds, Facts.AmmoNorm,
				Facts.bHasReserveAmmo ? TEXT("yes") : TEXT("no"));
		}
	}
	else if (bWantsReload && !bMeleeWarranted)
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
			// THE CROUCH INSTRUMENT (founder, 28 Aug: "they are just staying crouched").
			// Measured 1222 reload REFUSALS against 1 activation in a single match, so the
			// bot asks every second, is told no, and holds the crouch that goes with asking.
			// The refusal happens on the GAME side and its reason is invisible from here, so
			// this prints the inputs the decision was made on — ammo, reserve, and whether the
			// body actually crouched — which is the half this module can see.
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s wants RELOAD — ammo %.2f (gate %.2f), reserve %s, crouched %s."),
				*Bot->GetName(), Facts.AmmoNorm, ReloadAtMagazineFraction,
				Facts.bHasReserveAmmo ? TEXT("yes") : TEXT("no"),
				Avatar->IsCrouched() ? TEXT("yes") : TEXT("no"));
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

	// -- SWAP: hold the right thing, and NEVER hold nothing ---------------------------
	// The avatar answers; this presses. Nothing here knows what a weapon is or what one
	// is worth — pressing until the answer is yes walks past the host's null holster slot
	// the same way a mouse wheel does, which is why the host's equipment code needed no
	// change to make this work.
	//
	// TWO REASONS to spin the wheel, and conflating them is what left bots standing
	// around unarmed (founder, 1 Sep: "they tend to have not a weapon on it"):
	//
	//   EMPTY-HANDED — the hand cannot fight and the pouch can. A DEAD END, not a
	//     preference. The carry contains a deliberate null Unarmed slot; a cycle that
	//     stopped on it left IsBestWeaponForRange answering false forever (Best != Current
	//     AND Current cannot fight), so the "settled" reset below never fired, the press
	//     budget never refilled, and the bot was unarmed for the rest of its life. It
	//     also does not need a TARGET: a bot holding nothing should draw while it walks,
	//     which is why this arm ignores bHasDistance and asks at a default range.
	//   WRONG-RANGED — the hand works, something else works better here. A preference.
	//
	// THE PRESS CAP BELONGS TO THE SECOND CASE ONLY. It exists to stop a bot whose whole
	// loadout is dry from cycling for the rest of the match — and HasUsableWeapon() is
	// that test, properly stated. "I am standing on the empty holster" is not a dry
	// loadout, and budgeting it was the bug.
	// THE RANGE THE QUESTION IS ASKED AT. With a target it is the real distance; without
	// one it is a mid-map default, so a bot patrolling with the wrong tool in its hands
	// still walks toward the general-purpose answer instead of waiting to be shot at
	// before it thinks about its loadout. ONE range in play, asked once.
	const float SwapRangeUU = bHasDistance ? DistanceUU : AIB::NoTargetSwapRangeUU;
	// The two door reads are the ones taken above the melee, not fresh ones: one tick,
	// one answer, so the melee cannot believe the hand is empty while the swap believes
	// it is full.
	const FAIBSwapDecision Swap = FAIBWeaponPolicy::Decide(
		bCanFight, bHasUsableWeapon, Avatar->IsBestWeaponForRange(SwapRangeUU),
		InstanceData.SwapPresses, MaxSwapPresses);
	if (Swap.bCycle)
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
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s cycling weapons for %.0fuu (%s, press %d/%d)."),
				*Bot->GetName(), SwapRangeUU,
				Swap.bEmptyHanded ? TEXT("EMPTY-HANDED, uncapped") : TEXT("wrong-ranged"),
				InstanceData.SwapPresses, MaxSwapPresses);
		}
		// Do NOT fire mid-cycle: the hand may be empty or holding the wrong answer, and a
		// burst pressed into an equip montage is a burst that never leaves the barrel.
		return EStateTreeRunStatus::Running;
	}
	// SETTLED. Two conditions, because the old single one could not be met from the dead
	// end it was supposed to release: a bot on the null slot is never "best", so the reset
	// never fired. Something in hand that can fight is the first half of settled; being
	// the right thing for the range is the second, and with no target there is no range
	// to be right for.
	if (InstanceData.SwapPresses > 0 && Swap.bSettled)
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

	// THE BLAST OUTRANKS THE ENEMY, and this is what turns the Evade want into a dodge.
	// A grenade at your feet is a closer problem than the rifle across the room, and running
	// "away from the shooter" while standing on the grenade is how a bot dies looking clever.
	// BlastCenterRelative is centre-minus-self, so self + it is the centre in world space —
	// the fact was published for exactly this and had no reader until now.
	//
	// Same node, same mover, same projection and stall handling: the dodge is a flee with a
	// different threat, not a second movement system.
	FVector ThreatPoint;
	bool bHasThreatPoint = false;
	const FAIBFacts& BlastFacts = Bot->GetLastFacts();
	if (BlastFacts.bIncomingBlast)
	{
		ThreatPoint = Pawn->GetActorLocation() + BlastFacts.BlastCenterRelative;
		bHasThreatPoint = true;
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s scattering — blast %.1fs out, %.0fuu away."),
			*Bot->GetName(), BlastFacts.BlastSecondsToDetonation, BlastFacts.BlastCenterRelative.Size2D());

		// AND DASH OUT OF IT. This is what the verb is FOR: a grenade at your feet is the one
		// moment where walking away is not fast enough, and the host's dash covers ~500uu in a
		// quarter second — most of a blast radius, in the window a fuse actually leaves.
		//
		// Pressed HERE rather than in the strafe or on a timer, so it fires once per scatter
		// and only when something is genuinely about to go off. The bot tracks its own window
		// (CanDash) because the host refuses a dash on cooldown, and a refused verb pressed on
		// a timer is exactly the futile-press shape F7 bans — the reload taught that lesson.
		//
		// The MOVER still owns where the body goes: the dash is a shove along the direction
		// the flee already chose, not a second opinion about where safety is.
		if (Bot->CanDash())
		{
			if (IAIBAvatarInterface* DashAvatar = Bot->GetAvatar())
			{
				DashAvatar->PressVerb(AIBTags::Verb_Dash);
				DashAvatar->ReleaseVerb(AIBTags::Verb_Dash);
				Bot->NoteDashed(AIB::DashThrottleSeconds);
				UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s dashed clear of the blast."), *Bot->GetName());
			}
		}
	}
	// Otherwise: away from the freshest threat knowledge we hold — the visible belief, else
	// memory. With NOTHING held — hurt, threat unknown — Retreat still needs an executable exit
	// (W-REVIEW P3 H1): reposition to a random reachable point. A hurt bot that
	// relocates reads as falling back; a hurt bot frozen mid-arena reads as broken, and
	// the hysteresis defends the freeze.
	else if ((bHasThreatPoint = Bot->GetSensorium().HasVisibleTarget()))
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
	// DEFEND, rather than run until the want expires. With the threat IN SIGHT and contact
	// already broken to the band's floor, this mover stands down and the footwork beside it
	// takes the legs — the bot evades and keeps shooting instead of jogging away with its
	// back turned (founder, 28 Aug). Out of sight it still flees: you cannot fight what you
	// cannot see, and breaking contact fully is the right answer there.
	// NEVER stand down on a live grenade. The defend band's whole purpose is to stop running
	// and fight — which is exactly the wrong answer while something is about to detonate, and
	// would cancel the scatter one tick after it started.
	if (InstanceData.DefendRangeUU > 0.f && !Bot->GetLastFacts().bIncomingBlast
		&& Bot->GetSensorium().HasVisibleTarget()
		&& !IsWithin(*Bot, Bot->GetSensorium().GetLastSeenLocation(), InstanceData.DefendRangeUU))
	{
		ReleaseLocomotion(*Bot, InstanceData.Locomotion);
		if (!InstanceData.bStoodDownToDefend)
		{
			// ONCE, on the edge. Per-tick this would cancel the strafe step every frame and
			// the bot would stand rooted — the exact "doing nothing" this change exists to end.
			InstanceData.bStoodDownToDefend = true;
			Bot->StopMovement();
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s broke contact — holding to DEFEND (%.0fuu out)."),
				*Bot->GetName(), FVector::Dist(Pawn->GetActorLocation(), Bot->GetSensorium().GetLastSeenLocation()));
		}
		// Deliberately NOT Succeeded: succeeding would re-select the branch and re-pick a
		// new flee goal, which is the running-away loop again. The bot stays in Retreat,
		// fighting, until the brain's own score says the danger has passed.
		return EStateTreeRunStatus::Running;
	}
	if (InstanceData.bStoodDownToDefend)
	{
		// THE WAY BACK OUT, and without it the band was a ONE-WAY DOOR (aib-critic H1, 28 Aug).
		// Standing down issued StopMovement, which kills the path request the enter state made.
		// Dropping back inside the band then cleared the flag and fell straight into
		// TickLocomotion, which holds sprint against a DEAD request — so the bot froze in the
		// open for about a second and a half until the wedge watchdog happened to re-issue it.
		//
		// And re-entry is not incidental, it is structural: the strafe walks a chord, whose
		// midpoint dips inward by ~11% at MaxArcDegrees 55, so two legs from 700-790uu put the
		// bot back under the floor by construction.
		InstanceData.bStoodDownToDefend = false;
		MoveToNavPoint(*Bot, InstanceData.FleeGoal, 150.f);
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s fell back inside the defend band — resuming the break."), *Bot->GetName());
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

	// THE SWEEP OWNS THE YAW TOO, and says so. It writes the control rotation directly
	// rather than through SteerControlRotation (it turns at a rate toward no point at
	// all), so it must claim by hand — otherwise a mover ticking beside it would drag the
	// body back toward the path and the search would read as a bot shaking its head.
	if (const UWorld* SweepWorld = Bot->GetWorld())
	{
		Bot->NoteYawClaimed(SweepWorld->GetTimeSeconds());
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
	InstanceData.TraversePhase = 0;

	// AIB19 — sometimes an idle leg is the climb (or the drop). Armed BEFORE the
	// ordinary picks so the traverse owns the Goal; every guard that fails just falls
	// through to the wander this leg would have been anyway. The short clock is charged
	// at ARMING, not at success — a route whose path immediately refuses must not
	// re-arm on every branch re-entry.
	if (MayGrappleTraverse())
	{
		IAIBWorldQuery* Query = Bot->GetWorldQuery();
		IAIBAvatarInterface* Avatar = Bot->GetAvatar();
		const double Now = Bot->GetWorld()->GetTimeSeconds();
		FVector Approach, Anchor;
		FName RouteId = NAME_None;
		if (Query && Avatar && Avatar->IsGrounded()
			&& Now >= InstanceData.NextTraverseAllowedSeconds
			&& Bot->GetSkillProfile().Level(EAIBSkill::Movement) >= EAIBCompetence::Trained
			&& Query->GetGrappleRoute(Pawn->GetActorLocation(), Approach, Anchor, RouteId))
		{
			const float SelfZ = Pawn->GetActorLocation().Z;
			FRandomStream& Random = Bot->GetPolicyRandom();
			if (Anchor.Z > SelfZ + InstanceData.MinTraverseRiseUU
				&& Random.FRand() < InstanceData.ClimbChance)
			{
				InstanceData.RouteApproach = Approach;
				InstanceData.RouteAnchor = Anchor;
				InstanceData.RouteId = RouteId;
				InstanceData.Goal = Approach;
				InstanceData.bHasGoal = true;
				InstanceData.TraversePhase = 1;
			}
			else if (SelfZ > Approach.Z + InstanceData.MinTraverseRiseUU
				&& Random.FRand() < InstanceData.DescendChance)
			{
				InstanceData.RouteApproach = Approach;
				InstanceData.RouteAnchor = Anchor;
				InstanceData.RouteId = RouteId;
				// The lip: the anchor's spot at the bot's own height — on this deck's
				// navmesh island by construction, so the walk there is an ordinary walk.
				InstanceData.Goal = FVector(Anchor.X, Anchor.Y, SelfZ);
				InstanceData.bHasGoal = true;
				InstanceData.TraversePhase = 4;
			}
			if (InstanceData.TraversePhase != 0)
			{
				InstanceData.PhaseSeconds = 0.f;
				InstanceData.bAirborneSeen = false;
				InstanceData.NextTraverseAllowedSeconds =
					Now + InstanceData.TraverseCooldownSeconds / 3.0;
			}
		}
	}

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

		// AIB17: an idle leg walks TOWARD the team's heard fight while the note is
		// fresh — the draw stays a navmesh RANDOM point (never a beeline, F6-clean),
		// just centred on the heard place with a tighter spread. The note was earned
		// by this bot's own ears at note time and decays in seconds; an unreachable
		// heard-point falls back to the plain self-centred draw below.
		const FAIBAllyFightMemory& AllyFight = Bot->GetAllyFightMemory();
		const bool bTowardFight = AllyFight.IsFresh(Bot->GetWorld()->GetTimeSeconds());
		FNavLocation Wander;
		if (bTowardFight && NavSys && NavSys->GetRandomReachablePointInRadius(
			AllyFight.HeardAt, InstanceData.WanderRadiusUU * 0.4f, Wander))
		{
			InstanceData.Goal = Wander.Location;
			InstanceData.bHasGoal = true;
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s wandering toward the team's fight."), *Bot->GetName());
		}
		else if (NavSys && NavSys->GetRandomReachablePointInRadius(
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
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn || !InstanceData.bHasGoal)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (InstanceData.TraversePhase != 0)
	{
		return TickTraverse(*Bot, *Pawn, InstanceData, DeltaTime);
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
	// A branch ended mid-traverse (the sentinel: the want moved on) resets the machine
	// but NOT a pull in flight — the host's movement component owns a started pull, and
	// a bot yanked into Engage mid-ride finishes arriving exactly like a human would.
	InstanceData.TraversePhase = 0;
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		ReleaseLocomotion(*Bot, InstanceData.Locomotion);
		Bot->StopMovement();
	}
}

/** AIB19's micro-machine. Every exit path either logs an outcome or logs a whiff, sets
 *  the traverse clock, and returns a status the branch can live with — a failed hook is
 *  never a stranded bot (F7): Succeeded re-selects Roam, which wanders as if the climb
 *  had never been offered. */
EStateTreeRunStatus FAIBMoveToPOITask::TickTraverse(AAIBBotController& Bot, APawn& Pawn,
	FInstanceDataType& InstanceData, float DeltaTime) const
{
	IAIBAvatarInterface* Avatar = Bot.GetAvatar();
	if (!Avatar)
	{
		InstanceData.TraversePhase = 0;
		return EStateTreeRunStatus::Failed;
	}
	const double Now = Bot.GetWorld()->GetTimeSeconds();
	const FVector Here = Pawn.GetActorLocation();
	const float RetrySoonSeconds = InstanceData.TraverseCooldownSeconds / 3.f;

	switch (InstanceData.TraversePhase)
	{
	case 1: // ---- walk to the approach --------------------------------------------
	case 4: // ---- walk to the lip (same walk, different doorstep) -----------------
	{
		if (IsWithin(Bot, InstanceData.Goal, InstanceData.ApproachReachUU))
		{
			ReleaseLocomotion(Bot, InstanceData.Locomotion);
			Bot.StopMovement();
			InstanceData.PhaseSeconds = 0.f;
			InstanceData.bAirborneSeen = false;
			if (InstanceData.TraversePhase == 1)
			{
				InstanceData.TraversePhase = 2;
			}
			else
			{
				// THE DROP: pathfinding OFF and no nav projection on purpose — the
				// approach point is a storey below this island, and walking straight
				// off the lip toward it is the whole mechanism. The fall is the move.
				Bot.MoveToLocation(InstanceData.RouteApproach, /*AcceptanceRadius=*/80.f,
					/*bStopOnOverlap=*/true, /*bUsePathfinding=*/false,
					/*bProjectDestinationToNavigation=*/false, /*bCanStrafe=*/true);
				UE_LOG(LogAIBot, Log, TEXT("AIBot: %s steps off the lip."), *Bot.GetName());
				InstanceData.TraversePhase = 5;
			}
			return EStateTreeRunStatus::Running;
		}
		// The ordinary walk machinery: progress bookkeeping, sprint, the wedge jump.
		const float DistNow = FVector::Dist(Here, InstanceData.Goal);
		if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
		{
			InstanceData.ClosestSoFarUU = DistNow;
			InstanceData.SecondsWithoutProgress = 0.f;
		}
		else if ((InstanceData.SecondsWithoutProgress += DeltaTime)
			>= InstanceData.GiveUpAfterNoProgressSeconds)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s could not reach the grapple %s on %s — back to wandering."),
				*Bot.GetName(), InstanceData.TraversePhase == 1 ? TEXT("approach") : TEXT("lip"),
				*RouteLabel(InstanceData.RouteId));
			InstanceData.TraversePhase = 0;
			InstanceData.NextTraverseAllowedSeconds = Now + RetrySoonSeconds;
			return EStateTreeRunStatus::Failed;
		}
		TickLocomotion(Bot, InstanceData.Locomotion, InstanceData.Goal,
			InstanceData.ApproachReachUU, DeltaTime);
		return EStateTreeRunStatus::Running;
	}

	case 2: // ---- aim at the anchor, then ONE press -------------------------------
	{
		SteerControlRotation(Bot, InstanceData.RouteAnchor, TraverseAimTurnRate, DeltaTime);
		const FVector Desired = (InstanceData.RouteAnchor - Pawn.GetPawnViewLocation()).GetSafeNormal();
		const float CosErr = FVector::DotProduct(Bot.GetControlRotation().Vector(), Desired);
		if (CosErr >= FMath::Cos(FMath::DegreesToRadians(InstanceData.AimToleranceDeg)))
		{
			// The press goes through the ONE door and the host's own authority
			// validation judges it — range, LOS, cooldown — like any player's press.
			Avatar->PressVerb(AIBTags::Verb_Grapple);
			Avatar->ReleaseVerb(AIBTags::Verb_Grapple);
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s grapples for the high ground on %s (rise %.0fuu)."),
				*Bot.GetName(), *RouteLabel(InstanceData.RouteId),
				InstanceData.RouteAnchor.Z - Here.Z);
			InstanceData.TraversePhase = 3;
			InstanceData.PhaseSeconds = 0.f;
			InstanceData.bAirborneSeen = false;
			return EStateTreeRunStatus::Running;
		}
		if ((InstanceData.PhaseSeconds += DeltaTime) >= InstanceData.AimTimeoutSeconds)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s hook shot never lined up on %s — back to wandering."),
				*Bot.GetName(), *RouteLabel(InstanceData.RouteId));
			InstanceData.TraversePhase = 0;
			InstanceData.NextTraverseAllowedSeconds = Now + RetrySoonSeconds;
			return EStateTreeRunStatus::Succeeded;
		}
		return EStateTreeRunStatus::Running;
	}

	case 3: // ---- the ride --------------------------------------------------------
	case 5: // ---- the drop (same watch: airborne, then ground again) --------------
	{
		InstanceData.PhaseSeconds += DeltaTime;
		if (!Avatar->IsGrounded())
		{
			InstanceData.bAirborneSeen = true;
		}
		const bool bLanded = Avatar->IsGrounded() && InstanceData.bAirborneSeen
			&& InstanceData.PhaseSeconds > 0.25f;
		// Never left the ground after a full second = the press was REFUSED (the host's
		// cooldown or validation said no) — waiting out the whole ride timeout would
		// just be a bot standing at a known doorstep. The refusal was the host's right;
		// the early exit is ours.
		const bool bTimedOut = InstanceData.PhaseSeconds
			>= InstanceData.RideTimeoutSeconds + (InstanceData.TraversePhase == 5 ? 1.f : 0.f)
			|| (!InstanceData.bAirborneSeen && InstanceData.PhaseSeconds > 1.f);
		if (!bLanded && !bTimedOut)
		{
			return EStateTreeRunStatus::Running;
		}
		const bool bClimb = InstanceData.TraversePhase == 3;
		const float RiseUU = Here.Z - InstanceData.RouteApproach.Z;
		const bool bMadeIt = bLanded && (bClimb
			? RiseUU > InstanceData.MinTraverseRiseUU * 0.5f
			: RiseUU < InstanceData.MinTraverseRiseUU * 0.5f);
		if (bMadeIt && bClimb)
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s made the deck on %s (%.0fuu up, wanted %.0fuu)."),
				*Bot.GetName(), *RouteLabel(InstanceData.RouteId), RiseUU,
				InstanceData.RouteAnchor.Z - InstanceData.RouteApproach.Z);
		}
		else if (bMadeIt)
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s dropped back down on %s (%.0fuu)."),
				*Bot.GetName(), *RouteLabel(InstanceData.RouteId), RiseUU);
		}
		else
		{
			// TWO DIFFERENT FAILURES wore one message, which is the other half of why
			// AIB19's finding could not be acted on. bAirborneSeen separates them and
			// they have OPPOSITE remedies:
			//   REFUSED    - never left the ground. The host's own range/LOS/cooldown
			//                validation said no, so the STANDOFF is wrong (generator).
			//   SHORT      - rode the hook and landed under the lip. The ANCHOR is wrong
			//                (manifest), or the pull cannot carry that rise.
			// Logged at Log, not Verbose: a whiff is the thing being measured, and a
			// diagnostic nobody's default verbosity prints is not an instrument.
			UE_LOG(LogAIBot, Log,
				TEXT("AIBot: %s traverse FAILED on %s (%s, rose %.0fuu of %.0fuu) — back to wandering."),
				*Bot.GetName(), *RouteLabel(InstanceData.RouteId),
				!InstanceData.bAirborneSeen ? TEXT("REFUSED - never left the ground")
					: (bClimb ? TEXT("SHORT - rode the hook, landed under the lip")
							  : TEXT("SHORT - drop never cleared the deck")),
				RiseUU, InstanceData.RouteAnchor.Z - InstanceData.RouteApproach.Z);
		}
		InstanceData.TraversePhase = 0;
		InstanceData.NextTraverseAllowedSeconds = Now
			+ (bMadeIt ? InstanceData.TraverseCooldownSeconds : RetrySoonSeconds);
		return EStateTreeRunStatus::Succeeded;
	}

	default:
		InstanceData.TraversePhase = 0;
		return EStateTreeRunStatus::Failed;
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
			// Re-seed the RATCHET against the new goal — progress toward a post that moved
			// must be measured from where it moved to.
			InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);

			// But do NOT reset the stall CLOCK here. It used to, and that silently disabled
			// the give-up entirely: the goal is a live teammate, the repoll runs every
			// RepollIntervalSeconds (0.5s), and any teammate who is walking, strafing or
			// fighting moves more than 50uu in that time — so the clock was zeroed ~16
			// times before it could ever reach GiveUpAfterNoProgressSeconds (8s).
			// NoteCurrentAmbitionFailed() below became unreachable, and a bot walled off
			// from its team stood there for the rest of the match with no F7 line to say
			// so. Under teams-by-default that is a silent, permanent deadlock.
			//
			// The clock is now cleared in exactly one place — the genuine-progress branch
			// below — so a bot that is actually closing keeps its reprieve and a bot that
			// is not eventually gives up and lets another want run.
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
	// THE BAND'S FLOOR (Retreat only; 0 in Engage). Inside it the flee mover is still
	// breaking contact and owns the legs — one mover per tick, always.
	if (InstanceData.MinRangeUU > 0.f && IsWithin(*Bot, Belief, InstanceData.MinRangeUU))
	{
		return EStateTreeRunStatus::Running;
	}
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

	// THE COMBAT DASH. The scatter proved the verb works; this is what makes it visible,
	// because grenades are rare and a dash nobody sees may as well not exist.
	//
	// Runs in BOTH branches, unlike the hop: dashing is how a fight stops being two bodies
	// sliding left and right at each other, and a bot that only dashes while retreating
	// reads as a bot that only panics. The direction is the LEG's — the dash launches along
	// the movement input the step already set, so it lengthens the footwork rather than
	// arguing with it.
	//
	// Gated on the bot's own throttle FIRST and the tier roll second: the throttle is what
	// actually bounds the rate (one dash per 3.5s however eager the tier), and asking it
	// first is what keeps this off the futile-press path when the host would refuse.
	if (Bot->CanDash()
		&& Bot->GetPolicyRandom().FRand() < FAIBMovementPolicy::DashChance(
			Bot->GetSkillProfile().Level(EAIBSkill::Movement)))
	{
		if (IAIBAvatarInterface* DashAvatar = Bot->GetAvatar())
		{
			if (DashAvatar->IsGrounded())
			{
				DashAvatar->PressVerb(AIBTags::Verb_Dash);
				DashAvatar->ReleaseVerb(AIBTags::Verb_Dash);
				Bot->NoteDashed(AIB::DashThrottleSeconds);
				UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s dashed on the strafe leg."), *Bot->GetName());
			}
		}
	}

	// THE EVASIVE HOP. A defending bot that only slides left and right is a predictable
	// target on a flat plane; breaking the vertical is what spoils a tracking aim
	// (founder, 28 Aug: "crouch jump, evade, fire").
	//
	// ITS OWN CHANCE, not a rider on the juke. The first cut fired only on juke legs so it
	// would inherit JukeChance's tier gate for free — but that gate is 0.00 below Skilled,
	// so the jump was structurally impossible at Marine, the tier actually being played.
	// Measured: 9 defend stand-downs and 0 hops in a full match. HopChance is nonzero on
	// every rung and still rises with competence, so it is capability-shaped without being
	// capability-BLOCKED (R28 wants tiers to differ, not to silence a behaviour).
	//
	// DEFEND ONLY (MinRangeUU > 0 is Retreat's band, 0 in Engage). Engage's footwork is
	// unchanged, byte for byte, which keeps every strafe measurement in the AIB tickets
	// comparable rather than silently re-baselined.
	if (InstanceData.MinRangeUU > 0.f
		&& Bot->GetPolicyRandom().FRand() < FAIBMovementPolicy::HopChance(
			Bot->GetSkillProfile().Level(EAIBSkill::Movement)))
	{
		if (IAIBAvatarInterface* Avatar = Bot->GetAvatar())
		{
			// Grounded only: pressing jump mid-air is a no-op the ability system still has
			// to refuse, and a bot that spams it reads as a stuck key.
			if (Avatar->IsGrounded())
			{
				Avatar->PressVerb(AIBTags::Verb_Jump);
				Avatar->ReleaseVerb(AIBTags::Verb_Jump);
				UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s defensive juke — hopping the direction change."), *Bot->GetName());
			}
		}
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
