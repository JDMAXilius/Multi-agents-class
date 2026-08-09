#include "UI/OSMainMenu.h"

#include "UI/Components/OSBaseCommonButton.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSessionSettings.h"
#include "Subsystems/OSSessionsSubsystem.h"
#include "TimerManager.h"
#include "AkAudioEvent.h"
#include "AkGameplayStatics.h"
#include "AkGameplayTypes.h"

// --- Lifecycle ---

void UOSMainMenu::NativeConstruct()
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] NativeConstruct"));
	Super::NativeConstruct();

	if (UGameInstance* GameInstance = GetGameInstance())
		SessionsSubsystem = GameInstance->GetSubsystem<UOSSessionsSubsystem>();

	if (!ensureMsgf(SessionsSubsystem != nullptr, TEXT("OSSessionsSubsystem not found")))
		return;

	BindSessionDelegates();
	BindButtonHandlers();

	if (MenuStack)
	{
		MenuStack->OnDisplayedWidgetChanged().AddUObject(this, &UOSMainMenu::OnMenuStackDisplayedWidgetChanged);
		OnMenuStackDisplayedWidgetChanged(MenuStack->GetActiveWidget());
	}
}

void UOSMainMenu::NativeDestruct()
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] NativeDestruct"));

	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(DelayedActionTimer);

	UnbindSessionDelegates();
	UnbindButtonHandlers();

	if (MenuStack)
		MenuStack->OnDisplayedWidgetChanged().RemoveAll(this);

	Super::NativeDestruct();
}

void UOSMainMenu::NativeOnActivated()
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] NativeOnActivated"));
	Super::NativeOnActivated();

	HideError();

	if (HostButton)
		HostButton->SetIsEnabled(true);
	if (JoinButton)
		JoinButton->SetIsEnabled(true);
	if (QuitButton)
		QuitButton->SetIsEnabled(true);
}

void UOSMainMenu::NativeOnDeactivated()
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] NativeOnDeactivated"));
	Super::NativeOnDeactivated();
}

UWidget* UOSMainMenu::NativeGetDesiredFocusTarget() const
{
	if (HostButton)
		return HostButton;
	if (JoinButton)
		return JoinButton;
	return QuitButton;
}

// --- Wiring ---

void UOSMainMenu::BindSessionDelegates()
{
	if (!SessionsSubsystem)
		return;

	SessionsSubsystem->OnFindSessionsComplete.RemoveAll(this);
	SessionsSubsystem->OnSessionFlowError.RemoveAll(this);
	SessionsSubsystem->OnTravelStarted.RemoveAll(this);

	SessionsSubsystem->OnFindSessionsComplete.AddUObject(this, &UOSMainMenu::OnFindSessionsComplete);
	SessionsSubsystem->OnSessionFlowError.AddDynamic(this, &UOSMainMenu::OnSessionFlowError);
	SessionsSubsystem->OnTravelStarted.AddDynamic(this, &UOSMainMenu::OnTravelStarted);
}

void UOSMainMenu::UnbindSessionDelegates()
{
	if (!SessionsSubsystem)
		return;

	SessionsSubsystem->OnFindSessionsComplete.RemoveAll(this);
	SessionsSubsystem->OnSessionFlowError.RemoveAll(this);
	SessionsSubsystem->OnTravelStarted.RemoveAll(this);
}

void UOSMainMenu::BindButtonHandlers()
{
	if (HostButton)
	{
		HostButton->OnClicked().RemoveAll(this);
		HostButton->OnClicked().AddUObject(this, &UOSMainMenu::OnHostButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked().RemoveAll(this);
		JoinButton->OnClicked().AddUObject(this, &UOSMainMenu::OnJoinButtonClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked().RemoveAll(this);
		QuitButton->OnClicked().AddUObject(this, &UOSMainMenu::OnQuitButtonClicked);
	}
	if (OptionsButton)
	{
		OptionsButton->OnClicked().RemoveAll(this);
		OptionsButton->OnClicked().AddUObject(this, &UOSMainMenu::OnOptionsButtonClicked);
	}
	if (CharacterCreatorButton)
	{
		CharacterCreatorButton->OnClicked().RemoveAll(this);
		CharacterCreatorButton->OnClicked().AddUObject(this, &UOSMainMenu::OnCharacterCreatorButtonClicked);
	}
}

void UOSMainMenu::UnbindButtonHandlers()
{
	if (HostButton)
		HostButton->OnClicked().RemoveAll(this);
	if (JoinButton)
		JoinButton->OnClicked().RemoveAll(this);
	if (QuitButton)
		QuitButton->OnClicked().RemoveAll(this);
	if (OptionsButton)
		OptionsButton->OnClicked().RemoveAll(this);
	if (CharacterCreatorButton)
		CharacterCreatorButton->OnClicked().RemoveAll(this);
}

// --- Navigation ---

UCommonActivatableWidget* UOSMainMenu::PushActivatableOnOwningStack(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OSMainMenu] PushActivatable: null class"));
		return nullptr;
	}

	if (MenuStack)
	{
		UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] Pushing %s onto bound MenuStack"), *WidgetClass->GetName());
		return MenuStack->AddWidget<UCommonActivatableWidget>(WidgetClass);
	}

	UWidget* Current = GetParent();
	while (Current)
	{
		if (UCommonActivatableWidgetContainerBase* Container = Cast<UCommonActivatableWidgetContainerBase>(Current))
		{
			UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] Pushing %s onto %s"), *WidgetClass->GetName(), *Container->GetName());
			return Container->AddWidget<UCommonActivatableWidget>(WidgetClass);
		}
		Current = Current->GetParent();
	}

	UE_LOG(LogTemp, Warning, TEXT("[OSMainMenu] PushActivatable: no bound MenuStack and no CommonActivatableWidgetContainerBase in parent chain"));
	return nullptr;
}

