#pragma once

#include "CommonButtonBase.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRHighlightButton.generated.h"

class UBRHairlineBorder;
class UCommonTextBlock;
class UImage;
class UWidget;
class UWidgetAnimation;
class UWidgetSwitcher;

/**
 * REFERENCE-EXTRACTION Sec 5, `Highlight Buttons`: 13 variants on two axes --
 * Status (Idle / Hover / On Click) x Type (Main / Event / Disabled / Premium / Boring /
 * Photo Button). The reference does not author every product of the two axes, which is why the
 * count is 13 and not 18.
 *
 * THIRTEEN VARIANTS, ONE CLASS, ZERO SUBCLASSES:
 *   - Status is CommonUI's own button state. Hover/press/select/focus/gamepad focus all belong
 *     to `UCommonButtonBase` and re-deriving any of it here would be a `ue5-ui-architecture`
 *     Sec 6 routing finding as well as dead code.
 *   - Type is this enum. The order is the authoring contract: `TypeSwitcher`'s child index in
 *     the WBP must match it, because C++ selects the body by casting this enum to the index.
 */
UENUM(BlueprintType)
enum class EBRHighlightButtonType : uint8
{
	/** The default call-to-action. Chrome only. */
	Main,

	/** COMPONENT-SPECS Sec 8 `#ff5c00` "event / urgent" -- see the accent gap in the class doc. */
	Event,

	/** A Type value in the reference as well as a Status; kept for 1:1 fidelity. */
	Disabled,

	/** COMPONENT-SPECS Sec 8 `Premium Yellow` `#ffc11c` -- battle pass / store. */
	Premium,

	/** The low-emphasis variant: same shell, no accent. */
	Boring,

	/** Image-led. The art is the button; the label sits over it. */
	PhotoButton
};

