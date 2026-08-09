// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Multiplayer/Data/NMSubsytemData.h"
#include "NMSessionsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNMOnCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FNMOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FNMOnJoinSessionComplete, bool, bWasSuccessful, EEOSJoinSessionResultType, Type, const FString, Address);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNMOnDestroySessionComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNMOnEndSessionComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNMOnStartSessionComplete, bool, bWasSuccessful);

//Need to forward declare classes used 
class FOnlineSessionSearch;
class FOnlineSessionSearchResult;

UCLASS()
class NEWMOONS_API UNMSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UNMSessionsSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	// Public interface to start session creation
	UFUNCTION(BlueprintCallable, Category = "NM Subsystems|Sessions")
	void CreateSession(int32 NumPublicConnections, FString MatchType,const FString& SessionName, const bool bIsDedicatedServer, const bool bIsPrivate, const bool bIsLANMatch);
	UFUNCTION(BlueprintCallable, Category = "NM Subsystems|Sessions")
	void FindSessions(int32 MaxSearchResults, const bool bIsLANMatch);
	//UFUNCTION(BlueprintCallable, Category = "NM Subsystems|Sessions")
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	UFUNCTION(BlueprintCallable, Category = "NM Subsystems|Sessions")
	void DestroySession();
	UFUNCTION(BlueprintCallable, Category = "NM Subsystems|Sessions")
	void StartSession();
	// Function to end the session
	UFUNCTION(BlueprintCallable, Category = "NM Subsystems|Sessions")
	void EndSession();

	// Get the last session settings
	TSharedPtr<FOnlineSessionSettings> GetLastSessionSettings() { return LastSessionSettings; }
	
	//
	// Public delegates for class to bind callbacks
	//
	UPROPERTY(BlueprintAssignable, Category = "NM Subsystems|Sessions|Callback")
	FNMOnCreateSessionComplete OnCreateSessionComplete;
	//UPROPERTY(BlueprintAssignable, Category = "NM Subsystems|Sessions|Callback")
	FNMOnFindSessionsComplete OnFindSessionsComplete;
	UPROPERTY(BlueprintAssignable, Category = "NM Subsystems|Sessions|Callback")
	FNMOnJoinSessionComplete OnJoinSessionComplete;
	UPROPERTY(BlueprintAssignable, Category = "NM Subsystems|Sessions|Callback")
	FNMOnDestroySessionComplete OnDestroySessionComplete;
	UPROPERTY(BlueprintAssignable, Category = "NM Subsystems|Sessions|Callback")
	FNMOnEndSessionComplete OnEndSessionComplete;
	UPROPERTY(BlueprintAssignable, Category = "NM Subsystems|Sessions|Callback")
	FNMOnStartSessionComplete OnStartSessionComplete;

	int32 DesiredNumPublicConnections{};
	FString DesiredMatchType{};

protected:
	//
	// Internal callbacks for the delegates.
	//
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleEndSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleStartSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

private:

	bool bCreateSessionOnDestroy{ false };
	int32 LastNumPublicConnections;
	FString LastSessionName;
	FString LastMatchType;
	bool bLastSessionIsPrivate;
	bool bLastSessionIsLAN;

	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;
	
	//
	// Delegate Handles for the OnlineSubsystem session interfaces
	//
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	FDelegateHandle EndSessionCompleteDelegateHandle;
	FDelegateHandle StartSessionCompleteDelegateHandle;
	FDelegateHandle NetworkFailureDelegateHandle;
};
