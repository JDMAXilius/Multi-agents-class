#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "AIBStateTreeTasks.generated.h"

class AAIController;
class APawn;

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

	/** How the branch claims a current want. Default: EXACT equality — the 1:1 mirror.
	 *  The Mode gate widens it to a hierarchy match, because a host's want is a CHILD
	 *  tag (AIBot.Ambition.Mode.Hold) that exact == can never equal (W-AUDIT P6
	 *  finding 5 — the statue beside the objective). */
	virtual bool Matches(const FGameplayTag& Current) const;
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

/** Gates the SEEK branch — "I have somewhere to be" (AIBot.Ambition.Seek). The owed
 *  serial rename LANDED 26 Aug (old name: SeekWeapon), in the same commit that edits
 *  the probe list — a tree REBUILD is required before the next PIE run, and AIB11
 *  already owes one, which is why the rename rode this moment and no earlier one. */
USTRUCT(meta = (DisplayName = "AIB Gate: Seek", Category = "AIBot"))
struct FAIBGateSeekCondition : public FAIBAmbitionGateCondition
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

/** Gates the MODE branch: claims any want UNDER AIBot.Ambition.Mode (the parent itself
 *  included — the tags header serves both readings on purpose). One branch serves every
 *  mode ambition; which objective it walks to is the kind join, resolved per entry. */
USTRUCT(meta = (DisplayName = "AIB Gate: Mode", Category = "AIBot"))
struct FAIBGateModeCondition : public FAIBAmbitionGateCondition
{
	GENERATED_BODY()
	virtual FGameplayTag GetBranchTag() const override;
	virtual bool Matches(const FGameplayTag& Current) const override;
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

	/** FOOTWORK OWNS THE FIGHT (founder, 27 Aug: "they don't seem to be strafing at
	 *  all"): with the target VISIBLE inside this range, this mover stands down entirely
	 *  and the strafe task owns the legs — the old shape closed to 350uu before any
	 *  footwork could run, so bots beelined instead of fighting at range, and the strafe
	 *  measured gated-out. MIRRORS FAIBStrafeTaskInstanceData::FightRangeUU on purpose
	 *  (two movers issuing in one tick cancel per tick — one number is the whole
	 *  arbitration). Closing resumes the moment sight is lost (this task fails) or the
	 *  target is beyond this range. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float FightRangeUU = 900.f;

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

	/** Swap scratch. A THROTTLE: the verb behind it is a tap on an ability that can
	 *  silently refuse, and an untimed re-tap is a button pressed at tick rate forever.
	 *  The GRENADE's and the MELEE's throttles are deliberately NOT here — they live on
	 *  controller-owned policy state, because instance data is re-initialised on every
	 *  state entry and Engage re-enters whenever a belief blinks (W-REVIEW P4+5 H2: a
	 *  per-task melee countdown gave a free swing per blink). The swap pair stays: its
	 *  worst blink cost is one extra wheel press, which the equipment cycle absorbs. */
	float SwapCooldownLeft = 0.f;
	int32 SwapPresses = 0;

