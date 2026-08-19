#include "AI/BNBotStateTreeTasks.h"

#include "AI/BNBotBrain.h"
#include "AI/BNBotController.h"
#include "AI/BNPointOfInterest.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "BreachpointNext.h"
#include "Core/BNGameplayTags.h"
#include "Match/BNPlayerState.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	ABNBotController* GetBot(AAIController* Controller)
	{
		return Cast<ABNBotController>(Controller);
	}

	AActor* GetBotTarget(AAIController* Controller)
	{
		const ABNBotController* Bot = GetBot(Controller);
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
	return GetBotTarget(InstanceData.Controller) != nullptr;
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
	AActor* Target = GetBotTarget(InstanceData.Controller);
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.AimErrorDegrees <= 0.f)
	{
		// Exact focus: the engine tracks the actor, no re-jitter needed.
		InstanceData.Controller->SetFocus(Target);
	}
	else
	{
		InstanceData.Controller->SetFocalPoint(JitteredFocalPoint(*InstanceData.Controller, *Target, InstanceData.AimErrorDegrees));
	}

	InstanceData.SecondsUntilReaim = InstanceData.ReaimSeconds;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNFaceTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AActor* Target = GetBotTarget(InstanceData.Controller);
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.AimErrorDegrees > 0.f)
	{
		InstanceData.SecondsUntilReaim -= DeltaTime;
		if (InstanceData.SecondsUntilReaim <= 0.f)
		{
			InstanceData.Controller->SetFocalPoint(JitteredFocalPoint(*InstanceData.Controller, *Target, InstanceData.AimErrorDegrees));
			InstanceData.SecondsUntilReaim = InstanceData.ReaimSeconds;
		}
	}

	return EStateTreeRunStatus::Running;
}

void FBNFaceTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.Controller)
	{
		InstanceData.Controller->ClearFocus(EAIFocusPriority::Gameplay);
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
	AActor* Target = GetBotTarget(InstanceData.Controller);
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.Controller->MoveToActor(Target, InstanceData.AcceptanceRadius) == EPathFollowingRequestResult::Failed)
	{
		return EStateTreeRunStatus::Failed;
	}

	// AlreadyAtGoal falls through: the first Tick's radius check answers Succeeded.
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNMoveToTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AActor* Target = GetBotTarget(InstanceData.Controller);
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	const APawn* Pawn = InstanceData.Controller->GetPawn();
	const bool bInRadius = Pawn && FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation()) <= InstanceData.AcceptanceRadius;
	const bool bMoveDone = InstanceData.Controller->GetMoveStatus() == EPathFollowingStatus::Idle;
	if (bInRadius || bMoveDone)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FBNMoveToTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.Controller)
	{
		InstanceData.Controller->StopMovement();
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
	ABNBotController* Bot = GetBot(InstanceData.Controller);
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

	Bot->PressInputTag(BNTags::Input_Weapon_Fire);
	InstanceData.SecondsRemaining = InstanceData.BurstSeconds;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNFireBurstTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	ABNBotController* Bot = GetBot(InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
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
	if (ABNBotController* Bot = GetBot(InstanceData.Controller))
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

EStateTreeRunStatus FBNMoveToPointOfInterestTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* Controller = InstanceData.Controller;
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
	const ABNBotController* Bot = Cast<ABNBotController>(Controller);
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
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.CurrentPoint = Pick;
	InstanceData.bArrived = false;
	InstanceData.DwellRemaining = InstanceData.DwellSeconds;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FBNMoveToPointOfInterestTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	// A target appearing mid-walk is deliberately NOT handled here: the tree's Engage state
	// preempts this one through FBNHasTargetCondition.
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* Controller = InstanceData.Controller;
	const ABNPointOfInterest* Point = InstanceData.CurrentPoint.Get();
	if (!Controller || !Point)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.bArrived)
	{
		const APawn* Pawn = Controller->GetPawn();
		const bool bInRadius = Pawn && FVector::Dist(Pawn->GetActorLocation(), Point->GetActorLocation()) <= Point->Radius;
		const bool bMoveDone = Controller->GetMoveStatus() == EPathFollowingStatus::Idle;
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
	if (InstanceData.Controller)
	{
		InstanceData.Controller->StopMovement();
	}
}

#if WITH_EDITOR
FText FBNMoveToPointOfInterestTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString("<b>BN Move To Point Of Interest</b>");
}
#endif
