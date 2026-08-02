// Breachpoint. Platform session state: host, find, join, invite, leave — and nothing else.
#include "Online/BRSessionsSubsystem.h"

#include "Core/BRCore.h"
#include "Online/BRListenServerLifecycle.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

#define LOCTEXT_NAMESPACE "BRSessionsSubsystem"

class FBRSessionSearchState
{
public:
	TSharedPtr<FOnlineSessionSearch> Search;

	TArray<FOnlineSessionSearchResult> Resolved;

	FDelegateHandle CreateHandle;
	FDelegateHandle StartHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle InviteAcceptedHandle;

	int32 AddResolved(const FOnlineSessionSearchResult& Result)
	{
		return Resolved.Add(Result);
	}

	const FOnlineSessionSearchResult* GetResolved(int32 HandleId) const
	{
		return Resolved.IsValidIndex(HandleId) ? &Resolved[HandleId] : nullptr;
	}

	void ResetResolved()
	{
		Search.Reset();
		Resolved.Reset();
	}
};

namespace BRSessionsInternal
{
	static IOnlineSessionPtr GetSessions(const UWorld* World)
	{
		if (IOnlineSubsystem* const OSS = Online::GetSubsystem(World))
		{
			return OSS->GetSessionInterface();
		}
		return nullptr;
	}

	static IOnlineSubsystem* GetOSS(const UWorld* World)
	{
		return Online::GetSubsystem(World);
	}
}

void UBRSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SearchState = MakeShared<FBRSessionSearchState>();

	PreLoginHandle = FGameModeEvents::OnGameModePreLoginEvent().AddWeakLambda(this,
		[this](AGameModeBase* GameMode, const FUniqueNetIdRepl& NewPlayerId, FString& ErrorMessage)
		{
			const FString IdString = NewPlayerId.IsValid() ? NewPlayerId->ToString() : FString();
			OnPlayerPreLogin(GameMode, IdString, ErrorMessage);
		});

	PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddWeakLambda(this,
		[this](AGameModeBase* GameMode, APlayerController* NewPlayer)
		{
			OnPlayerPostLogin(GameMode, NewPlayer);
		});

	LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddWeakLambda(this,
		[this](AGameModeBase* GameMode, AController* Exiting)
		{
			OnPlayerLogout(GameMode, Exiting);
		});

	MatchStateHandle = FGameModeEvents::OnGameModeMatchStateSetEvent().AddWeakLambda(this,
		[this](FName MatchState)
		{
			OnMatchStateSet(MatchState);
		});

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddWeakLambda(this,
			[this](UWorld* FailedWorld, UNetDriver*, ENetworkFailure::Type FailureType, const FString& ErrorString)
			{
				OnNetworkFailed(FailedWorld, static_cast<int32>(FailureType), ErrorString);
			});

		TravelFailureHandle = GEngine->OnTravelFailure().AddWeakLambda(this,
			[this](UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
			{
				OnTravelFailed(FailedWorld, static_cast<int32>(FailureType), ErrorString);
			});
	}

	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddWeakLambda(this,
		[this](const FString& MapName)
		{
			HandlePreLoadMap(MapName);
		});

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddWeakLambda(this,
		[this](UWorld* LoadedWorld)
		{
			HandlePostLoadMap(LoadedWorld);
		});

	if (const IOnlineSessionPtr Sessions = BRSessionsInternal::GetSessions(GetWorld()))
	{
		SearchState->InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
			FOnSessionUserInviteAcceptedDelegate::CreateWeakLambda(this,
				[this](const bool bWasSuccessful, const int32, FUniqueNetIdPtr,
					const FOnlineSessionSearchResult& InviteResult)
				{
					int32 ResolvedIndex = INDEX_NONE;
					if (bWasSuccessful && InviteResult.IsValid() && SearchState.IsValid())
					{
						ResolvedIndex = SearchState->AddResolved(InviteResult);
					}
					OnInviteAcceptedInternal(bWasSuccessful, ResolvedIndex);
				}));
	}
	else
	{
	}

	const IOnlineSubsystem* const OSS = BRSessionsInternal::GetOSS(GetWorld());
	const FString PlatformName = OSS ? OSS->GetSubsystemName().ToString() : FString(TEXT("none"));
}

void UBRSessionsSubsystem::Deinitialize()
{
	ClearOperationTimeout();
	ClearAllOssDelegates();

	if (PreLoginHandle.IsValid())	{ FGameModeEvents::OnGameModePreLoginEvent().Remove(PreLoginHandle); }
	if (PostLoginHandle.IsValid())	{ FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle); }
	if (LogoutHandle.IsValid())		{ FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle); }
	if (MatchStateHandle.IsValid())	{ FGameModeEvents::OnGameModeMatchStateSetEvent().Remove(MatchStateHandle); }

	if (GEngine)
	{
		if (NetworkFailureHandle.IsValid())	{ GEngine->OnNetworkFailure().Remove(NetworkFailureHandle); }
		if (TravelFailureHandle.IsValid())	{ GEngine->OnTravelFailure().Remove(TravelFailureHandle); }
	}

	if (PreLoadMapHandle.IsValid())		{ FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle); }
	if (PostLoadMapHandle.IsValid())	{ FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle); }

	if (ServerLifecycle != nullptr && HostingStateHandle.IsValid())
	{
		ServerLifecycle->OnHostingStateChanged().Remove(HostingStateHandle);
	}

	SearchState.Reset();

	Super::Deinitialize();
}

