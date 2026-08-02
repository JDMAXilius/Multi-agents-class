// Breachpoint. Layer 2 — every StateTree task and condition ST_Bot is allowed to bind to.
#pragma once

#include "AI/BRBotBrain.h"
#include "AI/BRBotFacts.h"
#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"

#include "BRStateTreeTasks.generated.h"

class AActor;
class ABRBotController;
class UEnvQuery;

namespace BRBotStateTree
{
	BREACHPOINT_API ABRBotController* GetBotController(struct FStateTreeExecutionContext& Context);
}

namespace BRBotEngage
{
	BREACHPOINT_API void RunSelectorOnce(ABRBotController& Bot, AActor* Target);
}

USTRUCT()
struct BREACHPOINT_API FBRSTCondition_AmbitionIsInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EBRBotAmbition Ambition = EBRBotAmbition::None;
};

USTRUCT(meta = (DisplayName = "BR Ambition Is", Category = "Breachpoint|Bot"))
struct BREACHPOINT_API FBRSTCondition_AmbitionIs : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBRSTCondition_AmbitionIsInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT()
struct BREACHPOINT_API FBRSTCondition_StepVerbIsInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EBRBotStepVerb Verb = EBRBotStepVerb::Idle;
};

USTRUCT(meta = (DisplayName = "BR Step Verb Is", Category = "Breachpoint|Bot"))
struct BREACHPOINT_API FBRSTCondition_StepVerbIs : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBRSTCondition_StepVerbIsInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT()
struct BREACHPOINT_API FBRSTCondition_PreconditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EBRBotPrecondition Precondition = EBRBotPrecondition::TargetVisible;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

USTRUCT(meta = (DisplayName = "BR Precondition", Category = "Breachpoint|Bot"))
struct BREACHPOINT_API FBRSTCondition_Precondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBRSTCondition_PreconditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT()
struct BREACHPOINT_API FBRSTTask_MoveToAnchorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UEnvQuery> LocationQuery;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadius = 0.f;
};

USTRUCT(meta = (DisplayName = "BR Move To Step Anchor", Category = "Breachpoint|Bot"))
struct BREACHPOINT_API FBRSTTask_MoveToAnchor : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FBRSTTask_MoveToAnchor();

	using FInstanceDataType = FBRSTTask_MoveToAnchorInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct BREACHPOINT_API FBRSTTask_EngageInstanceData
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "BR Engage", Category = "Breachpoint|Bot"))
struct BREACHPOINT_API FBRSTTask_Engage : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FBRSTTask_Engage();

	using FInstanceDataType = FBRSTTask_EngageInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct BREACHPOINT_API FBRSTTask_FlushInstanceData
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "BR Flush", Category = "Breachpoint|Bot"))
struct BREACHPOINT_API FBRSTTask_Flush : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FBRSTTask_Flush();

	using FInstanceDataType = FBRSTTask_FlushInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct BREACHPOINT_API FBRSTTask_HoldInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float HoldSeconds = 0.f;
};

USTRUCT(meta = (DisplayName = "BR Hold Sightline", Category = "Breachpoint|Bot"))
struct BREACHPOINT_API FBRSTTask_Hold : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FBRSTTask_Hold();

	using FInstanceDataType = FBRSTTask_HoldInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct BREACHPOINT_API FBRSTTask_ContestRocketInstanceData
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "BR Contest Rocket", Category = "Breachpoint|Bot"))
struct BREACHPOINT_API FBRSTTask_ContestRocket : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FBRSTTask_ContestRocket();

	using FInstanceDataType = FBRSTTask_ContestRocketInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
