#pragma once

#include "CommonButtonBase.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRButton.generated.h"

class UBRHairlineBorder;
class UBRRule;
class UBRSettingsDataObject;
class UCommonTextBlock;
class UImage;
class UPanelWidget;
class USizeBox;
class UWidget;
class UWidgetAnimation;
class UWidgetSwitcher;

// =============================================================================================
// THE BUTTON MODULE -- one seam, and it depends on NOTHING but CommonUI, UMG and the tokens.
//
// Founder directive, 4 Aug 2026: "make sure that class is independent from any other class,
// only the native one ... from CommonUI. And for the button types, we can have that as data
// that will let the class know which button it is, so it could be changeable."
//
// So the include list above is the contract: `CommonButtonBase.h` is CommonUI, the forward
// declarations are UMG, and `BRComponentTokens.h` is a header-only table of constants -- data,
// not a class. There is no BR class dependency in this file and adding one is a finding.
//
// WHAT MERGED HERE, AND WHY IT IS ONE FILE:
//   Sec 1  the button STYLES        (was Styles/BRButtonStyles.h)
//   Sec 2  the TYPE axis            (was Components/BRMenuRow.h)
//   Sec 3  UBRButton                (was UBRMenuRow, Components/BRMenuRow.h)
//   Sec 4  UBRSettingsRow           (was Components/BRSettingsRow.h)
//   Sec 5  UBRHighlightButton       (was Components/BRHighlightButton.h)
//
// A file is an OWNERSHIP SEAM, not a class. These five are edited by one ticket, they share one
// design contract, and the styles are resolved by PATH (`/Script/Breachpoint.BRButtonStyle_*`)
// rather than by include -- verified: the old BRButtonStyles.h was included by exactly one .cpp,
// its own. So this merge costs zero include churn anywhere in the module.
//
// EVERY BUTTON CLASS IN THE PROJECT IS IN THIS FILE. That is the point of it: "we should only
// have BRButton for the source" (founder, 4 Aug). Sec 5's rules stay its own -- merging the
// FILE is not merging the BEHAVIOUR, and the warning in Sec 5 explains exactly which is which.
//
// WHAT DID NOT MERGE, AND THE MEASUREMENT THAT DECIDED IT:
//   `UBRHairlineBorder` -- 12 files include it, 11 of them not buttons (FeatureCard, ItemTile,
//     LeftRail, NavBar, PageTitle, Panel, ProgressBar, RosterPanel, Scrim, TableRow). Folding it
//     here would make eleven unrelated widgets include the button module TO PAINT A LINE, which
//     is dependency inversion wearing a file merge.
//   `BRComponentTokens.h` -- 23 consumers across components, HUD, screens and settings.
//   `BRUITokens.h` / `BRTextStyles.h` -- 9 and 2.
// None of those three is button source. They are the drawing layer and the design system that
// buttons CONSUME, no more this module's than `CommonButtonBase.h` is.
// =============================================================================================

// ============================================================================ Sec 1
// THE STYLES -- state brushes and text styles, resolved by PATH not by include.
// ============================================================================

/**
 * Base for every BREACHPOINT button style. C++, not a BP style asset, for the same
 * reason as UBRTextStyleBase: a BP child of an ENGINE class fails R26 conditions 1 and 5
 * and is therefore banned by R18. NotBlueprintable makes it structural.
 *
 * No asset references live in here at all. Every brush is an FSlateColorBrush -- a solid
 * tint with DrawAs = Image, no texture, no material, no ResourceObject. That is a
 * deliberate double win: nothing to load (so none of UBRTextStyleBase's deferred-resolve
 * machinery is needed), and DrawAs = Image cannot round a corner, so "corner radius 0
 * everywhere" is enforced by the type rather than by a review comment. If a style ever
 * needs ESlateBrushDrawType::RoundedBox, that is a design change, not an implementation
 * detail.
 *
 * Stroke thickness and whole-component opacity are NOT expressible on UCommonButtonStyle
 * -- it has brushes, padding and size clamps, nothing else. They live as
 * BR::Tokens::Stroke* / Opacity* constants for the widget layer to read.
 */
UCLASS(Abstract, NotBlueprintable)
class BREACHPOINT_API UBRButtonStyleBase : public UCommonButtonStyle
{
	GENERATED_BODY()

public:
	UBRButtonStyleBase();
};

