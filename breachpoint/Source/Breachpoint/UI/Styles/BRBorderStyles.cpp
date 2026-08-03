#include "UI/Styles/BRBorderStyles.h"

#include "Brushes/SlateColorBrush.h"
#include "UI/Styles/BRUITokens.h"

UBRBorderStyleBase::UBRBorderStyleBase()
{
	Background = FSlateColorBrush(FLinearColor::Transparent);
}

UBRBorderStyle_Panel::UBRBorderStyle_Panel()
{
	Background = FSlateColorBrush(BR::Tokens::Deep());
}

UBRBorderStyle_Rail::UBRBorderStyle_Rail()
{
	// 0.85, not 1.0: the front end is a camera looking at BR_Arena01, and a fully opaque
	// rail turns that scene back into a black quad (ui-presentation SKILL.md Sec.1).
	Background = FSlateColorBrush(BR::Tokens::Void(0.85f));
}

UBRBorderStyle_Modal::UBRBorderStyle_Modal()
{
	Background = FSlateColorBrush(BR::Tokens::Void(0.92f));
}
