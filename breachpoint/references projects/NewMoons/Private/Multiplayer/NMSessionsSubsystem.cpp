// Fill out your copyright notice in the Description page of Project Settings.


#include "Multiplayer/NMSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystemUtils.h"
#include "Multiplayer/Data/NMSubsytemData.h"


UNMSessionsSubsystem::UNMSessionsSubsystem()
{

}

void UNMSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const auto* Subsystem = Online::GetSubsystem(GetWorld());
	checkf(Subsystem != nullptr, TEXT("Unable to get Online Subsystem."));

	SessionInterface = Subsystem->GetSessionInterface();
	if (!ensureMsgf(SessionInterface.IsValid(), TEXT("Unable to get the Session Interface"))) return;

	bCreateSessionOnDestroy = false;
}

void UNMSessionsSubsystem::Deinitialize()
{
	Super::Deinitialize();

	SessionInterface.Reset();
}

void UNMSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType, const FString& SessionName,
	const bool bIsDedicatedServer, const bool bIsPrivate, const bool bIsLANMatch)
{
	if (!ensureMsgf(SessionInterface.IsValid(), TEXT("Unable to get the Session Interface"))) return;

	// Save session params
	LastNumPublicConnections = NumPublicConnections;
	LastSessionName = SessionName;
	bLastSessionIsPrivate = bIsPrivate;
	bLastSessionIsLAN = bIsLANMatch;
	DesiredMatchType = MatchType;
	if (!SessionInterface.IsValid())
	{
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	auto ExistingSession = SessionInterface->GetNamedSession(NMGameSession);
	if (ExistingSession != nullptr)
	{
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		LastMatchType = MatchType;

		DestroySession();
	}

	LastSessionSettings = MakeShareable(new FOnlineSessionSettings);
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bUseLobbiesIfAvailable = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->bIsDedicated = bIsDedicatedServer;
	LastSessionSettings->bIsLANMatch = bIsLANMatch;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(FName("IsPrivate"), bIsPrivate, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(FName("SessionName"), SessionName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	
	// Store the delegate in a FDelegateHandle so we can later remove it from the delegate list
	CreateSessionCompleteDelegateHandle = SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &ThisClass::HandleCreateSessionComplete);

	//Create the session
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	const bool success = SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NMGameSession, *LastSessionSettings);
	if (!success)
	{
		//We failed to create the session simply remove the delegate for completion
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
		OnCreateSessionComplete.Broadcast(false);
	}
}

void UNMSessionsSubsystem::FindSessions(int32 MaxSearchResults, const bool bIsLANMatch)
{
	if (!ensureMsgf(SessionInterface.IsValid(), TEXT("Unable to get the Session Interface"))) return;

	//Register the delegate for when the find session complete and store its handle for later removal
	FindSessionsCompleteDelegateHandle = SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &ThisClass::HandleFindSessionsComplete);

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch);
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->bIsLanQuery = bIsLANMatch;
	LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
																    
	//TODO:Get the player's Unique Net ID from our user subsystem
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	const bool success = SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef());

	if (!success)
	{
		//We failed to find sessions simply remove the delegate for completion
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
		
		OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}


void UNMSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if (!ensureMsgf(SessionInterface.IsValid(), TEXT("Unable to get the Session Interface"))) return;
	
	//Register the delegate for when the join session complete and store its handle for later removal
	JoinSessionCompleteDelegateHandle = SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UNMSessionsSubsystem::HandleJoinSessionComplete);

	//TODO:Get the player's Unique Net ID from our user subsystem
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	const bool success = SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NMGameSession, SessionResult);
	if (!success)
	{
		//We failed to join session simply remove the delegate for completion
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();

		OnJoinSessionComplete.Broadcast(false, EEOSJoinSessionResultType::UnknownError, "");
	}
}

void UNMSessionsSubsystem::DestroySession()
{
	if (!ensureMsgf(SessionInterface.IsValid(), TEXT("Unable to get the Session Interface"))) return;

	//Register the delegate for when the destroy session complete and store its handle for later removal
	DestroySessionCompleteDelegateHandle = SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UNMSessionsSubsystem::HandleDestroySessionComplete);

	//Destroy existing session if it exists
	const auto ExistingSession = SessionInterface->GetNamedSession(NMGameSession);
	if (ExistingSession != nullptr)
	{
		SessionInterface->DestroySession(NMGameSession);
	}
	else //No session found
	{
		//Remove the destroy session completion delegate
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();

		OnDestroySessionComplete.Broadcast(false);
	}
}

