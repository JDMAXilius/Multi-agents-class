// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ZoransResistance/Game/Data/ZoransGameModeData.h"
#include "ZoransResistance/AbilitySystem/Data/AbilitySystemData.h"
#include "ZoransGameMode.generated.h"

UCLASS()
class ZORANSRESISTANCE_API AZoransGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	
	AZoransGameMode();
	
	virtual void BeginPlay() override;

	virtual void AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC) override;

	virtual bool FindInactivePlayer(APlayerController* PC) override;

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability System")
	FGrantedAbilitySystemData AbilitySystemData;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability System")
	FGameModeDetails GameModeDetails;
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Respawn")
	void RequestRespawn(AController* InController, bool bFirstSpawn = false, APawn* OldPawn = nullptr, float RespawnTimeOverride = 0.0f);

	UFUNCTION()
	void Internal_HandleRespawn(AController* InController, bool bAIController = false, bool bFirstSpawn = false, APawn* OldPawn = nullptr, const bool bPreTeamInit = false);
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Spawning")
	float RespawnDelay = 6.0f;

	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	
	UPROPERTY()
	TArray<FString> AvailableBotNames = {};

protected:
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Bots")
	UDataTable* BotDataTable;

	FTimerHandle RespawnTimerHandle;

	virtual void GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList) override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Character Selection")
	UDataTable* CharacterDataTable = nullptr;
};