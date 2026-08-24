#include "AI/BNBotStateTreeTasks.h"

#include "AbilitySystem/Attributes/BNAttributeSet.h"

#include "AI/BNBotBrain.h"
#include "AI/BNBotController.h"
#include "AI/BNPointOfInterest.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "BreachpointNext.h"
#include "Core/BNCollision.h"
#include "Core/BNGameplayTags.h"
#include "Match/BNPlayerState.h"
#include "Data/BNDataRows.h"
#include "Weapons/BNWeapon.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	/** The controller every node runs on. Prefers the editor-bound Controller instance data, and
	 *  falls back to the execution context's OWNER — which for a StateTreeAIComponent is the
	 *  AIController the component lives on (UStateTreeComponent builds its context on
	 *  *GetOwner()). The fallback is what lets ST_BNBot be authored programmatically: property
	 *  bindings are editor-only graph data with no scripting surface, so a tree built in code
	 *  would otherwise run every node against a null Controller and do nothing, silently. */
	ABNBotController* ResolveBot(const FStateTreeExecutionContext& Context, AAIController* BoundController)
	{
		if (ABNBotController* Bound = Cast<ABNBotController>(BoundController))
		{
			return Bound;
		}
		return Cast<ABNBotController>(Context.GetOwner());
	}

	/** Turns the bot's CONTROL rotation toward a world point, at a bounded rate.
	 *
	 *  THIS EXISTS BECAUSE ABNBotController DOES NOT TICK. Its constructor disables the actor
	 *  tick (law 4), and AAIController::UpdateControlRotation — the engine code that swings the
	 *  control rotation onto whatever SetFocus/SetFocalPoint named — is called from exactly that
	 *  tick. With it off, focus is an intention nothing ever acts on: the control rotation keeps
	 *  the value it had when the pawn spawned, forever.
	 *
	 *  Two separate bugs fell out of that one fact, and neither looked like the other:
	 *    - BNGA_Fire traces along the control rotation, so bots fired 1272 shots into walls
	 *      2700-3200uu away and did zero damage, while the target they had "in sight" stood
	 *      well inside 800uu of them.
	 *    - ABNCharacter sets bUseControllerRotationYaw with bOrientRotationToMovement off, so
	 *      the pawn's facing IS the control rotation. A roaming bot therefore translated along
	 *      its path without ever turning — the founder's "not walking, just sliding".
	 *
	 *  The StateTree tasks already tick, so the turn is applied here instead of by re-enabling an
	 *  actor tick. Doing it explicitly also buys the turn RATE below, which the engine's instant
	 *  focus snap would not have given us.
	 */
	void SteerControlRotation(AAIController& Controller, const FVector& WorldPoint, float DegreesPerSecond, float DeltaTime)
	{
		const APawn* Pawn = Controller.GetPawn();
		if (!Pawn)
		{
			return;
		}

		// From the EYE, not the actor origin: the weapon traces from view height, and aiming a
		// rotation computed at the feet would shoot consistently low.
		const FVector ToPoint = WorldPoint - Pawn->GetPawnViewLocation();
		if (ToPoint.IsNearlyZero())
		{
			return;
		}

		const FRotator Desired = ToPoint.Rotation();
		const FRotator Stepped = DegreesPerSecond <= 0.f
			? Desired
			: FMath::RInterpConstantTo(Controller.GetControlRotation(), Desired, DeltaTime, DegreesPerSecond);
		Controller.SetControlRotation(Stepped);

		// AND THE BODY. SetControlRotation moves the CONTROLLER's rotation — which is what the
		// weapon traces along, so aiming alone is fixed by the line above. The PAWN's visible yaw
		// is a separate step: the engine applies it in APawn::FaceRotation, called from
		// AController::UpdateControlRotation — from the actor tick this controller does not have.
		// Skipping it produced the second half of the sliding bug and hid inside the first: bots
		// shot straight while their bodies walked at a fixed 120 degrees to their own velocity,
		// which is a strafe pose played over a forward path, i.e. a slide.
		if (APawn* MutablePawn = Controller.GetPawn())
		{
			MutablePawn->FaceRotation(Stepped, DeltaTime);
		}
	}

	/** Reports what the LOCOMOTION GRAPH sees, ~1/sec while the bot is actually moving.
	 *
	 *  UBNAnimInstance drives the third-person locomotion from the movement component's velocity
	 *  expressed in the ACTOR's frame (LocalVelocity2D -> LocalVelocityDirection). So "is the bot
	 *  walking or sliding?" is not a question about the anim graph at all — it is a question
	 *  about that angle. Near 0 is a forward walk. Near 180 is a backpedal. Near +/-90 is a
	 *  strafe, and a strafe with no strafe set authored is exactly what reads as sliding.
	 *
	 *  It lives here rather than in the anim instance because the AI is what CREATES the angle:
	 *  the control rotation these tasks steer is the pawn's facing, and the path is its velocity.
	 */
	void ReportLocomotion(const APawn& Pawn, const TCHAR* What, float DeltaTime, float& SecondsUntilLog)
	{
		SecondsUntilLog -= DeltaTime;
		if (SecondsUntilLog > 0.f)
		{
			return;
		}
		SecondsUntilLog = 1.f;

		const FVector Velocity = Pawn.GetVelocity();
		const float Speed = Velocity.Size2D();
		if (Speed < 10.f)
		{
			return;
		}

		// The anim instance's own transform: world velocity unrotated into the actor's frame.
		const FVector Local = Pawn.GetActorRotation().UnrotateVector(FVector(Velocity.X, Velocity.Y, 0.f));
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
		const float Abs = FMath::Abs(Angle);
		const TCHAR* Cardinal = Abs <= 45.f ? TEXT("FORWARD")
			: (Abs >= 135.f ? TEXT("BACKWARD") : (Angle > 0.f ? TEXT("STRAFE-RIGHT") : TEXT("STRAFE-LEFT")));

		UE_LOG(LogBN, Log, TEXT("BNLocomotion: %s %s at %.0f uu/s, local dir %+.0f deg (%s)"),
			*GetNameSafe(&Pawn), What, Speed, Angle, Cardinal);
	}

	/** Says WHY a move request failed, and says it ONCE per task instance. A silent Failed here
	 *  is indistinguishable from a bot that simply chose not to move, and the two have completely
	 *  different fixes: an unreachable goal is level design, a missing navmesh is a map setting.
	 *  So this reports which one it is — the bot's own position, the goal, and whether EITHER can
	 *  be projected onto the navigation mesh at all. */
	void ReportMoveFailure(const AAIController& Controller, const AActor* Goal, const TCHAR* What, bool& bAlreadyWarned)
	{
		if (bAlreadyWarned)
		{
			return;
		}
		bAlreadyWarned = true;

		const APawn* Pawn = Controller.GetPawn();
		UWorld* World = Controller.GetWorld();
		const UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<const UNavigationSystemV1>(World) : nullptr;
		if (!Nav)
		{
			UE_LOG(LogBN, Warning, TEXT("BNBots: %s failed — there is NO NAVIGATION SYSTEM in this world. Bots cannot path anywhere."), What);
			return;
		}

		FNavLocation Projected;
		const FVector Extent(200.f, 200.f, 400.f);
		const bool bSelfOnNav = Pawn && Nav->ProjectPointToNavigation(Pawn->GetActorLocation(), Projected, Extent);
		const bool bGoalOnNav = Goal && Nav->ProjectPointToNavigation(Goal->GetActorLocation(), Projected, Extent);

		UE_LOG(LogBN, Warning, TEXT("BNBots: %s failed for %s -> %s. On navmesh: self=%s goal=%s. %s"),
			What,
			Pawn ? *Pawn->GetName() : TEXT("(no pawn)"),
			Goal ? *Goal->GetName() : TEXT("(no goal)"),
			bSelfOnNav ? TEXT("yes") : TEXT("NO"),
			bGoalOnNav ? TEXT("yes") : TEXT("NO"),
			(!bSelfOnNav || !bGoalOnNav)
				? TEXT("A point off the navmesh means the mesh is missing or unbuilt here — check the NavMeshBoundsVolume and RuntimeGeneration.")
				: TEXT("Both ends are on the navmesh, so the goal is simply unreachable from here."));
	}

	AActor* GetBotTarget(const FStateTreeExecutionContext& Context, AAIController* BoundController)
	{
		const ABNBotController* Bot = ResolveBot(Context, BoundController);
		return Bot ? Bot->GetCurrentTarget() : nullptr;
	}

	/** The aim-error cone, drawn from a wall-clock-free stream: frame counter XOR the controller's
	 *  identity hash. Cheap and repeatable-enough until the determinism harness lands with the
	 *  brain (R8's spirit — no FMath::VRand off the global unseeded stream, no FPlatformTime). */
	FVector JitteredFocalPoint(const AAIController& Controller, const AActor& Target, float AimErrorDegrees)
	{
		const APawn* Pawn = Controller.GetPawn();
		const FVector TargetLoc = Target.GetActorLocation();

		// No body, no apex for the cone. AController hides its transform on purpose — a controller's
		// location is the origin or a corpse's last spot — so the honest degenerate answer is the
		// true target, never geometry we invented.
		if (!Pawn)
		{
			return TargetLoc;
		}

		const FVector Eye = Pawn->GetPawnViewLocation();

		const FVector TrueDir = (TargetLoc - Eye).GetSafeNormal();
		if (AimErrorDegrees <= 0.f || TrueDir.IsNearlyZero())
		{
			return TargetLoc;
		}

		FRandomStream Stream(static_cast<int32>(GFrameCounter) ^ static_cast<int32>(GetTypeHash(&Controller)));
		const FVector JitteredDir = Stream.VRandCone(TrueDir, FMath::DegreesToRadians(AimErrorDegrees));

		// Eye + jittered direction at the true distance == TargetLocation + a cone-bounded offset.
		return Eye + JitteredDir * FVector::Dist(Eye, TargetLoc);
	}
}

