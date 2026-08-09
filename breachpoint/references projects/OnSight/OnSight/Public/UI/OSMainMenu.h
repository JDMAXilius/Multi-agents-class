#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "OSMainMenu.generated.h"

class UOSBaseCommonButton;
class UTextBlock;
class UAkAudioEvent;
class UOSSessionsSubsystem;
class UCommonActivatableWidgetStack;
class FOnlineSessionSearchResult;

/*
  CommonUI main menu. Lives on a CommonActivatableWidgetStack.
  Host pushes the configured host menu onto the same stack;
  Join searches sessions and travels on match.
 */
UCLASS()
class ONSIGHT_API UOSMainMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// --- Bound Widgets ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> HostButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> JoinButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> QuitButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> OptionsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UOSBaseCommonButton> CharacterCreatorButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ErrorText;

	/* Optional: if present, Host pushes the host menu onto this stack directly (Zookeepers-style). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetStack> MenuStack;

	/* Optional: panel containing the main menu buttons. Auto-hidden while MenuStack has an active submenu, restored when popped. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MainMenuContent;

	// --- Configuration ---

	// Host menu to push onto the stack when Host is clicked.
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Navigation")
	TSubclassOf<UCommonActivatableWidget> HostMenuClass;

	// --- Session Settings ---

	/* When non-empty, Join prefers this OnSightMatchType; otherwise joins the first OnSight session. */
	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Session Settings", meta = (DisplayName = "Join Preference: OnSight Match Type"))
	FString DefaultMatchType;

	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Session Settings", meta = (DisplayName = "Force LAN (Editor Override)"))
	bool bDefaultUseLAN = false;

	// --- Audio ---

	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Audio|UI")
	TObjectPtr<UAkAudioEvent> SFX_Click_Host = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Audio|UI")
	TObjectPtr<UAkAudioEvent> SFX_Click_Join = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Audio|UI")
	TObjectPtr<UAkAudioEvent> SFX_Click_Quit = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "OnSight|Audio|UI", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float AudioFallbackDelay = 0.15f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// --- Button Handlers ---

	void OnHostButtonClicked();
	void OnJoinButtonClicked();
	void OnQuitButtonClicked();

	UFUNCTION() void OnOptionsButtonClicked();
	UFUNCTION() void OnCharacterCreatorButtonClicked();

	/* Stub for buttons whose target menu isn't implemented yet: logs + on-screen message. */
	UFUNCTION(BlueprintCallable, Category = "OnSight|Navigation")
	void LogUnimplementedMenu(const FString& MenuName);

	UFUNCTION()
	void OnMenuStackDisplayedWidgetChanged(UCommonActivatableWidget* NewWidget);

	// --- Session Subsystem Callbacks ---

	void OnFindSessionsComplete(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);

	UFUNCTION()
	void OnSessionFlowError(const FText& ErrorMessage);

	UFUNCTION()
	void OnTravelStarted();

	/*
	  Player-facing session failure. Default C++ sets ErrorText.
	  Override in Blueprint for a popup / animation.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "OnSight|Session|Feedback")
	void OnSessionError(const FText& ErrorMessage);

private:
	// --- Wiring ---

	void BindSessionDelegates();
	void UnbindSessionDelegates();
	void BindButtonHandlers();
	void UnbindButtonHandlers();

	// --- Navigation ---

	void PushHostMenu();
	UCommonActivatableWidget* PushActivatableOnOwningStack(TSubclassOf<UCommonActivatableWidget> WidgetClass);

	// --- Session Helpers ---

	static FString GetResultMatchType(const FOnlineSessionSearchResult& Result);
	const FOnlineSessionSearchResult* PickSessionToJoin(const TArray<FOnlineSessionSearchResult>& Results) const;
	void HandleFindSessionsFailure(bool bWasSuccessful);

	// --- Visibility Helpers ---

	void HideError();

	// --- Audio Helpers ---

	void PlayUISFX(UAkAudioEvent* EventToPlay, const FTimerDelegate& AfterSFXDelegate);

	// --- State ---

	UPROPERTY(Transient)
	TObjectPtr<UOSSessionsSubsystem> SessionsSubsystem;

	FTimerHandle DelayedActionTimer;
};
