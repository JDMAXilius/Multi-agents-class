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
	/** NEGATIVE means "ask the bot's TIER" (R10), and that is the default: difficulty owns aim.
	 *  Zero is still hitscan-perfect and any positive value is a deliberate per-state override the
	 *  tree keeps — a state that must aim a particular way can still say so. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AimErrorDegrees = -1.f;

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

	/** Internal (R9.5): one jump per wedge, tried at HALF the give-up window. A lip, a crate or a
	 *  step is the commonest reason a path exists and the bot still gets nowhere, and a jump is
	 *  exactly the move that clears it — so it is spent before the target is written off, not
	 *  after. One attempt: a bot that jumps repeatedly at a wall it cannot pass reads as stuck
	 *  AND stupid, where giving up reads as a decision. */
	bool bTriedWedgeJump = false;

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
struct FBNShouldTakeCoverConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** Below this fraction of health, and only while actually under fire. NOT the Survive
	 *  threshold (0.35): Survive is "leave the fight", this is "stop standing in it" — the
	 *  middle ground Halo gets for free from its shield economy and BN has to say out loud. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float HealthBelow = 0.6f;
};

/**
 * Should this bot break line of sight? Three things at once, and the AND is the whole design:
 * hurt, under fire RIGHT NOW, and not already spending its cover cooldown.
 *
 * "Under fire" is `State.Combat.RecentDamage` — the tag the shield window already applies on
 * every landed hit. Without it a bot chipped once ten seconds ago would dive for cover in the
 * middle of a fight it is winning, which reads as cowardice rather than tactics.
 */
USTRUCT(meta = (DisplayName = "BN Should Take Cover", Category = "BN"))
struct FBNShouldTakeCoverCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNShouldTakeCoverConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNTakeCoverTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** How far to look for a spot. Far enough to get behind something, near enough that the bot
	 *  is not crossing the map with its back turned. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float SearchRadius = 700.f;

	/** How many directions are sampled around the bot. Eight is a 45° rosette — enough to find a
	 *  pillar, cheap enough to run on one frame with no EQS. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 SampleCount = 8;

	/** How long to sit behind cover before re-engaging. This is the beat that makes cover READ as
	 *  cover; a bot that touches the spot and walks straight back out looks like a pathing bug. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float HoldSeconds = 1.5f;

	/** Internal: the chosen spot, and whether the bot has reached it. */
	FVector CoverPoint = FVector::ZeroVector;
	bool bArrived = false;
	float HoldRemaining = 0.f;
};

/**
 * BREAK LINE OF SIGHT — the behaviour Halo's bots get from their shield economy and BN's have
 * never had: hurt, under fire, so stop standing in the open.
 *
 * NO EQS, and that is a deliberate scope call rather than a limitation to apologise for: a
 * rosette of navmesh-projected samples, each one traced back at the threat on the WEAPON channel,
 * answers the only question cover asks — *can this spot be shot from where they are standing* —
 * and it answers it with the same geometry the bullets use. An EQS query would ask it more
 * expensively and no more truthfully. The day BN wants scored cover (flanking angles, distance
 * bands, height) EQS earns its place; picking a wall does not need it.
 *
 * FAILS when nothing blocks. That is not a bug: an open arena has no cover, and the tree falling
 * through to Close/Shoot is the correct answer to "there is nowhere to hide".
 */
USTRUCT(meta = (DisplayName = "BN Take Cover", Category = "BN"))
struct FBNTakeCoverTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNTakeCoverTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNTakeCoverTask()
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
struct FBNStrafeTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** How far each sidestep goes. Short on purpose: this is a fighter shifting its weight, not
	 *  a flank. Long steps read as the bot losing interest and walking off mid-burst. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float StepDistance = 300.f;

	/** How often a new step is taken. */
	/** Negative asks the TIER, like the aim cone. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float StepIntervalSeconds = -1.f;

	/** Internal: countdown to the next step. */
	float SecondsUntilStep = 0.f;

	/** Internal: which way the next step goes. Flipped every step, and flipped AGAIN when a step
	 *  fails — a bot with its back to a wall must not keep walking into it. */
	bool bStepRight = false;

	/** Internal: warned once. A strafe that can never path is worth one line, not one per step. */
	bool bWarnedStepFailed = false;

	/** Every Nth sidestep becomes a JUKE — the step plus a jump. Not every step: a bot airborne
	 *  half the fight cannot shoot straight and reads as a bug, while one that occasionally leaves
	 *  the ground reads as a player. Zero disables the juke entirely. */
	/** Negative asks the TIER; zero disables the juke for this state outright. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 JukeEveryNthStep = -1;

	/** Internal: steps taken this burst, counted for the juke. */
	int32 StepCount = 0;
};

/**
 * Sidesteps while shooting. A companion task, never alone: it runs beside BN Fire Burst and BN
 * Face Target in Shoot, returns Running forever like Face Target does, and lets the burst decide
 * when the state is over.
 *
 * It is safe only because ABNCharacter aims with the CONTROLLER (`bUseControllerRotationYaw`
 * true, `bOrientRotationToMovement` false) — the body moves sideways while the aim stays on the
 * target. On a character that orients to movement this task would spin the bot away mid-burst.
 *
 * The direction never touches the global RNG (§5): it is seeded off the controller's identity and
 * flipped from there, so two bots in one fight open opposite ways and nothing else in the frame
 * is perturbed by asking. (Identity here is a pointer hash, the same basis the reaction draw
 * uses — stable within a run, not across one, which is the honest limit of both.)
 */
USTRUCT(meta = (DisplayName = "BN Strafe", Category = "BN"))
struct FBNStrafeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNStrafeTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNStrafeTask()
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

	/** Internal (R9.5): one jump per failed leg, spent BEFORE the leg is abandoned. This is the
	 *  "get out of here" case — a bot that walked into a dip, a stairwell it cannot path out of,
	 *  or against a lip between it and the point it wants. */
	bool bTriedBlockedJump = false;
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

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNCanThrowGrenadeConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** Too close and the bot blows itself up; too far and the throw falls short. The band is the
	 *  whole tactical judgement, and it is a parameter because the arc belongs to BNGA_Grenade. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MinRange = 500.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MaxRange = 2200.f;
};

/** Passes when a grenade is worth throwing AND would actually activate: a live target in the
 *  throwing band, visible, and Cooldown.Grenade not held.
 *
 *  The cooldown check is the point. BNGA_Grenade applies a 4s cooldown tag and the ASC refuses
 *  the activation while it is held, so a bot that did not look would press a dead button three
 *  times a second and fill the log with REFUSED — the same futile-press shape BN Fire Burst
 *  already refuses to make. */
USTRUCT(meta = (DisplayName = "BN Can Throw Grenade", Category = "BN"))
struct FBNCanThrowGrenadeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNCanThrowGrenadeConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNCanThrowGrenadeCondition() = default;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FBNThrowGrenadeTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** Ceiling on the wait, not the throw's length — the montage owns that. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float TimeoutSeconds = 2.5f;

	float SecondsElapsed = 0.f;
};

/** Presses the SAME Input.Grenade a human's grenade key presses, then waits for the cooldown tag
 *  to appear — which is the honest proof the ability ACTIVATED rather than was refused. Nothing
 *  about the arc, the fuse, the damage or the projectile lives here; all of it stays inside
 *  BNGA_Grenade, where the purity contract keeps it. */
USTRUCT(meta = (DisplayName = "BN Throw Grenade", Category = "BN"))
struct FBNThrowGrenadeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBNThrowGrenadeTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FBNThrowGrenadeTask()
	{
		bShouldCallTick = true;
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
