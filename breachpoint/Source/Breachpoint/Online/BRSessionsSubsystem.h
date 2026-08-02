// Breachpoint. Platform session state: host, find, join, invite, leave — and nothing else.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/ScriptInterface.h"
#include "Online/BRServerLifecycle.h"

#include "BRSessionsSubsystem.generated.h"

class AController;
class AGameModeBase;
class APlayerController;
class UNetDriver;
class UWorld;
struct FUniqueNetIdRepl;

namespace BRSessions
{
	inline const FName SessionName = FName(TEXT("GameSession"));

	inline const FName KeywordValue = FName(TEXT("BREACHPOINT"));

	inline constexpr int32 DefaultMaxPlayers = 8;
}

UENUM()
enum class EBRSessionState : uint8
{
	Idle,
	Creating,
	Hosting,
	Searching,
	Joining,
	Travelling,
	InSession,
	Leaving
};

UENUM()
enum class EBRSessionFailure : uint8
{
	None,
	NoOnlineSubsystem,
	NoSessionInterface,
	NoLocalPlayer,
	AlreadyInSession,
	CreateFailed,
	SearchFailed,
	NoSessionsFound,
	JoinFailed,
	SessionFull,
	SessionGone,
	AddressResolveFailed,
	TravelFailed,
	DestroyFailed,
	AdmissionRejected,
	Timeout,
	Unsupported
};

UENUM()
enum class EBRDisconnectReason : uint8
{
	None,
	LocalPlayerLeft,
	HostEndedSession,
	ConnectionLost,
	Kicked,
	JoinRejected,
	VersionMismatch,
	Unknown
};

USTRUCT()
struct FBRSessionOpResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSucceeded = false;

	UPROPERTY()
	EBRSessionFailure Failure = EBRSessionFailure::None;

	UPROPERTY()
	FText UserMessage;

	UPROPERTY()
	FString DiagnosticCode;

	static FBRSessionOpResult Success()
	{
		FBRSessionOpResult Result;
		Result.bSucceeded = true;
		return Result;
	}
};

USTRUCT()
struct FBRDisconnectNotice
{
	GENERATED_BODY()

	UPROPERTY()
	EBRDisconnectReason Reason = EBRDisconnectReason::None;

	UPROPERTY()
	FText UserMessage;

	UPROPERTY()
	FString DiagnosticCode;

	UPROPERTY()
	bool bWasHost = false;
};

USTRUCT()
struct FBRHostSessionParams
{
	GENERATED_BODY()

	UPROPERTY()
	int32 MaxPlayers = BRSessions::DefaultMaxPlayers;

	UPROPERTY()
	bool bIsLANMatch = false;

	UPROPERTY()
	bool bFriendsOnly = true;

	UPROPERTY()
	FString MapPath;

	UPROPERTY()
	FString ExtraTravelOptions;
};

USTRUCT()
struct FBRSessionQuery
{
	GENERATED_BODY()

	UPROPERTY()
	int32 MaxResults = 25;

	UPROPERTY()
	bool bLanQuery = false;

	UPROPERTY()
	int32 MinOpenSlots = 1;

	UPROPERTY()
	bool bIncludeMatchesInProgress = true;

	UPROPERTY()
	float TimeoutSeconds = 20.f;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FBRSessionStateChangedSignature, EBRSessionState, EBRSessionState);
DECLARE_MULTICAST_DELEGATE_OneParam(FBRSessionOpCompleteSignature, const FBRSessionOpResult&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FBRSessionSearchCompleteSignature, int32, const FBRSessionOpResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FBRJoinTargetSignature, const FBRJoinTarget&);
DECLARE_MULTICAST_DELEGATE_OneParam(FBRSessionFailureSignature, const FBRSessionOpResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FBRDisconnectedSignature, const FBRDisconnectNotice&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FBRSessionMembershipChangedSignature, int32, int32);

