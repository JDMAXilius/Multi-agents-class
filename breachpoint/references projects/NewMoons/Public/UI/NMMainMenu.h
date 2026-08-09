// Fill out your copyright notice in the Description page of Project Settings. 

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Multiplayer/Data/NMSubsytemData.h"
#include "NMMainMenu.generated.h"

class UButton;
class UCheckBox;
class UListView;
class UNMRowSession;
class UComboBoxString;
class UNMSessionsSubsystem;

UCLASS()
class NEWMOONS_API UNMMainMenu : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Server Menu", meta = (BindWidget))
	UCheckBox* LANCheckBox;
	
	UPROPERTY(BlueprintReadOnly, Category = "Server Menu", meta = (BindWidget))
	UListView* ServerListView;
	
	UPROPERTY(BlueprintReadOnly, Category = "Server Menu", meta = (BindWidget))
	UButton* HostButton;

	UPROPERTY(BlueprintReadOnly, Category = "Server Menu", meta = (BindWidget))
	UButton* FindButton;

	UPROPERTY(BlueprintReadOnly, Category = "Server Menu", meta = (BindWidget))
	UButton* JoinButton;

	UPROPERTY(BlueprintReadOnly, Category = "Server Menu", meta = (BindWidget))
	UButton* QuickPlayButton;
	
	UPROPERTY(BlueprintReadOnly, Category = "Server Menu", meta = (BindWidget))
	UButton* BackButton;

	UPROPERTY(BlueprintReadOnly, Category = "Server Menu", meta = (BindWidget))
	UComboBoxString* LevelToHostDropDown;

protected:
	virtual bool Initialize() override;

	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
	UFUNCTION()
	void OnJoinSession(bool bWasSuccessful, EEOSJoinSessionResultType Type, const FString Address);
	
	void UpdateServerList(const TArray<FOnlineSessionSearchResult>& SearchResults);

private:

	UFUNCTION()
	void HostButtonClicked();
	
	UFUNCTION()
	void FindButtonClicked();
	
	UFUNCTION()
	void JoinButtonClicked();
	 
	UFUNCTION()
	void QuickPlayButtonClicked();

	UFUNCTION()
	void BackButtonClicked();

	void MenuClose();

	// The subsystem designed to handle all online session functionality
	UNMSessionsSubsystem* NMSessionsSubsystem;
	
	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly, Category = "Server Menu",  meta = (AllowPrivateAccess = "true"))
	int32 NumPublicConnections{4};

	UPROPERTY(BlueprintReadWrite,EditDefaultsOnly, Category = "Server Menu",  meta = (AllowPrivateAccess = "true"))
	FString MatchType{TEXT("DeathMatch")};

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Server Menu",  meta = (AllowPrivateAccess = "true"))
	TArray<TSoftObjectPtr<UWorld>> LevelsToHost;


	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Server Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AGameModeBase> GameModeToHost;
};