bool UBRSessionsSubsystem::HostListenSession(const FBRHostSessionParams& Params)
{
	if (SessionState != EBRSessionState::Idle)
	{
		FailOperation(EBRSessionFailure::AlreadyInSession,
			LOCTEXT("AlreadyInSession", "You are already in a session."),
			TEXT("already_in_session"), SessionState, &OnHostSessionCreated);
		return false;
	}

	const IOnlineSessionPtr Sessions = BRSessionsInternal::GetSessions(GetWorld());
	if (!Sessions.IsValid())
	{
		FailOperation(EBRSessionFailure::NoSessionInterface,
			LOCTEXT("NoSessions", "Online services are unavailable. Restart the game and try again."),
			TEXT("no_session_interface"), EBRSessionState::Idle, &OnHostSessionCreated);
		return false;
	}

	ULocalPlayer* const LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	const FUniqueNetIdRepl LocalId = LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();
	if (!LocalId.IsValid())
	{
		FailOperation(EBRSessionFailure::NoLocalPlayer,
			LOCTEXT("NoLocalPlayer", "No signed-in player. Sign in and try again."),
			TEXT("no_local_player"), EBRSessionState::Idle, &OnHostSessionCreated);
		return false;
	}

	PendingHostParams = Params;
	if (PendingHostParams.MapPath.IsEmpty())
	{
		PendingHostParams.MapPath = DefaultArenaMapPath;
	}

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = FMath::Clamp(Params.MaxPlayers, 1, 16);
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = Params.bIsLANMatch;
	Settings.bIsDedicated = false;
	Settings.bShouldAdvertise = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = !Params.bFriendsOnly;
	Settings.bAllowJoinViaPresenceFriendsOnly = Params.bFriendsOnly;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bUsesStats = false;
	Settings.bAntiCheatProtected = false;

	Settings.Set(SETTING_MAPNAME, PendingHostParams.MapPath, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(SEARCH_KEYWORDS, BRSessions::KeywordValue.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);

	SetSessionState(EBRSessionState::Creating);
	bIsHost = true;
	bLocalLeaveRequested = false;
	bDisconnectNoticeLatched = false;

	SearchState->CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateWeakLambda(this,
			[this](FName, bool bWasSuccessful) { OnCreateSessionFinished(bWasSuccessful); }));

	StartOperationTimeout(EBRSessionFailure::CreateFailed, TEXT("create_timeout"));

	if (!Sessions->CreateSession(*LocalId, BRSessions::SessionName, Settings))
	{
		ClearOperationTimeout();
		ClearAllOssDelegates();
		bIsHost = false;
		FailOperation(EBRSessionFailure::CreateFailed,
			LOCTEXT("CreateRefused", "Could not create a match. Check your connection and try again."),
			TEXT("create_refused"), EBRSessionState::Idle, &OnHostSessionCreated);
		return false;
	}

	return true;
}

void UBRSessionsSubsystem::OnCreateSessionFinished(bool bWasSuccessful)
{
	ClearOperationTimeout();
	ClearAllOssDelegates();

	if (!bWasSuccessful)
	{
		bIsHost = false;
		FailOperation(EBRSessionFailure::CreateFailed,
			LOCTEXT("CreateFailed", "Could not create a match. Check your connection and try again."),
			TEXT("create_failed"), EBRSessionState::Idle, &OnHostSessionCreated);
		return;
	}

	SetSessionState(EBRSessionState::Hosting);

	EnsureServerLifecycle();
	if (ServerLifecycle != nullptr)
	{
		ServerLifecycle->InitializeHosting(GetGameInstance());
	}

	OnHostSessionCreated.Broadcast(FBRSessionOpResult::Success());

	PendingTravelMapPath = PendingHostParams.MapPath;
	const FString Options = FString(TEXT("listen")) + PendingHostParams.ExtraTravelOptions;

	UGameplayStatics::OpenLevel(this, FName(*PendingTravelMapPath), true, Options);
}

void UBRSessionsSubsystem::OnStartSessionFinished(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
	}
	else
	{
		UE_LOG(LogBROnline, Warning, TEXT("Sessions: StartSession failed; the platform's view of this match may be stale."));
	}
}

