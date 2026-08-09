#include "Subsystems/OSSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Data/OSSessionsData.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "GameFramework/PlayerController.h"
#include "OSLogCategories.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "UnrealEd.h"
#endif

namespace OSSessionKeys
{
	static const FName OnSightGame(TEXT("OnSightGame"));
	// Steam lobby search rejects bool filters ("Unable to set search parameter ... Value=true : Equals : -1").
	// Use int32 on both advertise and query so discovery works with OnlineSubsystemSteam.
	static constexpr int32 OnSightGameId = 1;
	static const FName OnSightMatchType(TEXT("OnSightMatchType"));
	static const FName OnSightSessionName(TEXT("OnSightSessionName"));
	static const FName OnSightMapPath(TEXT("OnSightMapPath"));
	static const FName OnSightGameMode(TEXT("OnSightGameMode"));
	static const FName OnSightPlayerCount(TEXT("OnSightPlayerCount"));

	// Legacy keys kept for compatibility / debugging
	static const FName MatchType(TEXT("MatchType"));
	static const FName SessionName(TEXT("SessionName"));
}

UOSSessionsSubsystem::UOSSessionsSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionCompleteInternal)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsCompleteInternal)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionCompleteInternal)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionCompleteInternal)),
	EndSessionCompleteDelegate(FOnEndSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnEndSessionCompleteInternal)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionCompleteInternal))
{
}

void UOSSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Attempt early interface caching; lazy init in IsValidSessionInterface() serves as fallback
	IsValidSessionInterface();
}

void UOSSessionsSubsystem::Deinitialize()
{
	Super::Deinitialize();

	bJoinSessionInProgress = false;
	bHostFlowInProgress = false;
	bJoinFlowInProgress = false;
	PendingHostTravelURL.Empty();

	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	SessionInterface.Reset();
}

// --- High-level flows ---

void UOSSessionsSubsystem::HostAndTravel(const FString& TravelURL, const FOSHostMatchSettings& MatchSettings, const FString& SessionName, bool bIsLANMatch)
{
	if (bHostFlowInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Sessions] HostAndTravel ignored: already in progress"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Sessions] HostAndTravel — URL: %s"), *TravelURL);
	bHostFlowInProgress = true;
	PendingHostTravelURL = TravelURL;
	CreateSession(MatchSettings, SessionName, bIsLANMatch);
}

void UOSSessionsSubsystem::JoinAndTravel(const FOnlineSessionSearchResult& SessionResult)
{
	if (bJoinFlowInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Sessions] JoinAndTravel ignored: already in progress"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Sessions] JoinAndTravel"));
	bJoinFlowInProgress = true;
	JoinSession(SessionResult);
}

void UOSSessionsSubsystem::ExecuteHostTravel()
{
	UWorld* World = GetWorld();
	if (!World || PendingHostTravelURL.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[Sessions] ExecuteHostTravel failed: no world or empty URL"));
		bHostFlowInProgress = false;
		OnSessionFlowError.Broadcast(FText::FromString(TEXT("Failed to travel: invalid map path")));
		return;
	}

	FString TravelURL = PendingHostTravelURL;

	/* SteamSockets workaround: if already a listen server AND using Steam, traveling with ?listen
	   causes SteamSockets to create a second P2P listen socket on vport 17777 and fail. */
	const bool bAlreadyListenServer = (World->GetNetMode() == NM_ListenServer);
	const IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	const bool bUsesSteamSockets = OSS && OSS->GetSubsystemName() == FName("Steam");

	if (bAlreadyListenServer && bUsesSteamSockets)
		TravelURL.ReplaceInline(TEXT("?listen"), TEXT(""), ESearchCase::IgnoreCase);
	else if (!TravelURL.Contains(TEXT("listen"), ESearchCase::IgnoreCase))
		TravelURL.Append(TEXT("?listen"));

	UE_LOG(LogTemp, Log, TEXT("[Sessions] ExecuteHostTravel — ServerTravel: %s"), *TravelURL);

	bHostFlowInProgress = false;
	PendingHostTravelURL.Empty();
	OnTravelStarted.Broadcast();
	World->ServerTravel(TravelURL, true);
}

