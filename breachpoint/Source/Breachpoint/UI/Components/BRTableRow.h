#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "CommonUserWidget.h"
#include "UObject/Object.h"
#include "UI/BRUITypes.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRTableRow.generated.h"

class UBRHairlineBorder;
class UCommonTextBlock;
class UImage;
class UPanelWidget;
class USizeBox;
class UWidget;
class UWidgetAnimation;
class UWidgetSwitcher;

/**
 * SCREEN-MANIFEST Sec 6.5 variant axis `Type = Servers | Files`.
 *
 * THIS ENUM IS THE REASON THERE IS ONE BROWSER SCREEN AND NOT TWO. `WBP_Screen_Browser` serves
 * both the custom-game server browser (`CG_BrowserTable`) and the file browser (`FB_Table`)
 * because the source is DATA on one row class. Splitting it into `UBRServerRow` +
 * `UBRFileRow` costs an entire extra screen, its ViewModel and its nav wiring
 * (SCREEN-MANIFEST Sec 4: "8 frames -> 1 widget").
 *
 * If the WBP ever needs genuinely different column GEOMETRY per type, it adds a
 * `TypeSwitcher` whose child index equals this enum's value -- same contract as
 * `UBRButton::TypeSwitcher`. It does NOT add a second class.
 */
UENUM(BlueprintType)
enum class EBRTableRowType : uint8
{
	/** Custom-game browser: a joinable session. */
	Servers,

	/** File browser: a map / mode / bundle entry. */
	Files
};

/**
 * The list item object behind one `UBRTableRow`.
 *
 * WHY IT EXISTS HERE: `UListView` + `IUserObjectListEntry` (SCREEN-MANIFEST Sec 7.1) requires a
 * `UObject` per row, and the ViewModel that will produce these -- `UBRVM_Browser` -- DOES NOT
 * EXIST (SCREEN-MANIFEST gap G9). This is the carrier only: it holds already-formatted display
 * text and nothing else. It performs no lookup, owns no game state, and has no getter into a
 * PlayerState, an ASC or a session interface. When `UBRVM_Browser` lands it becomes the type the
 * ViewModel emits into `SetListItems`, and this file does not change.
 *
 * FORMATTING IS THE PRODUCER'S JOB: `PlayerCount` is FText, not two ints, so "3/8" vs "3 / 8" vs
 * a localised form is decided once in the ViewModel instead of in every row on screen.
 */
UCLASS(BlueprintType)
class BREACHPOINT_API UBRTableRowItem : public UObject
{
	GENERATED_BODY()

public:
	/** Column 1. Empty renders as the unknown dash, never as a blank cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText DisplayName;

	/** Column 2. Files: the creator. Servers: the host's gamertag. */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText AuthorName;

	/** Column 3, pre-formatted by the producer (see class comment). */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	FText PlayerCount;

	/**
	 * Column 4, normalised 0..1. NEGATIVE MEANS UNRATED/UNKNOWN and the column renders empty
	 * rather than as a confident zero-star rating -- an unrated map and a one-star map are
	 * different statements (`ue5-ui-architecture` Sec 7).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	float Rating = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRTableRowType RowType = EBRTableRowType::Servers;

	/**
	 * Honest state for a row whose backing query has not answered yet. `Unknown` rows render
	 * dashes in every column; they never render a plausible-looking empty server.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIDataState DataState = EBRUIDataState::Unknown;
};

/**
 * `UBRTableRow` -- Table Buttons, 660 x 28 (SCREEN-MANIFEST Sec 6.5; Tier 4 leaf, blocks the
 * 8-frame Browser).
 *
 * BASE: `UCommonButtonBase`, the same widget base family as `UBRButton` -- CommonUI already owns
 * focus, gamepad navigation, hover/press/selection and input-action routing, so a table row is
 * reachable with a controller for free (`ue5-ui-architecture` Sec 6: hand-rolled focus math is a
 * finding).
 *
 * WHY IT IS A SIBLING OF `UBRButton` AND NOT A SUBCLASS, stated once so nobody "fixes" it:
 *   1. `UBRButton` declares five REQUIRED `BindWidget` members (`TextFrame`, `TextFrameFill`,
 *      `Label`, ...). Inheriting them would make `WBP_TableRow` fail to compile for reasons
 *      invisible in this header -- and `Tools/gen_ui/wbp_plan.py` parses ONE header per plan
 *      entry with no base-class chain (see its `BindWidget` regex), so the generator would not
 *      catch it either.
 *   2. `UBRButton::ApplyButtonType()` casts `EBRButtonType` (10 values) straight to
 *      `TypeSwitcher`'s child index. A 2-value `EBRTableRowType` sharing that switcher is a
 *      silent index collision.
 * The cost is the ~20 lines of inversion below, duplicated from `UBRButton::ApplyInvertedState`.
 * That is the smaller bug surface. If a third row type appears, hoist the inversion into a shared
 * `UBRRowBase` in ONE packet rather than growing a third copy.
 *
 * PUSH-ONLY, NO POLLING: every value arrives through `SetRowItem` (called by the list from
 * `NativeOnListItemObjectSet`). This class never reads `PlayerState`, an ASC, GameState or a
 * session interface, and it never sends a Server RPC -- a browser row's "join" is an INTENT that
 * the owning screen routes through the PlayerController boundary.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRTableRow : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** SCREEN-MANIFEST Sec 6.5: COMPONENT 660 x 28. 660 is deliberately OFF the column grid (Sec 1). */
	static constexpr float RowWidth = 660.0f;
	static constexpr float RowHeight = 28.0f;

