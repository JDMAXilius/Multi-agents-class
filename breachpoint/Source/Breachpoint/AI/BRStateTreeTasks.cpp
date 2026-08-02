// Breachpoint. Layer 2 implementation. Tasks press buttons and move; they never decide goals.
#include "AI/BRStateTreeTasks.h"

#include "AI/BRBotController.h"
#include "Core/BRGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogBRBotSpine, Log, All);

ABRBotController* BRBotStateTree::GetBotController(FStateTreeExecutionContext& Context)
{
	UObject* Owner = Context.GetOwner();

	if (ABRBotController* AsController = Cast<ABRBotController>(Owner))
	{
		return AsController;
	}
	if (const APawn* AsPawn = Cast<APawn>(Owner))
	{
		return Cast<ABRBotController>(AsPawn->GetController());
	}
	return nullptr;
}

void BRBotEngage::RunSelectorOnce(ABRBotController& Bot, AActor* Target)
{
	Bot.AimAtTargetWithError(Target);

	const FBRBotFacts Facts = Bot.BuildFacts();

	if (Facts.Has(EBRBotPrecondition::HoldingPowerWeapon)
		&& Bot.CanActivateByInputTag(BRGameplayTags::InputTag_Fire))
	{
		Bot.PressInputTag(BRGameplayTags::InputTag_Fire);
		return;
	}

	if (Facts.Has(EBRBotPrecondition::TargetShieldsBroken)
		&& Bot.CanActivateByInputTag(BRGameplayTags::InputTag_Grenade))
	{
		Bot.PressInputTag(BRGameplayTags::InputTag_Grenade);
		Bot.ReleaseInputTag(BRGameplayTags::InputTag_Grenade);
		return;
	}

	if (Facts.Has(EBRBotPrecondition::MeleeInRange)
		&& Bot.CanActivateByInputTag(BRGameplayTags::InputTag_Melee))
	{
		Bot.PressInputTag(BRGameplayTags::InputTag_Melee);
		return;
	}

	if (Bot.CanActivateByInputTag(BRGameplayTags::InputTag_Fire))
	{
		Bot.PressInputTag(BRGameplayTags::InputTag_Fire);
		return;
	}

	if (Bot.CanActivateByInputTag(BRGameplayTags::InputTag_Reload))
	{
		Bot.ReleaseInputTag(BRGameplayTags::InputTag_Fire);
		Bot.PressInputTag(BRGameplayTags::InputTag_Reload);
	}
}

bool FBRSTCondition_AmbitionIs::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const ABRBotController* Bot = BRBotStateTree::GetBotController(Context);
	return (Bot != nullptr) && (Bot->GetActiveAmbition() == Data.Ambition);
}

bool FBRSTCondition_StepVerbIs::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const ABRBotController* Bot = BRBotStateTree::GetBotController(Context);
	return (Bot != nullptr) && (Bot->GetCurrentStep().Verb == Data.Verb);
}

bool FBRSTCondition_Precondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	ABRBotController* Bot = BRBotStateTree::GetBotController(Context);
	if (Bot == nullptr)
	{
		return false;
	}

	const FBRBotFacts Facts = Bot->BuildFacts();
	const bool bHas = Facts.Has(Data.Precondition);
	return Data.bInvert ? !bHas : bHas;
}

FBRSTTask_MoveToAnchor::FBRSTTask_MoveToAnchor()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FBRSTTask_MoveToAnchor::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ABRBotController* Bot = BRBotStateTree::GetBotController(Context);
	if (Bot == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	Bot->RequestMoveToAnchor(Data.LocationQuery, Data.AcceptanceRadius);

	return EStateTreeRunStatus::Running;
}

void FBRSTTask_MoveToAnchor::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (ABRBotController* Bot = BRBotStateTree::GetBotController(Context))
	{
		Bot->CancelActiveMove();
	}
}

FBRSTTask_Engage::FBRSTTask_Engage()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FBRSTTask_Engage::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ABRBotController* Bot = BRBotStateTree::GetBotController(Context);
	if (Bot == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* Target = Bot->GetCurrentTarget();
	if (Target == nullptr)
	{
		Bot->ReportStepFailed();
		return EStateTreeRunStatus::Failed;
	}

	Bot->BeginEngage(Target);
	return EStateTreeRunStatus::Running;
}

void FBRSTTask_Engage::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (ABRBotController* Bot = BRBotStateTree::GetBotController(Context))
	{
		Bot->EndEngage();
	}
}

FBRSTTask_Flush::FBRSTTask_Flush()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FBRSTTask_Flush::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ABRBotController* Bot = BRBotStateTree::GetBotController(Context);
	if (Bot == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* Target = Bot->GetCurrentTarget();
	if (Target == nullptr || !Bot->CanActivateByInputTag(BRGameplayTags::InputTag_Grenade))
	{
		Bot->ReportStepFailed();
		return EStateTreeRunStatus::Failed;
	}

	Bot->AimAtTargetWithError(Target);
	Bot->PressInputTag(BRGameplayTags::InputTag_Grenade);
	Bot->ReleaseInputTag(BRGameplayTags::InputTag_Grenade);

	Bot->ReportStepCompleted();
	return EStateTreeRunStatus::Succeeded;
}

FBRSTTask_Hold::FBRSTTask_Hold()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FBRSTTask_Hold::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	ABRBotController* Bot = BRBotStateTree::GetBotController(Context);
	if (Bot == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (AActor* Target = Bot->GetCurrentTarget())
	{
		Bot->AimAtTargetWithError(Target);
	}

	Bot->StartHoldTimer(Data.HoldSeconds);
	return EStateTreeRunStatus::Running;
}

void FBRSTTask_Hold::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (ABRBotController* Bot = BRBotStateTree::GetBotController(Context))
	{
		Bot->CancelHoldTimer();
	}
}

FBRSTTask_ContestRocket::FBRSTTask_ContestRocket()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FBRSTTask_ContestRocket::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ABRBotController* Bot = BRBotStateTree::GetBotController(Context);
	if (Bot == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (AActor* Target = Bot->GetCurrentTarget())
	{
		Bot->BeginEngage(Target);
	}

	return EStateTreeRunStatus::Running;
}

void FBRSTTask_ContestRocket::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (ABRBotController* Bot = BRBotStateTree::GetBotController(Context))
	{
		Bot->EndEngage();
	}
}
