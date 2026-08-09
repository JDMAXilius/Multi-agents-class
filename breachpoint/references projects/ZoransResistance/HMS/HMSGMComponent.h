// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZoransResistance/HMS/FHMSActorState.h"
#include "ZoransResistance/HMS/EHMSTransferType.h"
#include "HMSGMComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActorSaveComplete, const TArray<FHMSActorState>&, ActorStates);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveComplete,bool, CalledOnUpdate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerEnter);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ZORANSRESISTANCE_API UHMSGMComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UHMSGMComponent();

	UPROPERTY(BlueprintAssignable)
	FActorSaveComplete OnActorsSaved;
	
	UPROPERTY(BlueprintAssignable)
	FOnSaveComplete OnSaveComplete;

	UPROPERTY(BlueprintAssignable)
	FOnPlayerEnter OnPlayerEnter;

	UPROPERTY(BlueprintReadOnly)
	TArray<APlayerController*> AllPCHMS;

	UPROPERTY(BlueprintReadOnly)
	FHMSGameSave CurrentStateHMS;
	
	UPROPERTY(BlueprintReadWrite,EditAnywhere,Category = "HMS")
	float UpdatePeriod;

	UPROPERTY(BlueprintReadOnly)
	APlayerController* NewHostHMS;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "HMS")
	bool IgnoreHostPawn;

	UPROPERTY(BlueprintReadOnly)
	FString LocalIPHMS;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "HMS")
	TEnumAsByte<EHMSTransferType> SendTypeHMS;

	UPROPERTY(BlueprintReadOnly)
	bool Migrated;

	UPROPERTY(BlueprintReadOnly)
	int32 SendChunkSizeHMS;

	UPROPERTY(BlueprintReadOnly)
	int32 SendChunkIndexHMS;

	UPROPERTY(BlueprintReadOnly)
	bool SendingChunkHMS;
	
	UPROPERTY(BlueprintReadOnly)
	bool CalledOnUpdate;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void FinishSave();

	virtual void OnSaveComplete_Testing(bool bCalledOnUpdate);

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable)
	void LoadState();

	UFUNCTION(BlueprintCallable)
	void UpdateState();

	UFUNCTION(BlueprintCallable)
	void ChooseNewHost();

	UFUNCTION(BlueprintCallable)
	TArray<FHMSPlayerSave> SavePlayers();
		
	UFUNCTION(Server,Reliable,WithValidation)
	void StartStateUpdateLoop();
	bool StartStateUpdateLoop_Validate();

	UFUNCTION(BlueprintCallable)
	void PlayerLogin(APlayerController* NewPlayer);

	UFUNCTION(BlueprintCallable)
	void PlayerLogout(AController* ExitingController);

	UFUNCTION(BlueprintCallable)
	void SendGameSaveInChunks();

	UFUNCTION(BlueprintCallable)
	void SaveState(bool bCalledOnUpdate);

	UFUNCTION(BlueprintCallable)
	void EventActorsSaved(const TArray<FHMSActorState>& ActorStates);

	/**
	 * Test function.
	*/
	UFUNCTION(BlueprintCallable)
	void SimulateHostMigration();

};
