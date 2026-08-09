#pragma once

#include "CoreMinimal.h"
#include "UI/OSMainMenuWidget.h"
#include "Data/OSSessionsData.h"
#include "OSMapSelect.generated.h"

class UOSBaseButton;
class UVerticalBox;
class AGameModeBase;

/*
  Map selection screen. Scans a content directory for levels at runtime,
  spawns a button per map, and caches the player's choice for server travel.
  Inherits session plumbing from OSMainMenuWidget.
*/
UCLASS()
class ONSIGHT_API UOSMapSelect : public UOSMainMenuWidget
{
	GENERATED_BODY()

public:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- Bound Widgets ---

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> SelectMapBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LaunchButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Return_Main;

	// --- Game Mode Buttons ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseButton> BT_DeathMatch;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseButton> BT_TeamDeathMatch;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseButton> BT_Domination;

	// --- Player Count Buttons ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseButton> BT_PlayerCount2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseButton> BT_PlayerCount4;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseButton> BT_PlayerCount6;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseButton> BT_PlayerCount8;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseButton> BT_DevPlayTest;

	// --- Configuration ---

	UPROPERTY(EditDefaultsOnly, Category = "Map Select")
	TSubclassOf<UOSBaseButton> MapButtonClass;

	UPROPERTY(EditDefaultsOnly, Category = "Map Select")
	FString MapScanPath = TEXT("/Game/Maps/Deployable/PVP_Maps");

	// Only show maps whose asset name contains one of these strings (case-insensitive). Empty list = show all.
	UPROPERTY(EditDefaultsOnly, Category = "Map Select")
	TArray<FString> AllowedMapNames;

	// Override button display names. Key = asset name (e.g. "Bridges2"), Value = display text (e.g. "Bridges").
	UPROPERTY(EditDefaultsOnly, Category = "Map Select")
	TMap<FString, FString> MapDisplayNames;

	// --- Match Settings (host picks these before launching) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map Select|Match Settings")
	EOSGameModeType SelectedGameMode = EOSGameModeType::DeathMatch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map Select|Match Settings")
	EOSPlayerCount SelectedPlayerCount = EOSPlayerCount::Players_8;

	// Maps each game mode enum to its Blueprint GameMode class. Set these in the WBP defaults.
	UPROPERTY(EditDefaultsOnly, Category = "Map Select|Match Settings")
	TMap<EOSGameModeType, TSubclassOf<AGameModeBase>> GameModeClassMap;

private:
	// --- Map Discovery ---

	void ScanAndPopulateMaps();
	TArray<FAssetData> GetMapAssetsInPath() const;
	void SpawnMapButton(const FAssetData& MapAsset, bool bBindClick);

	// --- Selection ---

	UFUNCTION()
	void OnMapButtonClicked();

	UFUNCTION()
	void OnLaunchButtonClicked();

	void SelectMap(UOSBaseButton* ClickedButton, const FString& MapPath);
	UOSBaseButton* FindClickedButton() const;

	void SetGameMode(EOSGameModeType NewMode);
	void SetPlayerCount(EOSPlayerCount NewCount);

	UFUNCTION() void OnDeathMatchClicked();
	UFUNCTION() void OnTeamDeathMatchClicked();
	UFUNCTION() void OnDominationClicked();
	UFUNCTION() void OnPlayerCount2Clicked();
	UFUNCTION() void OnPlayerCount4Clicked();
	UFUNCTION() void OnPlayerCount6Clicked();
	UFUNCTION() void OnPlayerCount8Clicked();
	UFUNCTION() void OnDevPlayTestClicked();
	UFUNCTION() void OnReturnMainClicked();

	void BindSettingsButtons();
	void UnbindSettingsButtons();

	// --- State ---

	UPROPERTY()
	TMap<TObjectPtr<UOSBaseButton>, FString> ButtonToMapPath;

	UPROPERTY()
	TObjectPtr<UOSBaseButton> ActiveMapButton;
};
