#include "UI/BNSettingsPanel.h"

#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "UI/BNUITypes.h"

#define LOCTEXT_NAMESPACE "BNSettingsPanel"

namespace
{
	/** The section header's own label — 20 tall over a 3px underline, per `Decorative Line`. */
	constexpr int32 HeaderFontSize = 14;
	constexpr int32 RowFontSize = 12;
}

void UBNSettingsPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		RootSizeBox->SetWidthOverride(PanelWidth);
		RootSizeBox->SetHeightOverride(PanelHeight);
	}
}

void UBNSettingsPanel::SetGamemode(const FText& InTitle, const FText& InDescription,
	const TSoftObjectPtr<UTexture2D>& InIcon, const FText& InVersion)
{
	if (ModeTitle)
	{
		ModeTitle->SetText(InTitle);
	}

	if (ModeDescription)
	{
		ModeDescription->SetText(InDescription);
		ModeDescription->SetVisibility(InDescription.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (VersionText)
	{
		VersionText->SetText(InVersion);
		VersionText->SetVisibility(InVersion.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (!ModeIcon)
	{
		return;
	}

	// Collapse rather than draw an untextured UImage: an Image with no brush is a white box,
	// which is the loudest way a missing soft ref announces itself as a bug.
	if (InIcon.IsNull())
	{
		ModeIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (UTexture2D* Loaded = InIcon.LoadSynchronous())
	{
		ModeIcon->SetBrushFromTexture(Loaded, /*bMatchSize*/ false);
		ModeIcon->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
		ModeIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		ModeIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBNSettingsPanel::SetSections(const TArray<FBNSettingSection>& InSections)
{
	Sections = InSections;

	if (!SectionBox)
	{
		return;
	}

	SectionBox->ClearChildren();
	for (const FBNSettingSection& Section : Sections)
	{
		BuildSection(Section);
	}
}

void UBNSettingsPanel::BuildSection(const FBNSettingSection& InSection)
{
	// --- the header: a label with its own underline, then the full-width rule -------------
	if (!InSection.Header.IsEmpty())
	{
		UCommonTextBlock* Header = NewObject<UCommonTextBlock>(this);
		Header->SetText(InSection.Header);
		Header->SetColorAndOpacity(FSlateColor(BNUIColors::Self));
		if (UVerticalBoxSlot* HeaderSlot = SectionBox->AddChildToVerticalBox(Header))
		{
			// `Decorative Line` sits 23 tall and the list starts at +32, so the 9 below the
			// header is the measured gap, not padding chosen to look right.
			HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, SectionListOffsetY - SectionHeaderHeight));
			HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	// --- the rows: label left, value right, 18 tall at pitch 23 --------------------------
	const int32 LastIndex = InSection.Rows.Num() - 1;
	for (int32 Index = 0; Index <= LastIndex; ++Index)
	{
		const FBNSettingRow& Row = InSection.Rows[Index];

		UHorizontalBox* Line = NewObject<UHorizontalBox>(this);

		UCommonTextBlock* Label = NewObject<UCommonTextBlock>(this);
		Label->SetText(Row.Label);
		Label->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim));
		if (UHorizontalBoxSlot* LabelSlot = Line->AddChildToHorizontalBox(Label))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		// The value is right-aligned to the panel edge, so the label column can be any width
		// and the numbers still line up — which is the whole reason this is a box and not two
		// absolutely-placed TextBlocks.
		if (!Row.Value.IsEmpty())
		{
			UCommonTextBlock* Value = NewObject<UCommonTextBlock>(this);
			Value->SetText(Row.Value);
			Value->SetColorAndOpacity(FSlateColor(BNUIColors::Self));
			Value->SetJustification(ETextJustify::Right);
			if (UHorizontalBoxSlot* ValueSlot = Line->AddChildToHorizontalBox(Value))
			{
				ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				ValueSlot->SetHorizontalAlignment(HAlign_Right);
				ValueSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		if (UVerticalBoxSlot* LineSlot = SectionBox->AddChildToVerticalBox(Line))
		{
			// Pitch 23 on an 18-tall row is a 5 gap, carried as bottom padding on every row
			// but the last so the section's height stays exactly N*18 + (N-1)*5.
			const float Bottom = (Index == LastIndex) ? SectionListOffsetY : (RowPitch - RowHeight);
			LineSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Bottom));
			LineSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}
}

#undef LOCTEXT_NAMESPACE
