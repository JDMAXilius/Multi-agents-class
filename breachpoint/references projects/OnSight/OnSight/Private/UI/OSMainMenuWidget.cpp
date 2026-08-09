#include "UI/OSMainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Subsystems/OSSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "AkGameplayStatics.h"
#include "AkGameplayTypes.h"
#include "AkAudioEvent.h"

// ========================================
// WIDGET LIFECYCLE
// ========================================

void UOSMainMenuWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (HostMenuWidget)
		HostMenuWidget->SetVisibility(ESlateVisibility::Collapsed);

	if (BT_Ready)
		BT_Ready->SetVisibility(ESlateVisibility::Collapsed);
}

void UOSMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Setup input mode for menu
	SetupInputMode();

	// Get sessions subsystem
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		SessionsSubsystem = GameInstance->GetSubsystem<UOSSessionsSubsystem>();
	}

	if (!ensureMsgf(SessionsSubsystem != nullptr, TEXT("OSSessionsSubsystem not found")))
	{
		return;
	}

	/* Only the root menu widget binds session delegates. OSMapSelect is a child widget
	   that also inherits from OSMainMenuWidget: if both bind, the callback fires twice.
	   The root menu owns JoinButton; the embedded MapSelect does not. */
	if (JoinButton)
	{
		SessionsSubsystem->OnFindSessionsComplete.RemoveAll(this);
		SessionsSubsystem->OnSessionFlowError.RemoveAll(this);
		SessionsSubsystem->OnTravelStarted.RemoveAll(this);

		SessionsSubsystem->OnFindSessionsComplete.AddUObject(this, &UOSMainMenuWidget::OnFindSessionsComplete);
		SessionsSubsystem->OnSessionFlowError.AddDynamic(this, &UOSMainMenuWidget::OnSessionFlowError);
		SessionsSubsystem->OnTravelStarted.AddDynamic(this, &UOSMainMenuWidget::OnTravelStarted);
	}

	// Bind button click handlers (clear first to prevent double-binding on re-entry)
	if (HostButton)
	{
		HostButton->OnClicked.RemoveAll(this);
		HostButton->OnClicked.AddDynamic(this, &UOSMainMenuWidget::OnHostButtonClicked);
	}

	if (JoinButton)
	{
		JoinButton->OnClicked.RemoveAll(this);
		JoinButton->OnClicked.AddDynamic(this, &UOSMainMenuWidget::OnJoinButtonClicked);
	}

	if (QuitButton)
	{
		QuitButton->OnClicked.RemoveAll(this);
		QuitButton->OnClicked.AddDynamic(this, &UOSMainMenuWidget::OnQuitButtonClicked);
	}

	// Host menu and ready button start hidden
	if (HostMenuWidget)
		HostMenuWidget->SetVisibility(ESlateVisibility::Collapsed);

	if (BT_Ready)
		BT_Ready->SetVisibility(ESlateVisibility::Collapsed);
}

void UOSMainMenuWidget::NativeDestruct()
{
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DelayedActionTimer);
	}
	
	// Unbind from subsystem delegates (only root menu binds, see NativeConstruct)
	if (SessionsSubsystem && JoinButton)
	{
		SessionsSubsystem->OnFindSessionsComplete.RemoveAll(this);
		SessionsSubsystem->OnSessionFlowError.RemoveAll(this);
		SessionsSubsystem->OnTravelStarted.RemoveAll(this);
	}

	// Unbind button click handlers
	if (HostButton)
		HostButton->OnClicked.RemoveAll(this);
	if (JoinButton)
		JoinButton->OnClicked.RemoveAll(this);
	if (QuitButton)
		QuitButton->OnClicked.RemoveAll(this);

	// Restore input mode
	RestoreInputMode();

	Super::NativeDestruct();
}

namespace {
//gets duration of the Wwise Audio Event
static float GetAkEventDelaySeconds(const UAkAudioEvent* Event, float FallbackDelay)
{
	if (!Event)
	{
		return FallbackDelay;
	}
	
	if (Event->IsInfinite)
	{
		return FallbackDelay;
	}
	
	const float MaxDur = Event->MaximumDuration;
	const float MinDur = Event->MinimumDuration;
	
	const float Dur = (MaxDur > 0.f) ? MaxDur : ((MinDur > 0.f) ? MinDur : 0.f);
	return (Dur > 0.f) ? Dur : FallbackDelay;
}
} // anonymous namespace