////////////////////////////////////////////////////////////////////

bool FBNHasTargetCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	return GetBotTarget(Context, InstanceData.Controller) != nullptr;
}

#if WITH_EDITOR
FText FBNHasTargetCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Has Target</b>");
}
#endif

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FBNFaceTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	AActor* Target = Bot ? Bot->GetCurrentTarget() : nullptr;
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	// SetFocus/SetFocalPoint are still recorded so anything else reading the controller's focus
	// sees the truth — but they are NOT what turns the bot. SteerControlRotation is.
	if (InstanceData.AimErrorDegrees <= 0.f)
	{
		Bot->SetFocus(Target);
		InstanceData.AimPoint = Target->GetActorLocation();
	}
	else
	{
		InstanceData.AimPoint = JitteredFocalPoint(*Bot, *Target, InstanceData.AimErrorDegrees);
		Bot->SetFocalPoint(InstanceData.AimPoint);
	}

	InstanceData.SecondsUntilReaim = InstanceData.ReaimSeconds;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNFaceTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	AActor* Target = Bot ? Bot->GetCurrentTarget() : nullptr;
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	// R10 — THE TIER'S AIM, unless this state pinned its own. A negative authored value means
	// "ask the tier"; zero still means hitscan-perfect, and a positive value is a deliberate
	// per-state override the tree gets to keep.
	const FBNBotTuningRow& Tuning = Bot->GetTuning();
	const float AimError = InstanceData.AimErrorDegrees < 0.f ? Tuning.AimErrorDegrees : InstanceData.AimErrorDegrees;
	const float Reaim = InstanceData.AimErrorDegrees < 0.f ? Tuning.ReaimSeconds : InstanceData.ReaimSeconds;

	if (AimError > 0.f)
	{
		InstanceData.SecondsUntilReaim -= DeltaTime;
		if (InstanceData.SecondsUntilReaim <= 0.f)
		{
			InstanceData.AimPoint = JitteredFocalPoint(*Bot, *Target, AimError);
			Bot->SetFocalPoint(InstanceData.AimPoint);
			InstanceData.SecondsUntilReaim = Reaim;
		}
	}
	else
	{
		// Exact aim tracks the actor, so the point must be refreshed as the target moves.
		InstanceData.AimPoint = Target->GetActorLocation();
	}

	// The line that actually turns the bot — and therefore the line that decides whether its
	// bullets go anywhere near the target.
	SteerControlRotation(*Bot, InstanceData.AimPoint, InstanceData.TurnDegreesPerSecond, DeltaTime);

	return EStateTreeRunStatus::Running;
}

void FBNFaceTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->ClearFocus(EAIFocusPriority::Gameplay);
	}
}