void UOSMainMenu::PushHostMenu()
{
	if (!HostMenuClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OSMainMenu] HostMenuClass is not set"));
		return;
	}
	PushActivatableOnOwningStack(HostMenuClass);
}

// --- Audio ---

static float GetAkEventDelaySeconds(const UAkAudioEvent* Event, float FallbackDelay)
{
	if (!Event || Event->IsInfinite)
		return FallbackDelay;

	const float MaxDur = Event->MaximumDuration;
	const float MinDur = Event->MinimumDuration;

	const float Dur = (MaxDur > 0.f) ? MaxDur : ((MinDur > 0.f) ? MinDur : 0.f);
	return (Dur > 0.f) ? Dur : FallbackDelay;
}

void UOSMainMenu::PlayUISFX(UAkAudioEvent* EventToPlay, const FTimerDelegate& AfterSFXDelegate)
{
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(DelayedActionTimer);

	if (!EventToPlay)
	{
		AfterSFXDelegate.Execute();
		return;
	}

	UAkGameplayStatics::PostEvent(EventToPlay, nullptr, 0, FOnAkPostEventCallback());

	const float DelaySeconds = GetAkEventDelaySeconds(EventToPlay, AudioFallbackDelay);

	if (UWorld* World = GetWorld())
		World->GetTimerManager().SetTimer(DelayedActionTimer, AfterSFXDelegate, DelaySeconds, false);
}

// --- Submenu Visibility ---

void UOSMainMenu::OnMenuStackDisplayedWidgetChanged(UCommonActivatableWidget* NewWidget)
{
	const bool bSubmenuActive = (NewWidget != nullptr);
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] MenuStack displayed widget changed -> %s"),
		NewWidget ? *NewWidget->GetName() : TEXT("<none>"));

	if (MainMenuContent)
		MainMenuContent->SetVisibility(bSubmenuActive ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

// --- Unimplemented Menu Stub ---

void UOSMainMenu::LogUnimplementedMenu(const FString& MenuName)
{
	UE_LOG(LogTemp, Warning, TEXT("[OSMainMenu] Unimplemented menu button pressed: %s"), *MenuName);
#if !UE_BUILD_SHIPPING
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
			FString::Printf(TEXT("[Menu] '%s' not implemented yet"), *MenuName));
#endif
}

// --- Button Handlers ---

void UOSMainMenu::OnOptionsButtonClicked()
{
	LogUnimplementedMenu(TEXT("Options"));
}

void UOSMainMenu::OnCharacterCreatorButtonClicked()
{
	LogUnimplementedMenu(TEXT("Character Creator"));
}

void UOSMainMenu::OnHostButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] HostButton clicked"));

	HideError();

	if (SFX_Click_Host)
		UAkGameplayStatics::PostEvent(SFX_Click_Host, nullptr, 0, FOnAkPostEventCallback());

	PushHostMenu();
}

void UOSMainMenu::OnJoinButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] JoinButton clicked"));

	if (!SessionsSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OSMainMenu] JoinButton: SessionsSubsystem is null"));
		return;
	}

	HideError();

	if (!SessionsSubsystem->IsSteamConnected())
	{
		UE_LOG(LogTemp, Warning, TEXT("[OSMainMenu] JoinButton: Steam not connected"));
#if !UE_BUILD_SHIPPING
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Steam not connected - Please ensure Steam is running and logged in"));
#endif
		return;
	}

#if !UE_BUILD_SHIPPING
	const FString PlayerName = SessionsSubsystem->GetSteamPlayerName();
	if (!PlayerName.IsEmpty() && GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("Logged in as %s"), *PlayerName));
#endif

	if (JoinButton)
		JoinButton->SetIsEnabled(false);

