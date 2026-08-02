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

	if (!PlayerUI.CombatViewModel)
	{
		PlayerUI.CombatViewModel = NewObject<UBRVM_Combat>(this);
	}
	if (!PlayerUI.MatchViewModel)
	{
		PlayerUI.MatchViewModel = NewObject<UBRVM_Match>(this);
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
		UE_LOG(LogBRUI, Error,
			TEXT("BRUISettings.RootLayoutClass is unset. Set it in Config/DefaultGame.ini under ")
			TEXT("[/Script/Breachpoint.BRUISettings] to a WBP deriving from UBRRootLayout. ")
			TEXT("No HUD will appear until then."));
		return nullptr;
	}

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
		UE_LOG(LogBRUI, Error, TEXT("PushWidgetToLayer(%s): the configured widget class is unset."),
			*LayerTag.ToString());
		return nullptr;
	}

	const bool bWasResident = WidgetClass.Get() != nullptr;
	UClass* ResolvedClass = WidgetClass.LoadSynchronous();
	if (!bWasResident)
	{
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
