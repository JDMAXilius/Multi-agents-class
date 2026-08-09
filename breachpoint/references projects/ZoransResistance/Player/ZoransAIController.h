// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"

#include "ZoransResistance/Game/Data/StaticGameData.h"
#include "ZoransResistance/Characters/ZoransCharacterBase.h"
#include "Perception/AIPerceptionComponent.h"
#include "ZoransResistance/Game/ZoransGameState.h"
#include "ZoransAIController.generated.h"

class UBehaviorTree;
class UBlackboardComponent;
class UAISenseConfig_Sight;
class UZoransGameInstance;
class ZoransWorldSubsystem;

UCLASS()
class ZORANSRESISTANCE_API AZoransAIController : public AAIController
{
	GENERATED_BODY()

public:

	AZoransAIController();

	UFUNCTION()
	void SetWeaponDefaults();
	virtual void OnPossess(APawn* InPawn) override;

	void SetOwningAI(AZoransCharacterBase* InAI) { OwningAI = InAI; }

	AZoransCharacterBase* GetOwningAI() { return OwningAI.Get(); }
	void GetGameModeOverride(UZoransGameState* ZWState);

	TWeakObjectPtr<AZoransCharacterBase> OwningAI;

	//UFUNCTION(BlueprintCallable)
	//FAIStatsInfo* GetAIStatInfo();

	// Blackboard Key Ids
	uint8 OutOfAmmo;
	uint8 EngagementRange;
	uint8 MaxTargetDistance;
	uint8 GameModeOverride;

	UPROPERTY()
	EAIDificulTyLevel AIDificulTyLevel;
	UPROPERTY()
	TObjectPtr<UBehaviorTree> BehaviorTree;
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBlackboardComponent> BBC;
	UPROPERTY()
	TObjectPtr<UZoransWeaponComponent> ZW;
	UPROPERTY()
	FAIStatsInfo AIStatsInfo;
	

protected:

	////AIStatsInfo
	int32 AIDificulty = 1;
	
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree_Default;
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree_Tutorial;
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree_TeamDeathMatch;
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree_ControlPoint;
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree_GunGame;
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree_DataDash;
	
	//UPROPERTY(EditAnywhere, Category = "AI")
	//TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;
	

	//UFUNCTION()
	//void OnPerception(AActor* Actor, FAIStimulus Stimulus);

	/** A Sight Sense config for our AI */
	//UPROPERTY(VisibleAnywhere, Category = "AI")
	//TObjectPtr<UAISenseConfig_Sight> Sight;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};