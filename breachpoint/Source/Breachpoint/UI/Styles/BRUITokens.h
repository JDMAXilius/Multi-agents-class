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
 * Hex values come from Tools/gen_ui/figma_tokens.json, which was read live from the Figma
 * variable collections and SUPERSEDES the hand-copied tables in docs/UI-DESIGN-SYSTEM.md
 * Sec.2 and docs/ui/REFERENCE-EXTRACTION.md Sec.2. They are sRGB, exactly as authored in
 * Figma, and are gamma-decoded to linear here -- see Hex().
 *
 * THIS FILE OWNS EVERY HEX VALUE IN THE UI MODULE. `UI/Components/BRComponentTokens.h`
 * owns the Blueprint-facing token ENUM and resolves it by calling into here; it contains
 * no hex and must never grow any. One colour, one literal, one file to re-theme.
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

	// -- HUD / VISR channels (figma_tokens.json `visr`). A token means ONE thing. -------

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
	/**
	 * Disabled / unavailable / eliminated. This is `visr/dead` from the Figma variables --
	 * the value was already correct here but appears in no design document; the live read
	 * (figma_tokens.json _findings.visr_dead) confirms it rather than adding it.
	 */
	FORCEINLINE FLinearColor Dead(const float A = 1.0f) { return Hex(0x4A5A6B, A); }

	// -- Front end (figma_tokens.json `semantic`). ---------------------------------------
	//
	// The Semantic layer in Figma is pure ALIASES onto Primitives, and that indirection is
	// the point: `rarity/common -> white/100` means a re-theme edits one primitive, not
	// forty call sites. So the aliases below are functions calling functions, deliberately.
	//
	// The rarity ramp is DECORATIVE and is FORBIDDEN on the HUD (figma_tokens.json
	// _hud_vs_frontend): front end is flat and near-monochrome, the HUD carries the VISR
	// channels above. If the HUD shield bar and a lobby progress bar look alike, one is wrong.

	/** rarity/common -> white/100. An alias onto SelfWhite, not a second white literal. */
	FORCEINLINE FLinearColor RarityCommon(const float A = 1.0f) { return SelfWhite(A); }
	FORCEINLINE FLinearColor RarityCommonTint(const float A = 1.0f) { return Hex(0xC4C4C4, A); }
	FORCEINLINE FLinearColor RarityRare(const float A = 1.0f) { return Hex(0x6295DA, A); }
	FORCEINLINE FLinearColor RarityRareTint(const float A = 1.0f) { return Hex(0x70CDDF, A); }
	FORCEINLINE FLinearColor RarityEpic(const float A = 1.0f) { return Hex(0xAB55FF, A); }
	FORCEINLINE FLinearColor RarityEpicTint(const float A = 1.0f) { return Hex(0x7B61FF, A); }
	FORCEINLINE FLinearColor RarityLegendary(const float A = 1.0f) { return Hex(0xE8BA3D, A); }
	FORCEINLINE FLinearColor RarityLegendaryTint(const float A = 1.0f) { return Hex(0xFFED4F, A); }

	/** accent/event -> accent/orange. Limited-time and event framing. */
	FORCEINLINE FLinearColor AccentEvent(const float A = 1.0f) { return Hex(0xFF5C00, A); }
	/** accent/premium -> accent/premium. Currency, battle pass, store emphasis. */
	FORCEINLINE FLinearColor AccentPremium(const float A = 1.0f) { return Hex(0xFFC11C, A); }
	/** accent/highlight -> accent/cyan. Front-end selection glow. NOT the HUD self channel. */
	FORCEINLINE FLinearColor AccentHighlight(const float A = 1.0f) { return Hex(0x2EC3E5, A); }
	/** accent/danger -> accent/danger. Destructive confirms and errors, never a threat readout. */
	FORCEINLINE FLinearColor AccentDanger(const float A = 1.0f) { return Hex(0xFF4B4B, A); }

	/** surface/placeholder -> grey/placeholder. The grey plate standing in for missing art. */
	FORCEINLINE FLinearColor SurfacePlaceholder(const float A = 1.0f) { return Hex(0xD9D9D9, A); }
	/** surface/menu-ground -> grey/panel. A NON-BLACK menu ground; in no design doc before now. */
	FORCEINLINE FLinearColor SurfaceMenuGround(const float A = 1.0f) { return Hex(0x36383F, A); }

	// accent/steel (#34729b) and accent/sky (#3f97ce) are primitives that NOTHING in the
	// Semantic layer aliases. They are deliberately absent here: consuming a primitive
	// directly to "use them up" is exactly the fork this file exists to prevent. Find out
	// what they were for first (figma_tokens.json _findings.accent_steel_sky).

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
	// FOUR stroke weights exist. Nothing else is legal. Corner radius is 0 EVERYWHERE:
	// every brush in this folder is an FSlateColorBrush (DrawAs = Image), which cannot
	// round a corner even by accident. A rounded panel is a defect, so the type system
	// is doing the enforcing rather than a review comment.

	inline constexpr float StrokeHair = 0.5f;
	inline constexpr float StrokeThin = 1.0f;
	inline constexpr float StrokeHeavy = 2.0f;

	/**
	 * `stroke/focus = 3`. SETTLED: the live Figma variable read (figma_tokens.json
	 * layout.stroke, _findings.stroke_focus_3) shows FOUR weights, not the three
	 * COMPONENT-SPECS Sec.0 records -- that document is the stale half. 3px is authored
	 * and intended; the earlier substitution of 2px here was wrong and is corrected.
	 */
	inline constexpr float StrokeFocus = 3.0f;

	/** Nav-tab active border, drawn OUTSIDE the component. */
	inline constexpr float StrokeNavTabActive = StrokeFocus;

	/** Inactive nav tab dims the WHOLE component, not just its text. Applied by the
	 *  widget as render opacity -- there is no CommonUI style field for it. */
	inline constexpr float OpacityNavTabInactive = 0.6f;

	/** Menu-row underline at rest — 1.0 on hover is the inversion, expressed by clearing the
	 *  dim mask (`SetEdgeDimmed`), not by a second opacity token. */
	inline constexpr float OpacityMenuRowLineIdle = 0.3f;

	/** Consumed by `BRButtonStyles.cpp` as the button styles' MinHeight. (The pitch that used
	 *  to sit beside them had zero readers and was cut — CPP-AUDIT token dedup. The audit
	 *  initially marked all three dead; the post-cut grep caught these two live.) */
	inline constexpr int32 MenuRowHeight = 28;
	inline constexpr int32 NavBarHeight = 30;
}
