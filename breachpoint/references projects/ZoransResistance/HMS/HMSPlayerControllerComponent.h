// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HMSPlayerControllerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRecreateSession, UHMSPlayerControllerComponent*, Component);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ZORANSRESISTANCE_API UHMSPlayerControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UHMSPlayerControllerComponent();

	UPROPERTY(BlueprintAssignable,Category = "HMS")
	FRecreateSession RecreateSession;

	UPROPERTY(BlueprintReadOnly,Replicated)
	FString PlayerID;
	
	UPROPERTY(BlueprintReadWrite,EditAnywhere,Category = "HMS")
	FString Port;

	UPROPERTY(BlueprintReadOnly, Replicated)
	bool Spawned;

	UPROPERTY(BlueprintReadWrite,EditAnywhere,Category = "HMS")
	float ReconnectTime;

	UPROPERTY(BlueprintReadOnly, Replicated)
	bool Migrated;

	UPROPERTY(BlueprintReadOnly)
	TArray<uint8> ReceivedStateBuffer;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	void SetSpawned();
	void FinishPlayerEnter();

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server,Reliable,WithValidation,BlueprintCallable, Category = "HMS")
	void Server_SetSpawned(bool bSpawned);
	bool Server_SetSpawned_Validate(bool bSpawned);

	UFUNCTION(Server,Reliable,WithValidation,BlueprintCallable,Category = "HMS")
	void Server_SetID(const FString& MyID);
	bool Server_SetID_Validate(const FString& MyID);

	UFUNCTION(Server,Reliable,WithValidation,BlueprintCallable,Category = "HMS")
	void GMEnter();
	bool GMEnter_Validate();

	UFUNCTION(Client,Reliable,BlueprintCallable,Category = "HMS")
	void MigrateGameClient(FName LevelName);

	UFUNCTION(Server,Reliable,WithValidation,BlueprintCallable,Category = "HMS")
	void GetMyID();
	bool GetMyID_Validate();

	UFUNCTION(Client,Reliable,BlueprintCallable,Category = "HMS")
	void Connect_Migrate();

	UFUNCTION(Client,Reliable,BlueprintCallable,Category = "HMS")
	void ReceiveGameStateChunk(const TArray<uint8>& Chunk, bool bFinal, int32 TotalSize);

	UFUNCTION(BlueprintCallable, Category = "HMS")
	void OnPlayerEnter();

	UFUNCTION(Client,Reliable,BlueprintCallable,Category = "HMS")
	void ReceiveGameState(const TArray<uint8>& Bytes);

	UFUNCTION(BlueprintCallable, Category = "HMS")
	void GI_SetState(FHMSGameSave LastState);

	UFUNCTION(Client,Reliable,BlueprintCallable,Category = "HMS")
	void GI_SetNewHostIP(const FString& IP);

private:

	UFUNCTION(BlueprintPure)
	FString BuildConsoleCommand(FString HostIP);

	UPROPERTY()
	FTimerHandle OnPlayerEnterHandle;

};
