// Breachpoint. The screen-management spine.

#include "UI/BRUIManagerSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Core/BRCore.h"
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

	// Subsystem initialisation order vs. local-player creation is not something to bet on: catch
	// up on anyone who already exists. Missing this is how "works in PIE, not in a packaged
	// standalone client" starts.
	for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
	{
		HandleLocalPlayerAdded(LocalPlayer);
	}
}

void UBRUIManagerSubsystem::Deinitialize()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->OnLocalPlayerAddedEvent.Remove(LocalPlayerAddedHandle);
		GameInstance->OnLocalPlayerRemovedEvent.Remove(LocalPlayerRemovedHandle);
	}
	LocalPlayerAddedHandle.Reset();
	LocalPlayerRemovedHandle.Reset();

	// Copy the keys: RemoveLayoutForLocalPlayer mutates the map.
	TArray<TObjectPtr<ULocalPlayer>> Keys;
	PlayerUIs.GetKeys(Keys);
	for (const TObjectPtr<ULocalPlayer>& Key : Keys)
	{
		HandleLocalPlayerRemoved(Key.Get());
	}
	PlayerUIs.Empty();

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Local player bookkeeping
// ---------------------------------------------------------------------------

void UBRUIManagerSubsystem::HandleLocalPlayerAdded(ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer)
	{
		return;
	}

	FBRLocalPlayerUI& PlayerUI = FindOrAddPlayerUI(LocalPlayer);

	// ViewModels FIRST, and long before any widget. Gameplay is allowed to push into these from
	// its first RepNotify; a null ViewModel at that moment means state arrives and vanishes, which
	// is invisible in PIE (nothing arrives early there) and reproducible only on a real join.
	if (!PlayerUI.CombatViewModel)
	{
		PlayerUI.CombatViewModel = NewObject<UBRVM_Combat>(this);
	}
	if (!PlayerUI.MatchViewModel)
	{
		PlayerUI.MatchViewModel = NewObject<UBRVM_Match>(this);
	}

	PublishViewModelsToGlobalCollection(PlayerUI);

	UE_LOG(LogBRUI, Log, TEXT("UI ready for local player %s (ViewModels created; layout pending a PlayerController)."),
		*GetNameSafe(LocalPlayer));
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
	// TMap keyed on TObjectPtr: const_cast only to build the lookup key, never to mutate.
	return PlayerUIs.Find(const_cast<ULocalPlayer*>(LocalPlayer));
}

// ---------------------------------------------------------------------------
// ViewModels
// ---------------------------------------------------------------------------

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

UBRVM_Combat* UBRUIManagerSubsystem::GetPrimaryCombatViewModel() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GetCombatViewModel(GameInstance->GetFirstGamePlayer()) : nullptr;
}

UBRVM_Match* UBRUIManagerSubsystem::GetPrimaryMatchViewModel() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GetMatchViewModel(GameInstance->GetFirstGamePlayer()) : nullptr;
}

