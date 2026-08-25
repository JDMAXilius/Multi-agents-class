#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "BNBotController.generated.h"

class ABNWeapon;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UBNAbilitySystemComponent;
class UBNBotBrain;
class UStateTree;
class UStateTreeAIComponent;
enum class EBNBotAmbition : uint8;
struct FBNBotTuningRow;
struct FAIStimulus;
struct FOnAttributeChangeData;

/** The bot's head and hand. bWantsPlayerState gives it a real ABNPlayerState — the ASC, the
 *  abilities, the weapons, the score all ride that, exactly as a human's do. The controller adds
 *  only what a player does not have: eyes (sight perception) and a will (the StateTree). It never
 *  calls TryActivateAbility: it presses the same input tags ABNPlayerController's handlers press. */
UCLASS(Config=Game)
class BREACHPOINTNEXT_API ABNBotController : public AAIController
{
	GENERATED_BODY()

public:
	ABNBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The teams-later seam: FFA today, so everyone answers team 255 and every other pawn is
	 *  Hostile. Teams land by making these read a real team id — perception stays untouched. */
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	/** The HAND: presses the same buttons a human does, on the PlayerState ASC. */
	void PressInputTag(FGameplayTag InputTag);
	void ReleaseInputTag(FGameplayTag InputTag);

	/** Sprint is a HELD input: the press applies the speed GE and the tag, the release takes both
	 *  away. A task that pressed and forgot would leave a bot sprinting for the rest of its life,
	 *  so the hold state lives HERE — one owner, released on unpossess no matter which task was
	 *  mid-thought. Idempotent: repeat calls with the same value do nothing. */
	void SetSprinting(bool bWantSprint);

	/** Crouch is a TOGGLE, not a hold: UBNGA_Crouch flips (!bIsCrouched && !IsFalling). So this
	 *  reads the character's REAL crouch state and presses only when it disagrees — a task that
	 *  "pressed crouch" every tick would flap the bot up and down once a frame. Refuses while
	 *  falling, because mid-air the toggle only ever UNcrouches. */
	void SetCrouching(bool bWantCrouch);

	/** Null when the target is gone or dead — callers never see a corpse as a target. Null ALSO
	 *  while the ambition is Survive: the tree's Engage exits by its own HasTarget condition,
	 *  which is how the brain steers the tree without editing it. */
	AActor* GetCurrentTarget() const;
	void SetCurrentTarget(AActor* Target);
	void ClearCurrentTarget();

	/** The underlying TargetEnemy regardless of ambition — the thing Survive flees FROM. */
	AActor* GetThreat() const;

	EBNBotAmbition GetAmbition() const;

	/** The move tasks call this when the target cannot be pathed to. AAIController::MoveToActor
	 *  defaults to bAllowPartialPath=true, so an unreachable goal does NOT fail — it returns a
	 *  partial path to the nearest reachable point, and when the bot is already standing on that
	 *  point the answer is AlreadyAtGoal. Path following then reports Idle forever while the bot
	 *  stares at an enemy on a ledge it cannot climb. Measured: one bot held at 1898uu, Idle,
	 *  speed 0, for a whole match while its sibling closed normally.
	 *  Blacklisting the actor for a few seconds is what turns that deadlock into a decision: the
	 *  slot empties, the brain rescores, and the bot goes and does something else. */
	void NotifyTargetUnreachable(AActor* Target);

	/** Where the threat was last SEEN, and whether that memory is still worth acting on. Halo's
	 *  lesson 3 (legibility): a bot that forgets you the instant you break line of sight reads as
	 *  broken, while one that walks to where you WERE reads as hunting. */
	FVector GetLastKnownThreatLocation() const;
	bool HasFreshLastKnownLocation() const;