void UOSSessionsSubsystem::ExecuteJoinTravel(const FString& Address)
{
	UWorld* World = GetWorld();
	if (!World || Address.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[Sessions] ExecuteJoinTravel failed: no world or empty address"));
		bJoinFlowInProgress = false;
		OnSessionFlowError.Broadcast(FText::FromString(TEXT("Failed to join: invalid server address")));
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("[Sessions] ExecuteJoinTravel failed: no PlayerController"));
		bJoinFlowInProgress = false;
		OnSessionFlowError.Broadcast(FText::FromString(TEXT("Failed to join: no player controller")));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Sessions] ExecuteJoinTravel — ClientTravel: %s"), *Address);

	bJoinFlowInProgress = false;
	OnTravelStarted.Broadcast();
	PC->ClientTravel(Address, TRAVEL_Absolute);
}

// --- Low-level session operations ---

void UOSSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType, const FString& SessionName, bool bIsLANMatch)
{
	if (!IsValidSessionInterface())
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	// Guard against re-entry while a destroy-and-recreate is already in progress
	if (bCreateSessionOnDestroy)
		return;

	auto ExistingSession = SessionInterface->GetNamedSession(OnSightGameSession);
	if (ExistingSession != nullptr)
	{
		// Save session parameters for potential re-creation
		LastNumPublicConnections = NumPublicConnections;
		LastMatchType = MatchType;
		LastSessionName = SessionName;
		bLastSessionIsLAN = bIsLANMatch;
		bCreateSessionOnDestroy = true;
		DestroySession();
		return; // Return early - session will be recreated after destroy completes
	}

	UE_LOG(LogTemp, Log, TEXT("[Sessions] CreateSession — Type:\"%s\" Players:%d LAN:%s"),
		*MatchType, NumPublicConnections, bIsLANMatch ? TEXT("true") : TEXT("false"));

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	LastSessionSettings->bIsLANMatch = bIsLANMatch;

	// OnSight-namespaced keys so we can filter out foreign Spacewar lobbies.
	//LastSessionSettings->Set(OSSessionKeys::OnSightGame, true, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(OSSessionKeys::OnSightGame, OSSessionKeys::OnSightGameId, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(OSSessionKeys::OnSightMatchType, MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(OSSessionKeys::OnSightSessionName, SessionName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	// Keep legacy keys for compatibility/debugging
	LastSessionSettings->Set(OSSessionKeys::MatchType, MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(OSSessionKeys::SessionName, SessionName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->BuildUniqueId = 1;

	UWorld* World = GetWorld();
	const ULocalPlayer* LocalPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	if (!World || !LocalPlayer || !SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), OnSightGameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		OnCreateSessionComplete.Broadcast(false);
	}
}

void UOSSessionsSubsystem::CreateSession(const FOSHostMatchSettings& MatchSettings, const FString& SessionName, bool bIsLANMatch)
{
	if (!IsValidSessionInterface())
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	if (bCreateSessionOnDestroy)
		return;

	auto ExistingSession = SessionInterface->GetNamedSession(OnSightGameSession);
	if (ExistingSession != nullptr)
	{
		LastNumPublicConnections = MatchSettings.GetPlayerCount();
		LastMatchType = MatchSettings.GetMatchTypeString();
		LastSessionName = SessionName;
		bLastSessionIsLAN = bIsLANMatch;
		bCreateSessionOnDestroy = true;
		DestroySession();
		return;
	}

	const FString MatchType = MatchSettings.GetMatchTypeString();

	UE_LOG(HookMapSelect, Log, TEXT("[Sessions] CreateSession — Mode:%s Map:%s Players:%d LAN:%s"),
		*MatchType, *MatchSettings.MapPath, MatchSettings.GetPlayerCount(), bIsLANMatch ? TEXT("true") : TEXT("false"));

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->NumPublicConnections = MatchSettings.GetPlayerCount();
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	LastSessionSettings->bIsLANMatch = bIsLANMatch;

	// OnSight-namespaced keys
	// LastSessionSettings->Set(OSSessionKeys::OnSightGame, true, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(OSSessionKeys::OnSightGame, OSSessionKeys::OnSightGameId, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(OSSessionKeys::OnSightMatchType, MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(OSSessionKeys::OnSightSessionName, SessionName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(OSSessionKeys::OnSightMapPath, MatchSettings.MapPath, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(OSSessionKeys::OnSightGameMode, static_cast<int32>(MatchSettings.GameMode), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(OSSessionKeys::OnSightPlayerCount, MatchSettings.GetPlayerCount(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	// Legacy keys
	LastSessionSettings->Set(OSSessionKeys::MatchType, MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(OSSessionKeys::SessionName, SessionName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->BuildUniqueId = 1;

	UWorld* World = GetWorld();
	const ULocalPlayer* LocalPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	if (!World || !LocalPlayer || !SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), OnSightGameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		OnCreateSessionComplete.Broadcast(false);
	}
}

void UOSSessionsSubsystem::FindSessions(int32 MaxSearchResults, bool bIsLANMatch, const FString& MatchTypeFilter, const FString& SessionNameFilter)
{
	UE_LOG(LogTemp, Log, TEXT("[Sessions] FindSessions — max:%d LAN:%s"), MaxSearchResults, bIsLANMatch ? TEXT("true") : TEXT("false"));

	// Each user-initiated call is a fresh attempt — cancel any in-flight retry
	// timer so a mid-retry re-press doesn't inherit a stale retry count.
	FindSessionsRetryCount = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
	}

	// Store parameters so RetryFindSessions can replay them
	PendingMaxSearchResults = MaxSearchResults;
	PendingbIsLANMatch = bIsLANMatch;
	PendingMatchTypeFilter = MatchTypeFilter;
	PendingSessionNameFilter = SessionNameFilter;

	FindSessionsInternal(MaxSearchResults, bIsLANMatch, MatchTypeFilter, SessionNameFilter);
}

void UOSSessionsSubsystem::FindSessionsInternal(int32 MaxSearchResults, bool bIsLANMatch, const FString& MatchTypeFilter, const FString& SessionNameFilter)
{
	if (!IsValidSessionInterface())
	{
		if (FindSessionsRetryCount < MaxFindSessionsRetries)
		{
			FindSessionsRetryCount++;

			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					FindSessionsRetryTimerHandle,
					this,
					&UOSSessionsSubsystem::RetryFindSessions,
					3.0f, // 3 second delay
					false
				);
			}
		}
		else
		{
			FindSessionsRetryCount = 0;
			OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		}
		return;
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->bIsLanQuery = bIsLANMatch;
	LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	// Critical for AppId 480 (Spacewar): filter server-side so we don't get spammed by other titles' lobbies.
	//LastSessionSearch->QuerySettings.Set(OSSessionKeys::OnSightGame, true, EOnlineComparisonOp::Equals);
	LastSessionSearch->QuerySettings.Set(OSSessionKeys::OnSightGame, OSSessionKeys::OnSightGameId, EOnlineComparisonOp::Equals);
	if (!MatchTypeFilter.IsEmpty())
	{
		LastSessionSearch->QuerySettings.Set(OSSessionKeys::OnSightMatchType, MatchTypeFilter, EOnlineComparisonOp::Equals);
	}
	if (!SessionNameFilter.IsEmpty())
	{
		LastSessionSearch->QuerySettings.Set(OSSessionKeys::OnSightSessionName, SessionNameFilter, EOnlineComparisonOp::Equals);
	}

	UWorld* World = GetWorld();
	const ULocalPlayer* LocalPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	if (!World || !LocalPlayer || !SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);

		if (FindSessionsRetryCount < MaxFindSessionsRetries && World)
		{
			FindSessionsRetryCount++;
			World->GetTimerManager().SetTimer(
				FindSessionsRetryTimerHandle,
				this,
				&UOSSessionsSubsystem::RetryFindSessions,
				3.0f,
				false
			);
		}
		else
		{
			FindSessionsRetryCount = 0;
			OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		}
	}
}

