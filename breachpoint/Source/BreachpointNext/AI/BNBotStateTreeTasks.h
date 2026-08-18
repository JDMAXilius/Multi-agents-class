#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "BNBotStateTreeTasks.generated.h"

class AAIController;
class ABNPointOfInterest;

/** The tree's C++ vocabulary, Variant_Shooter's mechanical shape: plain instance-data structs
 *  bound by the editor, nodes deriving the Common bases. Everything here runs server-side only —
 *  the AIController exists nowhere else — so no task carries replication concerns. */

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNHasTargetConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;
};

/** Passes when the owning ABNBotController holds a live target. The Engage state's gate. */
USTRUCT(meta = (DisplayName = "BN Has Target", Category = "BN"))
struct FBNHasTargetCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNHasTargetConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNHasTargetCondition() = default;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNFaceTargetTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** The ONE aim-humanization knob (G3 3.2): a random cone this wide around the true target
	 *  point, re-drawn every ReaimSeconds. Zero means SetFocus, hitscan-perfect. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AimErrorDegrees = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ReaimSeconds = 0.5f;

	/** Internal: countdown to the next jitter draw. */
	float SecondsUntilReaim = 0.f;
};

/** Faces the controller's current target through the focus system, with the aim-error cone. */
USTRUCT(meta = (DisplayName = "BN Face Target", Category = "BN"))
struct FBNFaceTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNFaceTargetTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNFaceTargetTask()
	{
		bShouldCallTick = true;
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNMoveToTargetTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** A mid-range firing distance: close enough to hit, not a melee shove. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadius = 800.f;
};

/** Moves toward the controller's current target; succeeds inside AcceptanceRadius. */
USTRUCT(meta = (DisplayName = "BN Move To Target", Category = "BN"))
struct FBNMoveToTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNMoveToTargetTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNMoveToTargetTask()
	{
		bShouldCallTick = true;
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNFireBurstTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float BurstSeconds = 0.6f;

	/** Internal: time left on the current burst. */
	float SecondsRemaining = 0.f;
};

/** Presses the SAME Fire input tag a human's trigger key presses, holds it BurstSeconds, releases.
 *  Refuses itself while State.Match.Frozen — the ASC would refuse anyway; this stops futile
 *  presses. ExitState ALWAYS releases: an interrupted burst must not leave the trigger held. */
USTRUCT(meta = (DisplayName = "BN Fire Burst", Category = "BN"))
struct FBNFireBurstTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNFireBurstTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNFireBurstTask()
	{
		bShouldCallTick = true;
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNMoveToPointOfInterestTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float DwellSeconds = 2.f;

	/** Internal: last point visited, so the roam never picks the same spot twice running. Weak —
	 *  a deleted point must not dangle. Persists across state re-entries by instance-data lifetime. */
	TWeakObjectPtr<ABNPointOfInterest> LastPoint;

	TWeakObjectPtr<ABNPointOfInterest> CurrentPoint;
	float DwellRemaining = 0.f;
	bool bArrived = false;
};

/** Roam: nearest ABNPointOfInterest that is not the last one, walk there, dwell, succeed.
 *  A target appearing mid-walk needs NO handling here — the tree's Engage state preempts this
 *  one through FBNHasTargetCondition. */
USTRUCT(meta = (DisplayName = "BN Move To Point Of Interest", Category = "BN"))
struct FBNMoveToPointOfInterestTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNMoveToPointOfInterestTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNMoveToPointOfInterestTask()
	{
		bShouldCallTick = true;
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
