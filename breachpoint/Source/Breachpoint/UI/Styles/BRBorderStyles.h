#pragma once

#include "CommonBorder.h"

#include "BRBorderStyles.generated.h"

/**
 * Base for every BREACHPOINT border style. C++, not a BP style asset -- same R18/R26
 * reasoning as UBRTextStyleBase; NotBlueprintable makes it structural.
 *
 * UCommonBorderStyle carries exactly one field, Background (CommonBorder.h:26), so these
 * classes are thin by nature. Every background is an FSlateColorBrush: flat tint, no
 * asset reference, and DrawAs = Image so a corner physically cannot be rounded.
 *
 * Hairline rules between panels are Edge-tinted borders at BR::Tokens::StrokeHair /
 * StrokeThin, drawn by the widget -- a border style has no thickness field.
 */
UCLASS(Abstract, NotBlueprintable)
class BREACHPOINT_API UBRBorderStyleBase : public UCommonBorderStyle
{
	GENERATED_BODY()

public:
	UBRBorderStyleBase();
};

/** Content panel ground. Deep -- one step up off the Void. */
UCLASS()
class BREACHPOINT_API UBRBorderStyle_Panel : public UBRBorderStyleBase
{
	GENERATED_BODY()

public:
	UBRBorderStyle_Panel();
};

/** The left rail. Sits ON the ground so the 3D scene reads through the composition. */
UCLASS()
class BREACHPOINT_API UBRBorderStyle_Rail : public UBRBorderStyleBase
{
	GENERATED_BODY()

public:
	UBRBorderStyle_Rail();
};

/** Modal scrim. Void at 0.92 -- the scene stays faintly present behind the dialog. */
UCLASS()
class BREACHPOINT_API UBRBorderStyle_Modal : public UBRBorderStyleBase
{
	GENERATED_BODY()

public:
	UBRBorderStyle_Modal();
};
