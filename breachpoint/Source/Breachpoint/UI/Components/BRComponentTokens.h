#pragma once

#include "CoreMinimal.h"
#include "UI/Styles/BRUITokens.h"

#include "BRComponentTokens.generated.h"

/**
 * Appearance tokens for the front-end components.
 *
 * SPLIT OF OWNERSHIP -- READ THIS BEFORE ADDING A COLOUR
 * -----------------------------------------------------
 * This file owns the NAMES: the Blueprint-facing `EBRUIColorToken` enum and its resolver.
 * `UI/Styles/BRUITokens.h` owns the VALUES: every hex literal in the UI module lives there,
 * behind `BR::Tokens::Hex()`, which gamma-decodes sRGB with `FLinearColor::FromSRGBColor`.
 *
 * There are no hex literals in this file and there must never be any. Two lanes noted the
 * split and asked for one owner per value; Styles won because it already held the VISR
 * channels and the decode helper, and because the dependency only points one way -- several
 * `UI/Components/` headers already include `BRUITokens.h`, nothing in Styles includes this.
 * The enum stays here because it is the UHT-reflected type the components' details panels
 * and the widget Blueprints bind to, and dragging UHT into the Styles namespace header would
 * buy nothing.
 *
 * White and black are still resolved as `FLinearColor::White` / `::Black` at a named alpha
 * rather than through `Hex()`: they are not colours anyone re-themes, and routing them
 * through a table lookup would only be slower.
 *
 * DEPENDENCY ON `UBRUISettings`: when the palette lands there, `BR::Tokens` becomes thin
 * forwarders and this resolver does not change at all.
 *
 * Values below are from Tools/gen_ui/figma_tokens.json, read live from the Figma variables.
 * It SUPERSEDES the hand-copied hex in docs/UI-DESIGN-SYSTEM.md Sec 2. Consume the SEMANTIC
 * name; never reach past it to a primitive.
 */
UENUM(BlueprintType)
enum class EBRUIColorToken : uint8
{
	/** Draws nothing. Used for "this edge is off" and "this plate has no fill". */
	None,

	/** COMPONENT-SPECS Sec 0: 1px white chrome at full opacity. The default stroke, 404 uses. */
	ChromeStroke,

	/** COMPONENT-SPECS Sec 2: the dimmed stroke -- idle bottom line and the left/right ticks. */
	ChromeStrokeDim,

	/** Body and label text on an un-inverted surface. */
	TextPrimary,

	/** COMPONENT-SPECS Sec 1: text on an INVERTED (solid white) surface. */
	TextInverted,

	/** COMPONENT-SPECS Sec 1: the inversion fill itself -- hover turns the plate solid white. */
	SurfaceInverted,

	/** COMPONENT-SPECS Sec 8: panel grounds are five discrete black alphas, not arbitrary. */
	PanelGround20,

	PanelGround40,

	PanelGround50,

	PanelGround60,

	PanelGround80,

	/**
	 * semantic `border/panel` -> white/20. The soft panel edge.
	 * `UBRRosterPanel::StrokeOpacity` fakes this today with `SetRenderOpacity` on a full-white
	 * brush, which dims the whole widget rather than the stroke -- use this instead.
	 */
	White20,

	/**
	 * white/50. Serves semantic `text/disabled` AND the white plates drawn at half strength
	 * (`UBRRosterRow::RankFrameFillOpacity`, `UBRRosterPanel::GradientOpacity`). Named by its
	 * alpha, like the PanelGround family above, because Figma aliases white/50 to a text role
	 * only and a rank-frame plate is not text -- inventing a semantic name would be a fork.
	 */
	White50,

	// -- Rarity ramp (semantic `rarity/*`). DECORATIVE, and FORBIDDEN on the HUD: the front
	// end is flat and near-monochrome, the HUD carries VISR semantics. The `-tint` of each
	// pair is the lighter partner used for gradients, glows and the tag fill.

	/** rarity/common -> white/100. */
	RarityCommon,

	RarityCommonTint,

	RarityRare,

	RarityRareTint,

	RarityEpic,

	RarityEpicTint,

	RarityLegendary,

	RarityLegendaryTint,

	// -- Accents (semantic `accent/*`). One meaning each. ---------------------------------

	/** accent/event -> accent/orange. Limited-time framing. Not a warning. */
	AccentEvent,

	/** accent/premium. Currency, battle pass, store emphasis. */
	AccentPremium,

	/** accent/highlight -> accent/cyan. Front-end selection. NOT the HUD self channel. */
	AccentHighlight,

	/** accent/danger. Destructive confirms and errors. NOT the HUD threat channel. */
	AccentDanger,

	// -- Non-black surfaces (semantic `surface/*`). ---------------------------------------

	/** surface/placeholder. The grey plate standing in for art that has not resolved. */
	SurfacePlaceholder,

	/** surface/menu-ground. A NON-BLACK menu ground; recorded in no design doc before now. */
	SurfaceMenuGround,

