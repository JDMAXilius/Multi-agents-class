// Breachpoint. Layer 2 implementation. Tasks press buttons and move; they never decide goals.

#include "AI/BRStateTreeTasks.h"

#include "AI/BRBotController.h"
#include "Core/BRGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogBRBotSpine, Log, All);

// =============================================================================================
// Shared helpers
// =============================================================================================

ABRBotController* BRBotStateTree::GetBotController(FStateTreeExecutionContext& Context)
{
	UObject* Owner = Context.GetOwner();

	// The StateTree component lives on the controller, so this is the ordinary case. The
	// pawn fallback exists because a future schema may run the tree from the pawn instead,
	// and a silent null here would look like "the bot ignores its plan".
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
	// AIM FIRST, ALWAYS. Error is applied to the control rotation before any activation, so
	// the shot the server validates is the shot the bot actually took (BP08 step 3). Doing
	// this after the press would make accuracy_pct decorative.
	Bot.AimAtTargetWithError(Target);

	// The selector reads LIVE facts, not the brain's snapshot: the brain reasons about the
	// world as it was when it noticed (reaction delay); the trigger finger reacts to the
	// world as it is. Two different jobs, deliberately not sharing a cache.
	const FBRBotFacts Facts = Bot.BuildFacts();

	// -----------------------------------------------------------------------------------
	// The prioritized selector. First branch whose GAS gate opens, wins. Order is behaviour
	// and is reviewed as behaviour.
	// -----------------------------------------------------------------------------------

	// 1. Power weapon in hand: it is the whole reason the bot wanted the pad. Same InputTag
	//    as any other primary fire — the ASC knows which weapon is equipped, we do not.
	if (Facts.Has(EBRBotPrecondition::HoldingPowerWeapon)
		&& Bot.CanActivateByInputTag(BRGameplayTags::InputTag_Fire))
	{
		Bot.PressInputTag(BRGameplayTags::InputTag_Fire);
		return;
	}

	// 2. Shields cracked and the grenade is up: the finisher, and the beat a player reads.
	if (Facts.Has(EBRBotPrecondition::TargetShieldsBroken)
		&& Bot.CanActivateByInputTag(BRGameplayTags::InputTag_Grenade))
	{
		Bot.PressInputTag(BRGameplayTags::InputTag_Grenade);
		Bot.ReleaseInputTag(BRGameplayTags::InputTag_Grenade);
		return;
	}

	// 3. Melee range: Halo's beat-down. Range is the ability's own business — if the ASC
	//    says it cannot activate, the bot is not in range, and nothing here needs a number.
	if (Facts.Has(EBRBotPrecondition::MeleeInRange)
		&& Bot.CanActivateByInputTag(BRGameplayTags::InputTag_Melee))
	{
		Bot.PressInputTag(BRGameplayTags::InputTag_Melee);
		return;
	}

	// 4. Otherwise: shoot. Reload is a consequence of the ASC refusing to fire, not of a
	//    magazine number this file knows.
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

// =============================================================================================
// Conditions
// =============================================================================================

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

	// A LIVE fact fill, not the brain's last snapshot: transitions must react to the world
	// as it is now, while the brain deliberately reasons about the world as it was when it
	// noticed. Those are different jobs and conflating them is how a bot reacts before it
	// legally could (R11).
	const FBRBotFacts Facts = Bot->BuildFacts();
	const bool bHas = Facts.Has(Data.Precondition);
	return Data.bInvert ? !bHas : bHas;
}

// =============================================================================================
// Tasks
// =============================================================================================

FBRSTTask_MoveToAnchor::FBRSTTask_MoveToAnchor()
{
	// Law 4, restated per task: no per-frame work. Completion arrives on the move delegate.
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

	// Running until the brain moves the plan on. The task never decides it is done — the
	// controller reports to the brain, the brain changes the step, and the tree transitions.
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
		// Entered Engage with nothing to fight: the plan is contradicted. Say so rather than
		// standing in the open looking clever.
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
		// The ASC refused (cooldown, no grenades) — a precondition died between the plan and
		// the act. Report, replan; never retry silently.
		Bot->ReportStepFailed();
		return EStateTreeRunStatus::Failed;
	}

	Bot->AimAtTargetWithError(Target);
	Bot->PressInputTag(BRGameplayTags::InputTag_Grenade);
	Bot->ReleaseInputTag(BRGameplayTags::InputTag_Grenade);

	// One press, one step. The plan's next step re-engages.
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

	// Watch the thing being held. If a target shows up, the Engage transition takes over —
	// holding is a stance, not a blindfold.
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

	// Contesting is fighting on a specific piece of ground: if someone is there, fight them;
	// the pickup itself happens by WALKING ONTO the pad, exactly as it does for a human.
	// Nothing in this task grants a weapon or touches an inventory.
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
