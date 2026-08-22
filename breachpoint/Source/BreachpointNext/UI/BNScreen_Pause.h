#pragma once

#include "UI/BNActivatableWidget.h"
#include "BNScreen_Pause.generated.h"

class UButton;
class UTextBlock;

/**
 * The pause menu — GameMenu layer, Menu input, and the ONE screen in R7 that takes clicks.
 *
 * IT DOES NOT PAUSE THE MATCH, and says so on its own face: the match is server-authoritative
 * and keeps running while this is up, so a player who opens it can still be killed. That is the
 * design's own warning text, not a caveat invented here.
 *
 * Two rows only. The design's chassis has five (Settings, Controls, File Share); those screens
 * do not exist, and a button that opens nothing is worse than a button that is absent.
 *
 * Closing is the WIDGET's job, never the controller's: once Menu input is desired, a game input
 * action cannot be relied on to come back: `bIsBackHandler` gives Esc/B to CommonUI, and Resume
 * gives it to the mouse.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNScreen_Pause : public UBNActivatableWidget
{
	GENERATED_BODY()

public:
	UBNScreen_Pause();

protected:
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UButton> LeaveButton;

	/** "THE MATCH DOES NOT PAUSE" — set from C++ so the warning cannot drift in an asset. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UTextBlock> WarningText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "BN|UI")
	TObjectPtr<UTextBlock> WarningBodyText;
};