void UOSMainMenuWidget::PlayUISFX(UAkAudioEvent* EventToPlay, const FTimerDelegate& AfterSFXDelegate)
{
	// if (UWorld* World = GetWorld())
	// {
	// 	World->GetTimerManager().ClearTimer(DelayedActionTimer);
	// }
	//
	// if (!EventToPlay)
	// {
	// 	AfterSFXDelegate.Execute();
	// 	return;
	// }
	//
	// UAkGameplayStatics::PostEvent(EventToPlay, nullptr, 0, FOnAkPostEventCallback());
	//
	// const float DelaySeconds = GetAkEventDelaySeconds(EventToPlay, AudioFallbackDelay);
	//
	// if (UWorld* World = GetWorld())
	// {
	// 	World->GetTimerManager().SetTimer(DelayedActionTimer, AfterSFXDelegate, DelaySeconds, false);
	// }
}

// ========================================
// BUTTON HANDLERS
// ========================================

void UOSMainMenuWidget::OnHostButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] HostButton clicked - showing host menu"));

	if (ErrorText)
		ErrorText->SetVisibility(ESlateVisibility::Hidden);

	if (SFX_Click_Host)
		UAkGameplayStatics::PostEvent(SFX_Click_Host, nullptr, 0, FOnAkPostEventCallback());

	if (MainMenuContent)
		MainMenuContent->SetVisibility(ESlateVisibility::Collapsed);

	if (LogoImage)
		LogoImage->SetVisibility(ESlateVisibility::Collapsed);

	if (HostMenuWidget)
		HostMenuWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UOSMainMenuWidget::OnJoinButtonClicked()
{
	if (!SessionsSubsystem)
	{
		return;
	}

	// Clear any previous error — player is trying again
	if (ErrorText)
	{
		ErrorText->SetVisibility(ESlateVisibility::Hidden);
	}

	if (!SessionsSubsystem->IsSteamConnected())
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Steam not connected - Please ensure Steam is running and logged in"));
		}
#endif
		return;
	}
	else
	{
		// Get and display player name
		FString PlayerName = SessionsSubsystem->GetSteamPlayerName();
		if (!PlayerName.IsEmpty())
		{
#if !UE_BUILD_SHIPPING
			if (GEngine)
			{
				FString Message = FString::Printf(TEXT("Logged in as %s"), *PlayerName);
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, Message);
			}
#endif
		}
	}

	// Disable button during search
	if (JoinButton)
	{
		JoinButton->SetIsEnabled(false);
	}

#if !UE_BUILD_SHIPPING
	// Show searching message
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Searching for sessions..."));
	}
#endif

	// Determine LAN mode: use editor variable if true, otherwise use automatic detection
	bool bUseLAN = bDefaultUseLAN ? true : SessionsSubsystem->ShouldUseLANMode();
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] JoinButton - Using LAN mode: %s"), bUseLAN ? TEXT("true") : TEXT("false"));

	// Find sessions (will auto-join on completion). Do not filter by match type or session name at OSS:
	// Steam returns only OnSight lobbies (OnSightGame key); client picks preferred match type or first result.
	TWeakObjectPtr<UOSMainMenuWidget> WeakThis(this);
	
	FTimerDelegate AfterSFX;
	AfterSFX.BindLambda([WeakThis, bUseLAN]()
{
	if (!WeakThis.IsValid())
	{
		return;
	}
		
	UOSMainMenuWidget* This = WeakThis.Get();
	if (!This->SessionsSubsystem)
	{
		return;
	}
		
	This->SessionsSubsystem->FindSessions(1000, bUseLAN, FString(), FString());
});
	PlayUISFX(SFX_Click_Join, AfterSFX);
	
}

void UOSMainMenuWidget::OnQuitButtonClicked()
{
	/*
	 if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		// Quit the game
		PlayerController->ConsoleCommand(TEXT("quit"));
	}
	*/
	
	if (QuitButton)
	{
		QuitButton->SetIsEnabled(false);
	}
	
	TWeakObjectPtr<UOSMainMenuWidget> WeakThis(this);
	FTimerDelegate AfterSFX;
	AfterSFX.BindLambda([WeakThis]()
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		
		if (APlayerController* PlayerController = WeakThis->GetWorld() ? WeakThis->GetWorld()->GetFirstPlayerController() : nullptr)
		{
			PlayerController->ConsoleCommand(TEXT("quit"));
		}
	});
	
	PlayUISFX(SFX_Click_Quit, AfterSFX);
}

	// ========================================
	// SESSION SUBSYSTEM CALLBACKS
	// ========================================