void UOSSessionsSubsystem::RetryFindSessions()
{
	UE_LOG(LogTemp, Log, TEXT("[OSSessionsSubsystem] Retrying FindSessions (Attempt %d/%d)"),
		FindSessionsRetryCount, MaxFindSessionsRetries);

	// Call internal directly — FindSessions would reset FindSessionsRetryCount to 0
	FindSessionsInternal(PendingMaxSearchResults, PendingbIsLANMatch, PendingMatchTypeFilter, PendingSessionNameFilter);
}

void UOSSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if (!IsValidSessionInterface())
	{
		OnJoinSessionComplete.Broadcast(false, OSJoinSessionResultType::UnknownError, TEXT(""));
		return;
	}

	// Destroy stale local session before joining a new one
	auto ExistingSession = SessionInterface->GetNamedSession(OnSightGameSession);
	if (ExistingSession != nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[Sessions] JoinSession — stale local session exists, destroying first"));
		PendingJoinSearchResult = SessionResult;
		bJoinSessionOnDestroy = true;
		DestroySession();
		return;
	}

	if (bJoinSessionInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Sessions] JoinSession ignored — already joining (duplicate UI bind or double click)"));
		return;
	}

	bJoinSessionInProgress = true;

	UE_LOG(LogTemp, Log, TEXT("[Sessions] JoinSession"));
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	UWorld* World = GetWorld();
	const ULocalPlayer* LocalPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	if (!World || !LocalPlayer || !SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), OnSightGameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		bJoinSessionInProgress = false;
		OnJoinSessionComplete.Broadcast(false, OSJoinSessionResultType::UnknownError, TEXT(""));
	}
}

