#include "UI/BNPromptButton.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

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
