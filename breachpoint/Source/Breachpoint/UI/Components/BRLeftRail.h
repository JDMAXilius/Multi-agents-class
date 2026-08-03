#pragma once

#include "CommonUserWidget.h"
#include "UI/Components/BRComponentTokens.h"
#include "UI/Components/BRMenuRow.h"

#include "BRLeftRail.generated.h"

class UBRFeatureCard;
class UBRHairlineBorder;
class UPanelWidget;
class USizeBox;
class UWidget;
class UWidgetAnimation;

/**
 * `UBRLeftRail` -- the `Menu Combo` (SCREEN-MANIFEST Sec 6.2: 6 of 31 screens, "the reason the
 * rail stays aligned"). Figma node `2382:32507`, 349 x 510 at (69, 138) on `FE_Play`.
 *
 * IT EXISTS SO THE RAIL CANNOT DRIFT. Play, Create, Community, Control Panel, Customize and the
 * lobby all draw the same column-1 instrument. Authored per screen, that is six places the
 * feature card / menu / description spacing diverges by two pixels each and nobody can diff it.
 * Here the whole stack is ONE widget whose internal offsets are `static constexpr` measured
 * numbers, and a screen is only allowed to change what actually varies.
 *
 * WHAT VARIES AND WHAT DOES NOT (SCREEN-MANIFEST Sec 6.2 / Sec 7.2):
 *   - **y is DATA**: (69,138) on the front end, (69,286) with 6 rows on Control Panel, (69,327)
 *     with 5 rows on Customize. Push it with `SetRailOriginY`.
 *   - **row count is DATA**: it drives the menu panel's height and therefore the rail's.
 *   - **x = 69 and width = 349 are NOT.** "The rail is a fixed-width instrument. It never
 *     stretches" (SCREEN-MANIFEST Sec 7.2, column 1). C++ pins both on the rail's own canvas
 *     slot, so no WBP and no screen can fork them.
 *
 * THE SELECTION CARET (`Rectangle 278`, 3 x 65 at x = -4) is rail-hugging and DELIBERATELY
 * OUTSIDE the panel -- the parent canvas must therefore not clip the rail. The reference file
 * authors it by hand at y = 54 / 78 / 98 / 138 / 180, i.e. the y encodes which row has focus.
 * **This class exposes the INDEX and derives the y** from the measured row pitch: an authored y
 * would put a layout constant in every call site and would go stale the moment a screen changes
 * its row count. The five authored values are irregular (deltas 24 / 20 / 40 / 42) and do NOT sit
 * on the 40 pitch; that disagreement is recorded in `AuthoredCaretYNote` below rather than
 * averaged away, per the geometry file's reading law.
 *
 * THE REVEAL NOTCHES (`Rectangle 258` / `259`, 88 x 4.727) are chamfers subtracted from the
 * panel border, top and bottom. **The open/close wipe ORIGINATES from them -- that is why the
 * panel unzips rather than fades.** They are authored as GEOMETRY (this class drives their size;
 * their x is unmeasured and is left to the WBP), and the wipe itself is a
 * `BindWidgetAnimOptional` played from C++ in `SetRailOpen`. Never a graph node
 * (`ui-presentation` Sec 10/11).
 *
 * NOT ACTIVATABLE, NOT A BUTTON: the rail routes nothing and decides nothing. Its focus targets
 * are the `UBRFeatureCard` and the `UBRMenuRow`s inside it, all of which are `UCommonButtonBase`
 * and therefore already gamepad-navigable; `GetFirstFocusTarget` hands the owning screen the
 * entry point so no one hand-rolls focus math (`ue5-ui-architecture` Sec 6).
 *
 * NO GAME STATE. The rail holds no ViewModel, reads no PlayerState and owns no rows -- the screen
 * fills `MenuRowSlot` and `DescriptionSlot`. That is what lets the same widget survive travel and
 * the null-PlayerState frames after a join (`ue5-ui-architecture` Sec 7).
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRLeftRail : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** figma_geometry `UBRLeftRail` (node 2382:32507) / COMPONENT-SPECS Sec 6: 349 x 510 @ (69,138). */
	static constexpr float RailX = 69.0f;
	static constexpr float RailWidth = 349.0f;

	/** The measured height of the FE_Play instance. Also the output of `ComputeRailHeight(4,true)`. */
	static constexpr float MeasuredRailHeight = 510.0f;

	/** SCREEN-MANIFEST Sec 6.2: the three observed y origins. The rail's y is data. */
	static constexpr float OriginYFrontEnd = 138.0f;
	static constexpr float OriginYControlPanel = 286.0f;
	static constexpr float OriginYCustomize = 327.0f;

	/** Child geometry, all rail-local, all from figma_geometry `UBRLeftRail.children`. */
	static constexpr float FeatureCardHeight = 222.0f;

	/** Derived from the measured origins: the card ends at 222, the menu border starts at 232. */
	static constexpr float FeatureCardGap = 10.0f;

	static constexpr float MenuBorderHeight = 186.0f;

	/**
	 * The row list inside the bordered panel: (16,16) 311 x 148 (node 0:1183 `Contents`).
	 * NOTE THE ASYMMETRY: 16 left, but 349 - 16 - 311 = 22 right. Measured, not a typo, and not
	 * "corrected" to 16 here -- an invented symmetry would move every row 3px.
	 */
	static constexpr float MenuRowSlotInset = 16.0f;
	static constexpr float MenuRowSlotInsetRight = 22.0f;
	static constexpr float MenuRowSlotWidth = 311.0f;
	static constexpr float MenuRowSlotHeight = 148.0f;

	/** 186 - 148: the border's own vertical chrome. Derived, so a row-count change keeps it. */
	static constexpr float MenuBorderChromeHeight = MenuBorderHeight - MenuRowSlotHeight;

	/** Description strip: 349 x 37 at rail-local y = 473, i.e. 55 below the menu border. */
	static constexpr float DescriptionHeight = 37.0f;
	static constexpr float DescriptionGap = 55.0f;

	/** `Rectangle 278`: 3 x 65 at x = -4. Outside the panel on purpose. */
	static constexpr float CaretWidth = 3.0f;
	static constexpr float CaretHeight = 65.0f;
	static constexpr float CaretOffsetX = -4.0f;

	/** `Rectangle 258` / `259`: 88 x 4.727 chamfers. Their x is UNMEASURED -- the WBP owns it. */
	static constexpr float NotchWidth = 88.0f;
	static constexpr float NotchHeight = 4.727f;

	/**
	 * 148 = (4 - 1) * 40 + 28, so the measured slot holds FOUR rows at the Sec 5 pitch, even
	 * though `FE_Play` instances five `UBRMenuRow`s. Recorded, unresolved (SCREEN-MANIFEST Sec 9);
	 * the row count is data, so the rail grows rather than cropping the fifth row.
	 */
	static constexpr int32 MeasuredRowCount = 4;

	// The authored caret y values in the reference file are 54 / 78 / 98 / 138 / 180. They are
	// kept HERE AS A COMMENT, not as a table: they are the record of the delta described in the
	// class header, and nothing may position from them.

	/** Rows are h28 on a 40 pitch: N rows occupy (N-1) * pitch + height. */
	static constexpr float ComputeRowsHeight(int32 InRowCount)
	{
		return InRowCount > 0
			? (static_cast<float>(InRowCount - 1) * UBRMenuRow::RowPitch) + UBRMenuRow::RowHeight
			: 0.0f;
	}

	static constexpr float ComputeMenuBorderHeight(int32 InRowCount)
	{
		return ComputeRowsHeight(InRowCount) + MenuBorderChromeHeight;
	}

	/**
	 * The whole instrument, derived from its parts. `(4, true)` is exactly the measured 510.
	 *
	 * THE DERIVATION CHECKS ITSELF AGAINST THE OTHER TWO SCREENS, which is why it is trusted:
	 *   FE_Play        y 138 + ComputeRailHeight(4, true)  = 510 -> bottom 648
	 *   ControlPanel   y 286 + ComputeRailHeight(6, false) = 358 -> bottom 644
	 *   Customize      y 327 + ComputeRailHeight(5, false) = 318 -> bottom 645
	 * Three independently authored frames land on the same baseline within 4px. The rail is
	 * BOTTOM-anchored in the reference file and its y is what falls out of the row count -- that
	 * is the whole reason y is data and x/width are not.
	 */
	static constexpr float ComputeRailHeight(int32 InRowCount, bool bWithFeatureCard)
	{
		return (bWithFeatureCard ? FeatureCardHeight + FeatureCardGap : 0.0f)
			+ ComputeMenuBorderHeight(InRowCount)
			+ DescriptionGap
			+ DescriptionHeight;
	}

	/** SCREEN-MANIFEST Sec 6.2: the rail's y is data. x and width are not, and are not settable. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetRailOriginY(float InOriginY);

	/** Layout-only. Call before filling `MenuRowSlot`, or call `RefreshLayout` after. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetRowCount(int32 InRowCount);

	/**
	 * Adopt the real child count of `MenuRowSlot` and re-apply the layout. The screen calls this
	 * once after it has populated the rows -- one source of truth, so the panel height can never
	 * disagree with what is actually in it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void RefreshLayout();

	/**
	 * Move the selection caret to a row. `INDEX_NONE` hides it -- nothing focused renders as
	 * nothing, never as a caret parked on row 0 that says something false about the focus.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetFocusedRowIndex(int32 InRowIndex);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	int32 GetFocusedRowIndex() const { return FocusedRowIndex; }

	/** Plays the notch-origin wipe. With no animation bound it is an honest visibility change. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetRailOpen(bool bInOpen, bool bPlayAnimation = true);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	bool IsRailOpen() const { return bIsOpen; }

	/**
	 * The gamepad entry point into column 1: the feature card if the rail has one, otherwise the
	 * first menu row. The owning screen hands this to CommonUI as its desired focus target rather
	 * than reaching into the rail's children itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	UWidget* GetFirstFocusTarget() const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	UPanelWidget* GetMenuRowSlot() const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	UPanelWidget* GetDescriptionSlot() const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	UBRFeatureCard* GetFeatureCard() const;

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;

	/** The rail's canvas slot does not exist until it is added to one; re-apply on construct. */
	virtual void NativeConstruct() override;
	//~ End UUserWidget interface

	/** The one place the rail's geometry is written. Both lifecycle hooks and every setter route here. */
	void ApplyRailLayout();

	void ApplyCaret();

	/** Rail-local top of the bordered menu panel: 232 with a feature card, 0 without one. */
	float GetMenuBorderTop() const;

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_LeftRail`. Parsed out of this header by
	// `Tools/gen_ui/wbp_plan.py`; these names must match the WBP's widget names exactly.
	// Figma layer -> UMG name mapping (node 2382:32507 `Menu Combo`):
	//   `News Button` 0:933        -> FeatureCard      `0:1028` (menu in border) -> MenuBorderBox
	//   the border strokes          -> MenuBorder      `Contents` 0:1183         -> MenuRowSlot
	//   `Decription Frame` 0:1245   -> DescriptionSlot `Rectangle 278` 0:1182    -> SelectionCaret
	//   `Rectangle 258` 0:1180      -> RevealNotchTop  `Rectangle 259` 0:1181    -> RevealNotchBottom
	//
	// LAYOUT CONTRACT: `SelectionCaret`, `RevealNotchTop` and `RevealNotchBottom` must be direct
	// children of the rail's ROOT CanvasPanel, because C++ positions the caret in RAIL-LOCAL
	// coordinates (that is the only space in which x = -4 means what the reference file means).
	// ---------------------------------------------------------------------------------------

	/** Width is pinned to 349 here and height is derived from the row count. Never authored. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/**
	 * Optional: `ST_ControlPanel` and `OP_Customize` run the rail WITHOUT a feature card. Its
	 * presence is the single source of truth for whether the 222 + 10 block is in the stack --
	 * there is deliberately no second "bHasFeatureCard" flag to disagree with it.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRFeatureCard> FeatureCard;

	/** The bordered menu panel. C++ drives its height from the row count. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> MenuBorderBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> MenuBorder;

	/** The screen fills this with `UBRMenuRow`s. The rail neither creates nor owns them. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UPanelWidget> MenuRowSlot;

	/** Holds a `UBRDescriptionStrip` (a different packet's class), so this is a bare container. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UPanelWidget> DescriptionSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> SelectionCaret;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> RevealNotchTop;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> RevealNotchBottom;

	/**
	 * The unzip. Authored in the WBP from the notches outward, played from C++ only.
	 * MOTION-MEASURED owns its duration and curve; this class owns only when it plays.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> RevealAnim;

	/** Design-time default; overwritten by `SetRowCount` or by `RefreshLayout`'s child count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI", meta = (ClampMin = "0"))
	int32 RowCount = MeasuredRowCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	float RailOriginY = OriginYFrontEnd;

private:
	int32 FocusedRowIndex = INDEX_NONE;

	bool bIsOpen = true;
};