/**
 * The left-rail menu row, and the signature interaction of the whole front end: the
 * idle -> hover INVERSION. At rest the row is transparent with a dim underline; on hover
 * or selection the fill goes solid white and the text flips to black.
 *
 * The underline itself (StrokeThin at OpacityMenuRowLineIdle -> OpacityMenuRowLineActive)
 * is a widget-owned element -- UCommonButtonStyle has no line field. Tokens carry it.
 */
UCLASS()
class BREACHPOINT_API UBRButtonStyle_MenuRow : public UBRButtonStyleBase
{
	GENERATED_BODY()

public:
	UBRButtonStyle_MenuRow();
};

/**
 * Top nav tab. Active state is a StrokeNavTabActive border drawn OUTSIDE the component;
 * inactive dims the WHOLE component to OpacityNavTabInactive -- not just the label.
 * Both are widget-layer concerns (see tokens); this style carries the fills and type.
 */
UCLASS()
class BREACHPOINT_API UBRButtonStyle_NavTab : public UBRButtonStyleBase
{
	GENERATED_BODY()

public:
	UBRButtonStyle_NavTab();
};

/** The one-per-screen affirmative action. Amber = a clock is running / act now. */
UCLASS()
class BREACHPOINT_API UBRButtonStyle_Highlight : public UBRButtonStyleBase
{
	GENERATED_BODY()

public:
	UBRButtonStyle_Highlight();
};

// ============================================================================ Sec 2
// THE TYPE AXIS -- the DATA that tells the class which button it is.
// ============================================================================


/**
 * COMPONENT-SPECS Sec 2 "Type axis (10 values)": what sits INSIDE the same 250x28 shell.
 * The order is the authoring contract -- the WBP's `TypeSwitcher` child index must match it,
 * because C++ selects the body by casting this enum to the switcher index.
 *
 * There are 27 variants in the reference file. There is ONE class. The 27 are
 * (10 Types x 6 Statuses x 2 Alignments) collapsed onto three axes: Type is this enum, Status
 * is CommonUI's own button state, Alignment is EBRButtonAlignment.
 */
UENUM(BlueprintType)
enum class EBRButtonType : uint8
{
	Default,

	/** A Type value in the reference file as well as a Status; kept for 1:1 fidelity. */
	Disabled,

	DropDown,

	DigDown,

	/** COMPONENT-SPECS Sec 2: 40 x 40, not 250 x 28. */
	IconOnly,

	Slider,

	Checkbox,

	Radio,

	/** COMPONENT-SPECS Sec 2: 250 x 60. */
	MapVoting,

	/** COMPONENT-SPECS Sec 2: 250 x 120. */
	Image
};

/** COMPONENT-SPECS Sec 2 "Alignment axis". */
UENUM(BlueprintType)
enum class EBRButtonAlignment : uint8
{
	Left,

	Center
};

// ============================================================================ Sec 3
// UBRButton -- the one modular button.
// ============================================================================

