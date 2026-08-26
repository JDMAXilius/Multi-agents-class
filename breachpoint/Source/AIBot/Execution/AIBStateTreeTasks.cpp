#include "Execution/AIBStateTreeTasks.h"

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
		const FRotator Stepped = DegreesPerSecond <= 0.f
			? Desired
			: FMath::RInterpConstantTo(Controller.GetControlRotation(), Desired, DeltaTime, DegreesPerSecond);
		Controller.SetControlRotation(Stepped);
		Pawn->FaceRotation(Stepped, DeltaTime);
	}

	bool IsWithin(const AAIController& Controller, const FVector& Point, float RadiusUU)
	{
		const APawn* Pawn = Controller.GetPawn();
		return Pawn && FVector::Dist(Pawn->GetActorLocation(), Point) <= RadiusUU;
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
	// Already in position: station-keep from here (issuing a move to where we stand
	// would complete instantly and thrash the branch — the never-succeed contract).
	if (!IsWithin(*Bot, InstanceData.LastGoal, InstanceData.AcceptanceRadiusUU))
	{
		Bot->MoveToLocation(InstanceData.LastGoal, InstanceData.AcceptanceRadiusUU);
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
	// this task; the sentinel or a visibility loss is what ends the branch.
	const FVector Belief = Bot->GetSensorium().GetLastSeenLocation();
	if (!IsWithin(*Bot, Belief, InstanceData.AcceptanceRadiusUU)
		&& FVector::Dist(Belief, InstanceData.LastGoal) > InstanceData.RepathAtDriftUU)
	{
		InstanceData.LastGoal = Belief;
		Bot->MoveToLocation(Belief, InstanceData.AcceptanceRadiusUU);
	}
	return EStateTreeRunStatus::Running;
}

void FAIBMoveNearBeliefTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->StopMovement();
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBFireWhenAbleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bHolding = false;
	InstanceData.PhaseSecondsLeft = 0.f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBFireWhenAbleTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	IAIBAvatarInterface* Avatar = Bot ? Bot->GetAvatar() : nullptr;
	if (!Bot || !Avatar)
	{
		return EStateTreeRunStatus::Failed;
	}

	// The cached facts are the one info door: matured visibility + the assembled
	// can-fight answer, never raw avatar reads scattered through tasks.
	const FAIBFacts& Facts = Bot->GetLastFacts();
	const bool bMayFire = Facts.bTargetVisible && Facts.bWeaponCanFight;

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
	// ALWAYS release: a held verb on the persistent ASC outlives the body.
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (InstanceData.bHolding && Bot && Bot->GetAvatar())
	{
		Bot->GetAvatar()->ReleaseVerb(AIBTags::Verb_Fire);
	}
	InstanceData.bHolding = false;
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
	FVector ThreatPoint;
	if (Bot->GetSensorium().HasVisibleTarget())
	{
		ThreatPoint = Bot->GetSensorium().GetLastSeenLocation();
	}
	else
	{
		const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
		if (!Bot->GetSensorium().Memory().GetFresh(
			Bot->GetWorld()->GetTimeSeconds(), Window > 0.f ? Window : 16.f, ThreatPoint))
		{
			return EStateTreeRunStatus::Failed; // nothing to flee from
		}
	}

	const FVector Away = (Pawn->GetActorLocation() - ThreatPoint).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		return EStateTreeRunStatus::Failed;
	}
	InstanceData.FleeGoal = Pawn->GetActorLocation() + Away * InstanceData.FleeDistanceUU;
	Bot->MoveToLocation(InstanceData.FleeGoal, 150.f);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBFleeFromBeliefTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}
	return IsWithin(*Bot, InstanceData.FleeGoal, 200.f)
		? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

void FAIBFleeFromBeliefTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->StopMovement();
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBMoveToLastKnownTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}

	const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
	FVector LastKnown;
	if (!Bot->GetSensorium().Memory().GetFresh(
		Bot->GetWorld()->GetTimeSeconds(), Window > 0.f ? Window : 16.f, LastKnown))
	{
		return EStateTreeRunStatus::Failed; // stale: Root re-selects
	}
	Bot->MoveToLocation(LastKnown, InstanceData.AcceptanceRadiusUU);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBMoveToLastKnownTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
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
		Bot->GetWorld()->GetTimeSeconds(), Window > 0.f ? Window : 16.f, LastKnown))
	{
		return EStateTreeRunStatus::Failed;
	}
	// Arrived: STAND at the post and let SweepLook hunt. The branch ends when the
	// memory stales (above), someone appears (Succeeded), or the want moves on.
	return EStateTreeRunStatus::Running;
}

void FAIBMoveToLastKnownTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
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

	Bot->MoveToLocation(InstanceData.Goal, InstanceData.AcceptanceRadiusUU);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBMoveToPOITask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}
	return (InstanceData.bHasGoal && IsWithin(*Bot, InstanceData.Goal, InstanceData.AcceptanceRadiusUU))
		? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

void FAIBMoveToPOITask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->StopMovement();
	}
}

FGameplayTag FAIBMoveToPOITask::GetPOIKind() const           { return FGameplayTag(); }
bool FAIBMoveToPOITask::ShouldWanderWithoutProvider() const  { return false; }