	/** hud/dead -> visr/dead. Eliminated / unavailable. Absent from UI-DESIGN-SYSTEM.md Sec 2. */
	Dead
};

/**
 * The legal stroke weights. The enum exists so a fifth cannot be typed into a details panel.
 *
 * COMPONENT-SPECS Sec 0 records only THREE weights and is the STALE HALF. The live Figma
 * variable read (Tools/gen_ui/figma_tokens.json, `layout.stroke` and
 * `_findings.stroke_focus_3`) has four: 0.5 / 1 / 2 / 3. Two lanes flagged the 3px nav-tab
 * active border as a conflict and one substituted 2px -- the variable settles it, 3 is
 * authored and intended. Correct the components, not the token.
 */
UENUM(BlueprintType)
enum class EBRStrokeWeight : uint8
{
	/** 0.5px -- fine rules. 323 uses. */
	Hairline,

	/** 1px -- chrome. 404 uses. The default. */
	Chrome,

	/** 2px -- item tiles and emphasis. 177 uses. */
	Emphasis,

	/** 3px -- `stroke/focus`: the nav-tab active border, drawn OUTSIDE the component. */
	Focus
};

namespace BRUI
{
	/** COMPONENT-SPECS Sec 8: the five measured panel-ground alphas. Appearance, not gameplay. */
	inline constexpr float PanelGroundAlpha20 = 0.2f;
	inline constexpr float PanelGroundAlpha40 = 0.4f;
	inline constexpr float PanelGroundAlpha50 = 0.5f;
	inline constexpr float PanelGroundAlpha60 = 0.6f;
	inline constexpr float PanelGroundAlpha80 = 0.8f;

	/** COMPONENT-SPECS Sec 2: the idle bottom line and the side ticks sit at 0.3. */
	inline constexpr float DimStrokeAlpha = 0.3f;

	/**
	 * The four legal stroke weights, in design pixels at the 1280 base. Forwarders: the
	 * numbers are owned by `BR::Tokens`, same one-owner rule as the colours. They were
	 * duplicated in both files; this is the deduplication, and no caller changes.
	 */
	inline constexpr float HairlineWeightPx = BR::Tokens::StrokeHair;
	inline constexpr float ChromeWeightPx = BR::Tokens::StrokeThin;
	inline constexpr float EmphasisWeightPx = BR::Tokens::StrokeHeavy;
	inline constexpr float FocusWeightPx = BR::Tokens::StrokeFocus;

	/**
	 * The em dash shown where a value is not known yet. Empty is never blank: a blank field
	 * next to a populated one reads as "this has no name", the dash reads as "not known yet",
	 * which is the true statement while replicated state is still in flight.
	 *
	 * Written as a universal-character escape so every consumer stays pure ASCII. This is the
	 * single copy -- see the header note in the report for the five classes still forking it.
	 */
	inline FText UnknownValueText()
	{
		return NSLOCTEXT("BRUI", "UnknownValue", "\u2014");
	}

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

		case EBRUIColorToken::White20:
			return FLinearColor::White.CopyWithNewOpacity(PanelGroundAlpha20);

		case EBRUIColorToken::White50:
			return FLinearColor::White.CopyWithNewOpacity(PanelGroundAlpha50);

		// Everything below is a NAMED value owned by BR::Tokens. No hex here, ever.
		case EBRUIColorToken::RarityCommon:
			return BR::Tokens::RarityCommon();

		case EBRUIColorToken::RarityCommonTint:
			return BR::Tokens::RarityCommonTint();

		case EBRUIColorToken::RarityRare:
			return BR::Tokens::RarityRare();

		case EBRUIColorToken::RarityRareTint:
			return BR::Tokens::RarityRareTint();

		case EBRUIColorToken::RarityEpic:
			return BR::Tokens::RarityEpic();

		case EBRUIColorToken::RarityEpicTint:
			return BR::Tokens::RarityEpicTint();

		case EBRUIColorToken::RarityLegendary:
			return BR::Tokens::RarityLegendary();

		case EBRUIColorToken::RarityLegendaryTint:
			return BR::Tokens::RarityLegendaryTint();

		case EBRUIColorToken::AccentEvent:
			return BR::Tokens::AccentEvent();

		case EBRUIColorToken::AccentPremium:
			return BR::Tokens::AccentPremium();

		case EBRUIColorToken::AccentHighlight:
			return BR::Tokens::AccentHighlight();

		case EBRUIColorToken::AccentDanger:
			return BR::Tokens::AccentDanger();

		case EBRUIColorToken::SurfacePlaceholder:
			return BR::Tokens::SurfacePlaceholder();

		case EBRUIColorToken::SurfaceMenuGround:
			return BR::Tokens::SurfaceMenuGround();

		case EBRUIColorToken::Dead:
			return BR::Tokens::Dead();

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

		case EBRStrokeWeight::Focus:
			return FocusWeightPx;

		case EBRStrokeWeight::Chrome:
		default:
			return ChromeWeightPx;
		}
	}
}
