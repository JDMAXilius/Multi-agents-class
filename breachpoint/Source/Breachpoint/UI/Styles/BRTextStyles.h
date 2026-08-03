#pragma once

#include "CommonTextBlock.h"

#include "BRTextStyles.generated.h"

/**
 * Base for every BREACHPOINT text style.
 *
 * WHY C++ AND NOT A BLUEPRINT STYLE ASSET
 * A BP style asset would be parented to an ENGINE class (UCommonTextStyle), which fails
 * R26 conditions 1 and 5 -- R26 only permits a direct BP child of a BR-prefixed C++ class
 * named BP_<CppClassWithoutPrefix>. It is therefore forbidden by R18, not merely
 * unaddressed. NotBlueprintable below makes that structural: UCommonTextStyle is marked
 * Blueprintable by the engine, so without this specifier the illegal asset stays one
 * right-click away.
 *
 * HOW THE FONT GETS IN (the whole reason this class has any code in it)
 * UCommonTextBlock::UpdateFromStyle() reads TextStyle->Font DIRECTLY off the style CDO
 * (CommonTextBlock.cpp:492-514, `SetFont(TextStyle->Font)` at :496). It is non-virtual
 * (CommonTextBlock.h:223), as is GetStyleCDO() (:224). ToTextBlockStyle/ApplyToTextBlock
 * ARE virtual but are not on that path -- UpdateFromStyle never calls them. So there is
 * no behavioural hook: the ONLY way to influence a UCommonTextBlock through a style is to
 * have real data sitting in the CDO's Font member by the time it is read.
 *
 * FSlateFontInfo::FontObject is a hard TObjectPtr, and both routes for filling it at
 * construction are closed: ConstructorHelpers is banned (and hook-blocked), and
 * UCommonTextStyle::Font carries no Config specifier so no ini line can reach it.
 *
 * The route that works: a NEW property we own -- FontAsset -- which CAN be Config, plus a
 * DEFERRED resolve. It must be deferred: LoadConfig runs before PostInitProperties
 * (UObjectGlobals.cpp:4274 vs :4320, so the ini value IS present), but a native CDO is
 * constructed during module load, and a synchronous package load there is the classic
 * early-startup hang. We do not need it that early anyway -- UpdateFromStyle is also
 * called from UCommonTextBlock::SynchronizeProperties() (CommonTextBlock.cpp:427), i.e.
 * at widget construction, long after the engine is up. So we resolve on
 * FCoreDelegates::GetOnPostEngineInit(), which is safely before any widget exists.
 */
UCLASS(Abstract, NotBlueprintable, Config = Game)
class BREACHPOINT_API UBRTextStyleBase : public UCommonTextStyle
{
	GENERATED_BODY()

public:
	UBRTextStyleBase();

	/**
	 * Soft path to the font asset for this style. Config so a font swap is one ini line
	 * in [/Script/Breachpoint.BRTextStyle_<Name>] and never a recompile.
	 *
	 * Typed TSoftObjectPtr<UObject> rather than <UFont> on purpose: FSlateFontInfo's own
	 * FontObject is a `const UObject*` accepting UFont AND UFontFace, and we should not
	 * be narrower than the field we assign into.
	 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Font")
	TSoftObjectPtr<UObject> FontAsset;

	/**
	 * Loads FontAsset into Font.FontObject. Idempotent and cheap after the first call.
	 * Public so a widget can force it if it ever constructs before engine-init completes
	 * (it should not, but the honest failure mode is a fallback font, not a crash).
	 */
	void ResolveFont();

	//~ UObject
	virtual void PostInitProperties() override;
	//~ End UObject
};

/**
 * THE RETENTION RULE (CPP-AUDIT, styles verdict): a text style class exists ONLY to fill a
 * `UCommonButtonStyle` `TSubclassOf<UCommonTextStyle>` slot — the four below are exactly the
 * set `BRButtonStyles.cpp` assigns. Every other piece of type data is `figma_tokens.json`,
 * which the WBP generator writes directly onto each CommonTextBlock. A style class that
 * duplicates a token row is a SECOND source of the same truth, and the three that existed
 * (`_Body`, `_Numeral`, `_Flavor`) had already drifted from the tokens they shadowed before
 * anything ever referenced them. Do not add a fifth without a button-style slot demanding it.
 */

/** Nav tabs. Rajdhani SemiBold 14, letter-spacing 150 (15%). */
UCLASS()
class BREACHPOINT_API UBRTextStyle_Tab : public UBRTextStyleBase
{
	GENERATED_BODY()

public:
	UBRTextStyle_Tab();
};

/** Menu rows and buttons at rest. Rajdhani SemiBold 16, letter-spacing 100 (10%). */
UCLASS()
class BREACHPOINT_API UBRTextStyle_MenuRow : public UBRTextStyleBase
{
	GENERATED_BODY()

public:
	UBRTextStyle_MenuRow();
};

/**
 * Menu row on the inverted side of the hover flip: same metrics as _MenuRow, black ink
 * on the solid white fill. Not in the packet's list -- UBRButtonStyle_MenuRow needs a
 * TSubclassOf<UCommonTextStyle> for NormalHoveredTextStyle and the inversion is not
 * expressible any other way. Flagged in the report.
 */
UCLASS()
class BREACHPOINT_API UBRTextStyle_MenuRowInverted : public UBRTextStyleBase
{
	GENERATED_BODY()

public:
	UBRTextStyle_MenuRowInverted();
};

/** The DisabledTextStyle of all three button styles. Rajdhani SemiBold 11, InkDim. */
UCLASS()
class BREACHPOINT_API UBRTextStyle_Caption : public UBRTextStyleBase
{
	GENERATED_BODY()

public:
	UBRTextStyle_Caption();
};
