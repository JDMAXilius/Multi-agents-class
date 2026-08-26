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

/**
 * Every mover's locomotion scratch: the sprint HOLD's edge state and the wedge-jump
 * watchdog. Deliberately NOT a UPROPERTY and not a USTRUCT — it is per-run state, never
 * authored and never serialized, so adding it changes nothing the StateTree compiler
 * bakes into /Game/AIBot/AI/ST_AIBBot (no node, no gate, no branch order moved).
 */
struct FAIBLocomotionState
{
	bool bSprintHeld = false;
	bool bTriedWedgeJump = false;
	bool bHasBestPoint = false;
	float StallSeconds = 0.f;
	FVector BestPoint = FVector::ZeroVector;
};

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

/** Gates Roam — IN USE again since the Phase-3 W-REVIEW barrier: the always-selectable
 *  state the compiler demands is now the dedicated ungated Fallback branch (sentinel +
 *  FAIBUnservedWantTask), so every REAL ambition, Roam included, keeps its gate and the
 *  tree mirrors arbitration 1:1. The seven-statue failure this gate once caused was the
 *  t=0 invalid-ambition cold start, closed at its root by the controller's
 *  Think-before-StartLogic — not by ungating the floor. */
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

	/** Floor between move requests. Out of position for ANY reason — the belief drifted
	 *  or the BOT was displaced (knockback, a pad) — re-closes on this cadence, which is
	 *  also what keeps pathfinds off per-frame cost (W-REVIEW P3 M3: the old drift-only
	 *  trigger left a knocked-back bot standing at the wrong post while the belief held
	 *  still). */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RepathIntervalSeconds = 0.5f;

	FVector LastGoal = FVector::ZeroVector;
	float RepathCooldown = 0.f;
	FAIBLocomotionState Locomotion;
};

/** Closes toward the belief and then KEEPS STATION there — sprinting while there is
 *  ground to cover and walking the last stretch, because a bot that sprints into its own
 *  firing position arrives unable to shoot (the sprint state holds while the key is down).
 *  It — it never succeeds, because
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

	/** Reload scratch: the re-tap throttle, and whether WE are the ones holding the
	 *  crouch (so the task uncrouches only what it crouched). */
	float ReloadCooldownLeft = 0.f;
	bool bCrouchedToReload = false;

	/** Close-quarters, blast and swap scratch. Every one of these is a THROTTLE, and each
	 *  exists because the verb behind it is a tap on an ability that can silently refuse:
	 *  an untimed re-tap is a button pressed at tick rate forever. The GRENADE's throttle
	 *  is deliberately NOT here — it lives on the controller, because instance data is
	 *  re-initialised on every state entry and Engage re-enters whenever a belief blinks. */
	float MeleeCooldownLeft = 0.f;
	float SwapCooldownLeft = 0.f;
	int32 SwapPresses = 0;
};

/** Presses Verb_Fire in bursts while the cached FACTS say the weapon can fight and the
 *  target is visible — one info door, never the avatar's raw reads. Releases on exit
 *  ALWAYS: a held verb on the persistent ASC outlives the body (the host's sprint-leak
 *  lesson).
 *
 *  It also owns RELOAD and the crouch that goes with it: below a magazine fraction with
 *  reserve left, the burst stops, the bot crouches, and it taps Verb_Reload — the host's
 *  own rule and its reasoning (a reload is the only moment a bot chooses to spend seconds
 *  it cannot shoot back in, so it spends them small). Reload lives HERE rather than in a
 *  branch of its own because a new branch means a new node struct, and the node list is
 *  pinned outside this module.
 *
 *  AND THE REST OF THE FIGHT'S VOCABULARY, for the same pinned-node reason: the SWAP to
 *  whatever the avatar says is right for this range, the MELEE inside the held weapon's
 *  own reach, and the GRENADE inside a band, on a cooldown. Priority is the order they
 *  are asked in and it is deliberate — hands-busy first (reload), then holding the right
 *  thing, then the point-blank answer, then the area one, then the trigger. */
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

	/** No closer approach to the goal for this long = the path is dead: fail LOUDLY
	 *  instead of standing in the open "fleeing" forever (W-REVIEW P3 H3). */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float GiveUpAfterNoProgressSeconds = 6.f;

	FVector FleeGoal = FVector::ZeroVector;
	float ClosestSoFarUU = 0.f;
	float SecondsWithoutProgress = 0.f;
	FAIBLocomotionState Locomotion;
};

