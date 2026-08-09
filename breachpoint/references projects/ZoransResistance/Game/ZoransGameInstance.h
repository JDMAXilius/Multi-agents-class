// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Data/StaticGameData.h"
#include "ZoransResistance/HMS/FHMSGameSave.h"
#include "Blueprint/UserWidget.h"
#include "ZoransGameInstance.generated.h"

class UZoransLocalPlayerSubsystemBase;
class UZoransInstanceAudioSubsystemConfig;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayfabLoginDelegate, bool, bSuccessful, FString, PlayfabId);

UCLASS()
class ZORANSRESISTANCE_API UZoransGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	UZoransGameInstance();
	

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Details")
	TMap<int32, FAIStatsInfo> AIStatsInfo;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Details")
	int32 AIDificulty = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Config")
	TObjectPtr<UZoransInstanceAudioSubsystemConfig> ZoransInstanceAudioSubsystemConfig;

	UPROPERTY(BlueprintReadOnly)
	FString RegionEnvVar;

	UPROPERTY(BlueprintReadOnly)
	FString IsMatchmakingServer;

	UPROPERTY(BlueprintReadOnly)
	FString IsDevOnlyServer;

	UPROPERTY(BlueprintReadOnly)
	FString MapRouletteEnabled;

	UPROPERTY(BlueprintReadOnly)
	FString GameModeRouletteEnabled;

	UPROPERTY(BlueprintReadOnly)
	FString BotsEnabled;
	
	UPROPERTY(BlueprintReadOnly)
	FString VersionCode;
		
	UPROPERTY(BlueprintReadOnly)
	FString PortBeacon;
			
	UPROPERTY(BlueprintReadOnly)
	FString IsSandbox;
				
	UPROPERTY(BlueprintReadOnly)
	FString ServerIP;

	FString MatchTimeOverrideString;

	UPROPERTY(BlueprintReadOnly)
	float MatchTimeOverrideFloat = -1.f;

	UPROPERTY(BlueprintReadOnly)
	FString Temp_ShouldDestroyPlayerState;

	UPROPERTY(BlueprintReadOnly)
	FString ServerName;
		
	UPROPERTY(BlueprintReadOnly)
	FHMSGameSave LastState;
		
	UPROPERTY(BlueprintReadOnly)
	FString NewHostIP;
		
	UPROPERTY(BlueprintReadOnly)
	UUserWidget* MigrationWidget;
		
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Widgets")
	TSubclassOf<UUserWidget> MigrationWidgetClass;
			
	UPROPERTY(BlueprintReadOnly)
	UUserWidget* NetworkErrorWidget;
		
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Widgets")
	TSubclassOf<UUserWidget> NetworkErrorWidgetClass;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TSubclassOf<UZoransLocalPlayerSubsystemBase> UserInterfaceSubsystemClass;
	
	virtual void Init() override;

	UFUNCTION(BlueprintCallable)
	void FinishCompilingShaders();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool GetIsAsyncCompilingShaders();

	UFUNCTION(BlueprintCallable)
	void ShowNetworkErrorWidget();

	UFUNCTION(BlueprintCallable)
	void ShowMigrationWidget();

	UFUNCTION(BlueprintCallable)
	void HideNetworkErrorWidget();

	UFUNCTION(BlueprintCallable)
	void HideMigrationWidget();

};