// Breachpoint. The glue: events -> brain -> StateTree -> InputTags. Nothing else touches the world.

#pragma once

#include "AIController.h"
#include "AI/BRBotBrain.h"
#include "AI/BRBotFacts.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "BRBotController.generated.h"

class AActor;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBRAbilitySystemComponent;
class UEnvQuery;
class UStateTree;
class UStateTreeAIComponent;
struct FAIStimulus;
struct FPathFollowingResult;

/**
 * ABRBotController — the ONE seam between the three layers (BREACHPOINT-ARCHITECTURE §3.7).
 *
 * It owns four jobs and refuses every fifth one:
 *
 *   1. PERCEIVE. Server gameplay events and the sight sense arrive here, are delayed by the
 *      tier's seeded, quantized reaction (>= 200 ms, R11 — no path skips the clamp), and
 *      become an FBRBotEvent + an FBRBotFacts snapshot.
 *   2. THINK. It hands that pair to UBRBotBrain and gets an ambition + a step back. It never
 *      second-guesses the brain and never decides anything itself.
 *   3. EXECUTE. The step becomes StateTree state; the StateTree's tasks call back into this
 *      class to act.
 *   4. PRESS BUTTONS. PressInputTag() is the ONE function through which every bot action in
 *      the entire game passes, and it does exactly what ABRPlayerController does for a human:
 *      relays an InputTag to the ASC on its own PlayerState.
 *
 * WHAT IT WILL NOT DO — the iron rule, made concrete. There is no ApplyGameplayEffect here,
 * no SetActorLocation, no attribute write, no damage call, no spawn. A bot with a bug is a
 * bot that presses the wrong button, never a bot that edits the simulation. Movement is
 * AAIController path following, which drives the same AddMovementInput a human's stick does
 * — the pawn is never teleported or velocity-set.
 *
 * NO PRIVILEGED STATE. Targets come from the sight sense (radius/FOV from DT_BotTuning), not
 * from an actor list; enemy shield belief comes from observed cues, not from the victim's
 * ASC; aim error is applied BEFORE the fire ability activates, so the shot the server
 * validates is the shot the bot actually aimed (ticket BP08 step 3, and the only ordering
 * that makes accuracy_pct honest).
 *
 * NO TICK (law 4). Every entry point here is a delegate, a timer, or a StateTree task
 * callback.
 */
UCLASS()
class BREACHPOINT_API ABRBotController : public AAIController
{
	GENERATED_BODY()

public:
	ABRBotController(const FObjectInitializer& ObjectInitializer);

	// -- Setup (called by UBRBotManagerComponent, server only) ---------------------------

	/**
	 * Seed and configure this bot. Must be called before possession completes.
	 *
	 * @param InSeed      match seed combined with the bot's slot index by the manager, so a
	 *                    match replays identically and no two bots share a stream.
	 * @param InScalars   the tier row (DT_BotTuning), already read.
	 * @param InAmbitions the ambition table (DT_BotAmbitions), already read, in table order.
	 */
	void ConfigureBot(int32 InSeed, const FBRBotTierScalars& InScalars, const TArray<FBRBotAmbitionDef>& InAmbitions);

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// -- Layer 1 <-> layer 2 --------------------------------------------------------------

	UBRBotBrain* GetBrain() const { return Brain; }

	/** The active ambition, for StateTree conditions and for bot callouts. */
	EBRBotAmbition GetActiveAmbition() const;

	/** The current plan step, for StateTree conditions. */
	FBRBotPlanStep GetCurrentStep() const;

	/**
	 * Queue a perception/match event. Applies the tier's seeded reaction delay to everything
	 * a bot has to SEE, and dispatches immediately for things it inherently knows (its own
	 * shields breaking, its own plan step completing).
	 *
	 * This is the only way an event reaches the brain, which is what makes the 200 ms floor
	 * unbypassable rather than merely respected.
	 */
	void PostBotEvent(EBRBotEventType EventType, int32 SubjectId = INDEX_NONE);

	/** The StateTree reports the current step done / undoable. Immediate: no perception involved. */
	void ReportStepCompleted();
	void ReportStepFailed();

	// -- Layer 2 -> layer 3: THE intent function ------------------------------------------

