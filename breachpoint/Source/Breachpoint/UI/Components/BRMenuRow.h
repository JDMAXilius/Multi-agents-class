#pragma once

#include "CommonButtonBase.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRMenuRow.generated.h"

class UBRHairlineBorder;
class UBRRule;
class UCommonTextBlock;
class UImage;
class USizeBox;
class UWidget;
class UWidgetAnimation;
class UWidgetSwitcher;

/**
 * COMPONENT-SPECS Sec 2 "Type axis (10 values)": what sits INSIDE the same 250x28 shell.
 * The order is the authoring contract -- the WBP's `TypeSwitcher` child index must match it,
 * because C++ selects the body by casting this enum to the switcher index.
 *
 * There are 27 variants in the reference file. There is ONE class. The 27 are
 * (10 Types x 6 Statuses x 2 Alignments) collapsed onto three axes: Type is this enum, Status
 * is CommonUI's own button state, Alignment is EBRMenuRowAlignment.
 */
UENUM(BlueprintType)
enum class EBRMenuRowType : uint8
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
enum class EBRMenuRowAlignment : uint8
{
	Left,

	Center
};

/**
 * `UBRMenuRow` -- the atom (SCREEN-MANIFEST Sec 5 Tier 0: unblocks 26 of 31 screens; the
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
class BREACHPOINT_API UBRMenuRow : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/**
	 * COMPONENT-SPECS Sec 2: COMPONENT 250 x 28. `RowWidth` is deliberately NOT declared — the
	 * row fills its rail (349 / 536 / a column's Fill) and 250 is the component-board width,
	 * recorded in `MCP-BUILD-PLANS.md` with the text-frame and icon geometry the WBP authors
	 * (CPP-AUDIT cut the seven constants that mirrored it here unread).
	 */
	static constexpr float RowHeight = 28.0f;

	/** COMPONENT-SPECS Sec 2: the border's side lines are 20 tall on a 28 row -- ticks, not edges. */
	static constexpr float BorderSideTickLength = 20.0f;

	/** COMPONENT-SPECS Sec 2 Type axis: the three types that change the 250 x 28 shell. */
	static constexpr float IconOnlySize = 40.0f;
	static constexpr float MapVotingHeight = 60.0f;
	static constexpr float ImageHeight = 120.0f;

	/** COMPONENT-SPECS Sec 2 status table: Disabled dims every child to 0.5, geometry unchanged. */
	static constexpr float DisabledOpacity = 0.5f;

	/** ui-presentation Sec 5 / SCREEN-MANIFEST Sec 1: rows are h28 on a 40 pitch (12 px gap). */
	static constexpr float RowPitch = 40.0f;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetLabelText(const FText& InText);

	/** COMPONENT-SPECS Sec 2: the right-aligned `Selection` text -- the value on a settings row. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetSelectionText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetRowType(EBRMenuRowType InRowType);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRMenuRowType GetRowType() const { return RowType; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetRowAlignment(EBRMenuRowAlignment InAlignment);

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;

	/** CPP-AUDIT P3: `RowType`/`Alignment` are EditAnywhere and must preview in the designer. */
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

	void ApplyRowType();

	// ---------------------------------------------------------------------------------------
	// BindWidget contract. These names are parsed out of this header by the WBP generator and
	// must match the widget names in `WBP_MenuRow` exactly. Figma layer -> UMG name mapping:
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
	 * One child per EBRMenuRowType, in enum order. This is what keeps the 10-value Type axis
	 * from becoming 10 classes or 10 WBPs.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidgetSwitcher> TypeSwitcher;

	/** Played forward on hover / selection, reverse on release. Appearance only. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> InvertAnim;

	/** COMPONENT-SPECS Sec 2 Active: "the disclosure glyph rotates". Appearance only. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> DisclosureAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRMenuRowType RowType = EBRMenuRowType::Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRMenuRowAlignment Alignment = EBRMenuRowAlignment::Left;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken InvertedFillToken = EBRUIColorToken::SurfaceInverted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken InvertedTextToken = EBRUIColorToken::TextInverted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken IdleTextToken = EBRUIColorToken::TextPrimary;

private:
	bool bIsInverted = false;
};
