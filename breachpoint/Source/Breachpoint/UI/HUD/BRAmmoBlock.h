#pragma once

#include "CommonUserWidget.h"
#include "FieldNotificationId.h"

#include "BRAmmoBlock.generated.h"

class UBRVM_Combat;
class UCommonTextBlock;
class USizeBox;
class UWidgetSwitcher;

/**
 * The three authored ammo states. `HUD_Ammo_Readout.svg`, `_Low` and `_Battery` all export at
 * viewBox 190 x 40 -- ONE size, three states. That is the signal they are one widget, not three,
 * and this enum is how that fact survives contact with a WBP author.
 *
 * WHICH STATES THIS CLASS CAN ACTUALLY REACH TODAY: `Unknown` and `Normal`. `Low` needs a
 * low-ammo signal (or a magazine capacity to derive one from) and `Battery` needs an
 * energy-weapon flag; `UBRVM_Combat` exposes neither, and law 3 forbids inventing the threshold
 * here. Both are filed as contract gaps -- the state machine is written and wired so that
 * landing the ViewModel fields is a one-line change in `ResolveState`, and NOT so that someone
 * can type a number into a details panel in the meantime.
 */
UENUM(BlueprintType)
enum class EBRAmmoReadoutState : uint8
{
	/** No equipment data has arrived. Dashes, dimmed -- never a confident "0 / 100". */
	Unknown,

	/** Mag in `visr/shield`, reserve in `visr/ink-dim`. The resting readout. */
	Normal,

	/**
	 * A clock is running on this magazine -> `visr/amber`.
	 *
	 * DELIBERATE DIVERGENCE FROM THE EXPORT, RECORDED ON PURPOSE. `HUD_Ammo_Low.svg` fills the
	 * mag glyph with `#FF4A3D`, which is `visr/enemy`. `ui-presentation` Sec 3 forbids exactly
	 * that: "Red is a threat channel -- do not spend it on a low-ammo warning that is not
	 * lethal." Low ammo is a clock, not a threat, so it takes Amber. The same skill's one-way
	 * rule ("Figma is right about appearance") does not rescue the export here, because the
	 * skill's own colour discipline is the more specific statement and the two are in direct
	 * conflict -- filed for the art lane rather than silently resolved twice.
	 */
	Low,

	/** Energy weapon: charge symbol and a percentage instead of mag / reserve. */
	Battery
};

/**
 * `UBRAmmoBlock` -- the RIGHT HUD surface, bottom-right: current weapon, ammo, and the GHOSTED
 * second weapon (the swap target).
 *
 * GEOMETRY. 190 x 40, measured from all three exports. Nothing else in this class is geometry.
 *
 * DATA. `UBRVM_Combat` and nothing else -- no pawn, no ASC, no weapon actor, no data table. The
 * magazine capacity that would let this widget compute "low" is in `DT_Weapons` (MagSize), and
 * reaching for it from a widget would be the workaround `ui-presentation` Sec 7.3 names: "if a
 * field does not exist on a ViewModel, that is a C++ gap -- file it, do not work around it in
 * the widget."
 *
 * `meta = (DisableNativeTick)`. Bind on construct, UNBIND on destruct.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRAmmoBlock : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Measured: all three ammo exports share this viewBox. */
	static constexpr float AmmoWidth = 190.0f;
	static constexpr float AmmoHeight = 40.0f;

	/** `StateSwitcher` indices. Readout and Low are the SAME layout at a different colour. */
	static constexpr int32 ReadoutLayoutIndex = 0;
	static constexpr int32 BatteryLayoutIndex = 1;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|HUD")
	EBRAmmoReadoutState GetReadoutState() const { return State; }

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_BRAmmoBlock`. Figma -> UMG mapping:
	//   `SET Ammo / Readout` `Mag`      -> MagazineText     `Reserve` -> ReserveText
	//   `SET Ammo / Battery` `Sym`      -> the switcher's battery page (layout, WBP-owned)
	//                        `Pct`      -> BatteryPercentText
	//   active weapon caption           -> ActiveWeaponText
	//   ghosted swap target             -> StowedWeaponText
	// ---------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|HUD")
	TObjectPtr<UCommonTextBlock> MagazineText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|HUD")
	TObjectPtr<UCommonTextBlock> ReserveText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<UCommonTextBlock> ActiveWeaponText;

	/** The swap target. Ghosted with the `visr/ink-dim` token -- never a bare opacity literal. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<UCommonTextBlock> StowedWeaponText;

	/** Readout (0) and Battery (1). Absent switcher = a WBP that only authored the readout. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<UWidgetSwitcher> StateSwitcher;

	/** Stays on the dash until the ViewModel gains a charge field. See the contract gaps. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<UCommonTextBlock> BatteryPercentText;

	/** The only geometry C++ touches: the measured 190 x 40 box. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<USizeBox> RootSizeBox;

	/** For the WBP's state animations only. Carries no data and decides nothing. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD", meta = (DisplayName = "On Ammo State Changed"))
	void BP_OnAmmoStateChanged(EBRAmmoReadoutState NewState);

private:
	void BindViewModel();
	void UnbindViewModel();

	void HandleFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);

	void Refresh();

	/** The ONE place a ViewModel reading becomes a state. Low and Battery land here or nowhere. */
	EBRAmmoReadoutState ResolveState() const;

	void ApplyState(EBRAmmoReadoutState InState);

	UPROPERTY(Transient)
	TWeakObjectPtr<UBRVM_Combat> BoundViewModel;

	EBRAmmoReadoutState State = EBRAmmoReadoutState::Unknown;

	/** False until the first ApplyState, so the WBP gets one push for the state it opens in. */
	bool bStateApplied = false;
};