#if WITH_EDITOR
FText FBNFaceTargetTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Face Target</b>");
}
#endif

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FBNMoveToTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	AActor* Target = Bot ? Bot->GetCurrentTarget() : nullptr;
	if (!Target || !Bot->GetPawn())
	{
		return EStateTreeRunStatus::Failed;
	}

	const bool bAlreadyInRadius =
		FVector::Dist(Bot->GetPawn()->GetActorLocation(), Target->GetActorLocation()) <= InstanceData.AcceptanceRadius;

	InstanceData.SecondsUntilRepath = FMath::Max(0.1f, InstanceData.RepathIntervalSeconds);
	InstanceData.BestDistance = FVector::Dist(Bot->GetPawn()->GetActorLocation(), Target->GetActorLocation());
	InstanceData.SecondsWithoutProgress = 0.f;

	const EPathFollowingRequestResult::Type Result = Bot->MoveToActor(Target, InstanceData.AcceptanceRadius);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		ReportMoveFailure(*Bot, Target, TEXT("move to target"), InstanceData.bWarnedMoveFailed);
		return EStateTreeRunStatus::Failed;
	}

	// AlreadyAtGoal while still OUTSIDE the firing radius is the unreachable-target signature.
	// MoveToActor defaults to bAllowPartialPath, so an enemy on a ledge yields a partial path to
	// the nearest reachable point — and when the bot already stands on that point, the honest
	// answer "no path" arrives disguised as the cheerful "you have arrived".
	if (Result == EPathFollowingRequestResult::AlreadyAtGoal && !bAlreadyInRadius)
	{
		Bot->NotifyTargetUnreachable(Target);
		return EStateTreeRunStatus::Failed;
	}

	// AlreadyAtGoal falls through: the first Tick's radius check answers Succeeded.
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNMoveToTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const AActor* Target = Bot ? Bot->GetCurrentTarget() : nullptr;
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	const APawn* Pawn = Bot->GetPawn();
	const bool bInRadius = Pawn && FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation()) <= InstanceData.AcceptanceRadius;
	const bool bMoveDone = Bot->GetMoveStatus() == EPathFollowingStatus::Idle;

	// "Arrived" is a FIRING POSITION, not a distance: in range with the target actually visible.
	// Range alone succeeds behind a wall, and the burst that follows hits masonry. When the path
	// is done but sight is not, keep closing — MoveToActor re-issued from a stopped state is how
	// the bot walks out from behind its own cover instead of standing there shooting it.
	if (bInRadius && Bot->HasLineOfSightToTarget())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// No-progress watchdog. AlreadyAtGoal catches the "no path exists" shape; this catches the
	// other one — a path exists, path following reports Moving, and the bot still gets nowhere
	// because it is wedged on geometry or circling a pillar. Both end the same way: give up on
	// this target rather than spend the match failing to reach it.
	const float DistanceNow = Pawn ? FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation()) : 0.f;
	if (DistanceNow < InstanceData.BestDistance - 50.f)
	{
		InstanceData.BestDistance = DistanceNow;
		InstanceData.SecondsWithoutProgress = 0.f;
		// Moving again: the next wedge gets its own jump.
		InstanceData.bTriedWedgeJump = false;
	}
	else
	{
		InstanceData.SecondsWithoutProgress += DeltaTime;

		// R9.5 — SPEND A JUMP BEFORE WRITING THE TARGET OFF. A lip, a crate, a step: a path
		// exists, path following says Moving, and the bot gets nowhere. That is the shape a jump
		// clears, and it costs half the give-up window to find out. One attempt per wedge.
		if (!InstanceData.bTriedWedgeJump
			&& InstanceData.SecondsWithoutProgress >= InstanceData.GiveUpAfterNoProgressSeconds * 0.5f)
		{
			InstanceData.bTriedWedgeJump = true;
			if (Bot->TryJump())
			{
				UE_LOG(LogBN, Verbose, TEXT("BNBots: %s jumped to clear whatever it is wedged on."),
					*GetNameSafe(Pawn));
			}
		}

		if (InstanceData.SecondsWithoutProgress >= InstanceData.GiveUpAfterNoProgressSeconds)
		{
			Bot->NotifyTargetUnreachable(const_cast<AActor*>(Target));
			return EStateTreeRunStatus::Failed;
		}
	}

	if (Pawn)
	{
		ReportLocomotion(*Pawn, TEXT("closing"), DeltaTime, InstanceData.SecondsUntilLocomotionLog);
	}

	InstanceData.SecondsUntilRepath -= DeltaTime;

	// Diagnostic, ~1/sec while closing. A bot that neither arrives nor moves is the hardest
	// failure to read from outside, because every individual piece reports success: the move
	// request is accepted, the state is entered, the task returns Running. This line prints the
	// four numbers that separate "walking" from "path following thinks it is walking".
	InstanceData.SecondsUntilCloseLog -= DeltaTime;
	if (InstanceData.SecondsUntilCloseLog <= 0.f)
	{
		InstanceData.SecondsUntilCloseLog = 1.f;
		UE_LOG(LogBN, Log, TEXT("BNBots: %s closing on %s — dist %.0f (radius %.0f) los=%s movestatus=%d speed=%.0f"),
			*GetNameSafe(Pawn), *GetNameSafe(Target),
			Pawn ? FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation()) : -1.f,
			InstanceData.AcceptanceRadius,
			Bot->HasLineOfSightToTarget() ? TEXT("yes") : TEXT("no"),
			static_cast<int32>(Bot->GetMoveStatus()),
			Pawn ? Pawn->GetVelocity().Size2D() : -1.f);
	}

	if (bMoveDone)
	{
		if (InstanceData.SecondsUntilRepath > 0.f)
		{
			// Inside the throttle window: stand, do not re-ask. The bot is already as close as
			// the last path could bring it.
			return EStateTreeRunStatus::Running;
		}

		// No sight and nowhere left on the old path: ask for a fresh one to where the target is
		// NOW, and to a TIGHTER radius than the firing distance — re-requesting at the same
		// radius the bot is already inside answers AlreadyAtGoal and moves nobody. Failed means
		// unreachable, and the state's transition delay throttles the retry from there.
		const float CloseRadius = FMath::Min(InstanceData.AcceptanceRadius * 0.5f, 200.f);
		InstanceData.SecondsUntilRepath = FMath::Max(0.1f, InstanceData.RepathIntervalSeconds);
		const EPathFollowingRequestResult::Type Result = Bot->MoveToActor(const_cast<AActor*>(Target), CloseRadius);
		if (Result == EPathFollowingRequestResult::Failed)
		{
			ReportMoveFailure(*Bot, Target, TEXT("repath to target"), InstanceData.bWarnedMoveFailed);
			return EStateTreeRunStatus::Failed;
		}
		if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			// Still outside the firing radius (checked above) yet told we have arrived: the goal
			// is unreachable and re-asking every half second forever is the deadlock this fixes.
			Bot->NotifyTargetUnreachable(const_cast<AActor*>(Target));
			return EStateTreeRunStatus::Failed;
		}
	}

	return EStateTreeRunStatus::Running;
}

void FBNMoveToTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->StopMovement();
	}
}

#if WITH_EDITOR
FText FBNMoveToTargetTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Move To Target</b>");
}
#endif

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FBNFireBurstTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}

	const ABNPlayerState* PS = Bot->GetPlayerState<ABNPlayerState>();
	const UBNAbilitySystemComponent* ASC = PS ? PS->GetBNAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Frozen: the ASC would refuse the activation anyway — failing here just stops futile presses.
	if (ASC->HasMatchingGameplayTag(BNTags::State_Match_Frozen))
	{
		return EStateTreeRunStatus::Failed;
	}

	// An empty magazine is not a burst. BNGA_Fire refuses it anyway; failing HERE is what hands
	// control back to the tree so the reload/swap branch gets its turn instead of the trigger
	// being held down on a dry weapon for BurstSeconds.
	const ABNWeapon* Weapon = Bot->GetCurrentWeapon();
	if (!Weapon || !Weapon->HasAmmo())
	{
		return EStateTreeRunStatus::Failed;
	}

	// Do not shoot what cannot be seen — the sight sense remembers targets through walls.
	if (!Bot->HasLineOfSightToTarget())
	{
		return EStateTreeRunStatus::Failed;
	}

	Bot->PressInputTag(BNTags::Input_Weapon_Fire);
	InstanceData.SecondsRemaining = InstanceData.BurstSeconds;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNFireBurstTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}

	// The burst ends EARLY on a dry magazine or a lost sightline — both are the tree's cue to
	// re-select (reload, swap, or chase). Succeeded, not Failed: the burst did its job, and
	// Failed would take the Engage state's 1.0s penalty delay for an ordinary empty magazine.
	const ABNWeapon* Weapon = Bot->GetCurrentWeapon();
	if (!Weapon || !Weapon->HasAmmo() || !Bot->HasLineOfSightToTarget())
	{
		Bot->ReleaseInputTag(BNTags::Input_Weapon_Fire);
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.SecondsRemaining -= DeltaTime;
	if (InstanceData.SecondsRemaining <= 0.f)
	{
		Bot->ReleaseInputTag(BNTags::Input_Weapon_Fire);
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FBNFireBurstTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// ALWAYS released: a task interrupted mid-burst must not leave the trigger held. A release
	// after Tick already released, or after a refused EnterState, is a harmless no-op on the ASC.
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->ReleaseInputTag(BNTags::Input_Weapon_Fire);
	}
}

