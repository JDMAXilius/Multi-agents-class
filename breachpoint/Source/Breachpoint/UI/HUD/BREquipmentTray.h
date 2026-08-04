#pragma once

#include "CommonUserWidget.h"
#include "FieldNotificationId.h"
#include "UI/BRUITypes.h"

#include "BREquipmentTray.generated.h"

class UBRProgressBar;
class UBRVM_Combat;
class UCommonTextBlock;
class UWidgetAnimation;

/**
 * `UBREquipmentTray` -- the BOTTOM-RIGHT HUD surface: grenade count and the grapple cooldown.
 * (The old BOTTOM-LEFT claim here was stale — `figma_hud_layout.json` measures the tray at
 * x=940 of 1280, and the plan builds it there. HUD-CPP-AUDIT stale-comment sweep.)
 *
 * SCOPE, AND WHY IT LOOKS SMALLER THAN THE ART
 * -------------------------------------------
 * `docs/tickets/TICKET_BP69_HUD_ART_EXCEEDS_SIM.md` measures the divergence this class sits on
 * top of: the Figma HUD ships FOUR grenade types (Frag, Plasma, Spike, Dynamo) and SIX abilities
 * (Grapple, Repulsor, Threat, Drop Wall, Thruster, Overshield). The sim ships ONE untyped grenade
 * count and ONE ability. The ticket's step 1 -- "Rule the scope, per family" -- "is a design call"
 * and "gates everything else", and its step 4 is explicit: "Do not file them speculatively -- six
 * abilities and four grenade types would be a large phantom backlog."
 *
 * So this class renders exactly what `UBRVM_Combat` exposes today: `GrenadeCount` (no type axis)
 * and `GrappleCooldownDuration` / `bGrappleReady`. There is no `EBRGrenadeType` here, and no
 * six-slot ability strip. The ticket already records the reason; inventing the enum here would
 * settle a founder call by being the next person to open the HUD, which is the exact failure the
 * ticket names.
 *
 * A TYPE AXIS LATER IS A VIEWMODEL CHANGE, NOT A REWRITE OF THIS CLASS. BP69 says so in its own
 * notes: "`UBRVM_Combat` has no grenade-type or ability-type axis. If step 1 puts either in the
 * slice, that is a ViewModel change and therefore a C++ gap, not a widget change." When that
 * field arrives, `RefreshGrenades()` binds one more FieldNotify id and hands the type to the WBP
 * through one more `BP_On...` hook; the count path, the cooldown path and the WBP layout are
 * untouched.
 *
 * NO TICK (law 4, `meta = (DisableNativeTick)`). Nothing here samples game state per frame:
 *  - grenade count and equipment state arrive as FieldNotify pushes,
 *  - the cooldown STARTS from the ViewModel's `GrappleCooldownStarted` event, which is fed by
 *    `GE_Cooldown` tag events (`ue5-ui-architecture` Sec 3), and READY arrives as its own event.
 *    Between the two, a timer at `CooldownRefreshSeconds` advances a purely visual interpolation
 *    between two known instants. It reads a clock, never the sim.
 *
 * THE WIDGET IS THE CONSUMER OF THE READY STATE, NEVER ITS SOURCE. Per GAS purity, "ready" is a
 * GE-applied tag. When the local interpolation runs out this class holds the bar full and STOPS
 * -- it does not declare the grapple ready. Only the ViewModel's `GrappleReady` event does that.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBREquipmentTray : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UBREquipmentTray();

	/**
	 * MEASURED from `Export/UI/HUD/`, for the WBP author. Grenade glyphs are ~23 x 17
	 * (`HUD_Grenade_*_Sel.svg`) and the ability pill is 52 x 31 (`HUD_Ability_Grapple_Ready.svg`).
	 * Declared, not enforced: geometry is the WBP's, these numbers stop it being guessed.
	 */
	// (The four glyph/pill constants that sat here were CUT — zero readers; the measured
	// numbers live in the export files and MCP-BUILD-PLANS with the rest of the WBP geometry.)

	/**
	 * Display refresh of the cooldown interpolation, in seconds. A PRESENTATION number, not a
	 * tuning number -- it changes how smoothly a ring sweeps and nothing about the ability, so it
	 * is not a `Content/Data` row (law 3). Pinned in C++ so no WBP can fork it.
	 *
	 * ponytail: fixed 20 Hz. If a ring ever reads steppy at high refresh, the upgrade is a UMG
	 * sequence played once at a scaled rate, not a smaller interval and not a Tick.
	 */
	static constexpr float CooldownRefreshSeconds = 0.05f;

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	// ---------------------------------------------------------------------------------------
	// BindWidget contract -- the authoring names for `WBP_EquipmentTray`. The BP subclass holds
	// layout, the glyph art and animation ONLY; every branch on state is above this line.
	// ---------------------------------------------------------------------------------------

	/** The count. One number, because the sim has one grenade type -- see the class doc. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|HUD")
	TObjectPtr<UCommonTextBlock> GrenadeCountText;

	/**
	 * The grapple cooldown ring/bar. `UBRProgressBar` with treatment `CombatVISR` and channel
	 * `Amber` ("a clock is running", pinned in `NativeOnInitialized`). There is deliberately no
	 * second bar class in this folder -- the ring is a `UBRProgressBar` whose WBP fill brush is
	 * radial.
	 */
	/**
	 * DEMOTED to Optional 3 Aug 2026, and the reason is a measurement, not a preference.
	 * `HUD / Weapon Tray` (working file `yznvnVdOFDADaugZSeomfP`, node `24:35`, 240x90) draws
	 * NO cooldown bar: its nine children are the two slot rects, their two counts, the
	 * magazine / separator / reserve texts, the weapon silhouette and the tray rule. Required
	 * here meant a 1:1 authoring of that symbol could not satisfy the contract -- the WBP
	 * would fail at ASSET LOAD unless someone invented a position for a bar the design does
	 * not have. Optional lets the measured tray build, and `ApplyGrappleState` already guards
	 * every use on null, so nothing downstream changes.
	 *
	 * This does NOT say the grapple ring is cut. It says the ring is not in THIS symbol, and
	 * a member whose art lives in a different frame should not block this one.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<UBRProgressBar> CooldownBar;

	/**
	 * The EQUIP slot's count -- node `24:39`, symbol-local (156, 4) 6x19, beside the GRENADE
	 * slot's `24:37`. Added 3 Aug 2026 because the symbol measures a number this class had
	 * nowhere to put: the tray could draw the EQUIP rectangle and then had to leave it empty.
	 *
	 * Optional, deliberately. The sim has ONE untyped equipment count today (R39 buckets the
	 * five post-slice abilities), so a required member would force every author to bind a
	 * number nothing publishes yet. When a producer exists it is one `SetEquipmentCount` away.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<UCommonTextBlock> EquipmentCountText;

	/** Printed in the bar's readout while the ability is off cooldown. Display string only.
	 *  EditDefaultsOnly, and note it has no designer preview path — it is applied on the
	 *  ready transition at runtime, so a per-asset override is legitimate but invisible in the
	 *  designer (recorded, HUD-CPP-AUDIT P3). */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|HUD")
	FText GrappleReadyText;

	/**
	 * The Ready <-> Cooling pill swap, authored in the WBP and PLAYED from C++ — the lawful
	 * replacement for the old `BP_OnGrappleReadyChanged` BIE (unimplementable under R18).
	 * Forward on ready, reverse into cooldown.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> GrappleReadyAnim;

private:
	void BindViewModel();
	void UnbindViewModel();

	void HandleCombatFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);

	void RefreshGrenades();

	/** Fired by the ViewModel's cooldown-start event. Records the two instants, starts the timer. */
	void StartCooldownVisual(float DurationSeconds);

	/** Fired by the ViewModel's ready event -- the ONLY path that may report ready. */
	void ApplyGrappleReady();

	/** Timer callback. Interpolates between the two recorded instants. Reads no game state. */
	void AdvanceCooldownVisual();

	/** Honest unknown: cooling, but this widget never saw the start (join in progress). */
	void ApplyCooldownUnknown();

	void StopCooldownTimer();

	UPROPERTY(Transient)
	TWeakObjectPtr<UBRVM_Combat> BoundCombat;

	FDelegateHandle GrenadeCountHandle;
	FDelegateHandle EquipmentStateHandle;
	FDelegateHandle CooldownStartedHandle;
	FDelegateHandle GrappleReadyHandle;

	FTimerHandle CooldownTimerHandle;

	/** Display state only -- world-clock instants, never simulation state. */
	float CooldownEndTimeSeconds = 0.0f;
	float CooldownDurationSeconds = 0.0f;
};
