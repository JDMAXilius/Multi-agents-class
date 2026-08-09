#include "UI/OSCommonMapSelect.h"

#include "UI/Components/OSBaseCommonButton.h"
#include "CommonButtonBase.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/OSSessionsSubsystem.h"
#include "OSLogCategories.h"

// --- Lifecycle ---

void UOSCommonMapSelect::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
		ScanAndPopulateMaps();
}

void UOSCommonMapSelect::NativeConstruct()
{
	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] NativeConstruct"));
	Super::NativeConstruct();

	ScanAndPopulateMaps();
	BindLaunchAndReturnButtons();
	BindSettingsButtons();
}

void UOSCommonMapSelect::NativeDestruct()
{
	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] NativeDestruct"));

	UnbindMapButtons();
	UnbindLaunchAndReturnButtons();
	UnbindSettingsButtons();

	Super::NativeDestruct();
}

void UOSCommonMapSelect::NativeOnActivated()
{
	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] NativeOnActivated"));
	Super::NativeOnActivated();
}

void UOSCommonMapSelect::NativeOnDeactivated()
{
	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] NativeOnDeactivated"));
	Super::NativeOnDeactivated();
}

UWidget* UOSCommonMapSelect::NativeGetDesiredFocusTarget() const
{
	if (ActiveMapButton)
		return ActiveMapButton;

	for (const auto& Pair : ButtonToMapPath)
	{
		if (Pair.Key)
			return Pair.Key;
	}

	if (LaunchButton)
		return LaunchButton;

	return Return_Main;
}

// --- Map Discovery ---

void UOSCommonMapSelect::ScanAndPopulateMaps()
{
	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] ScanAndPopulateMaps: SelectMapBox=%s, MapButtonClass=%s"),
		SelectMapBox ? TEXT("valid") : TEXT("null"),
		MapButtonClass ? *MapButtonClass->GetName() : TEXT("null"));

	if (!SelectMapBox || !MapButtonClass)
		return;

	UnbindMapButtons();
	SelectMapBox->ClearChildren();
	ButtonToMapPath.Empty();
	ActiveMapButton = nullptr;

	TArray<FAssetData> MapAssets = GetMapAssetsInPath();
	ApplyMapNameFilter(MapAssets);

	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] Found %d map assets in path: %s"), MapAssets.Num(), *MapScanPath);

	const bool bBindClick = !IsDesignTime();
	for (const FAssetData& Asset : MapAssets)
	{
		UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] Spawning button for: %s"), *Asset.AssetName.ToString());
		SpawnMapButton(Asset, bBindClick);
	}
}

TArray<FAssetData> UOSCommonMapSelect::GetMapAssetsInPath() const
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

void UOSCommonMapSelect::ApplyMapNameFilter(TArray<FAssetData>& MapAssets) const
{
	if (AllowedMapNames.Num() == 0)
		return;

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

FString UOSCommonMapSelect::ResolveDisplayName(const FString& AssetName) const
{
	if (const FString* Override = MapDisplayNames.Find(AssetName))
		return *Override;

	FString Pretty = AssetName;
	Pretty.ReplaceInline(TEXT("_"), TEXT(" "));
	return Pretty;
}

void UOSCommonMapSelect::SpawnMapButton(const FAssetData& MapAsset, bool bBindClick)
{
	UOSBaseCommonButton* NewButton = IsDesignTime()
		? CreateWidget<UOSBaseCommonButton>(this, MapButtonClass)
		: CreateWidget<UOSBaseCommonButton>(GetOwningPlayer(), MapButtonClass);
	if (!NewButton)
	{
		UE_LOG(HookMapSelect, Warning, TEXT("[OSCommonMapSelect] CreateWidget failed for %s"), *MapAsset.AssetName.ToString());
		return;
	}

	const FString AssetName = MapAsset.AssetName.ToString();
	const FString PackagePath = MapAsset.PackageName.ToString();

	NewButton->SetButtonName(FText::FromString(ResolveDisplayName(AssetName)));

	if (bBindClick)
	{
		TWeakObjectPtr<UOSCommonMapSelect> WeakThis(this);
		TWeakObjectPtr<UOSBaseCommonButton> WeakButton(NewButton);
		NewButton->OnClicked().AddWeakLambda(this, [WeakThis, WeakButton]()
		{
			if (WeakThis.IsValid() && WeakButton.IsValid())
				WeakThis->HandleMapButtonClicked(WeakButton.Get());
		});
	}

	if (UVerticalBoxSlot* ButtonSlot = SelectMapBox->AddChildToVerticalBox(NewButton))
	{
		ButtonSlot->SetPadding(NewButton->SlotPadding);
		ButtonSlot->SetSize(NewButton->SlotSize);
		ButtonSlot->SetHorizontalAlignment(NewButton->SlotHorizontalAlignment);
		ButtonSlot->SetVerticalAlignment(NewButton->SlotVerticalAlignment);
	}

	ButtonToMapPath.Add(NewButton, PackagePath);
}

// --- Button Wiring ---

void UOSCommonMapSelect::BindLaunchAndReturnButtons()
{
	if (LaunchButton)
	{
		LaunchButton->OnClicked().RemoveAll(this);
		LaunchButton->OnClicked().AddUObject(this, &UOSCommonMapSelect::OnLaunchButtonClicked);
	}

	if (Return_Main)
	{
		UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] Return_Main found, binding click"));
		Return_Main->OnClicked().RemoveAll(this);
		Return_Main->OnClicked().AddUObject(this, &UOSCommonMapSelect::OnReturnMainClicked);
	}
	else
	{
		UE_LOG(HookMapSelect, Warning, TEXT("[OSCommonMapSelect] Return_Main is NULL - not bound in widget"));
	}
}

