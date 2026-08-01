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

/**
 * BRStateTreeTasks — LAYER 2's entire C++ surface.
 *
 * WHERE "BEHAVIOR TREE" LIVES (ruling R9, and the question this file exists to answer):
 * StateTree is UE 5.8's production successor to the BT + AIController stack, and Halo's
 * contribution was never the asset format — it was the PATTERN: prioritized, hierarchical
 * action selection. So we keep the pattern and drop the second asset. Engage's task is a
 * priority selector (rocket-if-held -> grenade-if-shields-cracked -> fire-primary ->
 * melee-in-range) written as ordinary C++ inside one StateTree task. There is no
 * BehaviorTree asset in this project and there is not going to be one; if StateTree ever
 * hits a genuine wall the answer is a contract_gap to the lead, not a quiet second brain.
 *
 * THE DIVISION OF LABOUR, restated because it is the whole design:
 *   - the BRAIN decides WHAT (ambition) and roughly WHERE (an abstract anchor);
 *   - the TREE decides which stance expresses that, and its tasks decide WHICH BUTTON;
 *   - GAS decides whether the button does anything.
 * A task that scores a goal is a layering violation. A brain that names an InputTag is too.
 *
 * NO TICK IN ANY TASK. Every task below sets bShouldCallTick = false and returns Running;
 * work finishes through a delegate (move completion) or a data-rate timer (the Engage
 * selector) on ABRBotController, which then reports to the brain, which changes the step,
 * which the tree's transition conditions read. Nothing polls gameplay per frame.
 *
 * ---------------------------------------------------------------------------------------
 * WHAT `Content/AI/ST_Bot` MUST CONTAIN — the authoring spec for the Tier-4 packet.
 *
 * ST_Bot is the ONE StateTree asset in the project (all tiers, forever). It is an asset and
 * not C++ only because UE 5.8 has no C++ path for a state graph (R18 Tier 4). Everything it
 * needs to bind to is declared in this header; it must invent nothing.
 *
 *   Schema:  StateTreeAIComponentSchema (it runs on ABRBotController's UStateTreeAIComponent).
 *
 *   ROOT
 *   +- Dead            [BR Precondition: SelfDead]                       -> no tasks
 *   +- ControlRocket   [BR Ambition Is: ControlRocket]
 *   |    +- Approach   [BR Step Verb Is: MoveTo]      -> BR Move To Step Anchor
 *   |    |                                              (Query = EQS_Bot_PadApproach)
 *   |    +- Watch      [BR Step Verb Is: Hold]        -> BR Hold Sightline
 *   |    +- Contest    [BR Step Verb Is: PickupOrContest] -> BR Contest Rocket
 *   +- WinThisFight    [BR Ambition Is: WinThisFight]
 *   |    +- Flush      [BR Step Verb Is: Flush]       -> BR Flush
 *   |    +- Engage     [BR Step Verb Is: Engage]      -> BR Engage
 *   |    +- Seek       [BR Step Verb Is: MoveTo]      -> BR Move To Step Anchor
 *   |                                                   (Query = EQS_Bot_FiringPoint)
 *   +- Survive         [BR Ambition Is: Survive]
 *   |    +- Retreat    [BR Step Verb Is: Retreat]     -> BR Move To Step Anchor
 *   |    |                                              (Query = EQS_Bot_CoverFromThreat)
 *   |    +- HoldCover  [BR Step Verb Is: Hold]        -> BR Hold Sightline
 *   |    +- Regroup    [BR Step Verb Is: Regroup]     -> BR Move To Step Anchor
 *   |                                                   (Query = EQS_Bot_Teammate)
 *   +- TakeGround      [BR Ambition Is: TakeGround]   -> MoveTo / Hold, as above
 *   +- HuntWeapon      [BR Ambition Is: HuntWeapon]   -> MoveTo / Contest, as above
 *   +- Roam            [BR Ambition Is: Roam]         -> BR Move To Step Anchor
 *                                                       (Query = EQS_Bot_Landmark)
 *
 *   Transitions: every state transitions to ROOT on condition failure, so a step change in
 *   the brain re-selects a stance. No state transitions to a SIBLING directly — stance
 *   changes go through the root selector, which is what keeps "what is this bot doing" a
 *   single readable question (R12).
 *
 *   EQS assets (Content/AI/, same packet), each scoring the AUTHORED vocabulary only —
 *   contexts from BREnvQueryContexts.h, tests and weights inside the asset:
 *     EQS_Bot_PadApproach     generator: points around UBREnvQueryContext_RocketPad;
 *                             tests: LoS to pad, distance, cover (UBREnvQueryContext_Threat)
 *     EQS_Bot_FiringPoint     generator: points around UBREnvQueryContext_Threat;
 *                             tests: LoS to threat, distance <= the 35 m sightline cap
 *     EQS_Bot_CoverFromThreat generator: UBREnvQueryContext_CoverPoints (actors);
 *                             tests: NO LoS from threat, path distance
 *     EQS_Bot_Perch           generator: UBREnvQueryContext_GrapplePerches
 *     EQS_Bot_Teammate        generator: UBREnvQueryContext_Teammates
 *     EQS_Bot_Landmark        generator: UBREnvQueryContext_StepAnchor
 *
 *   FORBIDDEN in the asset: any second BehaviorTree (R9); any per-tier variant (one tree,
 *   scalars do the rest); any numeric gameplay constant that belongs in DT_BotTuning; any
 *   generator that samples raw navmesh instead of the manifest's authored places.
 * ---------------------------------------------------------------------------------------
 */