void UBRUIManagerSubsystem::PublishViewModelsToGlobalCollection(const FBRLocalPlayerUI& PlayerUI)
{
	// One collection per GameInstance means one player's ViewModels can live there. Publishing a
	// second player's would silently overwrite the first and every WBP would bind to the wrong
	// pawn - so we publish exactly once and say nothing further.
	if (PublishedLocalPlayer)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UMVVMGameSubsystem* MVVMSubsystem = GameInstance ? GameInstance->GetSubsystem<UMVVMGameSubsystem>() : nullptr;
	UMVVMViewModelCollectionObject* CollectionObject = MVVMSubsystem ? MVVMSubsystem->GetViewModelCollection() : nullptr;
	if (!CollectionObject)
	{
		UE_LOG(LogBRUI, Warning,
			TEXT("MVVM game subsystem unavailable; WBPs must be handed ViewModels explicitly."));
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

	if (const UGameInstance* GI = GetGameInstance())
	{
		PublishedLocalPlayer = GI->GetFirstGamePlayer();
	}
}

void UBRUIManagerSubsystem::UnpublishViewModelsFromGlobalCollection(const FBRLocalPlayerUI& PlayerUI)
{
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

	PublishedLocalPlayer = nullptr;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

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
		// Not an error, just early. AddToPlayerScreen requires a PlayerController; the caller
		// (ABRPlayerController) will be back.
		UE_LOG(LogBRUI, Verbose, TEXT("CreateLayoutForLocalPlayer: no PlayerController yet."));
		return nullptr;
	}

	const UBRUISettings& Settings = UBRUISettings::Get();
	if (Settings.RootLayoutClass.IsNull())
	{
		UE_LOG(LogBRUI, Error,
			TEXT("BRUISettings.RootLayoutClass is unset. Set it in Config/DefaultGame.ini under ")
			TEXT("[/Script/Breachpoint.BRUISettings] to a WBP deriving from UBRRootLayout. ")
			TEXT("No HUD will appear until then."));
		return nullptr;
	}

	// Soft -> hard at the last possible moment (law 3). Synchronous is acceptable here and only
	// here: this runs once, at possession, before the player has control.
	UClass* LayoutClass = Settings.RootLayoutClass.LoadSynchronous();
	if (!LayoutClass)
	{
		UE_LOG(LogBRUI, Error, TEXT("Failed to load RootLayoutClass '%s'."),
			*Settings.RootLayoutClass.ToString());
		return nullptr;
	}

	UBRRootLayout* Layout = CreateWidget<UBRRootLayout>(OwningPC, LayoutClass);
	if (!Layout)
	{
		UE_LOG(LogBRUI, Error, TEXT("CreateWidget failed for RootLayoutClass '%s'."), *LayoutClass->GetName());
		return nullptr;
	}

	// ZOrder 0: this widget IS the UI. Everything else is a layer inside it, so there is exactly
	// one thing in the viewport and no z-order arms race between screens.
	Layout->AddToPlayerScreen(0);
	PlayerUI.RootLayout = Layout;

	UE_LOG(LogBRUI, Log, TEXT("Root layout '%s' created for %s."), *LayoutClass->GetName(), *GetNameSafe(LocalPlayer));
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

// ---------------------------------------------------------------------------
// Screen stack
// ---------------------------------------------------------------------------

UBRActivatableWidget* UBRUIManagerSubsystem::PushWidgetToLayer(
	ULocalPlayer* LocalPlayer, FUITag LayerTag, const TSoftClassPtr<UBRActivatableWidget>& WidgetClass)
{
	if (WidgetClass.IsNull())
	{
		UE_LOG(LogBRUI, Error, TEXT("PushWidgetToLayer(%s): the configured widget class is unset."),
			*LayerTag.ToString());
		return nullptr;
	}

	const bool bWasResident = WidgetClass.Get() != nullptr;
	UClass* ResolvedClass = WidgetClass.LoadSynchronous();
	if (!bWasResident)
	{
		// Say it out loud. A synchronous class load during a match is a hitch, and the only way
		// anyone finds out is if the code that does it admits to it.
		UE_LOG(LogBRUI, Log, TEXT("Synchronously loaded widget class '%s' for layer %s."),
			*WidgetClass.ToString(), *LayerTag.ToString());
	}

	return PushWidgetClassToLayer(LocalPlayer, LayerTag, ResolvedClass);
}

UBRActivatableWidget* UBRUIManagerSubsystem::PushWidgetClassToLayer(
	ULocalPlayer* LocalPlayer, FUITag LayerTag, TSubclassOf<UBRActivatableWidget> WidgetClass)
{
	UBRRootLayout* Layout = GetRootLayout(LocalPlayer);
	if (!Layout)
	{
		UE_LOG(LogBRUI, Warning, TEXT("PushWidgetClassToLayer(%s): no root layout for that local player."),
			*LayerTag.ToString());
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

// ---------------------------------------------------------------------------
// Convenience flows
// ---------------------------------------------------------------------------

UBRActivatableWidget* UBRUIManagerSubsystem::ShowHUD(ULocalPlayer* LocalPlayer)
{
	return PushWidgetToLayer(LocalPlayer, FBRUITags::Get().Layer_Game, UBRUISettings::Get().HUDLayoutClass);
}

UBRActivatableWidget* UBRUIManagerSubsystem::ShowMainMenu(ULocalPlayer* LocalPlayer)
{
	return PushWidgetToLayer(LocalPlayer, FBRUITags::Get().Layer_Menu, UBRUISettings::Get().MainMenuScreenClass);
}

UBRActivatableWidget* UBRUIManagerSubsystem::ShowDeathOverlay(ULocalPlayer* LocalPlayer)
{
	return PushWidgetToLayer(LocalPlayer, FBRUITags::Get().Layer_GameMenu, UBRUISettings::Get().DeathOverlayClass);
}

UBRActivatableWidget* UBRUIManagerSubsystem::ShowCarnageReport(ULocalPlayer* LocalPlayer)
{
	return PushWidgetToLayer(LocalPlayer, FBRUITags::Get().Layer_GameMenu, UBRUISettings::Get().CarnageReportClass);
}
