#include "UI/BRUIManagerSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Core/BRCore.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MVVMGameSubsystem.h"
#include "Types/MVVMViewModelCollection.h"
#include "Types/MVVMViewModelContext.h"
#include "UI/BRActivatableWidget.h"
#include "UI/BRRootLayout.h"
#include "UI/BRUISettings.h"
#include "UI/BRViewModels.h"
#include "UI/Screens/BRScreen_FrontEnd.h"
#include "UI/ViewModels/BRVM_FrontEnd.h"
#include "UI/ViewModels/BRVM_Player.h"

UBRUIManagerSubsystem* UBRUIManagerSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UBRUIManagerSubsystem>() : nullptr;
}

void UBRUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	LocalPlayerAddedHandle = GameInstance->OnLocalPlayerAddedEvent.AddUObject(
		this, &UBRUIManagerSubsystem::HandleLocalPlayerAdded);
	LocalPlayerRemovedHandle = GameInstance->OnLocalPlayerRemovedEvent.AddUObject(
		this, &UBRUIManagerSubsystem::HandleLocalPlayerRemoved);

	for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
	{
		HandleLocalPlayerAdded(LocalPlayer);
	}

	// Warm the mid-match screens now, off the game thread. Unset paths (DeathOverlay and
	// CarnageReport have no assets yet) simply do not appear in the request.
	const UBRUISettings& Settings = UBRUISettings::Get();
	TArray<FSoftObjectPath> MidMatchClasses;
	for (const TSoftClassPtr<UBRActivatableWidget>& SoftClass :
		{ Settings.HUDLayoutClass, Settings.DeathOverlayClass, Settings.CarnageReportClass })
	{
		if (!SoftClass.IsNull())
		{
			MidMatchClasses.Add(SoftClass.ToSoftObjectPath());
		}
	}
	if (MidMatchClasses.Num() > 0)
	{
		MidMatchPreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MidMatchClasses);
	}
}

void UBRUIManagerSubsystem::Deinitialize()
{
	if (MidMatchPreloadHandle.IsValid())
	{
		MidMatchPreloadHandle->ReleaseHandle();
		MidMatchPreloadHandle.Reset();
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->OnLocalPlayerAddedEvent.Remove(LocalPlayerAddedHandle);
		GameInstance->OnLocalPlayerRemovedEvent.Remove(LocalPlayerRemovedHandle);
	}
	LocalPlayerAddedHandle.Reset();
	LocalPlayerRemovedHandle.Reset();

	TArray<TObjectPtr<ULocalPlayer>> Keys;
	PlayerUIs.GetKeys(Keys);
	for (const TObjectPtr<ULocalPlayer>& Key : Keys)
	{
		HandleLocalPlayerRemoved(Key.Get());
	}
	PlayerUIs.Empty();

	Super::Deinitialize();
}

void UBRUIManagerSubsystem::HandleLocalPlayerAdded(ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer)
	{
		return;
	}

	FBRLocalPlayerUI& PlayerUI = FindOrAddPlayerUI(LocalPlayer);
	PlayerUI.Owner = LocalPlayer;

	if (!PlayerUI.CombatViewModel)
	{
		PlayerUI.CombatViewModel = NewObject<UBRVM_Combat>(this);
	}
	if (!PlayerUI.MatchViewModel)
	{
		PlayerUI.MatchViewModel = NewObject<UBRVM_Match>(this);
	}
	if (!PlayerUI.FrontEndViewModel)
	{
		PlayerUI.FrontEndViewModel = NewObject<UBRVM_FrontEnd>(this);
	}
	if (!PlayerUI.PlayerViewModel)
	{
		PlayerUI.PlayerViewModel = NewObject<UBRVM_Player>(this);
	}

	PublishViewModelsToGlobalCollection(PlayerUI);
}

void UBRUIManagerSubsystem::HandleLocalPlayerRemoved(ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer)
	{
		return;
	}

	if (FBRLocalPlayerUI* PlayerUI = PlayerUIs.Find(LocalPlayer))
	{
		UnpublishViewModelsFromGlobalCollection(*PlayerUI);

		if (PlayerUI->RootLayout)
		{
			PlayerUI->RootLayout->RemoveFromParent();
			PlayerUI->RootLayout = nullptr;
		}
		if (PlayerUI->CombatViewModel)
		{
			PlayerUI->CombatViewModel->ClearToUnknown();
		}
		if (PlayerUI->MatchViewModel)
		{
			PlayerUI->MatchViewModel->ClearToUnknown();
		}
		if (PlayerUI->FrontEndViewModel)
		{
			PlayerUI->FrontEndViewModel->ClearToUnknown();
		}
		if (PlayerUI->PlayerViewModel)
		{
			PlayerUI->PlayerViewModel->ClearToUnknown();
		}
	}

	PlayerUIs.Remove(LocalPlayer);
}