void UOSSessionsSubsystem::DestroySession()
{
	if (!IsValidSessionInterface())
	{
		OnDestroySessionComplete.Broadcast(false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Sessions] DestroySession"));
	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(OnSightGameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		OnDestroySessionComplete.Broadcast(false);
	}
}

void UOSSessionsSubsystem::StartSession()
{
	if (!IsValidSessionInterface())
	{
		OnStartSessionComplete.Broadcast(false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Sessions] StartSession"));
	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegate);

	if (!SessionInterface->StartSession(OnSightGameSession))
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		OnStartSessionComplete.Broadcast(false);
	}
}

void UOSSessionsSubsystem::OnStartSessionCompleteInternal(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("[Sessions] StartSession complete — %s"), bWasSuccessful ? TEXT("✓ session active, players can join") : TEXT("✗ failed"));

	if (bHostFlowInProgress)
	{
		if (bWasSuccessful)
			ExecuteHostTravel();
		else
		{
			bHostFlowInProgress = false;
			PendingHostTravelURL.Empty();
			OnSessionFlowError.Broadcast(FText::FromString(TEXT("Failed to start session: please try again")));
		}
	}

	OnStartSessionComplete.Broadcast(bWasSuccessful);
}

void UOSSessionsSubsystem::EndSession()
{
	if (!IsValidSessionInterface())
	{
		OnEndSessionComplete.Broadcast(false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Sessions] EndSession"));
	EndSessionCompleteDelegateHandle = SessionInterface->AddOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegate);

	if (!SessionInterface->EndSession(OnSightGameSession))
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		OnEndSessionComplete.Broadcast(false);
	}
}

void UOSSessionsSubsystem::OnEndSessionCompleteInternal(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("[Sessions] EndSession complete — %s"), bWasSuccessful ? TEXT("✓") : TEXT("✗"));
	OnEndSessionComplete.Broadcast(bWasSuccessful);
}

bool UOSSessionsSubsystem::IsValidSessionInterface()
{
	if (!SessionInterface)
	{
		IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
		if (Subsystem)
		{
			SessionInterface = Subsystem->GetSessionInterface();
		}
	}
	return SessionInterface.IsValid();
}