bool UBRSessionsSubsystem::FindAndJoinBestSession(const FBRSessionQuery& Query)
{
	if (SessionState != EBRSessionState::Idle)
	{
		FBRSessionOpResult Result = FailOperation(EBRSessionFailure::AlreadyInSession,
			LOCTEXT("AlreadyInSession2", "You are already in a session."),
			TEXT("already_in_session"), SessionState, nullptr);
		OnSessionSearchCompleted.Broadcast(0, Result);
		return false;
	}

	const IOnlineSessionPtr Sessions = BRSessionsInternal::GetSessions(GetWorld());
	if (!Sessions.IsValid())
	{
		FBRSessionOpResult Result = FailOperation(EBRSessionFailure::NoSessionInterface,
			LOCTEXT("NoSessions2", "Online services are unavailable. Restart the game and try again."),
			TEXT("no_session_interface"), EBRSessionState::Idle, nullptr);
		OnSessionSearchCompleted.Broadcast(0, Result);
		return false;
	}

	ULocalPlayer* const LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
	const FUniqueNetIdRepl LocalId = LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();
	if (!LocalId.IsValid())
	{
		FBRSessionOpResult Result = FailOperation(EBRSessionFailure::NoLocalPlayer,
			LOCTEXT("NoLocalPlayer2", "No signed-in player. Sign in and try again."),
			TEXT("no_local_player"), EBRSessionState::Idle, nullptr);
		OnSessionSearchCompleted.Broadcast(0, Result);
		return false;
	}

	PendingQuery = Query;
	SearchState->ResetResolved();
	SearchState->Search = MakeShared<FOnlineSessionSearch>();
	SearchState->Search->MaxSearchResults = FMath::Clamp(Query.MaxResults, 1, 200);
	SearchState->Search->bIsLanQuery = Query.bLanQuery;
	SearchState->Search->TimeoutInSeconds = FMath::Max(Query.TimeoutSeconds, 1.f);

	SearchState->Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SearchState->Search->QuerySettings.Set(SEARCH_KEYWORDS, BRSessions::KeywordValue.ToString(), EOnlineComparisonOp::Equals);
	SearchState->Search->QuerySettings.Set(SEARCH_MINSLOTSAVAILABLE,
		FMath::Max(Query.MinOpenSlots, 1), EOnlineComparisonOp::GreaterThanEquals);

	SetSessionState(EBRSessionState::Searching);
	bIsHost = false;
	bLocalLeaveRequested = false;
	bDisconnectNoticeLatched = false;

	SearchState->FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateWeakLambda(this,
			[this](bool bWasSuccessful) { OnFindSessionsFinished(bWasSuccessful); }));

	StartOperationTimeout(EBRSessionFailure::SearchFailed, TEXT("search_timeout"));

	if (!Sessions->FindSessions(*LocalId, SearchState->Search.ToSharedRef()))
	{
		ClearOperationTimeout();
		ClearAllOssDelegates();
		FBRSessionOpResult Result = FailOperation(EBRSessionFailure::SearchFailed,
			LOCTEXT("SearchRefused", "Could not search for matches. Check your connection and try again."),
			TEXT("search_refused"), EBRSessionState::Idle, nullptr);
		OnSessionSearchCompleted.Broadcast(0, Result);
		return false;
	}

	return true;
}

void UBRSessionsSubsystem::OnFindSessionsFinished(bool bWasSuccessful)
{
	ClearOperationTimeout();
	ClearAllOssDelegates();

	if (!bWasSuccessful || !SearchState.IsValid() || !SearchState->Search.IsValid())
	{
		FBRSessionOpResult Result = FailOperation(EBRSessionFailure::SearchFailed,
			LOCTEXT("SearchFailed", "Could not search for matches. Check your connection and try again."),
			TEXT("search_failed"), EBRSessionState::Idle, nullptr);
		OnSessionSearchCompleted.Broadcast(0, Result);
		return;
	}

	const int32 BestIndex = SelectBestSearchResult(PendingQuery);
	const int32 JoinableCount = (BestIndex == INDEX_NONE) ? 0 : 1;

	if (BestIndex == INDEX_NONE)
	{
		FBRSessionOpResult Result = FailOperation(EBRSessionFailure::NoSessionsFound,
			LOCTEXT("NoSessionsFound", "No matches are available right now. Ask a friend for an invite, or host one."),
			TEXT("no_sessions_found"), EBRSessionState::Idle, nullptr);
		OnSessionSearchCompleted.Broadcast(0, Result);
		return;
	}

	const FOnlineSessionSearchResult& Winner = SearchState->Search->SearchResults[BestIndex];
	const int32 HandleId = SearchState->AddResolved(Winner);

	FBRJoinTarget Target;
	Target.Kind = EBRJoinTargetKind::SessionHandle;
	Target.SessionHandleId = HandleId;
	Target.OpenSlots = Winner.Session.NumOpenPublicConnections;
	Target.PingMs = Winner.PingInMs;
	Target.DisplayLabel = FText::FromString(Winner.Session.OwningUserName);

	OnSessionSearchCompleted.Broadcast(JoinableCount, FBRSessionOpResult::Success());

	JoinResolvedTarget(Target);
}

int32 UBRSessionsSubsystem::SelectBestSearchResult(const FBRSessionQuery& Query) const
{
	if (!SearchState.IsValid() || !SearchState->Search.IsValid())
	{
		return INDEX_NONE;
	}

	int32 BestIndex = INDEX_NONE;
	int32 BestScore = MIN_int32;

	const TArray<FOnlineSessionSearchResult>& Results = SearchState->Search->SearchResults;
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Candidate = Results[Index];
		if (!Candidate.IsValid())
		{
			continue;
		}
		if (Candidate.Session.NumOpenPublicConnections < FMath::Max(Query.MinOpenSlots, 1))
		{
			continue;
		}

		const int32 Ping = (Candidate.PingInMs >= 0) ? Candidate.PingInMs : 250;
		const int32 Occupancy = Candidate.Session.SessionSettings.NumPublicConnections
			- Candidate.Session.NumOpenPublicConnections;
		const int32 Score = (-Ping * 4) + (Occupancy * 10);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = Index;
		}
	}

	return BestIndex;
}