	/**
	 * Press an ability InputTag on this bot's OWN PlayerState ASC.
	 *
	 * *** THIS IS THE ONLY WAY AI BECOMES ACTION IN BREACHPOINT. ***
	 *
	 * Identical call, identical component, identical cost/cooldown/tag gates as
	 * ABRPlayerController's human path (`UBRAbilitySystemComponent::AbilityInputTagPressed`,
	 * whose own header names this class as its second caller). There is no bot-only
	 * activation path, no direct TryActivateAbility, no GE application — which is exactly
	 * why a bot cannot out-cheat a human: the ASC would refuse it the same way.
	 *
	 * Server-only by construction: bots exist only on the authority.
	 */
	void PressInputTag(FGameplayTag InputTag);

	/** Release a held InputTag (WhileHeld abilities: sprint, automatic fire). */
	void ReleaseInputTag(FGameplayTag InputTag);

	/** Is this tag currently held by this bot? Mirrors the ASC's authoritative buffer. */
	bool IsInputTagHeld(FGameplayTag InputTag) const;

	/**
	 * Would the ASC accept this InputTag right now (cost, cooldown, blocking tags)?
	 *
	 * A GOAP precondition IS a GAS query (AI-BOTS §2) — this is that sentence as a function.
	 * The Engage selector uses it to skip a branch instead of pressing a button that would
	 * be refused, and the fact-fill uses it to set GrenadeReady/GrappleReady.
	 */
	bool CanActivateByInputTag(FGameplayTag InputTag) const;

	// -- Aim -------------------------------------------------------------------------------

	/**
	 * Point the control rotation at Target with the tier's seeded error, and nothing else.
	 *
	 * Called BEFORE PressInputTag(Fire), never after: error applied after activation would
	 * be cosmetic, and `accuracy_pct` would be a lie. The error is drawn from the BRAIN's
	 * stream (one stream per bot), so a shot sequence replays exactly.
	 */
	void AimAtTargetWithError(const AActor* Target);

	// -- Engage: the fight loop, driven by a data-rate timer (never a Tick) ----------------

	/**
	 * Enter the fight: focus the target and start re-running the Engage priority selector
	 * every `engage_update_ms`.
	 *
	 * A TIMER, not a tick (law 4), and its interval is a tuning column — a Recruit can be
	 * made to re-decide slower than a Veteran without a line of code. Called from the
	 * StateTree's Engage task; the selector itself lives in BRStateTreeTasks.cpp, where
	 * ARCHITECTURE §3.7 says it lives.
	 */
	void BeginEngage(AActor* Target);

	/** Leave the fight: release held fire, clear focus, stop the selector timer. */
	void EndEngage();

	/** The selector timer's callback. Not a Tick: it fires at the tier's own rate. */
	UFUNCTION()
	void RunEngageSelector();

	// -- Movement: EQS -> path follow -> delegate. No polling anywhere in the chain --------

	/**
	 * Run the state's EQS query and walk to the winner.
	 *
	 * The query scores the arena's AUTHORED vocabulary (BREnvQueryContexts.h), asynchronously;
	 * the result arrives on a delegate, the move ends on OnMoveCompleted, and the step is
	 * reported from there. No result, or no path, reports the step FAILED — which makes the
	 * brain replan instead of leaving a bot pressed against geometry.
	 *
	 * Movement is AAIController path following: it drives the same AddMovementInput a human's
	 * stick does. Nothing here sets a location or a velocity.
	 */
	void RequestMoveToAnchor(UEnvQuery* LocationQuery, float AcceptanceRadius);

	/** Abandon any move in flight (state exit). */
	void CancelActiveMove();

	/** Hold this position for N seconds, then report the step complete. A timer, not a tick. */
	void StartHoldTimer(float HoldSeconds);
	void CancelHoldTimer();

	/** Path following finished (or failed). The only place a MoveTo step is reported from. */
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	// -- Facts ------------------------------------------------------------------------------

	/**
	 * Snapshot everything the brain is allowed to know, normalized to [0,1].
	 *
	 * All world-unit maths lives here and nowhere else — that is what keeps UBRBotBrain
	 * free of arena constants and headless-testable.
	 */
	FBRBotFacts BuildFacts() const;

