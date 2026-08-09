#include "UI/OSMapSelect.h"
#include "UI/Components/OSBaseButton.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Subsystems/OSSessionsSubsystem.h"
#include "OSLogCategories.h"

void UOSMapSelect::NativeDestruct()
{
	for (auto& [Button, Path] : ButtonToMapPath)
	{
		if (Button)
			Button->OnClicked.RemoveAll(this);
	}

	if (LaunchButton)
		LaunchButton->OnClicked.RemoveAll(this);

	if (Return_Main)
		Return_Main->OnClicked.RemoveAll(this);

	UnbindSettingsButtons();
	Super::NativeDestruct();
}

void UOSMapSelect::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		ScanAndPopulateMaps();
	}
}

void UOSMapSelect::NativeConstruct()
{
	Super::NativeConstruct();

	ScanAndPopulateMaps();

	if (LaunchButton)
	{
		LaunchButton->OnClicked.RemoveAll(this);
		LaunchButton->OnClicked.AddDynamic(this, &UOSMapSelect::OnLaunchButtonClicked);
	}

	if (Return_Main)
	{
		UE_LOG(HookMapSelect, Log, TEXT("[OSMapSelect] Return_Main button found, binding click"));
		Return_Main->OnClicked.RemoveAll(this);
		Return_Main->OnClicked.AddDynamic(this, &UOSMapSelect::OnReturnMainClicked);
	}
	else
	{
		UE_LOG(HookMapSelect, Warning, TEXT("[OSMapSelect] Return_Main button is NULL - not bound in widget"));
	}

	BindSettingsButtons();
}

// --- Map Discovery ---

void UOSMapSelect::ScanAndPopulateMaps()
{
	UE_LOG(HookMapSelect, Log, TEXT("[OSMapSelect] ScanAndPopulateMaps: SelectMapBox=%s, MapButtonClass=%s"),
		SelectMapBox ? TEXT("valid") : TEXT("null"),
		MapButtonClass ? *MapButtonClass->GetName() : TEXT("null"));

	if (!SelectMapBox || !MapButtonClass)
	{
		return;
	}

	SelectMapBox->ClearChildren();
	ButtonToMapPath.Empty();
	ActiveMapButton = nullptr;

	TArray<FAssetData> MapAssets = GetMapAssetsInPath();

	// Filter to allowed maps if the whitelist is populated
	if (AllowedMapNames.Num() > 0)
	{
		MapAssets.RemoveAll([this](const FAssetData& Asset)
		{
			const FString Name = Asset.AssetName.ToString();
			for (const FString& Allowed : AllowedMapNames)
			{
				if (Name.Equals(Allowed, ESearchCase::IgnoreCase))
					return false;
			}
			return true;
		});
	}

	UE_LOG(HookMapSelect, Log, TEXT("[OSMapSelect] Found %d map assets in path: %s"), MapAssets.Num(), *MapScanPath);

	const bool bBindClick = !IsDesignTime();
	for (const FAssetData& Asset : MapAssets)
	{
		UE_LOG(HookMapSelect, Log, TEXT("[OSMapSelect] Spawning button for: %s"), *Asset.AssetName.ToString());
		SpawnMapButton(Asset, bBindClick);
	}
}

TArray<FAssetData> UOSMapSelect::GetMapAssetsInPath() const
{
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*MapScanPath));
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	TArray<FAssetData> MapAssets;
	AssetRegistry.GetAssets(Filter, MapAssets);

	return MapAssets;
}

void UOSMapSelect::SpawnMapButton(const FAssetData& MapAsset, bool bBindClick)
{
	UOSBaseButton* NewButton = IsDesignTime()
		? CreateWidget<UOSBaseButton>(this, MapButtonClass)
		: CreateWidget<UOSBaseButton>(GetOwningPlayer(), MapButtonClass);
	if (!NewButton)
	{
		return;
	}

	const FString AssetName = MapAsset.AssetName.ToString();
	FString DisplayName;

	const FString* Override = MapDisplayNames.Find(AssetName);
	if (Override)
		DisplayName = *Override;
	else
	{
		DisplayName = AssetName;
		DisplayName.ReplaceInline(TEXT("_"), TEXT(" "));
	}

	FString PackagePath = MapAsset.PackageName.ToString();

	NewButton->ButtonName = FText::FromString(DisplayName);
	if (bBindClick)
	{
		NewButton->OnClicked.AddDynamic(this, &UOSMapSelect::OnMapButtonClicked);
	}

	UVerticalBoxSlot* ButtonSlot = SelectMapBox->AddChildToVerticalBox(NewButton);
	if (ButtonSlot)
	{
		ButtonSlot->SetPadding(NewButton->SlotPadding);
		ButtonSlot->SetSize(NewButton->SlotSize);
		ButtonSlot->SetHorizontalAlignment(NewButton->SlotHorizontalAlignment);
		ButtonSlot->SetVerticalAlignment(NewButton->SlotVerticalAlignment);
	}

	ButtonToMapPath.Add(NewButton, PackagePath);
}

