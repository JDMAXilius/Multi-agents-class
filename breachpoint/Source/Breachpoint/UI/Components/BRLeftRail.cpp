#include "UI/Components/BRLeftRail.h"

#include "Animation/WidgetAnimation.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "UI/Components/BRFeatureCard.h"
#include "UI/Components/BRHairlineBorder.h"

namespace
{
	/**
	 * Written once because three call sites need it and each hand-rolled cast is a place the
	 * rail silently stops laying itself out when a WBP author reparents a child.
	 */
	UCanvasPanelSlot* BRGetCanvasSlot(const UWidget* InWidget)
	{
		return InWidget ? Cast<UCanvasPanelSlot>(InWidget->Slot) : nullptr;
	}
}

void UBRLeftRail::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// SCREEN-MANIFEST Sec 6.2: the notches are GEOMETRY, and the wipe originates from them. C++
	// owns their SIZE (88 x 4.727, measured); their x is unmeasured and stays with the WBP.
	if (UCanvasPanelSlot* TopSlot = BRGetCanvasSlot(RevealNotchTop))
	{
		TopSlot->SetSize(FVector2D(NotchWidth, NotchHeight));
	}

	if (UCanvasPanelSlot* BottomSlot = BRGetCanvasSlot(RevealNotchBottom))
	{
		BottomSlot->SetSize(FVector2D(NotchWidth, NotchHeight));
	}

	// Nothing has focus until a screen says so. See SetFocusedRowIndex.
	SetFocusedRowIndex(INDEX_NONE);

	ApplyRailLayout();
}

void UBRLeftRail::NativeConstruct()
{
	Super::NativeConstruct();

	// The rail's own canvas slot only exists once it has been added to a canvas, which is after
	// NativeOnInitialized. Re-applying here is what actually pins x = 69 / w = 349.
	ApplyRailLayout();
}

float UBRLeftRail::GetMenuBorderTop() const
{
	return FeatureCard ? FeatureCardHeight + FeatureCardGap : 0.0f;
}

void UBRLeftRail::ApplyRailLayout()
{
	const float RailHeight = ComputeRailHeight(RowCount, FeatureCard != nullptr);

	if (RootSizeBox)
	{
		// The rail is a fixed-width instrument (SCREEN-MANIFEST Sec 7.2). Width is pinned here so
		// no WBP details panel and no screen can widen column 1.
		RootSizeBox->SetWidthOverride(RailWidth);
		RootSizeBox->SetHeightOverride(RailHeight);
	}

	if (MenuBorderBox)
	{
		MenuBorderBox->SetWidthOverride(RailWidth);
		MenuBorderBox->SetHeightOverride(ComputeMenuBorderHeight(RowCount));
	}

	// x = 69 and width = 349 are constants; only y is data.
	if (UCanvasPanelSlot* RailSlot = BRGetCanvasSlot(this))
	{
		RailSlot->SetPosition(FVector2D(RailX, RailOriginY));
		RailSlot->SetSize(FVector2D(RailWidth, RailHeight));
	}

	ApplyCaret();
}

void UBRLeftRail::ApplyCaret()
{
	if (!SelectionCaret)
	{
		return;
	}

	if (FocusedRowIndex < 0)
	{
		// Honest empty state: no focus means no caret, not a caret parked on row 0.
		SelectionCaret->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SelectionCaret->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UCanvasPanelSlot* CaretSlot = BRGetCanvasSlot(SelectionCaret))
	{
		// Rail-local, so x = -4 hugs the outside of the panel exactly as the reference authors it.
		// The caret is 65 tall against a 28 row, so it is CENTRED on the row it marks rather than
		// aligned to its top -- that is the only reading of the authored art that survives a
		// changing row count.
		const float CaretY = GetMenuBorderTop()
			+ MenuRowSlotInset
			+ (static_cast<float>(FocusedRowIndex) * UBRMenuRow::RowPitch)
			+ ((UBRMenuRow::RowHeight - CaretHeight) * 0.5f);

		CaretSlot->SetPosition(FVector2D(CaretOffsetX, CaretY));
		CaretSlot->SetSize(FVector2D(CaretWidth, CaretHeight));
	}
}

void UBRLeftRail::SetRailOriginY(float InOriginY)
{
	RailOriginY = InOriginY;

	ApplyRailLayout();
}

void UBRLeftRail::SetRowCount(int32 InRowCount)
{
	RowCount = FMath::Max(0, InRowCount);

	ApplyRailLayout();
}

void UBRLeftRail::RefreshLayout()
{
	// No `> 0` guard (CPP-AUDIT D8): zero children IS a valid count. The old guard kept the
	// previous row count when the screen cleared its rows, so the rail rendered at a stale
	// five-row height with nothing in it and the caret could validate against rows that no
	// longer exist — on exactly the two empty-state paths the screen actually exercises.
	if (MenuRowSlot)
	{
		RowCount = MenuRowSlot->GetChildrenCount();
	}

	ApplyRailLayout();
}

void UBRLeftRail::SetFocusedRowIndex(int32 InRowIndex)
{
	FocusedRowIndex = (InRowIndex >= 0 && InRowIndex < RowCount) ? InRowIndex : INDEX_NONE;

	ApplyCaret();
}

void UBRLeftRail::SetRailOpen(bool bInOpen, bool bPlayAnimation)
{
	if (bIsOpen == bInOpen)
	{
		return;
	}

	bIsOpen = bInOpen;

	if (RevealAnim && bPlayAnimation)
	{
		// The wipe is authored from the notches outward; C++ owns only the direction.
		if (bIsOpen)
		{
			SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			PlayAnimationForward(RevealAnim);
		}
		else
		{
			PlayAnimationReverse(RevealAnim);
		}

		return;
	}

	// No animation bound: state the truth instantly rather than pretending a wipe happened.
	SetVisibility(bIsOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

UWidget* UBRLeftRail::GetFirstFocusTarget() const
{
	if (FeatureCard)
	{
		return FeatureCard.Get();
	}

	if (MenuRowSlot && MenuRowSlot->GetChildrenCount() > 0)
	{
		return MenuRowSlot->GetChildAt(0);
	}

	return nullptr;
}

UPanelWidget* UBRLeftRail::GetMenuRowSlot() const
{
	return MenuRowSlot;
}

UPanelWidget* UBRLeftRail::GetDescriptionSlot() const
{
	return DescriptionSlot;
}

UBRFeatureCard* UBRLeftRail::GetFeatureCard() const
{
	return FeatureCard;
}