	/**
	 * SCREEN-MANIFEST Sec 6.5: "the UBRButtonBorder 4-line treatment (top 1px @1.0, bottom 1px
	 * @0.3, side ticks @0.3)" -- i.e. exactly `UBRHairlineBorder`'s default `FBRHairlineStyle`.
	 * No lines are redrawn here; the tick length is COMPONENT-SPECS Sec 2's measured 20 on the
	 * same 28-tall shell.
	 */
	static constexpr float BorderSideTickLength = 20.0f;

	/** COMPONENT-SPECS Sec 2 status table: Disabled dims to 0.5 and changes no geometry. */
	static constexpr float DisabledOpacity = 0.5f;

	/**
	 * SCREEN-MANIFEST Sec 6.5 text: Rajdhani SemiBold 16, letter-spacing 10%, UPPERCASE.
	 * Sec 7.4: UMG letter spacing is 1/1000 em, so 10% is the UMG value 100 -- NOT 10.
	 * These two numbers are already encoded by `UBRTextStyle_MenuRow`
	 * (`UI/Styles/BRTextStyles.h`), which is the style every text block in `WBP_TableRow` must
	 * use. They are restated here as constants so a review can check the WBP against a number in
	 * source instead of against a document.
	 */
	static constexpr float TextFontSize = 16.0f;
	static constexpr int32 TextLetterSpacing = 100;

	// -------------------------------------------------------------------------------------------
	// UNMEASURED -- the internal column stops.
	//
	// How the 660 divides into name / author / players / rating is recorded NOWHERE: not in
	// COMPONENT-SPECS, not in SCREEN-MANIFEST (Sec 6.5 says so explicitly, and Sec 8 row 6 lists
	// it as an open question). READ IT BEFORE AUTHORING:
	//     node `CG_BrowserTable` `755:6805` in file `Kn87U5sy2VD0lP8K7h4LcQ`.
	//
	// The sentinel is negative on purpose: a guessed column layout is worse than an absent one,
	// so these values CANNOT be quietly passed to SetWidthOverride and look plausible. No code in
	// this class reads them. When the node is read, replace the sentinels with the measured
	// widths, cite the read date, and drive the columns from them.
	// -------------------------------------------------------------------------------------------

	/** UNMEASURED sentinel. Any column constant still equal to this has never been read. */
	static constexpr float UnmeasuredColumnStop = -1.0f;

	/** UNMEASURED -- node 755:6805. */
	static constexpr float NameColumnWidth = UnmeasuredColumnStop;

	/** UNMEASURED -- node 755:6805. */
	static constexpr float AuthorColumnWidth = UnmeasuredColumnStop;

	/** UNMEASURED -- node 755:6805. */
	static constexpr float PlayersColumnWidth = UnmeasuredColumnStop;

	/** UNMEASURED -- node 755:6805. */
	static constexpr float RatingColumnWidth = UnmeasuredColumnStop;

	/** Call this before any code drives a column width from the constants above. */
	static constexpr bool HasMeasuredColumnStops() { return NameColumnWidth > 0.0f; }

	/** The one push path. Null clears the row to the honest unknown state. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetRowItem(UBRTableRowItem* InItem);

	/** Dashes in every column, rating column empty, selection marker cleared. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearToUnknown();

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetRowType(EBRTableRowType InRowType);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRTableRowType GetRowType() const { return RowType; }

	/**
	 * SCREEN-MANIFEST Sec 6.5 `Selection Mode` axis. In selection mode the row is a toggle (the
	 * marker shows and the row is selectable); out of it, activating the row navigates to
	 * `FD_Detail` and selection is the list's transient highlight only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetSelectionMode(bool bInSelectionMode);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	bool IsSelectionMode() const { return bSelectionMode; }

	/**
	 * The em dash, written as an escape so this file stays pure ASCII (see BRProfileBar.cpp).
	 * Public because `UBRSmallHeader` below shares it -- one dash for the whole list page.
	 * GAP: `UBRProfileBar` has its own protected copy; this wants to be `BRUI::GetUnknownValueText`
	 * in `BRComponentTokens.h`, which is not this packet's file.
	 */
	static FText GetUnknownValueText();

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget interface

	//~ Begin IUserObjectListEntry / IUserListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnEntryReleased() override;
	//~ End IUserObjectListEntry / IUserListEntry interface

	//~ Begin UCommonButtonBase interface
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	virtual void NativeOnEnabled() override;
	virtual void NativeOnDisabled() override;
	//~ End UCommonButtonBase interface

	/** COMPONENT-SPECS Sec 1: idle -> hover is an INVERSION. One code path, two callers. */
	void ApplyInvertedState(bool bInverted, bool bPlayAnimation = true);

