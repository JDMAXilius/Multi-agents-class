#pragma once

#include "CommonUserWidget.h"
#include "FieldNotificationId.h"
#include "UI/Components/BRProgressBar.h"

#include "BRVitalsWidget.generated.h"

class UBRVM_Combat;
class USizeBox;
class UWidget;

/**
 * `UBRVitalsWidget` -- the LEFT HUD surface: shields over health, top-left.
 *
 * GEOMETRY. `Export/UI/HUD/HUD_Vitals_ShieldHealth.svg` is a 277 x 35 viewBox and
 * `HUD_Vitals_ShieldBroken.svg` is 276 x 35. The 1px delta is a source artefact (the broken
 * frame's arc is stroked at 1.5 rather than 1, which shifts its bounds), NOT two sizes: they
 * are the same box in two states. 277 is the one recorded here and the WBP uses it for both.
 *
 * NO SECOND BAR. Both channels are `UBRProgressBar` instances -- the segmented bar already
 * exists, already implements hidden-until-damaged for `CombatVISR` + `Health`, and already
 * constructs in the honest unknown state. This class supplies the DATA and pins the treatment
 * so a WBP cannot fork a HUD bar into a lobby bar.
 *
 * THE TWO RULES THIS CLASS ENFORCES (ui-presentation Sec 3, UI-DESIGN-SYSTEM Sec 2):
 *   1. Health is `visr/health` yellow and NEVER green. Pinned from C++ via the channel enum;
 *      no colour is ever passed in and none may be typed into the WBP.
 *   2. The health bar is HIDDEN until damaged -- that is what keeps the resting HUD quiet.
 *      Implemented once, inside `UBRProgressBar::ApplyBarVisibility`; we only feed it.
 *      Shields always show.
 *
 * SHIELD COLOUR. `visr/shield-low` is the authored low-shield treatment, and this class does
 * NOT implement it: `UBRVM_Combat` exposes no "shields are low" signal and
 * `EBRProgressChannel` has no `ShieldLow` member, so honouring it would mean inventing a
 * hardcoded 0.25f -- a gameplay number, which law 3 puts in `Content/Data/CT_Combat.csv` and
 * not in a widget. Both halves are filed as contract gaps in this packet's report. What the
 * ViewModel DOES give is `bShieldsBroken`, so the bar drives off exactly that: broken swaps
 * the channel to `Enemy`, which is a sanctioned use of the threat colour (the tokens name the
 * shield-break flash explicitly) and matches the red-stroked empty arc in the export.
 *
 * NO POLLING, NO REACH. `meta = (DisableNativeTick)`; every value arrives through a FieldNotify
 * delegate on `UBRVM_Combat`, which is this widget's ONLY data source. Nothing here reads a
 * pawn, an ASC or a PlayerState, and nothing here writes anything at all.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRVitalsWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Measured: `HUD_Vitals_ShieldHealth.svg` viewBox. See the class doc for the 1px delta. */
	static constexpr float VitalsWidth = 277.0f;
	static constexpr float VitalsHeight = 35.0f;

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_BRVitalsWidget`. These names are the authoring contract; the
	// WBP must name its widgets exactly this. Figma -> UMG mapping:
	//   `Shield Arc` / `Shield Arc Empty`         -> ShieldBar (a UBRProgressBar)
	//   `Health (nested, hidden until damaged)`   -> HealthBar (a UBRProgressBar)
	//   `Centre Tick`                             -> CentreTick
	// The WBP owns layout and animation only. There is no other geometry in this class.
	// ---------------------------------------------------------------------------------------

	/** Always visible. Channel and treatment are pinned in C++, never in the WBP. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|HUD")
	TObjectPtr<UBRProgressBar> ShieldBar;

	/** Hidden until damaged -- the rule lives in `UBRProgressBar`, this class only feeds it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|HUD")
	TObjectPtr<UBRProgressBar> HealthBar;

	/**
	 * The centre tick under the shield arc. Present in the intact export, absent from the broken
	 * one -- and only C++ knows which state is live, so its visibility cannot be a WBP decision.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<UWidget> CentreTick;

	/** The only geometry C++ touches: the measured 277 x 35 box. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<USizeBox> RootSizeBox;

	/**
	 * Fires when the shield-break or known/unknown state CHANGES, not on every attribute push --
	 * a shield-break animation retriggered once per regen tick would strobe. Display only: this
	 * hook decides nothing and the WBP may only animate from it.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Breachpoint|HUD", meta = (DisplayName = "On Vitals State Changed"))
	void BP_OnVitalsStateChanged(bool bShieldsBroken, bool bKnown);

private:
	void BindViewModel();
	void UnbindViewModel();

	/** Any observed field changed -> re-read all of them. Four getters is cheaper than a switch. */
	void HandleFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);

	void Refresh();

	/** Honest empty state: dimmed bars and no confident percentage. The state we construct in. */
	void ApplyUnknown();

	/** One place the BP hook is allowed to fire from, and only on an actual change. */
	void NotifyStateChanged(bool bInShieldsBroken, bool bInKnown);

	UPROPERTY(Transient)
	TWeakObjectPtr<UBRVM_Combat> BoundViewModel;

	/** Last pushed pair, so the BP hook fires on transitions only. */
	bool bLastShieldsBroken = false;
	bool bLastKnown = false;
	bool bStatePushed = false;
};
