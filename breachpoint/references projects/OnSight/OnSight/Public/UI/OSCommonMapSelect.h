#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/OSSessionsData.h"
#include "OSCommonMapSelect.generated.h"

class UOSBaseCommonButton;
class UCommonButtonBase;
class UVerticalBox;
class AGameModeBase;
struct FAssetData;

/*
  CommonUI map-select screen. Scans a content directory for levels at runtime,
  spawns a button per map, and hosts a session on the selected map. Pushed on
  a CommonActivatableWidgetStack by the main menu; DeactivateWidget() pops back.
 */
UCLASS()
class ONSIGHT_API UOSCommonMapSelect : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// --- Bound Widgets ---

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> SelectMapBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> LaunchButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> Return_Main;

	// --- Game Mode Buttons ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> BT_DeathMatch;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> BT_TeamDeathMatch;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> BT_Domination;

	// --- Player Count Buttons ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> BT_PlayerCount2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> BT_PlayerCount4;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> BT_PlayerCount6;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> BT_PlayerCount8;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> BT_DevPlayTest;

	// --- Configuration ---

	UPROPERTY(EditDefaultsOnly, Category = "Map Select")
	TSubclassOf<UOSBaseCommonButton> MapButtonClass;

	UPROPERTY(EditDefaultsOnly, Category = "Map Select")
	FString MapScanPath = TEXT("/Game/Maps/Deployable/PVP_Maps");

	// Whitelist of asset names to show (case-insensitive). Empty list = show all.
	UPROPERTY(EditDefaultsOnly, Category = "Map Select")
	TArray<FString> AllowedMapNames;

	// Override button display names. Key = asset name (e.g. "Bridges2"), Value = display text (e.g. "Bridges").
	UPROPERTY(EditDefaultsOnly, Category = "Map Select")
	TMap<FString, FString> MapDisplayNames;

	// --- Match Settings ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map Select|Match Settings")
	EOSGameModeType SelectedGameMode = EOSGameModeType::DeathMatch;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map Select|Match Settings")
	EOSPlayerCount SelectedPlayerCount = EOSPlayerCount::Players_8;

	UPROPERTY(EditDefaultsOnly, Category = "Map Select|Match Settings")
	TMap<EOSGameModeType, TSubclassOf<AGameModeBase>> GameModeClassMap;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	// --- Map Discovery ---

	void ScanAndPopulateMaps();
	TArray<FAssetData> GetMapAssetsInPath() const;
	void ApplyMapNameFilter(TArray<FAssetData>& MapAssets) const;
	void SpawnMapButton(const FAssetData& MapAsset, bool bBindClick);
	FString ResolveDisplayName(const FString& AssetName) const;

	// --- Button Wiring ---

	void BindLaunchAndReturnButtons();
	void UnbindLaunchAndReturnButtons();
	void BindSettingsButtons();
	void UnbindSettingsButtons();
	void UnbindMapButtons();

	// --- Selection ---

	void HandleMapButtonClicked(UOSBaseCommonButton* ClickedButton);
	void OnLaunchButtonClicked();
	void OnReturnMainClicked();

	void SelectMap(UOSBaseCommonButton* ClickedButton, const FString& MapPath);

	// --- Match Settings Helpers ---

	void SetGameMode(EOSGameModeType NewMode);
	void SetPlayerCount(EOSPlayerCount NewCount);

	void OnDeathMatchClicked();
	void OnTeamDeathMatchClicked();
	void OnDominationClicked();
	void OnPlayerCount2Clicked();
	void OnPlayerCount4Clicked();
	void OnPlayerCount6Clicked();
	void OnPlayerCount8Clicked();
	void OnDevPlayTestClicked();

	// --- Host Helpers ---

	FString BuildTravelUrlWithMode() const;

	// --- State ---

	UPROPERTY()
	TMap<TObjectPtr<UOSBaseCommonButton>, FString> ButtonToMapPath;

	UPROPERTY()
	TObjectPtr<UOSBaseCommonButton> ActiveMapButton;

	// Package path of the map the player clicked. Empty until a map is selected; checked in OnLaunchButtonClicked.
	FString GameMapPath;
};
