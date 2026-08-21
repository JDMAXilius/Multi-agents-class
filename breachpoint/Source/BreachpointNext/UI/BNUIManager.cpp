#include "UI/BNUIManager.h"
#include "BreachpointNext.h"
#include "Blueprint/UserWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/BNActivatableWidget.h"
#include "UI/BNRootLayout.h"
#include "UI/BNViewModels.h"

void UBNUIManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	PlayerAddedHandle = GameInstance->OnLocalPlayerAddedEvent.AddUObject(this, &UBNUIManager::HandleLocalPlayerAdded);
	PlayerRemovedHandle = GameInstance->OnLocalPlayerRemovedEvent.AddUObject(this, &UBNUIManager::HandleLocalPlayerRemoved);

	// Players that beat the subsystem to existence — the seed, same as the compiled reference.
	for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
	{
		HandleLocalPlayerAdded(LocalPlayer);
	}

	PreloadMidMatchClasses();
}

void UBNUIManager::Deinitialize()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->OnLocalPlayerAddedEvent.Remove(PlayerAddedHandle);
		GameInstance->OnLocalPlayerRemovedEvent.Remove(PlayerRemovedHandle);
	}

	if (MidMatchPreloadHandle.IsValid())
	{
		MidMatchPreloadHandle->ReleaseHandle();
		MidMatchPreloadHandle.Reset();
	}

	for (TPair<TObjectPtr<ULocalPlayer>, FBNLocalPlayerUI>& Pair : PerPlayerUI)
	{
		if (Pair.Value.RootLayout)
		{
			Pair.Value.RootLayout->RemoveFromParent();
		}
	}
	PerPlayerUI.Empty();

	Super::Deinitialize();
}

UBNUIManager* UBNUIManager::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UBNUIManager>() : nullptr;
}

void UBNUIManager::HandleLocalPlayerAdded(ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer || PerPlayerUI.Contains(LocalPlayer))
	{
		return;
	}

	// The BOOK entry exists from the player's arrival; the ViewModels with it, so the director
	// can feed them before any widget exists to read them. The LAYOUT is deliberately lazy —
	// EnsureLocalPlayerUI builds it when the director first has something to show, because at
	// this moment the player has no PlayerController for CreateWidget to own.
	FBNLocalPlayerUI& UI = PerPlayerUI.Add(LocalPlayer);
	UI.CombatViewModel = NewObject<UBNVM_Combat>(this);
	UI.MatchViewModel = NewObject<UBNVM_Match>(this);
}

void UBNUIManager::HandleLocalPlayerRemoved(ULocalPlayer* LocalPlayer)
{
	FBNLocalPlayerUI Removed;
	if (PerPlayerUI.RemoveAndCopyValue(LocalPlayer, Removed) && Removed.RootLayout)
	{
		Removed.RootLayout->RemoveFromParent();
	}
}

bool UBNUIManager::EnsureLocalPlayerUI(ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer)
	{
		return false;
	}

	// Late add (seamless travel orderings) — the book entry is idempotent.
	if (!PerPlayerUI.Contains(LocalPlayer))
	{
		HandleLocalPlayerAdded(LocalPlayer);
	}

	FBNLocalPlayerUI& UI = PerPlayerUI.FindChecked(LocalPlayer);
	if (UI.RootLayout)
	{
		return true;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	APlayerController* OwningPC = World ? LocalPlayer->GetPlayerController(World) : nullptr;
	if (!OwningPC)
	{
		// Not an error — the caller retries on the next possession edge.
		return false;
	}

	UClass* LayoutClass = RootLayoutClass.LoadSynchronous();
	if (!LayoutClass)
	{
		UE_LOG(LogBN, Warning, TEXT("BNUI: RootLayoutClass '%s' did not resolve — no UI this session. The ini names the class; TASK-R7-WBP-HUD builds the asset."),
			*RootLayoutClass.ToString());
		return false;
	}

	UBNRootLayout* Layout = CreateWidget<UBNRootLayout>(OwningPC, LayoutClass);
	if (!Layout)
	{
		UE_LOG(LogBN, Error, TEXT("BNUI: CreateWidget failed for %s."), *GetNameSafe(LayoutClass));
		return false;
	}

	Layout->AddToPlayerScreen(0);
	UI.RootLayout = Layout;
	UE_LOG(LogBN, Log, TEXT("BNUI: root layout up for %s."), *GetNameSafe(LocalPlayer));
	return true;
}

UBNVM_Combat* UBNUIManager::GetCombatViewModel(ULocalPlayer* LocalPlayer) const
{
	const FBNLocalPlayerUI* UI = LocalPlayer ? PerPlayerUI.Find(LocalPlayer) : nullptr;
	return UI ? UI->CombatViewModel : nullptr;
}

UBNVM_Match* UBNUIManager::GetMatchViewModel(ULocalPlayer* LocalPlayer) const
{
	const FBNLocalPlayerUI* UI = LocalPlayer ? PerPlayerUI.Find(LocalPlayer) : nullptr;
	return UI ? UI->MatchViewModel : nullptr;
}

UBNRootLayout* UBNUIManager::GetRootLayout(ULocalPlayer* LocalPlayer) const
{
	const FBNLocalPlayerUI* UI = LocalPlayer ? PerPlayerUI.Find(LocalPlayer) : nullptr;
	return UI ? UI->RootLayout : nullptr;
}

UBNActivatableWidget* UBNUIManager::PushWidgetToLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag, const TSoftClassPtr<UBNActivatableWidget>& WidgetClass)
{
	if (WidgetClass.IsNull())
	{
		// A null soft class fails SILENTLY on its own (the old module's D1) — this line is the
		// difference between "the death screen is an unbuilt asset" and a mystery.
		UE_LOG(LogBN, Warning, TEXT("BNUI: push to %s refused — the widget class is unset in [/Script/BreachpointNext.BNUIManager]."),
			*LayerTag.ToString());
		return nullptr;
	}

	if (!EnsureLocalPlayerUI(LocalPlayer))
	{
		return nullptr;
	}

	UClass* ResolvedClass = WidgetClass.LoadSynchronous();
	if (!ResolvedClass)
	{
		UE_LOG(LogBN, Warning, TEXT("BNUI: widget class '%s' did not resolve — asset missing at that path."),
			*WidgetClass.ToString());
		return nullptr;
	}

	UBNRootLayout* Layout = GetRootLayout(LocalPlayer);
	return Layout ? Layout->PushWidgetToLayer(LayerTag, ResolvedClass) : nullptr;
}

void UBNUIManager::RemoveWidgetFromLayer(ULocalPlayer* LocalPlayer, FUITag LayerTag, UBNActivatableWidget* Widget)
{
	if (UBNRootLayout* Layout = GetRootLayout(LocalPlayer))
	{
		Layout->RemoveWidgetFromLayer(LayerTag, Widget);
	}
}

void UBNUIManager::PreloadMidMatchClasses()
{
	TArray<FSoftObjectPath> MidMatchClasses;
	for (const TSoftClassPtr<UBNActivatableWidget>& SoftClass : { DeathScreenClass, ScoreboardClass })
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
