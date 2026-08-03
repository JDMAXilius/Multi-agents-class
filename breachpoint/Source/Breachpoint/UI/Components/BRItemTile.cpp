#include "UI/Components/BRItemTile.h"

#include "Breachpoint.h"
#include "Components/Image.h"
#include "Components/ListView.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "UI/Components/BRHairlineBorder.h"

EBRUIColorToken UBRItemTile::ResolveRarityStrokeToken(EBRItemRarity Rarity)
{
	switch (Rarity)
	{
	case EBRItemRarity::Common:
		// COMPONENT-SPECS Sec 5: common's bottom-line stroke IS plain white -- this one is exact.
		return EBRUIColorToken::ChromeStroke;

	case EBRItemRarity::Rare:
	case EBRItemRarity::Epic:
	case EBRItemRarity::Legendary:
	default:
	{
		// GAP (reported, not worked around): `EBRUIColorToken` carries no accent colours by
		// design, so the three coloured rarities have no token to resolve to and hex is banned
		// here. They render as the measured common white until the Styles lane adds the tokens.
		// Logged ONCE so a white legendary line is a known gap in the log rather than a silent
		// wrong colour on screen.
		static bool bWarnedOnce = false;
		if (!bWarnedOnce)
		{
			bWarnedOnce = true;
			UE_LOG(Logbreachpoint, Warning,
				TEXT("UBRItemTile: no palette token exists for the coloured rarities (rare/epic/legendary); ")
				TEXT("falling back to the common white stroke. Add rarity tokens to the UI palette."));
		}

		return EBRUIColorToken::ChromeStroke;
	}
	}
}

void UBRItemTile::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Border)
	{
		// COMPONENT-SPECS Sec 5: all lines 2px. This instance owns the top line and the two side
		// ticks only -- the bottom edge is `RarityLine`, so Bottom is switched off here.
		// Edges = Top | Left | Right (1 | 4 | 8).
		FBRHairlineStyle BorderStyle = Border->GetHairlineStyle();
		BorderStyle.Edges = static_cast<int32>(EBRBorderEdge::Top)
			| static_cast<int32>(EBRBorderEdge::Left)
			| static_cast<int32>(EBRBorderEdge::Right);
		BorderStyle.DimmedEdges = static_cast<int32>(EBRBorderEdge::Left)
			| static_cast<int32>(EBRBorderEdge::Right);
		BorderStyle.Weight = BorderWeight;

		// UNMEASURED: see `SideTickLengthUnmeasured`. The 82 is applied as the tick's length
		// because it is the only number the extraction gives; nothing here invents a height.
		BorderStyle.SideTickLength = SideTickLengthUnmeasured;
		Border->SetHairlineStyle(BorderStyle);
	}

	if (Ground)
	{
		Ground->SetColorAndOpacity(BRUI::ResolveColorToken(GroundToken));
	}

	ApplyTileSize();

	// Start in the honest empty cell. A tile is constructed long before anything pushes an item
	// into it -- and with no ViewModel in existence, "long before" may be forever.
	ClearItemData();
}

void UBRItemTile::ApplyTileSize()
{
	if (!RootSizeBox)
	{
		return;
	}

	const float Size = (TileSizeVariant == EBRItemTileSize::Mini) ? MiniTileSize : TileSize;
	RootSizeBox->SetWidthOverride(Size);
	RootSizeBox->SetHeightOverride(Size);
}

void UBRItemTile::SetTileSize(EBRItemTileSize InTileSize)
{
	if (TileSizeVariant == InTileSize)
	{
		return;
	}

	TileSizeVariant = InTileSize;
	ApplyTileSize();
}

void UBRItemTile::ApplyTileType()
{
	// COMPONENT-SPECS Sec 5 `Type` axis. Geometry never changes between the three -- only what is
	// drawn inside the shell and whether the shell answers input.
	const bool bInteractive = (TileType != EBRItemTileType::NonInteractive);
	SetIsFocusable(bInteractive);
	SetIsSelectable(bInteractive);
}

void UBRItemTile::SetTileType(EBRItemTileType InTileType)
{
	if (TileType == InTileType)
	{
		return;
	}

	TileType = InTileType;
	ApplyTileType();
	ApplyItemData();
}