#if WITH_EDITOR
FText FBNFireBurstTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Fire Burst</b>");
}
#endif

////////////////////////////////////////////////////////////////////

bool FBNIncomingBlastCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Bot || !Pawn)
	{
		return false;
	}

	FVector Center;
	float Radius = 0.f;
	if (!Bot->HasIncomingBlast(Center, Radius))
	{
		return false;
	}

	// Already clear of it: a bot that walked out on its own must not re-enter this state and run
	// further for no reason. The warning stays live for anyone still inside.
	return FVector::Dist(Pawn->GetActorLocation(), Center) <= Radius;
}

#if WITH_EDITOR
FText FBNIncomingBlastCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Incoming Blast</b>");
}
#endif

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FBNEvadeBlastTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	UWorld* World = Bot ? Bot->GetWorld() : nullptr;
	if (!Bot || !Pawn || !World)
	{
		return EStateTreeRunStatus::Failed;
	}

	FVector Center;
	float Radius = 0.f;
	if (!Bot->HasIncomingBlast(Center, Radius))
	{
		return EStateTreeRunStatus::Failed;
	}

	// STRAIGHT AWAY, flat. The direction a player picks without thinking, and the only one that is
	// right regardless of geometry: every other bearing out of a circle is longer.
	FVector Away = Pawn->GetActorLocation() - Center;
	Away.Z = 0.f;
	if (!Away.Normalize())
	{
		// Standing exactly on it. Any direction is equally good and none is better, so take the
		// bot's own facing rather than a random draw the determinism law would object to.
		Away = Pawn->GetActorForwardVector().GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			return EStateTreeRunStatus::Failed;
		}
	}

	const float RunDistance = Radius + FMath::Max(0.f, InstanceData.ClearMargin);
	const FVector Wanted = Center + Away * RunDistance;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return EStateTreeRunStatus::Failed;
	}

	FNavLocation Projected;
	if (!NavSys->ProjectPointToNavigation(Wanted, Projected, FVector(300.f, 300.f, 400.f)))
	{
		// Nowhere to run — cornered by a grenade, which happens and has no better answer than
		// carrying on with whatever the bot was doing.
		UE_LOG(LogBN, Verbose, TEXT("BNBots: %s is cornered by a grenade and cannot get clear."),
			*GetNameSafe(Pawn));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.EvadePoint = Projected.Location;
	if (Bot->MoveToLocation(InstanceData.EvadePoint, /*AcceptanceRadius=*/50.f) == EPathFollowingRequestResult::Failed)
	{
		return EStateTreeRunStatus::Failed;
	}
	InstanceData.bMoving = true;

	// A jump on the way out: it is what the cooldown is for, it covers ground the walk does not,
	// and it is the read a player recognises instantly as "it saw that coming".
	Bot->TryJump();

	UE_LOG(LogBN, Verbose, TEXT("BNBots: %s is diving away from a grenade."), *GetNameSafe(Pawn));
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNEvadeBlastTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Bot || !Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	FVector Center;
	float Radius = 0.f;
	if (!Bot->HasIncomingBlast(Center, Radius))
	{
		// It went off, or the window passed. Either way there is nothing left to run from, and
		// succeeding sends the bot straight back to the fight.
		return EStateTreeRunStatus::Succeeded;
	}

	// CLEAR IS ENOUGH. A bot that only had to take two steps goes back to fighting rather than
	// running the whole leg — the difference between reacting and fleeing.
	if (FVector::Dist(Pawn->GetActorLocation(), Center) > Radius)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// Still inside and the path has given up: nothing more this task can do, and holding the
	// state would only keep the bot standing in the blast looking busy.
	if (Bot->GetMoveStatus() == EPathFollowingStatus::Idle && InstanceData.bMoving)
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void FBNEvadeBlastTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bMoving = false;
	if (ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->StopMovement();
	}
}

#if WITH_EDITOR
FText FBNEvadeBlastTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Evade Blast</b>");
}
#endif

////////////////////////////////////////////////////////////////////

bool FBNShouldTakeCoverCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot || !Bot->GetCurrentTarget())
	{
		return false;
	}

	// Still spending the last break's window. Asked FIRST because it is the cheapest of the three
	// and because it is the one that stops a bot ping-ponging between cover and the fight.
	if (!Bot->CanTakeCoverNow())
	{
		return false;
	}

	if (Bot->GetHealthNorm() >= InstanceData.HealthBelow)
	{
		return false;
	}

	// UNDER FIRE RIGHT NOW, not hurt at some point in the past. State.Combat.RecentDamage is the
	// window the shield dance already applies on every landed hit, so this costs nothing new and
	// means exactly what it says.
	const ABNPlayerState* PS = Bot->GetPlayerState<ABNPlayerState>();
	const UBNAbilitySystemComponent* ASC = PS ? PS->GetBNAbilitySystemComponent() : nullptr;
	return ASC && ASC->HasMatchingGameplayTag(BNTags::State_Combat_RecentDamage);
}

#if WITH_EDITOR
FText FBNShouldTakeCoverCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Should Take Cover</b>");
}
#endif

////////////////////////////////////////////////////////////////////

namespace
{
	/** Eye height for a line-of-sight question — the same height a shot leaves from, near enough,
	 *  and far more honest than tracing from the navmesh at ankle level where everything is cover. */
	constexpr float BNCoverEyeHeight = 60.f;

	/**
	 * The rosette: SampleCount directions around the bot, each projected onto the navmesh, each
	 * traced back at the threat. The first BLOCKED one closest to the bot wins.
	 *
	 * Closest, not farthest: cover is wanted NOW, and a bot that crosses the arena to a better
	 * wall spends the whole trip being shot in the back.
	 */
	bool BNFindCoverPoint(const ABNBotController& Bot, const APawn& Pawn, const AActor& Threat,
		float SearchRadius, int32 SampleCount, FVector& OutPoint)
	{
		// NON-const UWorld*: FNavigationSystem::GetCurrent takes one, and the proven call in
		// ReportMoveFailure above is the shape to copy rather than re-derive.
		UWorld* World = Bot.GetWorld();
		UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
		if (!World || !NavSys)
		{
			return false;
		}

		const FVector Origin = Pawn.GetActorLocation();
		const FVector ThreatEye = Threat.GetActorLocation() + FVector(0.f, 0.f, BNCoverEyeHeight);
		const int32 Samples = FMath::Max(3, SampleCount);
		const float Radius = FMath::Max(100.f, SearchRadius);

		FCollisionQueryParams QueryParams(FName(TEXT("BNCoverTrace")), /*bTraceComplex=*/false, &Pawn);
		QueryParams.AddIgnoredActor(&Threat);

		bool bFound = false;
		float BestDistSq = TNumericLimits<float>::Max();

		for (int32 i = 0; i < Samples; ++i)
		{
			const float Angle = (2.f * PI * static_cast<float>(i)) / static_cast<float>(Samples);
			const FVector Candidate = Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;

			FNavLocation Projected;
			if (!NavSys->ProjectPointToNavigation(Candidate, Projected, FVector(200.f, 200.f, 300.f)))
			{
				continue;
			}

			// THE question, asked with the geometry the bullets use: can a shot from where the
			// threat is standing reach a bot standing here? A blocking hit means no — that is
			// cover, by the only definition that matters to something being shot at.
			const FVector CandidateEye = Projected.Location + FVector(0.f, 0.f, BNCoverEyeHeight);
			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(
				Hit, CandidateEye, ThreatEye, BNCollision::WeaponTrace, QueryParams);
			if (!bBlocked)
			{
				continue;
			}

			const float DistSq = FVector::DistSquared(Origin, Projected.Location);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutPoint = Projected.Location;
				bFound = true;
			}
		}

