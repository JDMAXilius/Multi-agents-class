#pragma once

#include "UI/Components/BRMenuRow.h"

#include "BRSettingsRow.generated.h"

class UBRSettingsDataObject;

/**
 * `UBRSettingsRow` -- a `UBRMenuRow` that knows which setting it is showing.
 *
 * IT IS A SUBCLASS AND THAT IS NOT A CONTRADICTION. `UBRMenuRow`'s own doc says 27 reference
 * variants collapse to ONE class because they differ only in appearance. This differs in
 * BEHAVIOUR: it holds a data object, subscribes to it, and turns left/right into a value change.
 * That is a new responsibility, not a new look, and it is exactly what a subclass is for. The
 * Type axis is still `UBRMenuRow`'s -- this class picks a value on it, it does not add one.
 *
 * WHY NOT A LIST VIEW ENTRY. `UBRModal_Options` already establishes the project's idiom: a
 * screenful of rows is built into a `UPanelWidget` on activation, and pooling is reserved for
 * the killfeed's per-kill churn. The longest settings tab is the key bindings, which is tens of
 * rows built once when a screen opens -- the same case, so it gets the same answer. Reaching for
 * `UCommonListView` here would add an entry-widget indirection and a pooling lifecycle to solve
 * a problem this screen does not have.
 *
 * THE ROW OWNS NO VALUE. Every read goes through the data object, every write goes through the
 * data object's setter, and the row redraws from `OnSettingChanged`. It cannot show a value the
 * model does not have, because it has nowhere to keep one.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRSettingsRow : public UBRMenuRow
{
	GENERATED_BODY()

public:
	/** Point the row at a setting. Safe to call repeatedly; the previous subscription is dropped. */
	void SetSetting(UBRSettingsDataObject* InSetting);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	UBRSettingsDataObject* GetSetting() const { return Setting; }

	/**
	 * Left/right on this row: one step for a scalar, one option for a discrete. `Direction` is
	 * -1 or +1. A row whose setting does neither (a header, a key binding) ignores it -- rebinding
	 * a key is an ACTIVATION, not a nudge, and routing it here would let a stray stick flick
	 * rebind a control.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	void NudgeValue(int32 Direction);

	/** Fired when the row is clicked or accepted. The screen decides what that means. */
	DECLARE_DELEGATE_OneParam(FBRSettingsRowActivated, UBRSettingsDataObject* /* Setting */);
	FBRSettingsRowActivated OnSettingRowActivated;

	/**
	 * Which `EBRMenuRowType` body a setting renders as -- WITHOUT needing a row to ask.
	 *
	 * Static because the screen has to pick a WIDGET CLASS before it can create the widget, and
	 * the type is what decides which class. `RefreshFromSetting` used to compute this on an
	 * already-built row and set `RowType` on it, which changed the row's height and nothing else:
	 * the per-type bodies live in different assets, so a Scalar setting and a Discrete one both
	 * rendered as a plain Default row. Resolving before construction is what closes that.
	 */
	static EBRMenuRowType ResolveRowTypeFor(const UBRSettingsDataObject* InSetting);

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	//~ Begin UCommonButtonBase interface
	virtual void NativeOnClicked() override;
	//~ End UCommonButtonBase interface

	/** Push the setting's label, value, row type and enabled state onto the widget. */
	void RefreshFromSetting();

	/** Which `EBRMenuRowType` body this setting renders as. */
	EBRMenuRowType ResolveRowType() const;

private:
	void HandleSettingChanged(UBRSettingsDataObject* Changed);

	/** Drop the delegate binding. Called from `SetSetting` and `NativeDestruct`. */
	void UnsubscribeFromSetting();

	UPROPERTY(Transient)
	TObjectPtr<UBRSettingsDataObject> Setting;
};
