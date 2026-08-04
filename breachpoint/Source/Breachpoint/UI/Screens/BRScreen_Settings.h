#pragma once

#include "UI/BRActivatableWidget.h"
#include "UI/Components/BRSettingsRow.h"

#include "BRScreen_Settings.generated.h"

class UBRNavBar;
class UBRSettingsDataObject;
class UBRSettingsRegistry;
class UBRSettingsValue_KeyRemap;
class UCommonTextBlock;
class UPanelWidget;

/**
 * `UBRScreen_Settings` -- the settings screen. WBP: `WBP_Screen_Settings`. Pushes to `Layer.Menu`.
 *
 * IT IS NOT `UBRModal_Options`, AND THE TWO MUST NOT BE MERGED. `UBRModal_Options` is a 451x682
 * row-CHOOSER: the caller hands it a payload of rows, the player picks one, it reports which and
 * pops. It has no concept of a value, a range, persistence, or apply. This screen is the settings
 * TREE: it owns a `UBRSettingsRegistry`, its rows hold live values, and it has a commit step.
 * They share `UBRMenuRow` and nothing else, which is the correct amount.
 *
 * TABS ARE `UBRNavBar`, NOT A NEW WIDGET. The nav bar already does tab definitions, exclusive
 * selection through a `UCommonButtonGroupBase`, and bumper-key navigation. Building a second tab
 * strip for this screen would fork the gamepad routing that component exists to own.
 *
 * ROWS ARE BUILT INTO A PANEL, NOT A LIST VIEW. Same call as `UBRModal_Options` makes, and for
 * the same stated reason -- see `UBRSettingsRow`'s class doc.
 *
 * THE COMMIT MODEL, which is the part worth reviewing:
 *   - Setters write through immediately, so a slider shows what you are dragging it to.
 *   - `Apply` is what calls `ApplySettings` + `SaveSettings`, and is the only thing that moves
 *     resolution, window mode or scalability.
 *   - Backing out with unsaved changes raises `UBRModal_Warning`. Confirming DISCARDS -- it calls
 *     `CancelChanges`, which walks every leaf back to the baseline captured on activation. The
 *     confirm is not decoration: without it, a mis-set resolution is unrecoverable by the only
 *     control the player still trusts.
 *
 * NO TICK. Rows redraw from `OnSettingChanged`; the screen redraws its dirty state from
 * `OnAnySettingChanged`. Nothing samples anything.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRScreen_Settings : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRScreen_Settings();

	/** A settings screen takes exclusive Menu input -- it is a full navigation level. */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	UBRSettingsRegistry* GetRegistry() const { return Registry; }

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget interface

	//~ Begin UCommonActivatableWidget interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/**
	 * Back / B / Escape. Returns true when it has handled the press.
	 *
	 * CommonUI's own hook rather than a `RegisterUIActionBinding` for the back action: the
	 * binding path is for EXTRA actions a widget adds (the nav bar's bumpers), whereas back is
	 * routed by the activatable stack to the top widget that claims it. Registering a second
	 * binding for it would mean two things answering one press.
	 */
	virtual bool NativeOnHandleBackAction() override;
	//~ End UCommonActivatableWidget interface

	/** Rebuild `RowContainer` for the selected tab. */
	void RebuildRows();

	void ReleaseRows();

	/**
	 * `UFUNCTION` because `UBRNavBar::OnTabSelected` is a DYNAMIC multicast delegate and binds
	 * with `AddDynamic`, which resolves the handler by reflected name. Without the macro this
	 * compiles and fails at runtime with a bind assert.
	 */
	UFUNCTION()
	void HandleTabSelected(int32 TabIndex);

	void HandleRowActivated(UBRSettingsDataObject* Setting);
	void HandleAnySettingChanged(UBRSettingsDataObject* Changed);

	/** Resolution of the discard-changes confirm. True discards and pops; false stays put. */
	void HandleDiscardConfirmResolved(bool bConfirmed);

	/** Focus moved to a row: the details panel shows that row's description. */
	void ShowDescriptionFor(const UBRSettingsDataObject* Setting);

	void PopSelf();

	// ------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_Screen_Settings`:
	//   tab strip     -> TabBar          row list box  -> RowContainer
	//   `Page Title`  -> TitleText       description   -> DescriptionText
	// `RowContainer` is a plain `UPanelWidget` so the WBP may use a vertical box today and a
	// scroll box later without a C++ change -- the same latitude `UBRModal_Options` takes.
	// ------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UBRNavBar> TabBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UPanelWidget> RowContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> TitleText;

	/** The focused row's description. Optional: a compact layout may omit the panel entirely. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> DescriptionText;

	// The three widget classes this screen needs -- the row, the rebind modal and the discard
	// confirm -- are NOT declared here. They live on `UBRUISettings` as `config` properties and
	// are set in `Config/DefaultGame.ini`; see the comment on that block for why (the WBP
	// generator cannot author a CDO default, so a member here could only be filled in by a hand
	// edit to a binary asset). All three are still SOFT and still resolved on use (law 3).

private:
	UPROPERTY(Transient)
	TObjectPtr<UBRSettingsRegistry> Registry;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBRSettingsRow>> RowWidgets;

	/** Mirrors `TabBar`'s index -> the registry tab id it selects. Built in `NativeOnActivated`. */
	UPROPERTY(Transient)
	TArray<FName> TabIds;

	UPROPERTY(Transient)
	FName SelectedTabId;

	/**
	 * Set while the discard confirm is up, so a second back press cannot raise a second modal
	 * over the first. Without it, holding B on a dirty screen stacks confirms.
	 */
	bool bConfirmPending = false;
};