/**
 * `UBRButton` -- the atom (SCREEN-MANIFEST Sec 5 Tier 0: unblocks 26 of 31 screens; the
 * highest-leverage single component in the project).
 *
 * BASE: `UCommonButtonBase`, which IS a `UCommonUserWidget` -- there is still exactly one widget
 * base. It is used rather than a bare `UCommonUserWidget` because a menu row must be
 * hover/press/select-aware and gamepad-focusable, and CommonUI already owns focus, navigation,
 * held-action progress and the input-action glyph. Re-deriving any of that here would be both
 * dead code and a gamepad-routing finding (`ue5-ui-architecture` Sec 6).
 *
 * THE STATE MODEL, which is the whole component:
 * COMPONENT-SPECS Sec 1 -- "Idle -> Hover is an INVERSION, not a highlight." Three things move
 * together and nothing else does:
 *   1. the Text Frame fill goes from nothing to solid white (`SurfaceInverted`),
 *   2. the label and selection text go black (`TextInverted`),
 *   3. the border's bottom line goes from 0.3 to 1.0 -- i.e. `Border->SetEdgeDimmed(Bottom,false)`.
 * `Disabled` changes no geometry at all, only opacity (Sec 2 status table).
 *
 * NO GRAPH NODES: state transitions are driven from C++ here; the WBP contributes layout plus
 * OPTIONAL `BindWidgetAnim` timelines that C++ plays. `ui-presentation` Sec 11 wants a WBP with
 * zero graph nodes, so this class deliberately exposes no BlueprintImplementableEvent.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/**
	 * Exists ONLY to set the designer preview mode, and that is the only way to set it.
	 *
	 * `UUserWidget::DesignSizeMode` is `WITH_EDITORONLY_DATA` declared as a bare `UPROPERTY()`
	 * with no edit specifier (UserWidget.h:1534), so it is invisible to the details panel, to
	 * `ObjectTools.set_properties` and to every config default — verified: a WidgetBlueprint
	 * returns 107 properties over MCP and this is not among them, so a write reads back null.
	 * The designer toolbar dropdown is the only UI for it, and a per-asset click is not
	 * something a generator can reproduce.
	 *
	 * Setting it on the CDO here gives every Menu Row asset `Desired` at creation, so a
	 * 250 x 28 row previews as 250 x 28 instead of floating in a 1280 x 720 canvas. Pure
	 * editor presentation: `WITH_EDITORONLY_DATA` means it does not exist in a packaged build.
	 */
	UBRButton(const FObjectInitializer& ObjectInitializer);

	/**
	 * COMPONENT-SPECS Sec 2: COMPONENT 250 x 28. `RowWidth` is deliberately NOT declared — the
	 * row fills its rail (349 / 536 / a column's Fill) and 250 is the component-board width,
	 * recorded in `MCP-BUILD-PLANS.md` with the text-frame and icon geometry the WBP authors
	 * (CPP-AUDIT cut the seven constants that mirrored it here unread).
	 */
	static constexpr float RowHeight = 28.0f;

	/** COMPONENT-SPECS Sec 2: the border's side lines are 20 tall on a 28 row -- ticks, not edges. */
	static constexpr float BorderSideTickLength = 20.0f;

	/**
	 * The component-board width, applied AT DESIGN TIME ONLY (see ApplyButtonType).
	 *
	 * This is not a contradiction of "width is deliberately not authored" -- that rule is about
	 * RUNTIME, where the row fills its rail (349 on the front end, 536 in the grid stack) and a
	 * pinned 250 would break every wider list silently. But with no width at all the designer
	 * preview stretches to whatever the canvas is, so the asset can never be compared 1:1
	 * against the 250 x 28 component board it was measured from. Design time gets the board
	 * width; the running game gets none, and `ClearWidthOverride` on the runtime path is what
	 * keeps that promise.
	 */
	static constexpr float ComponentBoardWidth = 250.0f;

	/** COMPONENT-SPECS Sec 2 Type axis: the three types that change the 250 x 28 shell. */
	static constexpr float IconOnlySize = 40.0f;
	static constexpr float MapVotingHeight = 60.0f;
	static constexpr float ImageHeight = 120.0f;

	/** COMPONENT-SPECS Sec 2 status table: Disabled dims every child to 0.5, geometry unchanged. */
	static constexpr float DisabledOpacity = 0.5f;

	/** ui-presentation Sec 5 / SCREEN-MANIFEST Sec 1: rows are h28 on a 40 pitch (12 px gap). */
	static constexpr float RowPitch = 40.0f;

	/**
	 * The two scalars `M_UI_MenuRowPlate` exposes, driven on `TextFrameFill`'s dynamic material
	 * when the plate carries one.
	 *
	 * WHY A MATERIAL AT ALL, when `SetColorAndOpacity` already does the inversion: a tint is a
	 * step function. The plate can only be transparent or white, so hover SNAPS. A material
	 * taking a 0..1 scalar can be eased, and the widget layer only has to move a float — which
	 * is the shape `M_SimpleGlow` uses for its own Hover/Pressed (see `mcp-ui/MATERIALS.md`).
	 *
	 * BOTH ARE DECLARED even though COMPONENT-SPECS Sec 2 gives them the SAME plate ("Pressed is
	 * the same plate -- the flip already reads as a press"). The material folds them with a
	 * clamped add, so today they are interchangeable; they are separate parameters so that a
	 * future press treatment is an instance tweak rather than a graph rebuild.
	 *
	 * `material_plan.py`'s `cpp_constants` gate parses these literals and fails the material
	 * build if either drifts from the parameter the graph declares.
	 */
	static const FName PlateHoverParameterName;
	static const FName PlatePressedParameterName;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetLabelText(const FText& InText);

	/** COMPONENT-SPECS Sec 2: the right-aligned `Selection` text -- the value on a settings row. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetSelectionText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetButtonType(EBRButtonType InButtonType);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRButtonType GetButtonType() const { return ButtonType; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetButtonAlignment(EBRButtonAlignment InAlignment);

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;

	/** CPP-AUDIT P3: `ButtonType`/`Alignment` are EditAnywhere and must preview in the designer. */
	virtual void NativePreConstruct() override;
	//~ End UUserWidget interface

	//~ Begin UCommonButtonBase interface
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	virtual void NativeOnEnabled() override;
	virtual void NativeOnDisabled() override;
	//~ End UCommonButtonBase interface

	/**
	 * The single place the inversion is expressed. Both callers (hover and selection) route
	 * here so an inverted-selected row and an inverted-hovered row can never diverge.
	 */
	void ApplyInvertedState(bool bInverted, bool bPlayAnimation = true);

	void ApplyButtonType();

	/**
	 * COMPONENT-SPECS Sec 2, every Type's hover row: "everything flips to #000000". The shell
	 * inversion above reaches Label and Selection only, so without this the per-type body --
	 * the disclosure triangle, the checkbox square, the radio fill, the slider track, the
	 * gametype emblem -- stays WHITE on a plate that just went white, i.e. it disappears at
	 * exactly the moment the row is focused. That is the whole reason this walk exists.
	 *
	 * Recursive rather than a fixed bind list because the Type axis holds ten different
	 * bodies with different child counts; one traversal covers all ten and every future one.
	 * Depth is bounded by the WBP's own tree (COMPONENT-SPECS' deepest body is four levels),
	 * and this runs on a state change, not on tick.
	 */
	void ApplyInversionToSubtree(UWidget* Root, const FSlateColor& InTextColor, bool bInverted);

	/** Show `TypeCheckMark` iff the row is selected. Idle/Hover are an empty box (Sec 2). */
	void ApplySelectedMark(bool bInSelected);

	/**
	 * Drive the plate's `Hover`/`Pressed` scalars, if `TextFrameFill` carries a material.
	 *
	 * Returns FALSE when it does not — which is the normal case today. Every Menu Row asset
	 * built so far uses a BRUSHLESS `UImage` that `ApplyInvertedState` tints directly, and that
	 * path is untouched: this is additive, and an asset opts in purely by having
	 * `M_UI_MenuRowPlate` as its brush. A `false` return is not a failure, it is "this asset
	 * does it the other way", and the caller falls back to the tint.
	 *
	 * `bAnimationDrivesHover` IS THE HANDOFF, and without it the two fight. `InvertAnim` keys
	 * the same `Hover` scalar; if C++ also snapped it to the target first, the plate would jump
	 * to full and then animate from zero — a visible flicker on every hover. When an animation
	 * is going to play, C++ sets the tint and the animation owns the scalar.
	 */
	bool ApplyPlateMaterialState(bool bInverted, bool bAnimationDrivesHover);

	// ---------------------------------------------------------------------------------------
	// BindWidget contract. These names are parsed out of this header by the WBP generator and
	// must match the widget names in `WBP_Button*` assets exactly. Figma layer -> UMG name mapping:
	//   `Text Frame`    -> TextFrame        `Text`      -> Label
	//   `Selection`     -> Selection        `Icon`      -> Icon
	//   `Filter Button` -> FilterButton     `Border`    -> Border
	//   `Background Line` (hover-only)      -> BackgroundLine
	// ---------------------------------------------------------------------------------------

	/** Root box. C++ drives W/H from the Type axis so 10 sizes are not 10 WBP variants. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/** COMPONENT-SPECS Sec 2 Border: four vector lines, stroke align CENTER. One widget. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> Border;

	/** The 246x24 plate that goes solid white on hover. Transparent when idle. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UImage> TextFrameFill;

	/** COMPONENT-SPECS Sec 2: auto-layout HORIZONTAL, primaryAxis MIN, counterAxis CENTER. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> TextFrame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Label;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> Selection;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> FilterButton;

	/**
	 * COMPONENT-SPECS Sec 2 hover: "an extra Background Line (opacity 0.3) appears behind".
	 * Collapsed when idle.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRRule> BackgroundLine;

	/**
	 * One child per EBRButtonType, in enum order. This is what keeps the 10-value Type axis
	 * from becoming 10 classes or 10 WBPs.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidgetSwitcher> TypeSwitcher;

	/**
	 * The per-type body -- whatever sits inside the shell for this row's Type. Everything
	 * beneath it follows the inversion (see ApplyInversionToSubtree). Optional: the Default
	 * type has no body beyond Label, and a null root simply means the walk does nothing.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UPanelWidget> TypeBody;

	/**
	 * The ONE measured exception to "everything flips to black" (COMPONENT-SPECS Sec 2,
	 * Slider hover): the handle `Circle` inverts its FILL to black but keeps a white 1px
	 * stroke, so it stays visible on the inverted plate. A blanket tint would erase it.
	 * Named as a bind rather than by string match so the exemption is greppable from the WBP.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> InversionExempt;

	/**
	 * The ACTIVE-ONLY mark: the checkbox's tick, the radio's 10x10 inner fill.
	 *
	 * COMPONENT-SPECS Sec 2 gives Checkbox and Radio four states, and the mark tracks exactly
	 * one axis of them: Idle and Hover are an EMPTY box, Active and Active Hover carry the
	 * mark. So it follows selection, NOT the hover inversion -- pointing at an unchecked box
	 * must not make it look checked. Without this the tick is painted permanently and the
	 * control can only ever render one of its four measured states.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> TypeCheckMark;

	/** Played forward on hover / selection, reverse on release. Appearance only. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> InvertAnim;

	/** COMPONENT-SPECS Sec 2 Active: "the disclosure glyph rotates". Appearance only. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> DisclosureAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRButtonType ButtonType = EBRButtonType::Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRButtonAlignment Alignment = EBRButtonAlignment::Left;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken InvertedFillToken = EBRUIColorToken::SurfaceInverted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken InvertedTextToken = EBRUIColorToken::TextInverted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken IdleTextToken = EBRUIColorToken::TextPrimary;

private:
	bool bIsInverted = false;
};

// ============================================================================ Sec 4
// UBRSettingsRow -- behaviour, not appearance: it holds a data object.
// ============================================================================


/**
 * `UBRSettingsRow` -- a `UBRButton` that knows which setting it is showing.
 *
 * IT IS A SUBCLASS AND THAT IS NOT A CONTRADICTION. `UBRButton`'s own doc says 27 reference
 * variants collapse to ONE class because they differ only in appearance. This differs in
 * BEHAVIOUR: it holds a data object, subscribes to it, and turns left/right into a value change.
 * That is a new responsibility, not a new look, and it is exactly what a subclass is for. The
 * Type axis is still `UBRButton`'s -- this class picks a value on it, it does not add one.
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
class BREACHPOINT_API UBRSettingsRow : public UBRButton
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
	 * Which `EBRButtonType` body a setting renders as -- WITHOUT needing a row to ask.
	 *
	 * Static because the screen has to pick a WIDGET CLASS before it can create the widget, and
	 * the type is what decides which class. `RefreshFromSetting` used to compute this on an
	 * already-built row and set `ButtonType` on it, which changed the row's height and nothing else:
	 * the per-type bodies live in different assets, so a Scalar setting and a Discrete one both
	 * rendered as a plain Default row. Resolving before construction is what closes that.
	 */
	static EBRButtonType ResolveButtonTypeFor(const UBRSettingsDataObject* InSetting);

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

	/** Which `EBRButtonType` body this setting renders as. */
	EBRButtonType ResolveButtonType() const;

