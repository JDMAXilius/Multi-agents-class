#pragma once

#include "CommonUserWidget.h"
#include "FieldNotificationId.h"

#include "BRAmmoBlock.generated.h"

class UBRVM_Combat;
class UCommonTextBlock;
class USizeBox;
class UWidgetAnimation;

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
	Normal
};

// `Low` and `Battery` were CUT (HUD-CPP-AUDIT): unreachable by construction — ResolveState
// could never return them because UBRVM_Combat has neither a low-ammo signal nor a charge
// field, and law 3 forbids inventing the threshold in a widget. They return WITH those
// ViewModel fields (BP69), along with the switcher/battery scaffolding that served them.
// One ruling worth keeping from the cut text: when Low returns it is AMBER, not the export's
// red — ui-presentation Sec 3 keeps red for threat, and low ammo is a clock.

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

	/** The only geometry C++ touches: the measured 190 x 40 box. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<USizeBox> RootSizeBox;

	/**
	 * The known-state flourish, authored in the WBP and PLAYED from C++ — the lawful
	 * replacement for the old `BP_OnAmmoStateChanged` BIE (unimplementable under R18).
	 * Forward on Unknown -> Normal, reverse on the way back; gated on actual transitions.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> StateAnim;

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
