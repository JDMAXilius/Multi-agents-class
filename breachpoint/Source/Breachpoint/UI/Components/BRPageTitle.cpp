#include "UI/Components/BRPageTitle.h"

#include "CommonTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Widget.h"
#include "UI/Components/BRHairlineBorder.h"

// ===========================================================================================
// UBRPageTitle
// ===========================================================================================

void UBRPageTitle::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		// Full-bleed band: only the height is driven. The width comes from the screen's
		// top-stretch anchor (SCREEN-MANIFEST Sec 7.3), so typing 1280 into a width override here
		// would break every aspect ratio wider than 16:9.
		RootSizeBox->SetHeightOverride(GetBandHeight());
	}

	if (Title)
	{
		Title->SetColorAndOpacity(FSlateColor(BRUI::ResolveColorToken(TitleTextToken)));
	}

	// Nothing is a valid title until a screen supplies one -- an empty band beats a stale one.
	SetTitleText(FText::GetEmpty());
	SetBreadcrumbText(FText::GetEmpty());
	ApplyTitleType();
}

void UBRPageTitle::SetTitleText(const FText& InText)
{
	if (Title)
	{
		Title->SetText(InText);
	}
}

void UBRPageTitle::SetBreadcrumbText(const FText& InText)
{
	if (!Breadcrumb)
	{
		return;
	}

	Breadcrumb->SetText(InText);

	// Honest empty state: a drill-down whose parent has not resolved shows no breadcrumb rather
	// than the previous screen's.
	const bool bShowBreadcrumb = (TitleType == EBRPageTitleType::DrillDown) && !InText.IsEmpty();
	Breadcrumb->SetVisibility(bShowBreadcrumb ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UBRPageTitle::SetTitleType(EBRPageTitleType InType)
{
	if (TitleType == InType)
	{
		return;
	}

	TitleType = InType;
	ApplyTitleType();
}

void UBRPageTitle::ApplyTitleType()
{
	const bool bDrillDown = (TitleType == EBRPageTitleType::DrillDown);

	if (BackAffordance)
	{
		BackAffordance->SetVisibility(bDrillDown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Breadcrumb)
	{
		const bool bShowBreadcrumb = bDrillDown && !Breadcrumb->GetText().IsEmpty();
		Breadcrumb->SetVisibility(bShowBreadcrumb ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// ===========================================================================================
// UBRItemTitle
// ===========================================================================================

void UBRItemTitle::NativeOnInitialized()
{
	// Runs the shared band setup first; GetBandHeight() is overridden, so the 105 lands there and
	// the 75 -> 105 swap has exactly one implementation.
	Super::NativeOnInitialized();

	if (RarityLabel)
	{
		RarityLabel->SetText(FText::GetEmpty());
		RarityLabel->SetVisibility(ESlateVisibility::Collapsed);
	}

	SetEquipped(bEquipped);
	ApplyRarity();
}

EBRUIColorToken UBRItemTitle::ResolveRarityToken(EBRItemRarity InRarity)
{
	// TODO(Styles lane): the four rarity tokens do not exist yet -- see the header's contract-gap
	// note. Every rarity deliberately resolves to the chrome stroke rather than to a hex literal.
	switch (InRarity)
	{
	case EBRItemRarity::Rare:
	case EBRItemRarity::Epic:
	case EBRItemRarity::Legendary:
	case EBRItemRarity::Common:
	default:
		return EBRUIColorToken::ChromeStroke;
	}
}

void UBRItemTitle::SetItem(const FText& InItemName, EBRItemRarity InRarity, const FText& InRarityLabel)
{
	SetTitleText(InItemName);

	Rarity = InRarity;

	if (RarityLabel)
	{
		RarityLabel->SetText(InRarityLabel);

		// An item whose rarity has not resolved shows no tag rather than a blank frame.
		RarityLabel->SetVisibility(InRarityLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	ApplyRarity();
}

void UBRItemTitle::ApplyRarity()
{
	if (!RarityTagBorder)
	{
		return;
	}

	FBRHairlineStyle Style = RarityTagBorder->GetHairlineStyle();
	Style.StrokeToken = ResolveRarityToken(Rarity);
	RarityTagBorder->SetHairlineStyle(Style);
}

void UBRItemTitle::SetEquipped(bool bInEquipped)
{
	bEquipped = bInEquipped;

	if (EquippedCheck)
	{
		EquippedCheck->SetVisibility(bEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
