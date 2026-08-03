#pragma once

#include "CommonButtonBase.h"

#include "BRButtonStyles.generated.h"

/**
 * Base for every BREACHPOINT button style. C++, not a BP style asset, for the same
 * reason as UBRTextStyleBase: a BP child of an ENGINE class fails R26 conditions 1 and 5
 * and is therefore banned by R18. NotBlueprintable makes it structural.
 *
 * No asset references live in here at all. Every brush is an FSlateColorBrush -- a solid
 * tint with DrawAs = Image, no texture, no material, no ResourceObject. That is a
 * deliberate double win: nothing to load (so none of UBRTextStyleBase's deferred-resolve
 * machinery is needed), and DrawAs = Image cannot round a corner, so "corner radius 0
 * everywhere" is enforced by the type rather than by a review comment. If a style ever
 * needs ESlateBrushDrawType::RoundedBox, that is a design change, not an implementation
 * detail.
 *
 * Stroke thickness and whole-component opacity are NOT expressible on UCommonButtonStyle
 * -- it has brushes, padding and size clamps, nothing else. They live as
 * BR::Tokens::Stroke* / Opacity* constants for the widget layer to read.
 */
UCLASS(Abstract, NotBlueprintable)
class BREACHPOINT_API UBRButtonStyleBase : public UCommonButtonStyle
{
	GENERATED_BODY()

public:
	UBRButtonStyleBase();
};

/**
 * The left-rail menu row, and the signature interaction of the whole front end: the
 * idle -> hover INVERSION. At rest the row is transparent with a dim underline; on hover
 * or selection the fill goes solid white and the text flips to black.
 *
 * The underline itself (StrokeThin at OpacityMenuRowLineIdle -> OpacityMenuRowLineActive)
 * is a widget-owned element -- UCommonButtonStyle has no line field. Tokens carry it.
 */
UCLASS()
class BREACHPOINT_API UBRButtonStyle_MenuRow : public UBRButtonStyleBase
{
	GENERATED_BODY()

public:
	UBRButtonStyle_MenuRow();
};

/**
 * Top nav tab. Active state is a StrokeNavTabActive border drawn OUTSIDE the component;
 * inactive dims the WHOLE component to OpacityNavTabInactive -- not just the label.
 * Both are widget-layer concerns (see tokens); this style carries the fills and type.
 */
UCLASS()
class BREACHPOINT_API UBRButtonStyle_NavTab : public UBRButtonStyleBase
{
	GENERATED_BODY()

public:
	UBRButtonStyle_NavTab();
};

/** The one-per-screen affirmative action. Amber = a clock is running / act now. */
UCLASS()
class BREACHPOINT_API UBRButtonStyle_Highlight : public UBRButtonStyleBase
{
	GENERATED_BODY()

public:
	UBRButtonStyle_Highlight();
};
