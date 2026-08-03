#pragma once

#include "CommonUserWidget.h"
#include "UI/BRUITypes.h"
#include "UI/Components/BRItemTile.h"

#include "BRItemGrid.generated.h"

class UBRScrollBar;
class UCommonTextBlock;
class USizeBox;
class UTileView;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBROnItemGridSelectionChanged, UBRItemTileEntry*, SelectedEntry);

/**
 * `UBRItemGrid` -- the 536 x 388 item grid (SCREEN-MANIFEST Sec 5 TIER 3; unblocks Waves 3-5).
 *
 * IT IS A `UTileView`, NOT N CHILDREN.
 * SCREEN-MANIFEST Sec 7.1: "A repeated row / tile ... `UListView` / `UTileView` with an entry WBP
 * implementing `IUserObjectListEntry`" and NEVER "N hand-placed children". `UBRItemTile`
 * implements that interface, so the view owns creation, recycling and virtualisation: a
 * 400-item inventory costs the widgets that fit on screen, not 400. Hand-placing tiles here
 * would also break the ultrawide rule below, because a hand-placed grid cannot re-wrap.
 *
 * ULTRAWIDE: THE GRID ADDS COLUMNS, IT NEVER STRETCHES TILES.
 * SCREEN-MANIFEST Sec 8.2 states it as a table row: "Item / tile grids -- add columns, do not
 * stretch tiles -- `UTileView` wrap; tile stays 114, pitch stays 130." That is exactly what the
 * entry width/height below encode: the view's cell is the PITCH (130) and the tile inside it is
 * always `UBRItemTile::TileSize` (114). Widening the grid fits more 130-cells; it never makes a
 * cell bigger. Sec 7.3 says the same thing for the off-grid module: "do not stretch. Add columns
 * instead if space allows."
 *
 * TWO CALLERS, ONE CLASS. `SCREEN-BUILD-SPEC` Sec 1/Sec 2 place an off-grid 504-wide module at
 * (86, 260) as well as the 536-wide panel. The only difference is the column count, so that is a
 * runtime property (`SetColumnCount`) rather than a second widget and a second asset.
 *
 * NO VIEWMODEL EXISTS. `UBRVM_Customization` is SCREEN-MANIFEST Sec 10 gap G6 and is unbuilt, so
 * this grid is PUSH-ONLY and starts in `EBRUIDataState::Unknown` -- not empty, not zero. It never
 * reaches into a pawn, an ASC, a PlayerState or a GameState; `ue5-ui-architecture` Sec 7 is the
 * reason (those are null or stale for the first frames after join and travel, and a confident
 * empty grid is a false statement about someone's inventory). Every field this grid wishes
 * existed is filed as a C++ gap, listed on `SetItemData`.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRItemGrid : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** REFERENCE-EXTRACTION Sec 5 / SCREEN-MANIFEST Sec 5: the `Item Grid` panel is 536 x 388. */
	static constexpr float PanelWidth = 536.0f;
	static constexpr float PanelHeight = 388.0f;

	/** SCREEN-BUILD-SPEC Sec 1 "Grid math, constant everywhere": pitch 130 = tile 114 + gutter 16. */
	static constexpr float TilePitch = 130.0f;
	static constexpr float TileGutter = 16.0f;

	/** SCREEN-BUILD-SPEC Sec 1 / Sec 2: the off-grid module, 4 columns at (86, 260). */
	static constexpr int32 ModuleColumnCount = 4;
	static constexpr float ModuleWidth = 504.0f;
	static constexpr float ModuleHeight = 374.0f;
	static constexpr float ModuleOriginX = 86.0f;
	static constexpr float ModuleOriginY = 260.0f;

	/** SCREEN-BUILD-SPEC Sec 1: the measured rail is 8 x 374 at x = 62. See `ScrollBar` below. */
	static constexpr float ScrollBarOriginX = 62.0f;

	/** The grid math, asserted rather than trusted. 4 * 130 - 16 = 504; 3 * 130 - 16 = 374. */
	static_assert(ModuleColumnCount * TilePitch - TileGutter == ModuleWidth,
		"SCREEN-BUILD-SPEC Sec 1: 4 columns at pitch 130 is 504 wide");
	static_assert(3 * TilePitch - TileGutter == ModuleHeight,
		"SCREEN-BUILD-SPEC Sec 1: the 504x374 module is 4 columns x THREE rows at pitch 130");
	static_assert(UBRItemTile::TileSize + TileGutter == TilePitch,
		"SCREEN-BUILD-SPEC Sec 1: pitch 130 = tile 114 + gutter 16");

	/**
	 * The width a `UTileView` needs in order to wrap at exactly N columns: N whole pitch cells.
	 * The MEASURED 504 is this minus the trailing gutter, which is empty space to the right of
	 * the last tile -- so the widget is `N * 130` wide and the design's 504 is what you see.
	 * Do not "fix" the difference by shrinking the cell; that stretches nothing and un-pitches
	 * everything.
	 */
	static constexpr float WrapWidthForColumns(int32 InColumnCount)
	{
		return static_cast<float>(InColumnCount) * TilePitch;
	}

	/** The measured content width for N columns: 504 at N = 4. */
	static constexpr float ContentWidthForColumns(int32 InColumnCount)
	{
		return static_cast<float>(InColumnCount) * TilePitch - TileGutter;
	}

	/**
	 * Push the visible page of items in. This is the ONLY way data enters the grid.
	 *
	 * GAPS this call papers over on the caller's behalf, all of them `UBRVM_Customization` (G6)
	 * fields that do not exist in C++ today and are reported, not invented here:
	 *   - `Items[]` itself (art soft ptr, rarity, owned, locked, favourite, price, multi-core)
	 *   - the equipped/selected item id, so the grid could restore selection after a refresh
	 *   - filter and sort state, which decides what this array even contains
	 *   - `bIsLoading` / inventory-fetch state, which is the difference between "you own nothing"
	 *     and "we have not asked yet" -- today the caller must say which by choosing between
	 *     `SetItemData` and `ClearToUnknown`
	 *   - the item ids the tiles would carry back with a selection intent.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetItemData(const TArray<FBRItemTileData>& InItems);

	/**
	 * Back to the honest unknown state: no tiles, and the unknown placeholder rather than an
	 * empty-looking grid. Call this on open, on travel, and whenever the feed is in doubt.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearToUnknown();

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRUIDataState GetDataState() const { return DataState; }

	/**
	 * Fix the grid to N columns (the 504 module uses 4), or pass <= 0 to let it wrap to whatever
	 * its slot allows -- which is the ultrawide behaviour: more width, more columns, same tiles.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetColumnCount(int32 InColumnCount);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	int32 GetColumnCount() const { return ColumnCount; }

	/**
	 * The grid's one selection surface. INTENT ONLY: the payload is presentation data, and
	 * whoever binds this routes any resulting action through the owning PlayerController's
	 * UI-intent boundary. Nothing in this class writes game state, replicated or otherwise.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Breachpoint|UI")
	FBROnItemGridSelectionChanged OnSelectionChanged;

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget interface

	/** Routed from the view's native selection event; re-published typed. */
	void HandleListSelectionChanged(UObject* Item);

	void ApplyColumnCount();

	/** Shows the unknown / empty placeholder honestly. Never renders a confident empty grid. */
	void ApplyDataState();

	static FText GetUnknownStateText();

	static FText GetEmptyStateText();

	// ---------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_ItemGrid`.
	//
	// `TileView`'s `EntryWidgetClass` is set to `WBP_ItemTile` in the WBP's details panel -- it
	// is a list-entry class reference on the view, not a C++ `UPROPERTY`, which is why there is
	// no hard widget-class pointer anywhere in this header (law 3 / `ue5-ui-architecture` Sec 1).
	// ---------------------------------------------------------------------------------------

	/** The 536 x 388 panel shell. Optional: the off-grid module sizes itself from its slot. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/**
	 * Wraps the tile view ONLY. C++ overrides its width to `WrapWidthForColumns(ColumnCount)` to
	 * pin a column count, and clears the override for the ultrawide "add columns" behaviour.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> TileViewBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UTileView> TileView;

	/**
	 * The measured 8 x 374 rail at x = 62. C++ COLLAPSES it and leaves it collapsed, because
	 * `UBRScrollBar`'s own contract says a bar that renders a plausible thumb over a list it is
	 * not connected to is a lie about how much content there is -- and UMG exposes no public path
	 * to drive an external `UScrollBar` from a `UTileView` (`STableViewBase` owns its internal
	 * bar; `UListView`'s scroll delegates are private). Reported as a gap; when the hook exists,
	 * this is the one place it lands.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRScrollBar> ScrollBar;

	/** The honest placeholder. Shown when there is nothing to show, and says WHICH nothing. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> EmptyStateLabel;

	/**
	 * 4 for the 504 module and for the 536 panel; <= 0 means "wrap to the slot", which is how the
	 * grid gains columns at 21:9 instead of stretching tiles (SCREEN-MANIFEST Sec 8.2).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI", meta = (ClampMin = "-1"))
	int32 ColumnCount = ModuleColumnCount;

private:
	/**
	 * Entry objects are REUSED across pushes rather than reallocated per refresh: a filter change
	 * on a large inventory would otherwise churn hundreds of UObjects per keystroke. The tile
	 * widgets themselves are recycled by the view; this is the data half of the same rule.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBRItemTileEntry>> EntryPool;

	/** Starts Unknown, on purpose. Nothing has fed this grid and it will not pretend otherwise. */
	UPROPERTY(Transient)
	EBRUIDataState DataState = EBRUIDataState::Unknown;
};