		return bFound;
	}
}

EStateTreeRunStatus FBNTakeCoverTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	// The THREAT, not the current target: this task must keep working if the ambition flips to
	// Survive mid-break, and Survive blanks GetCurrentTarget by design.
	const AActor* Threat = Bot ? Bot->GetThreat() : nullptr;
	if (!Bot || !Pawn || !Threat)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!BNFindCoverPoint(*Bot, *Pawn, *Threat, InstanceData.SearchRadius, InstanceData.SampleCount, InstanceData.CoverPoint))
	{
		// NOWHERE TO HIDE is a real answer in an open arena, not a failure to report. Failing here
		// drops the tree straight through to Close/Shoot, which is the right thing to do when the
		// only option left is to fight.
		UE_LOG(LogBN, Verbose, TEXT("BNBots: %s wanted cover and found none — fighting instead."),
			*GetNameSafe(Pawn));
		return EStateTreeRunStatus::Failed;
	}

	if (Bot->MoveToLocation(InstanceData.CoverPoint, /*AcceptanceRadius=*/60.f) == EPathFollowingRequestResult::Failed)
	{
		return EStateTreeRunStatus::Failed;
	}

	// The cooldown is spent on the ATTEMPT, not on the arrival: a bot that fails to reach cover
	// must not immediately try again, which is the loop that would leave it walking in circles
	// under fire.
	Bot->NotifyTookCover();

	InstanceData.bArrived = false;
	InstanceData.HoldRemaining = InstanceData.HoldSeconds;
	UE_LOG(LogBN, Verbose, TEXT("BNBots: %s is breaking line of sight."), *GetNameSafe(Pawn));
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNTakeCoverTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Bot || !Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.bArrived)
	{
		const bool bInRadius = FVector::Dist(Pawn->GetActorLocation(), InstanceData.CoverPoint) <= 100.f;
		const bool bMoveDone = Bot->GetMoveStatus() == EPathFollowingStatus::Idle;
		if (!bInRadius && !bMoveDone)
		{
			return EStateTreeRunStatus::Running;
		}
		InstanceData.bArrived = true;
		Bot->StopMovement();
	}

	// HOLD. The beat that makes this read as cover rather than as a pathing twitch — and the
	// window a shield would have used to recharge, the day shields come back on.
	InstanceData.HoldRemaining -= DeltaTime;
	return InstanceData.HoldRemaining > 0.f ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

void FBNTakeCoverTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->StopMovement();
	}
}

#if WITH_EDITOR
FText FBNTakeCoverTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Take Cover</b>");
}
#endif

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FBNStrafeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}

	// No global RNG (§5): identity decides which way this bot opens and every step after it just
	// flips, so two bots in one fight still open opposite ways without perturbing any stream.
	InstanceData.bStepRight = (GetTypeHash(Bot) & 1) != 0;
	// The first step is immediate — a burst that begins with a second of standing still is the
	// stationary bot this task exists to remove.
	InstanceData.SecondsUntilStep = 0.f;
	InstanceData.bWarnedStepFailed = false;
	InstanceData.StepCount = 0;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNStrafeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	const AActor* Target = Bot ? Bot->GetCurrentTarget() : nullptr;
	if (!Bot || !Pawn || !Target)
	{
		// No target is not this task's failure — Shoot's own conditions own that. Keep running so
		// the burst beside it decides the state, and simply stop stepping.
		return EStateTreeRunStatus::Running;
	}

	InstanceData.SecondsUntilStep -= DeltaTime;
	if (InstanceData.SecondsUntilStep > 0.f)
	{
		return EStateTreeRunStatus::Running;
	}
	// R10 — the tier's rhythm unless the state pinned one (negative = ask the tier).
	const FBNBotTuningRow& Tuning = Bot->GetTuning();
	const float Interval = InstanceData.StepIntervalSeconds < 0.f ? Tuning.StrafeIntervalSeconds : InstanceData.StepIntervalSeconds;
	const int32 JukeEvery = InstanceData.JukeEveryNthStep < 0 ? Tuning.JukeEveryNthStep : InstanceData.JukeEveryNthStep;
	InstanceData.SecondsUntilStep = FMath::Max(0.1f, Interval);

	// Perpendicular to the line of fire, flat: stepping toward or away from the target would be
	// closing or retreating, and those are Close's and Survive's jobs, not this one's.
	FVector ToTarget = Target->GetActorLocation() - Pawn->GetActorLocation();
	ToTarget.Z = 0.f;
	if (!ToTarget.Normalize())
	{
		return EStateTreeRunStatus::Running;
	}
	const FVector Lateral = FVector::CrossProduct(FVector::UpVector, ToTarget)
		* (InstanceData.bStepRight ? 1.f : -1.f);
	const FVector Destination = Pawn->GetActorLocation() + Lateral * FMath::Max(0.f, InstanceData.StepDistance);

	// Projected onto the navmesh by the move itself: a step into a wall or off a ledge resolves to
	// the nearest legal point instead of failing, and a genuinely impossible one flips the side.
	const EPathFollowingRequestResult::Type Result = Bot->MoveToLocation(
		Destination, /*AcceptanceRadius=*/50.f, /*bStopOnOverlap=*/true, /*bUsePathfinding=*/true,
		/*bProjectDestinationToNavigation=*/true, /*bCanStrafe=*/true);

	if (Result == EPathFollowingRequestResult::Failed)
	{
		// R9.5 — pinned against something mid-fight: a jump is the one move left that changes the
		// picture, and it is also what a player does when cornered. Cheap, because the cooldown
		// on the controller is what stops it becoming a pogo.
		Bot->TryJump();

		// Back to a wall: turn around rather than keep pressing into it. Flipping here AND at the
		// end of a good step is deliberate — a failed step costs a direction, not a beat.
		InstanceData.bStepRight = !InstanceData.bStepRight;
		if (!InstanceData.bWarnedStepFailed)
		{
			InstanceData.bWarnedStepFailed = true;
			UE_LOG(LogBN, Verbose, TEXT("BNBots: %s could not strafe — no navigable step either side of it."),
				*GetNameSafe(Pawn));
		}
		return EStateTreeRunStatus::Running;
	}

	// R9.5 — THE JUKE. Every Nth sidestep leaves the ground: a bot that never jumps in a firefight
	// is a bot that can be led by aim alone, and one that jumps every step cannot shoot. The
	// counter (not a coin) keeps the rhythm legible to the player who is fighting it.
	++InstanceData.StepCount;
	if (JukeEvery > 0 && (InstanceData.StepCount % JukeEvery) == 0)
	{
		Bot->TryJump();
	}

	InstanceData.bStepRight = !InstanceData.bStepRight;
	return EStateTreeRunStatus::Running;
}

void FBNStrafeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		// The next state issues its own move; a half-finished sidestep must not outlive the burst
		// that asked for it. Move priority only — BN Face Target owns the Gameplay-priority focus.
		Bot->StopMovement();
	}
}

#if WITH_EDITOR
FText FBNStrafeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Strafe</b>");
}
#endif

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FBNMoveToPointOfInterestTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Controller = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Nearest point that is not the last one; the last one only as a fallback when it is the
	// only point in the level (a one-POI map still roams rather than failing forever).
	// Survive flips the rule (R6 G2 2.4): FARTHEST from the threat instead of nearest to me —
	// same never-the-last-point law, same single-POI fallback.
	const ABNBotController* Bot = Controller;
	const AActor* Threat = (Bot && Bot->GetAmbition() == EBNBotAmbition::Survive) ? Bot->GetThreat() : nullptr;
	const bool bFarthest = Threat != nullptr;
	const FVector From = bFarthest ? Threat->GetActorLocation() : Pawn->GetActorLocation();
	ABNPointOfInterest* Best = nullptr;
	ABNPointOfInterest* BestAny = nullptr;
	float BestDistSq = bFarthest ? -1.f : TNumericLimits<float>::Max();
	float BestAnyDistSq = BestDistSq;
	for (TActorIterator<ABNPointOfInterest> It(World); It; ++It)
	{
		const float DistSq = FVector::DistSquared(From, It->GetActorLocation());
		if (bFarthest ? DistSq > BestAnyDistSq : DistSq < BestAnyDistSq)
		{
			BestAnyDistSq = DistSq;
			BestAny = *It;
		}
		if (*It != InstanceData.LastPoint.Get() && (bFarthest ? DistSq > BestDistSq : DistSq < BestDistSq))
		{
			BestDistSq = DistSq;
			Best = *It;
		}
	}

	ABNPointOfInterest* Pick = Best ? Best : BestAny;
	if (!Pick)
	{
		// Once, not per re-enter: the tree re-selects Roam after every failure, and a level with
		// no points would otherwise spin here in SILENCE — the exact failure shape §5c banned.
		if (!InstanceData.bWarnedNoPointsOfInterest)
		{
			InstanceData.bWarnedNoPointsOfInterest = true;
			UE_LOG(LogBN, Warning, TEXT("BNBots: no ABNPointOfInterest placed in this level — bots have nowhere to roam and will stand until they see a target."));
		}
		return EStateTreeRunStatus::Failed;
	}

	if (Controller->MoveToActor(Pick, Pick->Radius) == EPathFollowingRequestResult::Failed)
	{
		ReportMoveFailure(*Controller, Pick, TEXT("roam to point of interest"), InstanceData.bWarnedMoveFailed);
		return EStateTreeRunStatus::Failed;
	}

	// FACE THE GOAL BEFORE WALKING. Measured: 2 of 7 moving samples came back BACKWARD and one
	// STRAFE-LEFT, all of them at the START of a roam leg. The cause is arithmetic, not animation:
	// path following reaches 600 uu/s within a few frames, while the control rotation turns at a
	// bounded rate, so a point chosen BEHIND the bot is walked toward backwards for most of a
	// second — a moonwalk, which is the sliding the founder saw. Setting the heading here, before
	// there is any velocity to disagree with, removes the mismatch at its source instead of
	// trying to out-run it with a faster turn.
	if (const APawn* MyPawn = Controller->GetPawn())
	{
		const FVector ToPick = Pick->GetActorLocation() - MyPawn->GetActorLocation();
		if (!ToPick.IsNearlyZero())
		{
			FRotator Heading = Controller->GetControlRotation();
			Heading.Yaw = ToPick.Rotation().Yaw;
			Controller->SetControlRotation(Heading);
			if (APawn* MutablePawn = Controller->GetPawn())
			{
				MutablePawn->FaceRotation(Heading, 0.f);
			}
		}
	}

	InstanceData.CurrentPoint = Pick;
	InstanceData.bArrived = false;
	InstanceData.bTriedBlockedJump = false;
	InstanceData.DwellRemaining = InstanceData.DwellSeconds;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNMoveToPointOfInterestTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	// A target appearing mid-walk is deliberately NOT handled here: the tree's Engage state
	// preempts this one through FBNHasTargetCondition.
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* Controller = ResolveBot(Context, InstanceData.Controller);
	const ABNPointOfInterest* Point = InstanceData.CurrentPoint.Get();
	if (!Controller || !Point)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.bArrived)
	{
		const APawn* Pawn = Controller->GetPawn();

		// FACE THE WALK. ABNCharacter is an FPS pawn: bUseControllerRotationYaw is true and
		// bOrientRotationToMovement is false, so its yaw comes from the CONTROL rotation and from
		// nowhere else. A player turns that with the mouse every frame. A roaming bot has no
		// focus at all — Engage's BN Face Target cleared it on the way out — so the control
		// rotation just holds its last value while path following translates the capsule. The
		// result on screen is a character gliding sideways in an idle pose: the founder's
		// "not walking, just sliding".
		//
		// Steering the focal point down the ACTUAL velocity (not at the destination, which a
		// curving path would make a lie) turns the control rotation, which turns the pawn, which
		// gives the locomotion graph a forward-facing walk to play.
		if (Pawn)
		{
			const FVector Velocity = Pawn->GetVelocity();
			if (Velocity.SizeSquared2D() > FMath::Square(10.f))
			{
				const FVector Ahead = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal() * 500.f;
				const FVector LookAt = Pawn->GetPawnViewLocation() + Ahead;
				Controller->SetFocalPoint(LookAt, EAIFocusPriority::Move);
				SteerControlRotation(*Controller, LookAt, InstanceData.TurnDegreesPerSecond, DeltaTime);
			}
			ReportLocomotion(*Pawn, TEXT("roaming"), DeltaTime, InstanceData.SecondsUntilLocomotionLog);
		}

		const bool bInRadius = Pawn && FVector::Dist(Pawn->GetActorLocation(), Point->GetActorLocation()) <= Point->Radius;
		const bool bMoveDone = Controller->GetMoveStatus() == EPathFollowingStatus::Idle;

		// R9.5 — STOPPED SHORT. Path following says done and the bot is not there: a lip, a step,
		// the edge of a dip it walked into. Before accepting that as "arrived" (which is what
		// silently gave up on the point), spend ONE jump and re-issue the move — this is the
		// "get up there" and "get out of here" case, and it is the same move a player makes
		// without thinking. If the second attempt stalls too, arrival stands and the bot picks a
		// different point rather than fighting the geometry.
		if (bMoveDone && !bInRadius && !InstanceData.bTriedBlockedJump)
		{
			InstanceData.bTriedBlockedJump = true;
			if (ABNBotController* JumpingBot = Cast<ABNBotController>(Controller))
			{
				if (JumpingBot->TryJump())
				{
					UE_LOG(LogBN, Verbose, TEXT("BNBots: %s stopped short of its point and jumped for it."),
						*GetNameSafe(Pawn));
					Controller->MoveToActor(const_cast<ABNPointOfInterest*>(Point), Point->Radius);
					return EStateTreeRunStatus::Running;
				}
			}
		}

		if (bInRadius || bMoveDone)
		{
			InstanceData.bArrived = true;
			Controller->StopMovement();
		}
		return EStateTreeRunStatus::Running;
	}

	InstanceData.DwellRemaining -= DeltaTime;
	if (InstanceData.DwellRemaining <= 0.f)
	{
		InstanceData.LastPoint = InstanceData.CurrentPoint;
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FBNMoveToPointOfInterestTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->StopMovement();
		// Move priority only — clearing it must never disturb the Gameplay-priority focus that
		// BN Face Target owns while engaging.
		Bot->ClearFocus(EAIFocusPriority::Move);
	}
}