	/**
	 * R9.5 — THE JUMP, as a verb the bot can spend. Presses the same `Input.Jump` a human's
	 * spacebar presses, so it runs `UBNGA_Jump` with the same cost, the same `State.Movement.
	 * Jumping` tag and the same landing handling. Returns true only if the press actually went out.
	 *
	 * Refused while already in the air (no double jump — BN has none for humans either) and until
	 * the cooldown has passed, because the failure mode of a jumping bot is not a missed jump, it
	 * is a bot that pogos on the spot and reads as broken. The COOLDOWN is the whole difference
	 * between "uses jumps" and "is a rabbit".
	 *
	 * Callers decide WHEN — wedged on geometry, roaming into a lip, juking mid-burst. This decides
	 * only whether a jump is allowed at all.
	 */
	bool TryJump();

	/**
	 * R10.4 — A GRENADE IS ABOUT TO GO OFF HERE. Pushed by ABNProjectile a beat before its fuse,
	 * to the bots inside the blast radius and nobody else.
	 *
	 * The bot does NOT learn who threw it and does not acquire a target from it: this is a place
	 * to not be standing, and nothing more. Everything about how to leave is the tree's.
	 */
	void NotifyIncomingBlast(const FVector& Center, double DetonateAtSeconds, float BlastRadius);

	/** True while a warned blast has not yet gone off. Out params are the circle to leave. */
	bool HasIncomingBlast(FVector& OutCenter, float& OutRadius) const;

	/** R10 — health as the brain sees it, 0..1, or 1 when there is nothing to ask. Public because
	 *  the cover condition needs the same number the brain scores on, and two ways of computing
	 *  "how hurt am I" would disagree on the frame that matters. */
	float GetHealthNorm() const;

	/** R10 — cover is on a cooldown OWNED BY THE CONTROLLER, not by the state: a state cooldown
	 *  resets every time the tree re-selects, and a bot that re-enters cover the instant it
	 *  leaves is a bot that never shoots back. Returns false while the last break is still
	 *  spending its window. */
	bool CanTakeCoverNow() const;
	void NotifyTookCover();

	/** R10 — the TIER's numbers, resolved once at possession and read by the tasks that need
	 *  them. A task keeps its own authored parameter as an OVERRIDE; where it has none, it asks
	 *  here. That split is what lets a tier change how a bot fights without re-authoring the
	 *  StateTree, and lets the tree still pin a value when a state genuinely needs one. */
	const FBNBotTuningRow& GetTuning() const;

	/** The four shipped tiers in C++, mirrored by DT_BNBotTuning. PUBLIC because
	 *  UBNBotAuthoring builds that table from this function rather than re-typing the numbers —
	 *  the TABLE overrides these, and a hand-typed copy in both places is a second source of
	 *  truth that drifts the first time someone tunes one of them. */
	static FBNBotTuningRow DefaultTuning(FName TierName);

	/** R9 — the bot was SHOT, and by whom. Stamps the attacker's position as the last-known
	 *  threat so Search sends the bot to look, exactly as losing sight of a seen enemy does.
	 *  Deliberately NOT a target grant: being hit from behind should make a bot turn and hunt,
	 *  not acquire a perfect lock on someone it has never seen. Perception still has to do that,
	 *  and the reaction window still applies when it does. */
	void RememberThreatAt(const FVector& Where);

	/** R11: a bot may not shoot the instant it perceives. False until the reaction window since
	 *  target acquisition has passed. */
	bool HasReactedToTarget() const;

	/** R11 for explosives: has enough time passed since this blast was announced for a human to
	 *  have plausibly noticed it? Uses the same drawn reaction as a target sighting. */
	bool HasReactedToBlast() const;

	/** The weapon the bot is holding, through the pawn's equipment component — the same object a
	 *  human's HUD reads. Null when unarmed, mid-swap, or the pawn is gone. */
	ABNWeapon* GetCurrentWeapon() const;

	/** True when the bot can actually SEE its current target. Firing is gated on this: perception
	 *  REMEMBERS a target for LoseSightRadius seconds after it breaks cover, so "I have a target"
	 *  and "I can shoot it" are different questions and a bot that confuses them shoots walls. */
	bool HasLineOfSightToTarget() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void OnPerceptionForgotten(AActor* Actor);

