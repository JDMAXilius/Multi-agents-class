#pragma once

#include "CommonUserWidget.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRProgressBar.generated.h"

class UBRHairlineBorder;
class UCommonTextBlock;
class UImage;
class UProgressBar;

/**
 * `ui-presentation` Sec 4 -- 343's tier rule: "minimal and flat when far from play; the VISR
 * colour philosophy and a more skeuomorphic treatment once in-game", and the consequence the
 * skill states in one line: "If the HUD's shield bar and the lobby's progress bar look
 * identical, one of them is wrong."
 *
 * That is why this is an ENUM and not a bool and not two classes:
 *  - a bool ("bIsHUD") makes the tier rule an implementation detail nobody can grep for,
 *  - two classes fork one component into two that drift apart on the first restyle.
 * One class, one WBP family, two named treatments, selected per instance.
 */
UENUM(BlueprintType)
enum class EBRProgressTreatment : uint8
{
	/**
	 * Front end (menus, lobby, settings, match record, post-game XP). COMPONENT-SPECS Sec 0:
	 * the front end is white/black at varying alpha -- colour is accent only. The CHANNEL is
	 * deliberately IGNORED in this treatment: a lobby bar is chrome, full stop.
	 */
	FrontEndFlat,

	/**
	 * In-match HUD (vitals, cooldowns). Full VISR semantics: the fill takes the channel's
	 * meaning colour, and the Health channel's hidden-until-damaged rule is live.
	 */
	CombatVISR
};

/**
 * `ui-presentation` Sec 3 / UI-DESIGN-SYSTEM Sec 2 -- the VISR channels, which are semantic and
 * not decorative. The channel says WHAT THE BAR IS ABOUT; the caller never passes a colour.
 *
 * Mapping for this project's three uses of the component:
 *   vitals shields -> Shield ..... cooldowns -> Amber ("a clock is running")
 *   vitals health  -> Health ..... incoming-damage / shield-break flash -> Enemy
 *   match record, post-game XP -> FrontEndFlat treatment, channel Neutral
 */
UENUM(BlueprintType)
enum class EBRProgressChannel : uint8
{
	/** No channel meaning. Chrome. The only correct value for a front-end bar. */
	Neutral,

	/** `#35D0F2` -- YOU. Your shields, your team, your grapple. */
	Shield,

	/** `#F5C542` -- health beneath shields. Yellow, never green. Hidden until damaged. */
	Health,

	/** `#FFA333` -- a clock is running. Cooldowns, countdowns. */
	Amber,

	/** `#FF4A3D` -- threat, and ONLY threat. Not a low-ammo warning, not a UI error. */
	Enemy
};

