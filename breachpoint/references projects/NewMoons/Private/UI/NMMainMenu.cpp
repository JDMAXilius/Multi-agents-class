// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/NMMainMenu.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystemUtils.h"
#include "Components/ListView.h"
#include "Components/ComboBoxString.h"
#include "Multiplayer/Data/NMSubsytemData.h"
#include "Multiplayer/NMSessionsSubsystem.h"
#include "UI/NMRowSession.h"
#include "Game/NMGameMode.h"

void UNMMainMenu::NativePreConstruct()
{
	Super::NativePreConstruct();

	LevelsToHost.Sort([](const TSoftObjectPtr<UWorld> Level1, const TSoftObjectPtr<UWorld> Level2)
	{
		return Level1.GetAssetName() < Level2.GetAssetName();
	});
	
	LevelToHostDropDown->ClearOptions();

	for (const auto& Level : LevelsToHost)
	{
		LevelToHostDropDown->AddOption(Level.GetAssetName());
	}

	LevelToHostDropDown->SetSelectedIndex(0);
}

void UNMMainMenu::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		NMSessionsSubsystem = GameInstance->GetSubsystem<UNMSessionsSubsystem>();
	}

	if (NMSessionsSubsystem)
	{
		NMSessionsSubsystem->OnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		NMSessionsSubsystem->OnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessions);
		NMSessionsSubsystem->OnJoinSessionComplete.AddDynamic(this, &ThisClass::OnJoinSession);
	}
}

bool UNMMainMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HostButtonClicked);
	}
	if (FindButton)
	{
		FindButton->OnClicked.AddDynamic(this, &ThisClass::FindButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}
	if (QuickPlayButton)
	{
		QuickPlayButton->OnClicked.AddDynamic(this, &ThisClass::QuickPlayButtonClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddDynamic(this, &ThisClass::BackButtonClicked);
	}

	return true;
	
}

void UNMMainMenu::OnCreateSession(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		MenuClose();
		FString WelcomeMessage = FString::Printf(TEXT("Success to create session!"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, WelcomeMessage);
		
		if (UWorld* World = GetWorld())
		{
			const FString DestinationURL = FString::Printf(TEXT("%s?Game=%s?listen"), *LevelToHostDropDown->GetSelectedOption(), *GameModeToHost.Get()->GetPathName());
			World->ServerTravel(DestinationURL, true);
		}
	}
	else
	{
		FString WelcomeMessage = FString::Printf(TEXT("Failed to create session!"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, WelcomeMessage);
		HostButton->SetIsEnabled(true);
	}
}

void UNMMainMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	if (NMSessionsSubsystem == nullptr) return;
	if (bWasSuccessful)
	{
		UpdateServerList(SessionResults);
	}
	if (!bWasSuccessful || SessionResults.Num() == 0)
	{
		FString WelcomeMessage = FString::Printf(TEXT("Fail to Find Session!"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, WelcomeMessage);
		JoinButton->SetIsEnabled(true);
	}
}

void UNMMainMenu::OnJoinSession(bool bWasSuccessful, EEOSJoinSessionResultType Type, const FString Address)
{
	if (bWasSuccessful)
	{
		FString WelcomeMessage = FString::Printf(TEXT("Session Available!"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, WelcomeMessage);
		
		MenuClose();

		if (APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController())
		{
			PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
		}
	}
	else
	{
		FString WelcomeMessage = FString::Printf(TEXT("No Session Available!"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, WelcomeMessage);
	}
}

void UNMMainMenu::UpdateServerList(const TArray<FOnlineSessionSearchResult>& SearchResults)
{
	if (ServerListView)
	{
		ServerListView->ClearListItems();

		for (const auto& SearchResult : SearchResults)
		{
			if (UNMRowSession* ServerRow = CreateWidget<UNMRowSession>(this, UNMRowSession::StaticClass()))
			{
				ServerRow->SetServerData(SearchResult);
				ServerListView->AddItem(ServerRow);
			}
		}
	}
}

void UNMMainMenu::FindButtonClicked()
{
	//JoinButton->SetIsEnabled(false);
	if (NMSessionsSubsystem)
	{
		NMSessionsSubsystem->FindSessions(200'000, LANCheckBox->IsChecked());
	}
}

void UNMMainMenu::HostButtonClicked()
{
	HostButton->SetIsEnabled(false);
	if (NMSessionsSubsystem)
	{
		NMSessionsSubsystem->CreateSession(NumPublicConnections, MatchType, NMGameSession.ToString(), false, false, LANCheckBox->IsChecked());
	}
}

void UNMMainMenu::JoinButtonClicked() 
{
	if (!ServerListView->GetSelectedItem()) return;

	if (const UNMRowSession* SelectedServerRow = Cast<UNMRowSession>(ServerListView->GetSelectedItem()))
	{
		NMSessionsSubsystem->JoinSession(SelectedServerRow->GetSearchResult());
	}
}
void UNMMainMenu::QuickPlayButtonClicked()
{
	// Get the total number of items in the ServerListView
	int32 TotalItems = ServerListView->GetNumItems();
    
	// Check if there are any items in the list
	if (TotalItems == 0) return;

	// Select a random index within the range of available items
	int32 RandomIndex = FMath::RandRange(0, TotalItems - 1);

	if (const UNMRowSession* SelectedServerRow = Cast<UNMRowSession>(ServerListView->GetItemAt(RandomIndex)))
	{
		NMSessionsSubsystem->JoinSession(SelectedServerRow->GetSearchResult());
	}
}

void UNMMainMenu::BackButtonClicked()
{
	//// Get the player controller to handle quitting
	//if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	//{
	//	// If running in standalone or packaged game, use Console Command to quit
	//	PlayerController->ConsoleCommand("quit");
	//}
	//else
	//{
	//	// If running in editor, you might want to just exit the current level
	//	UE_LOG(LogTemp, Warning, TEXT("Quit button clicked - Exiting Game"));
	//       
	//	// If in editor, just close the PIE session
	//	if (GEngine)
	//	{
	//		GEngine->Exec(GetWorld(), TEXT("QUIT_EDITOR"));
	//	}
	//}

	RemoveFromParent();
}

void UNMMainMenu::MenuClose()
{
	//RemoveFromParent();
	
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}
