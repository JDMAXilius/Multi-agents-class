#include "Core/OSGameInstance.h"
#include "Data/OSSessionsData.h"
#include "Subsystems/OSSessionsSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "OnlineSubsystem.h"
#include "AbilitySystemGlobals.h"
#include "GameMapsSettings.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#endif

UOSGameInstance::UOSGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UOSGameInstance::LeaveServer()
{
	UOSSessionsSubsystem* SessionsSubsystem = GetSubsystem<UOSSessionsSubsystem>();
	if (!SessionsSubsystem)
	{
		TravelToMainMenu();
		return;
	}

	// If there is no active session, just travel — nothing to clean up.
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	IOnlineSessionPtr SessionInterface = OSS ? OSS->GetSessionInterface() : nullptr;
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(OnSightGameSession))
	{
		TravelToMainMenu();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[OSGameInstance] LeaveServer — destroying session"));

	// Guard against stacking if network failure fires repeatedly
	SessionsSubsystem->OnDestroySessionComplete.RemoveAll(this);
	SessionsSubsystem->OnDestroySessionComplete.AddDynamic(this, &UOSGameInstance::OnLeaveServerDestroyComplete);
	SessionsSubsystem->DestroySession();
	// OSS always fires the callback (success or failure) — no timer needed.
}

void UOSGameInstance::OnLeaveServerDestroyComplete(bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("[OSGameInstance] LeaveServer destroy complete (%s) — ClientTravel to main menu"),
		bWasSuccessful ? TEXT("✓") : TEXT("✗"));

	if (UOSSessionsSubsystem* SessionsSubsystem = GetSubsystem<UOSSessionsSubsystem>())
	{
		SessionsSubsystem->OnDestroySessionComplete.RemoveAll(this);
	}
	TravelToMainMenu();
}

void UOSGameInstance::EndMatchAndReturnToMenu()
{
	// Prevent re-entrant calls (Blueprint timers, duplicate events, etc.)
	// that would spam ServerTravel after the session is already destroyed.
	if (bIsReturningToMenu)
	{
		return;
	}

	// Only the server drives match end — ClientTravel on a client does nothing for other players.
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld || CurrentWorld->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OSGameInstance] EndMatchAndReturnToMenu called on non-server — ignored"));
		return;
	}

	bIsReturningToMenu = true;

	UOSSessionsSubsystem* SessionsSubsystem = GetSubsystem<UOSSessionsSubsystem>();
	if (!SessionsSubsystem)
	{
		ServerTravelToMainMenu();
		return;
	}

	// If there is no active session, skip straight to travel.
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	IOnlineSessionPtr SessionInterface = OSS ? OSS->GetSessionInterface() : nullptr;
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(OnSightGameSession))
	{
		ServerTravelToMainMenu();
		return;
	}

	// Clean up any stale bindings from previous calls
	SessionsSubsystem->OnEndSessionComplete.RemoveAll(this);
	SessionsSubsystem->OnDestroySessionComplete.RemoveAll(this);

	SessionsSubsystem->OnEndSessionComplete.AddDynamic(this, &UOSGameInstance::OnEndMatchEndSessionComplete);
	SessionsSubsystem->EndSession();
	// OSS always fires the callback (success or failure) — no timer needed.

	UE_LOG(LogTemp, Log, TEXT("[OSGameInstance] EndMatchAndReturnToMenu: EndSession initiated"));
}

void UOSGameInstance::OnEndMatchEndSessionComplete(bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("[OSGameInstance] EndSession complete (%s) — destroying session"),
		bWasSuccessful ? TEXT("✓") : TEXT("✗"));

	UOSSessionsSubsystem* SessionsSubsystem = GetSubsystem<UOSSessionsSubsystem>();
	if (!SessionsSubsystem)
	{
		ServerTravelToMainMenu();
		return;
	}

	SessionsSubsystem->OnEndSessionComplete.RemoveAll(this);
	SessionsSubsystem->OnDestroySessionComplete.AddDynamic(this, &UOSGameInstance::OnEndMatchDestroyComplete);
	SessionsSubsystem->DestroySession();
}

void UOSGameInstance::OnEndMatchDestroyComplete(bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("[OSGameInstance] Session destroyed (%s) — ServerTravel to main menu"),
		bWasSuccessful ? TEXT("✓") : TEXT("✗"));

	if (UOSSessionsSubsystem* SessionsSubsystem = GetSubsystem<UOSSessionsSubsystem>())
	{
		SessionsSubsystem->OnDestroySessionComplete.RemoveAll(this);
	}
	ServerTravelToMainMenu();
}