/**
 * `UBRProgressBar` -- one bar, two treatments (SCREEN-MANIFEST Sec 5 Tier 4; UI-DESIGN-SYSTEM
 * Sec 4 lists it as used for "vitals, cooldowns and match record", which is exactly why the
 * treatment axis has to be explicit).
 *
 * BASE: `UCommonUserWidget`. It is not interactive, so it is NOT a `UCommonButtonBase` -- but it
 * is the same base family as every other widget in this folder (`UBRMenuRow`'s
 * `UCommonButtonBase` IS a `UCommonUserWidget`). There is still exactly one widget base.
 *
 * WHAT THIS CLASS DOES NOT DO, DELIBERATELY -- two timings MOTION-MEASURED Sec 7 forbids
 * transferring, recorded here so nobody "fixes" the bar to match the reference later:
 *
 *   1. THE REFERENCE'S PROGRESS-FILL TIMING IS NOT A TIMING. `MOTION-MEASURED` Sec 4: the VISR
 *      install bar advances in FOUR unequal scripted jumps (5 -> 17 -> 141 -> 269 -> 280 px of a
 *      300 px track, 60-90 ms apart) over 360 ms. That is trailer choreography authored to look
 *      busy, not a data-driven bar. "Do not copy its step timings into a real loader." A real
 *      bar is driven by actual completion, so this class has NO fill duration, NO step table and
 *      NO self-animated fill: `SetPercent` snaps to the value it is given.
 *
 *   2. THE FULL-SCREEN LOADING REFERENCE IS NOT A UI ANIMATION. `MOTION-MEASURED` Sec 5: Shader
 *      Cache is a pre-rendered 3D cinematic (change signal never drops below 8.1 across 90
 *      frames; phase count 0) exported at 10 fps, which cannot resolve any transition shorter
 *      than 100 ms. Its 9.6 s length and 600 ms closing fade are film-editorial numbers. Neither
 *      transfers to a widget, and no loading bar in this project may cite them.
 *
 * SMOOTHING. Presentation smoothing (the shield-bar lerp) is a VISUAL interpolation and it lives
 * on the ViewModel -- `ue5-ui-architecture` Sec 3 and ARCHITECTURE Sec 5.3 put it there and are
 * explicit that "it never feeds back into simulation". So the smoothed value arrives here already
 * smoothed, this class holds no timer and no Tick, and there is no path from this widget back
 * into the sim: every member below is display state. If a bar ever looks like it lags the
 * attribute, the fix is on the ViewModel, not here.
 *
 * NO POLLING: `meta = (DisableNativeTick)`. The screen binds a FieldNotify ViewModel field to
 * `SetPercent` through MVVM; nothing here asks for state.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRProgressBar : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UBRProgressBar();

	/**
	 * No module geometry is declared here. The old 536x152 constants described ONE Figma
	 * art-board and were contradicted by every shipped instance (273.33x34 vitals, 38x13
	 * cooldown) — a class that ships at three sizes owns none of them; the WBP does
	 * (HUD-CPP-AUDIT stale-comment sweep).
	 */

	/**
	 * "Full" -- the boundary for the Health channel's hidden-until-damaged rule
	 * (UI-DESIGN-SYSTEM Sec 2, Halo Support, 343's own behaviour: the health bar "appears only
	 * after damage"). This is a STRUCTURAL boundary -- undamaged vs damaged -- not a tuning
	 * number. Any threshold that changes a bar's COLOUR (low health flashes, critical shield) is
	 * a gameplay number and belongs in a `CT_Combat` row (law 3); none is implemented here and
	 * none may be typed into the WBP.
	 */
	static constexpr float FullPercent = 1.0f;

	/** Display setter. Clamped 0..1. Snaps -- see the two forbidden transfers in the class doc. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetPercent(float InPercent);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	float GetPercent() const { return Percent; }

	/**
	 * Honest empty state (`ue5-ui-architecture` Sec 7). A late joiner has no PlayerState for one
	 * or more frames and the ASC's attribute delegates have not fired, so a bar that renders a
	 * confident 0% or 100% is telling the player something false about a fight. Unknown collapses
	 * the fill and prints `UnknownValueText`; it is the state this widget CONSTRUCTS in.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetPercentUnknown();

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetTreatment(EBRProgressTreatment InTreatment);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRProgressTreatment GetTreatment() const { return Treatment; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetChannel(EBRProgressChannel InChannel);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRProgressChannel GetChannel() const { return Channel; }

	/** The right-hand readout ("142 / 200", "0:45"). Formatting is the screen's job, not ours. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetValueText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetLabelText(const FText& InText);

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	//~ End UUserWidget interface

	/**
	 * The single place the treatment/channel pair becomes a colour. Both setters and
	 * initialisation route through it, so a HUD bar and a lobby bar can never diverge by
	 * accident -- they diverge only because `Treatment` says so.
	 */
	void ApplyTreatment();

	/** Resolves the fill colour. FrontEndFlat ignores the channel; CombatVISR obeys it. */
	FLinearColor ResolveFillColor() const;

	/** Health-hidden-until-damaged and the unknown state both land here. */
	void ApplyBarVisibility();

	// ---------------------------------------------------------------------------------------
	// BindWidget contract. These names are the authoring contract for `WBP_ProgressBar` and its
	// HUD sibling; the WBP must name its widgets exactly this. Figma -> UMG mapping:
	//   `Progress Bar` frame -> Frame      the fill track -> Fill
	//   track ground plate   -> Track      value readout  -> ValueText
	//   caption              -> LabelText
	// ---------------------------------------------------------------------------------------

	/**
	 * The fill. A stock `UProgressBar` because UMG already draws a clipped fill with a percent --
	 * a bespoke Slate leaf here would buy nothing that `UBRHairlineBorder` did not already buy
	 * for strokes. Its style brushes are set in the WBP as flat colour brushes (radius 0
	 * everywhere, COMPONENT-SPECS Sec 0); C++ owns the FILL COLOUR because that carries meaning.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UProgressBar> Fill;

	/** Optional ground plate behind the fill. COMPONENT-SPECS Sec 8: five discrete black alphas. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Track;

	/**
	 * MOTION-MEASURED headline, "the container frame does not animate": the reference's outer
	 * rectangle goes 43 px -> 278 px in ONE frame (30 ms). Snap the frame, ease only the fill.
	 * C++ pins its stroke weight so 31 screens cannot fork it; nothing else here touches it.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> Frame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> ValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> LabelText;

	/**
	 * NOT EditAnywhere, deliberately (HUD-CPP-AUDIT): `BRVitalsWidget.h` promises "no colour
	 * is ever passed in and none may be typed into the WBP" — an editable channel was that
	 * details panel. `SetTreatment`/`SetChannel` from the owning C++ are the only path, which
	 * is what makes the tier rule enforceable.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRProgressTreatment Treatment = EBRProgressTreatment::FrontEndFlat;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRProgressChannel Channel = EBRProgressChannel::Neutral;

	/** COMPONENT-SPECS Sec 8. A token name, never a colour; per-asset styling is legitimate
	 *  here, so it stays designer-editable — and previews via NativePreConstruct below. */
	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|UI")
	EBRUIColorToken TrackGroundToken = EBRUIColorToken::PanelGround40;

	/** Printed instead of a number while the value is unknown. Set ONCE in the constructor —
	 *  unknown is one concept and gets one mark, so no WBP may fork it. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText UnknownValueText;

private:
	/** Display state only. Nothing in this class is authoritative or writes anything back. */
	float Percent = 0.0f;

	/** False until a ViewModel has actually pushed a value. Constructs false, on purpose. */
	bool bHasValue = false;
};
