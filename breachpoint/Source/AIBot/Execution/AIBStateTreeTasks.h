#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "AIBStateTreeTasks.generated.h"

class AAIController;

/**
 * The tree's C++ vocabulary — the host's proven mechanical shape (plain instance-data
 * structs, nodes deriving the Common bases, an owner-fallback resolve because a
 * code-authored tree has no editor property bindings). Every node is THIN: verbs through
 * the avatar door, world through the sensorium's matured beliefs and the controller's
 * cached facts — no ASC, no game types, no decisions (deciding is the brain's).
 * Server-side only by construction: the controller exists nowhere else.
 */

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAIBAmbitionGateConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;
};

/** The one gate: a branch runs while the brain's current ambition IS this branch's tag.
 *  The executor mirrors arbitration, never re-does it.
 *
 *  The tag is a C++ VIRTUAL on a per-branch derived struct, not a node parameter: the
 *  proven authoring surface sets nothing on a node it adds (every compiled call is a
 *  zero-argument AddEnterCondition/AddTask), so anything a branch must differ by is
 *  baked into a distinct struct — the host's own pattern, and it keeps the baked value
 *  out of asset serialization entirely. */
USTRUCT(meta = (Hidden))
struct FAIBAmbitionGateCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBAmbitionGateConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	/** The base gates nothing (invalid tag never matches) — author with a derived gate. */
	virtual FGameplayTag GetBranchTag() const;
};

USTRUCT(meta = (DisplayName = "AIB Gate: Engage", Category = "AIBot"))
struct FAIBGateEngageCondition : public FAIBAmbitionGateCondition
{
	GENERATED_BODY()
	virtual FGameplayTag GetBranchTag() const override;
};

USTRUCT(meta = (DisplayName = "AIB Gate: Retreat", Category = "AIBot"))
struct FAIBGateRetreatCondition : public FAIBAmbitionGateCondition
{
	GENERATED_BODY()
	virtual FGameplayTag GetBranchTag() const override;
};

USTRUCT(meta = (DisplayName = "AIB Gate: Search", Category = "AIBot"))
struct FAIBGateSearchCondition : public FAIBAmbitionGateCondition
{
	GENERATED_BODY()
	virtual FGameplayTag GetBranchTag() const override;
};

/** Gates the SEEK branch — "I have somewhere to be" (AIBot.Ambition.Seek). The STRUCT
 *  NAME still says SeekWeapon and that is deliberate, not neglect: Tools/aib/70_aib_assets.py
 *  probes a fixed list of 16 node paths (/Script/AIBot.AIBGateSeekWeaponCondition among
 *  them) and that file is not this module's to edit. Renaming here would fail the probe
 *  and block the lead's tree build. The name is owed a serial rename in the same step
 *  that edits the probe list. */
USTRUCT(meta = (DisplayName = "AIB Gate: Seek", Category = "AIBot"))
struct FAIBGateSeekWeaponCondition : public FAIBAmbitionGateCondition
{
	GENERATED_BODY()
	virtual FGameplayTag GetBranchTag() const override;
};

/** Vocabulary only — the built tree does NOT use it. Roam is the LAST child and carries
 *  no enter condition, so it is the fallback selection can never miss; a gate there would
 *  hand the tree back the failure mode it just cost seven bots (see AIBTreeAuthoring.cpp).
 *  Kept because a tree that gates Roam explicitly is a legitimate future authoring. */
USTRUCT(meta = (DisplayName = "AIB Gate: Roam", Category = "AIBot"))
struct FAIBGateRoamCondition : public FAIBAmbitionGateCondition
{
	GENERATED_BODY()
	virtual FGameplayTag GetBranchTag() const override;
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAIBAmbitionSentinelTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** The ambition that selected this branch, cached at enter. */
	FGameplayTag AmbitionAtEnter;
};

/** Rides beside every branch's tasks and SUCCEEDS the moment the brain's current
 *  ambition is no longer the one that selected the branch. Gates are enter conditions —
 *  checked at selection, never after — so without this task a running branch would
 *  outlive its want (a bot bursting at a corpse while wanting Retreat). One struct for
 *  all branches: it compares against what it CACHED, so it needs no per-branch tag. */
USTRUCT(meta = (DisplayName = "AIB Ambition Sentinel", Category = "AIBot"))
struct FAIBAmbitionSentinelTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBAmbitionSentinelTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FAIBAmbitionSentinelTask() { bShouldCallTick = true; }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAIBFaceBeliefTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** Degrees per second the control rotation may swing. A teleporting aim is an aimbot;
	 *  the turn is applied HERE because the controller has no tick (the host's lesson —
	 *  focus alone never turns a tickless controller). */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float TurnDegreesPerSecond = 360.f;
};

/** Faces the sensorium's BELIEF — never the live actor. During the juke window this is
 *  the frozen last-seen spot, which is exactly the wall-track ban working. Aim ERROR is
 *  Phase 4's AimPolicy; Phase 3 faces the belief precisely. */
USTRUCT(meta = (DisplayName = "AIB Face Belief", Category = "AIBot"))
struct FAIBFaceBeliefTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBFaceBeliefTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FAIBFaceBeliefTask() { bShouldCallTick = true; }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAIBMoveNearBeliefTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadiusUU = 350.f;

	/** Re-issue the move when the belief has drifted this far from the last goal. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RepathAtDriftUU = 200.f;

	FVector LastGoal = FVector::ZeroVector;
};

/** Closes toward the belief and then KEEPS STATION there — it never succeeds, because
 *  the branch runs all its tasks at once (move + face + fire together is the whole
 *  Halo read) and a mover that completed on arrival would complete the state per frame
 *  while standing in range. It fails on visibility loss; the sentinel ends the branch
 *  when the want moves on. */