void UOSGameInstance::Init()
{
	Super::Init();

	// Ensure GAS global data is initialized early (loads GameplayCueNotifyPaths, cue manager, etc.).
	UAbilitySystemGlobals::Get().InitGlobalData();

#if WITH_EDITOR
	// Keep a short warning in case duplicate cue tags are reintroduced.
	// Duplicate tags cause: one cue "wins" and the others are skipped.
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;
		Filter.PackagePaths.Add(FName(TEXT("/Game/GAS/Cues")));
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);

		TMap<FString, TArray<FString>> TagToAssets;
		for (const FAssetData& Asset : Assets)
		{
			FString CueTagStr;
			// Many GameplayCue blueprints expose GameplayCueTag in the asset registry.
			if (Asset.GetTagValue(FName(TEXT("GameplayCueTag")), CueTagStr) && !CueTagStr.IsEmpty())
			{
				TagToAssets.FindOrAdd(CueTagStr).Add(Asset.GetSoftObjectPath().ToString());
			}
		}

		for (const auto& It : TagToAssets)
		{
			const FString& CueTagStr = It.Key;
			const TArray<FString>& Owners = It.Value;
			if (Owners.Num() > 1)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[GASCues] Duplicate GameplayCueTag '%s' detected (%d assets). One will win; others will be skipped."),
					*CueTagStr, Owners.Num());
			}
		}
	}
#endif

	// Ensure Online Subsystem is initialized (NULL if Steam not available)
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!Subsystem)
	{
		Subsystem = IOnlineSubsystem::Get(NAME_None);
		UE_LOG(LogTemp, Warning, TEXT("[OSGameInstance] Steam not available, using NULL subsystem"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[OSGameInstance] Online Subsystem: %s"), *Subsystem->GetSubsystemName().ToString());
	}

	GEngine->OnNetworkFailure().AddUObject(this, &UOSGameInstance::OnNetworkFailure);
	GEngine->OnTravelFailure().AddUObject(this, &UOSGameInstance::OnTravelFailure);

	// Reset bIsReturningToMenu when a new world initializes (after ServerTravel completes).
	// GameInstance persists across maps, so the flag must be cleared for the next match.
	FWorldDelegates::OnPostWorldInitialization.AddWeakLambda(this,
		[this](UWorld*, const UWorld::InitializationValues&)
		{
			bIsReturningToMenu = false;
		});

	UE_LOG(LogTemp, Log, TEXT("[OSGameInstance] Initializing OnSight Game Instance"));
}

void UOSGameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().RemoveAll(this);
		GEngine->OnTravelFailure().RemoveAll(this);
	}

	Super::Shutdown();

	UE_LOG(LogTemp, Log, TEXT("[OSGameInstance] Shutting down OnSight Game Instance"));
}

void UOSGameInstance::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// GEngine->OnNetworkFailure() is a global broadcast — in PIE every game instance receives
	// every world's failures. Only handle failures that belong to our own world.
	if (World != GetWorld())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[OSGameInstance] Network failure (%s): %s"),
		*UEnum::GetValueAsString(FailureType), *ErrorString);

	// After ServerTravel to main menu, clients get a ConnectionLost when the server stops
	// accepting connections. Ignore it — they've already arrived home.
	const UGameMapsSettings* MapsSettings = GetDefault<UGameMapsSettings>();
	if (MapsSettings && World)
	{
		const FString MainMenuMap = FPaths::GetBaseFilename(MapsSettings->GetGameDefaultMap());
		const FString CurrentMap  = FPaths::GetBaseFilename(World->GetMapName());
		if (CurrentMap.Equals(MainMenuMap, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Log, TEXT("[OSGameInstance] Network failure at main menu — already home, ignoring"));
			return;
		}
	}

	LeaveServer();
}

void UOSGameInstance::OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	// Same global-broadcast caveat as OnNetworkFailure — only handle our own world.
	if (World != GetWorld())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[OSGameInstance] Travel failure (%s): %s"),
		*UEnum::GetValueAsString(FailureType), *ErrorString);
	LeaveServer();
}

void UOSGameInstance::TravelToMainMenu() const
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	const UGameMapsSettings* MapsSettings = GetDefault<UGameMapsSettings>();
	if (!MapsSettings) return;

	const FString MainMenuMap = MapsSettings->GetGameDefaultMap();
	PC->ClientTravel(MainMenuMap, TRAVEL_Absolute);
}

void UOSGameInstance::ServerTravelToMainMenu() const
{
	UWorld* World = GetWorld();
	if (!World) return;

	const UGameMapsSettings* MapsSettings = GetDefault<UGameMapsSettings>();
	if (!MapsSettings) return;

	const FString MainMenuMap = MapsSettings->GetGameDefaultMap();
	UE_LOG(LogTemp, Log, TEXT("[OSGameInstance] ServerTravel all clients to main menu: %s"), *MainMenuMap);

	// ServerTravel (no ?listen) is the correct call here:
	// it flushes all pending RPCs to clients before the server transitions,
	// ensuring they receive the travel command before the NetDriver shuts down.
	// The ConnectionLost clients receive on arrival is caught in OnNetworkFailure
	// and ignored when they are already at the main menu.
	World->ServerTravel(MainMenuMap, true);
}
