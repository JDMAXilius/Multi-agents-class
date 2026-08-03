#include "UI/Components/BRButtonPrompt.h"

#include "CommonActionWidget.h"
#include "CommonTextBlock.h"
#include "Components/SizeBox.h"

void UBRButtonPrompt::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RootSizeBox)
	{
		// Height is the only fixed dimension. Deliberately no width override.
		RootSizeBox->SetHeightOverride(PromptHeight);
	}

	// Nothing is a valid prompt until a screen supplies one.
	ClearPrompt();
}

void UBRButtonPrompt::SetPrompt(UInputAction* InAction, const FText& InVerb)
{
	if (!InAction)
	{
		ClearPrompt();
		return;
	}

	if (ActionGlyph)
	{
		ActionGlyph->SetEnhancedInputAction(InAction);
	}

	if (Verb)
	{
		Verb->SetText(InVerb);
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UBRButtonPrompt::ClearPrompt()
{
	if (Verb)
	{
		Verb->SetText(FText::GetEmpty());
	}

	// CPP-AUDIT: the glyph must clear too. Collapsed hides the stale action today, but any
	// future path that shows the widget without SetPrompt would render the LAST prompt's glyph
	// — a promise about input that no longer exists.
	if (ActionGlyph)
	{
		ActionGlyph->SetEnhancedInputAction(nullptr);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}