void UOSMainMenuWidget::OnFindSessionsComplete(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] OnFindSessionsComplete - Result: %s, Found: %d sessions"), 
		bWasSuccessful ? TEXT("Success") : TEXT("Failed"), SessionResults.Num());

	if (!SessionsSubsystem)
	{
		if (JoinButton)
		{
			JoinButton->SetIsEnabled(true);
		}
		return;
	}

	if (bWasSuccessful && SessionResults.Num() > 0)
	{
		auto GetResultMatchType = [](const FOnlineSessionSearchResult& R) -> FString
		{
			FString MT;
			R.Session.SessionSettings.Get(FName("OnSightMatchType"), MT);
			if (MT.IsEmpty())
			{
				R.Session.SessionSettings.Get(FName("MatchType"), MT);
			}
			return MT;
		};

		const FOnlineSessionSearchResult* Chosen = nullptr;
		for (const FOnlineSessionSearchResult& Result : SessionResults)
		{
			const FString SettingsMatchType = GetResultMatchType(Result);
			if (DefaultMatchType.IsEmpty() || SettingsMatchType == DefaultMatchType)
			{
				Chosen = &Result;
				UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] Auto-joining session — MatchType: %s (preference: %s)"),
					*SettingsMatchType, DefaultMatchType.IsEmpty() ? TEXT("any") : *DefaultMatchType);
				break;
			}
		}

		if (!Chosen)
		{
			Chosen = &SessionResults[0];
			const FString FallbackType = GetResultMatchType(*Chosen);
			UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] No session with MatchType '%s'; joining first OnSight session (%s)"),
				*DefaultMatchType, *FallbackType);
		}

#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Joining session..."));
		}
#endif
		SessionsSubsystem->JoinAndTravel(*Chosen);
		return;
	}
	else
	{
		FText Msg;
		if (!bWasSuccessful && SessionsSubsystem && !SessionsSubsystem->IsSteamConnected())
		{
			Msg = FText::FromString(TEXT("Steam not connected — please ensure Steam is running and logged in"));
		}
		else if (!bWasSuccessful)
		{
			Msg = FText::FromString(TEXT("Session search failed — check your connection and try again"));
		}
		else
		{
			Msg = FText::FromString(TEXT("No sessions found — try again"));
		}

		OnSessionError(Msg);
#if !UE_BUILD_SHIPPING
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, Msg.ToString());
#endif
	}

	// Re-enable Join button for retry
	if (JoinButton)
	{
		JoinButton->SetIsEnabled(true);
	}
}

void UOSMainMenuWidget::OnSessionFlowError(const FText& ErrorMessage)
{
	UE_LOG(LogTemp, Warning, TEXT("[OSMainMenu] Session flow error: %s"), *ErrorMessage.ToString());

	// Re-enable buttons so the player can retry
	if (HostButton)
		HostButton->SetIsEnabled(true);
	if (JoinButton)
		JoinButton->SetIsEnabled(true);

	OnSessionError(ErrorMessage);
}

void UOSMainMenuWidget::OnTravelStarted()
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] Travel started, closing menu"));
	CloseMenu();
}

// ========================================
// SESSION ERROR FEEDBACK
// ========================================

void UOSMainMenuWidget::OnSessionError_Implementation(const FText& ErrorMessage)
{
	UE_LOG(LogTemp, Warning, TEXT("[OSMainMenu] Session error: %s"), *ErrorMessage.ToString());

	if (ErrorText)
	{
		ErrorText->SetText(ErrorMessage);
		ErrorText->SetVisibility(ESlateVisibility::Visible);
		// Error stays visible until the player acts — cleared at the top of each button handler.
	}
}

// ========================================
// INPUT MODE MANAGEMENT
// ========================================

void UOSMainMenuWidget::SetupInputMode()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			// Set UI-only input mode
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}
}

void UOSMainMenuWidget::RestoreInputMode()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			// Restore game input mode
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}

// ========================================
// NAVIGATION
// ========================================

void UOSMainMenuWidget::CloseMenu()
{
	// Remove from viewport
	RemoveFromParent();

	// Restore input mode
	RestoreInputMode();
}

void UOSMainMenuWidget::ShowMainMenu()
{
	if (HostMenuWidget)
		HostMenuWidget->SetVisibility(ESlateVisibility::Collapsed);

	if (MainMenuContent)
		MainMenuContent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (LogoImage)
		LogoImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}