void UBRItemTile::SetItemData(const FBRItemTileData& InItemData)
{
	ItemData = InItemData;

	if (TileType == EBRItemTileType::Empty)
	{
		TileType = EBRItemTileType::Default;
		ApplyTileType();
	}

	ApplyItemData();
}

void UBRItemTile::ClearItemData()
{
	ItemData = FBRItemTileData();
	ItemEntry = nullptr;
	TileType = EBRItemTileType::Empty;

	ApplyTileType();
	ApplyItemData();
}

void UBRItemTile::ApplyItemData()
{
	const bool bEmpty = (TileType == EBRItemTileType::Empty);

	// The rarity signal, and the only place it is expressed.
	if (RarityLine)
	{
		FBRHairlineStyle LineStyle = RarityLine->GetHairlineStyle();
		LineStyle.Weight = BorderWeight;
		LineStyle.StrokeToken = bEmpty
			? EBRUIColorToken::ChromeStrokeDim
			: ResolveRarityStrokeToken(ItemData.Rarity);
		RarityLine->SetHairlineStyle(LineStyle);
	}

	if (RarityTint)
	{
		// The GRADIENT is the WBP's brush; C++ supplies only the tint, and nothing at all when
		// the cell is empty -- an empty cell tinted "common" reads as an owned common item.
		RarityTint->SetColorAndOpacity(BRUI::ResolveColorToken(
			bEmpty ? EBRUIColorToken::None : ResolveRarityStrokeToken(ItemData.Rarity)));
	}

	if (Art)
	{
		if (bEmpty || ItemData.Art.IsNull())
		{
			// Cancel any in-flight streaming request from the item this recycled tile used to
			// show, THEN clear the brush. Without the cancel a scrolled-away texture can land on
			// a tile that now represents something else -- the classic pooled-entry art swap.
			Art->SetBrushFromSoftTexture(TSoftObjectPtr<UTexture2D>());
			Art->SetBrushFromTexture(nullptr);
			Art->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			// Soft ref, async load, no ConstructorHelpers, no hard reference anywhere (law 3).
			Art->SetVisibility(ESlateVisibility::HitTestInvisible);
			Art->SetBrushFromSoftTexture(ItemData.Art);
		}
	}

	const auto ApplyBadge = [bEmpty](UWidget* Badge, bool bShown)
	{
		if (Badge)
		{
			Badge->SetVisibility((!bEmpty && bShown)
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
		}
	};

	ApplyBadge(LockedBadge, ItemData.bLocked);
	ApplyBadge(CurrencyBadge, ItemData.bShowCurrency);
	ApplyBadge(FavouriteBadge, ItemData.bFavourite);
	ApplyBadge(CheckMark, ItemData.bChecked);
	ApplyBadge(MultiCoreNotch, ItemData.bMultiCore);
}

void UBRItemTile::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	ItemEntry = Cast<UBRItemTileEntry>(ListItemObject);

	if (!ItemEntry)
	{
		// An item of the wrong type, or none at all, renders as the empty cell. It does not
		// render the PREVIOUS item's art, which is what a recycled entry would otherwise show.
		ClearItemData();
		return;
	}

	SetItemData(ItemEntry->GetItemData());
}

void UBRItemTile::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);

	// CommonUI owns the visual selected state; this keeps the button's own notion of selection in
	// step with the list's so a gamepad-driven selection and a click look identical.
	SetIsSelected(bIsSelected, /*bGiveClickFeedback*/ false);
}

void UBRItemTile::NativeOnEntryReleased()
{
	IUserObjectListEntry::NativeOnEntryReleased();

	// Released back to the view's pool: drop the payload and any pending art stream.
	ClearItemData();
}

void UBRItemTile::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (!ItemEntry)
	{
		return;
	}

	// A `UCommonButtonBase` entry consumes the mouse press itself, so the owning view never sees
	// the click and its selection would stay wherever the last gamepad move left it. Telling the
	// list what was picked is what keeps mouse and gamepad selection identical -- and it is what
	// lets the grid publish ONE selection delegate instead of the screen binding N tiles.
	if (UListView* OwningList = Cast<UListView>(GetOwningListView()))
	{
		OwningList->SetSelectedItem(ItemEntry);
	}

	// Intent, published. The tile changes no state of its own beyond selection visuals.
	OnItemTileClicked.Broadcast(ItemEntry);
}
