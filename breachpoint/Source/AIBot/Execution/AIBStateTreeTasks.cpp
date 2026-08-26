#include "Execution/AIBStateTreeTasks.h"
#include "Navigation/PathFollowingComponent.h"

#include "AIBotModule.h"
#include "Brain/AIBAmbitionEngine.h"
#include "Core/AIBBotController.h"
#include "Core/AIBTags.h"
#include "Interfaces/AIBAvatarInterface.h"
#include "NavigationSystem.h"
#include "Perception/AIBSensorium.h"
#include "StateTreeExecutionContext.h"

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

	/** GRENADE band. Below the minimum a bot blows itself up; past the maximum the throw
	 *  falls short. The band is the whole tactical judgement — there is no "grenade skill"
	 *  wired yet (Phase 4 owns that), so these are the honest defaults. */
	constexpr float GrenadeMinRangeUU = 500.f;
	constexpr float GrenadeMaxRangeUU = 2200.f;

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
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s jumped to clear whatever it is wedged on."),
				*Bot.GetName());
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
	const FGameplayTag BranchTag = GetBranchTag();
	return Engine && BranchTag.IsValid() && Engine->GetCurrent() == BranchTag;
}

// Each branch's identity is a virtual, not a node parameter — the compiled authoring
// surface sets nothing on the nodes it adds, so per-branch data lives in the TYPE.
FGameplayTag FAIBAmbitionGateCondition::GetBranchTag() const     { return FGameplayTag(); }
FGameplayTag FAIBGateEngageCondition::GetBranchTag() const       { return AIBTags::Ambition_Engage; }
FGameplayTag FAIBGateRetreatCondition::GetBranchTag() const      { return AIBTags::Ambition_Retreat; }
FGameplayTag FAIBGateSearchCondition::GetBranchTag() const       { return AIBTags::Ambition_Search; }
FGameplayTag FAIBGateSeekWeaponCondition::GetBranchTag() const   { return AIBTags::Ambition_Seek; }
FGameplayTag FAIBGateRoamCondition::GetBranchTag() const         { return AIBTags::Ambition_Roam; }

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
	if (!Bot || !Bot->GetSensorium().HasVisibleTarget())
	{
		return EStateTreeRunStatus::Failed;
	}
	// THE BELIEF, never the live actor: during the juke window this is the frozen
	// last-seen spot — a bot aiming through the pillar is the bug this line bans.
	SteerControlRotation(*Bot, Bot->GetSensorium().GetLastSeenLocation(),
		InstanceData.TurnDegreesPerSecond, DeltaTime);
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
	// Already in position: station-keep from here (issuing a move to where we stand
	// would complete instantly and thrash the branch — the never-succeed contract).
	if (!IsWithin(*Bot, InstanceData.LastGoal, InstanceData.AcceptanceRadiusUU))
	{
		if (MoveToNavPoint(*Bot, InstanceData.LastGoal, InstanceData.AcceptanceRadiusUU)
			== EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s could not path to the belief — closing refused (F7)."), *Bot->GetName());
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

	// Station-keeping: chase the belief's drift, never complete. Firing runs beside
	// this task; the sentinel or a visibility loss is what ends the branch. Out of
	// position for ANY reason — belief drift or the BOT displaced (knockback, a pad) —
	// re-closes on the repath cadence (W-REVIEW P3 M3).
	InstanceData.RepathCooldown -= DeltaTime;
	const FVector Belief = Bot->GetSensorium().GetLastSeenLocation();
	if (!IsWithin(*Bot, Belief, InstanceData.AcceptanceRadiusUU)
		&& InstanceData.RepathCooldown <= 0.f)
	{
		InstanceData.LastGoal = Belief;
		InstanceData.RepathCooldown = InstanceData.RepathIntervalSeconds;
		if (MoveToNavPoint(*Bot, Belief, InstanceData.AcceptanceRadiusUU)
			== EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s could not path to the belief — closing refused (F7)."), *Bot->GetName());
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

	// The cached facts are the one info door: matured visibility + the assembled
	// can-fight answer, never raw avatar reads scattered through tasks.
	const FAIBFacts& Facts = Bot->GetLastFacts();

	// RELOAD, AND CROUCH WHILE THE HANDS ARE BUSY. The most dangerous seconds a bot has
	// are the ones it cannot shoot back in, and a reload is the only moment it chooses to
	// spend them — so it spends them small, and legibly: a crouched bot with its hands
	// busy reads as "reloading" from across the arena. No reserve is a different problem
	// (the weapon swap), and it is NOT handled here — see the ticket Log for why.
	// Every throttle decays here, ABOVE the reload's early return: a melee cooldown that
	// froze while the bot reloaded would fire the instant the magazine landed.
	InstanceData.ReloadCooldownLeft = FMath::Max(0.f, InstanceData.ReloadCooldownLeft - DeltaTime);
	InstanceData.MeleeCooldownLeft = FMath::Max(0.f, InstanceData.MeleeCooldownLeft - DeltaTime);
	InstanceData.SwapCooldownLeft = FMath::Max(0.f, InstanceData.SwapCooldownLeft - DeltaTime);
	if (Facts.bHasReserveAmmo && Facts.AmmoNorm <= ReloadAtMagazineFraction)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
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
	// think-rate DistToTargetUU, which is a want's number, not a swing's.
	float DistanceUU = 0.f;
	const bool bHasDistance = LiveDistanceToBelief(*Bot, DistanceUU);

	// -- MELEE: they are close enough to touch ---------------------------------------
	// Inside a fraction of the HELD weapon's own reach, which the door supplies — so a
	// bot with a knife commits from further out than one holding a rifle, without this
	// code ever learning what either is. A bot that shoots you from arm's length instead
	// of hitting you is the readable failure this prevents.
	const float MeleeRangeUU = Avatar->GetMeleeRangeUU();
	if (bHasDistance && MeleeRangeUU > 0.f && InstanceData.MeleeCooldownLeft <= 0.f
		&& DistanceUU <= MeleeRangeUU * MeleeCommitFraction)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		Avatar->PressVerb(AIBTags::Verb_Melee);
		Avatar->ReleaseVerb(AIBTags::Verb_Melee);
		InstanceData.MeleeCooldownLeft = MeleeRetrySeconds;
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

	// -- GRENADE: the band, the pouch, the cooldown -----------------------------------
	// Thrown at the BELIEF and only while the target is perceived, so this can never
	// become a bot reacting to something it never saw. GrenadeCount is the pouch (an
	// empty bot miming a throw is a bot not shooting), the band is the judgement, and the
	// cooldown is the thing that keeps a team of bots from carpeting the arena.
	// The cooldown is asked of the CONTROLLER, not of this task's scratch, because
	// StateTree re-initialises instance data on every state ENTRY and Engage re-enters
	// whenever a belief blinks — a per-task countdown would reset roughly once a second
	// and throttle nothing at all. See AAIBBotController::CanThrowGrenade.
	if (bHasDistance && Facts.bTargetVisible && Facts.GrenadeCount > 0
		&& Bot->CanThrowGrenade()
		&& DistanceUU >= GrenadeMinRangeUU && DistanceUU <= GrenadeMaxRangeUU)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		Avatar->PressVerb(AIBTags::Verb_Grenade);
		Avatar->ReleaseVerb(AIBTags::Verb_Grenade);
		Bot->NoteGrenadeThrown(GrenadeCooldownSeconds);
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s threw at %.0fuu (%d left in the pouch)."),
			*Bot->GetName(), DistanceUU, Facts.GrenadeCount);
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
		return EStateTreeRunStatus::Running; // stay: visibility may return next pump
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
		else
		{
			Avatar->PressVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = true;
			InstanceData.PhaseSecondsLeft = InstanceData.BurstSeconds;
		}
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
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s flee path REFUSED — failing loudly, not standing (F7)."), *Bot->GetName());
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
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s cannot path to the last-known spot — search fails loudly (F7)."), *Bot->GetName());
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
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s POI path refused — branch fails (F7)."), *Bot->GetName());
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