	/* --- NEVER IDLE (founder, 25 Aug) --------------------------------------------------------
	 * "make sure the ai always have a target, the closest one, if he is still looking for a new
	 * one for a period of time."
	 *
	 * Perception alone leaves a bot with NOTHING to want whenever the arena happens to put every
	 * enemy behind a wall — it roams, and roaming reads as disinterest. After the grace period
	 * this hands it the nearest valid enemy so the tree always has a Fight to select.
	 *
	 * THE GRACE PERIOD IS THE WHOLE DESIGN. Acquiring instantly would be omniscience and would
	 * delete Search, which is the behaviour that makes a bot look like it is hunting. Waiting
	 * first means the bot genuinely tried to find you, and only then stops pretending it cannot.
	 */
	void ArmNoTargetFallback();
	void OnNoTargetGraceElapsed();
	AActor* FindNearestValidEnemy() const;

	UBNAbilitySystemComponent* GetBotASC() const;

	/** The FFA target rule, one function: a live ABNCharacter that is not my pawn. */
	bool IsValidTarget(AActor* Actor) const;

	/** Distills the facts, resolves rows (table, else brain defaults) and asks the brain. Called
	 *  from EVENTS only — target gained/lost, health change, own damage — never a tick. */
	void RescoreBrain();

	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnRecentDamageTagChanged(const FGameplayTag Tag, int32 NewCount);

	UPROPERTY(VisibleAnywhere, Category = "Bot")
	TObjectPtr<UStateTreeAIComponent> StateTreeAI;

	UPROPERTY(VisibleAnywhere, Category = "Bot")
	TObjectPtr<UAIPerceptionComponent> BotPerception;

	UPROPERTY(VisibleAnywhere, Category = "Bot")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** R10 — EARS. Sight alone made bots deaf to a firefight ten metres away: they could be shot
	 *  in the back (R9.2 gave them a reaction to that) but a gunfight they were not part of was
	 *  silent. A noise becomes a LAST-KNOWN POSITION, never a target — hearing you shoot tells a
	 *  bot where to look, and perception still has to do the seeing. */
	UPROPERTY(VisibleAnywhere, Category = "Bot")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	/** THE TREE, by soft path from ini — C++-first: without this, assigning the StateTree would
	 *  require a Blueprint child of this controller just to hold one reference. Resolved in
	 *  OnPossess before StartLogic; unset or unresolved is a LOUD warning, because the visible
	 *  symptom (bots stand still) says nothing about the cause. */
	UPROPERTY(Config)
	TSoftObjectPtr<UStateTree> BotStateTree;

	/** R10 — WHICH TIER this bot fights at, by row name: Recruit, Marine, ODST, Spartan. Set in
	 *  DefaultGame.ini; a name with no row falls back to the C++ defaults for that tier, and an
	 *  unknown name falls back to Marine after one warning. The GameMode may later set this per
	 *  bot for a mixed lobby — the resolve happens in OnPossess, so anything that writes it
	 *  before possession is honoured. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Tier")
	FName BotTier = FName(TEXT("Marine"));

	/** The resolved row: the table's if it has one, else DefaultTuning's. Never null after
	 *  OnPossess — GetTuning returns it by reference and every caller assumes that. */
	TSharedPtr<FBNBotTuningRow> Tuning;

	/** Reads the tier row and applies everything that is applied ONCE: the sight sense. The rest
	 *  is read per use through GetTuning, so a re-resolve cannot leave half a tier behind. */
	void ResolveTuning();

	/** The reaction window for THIS acquisition, drawn once when the target was acquired.
	 *  Quantized and seeded, never FMath::RandRange off the global stream — §5's determinism law
	 *  says same seed + same event trace must give the same action trace. */
	float DrawReactionSeconds();

	/** Weak: the target's death or leave must never dangle here. */
	TWeakObjectPtr<AActor> TargetEnemy;