UCLASS(Config = Game)
class BREACHPOINT_API UBRSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool HostListenSession(const FBRHostSessionParams& Params);

	bool FindAndJoinBestSession(const FBRSessionQuery& Query);

	bool JoinResolvedTarget(const FBRJoinTarget& Target);

	bool ShowInviteUI();

	void LeaveSessionAndReturnToMainMenu();

	void HostQuitToMainMenu();

	void RequestMatchTeardown(const FBRMatchResultSummary& Summary);

	FBRJoinVerdict ValidateIncomingJoin(const FBRJoinRequest& Request);

	TScriptInterface<IBRServerLifecycle> GetServerLifecycle() const { return ServerLifecycle; }

	EBRSessionState GetSessionState() const { return SessionState; }

	bool IsInSession() const { return SessionState == EBRSessionState::Hosting || SessionState == EBRSessionState::InSession; }

	bool IsHosting() const { return bIsHost; }

	const FBRJoinTarget& GetActiveJoinTarget() const { return ActiveJoinTarget; }

	const FBRDisconnectNotice& GetLastDisconnectNotice() const { return LastDisconnectNotice; }

	FBRSessionStateChangedSignature OnSessionStateChanged;
	FBRSessionOpCompleteSignature OnHostSessionCreated;
	FBRSessionSearchCompleteSignature OnSessionSearchCompleted;
	FBRJoinTargetSignature OnJoinTargetResolved;
	FBRJoinTargetSignature OnInviteAccepted;
	FBRSessionOpCompleteSignature OnSessionJoined;
	FBRJoinTargetSignature OnClientTravelStarting;
	FBRSessionFailureSignature OnSessionFailure;
	FBRDisconnectedSignature OnDisconnected;
	FBRSessionMembershipChangedSignature OnSessionMembershipChanged;

protected:
	UPROPERTY(Config)
	FString DefaultArenaMapPath = TEXT("/Game/Maps/BR_Arena01");

	UPROPERTY(Config)
	float OperationTimeoutSeconds = 30.f;

private:
	void SetSessionState(EBRSessionState NewState);

	FBRSessionOpResult FailOperation(EBRSessionFailure Failure, const FText& UserMessage, const FString& Code,
		EBRSessionState RecoveryState, FBRSessionOpCompleteSignature* SpecificDelegate);

	void OnCreateSessionFinished(bool bWasSuccessful);
	void OnStartSessionFinished(bool bWasSuccessful);
	void OnFindSessionsFinished(bool bWasSuccessful);
	void OnJoinSessionFinished(int32 RawJoinResult);
	void OnDestroySessionFinished(bool bWasSuccessful);
	void OnInviteAcceptedInternal(bool bWasSuccessful, int32 ResolvedIndex);

	void ClearAllOssDelegates();
	void StartOperationTimeout(EBRSessionFailure OnTimeoutFailure, const FString& Code);
	void ClearOperationTimeout();
	void HandleOperationTimeout();

	void OnPlayerPreLogin(AGameModeBase* GameMode, const FString& PlatformIdString, FString& OutErrorMessage);
	void OnPlayerPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void OnPlayerLogout(AGameModeBase* GameMode, AController* Exiting);
	void OnMatchStateSet(FName MatchState);
	void OnNetworkFailed(UWorld* FailedWorld, int32 RawFailureType, const FString& ErrorString);
	void OnTravelFailed(UWorld* FailedWorld, int32 RawFailureType, const FString& ErrorString);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandlePreLoadMap(const FString& MapName);
	void HandleHostingStateChanged(EBRHostingState OldState, EBRHostingState NewState);

	bool IsOurWorld(const UWorld* OtherWorld) const;

	void EnsureServerLifecycle();
	void BroadcastDisconnect(EBRDisconnectReason Reason, const FText& Message, const FString& Code);
	void DestroySessionAndReturnToFrontEnd(EBRDisconnectReason Reason, const FText& Message, const FString& Code);
	APlayerController* GetLocalPlayerController() const;
	int32 GetLocalControllerId() const;

	int32 SelectBestSearchResult(const FBRSessionQuery& Query) const;

	UPROPERTY()
	TScriptInterface<IBRServerLifecycle> ServerLifecycle;

	EBRSessionState SessionState = EBRSessionState::Idle;

	bool bIsHost = false;

	bool bLocalLeaveRequested = false;

	bool bDisconnectNoticeLatched = false;

	FBRJoinTarget ActiveJoinTarget;
	FBRDisconnectNotice LastDisconnectNotice;
	FBRHostSessionParams PendingHostParams;
	FBRSessionQuery PendingQuery;

	FString PendingTravelMapPath;

	TSharedPtr<class FBRSessionSearchState> SearchState;

	FTimerHandle OperationTimeoutHandle;
	EBRSessionFailure TimeoutFailure = EBRSessionFailure::Timeout;
	FString TimeoutCode;

	FDelegateHandle PreLoginHandle;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
	FDelegateHandle MatchStateHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle HostingStateHandle;
};