void UNMSessionsSubsystem::StartSession()
{
	if (!ensureMsgf(SessionInterface.IsValid(), TEXT("Unable to get the Session Interface"))) return;

	StartSessionCompleteDelegateHandle = SessionInterface->OnStartSessionCompleteDelegates.AddUObject(this, &UNMSessionsSubsystem::HandleStartSessionComplete);

	if (!SessionInterface->StartSession(NMGameSession))
	{
		//We failed to join session simply remove the delegate for completion
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		StartSessionCompleteDelegateHandle.Reset();
		OnStartSessionComplete.Broadcast(false);
	}
}

void UNMSessionsSubsystem::EndSession()
{
	if (!ensureMsgf(SessionInterface.IsValid(), TEXT("Unable to get the Session Interface"))) return;

	//Register the delegate for when the destroy session complete and store its handle for later removal
	EndSessionCompleteDelegateHandle = SessionInterface->OnEndSessionCompleteDelegates.AddUObject(this, &UNMSessionsSubsystem::HandleEndSessionComplete);

	//End existing session if it exists
	const auto ExistingSession = SessionInterface->GetNamedSession(NMGameSession);
	if (ExistingSession != nullptr)
	{
		SessionInterface->EndSession(NMGameSession);
	}
	else //No session found
	{
		//Remove the end session completion delegate
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		EndSessionCompleteDelegateHandle.Reset();

		OnEndSessionComplete.Broadcast(false);
	}
}

void UNMSessionsSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
	}

	UE_LOG(LogTemp, Log, TEXT("Session named %s created: %s"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));
	OnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UNMSessionsSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
	}
	
	if (LastSessionSearch->SearchResults.Num() <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Sessions found: false"));
		OnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	// Enforcing to fix bug of 5.5. TODO: Remove as it gets patched by UE
	for (auto& SearchResult : LastSessionSearch->SearchResults)
	{
		SearchResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;
		SearchResult.Session.SessionSettings.bUsesPresence = true;
	}

	OnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, true);
}

void UNMSessionsSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	UE_LOG(LogTemp, Log, TEXT("Session named %s joined: %s"), *SessionName.ToString(), Result == EOnJoinSessionCompleteResult::Type::Success ? TEXT("true") : TEXT("false"));

	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();

		// Add network error handle if the game client fails to connect to the server
		NetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this, &UNMSessionsSubsystem::HandleNetworkFailure);
	}
	else
	{
		OnJoinSessionComplete.Broadcast(Result == EOnJoinSessionCompleteResult::Type::Success, UnknownError, "");
		return;
	}

	EEOSJoinSessionResultType ResultBP = EEOSJoinSessionResultType::UnknownError;
	switch (Result)
	{
	case EOnJoinSessionCompleteResult::Success:
		ResultBP = Success;
		break;
	case EOnJoinSessionCompleteResult::SessionIsFull:
		ResultBP = SessionIsFull;
		break;
	case EOnJoinSessionCompleteResult::SessionDoesNotExist:
		ResultBP = SessionDoesNotExist;
		break;
	case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
		ResultBP = CouldNotRetrieveAddress;
		break;
	case EOnJoinSessionCompleteResult::AlreadyInSession:
		ResultBP = AlreadyInSession;
		break;
	case EOnJoinSessionCompleteResult::UnknownError:
		ResultBP = UnknownError;
		break;
	}
	
	//Get the session address to make a client travel
	FString Address;
	SessionInterface->GetResolvedConnectString(NMGameSession, Address);
	
	//Fire our own delegate
	OnJoinSessionComplete.Broadcast(Result == EOnJoinSessionCompleteResult::Type::Success, ResultBP, Address);
}

void UNMSessionsSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();

		// Unregister network failure handler when we destroy the current session
		if (NetworkFailureDelegateHandle.IsValid())
		{
			GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
			NetworkFailureDelegateHandle.Reset();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Session named %s destroyed: %s"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));
	OnDestroySessionComplete.Broadcast(bWasSuccessful);

	// Check if we need to create another session right after it got destroyed
	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false; // Make sure to set this back to false to not get stuck in a loop
		//CreateSession(LastNumPublicConnections, LastSessionName, bLastSessionIsDedicatedServer, bLastSessionIsPrivate, bLastSessionIsLAN);
	}
}

void UNMSessionsSubsystem::HandleEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		EndSessionCompleteDelegateHandle.Reset();
	}

	UE_LOG(LogTemp, Log, TEXT("Session named %s ended: %s"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));
	OnEndSessionComplete.Broadcast(bWasSuccessful);
}

void UNMSessionsSubsystem::HandleStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		StartSessionCompleteDelegateHandle.Reset();
	}

	UE_LOG(LogTemp, Log, TEXT("Session named %s started: %s"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));
	OnStartSessionComplete.Broadcast(bWasSuccessful);
}

void UNMSessionsSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->DestroySession(NMGameSession);
		// Optional: Notify players or handle reconnection logic
	}

	// Unregister network failure handler
	GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);
	NetworkFailureDelegateHandle.Reset();
}