#if WITH_EDITOR
FText FBNMoveToPointOfInterestTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Move To Point Of Interest</b>");
}
#endif

////////////////////////////////////////////////////////////////////

bool FBNHasLineOfSightCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	return Bot && Bot->HasLineOfSightToTarget();
}

#if WITH_EDITOR
FText FBNHasLineOfSightCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Has Line Of Sight</b>");
}
#endif

////////////////////////////////////////////////////////////////////

bool FBNNeedsReloadCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const ABNWeapon* Weapon = Bot ? Bot->GetCurrentWeapon() : nullptr;
	if (!Weapon)
	{
		return false;
	}

	// No reserve, no reload — the swap branch owns that case. Saying "needs reload" here would
	// send the bot into a reload the ability refuses, forever.
	if (Weapon->GetAmmoReserve() <= 0)
	{
		return false;
	}

	const int32 MagazineSize = Weapon->GetMagazineSize();
	if (MagazineSize <= 0)
	{
		// A weapon with no magazine (melee, or an unresolved row) never reloads.
		return false;
	}

	const float Threshold = FMath::Clamp(InstanceData.ReloadAtMagazineFraction, 0.f, 1.f) * MagazineSize;
	return static_cast<float>(Weapon->GetCurrentAmmo()) <= Threshold;
}

#if WITH_EDITOR
FText FBNNeedsReloadCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Needs Reload</b>");
}
#endif

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FBNReloadTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const ABNWeapon* Weapon = Bot ? Bot->GetCurrentWeapon() : nullptr;
	if (!Weapon)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.AmmoAtStart = Weapon->GetCurrentAmmo();
	InstanceData.SecondsElapsed = 0.f;

	// The human's R key is one tap: press activates, release clears the held flag so the next
	// reload is a fresh press rather than an input the ASC still believes is down.
	Bot->PressInputTag(BNTags::Input_Weapon_Reload);
	Bot->ReleaseInputTag(BNTags::Input_Weapon_Reload);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNReloadTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const ABNWeapon* Weapon = Bot ? Bot->GetCurrentWeapon() : nullptr;
	if (!Weapon)
	{
		// Swapped or died mid-reload. Not this task's problem to fix, and not a failure worth a
		// penalty delay — the tree re-selects on the next frame with the truth.
		return EStateTreeRunStatus::Succeeded;
	}

	// The rounds landing IS the completion signal: BNGA_Reload refills from a montage notify, so
	// the duration is animation data and any number this task assumed would be a lie.
	if (Weapon->GetCurrentAmmo() > InstanceData.AmmoAtStart)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.SecondsElapsed += DeltaTime;
	if (InstanceData.SecondsElapsed >= InstanceData.TimeoutSeconds)
	{
		// A refused reload (dead, frozen, no montage) must not park the tree here. Failed sends
		// the bot back through selection under the state's transition delay.
		UE_LOG(LogBN, Verbose, TEXT("BNBots: reload timed out after %.1fs with %d in the magazine — the ability refused or never notified."),
			InstanceData.SecondsElapsed, Weapon->GetCurrentAmmo());
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FBNReloadTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Reload</b>");
}
#endif

////////////////////////////////////////////////////////////////////

namespace
{
	/** Can this weapon put rounds downrange, now or after a reload? The swap's whole question. */
	bool WeaponCanFight(const ABNWeapon* Weapon)
	{
		return Weapon && (Weapon->HasAmmo() || Weapon->GetAmmoReserve() > 0);
	}
}

EStateTreeRunStatus FBNSelectWeaponTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.SwapsMade = 0;
	InstanceData.SecondsUntilNextSwap = 0.f;

	// Already holding something that can fight: nothing to change. The common case costs no swap.
	if (WeaponCanFight(Bot->GetCurrentWeapon()))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNSelectWeaponTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (WeaponCanFight(Bot->GetCurrentWeapon()))
	{
		UE_LOG(LogBN, Verbose, TEXT("BNBots: swapped to %s after %d press(es)."),
			*GetNameSafe(Bot->GetCurrentWeapon()), InstanceData.SwapsMade);
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.SecondsUntilNextSwap -= DeltaTime;
	if (InstanceData.SecondsUntilNextSwap > 0.f)
	{
		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.SwapsMade >= FMath::Max(1, InstanceData.MaxSwaps))
	{
		// The whole loadout is dry. Failing is the honest answer: it is the tree's cue to stop
		// trying to shoot, not a reason to keep cycling weapons for the rest of the match.
		UE_LOG(LogBN, Verbose, TEXT("BNBots: no carried weapon has ammo after %d swap(s)."), InstanceData.SwapsMade);
		return EStateTreeRunStatus::Failed;
	}

	// The same button a human's mouse wheel presses — BNGA_Equip owns the swap, montage included.
	Bot->PressInputTag(BNTags::Input_Weapon_Next);
	Bot->ReleaseInputTag(BNTags::Input_Weapon_Next);
	++InstanceData.SwapsMade;
	// The equip is an ability with a montage; pressing again next frame would cancel it and the
	// weapon would never actually change. This pause is what makes the swap take.
	InstanceData.SecondsUntilNextSwap = FMath::Max(0.05f, InstanceData.SecondsBetweenSwaps);
	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FBNSelectWeaponTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Select Weapon</b>");
}
#endif

////////////////////////////////////////////////////////////////////

bool FBNReactedCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	return Bot && Bot->HasReactedToTarget();
}

#if WITH_EDITOR
FText FBNReactedCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Reacted</b>");
}
#endif

////////////////////////////////////////////////////////////////////

bool FBNInMeleeRangeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const AActor* Target = Bot ? Bot->GetCurrentTarget() : nullptr;
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Target || !Pawn)
	{
		return false;
	}

	// The reach comes from the HELD WEAPON's row — the same number BNGA_Melee resolves. Restating
	// it as a tree parameter would be a second source of truth for one distance, and law 3 keeps
	// tuning numbers in the table rather than scattered across two systems.
	const ABNWeapon* Weapon = Bot->GetCurrentWeapon();
	const FBNWeaponRow* Row = Weapon ? Weapon->GetRow() : nullptr;
	if (!Row || Row->MeleeRange <= 0.f)
	{
		return false;
	}

	const float Commit = Row->MeleeRange * FMath::Clamp(InstanceData.RangeFraction, 0.1f, 1.f);
	return FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation()) <= Commit;
}