bool UBRSessionsSubsystem::JoinResolvedTarget(const FBRJoinTarget& Target)
{
	if (!Target.IsValid())
	{
		FailOperation(EBRSessionFailure::JoinFailed,
			LOCTEXT("BadTarget", "That match is no longer available."),
			TEXT("invalid_join_target"), EBRSessionState::Idle, &OnSessionJoined);
		return false;
	}

	if (SessionState == EBRSessionState::Joining || SessionState == EBRSessionState::Travelling)
	{
		UE_LOG(LogBROnline, Warning, TEXT("Sessions: JoinResolvedTarget ignored — a join is already in flight."));
		return false;
	}

	ActiveJoinTarget = Target;
	bIsHost = false;
	bLocalLeaveRequested = false;
	bDisconnectNoticeLatched = false;

	OnJoinTargetResolved.Broadcast(ActiveJoinTarget);
	SetSessionState(EBRSessionState::Joining);

	switch (ActiveJoinTarget.Kind)
	{
	case EBRJoinTargetKind::SessionHandle:
	{
		const IOnlineSessionPtr Sessions = BRSessionsInternal::GetSessions(GetWorld());
		const FOnlineSessionSearchResult* Result = SearchState.IsValid()
			? SearchState->GetResolved(ActiveJoinTarget.SessionHandleId) : nullptr;

		if (!Sessions.IsValid() || !Result || !Result->IsValid())
		{
			FailOperation(EBRSessionFailure::SessionGone,
				LOCTEXT("SessionGone", "That match is no longer available."),
				TEXT("session_gone"), EBRSessionState::Idle, &OnSessionJoined);
			return false;
		}

		ULocalPlayer* const LocalPlayer = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
		const FUniqueNetIdRepl LocalId = LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId() : FUniqueNetIdRepl();
		if (!LocalId.IsValid())
		{
			FailOperation(EBRSessionFailure::NoLocalPlayer,
				LOCTEXT("NoLocalPlayer3", "No signed-in player. Sign in and try again."),
				TEXT("no_local_player"), EBRSessionState::Idle, &OnSessionJoined);
			return false;
		}

		SearchState->JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
			FOnJoinSessionCompleteDelegate::CreateWeakLambda(this,
				[this](FName, EOnJoinSessionCompleteResult::Type JoinResult)
				{
					OnJoinSessionFinished(static_cast<int32>(JoinResult));
				}));

		StartOperationTimeout(EBRSessionFailure::JoinFailed, TEXT("join_timeout"));

		if (!Sessions->JoinSession(*LocalId, BRSessions::SessionName, *Result))
		{
			ClearOperationTimeout();
			ClearAllOssDelegates();
			FailOperation(EBRSessionFailure::JoinFailed,
				LOCTEXT("JoinRefused", "Could not join that match. It may have just ended."),
				TEXT("join_refused"), EBRSessionState::Idle, &OnSessionJoined);
			return false;
		}
		return true;
	}

	case EBRJoinTargetKind::Address:
	{
		OnSessionJoined.Broadcast(FBRSessionOpResult::Success());
		SetSessionState(EBRSessionState::Travelling);

		FString TravelUrl = ActiveJoinTarget.ConnectAddress;
		const FString CredentialOption = ActiveJoinTarget.Credential.ToUrlOption();
		if (!CredentialOption.IsEmpty())
		{
			TravelUrl += TEXT("?") + CredentialOption;
		}

		APlayerController* const PC = GetLocalPlayerController();
		if (!PC)
		{
			FailOperation(EBRSessionFailure::TravelFailed,
				LOCTEXT("NoPCForTravel", "Could not join that match."),
				TEXT("no_local_pc"), EBRSessionState::Idle, &OnSessionJoined);
			return false;
		}

		OnClientTravelStarting.Broadcast(ActiveJoinTarget);
		PC->ClientTravel(TravelUrl, TRAVEL_Absolute);
		return true;
	}

	default:
		checkNoEntry();
		return false;
	}
}

void UBRSessionsSubsystem::OnJoinSessionFinished(int32 RawJoinResult)
{
	ClearOperationTimeout();
	ClearAllOssDelegates();

	const EOnJoinSessionCompleteResult::Type JoinResult =
		static_cast<EOnJoinSessionCompleteResult::Type>(RawJoinResult);

	if (JoinResult != EOnJoinSessionCompleteResult::Success)
	{
		EBRSessionFailure Failure = EBRSessionFailure::JoinFailed;
		FText Message = LOCTEXT("JoinFailed", "Could not join that match. It may have just ended.");
		FString Code = TEXT("join_failed");

		switch (JoinResult)
		{
		case EOnJoinSessionCompleteResult::SessionIsFull:
			Failure = EBRSessionFailure::SessionFull;
			Message = LOCTEXT("JoinFull", "That match filled up. Try another one.");
			Code = TEXT("session_full");
			break;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			Failure = EBRSessionFailure::SessionGone;
			Message = LOCTEXT("JoinGone", "That match has ended.");
			Code = TEXT("session_gone");
			break;
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
			Failure = EBRSessionFailure::AddressResolveFailed;
			Message = LOCTEXT("JoinNoAddress", "Could not reach that match's host.");
			Code = TEXT("address_resolve_failed");
			break;
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			Failure = EBRSessionFailure::AlreadyInSession;
			Message = LOCTEXT("JoinAlreadyIn", "You are already in that match.");
			Code = TEXT("already_in_session");
			break;
		default:
			break;
		}

		FailOperation(Failure, Message, Code, EBRSessionState::Idle, &OnSessionJoined);
		return;
	}

	const IOnlineSessionPtr Sessions = BRSessionsInternal::GetSessions(GetWorld());
	FString ConnectInfo;
	if (!Sessions.IsValid() || !Sessions->GetResolvedConnectString(BRSessions::SessionName, ConnectInfo)
		|| ConnectInfo.IsEmpty())
	{
		FailOperation(EBRSessionFailure::AddressResolveFailed,
			LOCTEXT("NoConnectString", "Could not reach that match's host."),
			TEXT("no_connect_string"), EBRSessionState::Idle, &OnSessionJoined);
		return;
	}

	APlayerController* const PC = GetLocalPlayerController();
	if (!PC)
	{
		FailOperation(EBRSessionFailure::TravelFailed,
			LOCTEXT("NoPCForTravel2", "Could not join that match."),
			TEXT("no_local_pc"), EBRSessionState::Idle, &OnSessionJoined);
		return;
	}

	OnSessionJoined.Broadcast(FBRSessionOpResult::Success());
	SetSessionState(EBRSessionState::Travelling);

	const FString CredentialOption = ActiveJoinTarget.Credential.ToUrlOption();
	if (!CredentialOption.IsEmpty())
	{
		ConnectInfo += (ConnectInfo.Contains(TEXT("?")) ? TEXT("&") : TEXT("?")) + CredentialOption;
	}

	OnClientTravelStarting.Broadcast(ActiveJoinTarget);

	PC->ClientTravel(ConnectInfo, TRAVEL_Absolute);
}