	void ApplyRowType();

	/**
	 * A table cell keeps its slot: empty text renders the unknown dash instead of collapsing,
	 * because a collapsed cell shifts every column to its right and makes the list unreadable.
	 */
	static void SetColumnText(UCommonTextBlock* Column, const FText& InText);

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_TableRow`. `Tools/gen_ui/wbp_plan.py` parses these names out of
	// this header; the WBP's widget names must match exactly.
	// -------------------------------------------------------------------------------------------

	/** C++ drives 660 x 28 so the two browsers cannot drift apart in two details panels. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/** The 4-line treatment. REUSED, not redrawn -- see `UBRHairlineBorder`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> Border;

	/** The plate that goes solid white on hover / selection. Transparent when idle. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UImage> RowFill;

	/** Column 1. The only text block both Types are guaranteed to carry. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> AuthorText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> PlayersText;

	/**
	 * Host slot for `UBRRating` (85 x 17, SCREEN-MANIFEST Sec 4) -- which DOES NOT EXIST YET.
	 * Until it does this class only toggles the slot's visibility from `UBRTableRowItem::Rating`;
	 * it does not push a value into it. Typed `UWidget` so the slot does not have to be rebuilt
	 * when the real component lands.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> RatingSlot;

	/** Shown only while `bSelectionMode` is true. Hidden, never collapsed -- see SetColumnText. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> SelectionMarker;

	/**
	 * OPTIONAL, and unbound by default. Only needed if the measured column stops turn out to
	 * differ between `Servers` and `Files`; child index == `EBRTableRowType` value. Read node
	 * 755:6805 before deciding whether this switcher is needed at all.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidgetSwitcher> TypeSwitcher;

	/** Appearance only. Played forward on invert, reverse on release. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> InvertAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRTableRowType RowType = EBRTableRowType::Servers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken InvertedFillToken = EBRUIColorToken::SurfaceInverted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken InvertedTextToken = EBRUIColorToken::TextInverted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken IdleTextToken = EBRUIColorToken::TextPrimary;

private:
	bool bIsInverted = false;

	bool bSelectionMode = false;
};

/**
 * `UBRSmallHeader` -- Small Header, 1180 x 27 (SCREEN-MANIFEST Sec 4 / Tier 4). The column-caption
 * band that sits above a `UListView` of `UBRTableRow`s on the list pages.
 *
 * THE 1180 IS DELIBERATE. DO NOT "FIX" IT TO THE 69px GRID.
 * SCREEN-MANIFEST Sec 1 records 1180 as a SECOND margin system: 1280 - 1180 = 100, i.e. a 50px
 * side margin, while the front-end rail pages use the 69px 3-column grid. It is shared by
 * `Decorative Line`, `Roster Group Header`, `Team Label` and `Small Header` -- four components, so
 * it is a family, not a stray. Sec 8 row 3 lists the two-grid conflict as OPEN and names the read
 * that closes it (`Roster Group Header` in-screen on `RS_Friends` `927:43283`). Until that read
 * happens the manifest's ruling stands verbatim: "list-page chrome is authored at 1180 wide
 * anchored centre-stretch, and the deviation is recorded here rather than silently reconciled."
 * Anchoring is centre-stretch (not left at x=50) so the band stays symmetric at every aspect --
 * which is also what makes the eventual 1180 -> 1142 correction a one-line anchor change.
 *
 * Anatomy below the outer box is UNMEASURED; the caption cells live in the WBP as layout, and
 * this class deliberately invents no internal geometry.
 *
 * Push-only and stateless, exactly like `UBRProfileBar`: it is on screen during travel and during
 * the frames where every browser query is still outstanding, so it renders what it was last
 * pushed and starts life showing the unknown dash.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRSmallHeader : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** SCREEN-MANIFEST Sec 4: COMPONENT 1180 x 27. */
	static constexpr float HeaderWidth = 1180.0f;
	static constexpr float HeaderHeight = 27.0f;

	/**
	 * SCREEN-MANIFEST Sec 1: 1180 on the 1280 design canvas implies a 50px side margin -- NOT the
	 * 69 the 3-column grid uses. Both numbers are here so the discrepancy is legible in source
	 * rather than looking like a typo. (1280 - 1180) / 2 == 50.
	 */
	static constexpr float DesignCanvasWidth = 1280.0f;
	static constexpr float ImpliedSideMargin = 50.0f;
	static constexpr float ColumnGridSideMargin = 69.0f;

	/** Empty text renders the unknown dash, never a blank band. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetHeaderText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearHeaderText();

protected:
	virtual void NativeOnInitialized() override;

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_SmallHeader`.
	// -------------------------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> HeaderText;

	/** The band's rule / underline, if the WBP carries one. Reused, not redrawn. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> Rule;

	/**
	 * Where the per-column captions live. Their stops are the SAME UNMEASURED numbers as
	 * `UBRTableRow`'s (node 755:6805) -- when those are read, the header captions and the row
	 * columns must be driven from one set of values or they will drift by a pixel forever.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UPanelWidget> CaptionContainer;
};