#if WITH_EDITOR
FText FBNInMeleeRangeCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN In Melee Range</b>");
}
#endif

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FBNMeleeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.SecondsElapsed = 0.f;

	// One tap, exactly as a human's melee key: press activates, release clears the held flag so
	// the next swing is a fresh press rather than an input the ASC still believes is down.
	Bot->PressInputTag(BNTags::Input_Melee);
	Bot->ReleaseInputTag(BNTags::Input_Melee);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNMeleeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}

	// The swing's real length is montage data, so this waits rather than assuming. Succeeded on
	// timeout, not Failed: a refused melee (dead, frozen, no montage) is an ordinary outcome the
	// tree should re-select from immediately, not one that earns the Engage penalty delay.
	InstanceData.SecondsElapsed += DeltaTime;
	return InstanceData.SecondsElapsed >= InstanceData.TimeoutSeconds
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FBNMeleeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Melee</b>");
}
#endif

////////////////////////////////////////////////////////////////////

bool FBNHasLastKnownCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	return Bot && Bot->HasFreshLastKnownLocation();
}

#if WITH_EDITOR
FText FBNHasLastKnownCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Has Last Known</b>");
}
#endif

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FBNSearchLastKnownTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot || !Bot->HasFreshLastKnownLocation())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bArrived = false;
	InstanceData.LookAroundRemaining = InstanceData.LookAroundSeconds;
	InstanceData.SweptDegrees = 0.f;

	// Same reason as Roam: face the destination before the walk starts, so the first second is a
	// forward walk rather than a backpedal.
	if (const APawn* MyPawn = Bot->GetPawn())
	{
		const FVector ToSpot = Bot->GetLastKnownThreatLocation() - MyPawn->GetActorLocation();
		if (!ToSpot.IsNearlyZero())
		{
			FRotator Heading = Bot->GetControlRotation();
			Heading.Yaw = ToSpot.Rotation().Yaw;
			Bot->SetControlRotation(Heading);
			if (APawn* MutablePawn = Bot->GetPawn())
			{
				MutablePawn->FaceRotation(Heading, 0.f);
			}
		}
	}

	if (Bot->MoveToLocation(Bot->GetLastKnownThreatLocation(), InstanceData.AcceptanceRadius) == EPathFollowingRequestResult::Failed)
	{
		// The spot may be somewhere the bot cannot stand — a ledge it was shot from. Not worth a
		// warning: failing hands straight back to Roam, which is the right answer anyway.
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNSearchLastKnownTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	// A target appearing mid-search needs no handling here: Engage sits above this state and
	// preempts it through BN Has Target, exactly as it preempts Roam.

	if (!InstanceData.bArrived)
	{
		// Face the walk, for the same reason Roam does — the pawn's yaw IS the control rotation.
		const FVector Velocity = Pawn->GetVelocity();
		if (Velocity.SizeSquared2D() > FMath::Square(10.f))
		{
			const FVector Ahead = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal() * 500.f;
			SteerControlRotation(*Bot, Pawn->GetPawnViewLocation() + Ahead, 180.f, DeltaTime);
		}

		const bool bInRadius = FVector::Dist2D(Pawn->GetActorLocation(), Bot->GetLastKnownThreatLocation()) <= InstanceData.AcceptanceRadius;
		if (bInRadius || Bot->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			InstanceData.bArrived = true;
			Bot->StopMovement();
		}
		return EStateTreeRunStatus::Running;
	}

	// Arrived: sweep the view instead of standing frozen. This is the readable beat — a player
	// watching from cover sees the bot arrive, look around, and give up, and reads a mind.
	const float Step = InstanceData.SweepDegreesPerSecond * DeltaTime;
	InstanceData.SweptDegrees += Step;
	FRotator Sweep = Bot->GetControlRotation();
	Sweep.Yaw += Step;
	Bot->SetControlRotation(Sweep);
	if (APawn* MutablePawn = Bot->GetPawn())
	{
		MutablePawn->FaceRotation(Sweep, DeltaTime);
	}

	InstanceData.LookAroundRemaining -= DeltaTime;
	return InstanceData.LookAroundRemaining <= 0.f
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

void FBNSearchLastKnownTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->StopMovement();
	}
}

#if WITH_EDITOR
FText FBNSearchLastKnownTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Search Last Known</b>");
}
#endif

////////////////////////////////////////////////////////////////////

bool FBNCanThrowGrenadeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const AActor* Target = Bot ? Bot->GetCurrentTarget() : nullptr;
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Target || !Pawn)
	{
		return false;
	}

	// Not through a wall, and not into one: the same sight rule the burst uses.
	if (!Bot->HasLineOfSightToTarget())
	{
		return false;
	}

	const float Distance = FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation());
	if (Distance < InstanceData.MinRange || Distance > InstanceData.MaxRange)
	{
		return false;
	}

	// THE COOLDOWN. BNGA_Grenade holds Cooldown.Grenade for its whole window and the ASC refuses
	// the activation while it is held. Asking here is what keeps the bot from pressing a dead
	// button every frame — the ability's own refusal would be correct but silent-ish and noisy.
	const ABNPlayerState* PS = Bot->GetPlayerState<ABNPlayerState>();
	const UBNAbilitySystemComponent* ASC = PS ? PS->GetBNAbilitySystemComponent() : nullptr;
	if (!ASC || ASC->HasMatchingGameplayTag(BNTags::Cooldown_Grenade))
	{
		return false;
	}

	// R7.4 — AND THE POUCH, for the same reason the cooldown is asked here rather than left to the
	// ability: grenades can now run out, and an empty bot would otherwise enter this state, press a
	// key the cost refuses, and spend the task's whole timeout NOT SHOOTING — every time it had
	// line of sight at the right range. The refusal is silent; this check is what keeps the bot
	// fighting instead of miming a throw.
	if (ASC->GetNumericAttribute(UBNAttributeSet::GetGrenadesAttribute()) < 1.f)
	{
		return false;
	}

	// Frozen: the ASC would refuse anyway. Same reason the burst checks it.
	return !ASC->HasMatchingGameplayTag(BNTags::State_Match_Frozen);
}

#if WITH_EDITOR
FText FBNCanThrowGrenadeCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Can Throw Grenade</b>");
}
#endif

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FBNThrowGrenadeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.SecondsElapsed = 0.f;

	// One tap, exactly as a human's grenade key.
	Bot->PressInputTag(BNTags::Input_Grenade);
	Bot->ReleaseInputTag(BNTags::Input_Grenade);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNThrowGrenadeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const ABNBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}

	// The cooldown tag APPEARING is the proof the ability actually activated — a refused press
	// leaves it absent, and waiting on a montage this task cannot see would be a guess.
	const ABNPlayerState* PS = Bot->GetPlayerState<ABNPlayerState>();
	const UBNAbilitySystemComponent* ASC = PS ? PS->GetBNAbilitySystemComponent() : nullptr;
	if (ASC && ASC->HasMatchingGameplayTag(BNTags::Cooldown_Grenade))
	{
		UE_LOG(LogBN, Log, TEXT("BNBots: %s threw a grenade."), *GetNameSafe(Bot->GetPawn()));
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.SecondsElapsed += DeltaTime;
	if (InstanceData.SecondsElapsed >= InstanceData.TimeoutSeconds)
	{
		// Refused for a reason the condition could not see. Succeeded, not Failed: the tree should
		// re-select immediately and shoot instead, not take the Engage penalty delay for it.
		UE_LOG(LogBN, Verbose, TEXT("BNBots: %s pressed grenade but no cooldown appeared — the ability refused."),
			*GetNameSafe(Bot->GetPawn()));
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FBNThrowGrenadeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Throw Grenade</b>");
}
#endif