	// -- ADS (founder, 27 Aug: "the aibot doing ADS properly") --------------------------
	/** The sights come up at mid range and stay down in a knife fight: closer than this,
	 *  hip fire tracks better and the speed penalty costs a duel. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AimRangeUU = 500.f;

	/** And no further than the fight range: beyond it the mover is CLOSING at sprint,
	 *  and the host's sprint-exclusion would bounce every press — mirrors FightRangeUU
	 *  so the aim band is exactly the band where footwork owns the legs. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AimMaxRangeUU = 900.f;

	/** The beat a human takes to re-raise the sights after a DESCOPE (the host cancels
	 *  the aim on a landed hit). Not zero on purpose: an instant re-scope erases the
	 *  descope's whole point and reads as a machine. Instance-data blink cost: one
	 *  extra beat per belief blink, absorbed like the swap throttle's. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ReAimSeconds = 0.75f;

	float ReAimCooldownLeft = 0.f;
	bool bAimHeld = false;
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

	// -- AIB19 grapple traversal (the wandering variant only; MayGrappleTraverse gates).
	// -- New UPROPERTYs on an existing instance struct default on load: the authored
	// -- tree asset needs NO regen for any of this.
	/** Odds one wander entry becomes a climb, when grounded, skilled, off cooldown and
	 *  the host offers a route meaningfully above. Roll, not schedule: patrols should
	 *  sometimes climb, not queue for the lift. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ClimbChance = 0.35f;

	/** Odds one wander entry from ON HIGH becomes the drop back down. Higher than the
	 *  climb's on purpose — a deck is a visit, not a residence, and the island's wander
	 *  draws cannot leave it any other way. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float DescendChance = 0.5f;

	/** A route only counts as a CLIMB when its anchor is at least this far above the
	 *  bot (and as a DROP when its approach is at least this far below) — stairs and
	 *  slopes stay the mover's ordinary business. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MinTraverseRiseUU = 250.f;

	/** Close enough to the approach point to stop and take the shot. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ApproachReachUU = 140.f;

	/** The press waits for the view to be INSIDE this cone of the anchor — the host's
	 *  hook traces where the eyes look, and a press mid-turn hooks a wall. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AimToleranceDeg = 4.f;

	/** A steer that never settles (geometry between, a fight yanking the view) aborts
	 *  to the plain wander instead of standing forever at the approach. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AimTimeoutSeconds = 1.5f;

	/** The ride's ceiling: pressed but never flew, or flew and snagged — either way the
	 *  branch moves on. The whiff is logged; the bot is never stranded (F7). */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RideTimeoutSeconds = 4.f;

	/** One traverse per this many seconds (whiffs retry at a third of it). The host's
	 *  own ability cooldown backstops spam besides — two clocks, different owners. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float TraverseCooldownSeconds = 30.f;

	/** 0 none · 1 walk-to-approach · 2 aim · 3 ride · 4 walk-to-lip · 5 drop.
	 *  Plain state, not serialized tuning — reset whenever a traverse starts or ends. */
	uint8 TraversePhase = 0;
	FVector RouteApproach = FVector::ZeroVector;
	FVector RouteAnchor = FVector::ZeroVector;
	float PhaseSeconds = 0.f;
	double NextTraverseAllowedSeconds = 0.0;
	bool bAirborneSeen = false;
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

	/** AIB19: may an idle leg become a grapple climb or a lip drop? ROAM ONLY — a Seek
	 *  leg is chasing something real and must not detour through the ceiling. */
	virtual bool MayGrappleTraverse() const { return false; }

protected:
	/** The traverse micro-machine's tick, shared so EnterState stays readable. Returns
	 *  true while a traverse owns this frame; false hands the frame back to the plain
	 *  wander flow. Defined beside the task in the .cpp. */
	EStateTreeRunStatus TickTraverse(class AAIBBotController& Bot, APawn& Pawn,
		FInstanceDataType& InstanceData, float DeltaTime) const;
};

/** SEEK's mover: somewhere to be, in order — the matured target belief, else any POI a
 *  provider offers, else a random reachable point. It CANNOT fail for want of a
 *  destination, which is the whole point of the 25 Aug replacement: the ambition it
 *  serves must never be able to strand a bot.
 *
 *  Renamed with the probe list 26 Aug (old name: MoveToWeaponPOI — it no longer hunts
 *  weapons, and AIBot.POI.Weapon is gone with the concept). NOTE the recorded
 *  divergence (W-AUDIT P7): this comment's "else any POI a provider offers" overstates
 *  the code — EnterState goes belief -> wander and consults no POI yet. Whoever wires
 *  Seek POIs inherits the claims filter obligation (P7 packet, ruling 7). */
USTRUCT(meta = (DisplayName = "AIB Seek Destination", Category = "AIBot"))
struct FAIBSeekDestinationTask : public FAIBMoveToPOITask
{
	GENERATED_BODY()
	virtual bool ShouldWanderWithoutProvider() const override { return true; }
	virtual bool ShouldMoveToBeliefFirst() const override { return true; }
};

////////////////////////////////////////////////////////////////////

