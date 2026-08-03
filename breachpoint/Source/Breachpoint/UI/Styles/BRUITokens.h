#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"
#include "UObject/SoftObjectPath.h"

/**
 * The BREACHPOINT design tokens, defined ONCE.
 *
 * `ui-presentation` SKILL.md Sec.9 requires the palette live in a single place. Its
 * preferred home is `UBRUISettings` (config = Game), which is NOT this packet's owner
 * path -- so the tokens live here for now and the migration is filed as a dependency.
 * When they move, this header becomes thin forwarders and NOTHING else changes: every
 * consumer already goes through these functions, not through hex literals.
 *
 * Hex values are transcribed from docs/UI-DESIGN-SYSTEM.md Sec.2 (colour) and
 * docs/ui/REFERENCE-EXTRACTION.md Sec.2 (type). They are sRGB, exactly as authored in
 * Figma, and are gamma-decoded to linear here -- see Hex().
 */
namespace BR::Tokens
{
	/**
	 * 0xRRGGBB as authored in Figma (sRGB) -> Slate's linear colour space.
	 *
	 * Uses FLinearColor::FromSRGBColor, which is what UMG's colour picker round-trips
	 * through when a designer types a hex value. ReinterpretAsLinear() (a raw /255) is
	 * the classic mistake here and produces visibly washed-out chrome.
	 *
	 * Not constexpr: FromSRGBColor is a table lookup. These run once per CDO.
	 */
	FORCEINLINE FLinearColor Hex(const uint32 RGB, const float Alpha = 1.0f)
	{
		FLinearColor Out = FLinearColor::FromSRGBColor(FColor(
			static_cast<uint8>((RGB >> 16) & 0xFFu),
			static_cast<uint8>((RGB >> 8) & 0xFFu),
			static_cast<uint8>(RGB & 0xFFu),
			255));
		Out.A = Alpha;
		return Out;
	}

	// -- Colour (UI-DESIGN-SYSTEM.md Sec.2). A token means ONE thing. ------------------

	/** You: your shields, your team, reticle at rest, grapple ready. */
	FORCEINLINE FLinearColor Shield(const float A = 1.0f) { return Hex(0x35D0F2, A); }
	/** Shield bar gradient floor. */
	FORCEINLINE FLinearColor ShieldLow(const float A = 1.0f) { return Hex(0x0E7E9B, A); }
	/** Health beneath shields. Hidden until damaged. Yellow, never green. */
	FORCEINLINE FLinearColor Health(const float A = 1.0f) { return Hex(0xF5C542, A); }
	/** A clock is running: rocket countdown, medals, host authority. */
	FORCEINLINE FLinearColor Amber(const float A = 1.0f) { return Hex(0xFFA333, A); }
	/** Threat channel ONLY: enemy reticle, incoming damage, shield-break flash. */
	FORCEINLINE FLinearColor Enemy(const float A = 1.0f) { return Hex(0xFF4A3D, A); }
	/** Opposing team in lists, feeds, scoreboards. */
	FORCEINLINE FLinearColor TeamThem(const float A = 1.0f) { return Hex(0xFF7A45, A); }
	/** You in a list of people; header bars; the inverted menu-row fill. */
	FORCEINLINE FLinearColor SelfWhite(const float A = 1.0f) { return Hex(0xFFFFFF, A); }
	/** Ground -- near-black with a blue bias, deliberately not neutral grey. */
	FORCEINLINE FLinearColor Void(const float A = 1.0f) { return Hex(0x05080C, A); }
	/** Panel ground. */
	FORCEINLINE FLinearColor Deep(const float A = 1.0f) { return Hex(0x0A1018, A); }
	/** Hairline borders. */
	FORCEINLINE FLinearColor Edge(const float A = 1.0f) { return Hex(0x1E2C3A, A); }
	/** Secondary text. */
	FORCEINLINE FLinearColor InkDim(const float A = 1.0f) { return Hex(0x8397A9, A); }
	/** Disabled / unavailable. */
	FORCEINLINE FLinearColor Dead(const float A = 1.0f) { return Hex(0x4A5A6B, A); }

	// -- Type (REFERENCE-EXTRACTION.md Sec.2) ------------------------------------------
	//
	// Rajdhani ships NO italic. Flavor text is therefore Roboto Condensed Medium Italic.
	// That font split is deliberate in the reference and we keep it.
	//
	// Letter spacing is FSlateFontInfo::LetterSpacing -- an int32 in 1/1000 em, so the
	// design system's "15%" is the UMG value 150. Verified: SlateFontInfo.h:179.

	/** Typeface name inside the Rajdhani composite font asset. */
	inline const FName TypefaceRajdhani = FName(TEXT("SemiBold"));
	/** Typeface name inside the Roboto Condensed composite font asset. */
	inline const FName TypefaceRobotoCondensedItalic = FName(TEXT("MediumItalic"));

	/**
	 * Default soft paths to the two font assets. SOFT, per CLAUDE.md law 3 -- an
	 * FSoftObjectPath built from a string literal creates no package dependency and
	 * loads nothing. Both are overridable per-style from Config/DefaultGame.ini
	 * (see UBRTextStyleBase::FontAsset), so nothing here is baked.
	 *
	 * Content/UI is another packet's owner path; these paths are a coordination point,
	 * not a promise that the assets exist yet.
	 */
	FORCEINLINE FSoftObjectPath FontRajdhani()
	{
		return FSoftObjectPath(TEXT("/Game/UI/Fonts/F_Rajdhani.F_Rajdhani"));
	}
	FORCEINLINE FSoftObjectPath FontRobotoCondensed()
	{
		return FSoftObjectPath(TEXT("/Game/UI/Fonts/F_RobotoCondensed.F_RobotoCondensed"));
	}

	// -- Stroke, opacity, geometry ------------------------------------------------------
	//
	// THREE stroke weights exist. Nothing else is legal. Corner radius is 0 EVERYWHERE:
	// every brush in this folder is an FSlateColorBrush (DrawAs = Image), which cannot
	// round a corner even by accident. A rounded panel is a defect, so the type system
	// is doing the enforcing rather than a review comment.

	inline constexpr float StrokeHair = 0.5f;
	inline constexpr float StrokeThin = 1.0f;
	inline constexpr float StrokeHeavy = 2.0f;

	/** Nav-tab active border, drawn OUTSIDE the component. See report: the packet says
	 *  3px, the three-weight rule says 2px is the heaviest legal stroke. 2px used. */
	inline constexpr float StrokeNavTabActive = StrokeHeavy;

	/** Inactive nav tab dims the WHOLE component, not just its text. Applied by the
	 *  widget as render opacity -- there is no CommonUI style field for it. */
	inline constexpr float OpacityNavTabInactive = 0.6f;

	/** Menu-row underline: 0.3 at rest, 1.0 when hovered/selected (the inversion). */
	inline constexpr float OpacityMenuRowLineIdle = 0.3f;
	inline constexpr float OpacityMenuRowLineActive = 1.0f;

	/** Measured geometry (REFERENCE-EXTRACTION.md Sec.3), base 1280x720. */
	inline constexpr int32 MenuRowHeight = 28;
	inline constexpr int32 MenuRowPitch = 40;
	inline constexpr int32 NavBarHeight = 30;
}
