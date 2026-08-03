#pragma once

#include "CoreMinimal.h"

#include "BRComponentTokens.generated.h"

/**
 * Appearance tokens for the Tier-0 front-end components.
 *
 * WHY THIS FILE EXISTS AT ALL, AND WHY IT IS TEMPORARY
 * ---------------------------------------------------
 * `ui-presentation` SKILL §9 and SCREEN-MANIFEST §7.4 both say colour lives in ONE place and
 * that place is `UBRUISettings`. `UBRUISettings` has no palette yet, and `UI/BRUISettings.h`
 * is not this packet's owner path, so the tokens are declared here as NAMES with a resolver.
 *
 * DEPENDENCY ON THE STYLES LANE: when the palette lands on `UBRUISettings`, replace the body
 * of `BRUI::ResolveColorToken` with a settings read. The signature is the contract — no caller
 * in `UI/Components/` needs to change.
 *
 * There are no hex literals in this file and there must never be any. COMPONENT-SPECS §0 is
 * explicit that the front end is "white/black at varying alpha" (729 of the library's strokes
 * are plain `#ffffff`), so the whole Tier-0 palette is expressible as `FLinearColor::White` /
 * `FLinearColor::Black` plus the five discrete alphas COMPONENT-SPECS §8 measured. Accent
 * colours (event orange, rarity tints, the VISR channels of `ui-presentation` §3) are NOT in
 * this enum: no Tier-0 component uses one, and inventing them here would fork the palette.
 */
UENUM(BlueprintType)
enum class EBRUIColorToken : uint8
{
	/** Draws nothing. Used for "this edge is off" and "this plate has no fill". */
	None,

	/** COMPONENT-SPECS §0: 1px white chrome at full opacity. The default stroke, 404 uses. */
	ChromeStroke,

	/** COMPONENT-SPECS §2: the dimmed stroke — idle bottom line and the left/right ticks. */
	ChromeStrokeDim,

	/** Body and label text on an un-inverted surface. */
	TextPrimary,

	/** COMPONENT-SPECS §1: text on an INVERTED (solid white) surface. */
	TextInverted,

	/** COMPONENT-SPECS §1: the inversion fill itself — hover turns the plate solid white. */
	SurfaceInverted,

	/** COMPONENT-SPECS §8: panel grounds are five discrete black alphas, not arbitrary. */
	PanelGround20,

	PanelGround40,

	PanelGround50,

	PanelGround60,

	PanelGround80
};

/**
 * COMPONENT-SPECS §0 / SCREEN-MANIFEST §7.4: three stroke weights are in use and only three.
 * The enum exists so a fourth cannot be typed into a details panel.
 */
UENUM(BlueprintType)
enum class EBRStrokeWeight : uint8
{
	/** 0.5px — fine rules. 323 uses. */
	Hairline,

	/** 1px — chrome. 404 uses. The default. */
	Chrome,

	/** 2px — item tiles and emphasis. 177 uses. */
	Emphasis
};

namespace BRUI
{
	/** COMPONENT-SPECS §8: the five measured panel-ground alphas. Appearance, not gameplay. */
	inline constexpr float PanelGroundAlpha20 = 0.2f;
	inline constexpr float PanelGroundAlpha40 = 0.4f;
	inline constexpr float PanelGroundAlpha50 = 0.5f;
	inline constexpr float PanelGroundAlpha60 = 0.6f;
	inline constexpr float PanelGroundAlpha80 = 0.8f;

	/** COMPONENT-SPECS §2: the idle bottom line and the side ticks sit at 0.3. */
	inline constexpr float DimStrokeAlpha = 0.3f;

	/** COMPONENT-SPECS §0: the three legal stroke weights, in design pixels at the 1280 base. */
	inline constexpr float HairlineWeightPx = 0.5f;
	inline constexpr float ChromeWeightPx = 1.0f;
	inline constexpr float EmphasisWeightPx = 2.0f;

	inline FLinearColor ResolveColorToken(EBRUIColorToken Token)
	{
		// TODO(Styles lane): forward to the UBRUISettings palette once it exists.
		switch (Token)
		{
		case EBRUIColorToken::ChromeStroke:
		case EBRUIColorToken::TextPrimary:
		case EBRUIColorToken::SurfaceInverted:
			return FLinearColor::White;

		case EBRUIColorToken::ChromeStrokeDim:
			return FLinearColor::White.CopyWithNewOpacity(DimStrokeAlpha);

		case EBRUIColorToken::TextInverted:
			return FLinearColor::Black;

		case EBRUIColorToken::PanelGround20:
			return FLinearColor::Black.CopyWithNewOpacity(PanelGroundAlpha20);

		case EBRUIColorToken::PanelGround40:
			return FLinearColor::Black.CopyWithNewOpacity(PanelGroundAlpha40);

		case EBRUIColorToken::PanelGround50:
			return FLinearColor::Black.CopyWithNewOpacity(PanelGroundAlpha50);

		case EBRUIColorToken::PanelGround60:
			return FLinearColor::Black.CopyWithNewOpacity(PanelGroundAlpha60);

		case EBRUIColorToken::PanelGround80:
			return FLinearColor::Black.CopyWithNewOpacity(PanelGroundAlpha80);

		case EBRUIColorToken::None:
		default:
			return FLinearColor::Transparent;
		}
	}

	inline constexpr float ResolveStrokeWeightPx(EBRStrokeWeight Weight)
	{
		switch (Weight)
		{
		case EBRStrokeWeight::Hairline:
			return HairlineWeightPx;

		case EBRStrokeWeight::Emphasis:
			return EmphasisWeightPx;

		case EBRStrokeWeight::Chrome:
		default:
			return ChromeWeightPx;
		}
	}
}