USTRUCT()
struct FAIBMoveToObjectiveTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> Controller;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadiusUU = 200.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float GiveUpAfterNoProgressSeconds = 8.f;

	FVector Goal = FVector::ZeroVector;
	bool bHasGoal = false;
	float ClosestSoFarUU = 0.f;
	float SecondsWithoutProgress = 0.f;

	/** Arrival distance for the goal actually chosen: the objective's own ReachRadiusUU
	 *  when it declares one, else AcceptanceRadiusUU. Captured at EnterState so Tick
	 *  cannot drift from the test EnterState used. */
	float GoalReachUU = 200.f;

	/** THE GOAL FOLLOWS THE POI (BN22 W-REVIEW H1): re-pick on this cadence, because a
	 *  Rally POI is a pawn and pawns move — a once-snapshotted goal statued bots at a
	 *  teammate's abandoned spot. The hill re-picks itself unchanged; the cadence is the
	 *  repath interval's own number. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RepollIntervalSeconds = 0.5f;
	float RepollCooldown = 0.f;

	FAIBLocomotionState Locomotion;
};

/** The MODE branch's mover (Phase 6): walks to the best world-query POI whose Kind is
 *  the CURRENT ambition's ObjectiveKind (the controller's cached mode set — data, not a
 *  serialized node parameter), then STANDS ON IT — a hill is held by being there, so
 *  arrival never completes; SweepLook rides beside, and the sentinel ends the branch
 *  when the want moves on. No POI for a servable want = the provider under-delivered:
 *  fail LOUDLY (F7). */
USTRUCT(meta = (DisplayName = "AIB Move To Objective", Category = "AIBot"))
struct FAIBMoveToObjectiveTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAIBMoveToObjectiveTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FAIBMoveToObjectiveTask() { bShouldCallTick = true; }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
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
	 *  orbiting: without it a long leg at close range swings most of a circle.
	 *  55, DERIVED (founder's strafe review, 26 Aug): the walk is the CHORD, whose
	 *  midpoint dips inward by R(1-cos(arc/2)) — at 70 deg that was 18% of range
	 *  (63uu at the 350 gate), which is what fed the inward spiral; at 55 deg the dip
	 *  is 11% (40uu), inside the mover's own 50uu acceptance noise, and the 323uu
	 *  chord still fills an Expert's average leg (375uu wanted) while a Trained leg
	 *  goes bursty — step, plant, step — which is footwork, not a rush. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float MaxArcDegrees = 55.f;

	/** The FLOOR of the stand-off band. Chord dips captured by mid-walk leg decisions
	 *  re-normalize OUT to at least this radius on the next leg (the spiral fix), so
	 *  a fight never creeps from station-keeping into melee-accident range. Above the
	 *  audited weapon reach (120uu x 0.8 commit) with margin; below FightRangeUU
	 *  so the band is real. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float StandOffMinUU = 280.f;

	/** Strafe whenever the fight is INSIDE this range of the belief — the whole visible
	 *  mid-range fight, not just the 350uu station (founder, 27 Aug). Mirrors
	 *  FAIBMoveNearBeliefTaskInstanceData::FightRangeUU on purpose: inside it the mover
	 *  stands down and footwork owns the legs; outside it the mover closes and this task
	 *  holds — one number, one arbitration, never two movers in one tick.
	 *
	 *  RENAMED from EngagedRadiusUU deliberately: the authored tree serialized the old
	 *  350 default into its task instance data, and a rename is what drops that stale
	 *  value to this fresh default on load — a plain default change would have been
	 *  silently pinned by the asset. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float FightRangeUU = 900.f;

	/* The last-actuated-leg stamp used to live here — and re-initialised on every Engage
	 * re-entry, so one leg re-fired per belief blink (W-REVIEW P4+5 H1). It is
	 * FAIBMovementState::LastActuatedLegStamp now, beside the leg clock it compares to. */
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

/** Roam's mover: any POI a provider offers, else a random reachable point — and, when
 *  the host publishes grapple routes (AIB19), sometimes the climb up or the drop down. */
USTRUCT(meta = (DisplayName = "AIB Wander", Category = "AIBot"))
struct FAIBWanderTask : public FAIBMoveToPOITask
{
	GENERATED_BODY()
	virtual bool ShouldWanderWithoutProvider() const override { return true; }
	virtual bool MayGrappleTraverse() const override { return true; }
};
