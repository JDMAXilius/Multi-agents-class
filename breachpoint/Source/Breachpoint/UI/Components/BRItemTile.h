#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "UI/Components/BRComponentTokens.h"

#include "BRItemTile.generated.h"

class UBRHairlineBorder;
class UBRItemTileEntry;
class UBRRule;
class UImage;
class USizeBox;
class UTexture2D;
class UWidget;

/**
 * COMPONENT-SPECS Sec 5: the four rarities the reference file ships.
 *
 * DECORATIVE RAMP -- FORBIDDEN ON THE HUD.
 * `REFERENCE-EXTRACTION` Sec 7 conflict 2 closed this: the VISR semantic tokens carry meaning
 * under stress and stay the semantic core; rarity and premium are a SEPARATE DECORATIVE ramp
 * "scoped to store/progression only", and "a decorative token may never appear on the HUD".
 * So this enum, this tile, and anything that resolves a rarity colour are front-end only. If a
 * combat surface needs to say something about an item it says it with a VISR token, not with
 * this ramp -- reusing `UBRItemTile` inside `UBRHUDLayout` is a finding, not a shortcut.
 */
UENUM(BlueprintType)
enum class EBRItemRarity : uint8
{
	Common,

	Rare,

	Epic,

	Legendary
};

/** COMPONENT-SPECS Sec 5 `Size` axis: the component ships at 114 and at mini 30. */
UENUM(BlueprintType)
enum class EBRItemTileSize : uint8
{
	/** 114 x 114. */
	Standard,

	/** 30 x 30. */
	Mini
};

/** COMPONENT-SPECS Sec 5 `Type` axis: `Default` | `Empty` | `Non-Interactive`. */
UENUM(BlueprintType)
enum class EBRItemTileType : uint8
{
	Default,

	/** An occupied grid cell with nothing in it. Border only -- no art, no badges. */
	Empty,

	/** Renders like Default but takes no focus and cannot be selected (read-only displays). */
	NonInteractive
};

/**
 * Everything one tile draws. It is a pure PRESENTATION payload: there is no id, no price maths,
 * no ownership rule and no gameplay number in it, because the tile is not allowed to decide any
 * of those. The owner (a screen, once `UBRVM_Customization` exists) fills this in and pushes it.
 *
 * Law 3: the art reference is SOFT. A tile that hard-refs its texture pulls every item image in
 * the game into memory the moment the grid class loads.
 */
USTRUCT(BlueprintType)
struct FBRItemTileData
{
	GENERATED_BODY()

	/**
	 * Not drawn by the tile -- COMPONENT-SPECS Sec 5 gives the tile no text node. It rides here
	 * because the tile's selection is what feeds `Gear Detail`, and the payload travels with the
	 * entry object rather than being looked up again from state the UI must not touch.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	TSoftObjectPtr<UTexture2D> Art;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	EBRItemRarity Rarity = EBRItemRarity::Common;

	/** COMPONENT-SPECS Sec 5: the `Locked` badge at (11,11), 22 x 22. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	bool bLocked = false;

	/** COMPONENT-SPECS Sec 5: the `Currency` badge at (81,11), 22 x 22 -- this item costs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	bool bShowCurrency = false;

	/** COMPONENT-SPECS Sec 5: the `Favourite` badge at (81,81), 22 x 22. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	bool bFavourite = false;

	/** COMPONENT-SPECS Sec 5: the `Checkbox` at (14,84), 16 x 16 -- "equipped" on a grid cell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	bool bChecked = false;

	/** COMPONENT-SPECS Sec 5: `Multi-Core`, the 28 x 10.33 tab sitting in the bottom-edge cutout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breachpoint|UI")
	bool bMultiCore = false;
};

/**
 * The list item a `UTileView` holds. SCREEN-MANIFEST Sec 7.1 requires the grid to be a
 * `UListView`/`UTileView` with an entry widget implementing `IUserObjectListEntry`, and those
 * views take `UObject*` items -- so the payload needs exactly this much wrapper and no more.
 *
 * It is a data carrier: no behaviour, no delegates, nothing that reads game state.
 */
UCLASS(BlueprintType)
class BREACHPOINT_API UBRItemTileEntry : public UObject
{
	GENERATED_BODY()

public:
	/** Grids reuse their entry objects across refreshes rather than reallocating N per push. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetItemData(const FBRItemTileData& InData) { ItemData = InData; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	const FBRItemTileData& GetItemData() const { return ItemData; }

private:
	UPROPERTY()
	FBRItemTileData ItemData;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBROnItemTileClicked, UBRItemTileEntry*, Entry);

/**
 * `UBRItemTile` -- the 114 x 114 item cell (SCREEN-MANIFEST Sec 5 TIER 3, 10/31 screens).
 *
 * THE BOTTOM LINE IS THE WHOLE STATE SIGNAL.
 * COMPONENT-SPECS Sec 5 puts every rarity difference on ONE node: `Bottom Line 112 x 11 <- RARITY
 * COLOUR`. Everything else on the tile is the same white 2px chrome at every rarity. That is why
 * `RarityLine` is a separate `UBRRule` rather than the bottom edge of `Border`: the shared
 * `FBRHairlineStyle` carries one stroke token for all four of its edges, so a per-edge rarity
 * colour is not expressible on a single border. Two widgets, one measured fact.
 *
 * BASE: `UCommonButtonBase` + `IUserObjectListEntry`. Same base family as `UBRMenuRow` (a
 * `UCommonButtonBase` IS a `UCommonUserWidget`), which is what makes the tile gamepad-focusable
 * and navigable for free -- `ue5-ui-architecture` Sec 6: navigation and focus are CommonUI's job.
 * The interface is what lets a `UTileView` own, recycle and virtualise these; hand-placing N
 * tiles is a SCREEN-MANIFEST Sec 7.1 violation.
 *
 * THE `82 x 0` CONTRADICTION -- NOT PAPERED OVER.
 * COMPONENT-SPECS Sec 5 records `Left Line 82 x 0` and `Right Line 82 x 0`. An 82-long, 0-tall
 * line is geometrically impossible as drawn. The two readings both fail:
 *   - read as W x H, the zero axis is the degenerate one (this is the Sec 2 menu-row convention,
 *     `Left Line 0 x 20` = a 20-tall vertical tick). Applied here it makes the tile's SIDE lines
 *     82 units wide and flat -- horizontal lines called "left" and "right" on a 114-tall tile;
 *   - read as "length 82 along the side", the tick is 82 of the tile's 114 height, which is
 *     plausible but is NOT what the extraction says, and Sec 5's own siblings
 *     (`Top Line 112 x 10`, `Bottom Line 112 x 11`) are not degenerate in either axis at all.
 * So the 82 is carried below as a LENGTH and marked UNMEASURED. No height is invented here, and
 * the WBP must not invent one either: re-measure the node before this tile ships.
 *
 * NO GAME STATE, EVER. This tile has no ViewModel (`UBRVM_Customization` is SCREEN-MANIFEST
 * Sec 10 gap G6 and does not exist), so it is push-only through `SetItemData` /
 * `NativeOnListItemObjectSet` and reads nothing from a pawn, an ASC, a PlayerState or a
 * GameState. A tile with no item renders the honest `Empty` type, never a confident blank.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRItemTile : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** COMPONENT-SPECS Sec 5: COMPONENT 114 x 114. */
	static constexpr float TileSize = 114.0f;