void UOSSessionsSubsystem::OnCreateSessionCompleteInternal(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	// Steam/NULL can report failure even though the session slot was created — treat it as success in that case.
	if (!bWasSuccessful && SessionInterface.IsValid())
	{
		if (SessionInterface->GetNamedSession(OnSightGameSession) != nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OSSessionsSubsystem] CreateSession reported failure but session exists — treating as success"));
			bWasSuccessful = true;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Sessions] CreateSession complete — %s"),
		bWasSuccessful ? TEXT("✓ created, chaining StartSession") : TEXT("✗ failed"));

	// Chain StartSession on success; StartSession completion handles travel if HostAndTravel is active.
	if (bWasSuccessful)
		StartSession();
	else if (bHostFlowInProgress)
	{
		bHostFlowInProgress = false;
		PendingHostTravelURL.Empty();
		OnSessionFlowError.Broadcast(FText::FromString(
			IsSteamConnected()
				? TEXT("Failed to create session: please try again")
				: TEXT("Steam not connected: please ensure Steam is running")));
	}

	OnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UOSSessionsSubsystem::OnFindSessionsCompleteInternal(bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}

	if (!bWasSuccessful || !LastSessionSearch.IsValid() || LastSessionSearch->SearchResults.Num() <= 0)
	{
		// If no results and we haven't exceeded retries, try again
		if (FindSessionsRetryCount < MaxFindSessionsRetries && bWasSuccessful)
		{
			FindSessionsRetryCount++;
			UE_LOG(LogTemp, Log, TEXT("[OSSessionsSubsystem] No sessions found, retrying (Attempt %d/%d)"), 
				FindSessionsRetryCount, MaxFindSessionsRetries);
			
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					FindSessionsRetryTimerHandle,
					this,
					&UOSSessionsSubsystem::RetryFindSessions,
					3.0f,
					false
				);
			}
			return;
		}
		
		// Reset retry count on final failure
		FindSessionsRetryCount = 0;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
		}
		
		OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	// Reset retry count on success
	FindSessionsRetryCount = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
	}

	OnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
}

void UOSSessionsSubsystem::OnJoinSessionCompleteInternal(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}

	bJoinSessionInProgress = false;

	// Map engine enum to Blueprint-friendly enum
	OSJoinSessionResultType ResultBP = OSJoinSessionResultType::UnknownError;
	switch (Result)
	{
	case EOnJoinSessionCompleteResult::Success:
		ResultBP = OSJoinSessionResultType::Success;
		break;
	case EOnJoinSessionCompleteResult::SessionIsFull:
		ResultBP = OSJoinSessionResultType::SessionIsFull;
		break;
	case EOnJoinSessionCompleteResult::SessionDoesNotExist:
		ResultBP = OSJoinSessionResultType::SessionDoesNotExist;
		break;
	case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
		ResultBP = OSJoinSessionResultType::CouldNotRetrieveAddress;
		break;
	case EOnJoinSessionCompleteResult::AlreadyInSession:
		ResultBP = OSJoinSessionResultType::AlreadyInSession;
		break;
	case EOnJoinSessionCompleteResult::UnknownError:
	default:
		ResultBP = OSJoinSessionResultType::UnknownError;
		break;
	}

	// Get the session address to make a client travel
	FString Address;
	SessionInterface->GetResolvedConnectString(OnSightGameSession, Address);

	UE_LOG(LogTemp, Log, TEXT("[Sessions] JoinSession complete — %s Address:%s"),
		Result == EOnJoinSessionCompleteResult::Success ? TEXT("✓") : TEXT("✗"),
		*Address);

	if (bJoinFlowInProgress)
	{
		if (Result == EOnJoinSessionCompleteResult::Success && !Address.IsEmpty())
			ExecuteJoinTravel(Address);
		else
		{
			bJoinFlowInProgress = false;
			OnSessionFlowError.Broadcast(FText::FromString(
				FString::Printf(TEXT("Failed to join session (%d): try again"), static_cast<int32>(Result))));
		}
	}

	// Broadcast to Blueprint with Blueprint-friendly enum
	this->OnJoinSessionComplete.Broadcast(
		Result == EOnJoinSessionCompleteResult::Success,
		ResultBP,
		Address
	);
}

