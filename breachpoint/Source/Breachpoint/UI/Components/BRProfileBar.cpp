#include "UI/Components/BRProfileBar.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"

#define LOCTEXT_NAMESPACE "BRProfileBar"

FText UBRProfileBar::GetUnknownValueText()
{
	// The em dash is written as a universal-character escape so this file stays pure ASCII
	// like every other source file in the module (a raw non-ASCII byte with no BOM is MSVC
	// C4819, and no existing Breachpoint .h/.cpp has one).
	return LOCTEXT("UnknownValue", "\u2014");
}

void UBRProfileBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		// ui-presentation Sec 5: the bottom 50 is reserved, always.
		RootSizeBox->SetHeightOverride(BarHeight);
	}

	if (Ground)
	{
		Ground->SetColorAndOpacity(BRUI::ResolveColorToken(GroundToken));
	}

	ClearIdentity();
}

void UBRProfileBar::SetIdentity(const FText& InGamertag, const FText& InStatus)
{
	if (Gamertag)
	{
		Gamertag->SetText(InGamertag.IsEmpty() ? GetUnknownValueText() : InGamertag);
	}

	if (Status)
	{
		Status->SetText(InStatus.IsEmpty() ? GetUnknownValueText() : InStatus);
	}
}

void UBRProfileBar::ClearIdentity()
{
	SetIdentity(FText::GetEmpty(), FText::GetEmpty());
}

#undef LOCTEXT_NAMESPACE
