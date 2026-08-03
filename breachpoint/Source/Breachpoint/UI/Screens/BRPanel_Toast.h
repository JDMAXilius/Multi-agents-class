#pragma once

#include "UI/BRActivatableWidget.h"

#include "BRPanel_Toast.generated.h"

class UCommonTextBlock;
class USizeBox;

/**
 * `UBRPanel_Toast` -- `OV_Toast` / `Warning Message` `707:5770`, 349 x 60, reference file
 * `Kn87U5sy2VD0lP8K7h4LcQ` (SCREEN-MANIFEST Sec 4.8). WBP: `WBP_Panel_Toast`. Pushes to
 * `Layer.Modal`.
 *
 * NON-BLOCKING, and that is the whole design:
 *  - `bIsModal` stays false and `bSupportsActivationFocus` is turned OFF, so the toast never takes
 *    gamepad focus and never dictates a `FUIInputConfig`. Whatever screen is beneath keeps its
 *    input exactly as it was -- which is why this class does NOT override
 *    `GetDesiredInputConfig()` while the two modals do.
 *  - `bIsBackHandler` stays false: back belongs to the screen underneath, not to a message that
 *    is about to dismiss itself.
 *
 * IT DISMISSES ON A TIMER, NEVER ON A TICK (law 4). `DisableNativeTick` plus one `FTimerHandle`;
 * there is no elapsed-time accumulator anywhere in this class.
 *
 * 349 wide is column 1 exactly (SCREEN-MANIFEST Sec 1: 3 columns x 349). Width and height are
 * driven onto `RootSizeBox` from C++ so the toast cannot drift off the column grid.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRPanel_Toast : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRPanel_Toast();

	/** SCREEN-MANIFEST Sec 4.8: `Warning Message` is 349 x 60. */
	static constexpr float ToastWidth = 349.0f;
	static constexpr float ToastHeight = 60.0f;

	/** SCREEN-MANIFEST Sec 1: column 1 origin. 349 == the column width exactly. */
	static constexpr float Column1OriginX = 69.0f;

	/**
	 * The message payload. `InDisplaySeconds <= 0` uses `DefaultDisplaySeconds`.
	 *
	 * Calling this on an already-visible toast replaces the message and RESTARTS the timer, so a
	 * second message can never inherit the remainder of the first one's dwell.
	 */
	void SetToastMessage(const FText& InMessage, float InDisplaySeconds = 0.0f);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_Panel_Toast`. Figma layer -> UMG name:
	//   `Warning Message` frame -> RootSizeBox      message text -> MessageText
	// The `UBRRule` the design draws under the message is authored in the WBP and is not bound:
	// C++ binds only what C++ drives.
	// -------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> MessageText;

	/**
	 * UNMEASURED: no source records a dwell time for `Warning Message` -- the Figma frame carries
	 * geometry only. 4 seconds is a stated default, not a measurement; it is `EditDefaultsOnly` so
	 * `WBP_Panel_Toast` can carry the tuned value (R26 condition: default values only) and callers
	 * can override per message.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|UI")
	float DefaultDisplaySeconds = 4.0f;

private:
	void StartDismissTimer();
	void HandleDismissTimer();

	FText Message;

	/** 0 == "use DefaultDisplaySeconds". Set per message by the caller. */
	float DisplaySeconds = 0.0f;

	FTimerHandle DismissTimerHandle;
};