/** RUNS — sprinting — directly away from the threat belief; with NO threat point held at
 *  all (hurt with nothing seen, heard, or remembered) it REPOSITIONS to a random
 *  reachable point, because Retreat must always have an executable exit — a hurt bot
 *  frozen mid-arena while hysteresis defends the frozen want was W-REVIEW P3 H1.
 *  Succeeds at the goal; the brain's Retreat score deciding when to STOP fleeing is
 *  arbitration's job, not this task's. */
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

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float GiveUpAfterNoProgressSeconds = 8.f;

	float ClosestSoFarUU = 0.f;
	float SecondsWithoutProgress = 0.f;
	FAIBLocomotionState Locomotion;
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

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float GiveUpAfterNoProgressSeconds = 8.f;

	FVector Goal = FVector::ZeroVector;
	bool bHasGoal = false;
	float ClosestSoFarUU = 0.f;
	float SecondsWithoutProgress = 0.f;
	FAIBLocomotionState Locomotion;
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

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAIBStrafeTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	/** FLOOR on one step's arc length. The real distance is derived from the leg in
	 *  flight (remaining seconds * max speed) so footwork fills its leg at every rung;
	 *  this is only the minimum, so a nearly-expired leg still reads as a step. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float StepDistanceUU = 220.f;

	/** Cap on how far around the belief ONE leg may carry the bot. Footwork, not
	 *  orbiting: without it a long leg at close range swings most of a circle. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MaxArcDegrees = 70.f;

	/** Strafe only while STATION-KEEPING — inside this radius of the belief. Mirrors
	 *  FAIBMoveNearBeliefTask's acceptance radius on purpose: outside it the mover owns
	 *  the legs, and two tasks issuing moves at once cancel each other per tick. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float EngagedRadiusUU = 350.f;

	/** The policy leg this task last actuated (its NextDecisionAt stamp) — one lateral
	 *  move per LEG, never per tick: per-tick MoveToLocation is a pathfind per frame. */
	double LastActuatedLegStamp = 0.0;
};

/** PHASE 4's footwork (the host's R9 lesson: a bot that stands perfectly still while
 *  firing is the loudest tell that it is not a person). FAIBMovementPolicy decides the
 *  RHYTHM — whether this level strafes at all, the leg cadence, the juke — from the
 *  controller's per-life state; this task only actuates: one lateral navmesh-projected
 *  step per leg, perpendicular to the belief line. Runs beside the burst and NEVER
 *  completes or fails — the fight's other tasks own the state's fate. */
USTRUCT(meta = (DisplayName = "AIB Strafe", Category = "AIBot"))
struct FAIBStrafeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBStrafeTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FAIBStrafeTask() { bShouldCallTick = true; }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

////////////////////////////////////////////////////////////////////

/** The Fallback branch's stand-and-report task (the Phase-3 W-REVIEW ruling): selected
 *  only when the brain's current want maps to NO branch — a Phase-6 mode ambition before
 *  its branch exists — it stands still and says WHY, once, at Warning (F7). The sentinel
 *  beside it ends the branch the moment the want becomes servable. This is what lets
 *  every real ambition, Roam included, keep its gate while the compiler still gets its
 *  always-selectable state. */
USTRUCT(meta = (DisplayName = "AIB Unserved Want", Category = "AIBot"))
struct FAIBUnservedWantTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBAmbitionSentinelTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/** Roam's mover: any POI a provider offers, else a random reachable point. */
USTRUCT(meta = (DisplayName = "AIB Wander", Category = "AIBot"))
struct FAIBWanderTask : public FAIBMoveToPOITask
{
	GENERATED_BODY()
	virtual bool ShouldWanderWithoutProvider() const override { return true; }
};
