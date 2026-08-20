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

	/** Weak: the target's death or leave must never dangle here. */
	TWeakObjectPtr<AActor> TargetEnemy;

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
