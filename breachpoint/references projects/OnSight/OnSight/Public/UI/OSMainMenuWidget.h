#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OSMainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UOSSessionsSubsystem;
class FOnlineSessionSearchResult;

class UAkAudioEvent;

/**
 * Main Menu Widget for multiplayer session management
 * Handles Host Game, Find Sessions, and Quit functionality
 * Visual design handled in Widget Editor, logic in C++
 */
UCLASS()
class ONSIGHT_API UOSMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ========================================
	// WIDGET LIFECYCLE
	// ========================================

	/** Return to main menu view (show buttons/logo, hide host menu) */
	void ShowMainMenu();

	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ========================================
	// WIDGET BINDINGS (Widget Editor)
	// ========================================

	/** Host Game button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> HostButton;

	/** Join button (searches and auto-joins sessions) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> JoinButton;

	/** Quit Game button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	/** Error message text — name this "ErrorText" in the Widget Blueprint. Auto-cleared after 4 seconds. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ErrorText;

	// Container holding the main menu buttons (Host/Join/Quit). Hidden when host menu is shown.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MainMenuContent;

	// Logo image. Hidden when host menu is shown.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LogoImage;

	// Ready button. Hidden on construct.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> BT_Ready;

	// The host menu (WBP_HostMenuTemp) placed in the widget tree. Starts hidden, shown when Host is clicked.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> HostMenuWidget;

protected:
	// ========================================
	// BUTTON HANDLERS
	// ========================================

	/** Called when Host Game button is clicked */
	UFUNCTION()
	void OnHostButtonClicked();

	/** Called when Join button is clicked (searches and auto-joins) */
	UFUNCTION()
	void OnJoinButtonClicked();

	/** Called when Quit button is clicked */
	UFUNCTION()
	void OnQuitButtonClicked();

	// ========================================
	// SESSION SUBSYSTEM CALLBACKS
	// ========================================

	/** Called when session search completes. Widget picks a result and calls JoinAndTravel. */
	void OnFindSessionsComplete(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);

	/** Called by subsystem when a flow error occurs. Shows error text. */
	UFUNCTION()
	void OnSessionFlowError(const FText& ErrorMessage);

	/** Called by subsystem when travel begins. Closes the menu. */
	UFUNCTION()
	void OnTravelStarted();

	/**
	 * Called on any session failure with a player-facing message.
	 * Default C++ implementation sets ErrorText.
	 * Override in Blueprint for a custom popup or animation.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "OnSight|Session|Feedback")
	void OnSessionError(const FText& ErrorMessage);

	// ========================================
	// INPUT MODE MANAGEMENT
	// ========================================

	/** Setup UI-only input mode for menu */
	void SetupInputMode();

	/** Restore game input mode */
	void RestoreInputMode();

	// ========================================
	// NAVIGATION
	// ========================================

	/** Close menu and restore game input */
	void CloseMenu();

private:
	// ========================================
	// INTERNAL STATE
	// ========================================

	/** Reference to the sessions subsystem */
	UPROPERTY()
	TObjectPtr<UOSSessionsSubsystem> SessionsSubsystem;

	/** When non-empty, Join prefers this OnSightMatchType; if no match, joins first OnSight session anyway. Empty = any mode (recommended for playtests). */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Session Settings", meta=(DisplayName="Join Preference: OnSight Match Type"))
	FString DefaultMatchType;

	/** Default LAN mode (if false, uses automatic detection) */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Session Settings", meta=(DisplayName="Force LAN (Editor Override)"))
	bool bDefaultUseLAN = false;

protected:
	/** Map to travel to after session creation (package path format: /Game/Maps/MyMap) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OnSight|Session Settings|Level Travel")
	FString GameMapPath = TEXT("/Game/Maps/Deployable/MarketPlace/MarketPlace_P");
	
	// ========================================
	// AUDIO
	// ========================================
	
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Audio|UI")
	TObjectPtr<UAkAudioEvent> SFX_Click_Host = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Audio|UI")
	TObjectPtr<UAkAudioEvent> SFX_Click_Join = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Audio|UI")
	TObjectPtr<UAkAudioEvent> SFX_Click_Quit = nullptr;
	
	//If audio event's duration can't be read, timer delay defaults to 0.15s
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Audio|UI", meta=(ClampMin = "0.0", ClampMax = "2.0"))
	float AudioFallbackDelay = 0.15f;

private:
	FTimerHandle DelayedActionTimer;

	void PlayUISFX(UAkAudioEvent* EventToPlay, const FTimerDelegate& AfterSFXDelegate);
};