FBRLocalPlayerUI& UBRUIManagerSubsystem::FindOrAddPlayerUI(ULocalPlayer* LocalPlayer)
{
	return PlayerUIs.FindOrAdd(LocalPlayer);
}

const FBRLocalPlayerUI* UBRUIManagerSubsystem::FindPlayerUI(const ULocalPlayer* LocalPlayer) const
{
	if (!LocalPlayer)
	{
		return nullptr;
	}
	return PlayerUIs.Find(const_cast<ULocalPlayer*>(LocalPlayer));
}

UBRVM_Combat* UBRUIManagerSubsystem::GetCombatViewModel(const ULocalPlayer* LocalPlayer) const
{
	const FBRLocalPlayerUI* PlayerUI = FindPlayerUI(LocalPlayer);
	return PlayerUI ? PlayerUI->CombatViewModel : nullptr;
}

UBRVM_Match* UBRUIManagerSubsystem::GetMatchViewModel(const ULocalPlayer* LocalPlayer) const
{
	const FBRLocalPlayerUI* PlayerUI = FindPlayerUI(LocalPlayer);
	return PlayerUI ? PlayerUI->MatchViewModel : nullptr;
}

UBRVM_FrontEnd* UBRUIManagerSubsystem::GetFrontEndViewModel(const ULocalPlayer* LocalPlayer) const
{
	const FBRLocalPlayerUI* PlayerUI = FindPlayerUI(LocalPlayer);
	return PlayerUI ? PlayerUI->FrontEndViewModel : nullptr;
}

UBRVM_Player* UBRUIManagerSubsystem::GetPlayerViewModel(const ULocalPlayer* LocalPlayer) const
{
	const FBRLocalPlayerUI* PlayerUI = FindPlayerUI(LocalPlayer);
	return PlayerUI ? PlayerUI->PlayerViewModel : nullptr;
}

void UBRUIManagerSubsystem::PublishViewModelsToGlobalCollection(const FBRLocalPlayerUI& PlayerUI)
{
	if (PublishedLocalPlayer)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UMVVMGameSubsystem* MVVMSubsystem = GameInstance ? GameInstance->GetSubsystem<UMVVMGameSubsystem>() : nullptr;
	UMVVMViewModelCollectionObject* CollectionObject = MVVMSubsystem ? MVVMSubsystem->GetViewModelCollection() : nullptr;
	if (!CollectionObject)
	{
		return;
	}

	const UBRUISettings& Settings = UBRUISettings::Get();

	if (PlayerUI.CombatViewModel)
	{
		FMVVMViewModelContext Context;
		Context.ContextClass = UBRVM_Combat::StaticClass();
		Context.ContextName = Settings.CombatViewModelContextName;
		CollectionObject->AddViewModelInstance(Context, PlayerUI.CombatViewModel);
	}
	if (PlayerUI.MatchViewModel)
	{
		FMVVMViewModelContext Context;
		Context.ContextClass = UBRVM_Match::StaticClass();
		Context.ContextName = Settings.MatchViewModelContextName;
		CollectionObject->AddViewModelInstance(Context, PlayerUI.MatchViewModel);
	}
	if (PlayerUI.FrontEndViewModel)
	{
		FMVVMViewModelContext Context;
		Context.ContextClass = UBRVM_FrontEnd::StaticClass();
		Context.ContextName = Settings.FrontEndViewModelContextName;
		CollectionObject->AddViewModelInstance(Context, PlayerUI.FrontEndViewModel);
	}
	if (PlayerUI.PlayerViewModel)
	{
		FMVVMViewModelContext Context;
		Context.ContextClass = UBRVM_Player::StaticClass();
		Context.ContextName = Settings.PlayerViewModelContextName;
		CollectionObject->AddViewModelInstance(Context, PlayerUI.PlayerViewModel);
	}

	// The published player is the one whose VMs went into the collection — which the guard at
	// the top of this function makes the first caller, not necessarily GetFirstGamePlayer().
	PublishedLocalPlayer = PlayerUI.Owner;
}