/**
 * `UBRHighlightButton` -- page-level call-to-action (SCREEN-MANIFEST Sec 5 Tier 1: 11 of 31
 * screens; the confirm/cancel pair on `OV_Warning`, the purchase button on `SH_BundleDetail` and
 * `NW_BattlePass` at Type=Premium, `PR_PostGameXP` and `PR_RankUp`'s continue).
 *
 * BASE: `UCommonButtonBase` -- the same base as `UBRButton`, so there is still exactly one
 * widget base family. CommonUI owns hover, press, select, focus, gamepad navigation and the
 * input-action glyph. The On Click status is CommonUI's Pressed brush on the shared
 * `CommonButtonStyle`, which is why this class overrides no press handler: a status that a style
 * asset already draws does not get a second implementation in C++.
 *
 * ONE EXCEPTION, and it is deliberate: the Disabled dim is applied HERE, not by CommonUI's
 * `bApplyAlphaOnDisable`. `SetIsInteractionEnabled` only applies that alpha when the Slate button
 * already exists, and Type=Disabled is set in `NativeOnInitialized` -- before `RebuildWidget` --
 * so an authored-disabled button would render at full opacity until something else toggled it.
 * `bApplyAlphaOnDisable` must therefore be OFF on the WBP CDO or the two dims compound.
 *
 * THE STATE MODEL, which is the whole component. COMPONENT-SPECS Sec 1 -- "Idle -> Hover is an
 * INVERSION, not a highlight. That single rule explains most of the file." Three things move
 * together and nothing else does:
 *   1. the fill plate goes from nothing to THE TYPE'S ACCENT (see `ResolveInvertedFillToken`),
 *   2. the label goes black (`TextInverted`),
 *   3. the border's bottom line goes 0.3 -> 1.0, i.e. `SetEdgeDimmed(Bottom, false)`.
 * It is implemented as ONE state (`ApplyInvertedState`) that hover and selection both route
 * through -- not as a tint applied at two call sites that will drift.
 *
 * THE HOVER PLATE IS NOT WHITE, AND THIS FILE USED TO SAY IT WAS. The old model here read "the
 * fill plate goes from nothing to solid white (`SurfaceInverted`)". Measured against the design
 * file 4 Aug 2026 (`Content/UI/Components/Buttons/Assets/01-HighlightButton.md`, Figma set
 * `12:1194`, all 13 variants), that is true for exactly ONE type -- `Boring` -- and wrong for the
 * other five: hover fills with the type's own accent, `#2ec3e5` Main, `#ff5c00` Event, `#ffc11c`
 * Premium. Photo Button follows Main. The white rule was presumably generalised from the one
 * variant that has no accent.
 *
 * `UBRButton` REALLY DOES INVERT TO WHITE and its doc is correct. Two components, two rules --
 * do not unify them on the assumption that "inversion" means one thing.
 *
 * ACCENT GAP — CLOSED FOR HOVER, STILL OPEN FOR IDLE. `AccentEvent` / `AccentPremium` /
 * `AccentHighlight` exist in `BR::Tokens` and their hex matches the measured file exactly, so the
 * hover half needs no new token and forks no palette. IDLE is a different shape of problem: the
 * file paints it with THREE STACKED PAINTS -- `#000000@0.8`, then a linear gradient
 * `#000000@0.3 -> accent` at `@0.8`, then `#000000@0.2` -- which a flat `SetColorAndOpacity` on a
 * `UImage` cannot express at all. `ResolveIdleFillToken` therefore still returns `None`; closing
 * it needs a gradient brush or material, not another enum value.
 *
 * NO GRAPH NODES: transitions are driven from C++; the WBP contributes layout plus an OPTIONAL
 * `BindWidgetAnim` timeline that C++ plays. `ui-presentation` Sec 11 wants a WBP with zero graph
 * nodes, so this class exposes no BlueprintImplementableEvent.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRHighlightButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/**
	 * MOTION-MEASURED Sec 7 -- the ONLY legal timing source for this component. The panel/fill
	 * transition is 330 ms in and 330 ms out on the SAME curve (measured on the centre-anchored
	 * fill expand and its time-reversed collapse, RMSE 0.007 / 0.005). It is symmetric: there is
	 * no in/out pair with different curves.
	 *
	 * Not read by C++ -- `InvertAnim` is a WBP timeline, and this is the number that timeline
	 * must be authored to. It is declared here so the WBP author has one place to read it and so
	 * a reviewer can diff it; a duration typed straight into a widget animation is otherwise
	 * unreviewable (the same argument as `ui-presentation` Sec 9 makes for colour).
	 */
	static constexpr float InversionDurationSeconds = 0.330f;

	/**
	 * MOTION-MEASURED, "the single most important result: one house curve fits everything" --
	 * `Motion.Ease.Standard` = `cubic-bezier(0.45, 0.15, 0.10, 1.00)`, mean RMSE 0.066 across all
	 * five measured series, versus 0.191 for `ease-in-out` and 0.201 for linear. The reference
	 * lands hard and flat; it does not decelerate symmetrically. Reach for `ease-in-out` here and
	 * you are measurably drawing the wrong shape.
	 */
	static constexpr float EaseStandardP1X = 0.45f;
	static constexpr float EaseStandardP1Y = 0.15f;
	static constexpr float EaseStandardP2X = 0.10f;
	static constexpr float EaseStandardP2Y = 1.00f;

	/**
	 * MOTION-MEASURED Sec 3: 150 ms per beat, measured on three reward elements entering five
	 * frames apart. Any screen that reveals a ROW of these buttons staggers them by this and
	 * nothing else.
	 */
	static constexpr float StaggerIntervalSeconds = 0.150f;

	/** COMPONENT-SPECS Sec 2 status table: Disabled dims to 0.5 and changes no geometry. */
	static constexpr float DisabledOpacity = 0.5f;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetLabelText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetButtonType(EBRHighlightButtonType InButtonType);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRHighlightButtonType GetButtonType() const { return ButtonType; }

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget interface

	//~ Begin UCommonButtonBase interface
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	//~ End UCommonButtonBase interface

	/** The single place the inversion is expressed. Hover and selection both route here. */
	void ApplyInvertedState(bool bInverted, bool bPlayAnimation = true);

	void ApplyButtonType();

	/** Type -> idle fill token. See the ACCENT GAP note in the class doc for why this is `None`. */
	EBRUIColorToken ResolveIdleFillToken() const;

	/**
	 * Type -> HOVER fill token. The measured accent per type, not white -- see the class doc.
	 *
	 * This replaced an `InvertedFillToken` UPROPERTY that defaulted to `SurfaceInverted`. A
	 * details-panel field cannot be right here: the correct value is a FUNCTION OF `ButtonType`,
	 * so an editable one is just an invitation to author the five wrong combinations, and a WBP
	 * that set it would silently outrank the design system. One owner, in code.
	 */
	EBRUIColorToken ResolveInvertedFillToken() const;

	// ---------------------------------------------------------------------------------------
	// BindWidget contract -- the WBP must use these exact names. Figma layer -> UMG name:
	//   `Fill` / plate      -> Fill          `Text`   -> Label
	//   `Border`            -> Border        `Icon`   -> Icon
	//   type body switcher  -> TypeSwitcher  (one child per EBRHighlightButtonType, in order)
	// No size constants: REFERENCE-EXTRACTION Sec 5 lists `Highlight Buttons` WITHOUT measured
	// dimensions. Inventing one here would be a number with no source, so the WBP owns the box
	// until somebody measures the node -- filed as a contract gap.
	// ---------------------------------------------------------------------------------------

	/** The plate that goes solid white on hover. Transparent when idle (Main / Boring / Photo). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Fill;

	/** COMPONENT-SPECS Sec 2: four vector lines, never a closed box. One widget, four draws. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> Border;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Label;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Icon;

	/** One child per EBRHighlightButtonType, in enum order -- 6 bodies, not 6 classes. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidgetSwitcher> TypeSwitcher;

	/**
	 * Played forward on hover / selection, reverse on release. Appearance only. Author it at
	 * `InversionDurationSeconds` on the `EaseStandard*` curve above -- no other timing is legal.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> InvertAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRHighlightButtonType ButtonType = EBRHighlightButtonType::Main;

	// `InvertedFillToken` was REMOVED 4 Aug 2026. It defaulted to `SurfaceInverted` and was the
	// mechanism by which this class rendered every hover white. Its replacement is
	// `ResolveInvertedFillToken()`; see that declaration for why this is not an editable field.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken InvertedTextToken = EBRUIColorToken::TextInverted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken IdleTextToken = EBRUIColorToken::TextPrimary;

private:
	bool bIsInverted = false;
};