#if !UE_BUILD_SHIPPING
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Searching for sessions..."));
#endif

	const bool bUseLAN = bDefaultUseLAN ? true : SessionsSubsystem->ShouldUseLANMode();
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] JoinButton - LAN mode: %s"), bUseLAN ? TEXT("true") : TEXT("false"));

	TWeakObjectPtr<UOSMainMenu> WeakThis(this);
	FTimerDelegate AfterSFX;
	AfterSFX.BindLambda([WeakThis, bUseLAN]()
	{
		if (!WeakThis.IsValid() || !WeakThis->SessionsSubsystem)
			return;
		WeakThis->SessionsSubsystem->FindSessions(1000, bUseLAN, FString(), FString());
	});

	PlayUISFX(SFX_Click_Join, AfterSFX);
}

void UOSMainMenu::OnQuitButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] QuitButton clicked"));

	if (QuitButton)
		QuitButton->SetIsEnabled(false);

	TWeakObjectPtr<UOSMainMenu> WeakThis(this);
	FTimerDelegate AfterSFX;
	AfterSFX.BindLambda([WeakThis]()
	{
		if (!WeakThis.IsValid())
			return;
		if (UWorld* World = WeakThis->GetWorld())
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
				PC->ConsoleCommand(TEXT("quit"));
		}
	});

	PlayUISFX(SFX_Click_Quit, AfterSFX);
}

// --- Session Callbacks ---

FString UOSMainMenu::GetResultMatchType(const FOnlineSessionSearchResult& Result)
{
	FString MT;
	Result.Session.SessionSettings.Get(FName("OnSightMatchType"), MT);
	if (MT.IsEmpty())
		Result.Session.SessionSettings.Get(FName("MatchType"), MT);
	return MT;
}

const FOnlineSessionSearchResult* UOSMainMenu::PickSessionToJoin(const TArray<FOnlineSessionSearchResult>& Results) const
{
	for (const FOnlineSessionSearchResult& Result : Results)
	{
		const FString MatchType = GetResultMatchType(Result);
		if (DefaultMatchType.IsEmpty() || MatchType == DefaultMatchType)
		{
			UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] Auto-joining session — MatchType: %s (preference: %s)"),
				*MatchType, DefaultMatchType.IsEmpty() ? TEXT("any") : *DefaultMatchType);
			return &Result;
		}
	}

	if (Results.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] No session with MatchType '%s'; falling back to first OnSight session"),
			*DefaultMatchType);
		return &Results[0];
	}
	return nullptr;
}

void UOSMainMenu::HandleFindSessionsFailure(bool bWasSuccessful)
{
	FText Msg;
	if (!bWasSuccessful && SessionsSubsystem && !SessionsSubsystem->IsSteamConnected())
		Msg = FText::FromString(TEXT("Steam not connected — please ensure Steam is running and logged in"));
	else if (!bWasSuccessful)
		Msg = FText::FromString(TEXT("Session search failed — check your connection and try again"));
	else
		Msg = FText::FromString(TEXT("No sessions found — try again"));

	OnSessionError(Msg);

#if !UE_BUILD_SHIPPING
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, Msg.ToString());
#endif

	if (JoinButton)
		JoinButton->SetIsEnabled(true);
}

void UOSMainMenu::OnFindSessionsComplete(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] OnFindSessionsComplete - Result: %s, Found: %d"),
		bWasSuccessful ? TEXT("Success") : TEXT("Failed"), SessionResults.Num());

	if (!SessionsSubsystem)
	{
		if (JoinButton)
			JoinButton->SetIsEnabled(true);
		return;
	}

	if (!bWasSuccessful || SessionResults.Num() == 0)
	{
		HandleFindSessionsFailure(bWasSuccessful);
		return;
	}

	const FOnlineSessionSearchResult* Chosen = PickSessionToJoin(SessionResults);
	if (!Chosen)
	{
		HandleFindSessionsFailure(bWasSuccessful);
		return;
	}

#if !UE_BUILD_SHIPPING
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Joining session..."));
#endif
	SessionsSubsystem->JoinAndTravel(*Chosen);
}

void UOSMainMenu::OnSessionFlowError(const FText& ErrorMessage)
{
	UE_LOG(LogTemp, Warning, TEXT("[OSMainMenu] Session flow error: %s"), *ErrorMessage.ToString());

	if (HostButton)
		HostButton->SetIsEnabled(true);
	if (JoinButton)
		JoinButton->SetIsEnabled(true);

	OnSessionError(ErrorMessage);
}

void UOSMainMenu::OnTravelStarted()
{
	UE_LOG(LogTemp, Log, TEXT("[OSMainMenu] Travel started — deactivating menu"));
	DeactivateWidget();
}

void UOSMainMenu::OnSessionError_Implementation(const FText& ErrorMessage)
{
	UE_LOG(LogTemp, Warning, TEXT("[OSMainMenu] Session error: %s"), *ErrorMessage.ToString());

	if (ErrorText)
	{
		ErrorText->SetText(ErrorMessage);
		ErrorText->SetVisibility(ESlateVisibility::Visible);
	}
}

// --- Visibility Helpers ---

void UOSMainMenu::HideError()
{
	if (ErrorText)
		ErrorText->SetVisibility(ESlateVisibility::Hidden);
}