void UBRUIManagerSubsystem::UnpublishViewModelsFromGlobalCollection(const FBRLocalPlayerUI& PlayerUI)
{
	// CPP-AUDIT D6: only the player whose VMs ARE the published set may unpublish. Without this,
	// player 2 leaving split-screen removed player 1's global contexts and every bound widget
	// went null mid-match with no re-publish path.
	if (PlayerUI.Owner != PublishedLocalPlayer)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UMVVMGameSubsystem* MVVMSubsystem = GameInstance ? GameInstance->GetSubsystem<UMVVMGameSubsystem>() : nullptr;
	UMVVMViewModelCollectionObject* CollectionObject = MVVMSubsystem ? MVVMSubsystem->GetViewModelCollection() : nullptr;
	if (!CollectionObject)
	{
		return;
	}

	const UBRUISettings& Settings = UBRUISettings::Get();

	FMVVMViewModelContext CombatContext;
	CombatContext.ContextClass = UBRVM_Combat::StaticClass();
	CombatContext.ContextName = Settings.CombatViewModelContextName;
	CollectionObject->RemoveViewModel(CombatContext);

	FMVVMViewModelContext MatchContext;
	MatchContext.ContextClass = UBRVM_Match::StaticClass();
	MatchContext.ContextName = Settings.MatchViewModelContextName;
	CollectionObject->RemoveViewModel(MatchContext);

	FMVVMViewModelContext FrontEndContext;
	FrontEndContext.ContextClass = UBRVM_FrontEnd::StaticClass();
	FrontEndContext.ContextName = Settings.FrontEndViewModelContextName;
	CollectionObject->RemoveViewModel(FrontEndContext);

	FMVVMViewModelContext PlayerContext;
	PlayerContext.ContextClass = UBRVM_Player::StaticClass();
	PlayerContext.ContextName = Settings.PlayerViewModelContextName;
	CollectionObject->RemoveViewModel(PlayerContext);

	PublishedLocalPlayer = nullptr;
}

UBRRootLayout* UBRUIManagerSubsystem::GetRootLayout(const ULocalPlayer* LocalPlayer) const
{
	const FBRLocalPlayerUI* PlayerUI = FindPlayerUI(LocalPlayer);
	return PlayerUI ? PlayerUI->RootLayout : nullptr;
}

UBRRootLayout* UBRUIManagerSubsystem::CreateLayoutForLocalPlayer(ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer)
	{
		return nullptr;
	}

	FBRLocalPlayerUI& PlayerUI = FindOrAddPlayerUI(LocalPlayer);
	if (PlayerUI.RootLayout)
	{
		return PlayerUI.RootLayout;
	}

	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	APlayerController* OwningPC = LocalPlayer->GetPlayerController(World);
	if (!OwningPC)
	{
		return nullptr;
	}

	const UBRUISettings& Settings = UBRUISettings::Get();
	if (Settings.RootLayoutClass.IsNull())
	{
		return nullptr;
	}

	UClass* LayoutClass = Settings.RootLayoutClass.LoadSynchronous();
	if (!LayoutClass)
	{
		return nullptr;
	}

	UBRRootLayout* Layout = CreateWidget<UBRRootLayout>(OwningPC, LayoutClass);
	if (!Layout)
	{
		return nullptr;
	}

	Layout->AddToPlayerScreen(0);
	PlayerUI.RootLayout = Layout;

	return Layout;
}

void UBRUIManagerSubsystem::RemoveLayoutForLocalPlayer(ULocalPlayer* LocalPlayer)
{
	if (FBRLocalPlayerUI* PlayerUI = LocalPlayer ? PlayerUIs.Find(LocalPlayer) : nullptr)
	{
		if (PlayerUI->RootLayout)
		{
			PlayerUI->RootLayout->RemoveFromParent();
			PlayerUI->RootLayout = nullptr;
		}
	}
}

UBRActivatableWidget* UBRUIManagerSubsystem::PushWidgetToLayer(
	ULocalPlayer* LocalPlayer, FUITag LayerTag, const TSoftClassPtr<UBRActivatableWidget>& WidgetClass)
{
	if (WidgetClass.IsNull())
	{
		return nullptr;
	}

	// Sync resolve is the accepted cost at boot/menu time; the three MID-MATCH screens are
	// preloaded in Initialize so this is a no-op lookup by the time a death fires (CPP-AUDIT).
	UClass* ResolvedClass = WidgetClass.LoadSynchronous();

	return PushWidgetClassToLayer(LocalPlayer, LayerTag, ResolvedClass);
}

UBRActivatableWidget* UBRUIManagerSubsystem::PushWidgetClassToLayer(
	ULocalPlayer* LocalPlayer, FUITag LayerTag, TSubclassOf<UBRActivatableWidget> WidgetClass)
{
	UBRRootLayout* Layout = GetRootLayout(LocalPlayer);
	if (!Layout)
	{
		return nullptr;
	}

	return Layout->PushWidgetToLayer(LayerTag, WidgetClass);
}