bool UBRSessionsSubsystem::ShowInviteUI()
{
	IOnlineSubsystem* const OSS = BRSessionsInternal::GetOSS(GetWorld());
	if (!OSS)
	{
		UE_LOG(LogBROnline, Warning, TEXT("Sessions: ShowInviteUI — no online subsystem."));
		return false;
	}

	const IOnlineExternalUIPtr ExternalUI = OSS->GetExternalUIInterface();
	if (!ExternalUI.IsValid())
	{
		return false;
	}

	if (!IsHosting() && SessionState != EBRSessionState::InSession)
	{
		UE_LOG(LogBROnline, Warning, TEXT("Sessions: ShowInviteUI with no session to invite into."));
		return false;
	}

	return ExternalUI->ShowInviteUI(GetLocalControllerId(), BRSessions::SessionName);
}

void UBRSessionsSubsystem::OnInviteAcceptedInternal(bool bWasSuccessful, int32 ResolvedIndex)
{
	if (!bWasSuccessful || ResolvedIndex == INDEX_NONE)
	{
		FailOperation(EBRSessionFailure::JoinFailed,
			LOCTEXT("InviteUnusable", "That invite is no longer valid."),
			TEXT("invite_unusable"), EBRSessionState::Idle, &OnSessionJoined);
		return;
	}

	if (SessionState != EBRSessionState::Idle)
	{
		LeaveSessionAndReturnToMainMenu();
	}

	const FOnlineSessionSearchResult* Result = SearchState.IsValid()
		? SearchState->GetResolved(ResolvedIndex) : nullptr;

	FBRJoinTarget Target;
	Target.Kind = EBRJoinTargetKind::SessionHandle;
	Target.SessionHandleId = ResolvedIndex;
	if (Result)
	{
		Target.OpenSlots = Result->Session.NumOpenPublicConnections;
		Target.PingMs = Result->PingInMs;
		Target.DisplayLabel = FText::FromString(Result->Session.OwningUserName);
	}

	OnInviteAccepted.Broadcast(Target);
	JoinResolvedTarget(Target);
}

void UBRSessionsSubsystem::LeaveSessionAndReturnToMainMenu()
{
	if (bIsHost)
	{
		HostQuitToMainMenu();
		return;
	}

	bLocalLeaveRequested = true;

	DestroySessionAndReturnToFrontEnd(EBRDisconnectReason::LocalPlayerLeft,
		FText::GetEmpty(), TEXT("local_leave"));
}

void UBRSessionsSubsystem::HostQuitToMainMenu()
{
	if (!bIsHost)
	{
		LeaveSessionAndReturnToMainMenu();
		return;
	}

	EnsureServerLifecycle();
	if (ServerLifecycle == nullptr)
	{
		UE_LOG(LogBROnline, Warning, TEXT("Sessions: host quit with no lifecycle; falling back to a local leave."));
		bLocalLeaveRequested = true;
		DestroySessionAndReturnToFrontEnd(EBRDisconnectReason::LocalPlayerLeft, FText::GetEmpty(), TEXT("local_leave"));
		return;
	}

	bLocalLeaveRequested = true;

	ServerLifecycle->RequestHostingEnd(EBRHostingEndReason::HostQuit);
}

void UBRSessionsSubsystem::RequestMatchTeardown(const FBRMatchResultSummary& Summary)
{
	if (!bIsHost)
	{
		return;
	}

	EnsureServerLifecycle();
	if (ServerLifecycle != nullptr)
	{
		ServerLifecycle->NotifyMatchComplete(Summary);
	}
}

void UBRSessionsSubsystem::HandleHostingStateChanged(EBRHostingState, EBRHostingState NewState)
{
	if (NewState == EBRHostingState::Ended)
	{
		const bool bWasHostQuit = bLocalLeaveRequested;
		DestroySessionAndReturnToFrontEnd(
			bWasHostQuit ? EBRDisconnectReason::LocalPlayerLeft : EBRDisconnectReason::HostEndedSession,
			FText::GetEmpty(), bWasHostQuit ? TEXT("host_quit") : TEXT("match_complete"));
	}
}

