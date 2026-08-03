#include "UI/Components/BRItemGrid.h"

#include "CommonTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/TileView.h"
#include "UI/Components/BRScrollBar.h"

#define LOCTEXT_NAMESPACE "BRItemGrid"

FText UBRItemGrid::GetUnknownStateText()
{
	// "We have not been told" -- NOT "you own nothing". The em dash is a universal-character
	// escape so this file stays pure ASCII like every other source file in the module.
	return LOCTEXT("ItemGridUnknown", "\u2014");
}

FText UBRItemGrid::GetEmptyStateText()
{
	return LOCTEXT("ItemGridEmpty", "NO ITEMS");
}

void UBRItemGrid::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (TileView)
	{
		// The cell IS the pitch. SCREEN-MANIFEST Sec 8.2: the tile stays 114 and the pitch stays
		// 130 at every aspect ratio; the 16 gutter is the slack inside a 130 cell, which is why
		// widening the grid can only ever add cells and can never grow one.
		TileView->SetEntryWidth(TilePitch);
		TileView->SetEntryHeight(TilePitch);

		// The view's own native selection event (its Blueprint delegates are private to UMG).
		// This is the whole reason `UBRItemTile` selects itself in its owning list on click.
		TileView->OnItemSelectionChanged().AddUObject(this, &UBRItemGrid::HandleListSelectionChanged);
	}

	if (ScrollBar)
	{
		// Collapsed on purpose -- see the header. An undriven rail is a false statement about how
		// much content the grid holds, and UMG gives no public way to drive it from a TileView.
		ScrollBar->SetVisibility(ESlateVisibility::Collapsed);
	}

	ApplyColumnCount();

	// Explicit unknown, before anything can push. There is no ViewModel behind this widget, so
	// "unknown" is not a transient first-frame state here -- it is the state until a screen feeds
	// it, and it must not read as an empty inventory.
	ClearToUnknown();
}

void UBRItemGrid::ApplyColumnCount()
{
	if (!TileViewBox)
	{
		return;
	}

	if (ColumnCount > 0)
	{
		TileViewBox->SetWidthOverride(WrapWidthForColumns(ColumnCount));
	}
	else
	{
		// Ultrawide / auto: the view wraps to whatever its slot allows and ADDS columns. Tiles
		// keep their 114 and the pitch keeps its 130 either way.
		TileViewBox->ClearWidthOverride();
	}
}

void UBRItemGrid::SetColumnCount(int32 InColumnCount)
{
	if (ColumnCount == InColumnCount)
	{
		return;
	}

	ColumnCount = InColumnCount;
	ApplyColumnCount();
}

void UBRItemGrid::SetItemData(const TArray<FBRItemTileData>& InItems)
{
	// Grow the entry pool to fit and reuse every object already in it; a filter keystroke on a
	// large inventory must not allocate a fresh UObject per visible item.
	if (EntryPool.Num() < InItems.Num())
	{
		EntryPool.Reserve(InItems.Num());
		while (EntryPool.Num() < InItems.Num())
		{
			EntryPool.Add(NewObject<UBRItemTileEntry>(this));
		}
	}

	TArray<UObject*> ListItems;
	ListItems.Reserve(InItems.Num());

	for (int32 Index = 0; Index < InItems.Num(); ++Index)
	{
		UBRItemTileEntry* Entry = EntryPool[Index];
		if (!Entry)
		{
			// A pooled slot that lost its object (GC of a stale outer) is refilled rather than
			// silently dropping the item -- a grid that shows 39 of 40 items is a lie nobody
			// counts.
			Entry = NewObject<UBRItemTileEntry>(this);
			EntryPool[Index] = Entry;
		}

		Entry->SetItemData(InItems[Index]);
		ListItems.Add(Entry);
	}

	if (TileView)
	{
		TileView->SetListItems(ListItems);
	}

	// An empty push is a LIVE empty ("we asked, there is nothing"), which is a different fact
	// from the unknown state and reads differently on screen.
	DataState = EBRUIDataState::Live;
	ApplyDataState();
}

void UBRItemGrid::ClearToUnknown()
{
	if (TileView)
	{
		TileView->ClearListItems();
	}

	DataState = EBRUIDataState::Unknown;
	ApplyDataState();

	// Selection cannot survive a feed we no longer trust; say so once rather than leaving a gear
	// detail panel describing an item that is no longer on screen.
	OnSelectionChanged.Broadcast(nullptr);
}

void UBRItemGrid::ApplyDataState()
{
	if (!EmptyStateLabel)
	{
		return;
	}

	const int32 NumItems = TileView ? TileView->GetNumItems() : 0;
	if (NumItems > 0)
	{
		EmptyStateLabel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	EmptyStateLabel->SetText((DataState == EBRUIDataState::Live)
		? GetEmptyStateText()
		: GetUnknownStateText());
	EmptyStateLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBRItemGrid::HandleListSelectionChanged(UObject* Item)
{
	// Null on deselection, and that is published too: the caller's detail panel must be able to
	// go back to nothing rather than keep describing the last thing anyone touched.
	OnSelectionChanged.Broadcast(Cast<UBRItemTileEntry>(Item));
}

#undef LOCTEXT_NAMESPACE