// --- Selection ---

void UOSMapSelect::OnMapButtonClicked()
{
#if !UE_BUILD_SHIPPING
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, TEXT("[MapSelect] OnMapButtonClicked fired"));
#endif

	UOSBaseButton* Clicked = FindClickedButton();
	if (!Clicked)
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[MapSelect] FindClickedButton returned null"));
#endif
		return;
	}

	const FString* MapPath = ButtonToMapPath.Find(Clicked);
	if (!MapPath)
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[MapSelect] Button not found in ButtonToMapPath"));
#endif
		return;
	}

	SelectMap(Clicked, *MapPath);
}

UOSBaseButton* UOSMapSelect::FindClickedButton() const
{
	APlayerController* PC = GetOwningPlayer();

	for (const auto& [Button, Path] : ButtonToMapPath)
	{
		if (!Button)
		{
			continue;
		}

		if (Button->Button && Button->Button->IsHovered())
		{
			return Button;
		}

		if (PC && Button->HasUserFocus(PC))
		{
			return Button;
		}
	}

	return nullptr;
}

void UOSMapSelect::SelectMap(UOSBaseButton* ClickedButton, const FString& MapPath)
{
	if (ActiveMapButton && ActiveMapButton != ClickedButton)
		ActiveMapButton->SetActive(false);

	ClickedButton->SetActive(true);
	ActiveMapButton = ClickedButton;

	GameMapPath = MapPath;

#if !UE_BUILD_SHIPPING
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, FString::Printf(TEXT("[MapSelect] Selected: %s"), *MapPath));
#endif
}

void UOSMapSelect::OnLaunchButtonClicked()
{
	if (GameMapPath.IsEmpty())
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[MapSelect] No map selected"));
#endif
		return;
	}

	// Append ?game= to GameMapPath so TravelToGameMap picks the right mode
	const TSubclassOf<AGameModeBase>* ModeClass = GameModeClassMap.Find(SelectedGameMode);
	if (ModeClass && *ModeClass)
	{
		FString ClassPath = (*ModeClass)->GetPathName();
		GameMapPath = FString::Printf(TEXT("%s?game=%s"), *GameMapPath, *ClassPath);
	}

	FOSHostMatchSettings MatchSettings;
	MatchSettings.MapPath = GameMapPath;
	MatchSettings.GameMode = SelectedGameMode;
	MatchSettings.PlayerCount = SelectedPlayerCount;

#if !UE_BUILD_SHIPPING
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
			FString::Printf(TEXT("[MapSelect] Launching: %s | Mode: %s | Players: %d"),
				*GameMapPath, *MatchSettings.GetMatchTypeString(), MatchSettings.GetPlayerCount()));
#endif

	UGameInstance* GI = GetGameInstance();
	UOSSessionsSubsystem* Sessions = GI ? GI->GetSubsystem<UOSSessionsSubsystem>() : nullptr;
	if (!Sessions)
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[MapSelect] SessionsSubsystem not found"));
#endif
		return;
	}

	if (LaunchButton)
		LaunchButton->SetIsEnabled(false);

	bool bUseLAN = Sessions->ShouldUseLANMode();
	Sessions->HostAndTravel(GameMapPath, MatchSettings, TEXT("OnSightGameSession"), bUseLAN);
}

// --- Settings Buttons ---

