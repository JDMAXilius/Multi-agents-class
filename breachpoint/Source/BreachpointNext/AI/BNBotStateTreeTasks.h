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

	/** How fast the bot may swing its aim, in degrees per second. 0 snaps instantly. This is a
	 *  fairness knob as much as a presentation one: a bot whose control rotation teleports onto
	 *  the player is an aimbot, and the turn is applied by this task rather than by the engine's
	 *  focus system — see SteerControlRotation in the .cpp for why. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float TurnDegreesPerSecond = 360.f;

	/** Internal: countdown to the next jitter draw. */
	float SecondsUntilReaim = 0.f;

	/** Internal: the point currently being aimed at, re-drawn every ReaimSeconds. Held so the
	 *  jitter is stable between draws instead of shimmering every frame. */
	FVector AimPoint = FVector::ZeroVector;
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

	/** Floor on how often a new path may be requested while hunting for a sightline. Without it,
	 *  a bot standing inside AcceptanceRadius behind a wall re-requests a path EVERY FRAME:
	 *  MoveToActor answers AlreadyAtGoal, the move status goes straight back to Idle, and the
	 *  repath fires again — the exact per-frame pathfind the state's transition delay exists to
	 *  prevent, except inside a single task where no transition delay can reach it. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RepathIntervalSeconds = 0.5f;

	/** Internal: countdown to the next allowed repath. */
	float SecondsUntilRepath = 0.f;

	/** Internal: the move-failure diagnosis is printed once, not once per frame. */
	bool bWarnedMoveFailed = false;

	/** Internal: throttles the closing diagnostic to roughly one line per second. */
	float SecondsUntilCloseLog = 0.f;
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

	/** Turn rate while roaming. Slower than combat: a bot strolling between points that snapped
	 *  to each path corner would read as a machine, not a person. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float TurnDegreesPerSecond = 180.f;

	/** Internal: warned once — a level with no points must be SAID, not spun on in silence. */
	bool bWarnedNoPointsOfInterest = false;

	/** Internal: last point visited, so the roam never picks the same spot twice running. Weak —
	 *  a deleted point must not dangle. Persists across state re-entries by instance-data lifetime. */
	TWeakObjectPtr<ABNPointOfInterest> LastPoint;

	TWeakObjectPtr<ABNPointOfInterest> CurrentPoint;
	float DwellRemaining = 0.f;
	bool bArrived = false;

	/** Internal: the move-failure diagnosis is printed once, not once per frame. */
	bool bWarnedMoveFailed = false;
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

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNHasLineOfSightConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;
};

/** Passes only when the bot can SEE its target right now. Distinct from BN Has Target on purpose:
 *  the sight sense REMEMBERS a target after it breaks line of sight (that is what LoseSightRadius
 *  and the forget timer are for), so a bot gated only on "have target" empties its magazine into
 *  the wall the player stepped behind. Fire is gated on this; the chase is not. */
USTRUCT(meta = (DisplayName = "BN Has Line Of Sight", Category = "BN"))
struct FBNHasLineOfSightCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNHasLineOfSightConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNHasLineOfSightCondition() = default;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNNeedsReloadConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** Reload once the magazine falls to this fraction of its size. 0 means "only when empty". */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ReloadAtMagazineFraction = 0.25f;
};

/** Passes when the held weapon's magazine is at or below the fraction AND the reserve can refill
 *  it. Reserve matters: without that half, a bot with an empty reserve would reload forever
 *  instead of swapping to the weapon that still has rounds. */
USTRUCT(meta = (DisplayName = "BN Needs Reload", Category = "BN"))
struct FBNNeedsReloadCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNNeedsReloadConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNNeedsReloadCondition() = default;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNReloadTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** Ceiling on the wait, not the reload's duration — the montage owns that. A reload the ASC
	 *  refused (dead, frozen, already full) must not park the tree in this state forever. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float TimeoutSeconds = 4.f;

	/** Internal: magazine count when the press went in — the reload LANDED when this grows. */
	int32 AmmoAtStart = 0;

	float SecondsElapsed = 0.f;
};

/** Presses the SAME Reload input tag a human's R key presses, then waits for the magazine to
 *  actually grow — the honest completion signal, because BNGA_Reload refills from a montage
 *  notify and the ability's duration is animation data, not a number this task may assume. */
USTRUCT(meta = (DisplayName = "BN Reload", Category = "BN"))
struct FBNReloadTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNReloadTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNReloadTask()
	{
		bShouldCallTick = true;
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNSelectWeaponTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** How many Next presses before giving up. The carried set is small; a cap is what stops an
	 *  all-empty loadout from cycling weapons for the rest of the match. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 MaxSwaps = 3;

	/** A swap is an ability with a montage — pressing Next every frame would cancel it repeatedly
	 *  and the weapon would never change. This is the pause between presses. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float SecondsBetweenSwaps = 0.4f;

	int32 SwapsMade = 0;
	float SecondsUntilNextSwap = 0.f;
};

/** Change weapon: press Input.Weapon.Next until the held weapon can actually shoot — rounds in
 *  the magazine, or a reserve to reload from. Succeeds the moment one can; fails after MaxSwaps,
 *  which is the tree's cue to stop fighting rather than to keep clicking an empty gun. */
USTRUCT(meta = (DisplayName = "BN Select Weapon", Category = "BN"))
struct FBNSelectWeaponTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNSelectWeaponTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNSelectWeaponTask()
	{
		bShouldCallTick = true;
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
