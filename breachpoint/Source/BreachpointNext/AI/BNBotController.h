#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "BNBotController.generated.h"

class ABNWeapon;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBNAbilitySystemComponent;
class UBNBotBrain;
class UStateTree;
class UStateTreeAIComponent;
enum class EBNBotAmbition : uint8;
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

	/** R9 — the bot was SHOT, and by whom. Stamps the attacker's position as the last-known
	 *  threat so Search sends the bot to look, exactly as losing sight of a seen enemy does.
	 *  Deliberately NOT a target grant: being hit from behind should make a bot turn and hunt,
	 *  not acquire a perfect lock on someone it has never seen. Perception still has to do that,
	 *  and the reaction window still applies when it does. */
	void RememberThreatAt(const FVector& Where);

	/** R11: a bot may not shoot the instant it perceives. False until the reaction window since
	 *  target acquisition has passed. */
	bool HasReactedToTarget() const;

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

	/** THE TREE, by soft path from ini — C++-first: without this, assigning the StateTree would
	 *  require a Blueprint child of this controller just to hold one reference. Resolved in
	 *  OnPossess before StartLogic; unset or unresolved is a LOUD warning, because the visible
	 *  symptom (bots stand still) says nothing about the cause. */
	UPROPERTY(Config)
	TSoftObjectPtr<UStateTree> BotStateTree;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Sight")
	float SightRadius = 2500.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Sight")
	float LoseSightRadius = 3000.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Sight")
	float PeripheralVisionAngleDegrees = 70.f;

	/** The reaction window for THIS acquisition, drawn once when the target was acquired.
	 *  Quantized and seeded, never FMath::RandRange off the global stream — §5's determinism law
	 *  says same seed + same event trace must give the same action trace. */
	float DrawReactionSeconds();

	/** Weak: the target's death or leave must never dangle here. */
	TWeakObjectPtr<AActor> TargetEnemy;

	/** R11's floor and ceiling. A bot that fires on the same frame it sees you is not difficult,
	 *  it is unfair — the player never gets the "I was seen" beat that makes a firefight readable. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Reaction")
	float ReactionSecondsMin = 0.22f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Reaction")
	float ReactionSecondsMax = 0.45f;

	/** Quantization bucket. Snapping the draw keeps the trace reproducible across float drift. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Reaction")
	float ReactionQuantumSeconds = 0.05f;

	/** How long an unreachable target stays ignored before the bot is willing to try again. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot")
	float UnreachableForgetSeconds = 8.f;

	/** How long a last-known position is worth walking to. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot")
	float LastKnownFreshSeconds = 8.f;

	double TargetAcquiredSeconds = -1.0;
	float CurrentReactionSeconds = 0.f;

	/** Counter, not a clock: it is the seeded stream's sequence number for this controller. */
	int32 ReactionDrawCount = 0;

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
