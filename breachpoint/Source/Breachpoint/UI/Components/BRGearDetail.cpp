#include "UI/Components/BRGearDetail.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"

#define LOCTEXT_NAMESPACE "BRGearDetail"

FText UBRGearDetail::GetUnknownValueText()
{
	// The em dash as a universal-character escape, so this file stays pure ASCII like every other
	// source file in the module. Identical to `UBRProfileBar::GetUnknownValueText`.
	// CONTRACT GAP: this is now the SECOND copy of the unknown-value text. It belongs in
	// `UI/BRUITypes.h` (`BRUI::UnknownValueText()`), which is not this packet's owner path.
	return LOCTEXT("UnknownValue", "\u2014");
}

void UBRGearDetail::ApplyFieldText(UCommonTextBlock* Field, const FText& InText)
{
	if (!Field)
	{
		return;
	}

	// Empty is never blank. A blank name next to a populated description reads as "this item has
	// no name"; the dash reads as "not known yet", which is the true statement.
	Field->SetText(InText.IsEmpty() ? GetUnknownValueText() : InText);
}

void UBRGearDetail::NativeOnInitialized()
{
	// Applies the panel ground + stroke weight.
	Super::NativeOnInitialized();

	ApplyVariant();

	// Start honest. Nothing has been focused yet on the frame this widget is constructed.
	ClearDetail();
}

void UBRGearDetail::ApplyVariant()
{
	const bool bIsMaterial = (Variant == EBRGearDetailVariant::Material);

	// SCREEN-BUILD-SPEC Sec 3.8: 586x161 -> 586x125. Width is authored in the WBP and never
	// driven -- SCREEN-MANIFEST Sec 8.2, the columns do not stretch.
	SetPanelHeight(bIsMaterial ? MaterialDetailHeight : DetailHeight);

	if (MakerRow)
	{
		// Collapsed, not Hidden: the row must give its MakerRowHeight back to the layout, which is
		// the entire reason the two heights differ.
		MakerRow->SetVisibility(bIsMaterial ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UBRGearDetail::SetVariant(EBRGearDetailVariant InVariant)
{
	if (Variant == InVariant)
	{
		return;
	}

	Variant = InVariant;
	ApplyVariant();
}

void UBRGearDetail::SetDetail(const FBRGearDetailView& InDetail)
{
	DataState = EBRUIDataState::Live;

	ApplyFieldText(ItemName, InDetail.DisplayName);
	ApplyFieldText(Description, InDetail.Description);

	// On `Material` the row is collapsed and this write is invisible -- deliberate. The text is
	// still set so a variant flip back to `Item` cannot expose a stale maker from two items ago.
	ApplyFieldText(MakerName, InDetail.Maker);
}

void UBRGearDetail::ClearDetail()
{
	SetDetail(FBRGearDetailView());

	// An empty push is the UNKNOWN state, not a live blank -- set after `SetDetail`, which marks
	// Live for real pushes.
	DataState = EBRUIDataState::Unknown;

	SetMakerMark(FSlateBrush());
}

void UBRGearDetail::SetMakerMark(const FSlateBrush& InBrush)
{
	if (!MakerMark)
	{
		return;
	}

	// No resource means no mark to draw. Law 3: the caller resolved the soft ref; this widget
	// never loads an asset and holds no hard reference to one.
	const bool bHasMark = (InBrush.GetResourceObject() != nullptr);

	if (bHasMark)
	{
		MakerMark->SetBrush(InBrush);
	}

	MakerMark->SetVisibility(bHasMark ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

#undef LOCTEXT_NAMESPACE