void UOSCommonMapSelect::UnbindLaunchAndReturnButtons()
{
	if (LaunchButton)
		LaunchButton->OnClicked().RemoveAll(this);
	if (Return_Main)
		Return_Main->OnClicked().RemoveAll(this);
}

void UOSCommonMapSelect::BindSettingsButtons()
{
	if (BT_DeathMatch)
	{
		BT_DeathMatch->OnClicked().RemoveAll(this);
		BT_DeathMatch->OnClicked().AddUObject(this, &UOSCommonMapSelect::OnDeathMatchClicked);
	}
	if (BT_TeamDeathMatch)
	{
		BT_TeamDeathMatch->OnClicked().RemoveAll(this);
		BT_TeamDeathMatch->OnClicked().AddUObject(this, &UOSCommonMapSelect::OnTeamDeathMatchClicked);
	}
	if (BT_Domination)
	{
		BT_Domination->OnClicked().RemoveAll(this);
		BT_Domination->OnClicked().AddUObject(this, &UOSCommonMapSelect::OnDominationClicked);
	}
	if (BT_PlayerCount2)
	{
		BT_PlayerCount2->OnClicked().RemoveAll(this);
		BT_PlayerCount2->OnClicked().AddUObject(this, &UOSCommonMapSelect::OnPlayerCount2Clicked);
	}
	if (BT_PlayerCount4)
	{
		BT_PlayerCount4->OnClicked().RemoveAll(this);
		BT_PlayerCount4->OnClicked().AddUObject(this, &UOSCommonMapSelect::OnPlayerCount4Clicked);
	}
	if (BT_PlayerCount6)
	{
		BT_PlayerCount6->OnClicked().RemoveAll(this);
		BT_PlayerCount6->OnClicked().AddUObject(this, &UOSCommonMapSelect::OnPlayerCount6Clicked);
	}
	if (BT_PlayerCount8)
	{
		BT_PlayerCount8->OnClicked().RemoveAll(this);
		BT_PlayerCount8->OnClicked().AddUObject(this, &UOSCommonMapSelect::OnPlayerCount8Clicked);
	}
	if (BT_DevPlayTest)
	{
		BT_DevPlayTest->OnClicked().RemoveAll(this);
		BT_DevPlayTest->OnClicked().AddUObject(this, &UOSCommonMapSelect::OnDevPlayTestClicked);
	}
}

void UOSCommonMapSelect::UnbindSettingsButtons()
{
	UOSBaseCommonButton* Buttons[] = { BT_DeathMatch, BT_TeamDeathMatch, BT_Domination,
		BT_PlayerCount2, BT_PlayerCount4, BT_PlayerCount6, BT_PlayerCount8, BT_DevPlayTest };

	for (UOSBaseCommonButton* Btn : Buttons)
	{
		if (Btn)
			Btn->OnClicked().RemoveAll(this);
	}
}

void UOSCommonMapSelect::UnbindMapButtons()
{
	for (auto& Pair : ButtonToMapPath)
	{
		if (Pair.Key)
			Pair.Key->OnClicked().RemoveAll(this);
	}
}

// --- Selection ---