	/** COMPONENT-SPECS Sec 5 `Size=mini`: 30 x 30. Its INTERIOR is unmeasured -- see below. */
	static constexpr float MiniTileSize = 30.0f;

	/** COMPONENT-SPECS Sec 5: Background / Gradient / Art all sit at (7,7), 100 x 100. */
	static constexpr float ArtInset = 7.0f;
	static constexpr float ArtSize = 100.0f;

	/** COMPONENT-SPECS Sec 5: Top Line and Bottom Line are 112 long inside the 114 shell. */
	static constexpr float EdgeLineLength = 112.0f;

	/**
	 * UNMEASURED -- COMPONENT-SPECS Sec 5 gives the side lines as `82 x 0`, which cannot be drawn
	 * (see the class comment). 82 is carried as the tick's LENGTH because that is the only number
	 * in the record; its axis and its thickness-in-the-other-direction are NOT known and are not
	 * guessed. Re-measure `Button Border`'s Left/Right Line before shipping.
	 */
	static constexpr float SideTickLengthUnmeasured = 82.0f;

	/** UNMEASURED for the same reason: Sec 5's corner ticks are recorded `12 x 0`. */
	static constexpr float CornerTickLengthUnmeasured = 12.0f;

	/** COMPONENT-SPECS Sec 5: Locked (11,11), Currency (81,11), Favourite (81,81) -- all 22 x 22. */
	static constexpr float BadgeSize = 22.0f;
	static constexpr float BadgeNearInset = 11.0f;
	static constexpr float BadgeFarInset = 81.0f;

	/** COMPONENT-SPECS Sec 5: Checkbox (14,84), 16 x 16. */
	static constexpr float CheckboxSize = 16.0f;
	static constexpr float CheckboxOriginX = 14.0f;
	static constexpr float CheckboxOriginY = 84.0f;

	/** COMPONENT-SPECS Sec 5: Multi-Core (42.88, 107.74), 28 x 10.33, inside the bottom cutout. */
	static constexpr float MultiCoreWidth = 28.0f;
	static constexpr float MultiCoreHeight = 10.33f;
	static constexpr float MultiCoreOriginX = 42.88f;
	static constexpr float MultiCoreOriginY = 107.74f;

	/** The measured art box is exactly the tile inset on both sides. If this fails, one moved. */
	static_assert(ArtInset + ArtSize + ArtInset == TileSize, "COMPONENT-SPECS Sec 5: 7 + 100 + 7 = 114");

