#pragma once

#include "InputCoreTypes.h"
#include "UI/BRActivatableWidget.h"

#include "BRScreen_KeyRemap.generated.h"

class UBRSettingsValue_KeyRemap;
class UCommonTextBlock;

/**
 * `UBRScreen_KeyRemap` -- "press a key" capture. WBP: `WBP_Modal_KeyRemap`. Pushes to `Layer.Modal`.
 *
 * IT LISTENS AT THE SLATE LEVEL, NOT THROUGH ENHANCED INPUT, and that inversion is the whole
 * point of the screen. Enhanced Input hands you ACTIONS -- "Jump fired" -- which is precisely the
 * layer being reconfigured. To learn that the player pressed the *space bar* the widget has to
 * read raw key events, which is `NativeOnKeyDown` / `NativeOnMouseButtonDown`. Trying to do this
 * through the input actions would only ever report the bindings that already exist.
 *
 * IT MUST SWALLOW EVERYTHING WHILE OPEN. `NativeOnKeyDown` returns Handled for every key, bound
 * or not. A capture screen that lets an unhandled key fall through rebinds the action AND fires
 * whatever that key currently does -- opening the map, firing a weapon, quitting to desktop.
 *
 * ESCAPE CANCELS AND IS NEVER BINDABLE. `UBRSettingsValue_KeyRemap::IsBindableKey` refuses it at
 * the model layer too, so the rule holds even if some other caller reaches the model directly.
 *
 * CONFLICTS ASK. A key already on another action raises the confirm and the player chooses; the
 * screen never silently steals or silently refuses. Both silent paths produce the same bug report
 * -- "rebinding does not work" -- from opposite causes.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRScreen_KeyRemap : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRScreen_KeyRemap();

	/** The binding being captured. Safe before construction; re-applied on activation. */
	void SetTargetBinding(UBRSettingsValue_KeyRemap* InBinding);

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnActivated() override;

	/** Every key is consumed. See the class doc -- this is not over-broad, it is the requirement. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End UUserWidget interface

	/** Route one captured key through the model, raising the conflict confirm if needed. */
	void AttemptBind(const FKey& InKey);

	void HandleStealConfirmResolved(bool bConfirmed);

	void UpdatePromptText();

	void PopSelf();

	// ------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_Modal_KeyRemap`:
	//   `Prompt` text -> PromptText        `Action` text -> ActionText
	// ------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> PromptText;

	/** The action being rebound, so the player can see what they are about to change. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> ActionText;

	// The steal-this-key confirm is `UBRUISettings::ConfirmModalClass` -- the SAME class the
	// settings screen raises for its discard prompt, set once in `Config/DefaultGame.ini`. Not
	// declared here for the reason given on that config block: the WBP generator cannot author
	// a CDO default, so a member here could only ever be filled in by hand.

private:
	UPROPERTY(Transient)
	TObjectPtr<UBRSettingsValue_KeyRemap> Binding;

	/** The key waiting on a steal confirm. Invalid when nothing is pending. */
	UPROPERTY(Transient)
	FKey PendingKey;

	/** Set while the conflict confirm is up, so further presses are ignored rather than stacked. */
	bool bConfirmPending = false;
};