private:
	void HandleSettingChanged(UBRSettingsDataObject* Changed);

	/** Drop the delegate binding. Called from `SetSetting` and `NativeDestruct`. */
	void UnsubscribeFromSetting();

	UPROPERTY(Transient)
	TObjectPtr<UBRSettingsDataObject> Setting;
};

// ============================================================================ Sec 5
// UBRHighlightButton -- the OTHER button, and it stays other.
//
// MERGED HERE 4 Aug 2026, and the merge is FILE-ONLY. Its own doc says it plainly:
// "Two components, two rules -- do not unify them on the assumption that 'inversion'
// means one thing." A menu row inverts to WHITE; a highlight button fills with a per-type
// ACCENT (#2ec3e5 Main, #ff5c00 Event, #ffc11c Premium). That warning is about BEHAVIOUR
// and it still binds -- nothing below was folded into `UBRButton`, and `EBRHighlightButtonType`
// is deliberately NOT `EBRButtonType`. They are two axes on two components.
//
// What the merge IS: a file is an ownership seam. Both classes are buttons, both are edited
// by the button ticket, and `UBRButtonStyle_Highlight` (Sec 1) now sits beside its only
// consumer instead of in a styles file nobody opened. Zero files included the old header,
// so this cost no include churn anywhere.
// ============================================================================


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