USTRUCT(meta = (DisplayName = "AIB Move Near Belief", Category = "AIBot"))
struct FAIBMoveNearBeliefTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBMoveNearBeliefTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FAIBMoveNearBeliefTask() { bShouldCallTick = true; }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAIBFireWhenAbleTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float BurstSeconds = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float BetweenBurstsSeconds = 0.4f;

	float PhaseSecondsLeft = 0.f;
	bool bHolding = false;
};

/** Presses Verb_Fire in bursts while the cached FACTS say the weapon can fight and the
 *  target is visible — one info door, never the avatar's raw reads. Releases on exit
 *  ALWAYS: a held verb on the persistent ASC outlives the body (the host's sprint-leak
 *  lesson). */
USTRUCT(meta = (DisplayName = "AIB Fire When Able", Category = "AIBot"))
struct FAIBFireWhenAbleTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBFireWhenAbleTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FAIBFireWhenAbleTask() { bShouldCallTick = true; }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAIBFleeFromBeliefTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float FleeDistanceUU = 900.f;

	FVector FleeGoal = FVector::ZeroVector;
};

/** Runs directly away from the threat belief. Succeeds at the flee goal; the brain's
 *  Retreat score deciding when to STOP fleeing is arbitration's job, not this task's. */
USTRUCT(meta = (DisplayName = "AIB Flee From Belief", Category = "AIBot"))
struct FAIBFleeFromBeliefTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBFleeFromBeliefTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FAIBFleeFromBeliefTask() { bShouldCallTick = true; }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAIBMoveToLastKnownTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadiusUU = 150.f;
};

/** Walks to the memory's fresh last-known spot, then STANDS there (SweepLook rides
 *  beside — the hunting read). SUCCEEDS on spotting someone (arbitration is already
 *  swinging); FAILS when the memory has gone stale — Root re-selects, and the Search
 *  ambition's own freshness decay agrees by data. Never completes just for arriving:
 *  that would re-select the branch per frame while the want holds. */
USTRUCT(meta = (DisplayName = "AIB Move To Last Known", Category = "AIBot"))
struct FAIBMoveToLastKnownTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBMoveToLastKnownTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FAIBMoveToLastKnownTask() { bShouldCallTick = true; }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAIBSweepLookTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float SweepDegreesPerSecond = 90.f;
};

/** The searching look: swings the control yaw at a steady rate. Never completes on its
 *  own — it rides beside a move task and ends when the state does. */
USTRUCT(meta = (DisplayName = "AIB Sweep Look", Category = "AIBot"))
struct FAIBSweepLookTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBSweepLookTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FAIBSweepLookTask() { bShouldCallTick = true; }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAIBMoveToPOITaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float WanderRadiusUU = 2500.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadiusUU = 150.f;

	FVector Goal = FVector::ZeroVector;
	bool bHasGoal = false;
};

/** Walks to a point worth being at — a provider's POI when one exists, else (for the
 *  wandering variant only) a random reachable point. Succeeds on arrival. What a branch
 *  seeks and whether it may wander are C++ virtuals on derived structs — the gate's
 *  reasoning, same paragraph. */
USTRUCT(meta = (Hidden))
struct FAIBMoveToPOITask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBMoveToPOITaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FAIBMoveToPOITask() { bShouldCallTick = true; }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Matches FAIBPointOfInterest::Kind — the typed join. Invalid = no kind sought. */
	virtual FGameplayTag GetPOIKind() const;

	/** With no world-query provider: true = wander to a random reachable point (the
	 *  honest fallback); false = FAIL loudly. A mover that can fail is a branch that can
	 *  strand its bot, so only a mover nothing else falls back to may answer false. */
	virtual bool ShouldWanderWithoutProvider() const;

	/** True = the matured target belief outranks any POI as the place to be. Seek's
	 *  answer; Roam's wander must NOT chase people it cannot fight. */
	virtual bool ShouldMoveToBeliefFirst() const { return false; }
};

/** SEEK's mover: somewhere to be, in order — the matured target belief, else any POI a
 *  provider offers, else a random reachable point. It CANNOT fail for want of a
 *  destination, which is the whole point of the 25 Aug replacement: the ambition it
 *  serves must never be able to strand a bot.
 *
 *  The struct name is frozen by the probe list (see FAIBGateSeekWeaponCondition); it no
 *  longer hunts weapons, and AIBot.POI.Weapon is gone with the concept. */
USTRUCT(meta = (DisplayName = "AIB Seek Destination", Category = "AIBot"))
struct FAIBMoveToWeaponPOITask : public FAIBMoveToPOITask
{
	GENERATED_BODY()
	virtual bool ShouldWanderWithoutProvider() const override { return true; }
	virtual bool ShouldMoveToBeliefFirst() const override { return true; }
};

/** Roam's mover: any POI a provider offers, else a random reachable point. */
USTRUCT(meta = (DisplayName = "AIB Wander", Category = "AIBot"))
struct FAIBWanderTask : public FAIBMoveToPOITask
{
	GENERATED_BODY()
	virtual bool ShouldWanderWithoutProvider() const override { return true; }
};