	/**
	 * The rarity ramp resolved to a token. GAP, stated in code rather than papered over: the
	 * project's only palette surface is `EBRUIColorToken`, which deliberately carries NO accent
	 * colours (see `BRComponentTokens.h`), so three of the four rarities have no token to name.
	 * Rather than type the four hex values from COMPONENT-SPECS Sec 5 into this file -- which is
	 * banned, and which would fork the palette in the one component that most needs a shared one
	 * -- unresolved rarities fall back to the measured `common` white and say so once in the log.
	 * Fix by adding rarity tokens to the palette in the Styles lane; this function is the only
	 * caller-visible thing that changes.
	 */
	static EBRUIColorToken ResolveRarityStrokeToken(EBRItemRarity Rarity);

	/** Push presentation in. The ONLY way data reaches a tile. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetItemData(const FBRItemTileData& InItemData);

	/** Back to the honest `Empty` cell: no art, no badges, rarity back to common. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void ClearItemData();

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetTileSize(EBRItemTileSize InTileSize);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRItemTileSize GetTileSize() const { return TileSizeVariant; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	void SetTileType(EBRItemTileType InTileType);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRItemTileType GetTileType() const { return TileType; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	EBRItemRarity GetRarity() const { return ItemData.Rarity; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|UI")
	const FBRItemTileData& GetItemData() const { return ItemData; }

	/**
	 * Fired on click with the entry this tile currently represents. INTENT ONLY -- the tile does
	 * not act on it. Whoever binds this routes it through the owning PlayerController's UI-intent
	 * boundary; a widget that calls a Server RPC directly is a netcode finding.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Breachpoint|UI")
	FBROnItemTileClicked OnItemTileClicked;

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget interface

	//~ Begin UCommonButtonBase interface
	virtual void NativeOnClicked() override;
	//~ End UCommonButtonBase interface

	//~ Begin IUserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnEntryReleased() override;
	//~ End IUserObjectListEntry interface

	/** The one place ItemData reaches Slate. Every setter funnels here. */
	void ApplyItemData();

	void ApplyTileSize();

	void ApplyTileType();

	// ---------------------------------------------------------------------------------------
	// BindWidget contract. These names are the authoring contract for `WBP_ItemTile` and must
	// match its widget names exactly. COMPONENT-SPECS Sec 5 layer -> UMG name mapping:
	//   `Button Border` (Top + side ticks) -> Border        `Bottom Line` -> RarityLine
	//   `Background`  -> Ground            `Gradient` + Background tint -> RarityTint
	//   `Art`         -> Art               `Locked`      -> LockedBadge
	//   `Currency`    -> CurrencyBadge     `Favourite`   -> FavouriteBadge
	//   `Checkbox`    -> CheckMark         `Multi-Core`  -> MultiCoreNotch
	// ---------------------------------------------------------------------------------------

	/** Root box. C++ drives 114 vs mini 30 from the Size axis so mini is not a second WBP. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<USizeBox> RootSizeBox;

	/**
	 * COMPONENT-SPECS Sec 5 `Button Border`: ALL LINES 2px, align CENTER. This instance draws the
	 * top line and the two side ticks; the bottom line is `RarityLine` because it is the only
	 * edge whose colour moves.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UBRHairlineBorder> Border;

	/** The rarity signal. 2px, horizontal, tinted from `ResolveRarityStrokeToken`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UBRRule> RarityLine;

	/** COMPONENT-SPECS Sec 5 `Background` (7,7) 100 x 100: the black-at-0.5 plate under the art. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Ground;

	/**
	 * COMPONENT-SPECS Sec 5 records a linear GRADIENT rarity tint (0 -> 1 alpha) over the
	 * background. A gradient is a material (authoring-matrix Tier 4), so C++ only tints this
	 * plate with the rarity token; the ramp itself belongs to the WBP's brush/material.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> RarityTint;

	/** COMPONENT-SPECS Sec 5 `Art` (7,7) 100 x 100. Fed from a SOFT texture ref (law 3). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UImage> Art;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> LockedBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> CurrencyBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> FavouriteBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> CheckMark;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> MultiCoreNotch;

	/**
	 * `Size=mini` is 30 x 30 and NOTHING ELSE about it is measured -- the reference gives no
	 * interior inset, art box or badge layout at 30. C++ therefore drives only the outer box and
	 * leaves the interior to the WBP. Do not scale the 7 / 100 numbers by 30/114 and call it
	 * measured.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRItemTileSize TileSizeVariant = EBRItemTileSize::Standard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRItemTileType TileType = EBRItemTileType::Empty;

	/** COMPONENT-SPECS Sec 5: the whole border is the 2px `Emphasis` weight, all four lines. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRStrokeWeight BorderWeight = EBRStrokeWeight::Emphasis;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breachpoint|UI")
	EBRUIColorToken GroundToken = EBRUIColorToken::PanelGround50;

private:
	/** Presentation payload only. Never read from, or written to, game state. */
	UPROPERTY(Transient)
	FBRItemTileData ItemData;

	/** The list item this tile currently represents; null between release and re-use. */
	UPROPERTY(Transient)
	TObjectPtr<UBRItemTileEntry> ItemEntry;
};