	// SIGHT, REACTION and the JUMP COOLDOWN moved to the TIER (R10). They were per-controller
	// Config keys; a tier that could not change them would not be a difficulty setting, and
	// keeping both would be two sources of truth for one number. DefaultGame.ini's tier row is
	// where they live now, and FBNBotTuningRow carries the founder's arena-tuned sight values as
	// Marine's defaults so the shipped behaviour did not move.

	/** Quantization bucket. Snapping the draw keeps the trace reproducible across float drift. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Reaction")
	float ReactionQuantumSeconds = 0.05f;

	/** World seconds when the next jump is allowed. Negative so the first one always passes. */
	double NextJumpAllowedSeconds = -1.0;

	/** How long an unreachable target stays ignored before the bot is willing to try again.
	 *  8 -> 3 was tried on 25 Aug and was too eager: the bot re-acquired an unreachable target
	 *  every 3s and never got a window to do anything else. 6 keeps it persistent without
	 *  starving Roam, which is what carries a bot down off the top platform. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot")
	float UnreachableForgetSeconds = 6.f;

	/** How long a bot may have NO target before it is handed the nearest enemy. Zero disables the
	 *  fallback entirely and returns the bot to pure perception. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Aggression")
	float NoTargetGraceSeconds = 5.f;

	FTimerHandle NoTargetTimerHandle;

	/** How long a last-known position is worth walking to. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot")
	float LastKnownFreshSeconds = 16.f;     // was 8 - hunt a memory twice as long

	double TargetAcquiredSeconds = -1.0;
	float CurrentReactionSeconds = 0.f;

	/** Counter, not a clock: it is the seeded stream's sequence number for this controller. */
	int32 ReactionDrawCount = 0;

	// R10.4 — the warned blast. Cleared by time alone: the projectile that warned us may be
	// destroyed before we act, so nothing here may hold a pointer to it.
	FVector IncomingBlastCenter = FVector::ZeroVector;
	/** True while the sprint input is held. See SetSprinting. */
	bool bSprintHeld = false;

	/* --- line-of-sight cache -----------------------------------------------------------------
	 * AAIController::LineOfSightTo is a real trace (up to 7 of them against the target's sight
	 * points). Shoot runs three ticking tasks and TWO of them asked every frame, so eight
	 * fighting bots spent 16+ traces per frame re-answering a question that changes on a ~100 ms
	 * timescale. The quality bar is 30 Hz with 8 fighters and nobody had measured it.
	 *
	 * Cached per TARGET, not just per time: switching target must invalidate immediately or a bot
	 * inherits the last one's visibility for up to the cache window and fires at a wall. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot")
	float LineOfSightCacheSeconds = 0.1f;

	mutable TWeakObjectPtr<const AActor> LosCachedTarget;
	mutable double LosCachedAtSeconds = -1.0;
	mutable bool bLosCachedResult = false;

	double IncomingBlastAtSeconds = -1.0;

	/** When this bot was TOLD about the blast. R11's clock for explosives starts here. */
	double IncomingBlastNoticedSeconds = -1.0;
	float IncomingBlastRadius = 0.f;

	/** How long after breaking line of sight before this bot is willing to do it again. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Cover")
	float CoverCooldownSeconds = 8.f;

	double NextCoverAllowedSeconds = -1.0;

	TWeakObjectPtr<AActor> UnreachableActor;
	double UnreachableUntilSeconds = -1.0;

	FVector LastKnownThreatLocation = FVector::ZeroVector;
	double LastKnownThreatSeconds = -1.0;

	UPROPERTY()
	TObjectPtr<UBNBotBrain> Brain;

	/** Cached so OnUnPossess unregisters from the SAME ASC it registered on — ABNCharacter's
	 *  EndPlay discipline: the PlayerState outlives the pawn, the handles must not. */
	TWeakObjectPtr<UBNAbilitySystemComponent> BrainEventASC;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle RecentDamageHandle;

	/** BNGA_ADS's baseline pattern: only a count INCREASE is a landed hit; a decrease is a
	 *  damage window expiring, and that must never fire a rescore. */
	int32 LastRecentDamageCount = 0;
};
