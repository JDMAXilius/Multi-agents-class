#include "UI/Components/BRScrollBar.h"

#include "Components/ScrollBar.h"
#include "Components/SizeBox.h"

float UBRScrollBar::GetTrackThickness(EBRScrollBarWeight InWeight)
{
	return (InWeight == EBRScrollBarWeight::Wide) ? WideTrackThickness : SlimTrackThickness;
}

float UBRScrollBar::GetThumbThickness(EBRScrollBarWeight InWeight)
{
	return (InWeight == EBRScrollBarWeight::Wide) ? WideThumbThickness : SlimThumbThickness;
}

void UBRScrollBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ApplyWeight();
}

void UBRScrollBar::ApplyWeight()
{
	const float TrackThickness = GetTrackThickness(Weight);
	const float ThumbThickness = GetThumbThickness(Weight);

	if (Track)
	{
		if (bVertical)
		{
			Track->SetWidthOverride(TrackThickness);
			Track->ClearHeightOverride();
		}
		else
		{
			Track->SetHeightOverride(TrackThickness);
			Track->ClearWidthOverride();
		}
	}

	if (ScrollBar)
	{
		// SScrollBar takes thickness on both axes; the minor axis is the one that reads.
		ScrollBar->SetThickness(FVector2D(ThumbThickness, ThumbThickness));
	}
}

void UBRScrollBar::SetScrollBarWeight(EBRScrollBarWeight InWeight)
{
	if (Weight == InWeight)
	{
		return;
	}

	Weight = InWeight;
	ApplyWeight();
}