void UOSSessionsSubsystem::OnDestroySessionCompleteInternal(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("[Sessions] DestroySession complete — %s%s"),
		bWasSuccessful ? TEXT("✓") : TEXT("✗"),
		(bWasSuccessful && bCreateSessionOnDestroy) ? TEXT(" — recreating session") : TEXT(""));

	// Always reset flags — if destroy failed we cannot safely proceed, so drop pending requests.
	const bool bShouldRecreate = bWasSuccessful && bCreateSessionOnDestroy;
	const bool bShouldJoin = bWasSuccessful && bJoinSessionOnDestroy;
	bCreateSessionOnDestroy = false;
	bJoinSessionOnDestroy = false;

	if (bShouldRecreate)
	{
		CreateSession(LastNumPublicConnections, LastMatchType, LastSessionName, bLastSessionIsLAN);
	}
	else if (bShouldJoin)
	{
		UE_LOG(LogTemp, Log, TEXT("[Sessions] Stale session destroyed — now joining new session"));
		JoinSession(PendingJoinSearchResult);
	}
	OnDestroySessionComplete.Broadcast(bWasSuccessful);
}

bool UOSSessionsSubsystem::ShouldUseLANMode() const
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	FName SubsystemName = Subsystem ? Subsystem->GetSubsystemName() : NAME_None;
	
	// No subsystem or NULL subsystem = offline = use LAN
	if (!Subsystem || SubsystemName == "NULL")
	{
		return true;
	}
	
	// Steam subsystem = check environment and command line
	if (SubsystemName == "Steam")
	{
		// Command line overrides (highest priority)
		if (FParse::Param(FCommandLine::Get(), TEXT("lan"))) return true;
		if (FParse::Param(FCommandLine::Get(), TEXT("steam"))) return false;
		
		// PIE = same computer = use LAN
		#if WITH_EDITOR
		UWorld* World = GetWorld();
		if (GIsEditor && World && World->IsPlayInEditor()) return true;
		#endif
		
		// Default: Steam active = use P2P (not LAN)
		return false;
	}
	
	// Other subsystems (EOS, etc.) = use P2P (not LAN)
	return false;
}

bool UOSSessionsSubsystem::IsSteamConnected() const
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	FName SubsystemName = Subsystem ? Subsystem->GetSubsystemName() : NAME_None;
	IOnlineIdentityPtr IdentityInterface = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	UWorld* World = GetWorld();
	const ULocalPlayer* LocalPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	FUniqueNetIdRepl UniqueNetIdRepl = LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();

	if (!Subsystem || SubsystemName == "NULL" || !IdentityInterface.IsValid() || !LocalPlayer || !UniqueNetIdRepl.IsValid())
	{
		return false;
	}

	const FUniqueNetId& UniqueNetIdRef = *UniqueNetIdRepl;
	ELoginStatus::Type LoginStatus = IdentityInterface->GetLoginStatus(UniqueNetIdRef);
	return LoginStatus == ELoginStatus::LoggedIn;
}

FString UOSSessionsSubsystem::GetSteamPlayerName() const
{
	UWorld* World = GetWorld();
	const ULocalPlayer* LocalPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	IOnlineIdentityPtr IdentityInterface = Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	FUniqueNetIdRepl UniqueNetIdRepl = LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();

	if (!LocalPlayer || !Subsystem || !IdentityInterface.IsValid() || !UniqueNetIdRepl.IsValid())
	{
		return FString();
	}

	const FUniqueNetId& UniqueNetIdRef = *UniqueNetIdRepl;
	return IdentityInterface->GetPlayerNickname(UniqueNetIdRef);
}

