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

UCLASS()
class BREACHPOINT_API ABRBotController : public AAIController
{
	GENERATED_BODY()

public:
	ABRBotController(const FObjectInitializer& ObjectInitializer);

	void ConfigureBot(int32 InSeed, const FBRBotTierScalars& InScalars, const TArray<FBRBotAmbitionDef>& InAmbitions);

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UBRBotBrain* GetBrain() const { return Brain; }

	EBRBotAmbition GetActiveAmbition() const;

	FBRBotPlanStep GetCurrentStep() const;

	void PostBotEvent(EBRBotEventType EventType, int32 SubjectId = INDEX_NONE);

	void ReportStepCompleted();
	void ReportStepFailed();

	void PressInputTag(FGameplayTag InputTag);

	void ReleaseInputTag(FGameplayTag InputTag);

	bool IsInputTagHeld(FGameplayTag InputTag) const;

	bool CanActivateByInputTag(FGameplayTag InputTag) const;

	void AimAtTargetWithError(const AActor* Target);

	void BeginEngage(AActor* Target);

	void EndEngage();

	UFUNCTION()
	void RunEngageSelector();

	void RequestMoveToAnchor(UEnvQuery* LocationQuery, float AcceptanceRadius);

	void CancelActiveMove();

	void StartHoldTimer(float HoldSeconds);
	void CancelHoldTimer();

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	FBRBotFacts BuildFacts() const;

	AActor* GetCurrentTarget() const;

	UBRAbilitySystemComponent* GetBotASC() const;

protected:
	virtual void FillMatchFacts(FBRBotFacts& Facts) const;

	virtual void FillSelfFacts(FBRBotFacts& Facts) const;

	virtual void FillTargetFacts(FBRBotFacts& Facts) const;

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void HandleShieldsBrokenTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	void DispatchBotEvent(FBRBotEvent Event);

	static bool RequiresReactionDelay(EBRBotEventType EventType);

	void ApplyDecisionToSpine(const FBRBotDecision& Decision);

	UPROPERTY(VisibleAnywhere, Category = "Breachpoint|Bot")
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

	UPROPERTY(VisibleAnywhere, Category = "Breachpoint|Bot")
	TObjectPtr<UAIPerceptionComponent> BotPerception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UBRBotBrain> Brain;

	TWeakObjectPtr<AActor> CurrentTarget;

	float LastTargetSeenSeconds = 0.f;

	TArray<FTimerHandle> PendingReactionTimers;

	FTimerHandle EngageTimer;

	FTimerHandle HoldTimer;

	float PendingMoveAcceptanceRadius = 0.f;

	void HandleAnchorQueryFinished(TSharedPtr<struct FEnvQueryResult> Result);

	void HandleHoldElapsed();

private:
	void ResolveStateTreeAsset();

	void OnStateTreeAssetLoaded();

	void BindAbilitySystemEvents();
	void UnbindAbilitySystemEvents();

	float GetMatchTimeSeconds() const;

	FDelegateHandle ShieldsBrokenHandle;

	bool bConfigured = false;

	mutable bool bWarnedMatchFactsSeam = false;
};