void UBRSessionsSubsystem::DestroySessionAndReturnToFrontEnd(EBRDisconnectReason Reason,
	const FText& Message, const FString& Code)
{
	const IOnlineSessionPtr Sessions = BRSessionsInternal::GetSessions(GetWorld());

	BroadcastDisconnect(Reason, Message, Code);

	if (Sessions.IsValid() && Sessions->GetNamedSession(BRSessions::SessionName))
	{
		SetSessionState(EBRSessionState::Leaving);
		SearchState->DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateWeakLambda(this,
				[this](FName, bool bWasSuccessful) { OnDestroySessionFinished(bWasSuccessful); }));

		if (!Sessions->DestroySession(BRSessions::SessionName))
		{
			ClearAllOssDelegates();
			OnDestroySessionFinished(false);
		}
		return;
	}

	OnDestroySessionFinished(true);
}

void UBRSessionsSubsystem::OnDestroySessionFinished(bool bWasSuccessful)
{
	ClearAllOssDelegates();
	ClearOperationTimeout();

	if (!bWasSuccessful)
	{
		UE_LOG(LogBROnline, Warning, TEXT("Sessions: DestroySession failed; returning to the front end anyway."));
	}

	bIsHost = false;
	ActiveJoinTarget = FBRJoinTarget();
	PendingTravelMapPath.Reset();
	if (SearchState.IsValid())
	{
		SearchState->ResetResolved();
	}

	SetSessionState(EBRSessionState::Idle);

	if (UGameInstance* const GI = GetGameInstance())
	{
		GI->ReturnToMainMenu();
	}
}

void UBRSessionsSubsystem::BroadcastDisconnect(EBRDisconnectReason Reason, const FText& Message, const FString& Code)
{
	if (bDisconnectNoticeLatched)
	{
		return;
	}

	bDisconnectNoticeLatched = true;

	LastDisconnectNotice.Reason = Reason;
	LastDisconnectNotice.DiagnosticCode = Code;
	LastDisconnectNotice.bWasHost = bIsHost;
	LastDisconnectNotice.UserMessage = Message;

	if (LastDisconnectNotice.UserMessage.IsEmpty())
	{
		switch (Reason)
		{
		case EBRDisconnectReason::HostEndedSession:
			LastDisconnectNotice.UserMessage = LOCTEXT("DiscHostEnded", "The host ended the match.");
			break;
		case EBRDisconnectReason::ConnectionLost:
			LastDisconnectNotice.UserMessage = LOCTEXT("DiscConnLost", "Connection to the host was lost.");
			break;
		case EBRDisconnectReason::Kicked:
			LastDisconnectNotice.UserMessage = LOCTEXT("DiscKicked", "You were removed from the match.");
			break;
		case EBRDisconnectReason::JoinRejected:
			LastDisconnectNotice.UserMessage = LOCTEXT("DiscRejected", "The match refused your connection.");
			break;
		case EBRDisconnectReason::VersionMismatch:
			LastDisconnectNotice.UserMessage = LOCTEXT("DiscVersion", "Your game version does not match the host's.");
			break;
		case EBRDisconnectReason::Unknown:
			LastDisconnectNotice.UserMessage = LOCTEXT("DiscUnknown", "You were disconnected from the match.");
			break;
		case EBRDisconnectReason::LocalPlayerLeft:
		default:
			break;
		}
	}

	OnDisconnected.Broadcast(LastDisconnectNotice);
}

FBRJoinVerdict UBRSessionsSubsystem::ValidateIncomingJoin(const FBRJoinRequest& Request)
{
	EnsureServerLifecycle();
	if (ServerLifecycle == nullptr)
	{
		return FBRJoinVerdict::Reject(
			LOCTEXT("NoHostingToJoin", "This match is not accepting players."),
			TEXT("no_lifecycle"));
	}

	return ServerLifecycle->ValidateJoin(Request);
}

void UBRSessionsSubsystem::OnPlayerPreLogin(AGameModeBase* GameMode, const FString& PlatformIdString,
	FString& OutErrorMessage)
{
	if (!GameMode || !IsOurWorld(GameMode->GetWorld()))
	{
		return;
	}

	FBRJoinRequest Request;
	Request.PlatformIdString = PlatformIdString;

	const AGameMode* const FullGameMode = Cast<AGameMode>(GameMode);
	Request.bJoinInProgress = FullGameMode && (FullGameMode->GetMatchState() == MatchState::InProgress);

	const FBRJoinVerdict Verdict = ValidateIncomingJoin(Request);
	if (!Verdict.IsAccepted())
	{
		OutErrorMessage = Verdict.DenialReason.IsEmpty()
			? FString(TEXT("Join refused."))
			: Verdict.DenialReason.ToString();
	}
}

void UBRSessionsSubsystem::OnPlayerPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!GameMode || !IsOurWorld(GameMode->GetWorld()) || !NewPlayer)
	{
		return;
	}

	EnsureServerLifecycle();
	if (ServerLifecycle != nullptr)
	{
		FString IdString;
		if (const APlayerState* const PS = NewPlayer->PlayerState)
		{
			IdString = PS->GetUniqueId().IsValid() ? PS->GetUniqueId()->ToString() : FString();
		}
		ServerLifecycle->NotifyPlayerJoined(IdString);
	}

	const IOnlineSessionPtr Sessions = BRSessionsInternal::GetSessions(GetWorld());
	if (Sessions.IsValid())
	{
		if (const FNamedOnlineSession* const Named = Sessions->GetNamedSession(BRSessions::SessionName))
		{
			OnSessionMembershipChanged.Broadcast(Named->RegisteredPlayers.Num(),
				Named->SessionSettings.NumPublicConnections);
		}
	}
}