void UOSCommonMapSelect::HandleMapButtonClicked(UOSBaseCommonButton* ClickedButton)
{
	if (!ClickedButton)
	{
		UE_LOG(HookMapSelect, Warning, TEXT("[OSCommonMapSelect] HandleMapButtonClicked: null button"));
		return;
	}

	const FString* MapPath = ButtonToMapPath.Find(ClickedButton);
	if (!MapPath)
	{
		UE_LOG(HookMapSelect, Warning, TEXT("[OSCommonMapSelect] Clicked button not in ButtonToMapPath"));
		return;
	}

	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] Map button clicked: %s"), **MapPath);
	SelectMap(ClickedButton, *MapPath);
}

void UOSCommonMapSelect::SelectMap(UOSBaseCommonButton* ClickedButton, const FString& MapPath)
{
	if (ActiveMapButton && ActiveMapButton != ClickedButton)
		ActiveMapButton->SetIsSelected(false, false);

	ClickedButton->SetIsSelected(true, false);
	ActiveMapButton = ClickedButton;

	GameMapPath = MapPath;
	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] Selected: %s"), *MapPath);
}

// --- Host Helpers ---

FString UOSCommonMapSelect::BuildTravelUrlWithMode() const
{
	const TSubclassOf<AGameModeBase>* ModeClass = GameModeClassMap.Find(SelectedGameMode);
	if (ModeClass && *ModeClass)
	{
		const FString ClassPath = (*ModeClass)->GetPathName();
		return FString::Printf(TEXT("%s?game=%s"), *GameMapPath, *ClassPath);
	}
	return GameMapPath;
}

void UOSCommonMapSelect::OnLaunchButtonClicked()
{
	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] Launch clicked"));

	if (GameMapPath.IsEmpty())
	{
		UE_LOG(HookMapSelect, Warning, TEXT("[OSCommonMapSelect] Launch aborted - no map selected"));
		return;
	}

	GameMapPath = BuildTravelUrlWithMode();

	FOSHostMatchSettings MatchSettings;
	MatchSettings.MapPath = GameMapPath;
	MatchSettings.GameMode = SelectedGameMode;
	MatchSettings.PlayerCount = SelectedPlayerCount;

	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] Launching: %s | Mode: %s | Players: %d"),
		*GameMapPath, *MatchSettings.GetMatchTypeString(), MatchSettings.GetPlayerCount());

	UGameInstance* GI = GetGameInstance();
	UOSSessionsSubsystem* Sessions = GI ? GI->GetSubsystem<UOSSessionsSubsystem>() : nullptr;
	if (!Sessions)
	{
		UE_LOG(HookMapSelect, Warning, TEXT("[OSCommonMapSelect] SessionsSubsystem not found"));
		return;
	}

	if (LaunchButton)
		LaunchButton->SetIsEnabled(false);

	const bool bUseLAN = Sessions->ShouldUseLANMode();
	Sessions->HostAndTravel(GameMapPath, MatchSettings, TEXT("OnSightGameSession"), bUseLAN);
}

void UOSCommonMapSelect::OnReturnMainClicked()
{
	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] Return_Main clicked - deactivating"));
	DeactivateWidget();
}

// --- Match Settings Helpers ---

void UOSCommonMapSelect::SetGameMode(EOSGameModeType NewMode)
{
	SelectedGameMode = NewMode;

	FOSHostMatchSettings Temp;
	Temp.GameMode = NewMode;
	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] Game Mode: %s"), *Temp.GetMatchTypeString());
}

void UOSCommonMapSelect::SetPlayerCount(EOSPlayerCount NewCount)
{
	SelectedPlayerCount = NewCount;
	UE_LOG(HookMapSelect, Log, TEXT("[OSCommonMapSelect] Player Count: %d"), static_cast<int32>(NewCount));
}

void UOSCommonMapSelect::OnDeathMatchClicked()     { SetGameMode(EOSGameModeType::DeathMatch); }
void UOSCommonMapSelect::OnTeamDeathMatchClicked() { SetGameMode(EOSGameModeType::TeamDeathMatch); }
void UOSCommonMapSelect::OnDominationClicked()     { SetGameMode(EOSGameModeType::Domination); }
void UOSCommonMapSelect::OnPlayerCount2Clicked()   { SetPlayerCount(EOSPlayerCount::Players_2); }
void UOSCommonMapSelect::OnPlayerCount4Clicked()   { SetPlayerCount(EOSPlayerCount::Players_4); }
void UOSCommonMapSelect::OnPlayerCount6Clicked()   { SetPlayerCount(EOSPlayerCount::Players_6); }
void UOSCommonMapSelect::OnPlayerCount8Clicked()   { SetPlayerCount(EOSPlayerCount::Players_8); }
void UOSCommonMapSelect::OnDevPlayTestClicked()    { SetPlayerCount(EOSPlayerCount::Players_40); }
