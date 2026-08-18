#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "BNBotController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBNAbilitySystemComponent;
class UStateTreeAIComponent;
struct FAIStimulus;

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

	/** Null when the target is gone or dead — callers never see a corpse as a target. */
	AActor* GetCurrentTarget() const;
	void SetCurrentTarget(AActor* Target);
	void ClearCurrentTarget();

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

	UPROPERTY(VisibleAnywhere, Category = "Bot")
	TObjectPtr<UStateTreeAIComponent> StateTreeAI;

	UPROPERTY(VisibleAnywhere, Category = "Bot")
	TObjectPtr<UAIPerceptionComponent> BotPerception;

	UPROPERTY(VisibleAnywhere, Category = "Bot")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Sight")
	float SightRadius = 2500.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Sight")
	float LoseSightRadius = 3000.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Bot|Sight")
	float PeripheralVisionAngleDegrees = 70.f;

	/** Weak: the target's death or leave must never dangle here. */
	TWeakObjectPtr<AActor> TargetEnemy;
};