void UBRUIManagerSubsystem::RemoveWidgetFromLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag, UBRActivatableWidget* Widget)
{
	if (UBRRootLayout* Layout = GetRootLayout(LocalPlayer))
	{
		Layout->RemoveWidgetFromLayer(LayerTag, Widget);
	}
}

void UBRUIManagerSubsystem::ClearLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag)
{
	if (UBRRootLayout* Layout = GetRootLayout(LocalPlayer))
	{
		Layout->ClearLayer(LayerTag);
	}
}

UBRActivatableWidget* UBRUIManagerSubsystem::ShowHUD(ULocalPlayer* LocalPlayer)
{
	return PushWidgetToLayer(LocalPlayer, FBRUITags::Get().Layer_Game, UBRUISettings::Get().HUDLayoutClass);
}

UBRActivatableWidget* UBRUIManagerSubsystem::ShowMainMenu(ULocalPlayer* LocalPlayer)
{
	UBRActivatableWidget* Pushed = PushWidgetToLayer(LocalPlayer, FBRUITags::Get().Layer_Menu, UBRUISettings::Get().MainMenuScreenClass);

	// The screen cannot fetch its own ViewModel (its header files the gap); it is pushed here,
	// closing the loop. SetFrontEndViewModel unbinds-then-rebinds when already activated, so
	// arriving after activation is safe (CPP-AUDIT D7).
	if (UBRScreen_FrontEnd* FrontEnd = Cast<UBRScreen_FrontEnd>(Pushed))
	{
		FrontEnd->SetFrontEndViewModel(GetFrontEndViewModel(LocalPlayer));
	}

	return Pushed;
}

namespace BRUIManagerConsole
{
	/**
	 * `BR.ShowSettings` -- push the settings screen for the first local player.
	 *
	 * WHY A CONSOLE COMMAND EXISTS AT ALL. Nothing in the front end navigates to settings yet
	 * (the main menu has no settings row wired), so without this the screen is reachable only
	 * from C++ that does not exist. That makes it unverifiable: `ui-presentation` §11 requires a
	 * screen be RENDERED and looked at, and "it compiles" is not that.
	 *
	 * It is a development affordance, not a shipping entry point. When the main menu grows a
	 * settings row it calls `ShowSettings` directly and this stays as the debug path.
	 *
	 * `FAutoConsoleCommandWithWorld` gives the world, which is the only way to find the local
	 * player without reaching for a global.
	 */
	static FAutoConsoleCommandWithWorld GShowSettingsCmd(
		TEXT("BR.ShowSettings"),
		TEXT("Push the Breachpoint settings screen for the first local player."),
		FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
		{
			if (!World)
			{
				return;
			}

			UBRUIManagerSubsystem* Manager = UBRUIManagerSubsystem::Get(World);
			ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();

			if (!Manager || !LocalPlayer)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("BR.ShowSettings: no UI manager or no local player in this world."));
				return;
			}

			// The layout is created per local player on join; without it there is no layer to
			// push onto and the push would silently no-op.
			if (!Manager->GetRootLayout(LocalPlayer))
			{
				Manager->CreateLayoutForLocalPlayer(LocalPlayer);
			}

			if (!Manager->ShowSettings(LocalPlayer))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("BR.ShowSettings: push returned null. Check UBRUISettings::SettingsScreenClass ")
					TEXT("in Config/DefaultGame.ini."));
			}
		}));
}

UBRActivatableWidget* UBRUIManagerSubsystem::ShowSettings(ULocalPlayer* LocalPlayer)
{
	// Layer.Menu, not Layer.Modal. Settings is a full navigation LEVEL -- it takes exclusive
	// Menu input and claims the back action -- whereas Layer.Modal is for the confirm prompts
	// it raises ON TOP of itself. Pushing it to Modal would put the discard confirm on the same
	// layer as the thing it is asking about.
	return PushWidgetToLayer(LocalPlayer, FBRUITags::Get().Layer_Menu, UBRUISettings::Get().SettingsScreenClass);
}

UBRActivatableWidget* UBRUIManagerSubsystem::ShowDeathOverlay(ULocalPlayer* LocalPlayer)
{
	return PushWidgetToLayer(LocalPlayer, FBRUITags::Get().Layer_GameMenu, UBRUISettings::Get().DeathOverlayClass);
}

UBRActivatableWidget* UBRUIManagerSubsystem::ShowCarnageReport(ULocalPlayer* LocalPlayer)
{
	return PushWidgetToLayer(LocalPlayer, FBRUITags::Get().Layer_GameMenu, UBRUISettings::Get().CarnageReportClass);
}
