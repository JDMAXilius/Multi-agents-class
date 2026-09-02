#include "UI/BNPromptButton.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

UBNButtonStyle_Prompt::UBNButtonStyle_Prompt()
{
	const FLinearColor Idle(1.0f, 1.0f, 1.0f, 0.10f);
	const FLinearColor Hover(1.0f, 1.0f, 1.0f, 0.35f);
	// sRGB (55,167,193): the cyan the START row's bottom rule was sampled at.
	const FLinearColor Pressed(0.037f, 0.385f, 0.533f, 1.0f);
	NormalBase = FSlateColorBrush(Idle);
	NormalHovered = FSlateColorBrush(Hover);
	NormalPressed = FSlateColorBrush(Pressed);
	SelectedBase = NormalBase;
	SelectedHovered = NormalHovered;
	SelectedPressed = NormalPressed;
	ButtonPadding = FMargin(8.0f, 3.0f, 12.0f, 3.0f);
}

void UBNPromptButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Synchronous on purpose: a 40px disc, loaded once when a menu screen is built. An async
	// handle here would be machinery for a stall nobody can measure.
	if (Glyph)
	{
		if (UTexture2D* Texture = GlyphTexture.LoadSynchronous())
		{
			Glyph->SetBrushFromTexture(Texture, /*bMatchSize*/ false);
		}
	}
}

void UBNPromptButton::SetVerbText(const FText& InVerb)
{
	if (Verb)
	{
		Verb->SetText(InVerb);
	}
}