void UBRSessionsSubsystem::OnPlayerLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (!GameMode || !IsOurWorld(GameMode->GetWorld()))
	{
		return;
	}

	EnsureServerLifecycle();
	if (ServerLifecycle != nullptr)
	{
		FString IdString;
		if (const APlayerController* const PC = Cast<APlayerController>(Exiting))
		{
			if (const APlayerState* const PS = PC->PlayerState)
			{
				IdString = PS->GetUniqueId().IsValid() ? PS->GetUniqueId()->ToString() : FString();
			}
		}
		ServerLifecycle->NotifyPlayerLeft(IdString);
	}

	const IOnlineSessionPtr Sessions = BRSessionsInternal::GetSessions(GetWorld());
	if (Sessions.IsValid())
	{
		if (const FNamedOnlineSession* const Named = Sessions->GetNamedSession(BRSessions::SessionName))
		{
			OnSessionMembershipChanged.Broadcast(Named->RegisteredPlayers.Num(),
				Named->SessionSettings.NumPublicConnections);
		}
	}
}

void UBRSessionsSubsystem::OnMatchStateSet(FName MatchStateName)
{
	if (!bIsHost)
	{
		return;
	}

	const UWorld* const World = GetWorld();
	const AGameMode* const GameMode = World ? Cast<AGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!GameMode || GameMode->GetMatchState() != MatchStateName)
	{
		return;
	}

	const IOnlineSessionPtr Sessions = BRSessionsInternal::GetSessions(World);
	if (!Sessions.IsValid())
	{
		return;
	}

	if (MatchStateName == MatchState::InProgress)
	{
		SearchState->StartHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(
			FOnStartSessionCompleteDelegate::CreateWeakLambda(this,
				[this](FName, bool bWasSuccessful) { OnStartSessionFinished(bWasSuccessful); }));
		Sessions->StartSession(BRSessions::SessionName);
	}
	else if (MatchStateName == MatchState::WaitingPostMatch)
	{
		Sessions->EndSession(BRSessions::SessionName);
	}
}

void UBRSessionsSubsystem::OnNetworkFailed(UWorld* FailedWorld, int32 RawFailureType, const FString& ErrorString)
{
	if (!IsOurWorld(FailedWorld))
	{
		return;
	}

	const ENetworkFailure::Type FailureType = static_cast<ENetworkFailure::Type>(RawFailureType);

	UE_LOG(LogBROnline, Warning, TEXT("Sessions: network failure '%s' (%s)."),
		ENetworkFailure::ToString(FailureType), *ErrorString);

	EBRDisconnectReason Reason = EBRDisconnectReason::Unknown;
	FString Code = TEXT("network_failure");

	switch (FailureType)
	{
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::ConnectionTimeout:
		Reason = EBRDisconnectReason::ConnectionLost;
		Code = TEXT("connection_lost");
		break;
	case ENetworkFailure::FailureReceived:
		Reason = EBRDisconnectReason::Kicked;
		Code = TEXT("failure_received");
		break;
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
	case ENetworkFailure::NetGuidMismatch:
	case ENetworkFailure::NetChecksumMismatch:
		Reason = EBRDisconnectReason::VersionMismatch;
		Code = TEXT("version_mismatch");
		break;
	case ENetworkFailure::PendingConnectionFailure:
		Reason = EBRDisconnectReason::JoinRejected;
		Code = TEXT("connection_refused");
		break;
	default:
		break;
	}

	DestroySessionAndReturnToFrontEnd(Reason, FText::GetEmpty(), Code);
}

void UBRSessionsSubsystem::OnTravelFailed(UWorld* FailedWorld, int32, const FString& ErrorString)
{
	if (!IsOurWorld(FailedWorld))
	{
		return;
	}

	UE_LOG(LogBROnline, Warning, TEXT("Sessions: travel failure (%s)."), *ErrorString);

	if (SessionState == EBRSessionState::Travelling || SessionState == EBRSessionState::Joining)
	{
		FailOperation(EBRSessionFailure::TravelFailed,
			LOCTEXT("TravelFailed", "Could not enter that match."),
			TEXT("travel_failed"), EBRSessionState::Idle, &OnSessionJoined);
		return;
	}

	if (bIsHost && SessionState == EBRSessionState::Hosting)
	{
		FailOperation(EBRSessionFailure::TravelFailed,
			LOCTEXT("HostTravelFailed", "Could not open the arena."),
			TEXT("host_travel_failed"), EBRSessionState::Hosting, &OnHostSessionCreated);
		DestroySessionAndReturnToFrontEnd(EBRDisconnectReason::Unknown, FText::GetEmpty(), TEXT("host_travel_failed"));
	}
}

void UBRSessionsSubsystem::HandlePreLoadMap(const FString& MapName)
{
	if (SessionState == EBRSessionState::InSession && !bIsHost && !bLocalLeaveRequested)
	{
		DestroySessionAndReturnToFrontEnd(EBRDisconnectReason::HostEndedSession, FText::GetEmpty(), TEXT("host_ended_session"));
	}
}

void UBRSessionsSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!IsOurWorld(LoadedWorld))
	{
		return;
	}

	if (bIsHost && SessionState == EBRSessionState::Hosting)
	{
		EnsureServerLifecycle();
		if (ServerLifecycle != nullptr)
		{
			ServerLifecycle->InitializeHosting(GetGameInstance());
			ServerLifecycle->NotifyServerReadyForPlayers();
		}
		return;
	}

	if (SessionState == EBRSessionState::Travelling)
	{
		SetSessionState(EBRSessionState::InSession);
	}
}

void UBRSessionsSubsystem::EnsureServerLifecycle()
{
	if (ServerLifecycle != nullptr)
	{
		return;
	}

	UBRListenServerLifecycle* const Listen = NewObject<UBRListenServerLifecycle>(this);
	ServerLifecycle.SetObject(Listen);
	ServerLifecycle.SetInterface(Cast<IBRServerLifecycle>(Listen));

	HostingStateHandle = ServerLifecycle->OnHostingStateChanged().AddWeakLambda(this,
		[this](EBRHostingState OldState, EBRHostingState NewState)
		{
			HandleHostingStateChanged(OldState, NewState);
		});
}

void UBRSessionsSubsystem::SetSessionState(EBRSessionState NewState)
{
	if (SessionState == NewState)
	{
		return;
	}

	const EBRSessionState OldState = SessionState;
	SessionState = NewState;

	OnSessionStateChanged.Broadcast(OldState, NewState);
}

FBRSessionOpResult UBRSessionsSubsystem::FailOperation(EBRSessionFailure Failure, const FText& UserMessage,
	const FString& Code, EBRSessionState RecoveryState, FBRSessionOpCompleteSignature* SpecificDelegate)
{
	FBRSessionOpResult Result;
	Result.bSucceeded = false;
	Result.Failure = Failure;
	Result.UserMessage = UserMessage;
	Result.DiagnosticCode = Code;

	UE_LOG(LogBROnline, Warning, TEXT("Sessions: operation failed (%s)."), *Code);

	SetSessionState(RecoveryState);

	if (SpecificDelegate)
	{
		SpecificDelegate->Broadcast(Result);
	}

	OnSessionFailure.Broadcast(Result);

	return Result;
}

void UBRSessionsSubsystem::ClearAllOssDelegates()
{
	if (!SearchState.IsValid())
	{
		return;
	}

	const IOnlineSessionPtr Sessions = BRSessionsInternal::GetSessions(GetWorld());
	if (!Sessions.IsValid())
	{
		return;
	}

	if (SearchState->CreateHandle.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(SearchState->CreateHandle);
		SearchState->CreateHandle.Reset();
	}
	if (SearchState->StartHandle.IsValid())
	{
		Sessions->ClearOnStartSessionCompleteDelegate_Handle(SearchState->StartHandle);
		SearchState->StartHandle.Reset();
	}
	if (SearchState->FindHandle.IsValid())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(SearchState->FindHandle);
		SearchState->FindHandle.Reset();
	}
	if (SearchState->JoinHandle.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(SearchState->JoinHandle);
		SearchState->JoinHandle.Reset();
	}
	if (SearchState->DestroyHandle.IsValid())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(SearchState->DestroyHandle);
		SearchState->DestroyHandle.Reset();
	}
}

void UBRSessionsSubsystem::StartOperationTimeout(EBRSessionFailure OnTimeoutFailure, const FString& Code)
{
	TimeoutFailure = OnTimeoutFailure;
	TimeoutCode = Code;

	if (UGameInstance* const GI = GetGameInstance())
	{
		GI->GetTimerManager().SetTimer(OperationTimeoutHandle, this,
			&UBRSessionsSubsystem::HandleOperationTimeout,
			FMath::Clamp(OperationTimeoutSeconds, 5.f, 120.f), false);
	}
}

void UBRSessionsSubsystem::ClearOperationTimeout()
{
	if (UGameInstance* const GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(OperationTimeoutHandle);
	}
}

void UBRSessionsSubsystem::HandleOperationTimeout()
{
	ClearAllOssDelegates();

	FBRSessionOpCompleteSignature* Specific = nullptr;
	switch (SessionState)
	{
	case EBRSessionState::Creating:		Specific = &OnHostSessionCreated; break;
	case EBRSessionState::Joining:		Specific = &OnSessionJoined; break;
	default:							break;
	}

	if (bIsHost && SessionState == EBRSessionState::Creating)
	{
		bIsHost = false;
	}

	FBRSessionOpResult Result = FailOperation(TimeoutFailure,
		LOCTEXT("OpTimeout", "The online service did not respond. Try again."),
		TimeoutCode.IsEmpty() ? TEXT("timeout") : TimeoutCode, EBRSessionState::Idle, Specific);

	if (!Specific)
	{
		OnSessionSearchCompleted.Broadcast(0, Result);
	}
}

bool UBRSessionsSubsystem::IsOurWorld(const UWorld* OtherWorld) const
{
	const UGameInstance* const GI = GetGameInstance();
	return OtherWorld != nullptr && GI != nullptr && OtherWorld->GetGameInstance() == GI;
}

APlayerController* UBRSessionsSubsystem::GetLocalPlayerController() const
{
	const UGameInstance* const GI = GetGameInstance();
	ULocalPlayer* const LocalPlayer = GI ? GI->GetFirstGamePlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
}

int32 UBRSessionsSubsystem::GetLocalControllerId() const
{
	const UGameInstance* const GI = GetGameInstance();
	const ULocalPlayer* const LocalPlayer = GI ? GI->GetFirstGamePlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetControllerId() : 0;
}

#undef LOCTEXT_NAMESPACE