	/** The currently perceived target actor, or null. */
	AActor* GetCurrentTarget() const;

	/** This bot's ASC (on its PlayerState, never on the pawn). */
	UBRAbilitySystemComponent* GetBotASC() const;

protected:
	/**
	 * Fill the match-scale facts: rocket timer, score deficit, clock.
	 *
	 * VIRTUAL AND EMPTY BY DESIGN, with a one-time warning. Those three numbers live on
	 * ABRGameState / the rocket subsystem, which BP08 does not own and which were being
	 * written in parallel with it. This is the declared seam (BP08 contract_gap 2): whoever
	 * lands the rocket timer overrides or fills this, and until then ControlRocket simply
	 * never scores — visibly, not silently.
	 */
	virtual void FillMatchFacts(FBRBotFacts& Facts) const;

	/** Fill self vitals + the precondition mask from the ASC. */
	virtual void FillSelfFacts(FBRBotFacts& Facts) const;

	/** Fill target-derived facts from the perceived target. */
	virtual void FillTargetFacts(FBRBotFacts& Facts) const;

	// -- Perception ---------------------------------------------------------------------

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** Own State.Shields.Broken applied/removed on our ASC. */
	void HandleShieldsBrokenTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	/** Delivered after the reaction delay. The brain sees ONLY this. */
	void DispatchBotEvent(FBRBotEvent Event);

	/** Does this event class have to be perceived (delayed), or is it inherent knowledge? */
	static bool RequiresReactionDelay(EBRBotEventType EventType);

	/** Push the brain's decision into the spine and kick a re-evaluation. */
	void ApplyDecisionToSpine(const FBRBotDecision& Decision);

	// -- Components -----------------------------------------------------------------------

	/**
	 * Layer 2. The one StateTree per bot; the asset (ST_Bot) is a Tier-4 binary that C++
	 * cannot express, and it is referenced SOFTLY through DT_BotTuning — never a hard ref
	 * or ConstructorHelpers here (law 3, hook-enforced).
	 */
	UPROPERTY(VisibleAnywhere, Category = "Breachpoint|Bot")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

	/**
	 * Sight + damage senses, configured from DT_BotTuning. Engine-owned and event-delivered:
	 * we never sweep for enemies ourselves, and the geometry is the same geometry that
	 * decides what a human can see.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Breachpoint|Bot")
	TObjectPtr<UAIPerceptionComponent> BotPerception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** Layer 1. */
	UPROPERTY()
	TObjectPtr<UBRBotBrain> Brain;

	/** The perceived target. A weak pointer: a target that dies must not be kept alive by a bot's opinion. */
	TWeakObjectPtr<AActor> CurrentTarget;

	/** Match time (facts clock) at which CurrentTarget was last actually seen. */
	float LastTargetSeenSeconds = 0.f;

	/** Reaction timers in flight. Cleared on unpossess so a respawn cannot inherit a stale reaction. */
	TArray<FTimerHandle> PendingReactionTimers;

	/** The Engage selector's repeating timer. Cleared by EndEngage and by unpossess. */
	FTimerHandle EngageTimer;

	/** The Hold task's timer. */
	FTimerHandle HoldTimer;

	/** Acceptance radius carried from the task to the move request the EQS result triggers. */
	float PendingMoveAcceptanceRadius = 0.f;

	/** EQS finished: move to the winning item, or report the step failed. */
	void HandleAnchorQueryFinished(TSharedPtr<struct FEnvQueryResult> Result);

	/** Hold timer elapsed. */
	void HandleHoldElapsed();

private:
	/** Load ST_Bot from the tier's soft path and hand it to the StateTree component. */
	void ResolveStateTreeAsset();

	void OnStateTreeAssetLoaded();

	/** Bind the ASC tag/event delegates. Called once the pawn and its PlayerState exist. */
	void BindAbilitySystemEvents();
	void UnbindAbilitySystemEvents();

	/** Match clock, from the GameState. Zero (and warned once) until the seam is wired. */
	float GetMatchTimeSeconds() const;

	FDelegateHandle ShieldsBrokenHandle;

	bool bConfigured = false;

	/** Mutable so the one-time seam warning can fire from the const fact-fill path. */
	mutable bool bWarnedMatchFactsSeam = false;
};
