#include "UI/Styles/BRTextStyles.h"

#include "CoreGlobals.h"       // GIsRunning
#include "Misc/CoreDelegates.h"
#include "Misc/CoreMisc.h"     // IsRunningDedicatedServer()
#include "UI/Styles/BRUITokens.h"

// -- UBRTextStyleBase ------------------------------------------------------------------

UBRTextStyleBase::UBRTextStyleBase()
{
	// Everything that is not an asset reference is set here, in plain C++. Only the font
	// object needs the deferred dance below.
	Color = BR::Tokens::SelfWhite();
	bUsesDropShadow = false;
	ShadowOffset = FVector2D::ZeroVector;
	ShadowColor = FLinearColor::Transparent;
	Margin = FMargin(0.0f);
	LineHeightPercentage = 1.0f;
	ApplyLineHeightToBottomLine = true;

	Font.Size = 14.0f;
	Font.LetterSpacing = 0;
	Font.TypefaceFontName = BR::Tokens::TypefaceRajdhani;

	FontAsset = BR::Tokens::FontRajdhani();
}

void UBRTextStyleBase::ResolveFont()
{
	if (Font.FontObject || FontAsset.IsNull())
	{
		return;
	}

	// Synchronous by design: this runs once, at post-engine-init, off the game thread's
	// critical path and long before the first widget is built. An async load here would
	// buy nothing and open a window where a widget constructs against a null font.
	if (UObject* Loaded = FontAsset.LoadSynchronous())
	{
		Font.FontObject = Loaded;
	}
}

void UBRTextStyleBase::PostInitProperties()
{
	Super::PostInitProperties();

	// Only the CDO matters: UCommonTextBlock::GetStyleCDO() is the only reader.
	// Dedicated servers never draw text -- UCommonButtonStyle::NeedsLoadForServer()
	// establishes that precedent in CommonUI itself (CommonButtonBase.cpp:56).
	if (!HasAnyFlags(RF_ClassDefaultObject) || IsRunningDedicatedServer())
	{
		return;
	}

	if (GIsRunning)
	{
		// Late class registration (Live Coding, hot reload): engine is already up, so
		// GetOnPostEngineInit() has already broadcast and would never fire for us.
		ResolveFont();
	}
	else
	{
		// Normal startup. AddWeakLambda keys on `this` (the CDO), so the binding cannot
		// outlive the object -- and a CDO is never collected, so it always fires.
		FCoreDelegates::GetOnPostEngineInit().AddWeakLambda(this, [this]() { ResolveFont(); });
	}
}

// -- Concrete styles -------------------------------------------------------------------
//
// Sizes and letter spacings marked (measured) come from REFERENCE-EXTRACTION.md Sec.2.
// The rest are chosen here and flagged in the report as unmeasured.

UBRTextStyle_Tab::UBRTextStyle_Tab()
{
	Font.Size = 14.0f;
	Font.LetterSpacing = 150; // (measured) 15%
	Color = BR::Tokens::SelfWhite();
}

UBRTextStyle_MenuRow::UBRTextStyle_MenuRow()
{
	Font.Size = 16.0f;
	Font.LetterSpacing = 100; // (measured) 10%
	Color = BR::Tokens::SelfWhite();
}

UBRTextStyle_MenuRowInverted::UBRTextStyle_MenuRowInverted()
{
	Font.Size = 16.0f;
	Font.LetterSpacing = 100;
	// Void, not pure black: the ground colour is the ink when the fill inverts.
	Color = BR::Tokens::Void();
}

UBRTextStyle_Body::UBRTextStyle_Body()
{
	Font.Size = 14.0f;
	Font.LetterSpacing = 25;
	Color = BR::Tokens::SelfWhite();
}

UBRTextStyle_Numeral::UBRTextStyle_Numeral()
{
	Font.Size = 24.0f;
	Font.LetterSpacing = 0; // digits must sit on a fixed pitch; tracking breaks alignment
	Color = BR::Tokens::SelfWhite();
}

UBRTextStyle_Caption::UBRTextStyle_Caption()
{
	Font.Size = 11.0f;
	Font.LetterSpacing = 50;
	Color = BR::Tokens::InkDim();
}

UBRTextStyle_Flavor::UBRTextStyle_Flavor()
{
	Font.Size = 14.0f;
	Font.LetterSpacing = 80; // (measured) 8%
	Font.TypefaceFontName = BR::Tokens::TypefaceRobotoCondensedItalic;
	Color = BR::Tokens::InkDim();

	// The one style on a different family. Rajdhani ships no italic.
	FontAsset = BR::Tokens::FontRobotoCondensed();
}
