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
 * action cannot be relied on to come back. `NativeOnKeyDown` below is the real keyboard/pad exit
 * — `bIsBackHandler` is set for the day this project ships a CommonUI input-data asset, but it
 * binds nothing today, so it is a hook, not a way out (see the override's comment).
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINTNEXT_API UBNScreen_Pause : public UBNActivatableWidget
{
	GENERATED_BODY()

public:
	UBNScreen_Pause();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;

	/** THE WAY OUT on keyboard and pad. Written when CommonUI's back action was bound to
	 *  NOTHING — this project shipped no `CommonInputSettings`, so `NativeOnHandleBackAction`
	 *  could never fire and a mouse click was the only exit (a soft lock if the cursor failed
	 *  to appear). Unhandled keys bubble up from the focused button to here.
	 *
	 *  THAT PREMISE EXPIRED ON 2 SEP (BN43 wired `InputData_Default`; the founder's 3 Sep pass
	 *  added the Windows ControllerData list). CommonUI's back action is bound NOW, so this
	 *  screen has TWO live paths to the same exit: the action router into
	 *  `NativeOnHandleBackAction`, and this override. Whether that pops one level or two is
	 *  UNMEASURED — it depends on whether returning `Handled()` here suppresses the router,
	 *  which cannot be read from this repository. BN46 is the test. If it double-pops, the
	 *  redundant half is the gamepad key below, not the action handler. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UButton> LeaveButton;

	/** "THE MATCH DOES NOT PAUSE" — set from C++ so the warning cannot drift in an asset, and a
	 *  HARD BindWidget (critic): it is this screen's whole justification, and an optional bind
	 *  lets a WBP omit it and still load clean. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UTextBlock> WarningText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "BN|UI")
	TObjectPtr<UTextBlock> WarningBodyText;
};