// =============================================================================================
// Shared helpers
// =============================================================================================

namespace BRBotStateTree
{
	/** The bot controller running this tree, or null (which every task treats as "fail loudly"). */
	BREACHPOINT_API ABRBotController* GetBotController(struct FStateTreeExecutionContext& Context);
}

namespace BRBotEngage
{
	/**
	 * ONE pass of the Engage priority selector — the BT-shaped core of this design.
	 *
	 * Ordered, first-match-wins, exactly like a BT's prioritized selector node, and every
	 * branch's gate is a GAS question (CanActivateByInputTag), so the selector can never
	 * pick a branch the ASC would refuse:
	 *
	 *   1. hold the power weapon and have a shot   -> fire it
	 *   2. target's shields are cracked, grenade up -> grenade (the finisher)
	 *   3. target in melee range                    -> melee
	 *   4. otherwise                                -> primary fire
	 *
	 * The ORDER is behaviour and is reviewed as behaviour (R12: a bot whose branch order
	 * reads as random fails review even if it wins more). The RATES and ACCURACY that make
	 * each branch feel like a tier are all data.
	 *
	 * Called from ABRBotController's engage timer, never from a Tick.
	 */
	BREACHPOINT_API void RunSelectorOnce(ABRBotController& Bot, AActor* Target);
}

// =============================================================================================
// Conditions — how ST_Bot's states select on layer 1's decision
// =============================================================================================

USTRUCT()
struct BREACHPOINT_API FBRSTCondition_AmbitionIsInstanceData
{
	GENERATED_BODY()

	/** The ambition this branch of the tree serves. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EBRBotAmbition Ambition = EBRBotAmbition::None;
};

/** "Is the brain currently wanting X?" — the top-level selector of ST_Bot. */
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

/** "Is the current plan step's verb X?" — the state selector inside an ambition. */
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

/**
 * "Is this world-fact true right now?" — a live ASC/perception query, not a cached fact.
 *
 * Deliberately the SAME enum the brain plans against: one vocabulary for preconditions
 * across both layers is what stops the tree and the brain from disagreeing about the world.
 */
USTRUCT(meta = (DisplayName = "BR Precondition", Category = "Breachpoint|Bot"))
struct BREACHPOINT_API FBRSTCondition_Precondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FBRSTCondition_PreconditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

// =============================================================================================
// Tasks — the stances
// =============================================================================================

USTRUCT()
struct BREACHPOINT_API FBRSTTask_MoveToAnchorInstanceData
{
	GENERATED_BODY()

	/**
	 * The EQS query that scores WHERE to go. Authored in ST_Bot, per state, over the
	 * contexts in BREnvQueryContexts.h.
	 *
	 * A hard UObject pointer is correct HERE and only here: it is a property of an authored
	 * asset (the tree), not a reference in C++ or in a data table (law 3 governs those).
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UEnvQuery> LocationQuery;

	/** Acceptance radius in centimetres, authored per state on the tree. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float AcceptanceRadius = 0.f;
};

/**
 * Seek / Reposition / Retreat: go where the current step's anchor points.
 *
 * Runs the state's EQS query asynchronously, moves to the winner, and reports the step
 * complete on the move-completed delegate. No result or no path => ReportStepFailed(), which
 * makes the brain replan on the next event rather than leaving the bot standing in a
 * doorway. "Stuck navmesh" is a soak finding, never a silent state.
 */
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

/**
 * Engage: the fight. The BT-shaped priority selector lives here (see BRBotEngage above).
 *
 * EnterState starts the tier's selector timer and focuses the target; ExitState releases
 * every held input. Holding no state of its own is what lets one Engage state serve all
 * three tiers — the difference between Recruit and Veteran is entirely in the timer
 * interval, the accuracy, and the ambition that got them here.
 */
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

/**
 * Flush: force a target out of cover with the grenade, then let the plan's next step
 * re-engage.
 *
 * One press, one step, one report. It exists as its own state because it is a READABLE
 * beat: "the bot grenaded me out of cover" is a sentence a playtester should be able to say
 * (R12), which they cannot if the grenade is just another branch buried in Engage.
 */
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

	/** Seconds to hold before reporting the step complete. Authored on the tree, per state. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float HoldSeconds = 0.f;
};

/**
 * Hold: keep a sightline on the anchor and wait.
 *
 * The visible half of the rocket contest — a bot that walks to the pad and WATCHES it is
 * the T-10 s beat BP08's legibility box asks a playtester to name. Fires the step-complete
 * report from a timer, never from a tick.
 */
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

/**
 * ContestRocket: take the pickup if it is there, deny it if someone else is on it.
 *
 * Pickup is not an ability — walking onto the pad is what takes the weapon — so this task's
 * only actions are "keep moving onto the anchor" and, if an enemy is contesting, "press the
 * same buttons Engage would". It never grants itself a weapon; it walks like a human does.
 */
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
