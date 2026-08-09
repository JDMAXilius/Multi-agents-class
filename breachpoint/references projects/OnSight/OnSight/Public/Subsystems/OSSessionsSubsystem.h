#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Data/OSSessionsData.h"
#include "TimerManager.h"
#include "OSSessionsSubsystem.generated.h"

// Forward declarations
class FOnlineSessionSearch;
class FOnlineSessionSearchResult;
class IOnlineIdentity;
class IOnlinePresence;
class FUniqueNetId;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOSOnLoginComplete, bool, bWasSuccessful, const FString, ErrorString);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSOnCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOSOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOSOnJoinSessionComplete, bool, bWasSuccessful, OSJoinSessionResultType, ResultType, const FString, Address);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSOnEndSessionComplete, bool, bWasSuccessful);

// --- High-level flow delegates (for UI) ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOSOnTravelStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSOnSessionFlowError, const FText&, ErrorMessage);

/**
 * GameInstance Subsystem for managing online sessions
 * Handles session creation, discovery, joining, and lifecycle management
 */
UCLASS()
class ONSIGHT_API UOSSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UOSSessionsSubsystem();
	
	/** Initialize the subsystem */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Deinitialize the subsystem */
	virtual void Deinitialize() override;

	// --- High-level flows (UI calls these) ---

	/* Create session, start it, and ServerTravel to the given URL.
	   TravelURL should already include ?game= and map path. */
	void HostAndTravel(const FString& TravelURL, const FOSHostMatchSettings& MatchSettings, const FString& SessionName, bool bIsLANMatch);

	/* Join the given session and ClientTravel to its resolved address on success. */
	void JoinAndTravel(const FOnlineSessionSearchResult& SessionResult);

	// --- Low-level session operations ---

	void CreateSession(int32 NumPublicConnections, FString MatchType, const FString& SessionName, bool bIsLANMatch);
	void CreateSession(const FOSHostMatchSettings& MatchSettings, const FString& SessionName, bool bIsLANMatch);
	void FindSessions(int32 MaxSearchResults, bool bIsLANMatch, const FString& MatchTypeFilter = FString(), const FString& SessionNameFilter = FString());
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	void StartSession();
	void EndSession();
	void DestroySession();
	bool IsValidSessionInterface();

	/** Determines if LAN mode should be used based on subsystem, environment, and command line */
	bool ShouldUseLANMode() const;
	
	/** Checks if the user is logged into Steam/Online Subsystem */
	bool IsSteamConnected() const;
	
	/** Gets the Steam player nickname */
	UFUNCTION(BlueprintCallable) // did this for a temp change
	FString GetSteamPlayerName() const;

	/** Delegate fired when session creation completes */
	UPROPERTY(BlueprintAssignable, Category = "OnSight|Sessions|Callbacks")
	FOSOnCreateSessionComplete OnCreateSessionComplete;
	/** Delegate fired when session search completes */
	FOSOnFindSessionsComplete OnFindSessionsComplete;
	/** Delegate fired when session join completes */
	UPROPERTY(BlueprintAssignable, Category = "OnSight|Sessions|Callbacks")
	FOSOnJoinSessionComplete OnJoinSessionComplete;
	/** Delegate fired when session start completes */
	UPROPERTY(BlueprintAssignable, Category = "OnSight|Sessions|Callbacks")
	FOSOnCreateSessionComplete OnStartSessionComplete;
	/** Delegate fired when session end completes */
	UPROPERTY(BlueprintAssignable, Category = "OnSight|Sessions|Callbacks")
	FOSOnEndSessionComplete OnEndSessionComplete;
	/** Delegate fired when session destroy completes */
	UPROPERTY(BlueprintAssignable, Category = "OnSight|Sessions|Callbacks")
	FOSOnCreateSessionComplete OnDestroySessionComplete;

	// --- High-level flow delegates ---

	/* Fired when HostAndTravel or JoinAndTravel begins travel. UI should close the menu. */
	UPROPERTY(BlueprintAssignable, Category = "OnSight|Sessions|Flow")
	FOSOnTravelStarted OnTravelStarted;

	/* Fired when any session flow hits an error. UI should display the message. */
	UPROPERTY(BlueprintAssignable, Category = "OnSight|Sessions|Flow")
	FOSOnSessionFlowError OnSessionFlowError;


	/** Get the last session settings that were created */
	TSharedPtr<FOnlineSessionSettings> GetLastSessionSettings() const { return LastSessionSettings; }
	/** Get the last session search results */
	TSharedPtr<FOnlineSessionSearch> GetLastSessionSearch() const { return LastSessionSearch; }

	/** True while a HostAndTravel or JoinAndTravel flow is in progress. */
	bool IsSessionFlowInProgress() const { return bHostFlowInProgress || bJoinFlowInProgress; }

protected:
	
	void OnCreateSessionCompleteInternal(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsCompleteInternal(bool bWasSuccessful);
	void OnJoinSessionCompleteInternal(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnStartSessionCompleteInternal(FName SessionName, bool bWasSuccessful);
	void OnEndSessionCompleteInternal(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionCompleteInternal(FName SessionName, bool bWasSuccessful);
	
	/** Retry FindSessions after delay — calls FindSessionsInternal to preserve retry count */
	void RetryFindSessions();

private:
	/** Core search logic shared by FindSessions (fresh) and RetryFindSessions (retry).
	 *  FindSessions resets retry state; FindSessionsInternal does not. */
	void FindSessionsInternal(int32 MaxSearchResults, bool bIsLANMatch, const FString& MatchTypeFilter, const FString& SessionNameFilter);



	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FDelegateHandle StartSessionCompleteDelegateHandle;
	FOnEndSessionCompleteDelegate EndSessionCompleteDelegate;
	FDelegateHandle EndSessionCompleteDelegateHandle;
	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	bool bCreateSessionOnDestroy{ false };
	bool bJoinSessionOnDestroy{ false };
	int32 LastNumPublicConnections;
	FString LastMatchType;
	FString LastSessionName;
	bool bLastSessionIsLAN;
	FOnlineSessionSearchResult PendingJoinSearchResult;
	
	// Retry logic for FindSessions
	int32 FindSessionsRetryCount{ 0 };
	const int32 MaxFindSessionsRetries{ 2 };
	FTimerHandle FindSessionsRetryTimerHandle;
	int32 PendingMaxSearchResults;
	bool PendingbIsLANMatch;
	FString PendingMatchTypeFilter;
	FString PendingSessionNameFilter;
	
	/** Prevents overlapping JoinSession OSS calls. */
	bool bJoinSessionInProgress = false;

	// --- High-level flow state ---
	bool bHostFlowInProgress = false;
	bool bJoinFlowInProgress = false;
	FString PendingHostTravelURL;

	void ExecuteHostTravel();
	void ExecuteJoinTravel(const FString& Address);
};

