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

	/** Give up on a target the bot has stopped getting closer to. Catches the wedged case that
	 *  AlreadyAtGoal does not: a valid path that makes no headway. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float GiveUpAfterNoProgressSeconds = 6.f;

	/** Internal: closest the bot has been on this approach, and how long since that improved. */
	float BestDistance = 0.f;
	float SecondsWithoutProgress = 0.f;

	/** Internal: throttles the locomotion report to roughly one line per second. */
	float SecondsUntilLocomotionLog = 0.f;
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

	/** Turn rate while roaming. The heading is set to face the goal when the leg BEGINS, so this
	 *  only has to cover corners mid-path — at 180 a sharp one stayed visible as a strafe for most
	 *  of a second, which is why it is 360: a person pivoting briskly, not a turret snapping. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float TurnDegreesPerSecond = 360.f;

	/** Internal: warned once — a level with no points must be SAID, not spun on in silence. */
	bool bWarnedNoPointsOfInterest = false;

	/** Internal: last point visited, so the roam never picks the same spot twice running. Weak —
	 *  a deleted point must not dangle. Persists across state re-entries by instance-data lifetime. */
	TWeakObjectPtr<ABNPointOfInterest> LastPoint;

	TWeakObjectPtr<ABNPointOfInterest> CurrentPoint;
	float DwellRemaining = 0.f;
	bool bArrived = false;

	/** Internal: throttles the locomotion report to roughly one line per second. */
	float SecondsUntilLocomotionLog = 0.f;

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

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNReactedConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;
};

/** Passes once the bot's reaction window since acquiring its target has elapsed (R11: >= 200ms,
 *  quantized and seeded on the controller). The gate on shooting and on melee — never on moving,
 *  because a bot that cannot even START WALKING for a quarter second reads as asleep, not human. */
USTRUCT(meta = (DisplayName = "BN Reacted", Category = "BN"))
struct FBNReactedCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNReactedConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNReactedCondition() = default;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNInMeleeRangeConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** Fraction of the weapon's OWN MeleeRange at which the bot commits to a swing. Below 1 so it
	 *  swings inside the ability's reach rather than exactly at its edge, where a step backwards
	 *  turns a hit into a whiff. The reach itself is never restated here — it is read from the
	 *  held weapon's row, which is where BNGA_Melee reads it too. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RangeFraction = 0.8f;
};

/** Passes when the target is inside the HELD WEAPON's melee reach. The top of the engage priority
 *  selector: doctrine §4 names the shape (rocket-if-held -> grenade-if-cracked -> fire -> melee),
 *  and melee is the one step this game already has an ability for. A knife bot that shoots you
 *  from arm's length instead of stabbing is the readable failure this prevents. */
USTRUCT(meta = (DisplayName = "BN In Melee Range", Category = "BN"))
struct FBNInMeleeRangeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNInMeleeRangeConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNInMeleeRangeCondition() = default;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNMeleeTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** Ceiling on the wait, not the swing's length — the montage owns that. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float TimeoutSeconds = 1.5f;

	float SecondsElapsed = 0.f;
};

/** Presses the SAME Input.Melee a human's melee key presses, then waits out the swing. GAS-pure
 *  by construction: the damage, the reach and the montage all belong to BNGA_Melee, and this task
 *  knows none of them. */
USTRUCT(meta = (DisplayName = "BN Melee", Category = "BN"))
struct FBNMeleeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNMeleeTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNMeleeTask()
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
struct FBNHasLastKnownConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;
};

/** Passes when the bot has no target but DOES remember where one just was, recently enough to be
 *  worth walking to. The gate on the Search state. */
USTRUCT(meta = (DisplayName = "BN Has Last Known", Category = "BN"))
struct FBNHasLastKnownCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNHasLastKnownConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNHasLastKnownCondition() = default;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNSearchLastKnownTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadius = 150.f;

	/** How long to stand and look around on arrival. The beat that makes the hunt readable. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float LookAroundSeconds = 2.f;

	/** Degrees per second the bot sweeps its view while looking around. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float SweepDegreesPerSecond = 90.f;

	bool bArrived = false;
	float LookAroundRemaining = 0.f;
	float SweptDegrees = 0.f;
};

/** Walks to where the threat was last seen, then sweeps its view. Halo's legibility lesson made
 *  concrete: the bot that hunts your last position reads as intelligent, while the one that
 *  forgets you the instant you round a corner reads as broken — and the two cost the same. */
USTRUCT(meta = (DisplayName = "BN Search Last Known", Category = "BN"))
struct FBNSearchLastKnownTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNSearchLastKnownTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNSearchLastKnownTask()
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