void UOSMapSelect::BindSettingsButtons()
{
	if (BT_DeathMatch)		
	{
		BT_DeathMatch->OnClicked.RemoveAll(this);		
		BT_DeathMatch->OnClicked.AddDynamic(this, &UOSMapSelect::OnDeathMatchClicked); 
	}
	if (BT_TeamDeathMatch)	
	{ 
		BT_TeamDeathMatch->OnClicked.RemoveAll(this);	
		BT_TeamDeathMatch->OnClicked.AddDynamic(this, &UOSMapSelect::OnTeamDeathMatchClicked); 
	}
	if (BT_Domination)		
	{ 
		BT_Domination->OnClicked.RemoveAll(this);		
		BT_Domination->OnClicked.AddDynamic(this, &UOSMapSelect::OnDominationClicked); 
	}
	if (BT_PlayerCount2)	
	{ 
		BT_PlayerCount2->OnClicked.RemoveAll(this);	
		BT_PlayerCount2->OnClicked.AddDynamic(this, &UOSMapSelect::OnPlayerCount2Clicked); 
	}
	if (BT_PlayerCount4)	
	{ 
		BT_PlayerCount4->OnClicked.RemoveAll(this);	
		BT_PlayerCount4->OnClicked.AddDynamic(this, &UOSMapSelect::OnPlayerCount4Clicked); 
	}
	if (BT_PlayerCount6)	
	{ 
		BT_PlayerCount6->OnClicked.RemoveAll(this);	
		BT_PlayerCount6->OnClicked.AddDynamic(this, &UOSMapSelect::OnPlayerCount6Clicked); 
	}
	if (BT_PlayerCount8)	
	{ 
		BT_PlayerCount8->OnClicked.RemoveAll(this);	
		BT_PlayerCount8->OnClicked.AddDynamic(this, &UOSMapSelect::OnPlayerCount8Clicked); 
	}
	if (BT_DevPlayTest)		
	{ 
		BT_DevPlayTest->OnClicked.RemoveAll(this);	
		BT_DevPlayTest->OnClicked.AddDynamic(this, &UOSMapSelect::OnDevPlayTestClicked); 
	}
}

void UOSMapSelect::UnbindSettingsButtons()
{
	UOSBaseButton* Buttons[] = { BT_DeathMatch, BT_TeamDeathMatch, BT_Domination,
		BT_PlayerCount2, BT_PlayerCount4, BT_PlayerCount6, BT_PlayerCount8, BT_DevPlayTest };

	for (UOSBaseButton* Btn : Buttons)
	{
		if (Btn)
			Btn->OnClicked.RemoveAll(this);
	}
}

void UOSMapSelect::SetGameMode(EOSGameModeType NewMode)
{
	SelectedGameMode = NewMode;

#if !UE_BUILD_SHIPPING
	FOSHostMatchSettings Temp;
	Temp.GameMode = NewMode;
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("[MapSelect] Game Mode: %s"), *Temp.GetMatchTypeString()));
#endif
}

void UOSMapSelect::SetPlayerCount(EOSPlayerCount NewCount)
{
	SelectedPlayerCount = NewCount;

#if !UE_BUILD_SHIPPING
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("[MapSelect] Player Count: %d"), static_cast<int32>(NewCount)));
#endif
}

void UOSMapSelect::OnDeathMatchClicked()		{ SetGameMode(EOSGameModeType::DeathMatch); }
void UOSMapSelect::OnTeamDeathMatchClicked()	{ SetGameMode(EOSGameModeType::TeamDeathMatch); }
void UOSMapSelect::OnDominationClicked()		{ SetGameMode(EOSGameModeType::Domination); }
void UOSMapSelect::OnPlayerCount2Clicked()		{ SetPlayerCount(EOSPlayerCount::Players_2); }
void UOSMapSelect::OnPlayerCount4Clicked()		{ SetPlayerCount(EOSPlayerCount::Players_4); }
void UOSMapSelect::OnPlayerCount6Clicked()		{ SetPlayerCount(EOSPlayerCount::Players_6); }
void UOSMapSelect::OnPlayerCount8Clicked()		{ SetPlayerCount(EOSPlayerCount::Players_8); }
void UOSMapSelect::OnDevPlayTestClicked()		{ SetPlayerCount(EOSPlayerCount::Players_40); }

void UOSMapSelect::OnReturnMainClicked()
{
	UE_LOG(HookMapSelect, Log, TEXT("[OSMapSelect] Return_Main clicked"));

	UOSMainMenuWidget* Menu = GetTypedOuter<UOSMainMenuWidget>();
	if (Menu)
	{
		UE_LOG(HookMapSelect, Log, TEXT("[OSMapSelect] Found OSMainMenuWidget outer, calling ShowMainMenu"));
		Menu->ShowMainMenu();
	}
	else
	{
		UE_LOG(HookMapSelect, Warning, TEXT("[OSMapSelect] No OSMainMenuWidget found as outer"));
	}
}
