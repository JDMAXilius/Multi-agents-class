#include "UI/Styles/BRButtonStyles.h"

#include "Brushes/SlateColorBrush.h"
#include "UI/Styles/BRTextStyles.h"
#include "UI/Styles/BRUITokens.h"

namespace
{
	/** Every fill in the front end is a flat, sharp-cornered tint. No exceptions. */
	FSlateBrush Fill(const FLinearColor& Colour)
	{
		return FSlateColorBrush(Colour);
	}

	FSlateBrush NoFill()
	{
		return FSlateColorBrush(FLinearColor::Transparent);
	}
}

// -- UBRButtonStyleBase ----------------------------------------------------------------

UBRButtonStyleBase::UBRButtonStyleBase()
{
	bSingleMaterial = false;

	// Sane, invisible defaults. Concrete styles override what they care about; anything
	// left alone is a transparent rect rather than the engine's default white block.
	NormalBase = NoFill();
	NormalHovered = NoFill();
	NormalPressed = NoFill();
	SelectedBase = NoFill();
	SelectedHovered = NoFill();
	SelectedPressed = NoFill();
	Disabled = NoFill();

	ButtonPadding = FMargin(0.0f);
	CustomPadding = FMargin(0.0f);
	MinWidth = 0;
	MinHeight = 0;
	MaxWidth = 0;
	MaxHeight = 0;
}

// -- UBRButtonStyle_MenuRow ------------------------------------------------------------

UBRButtonStyle_MenuRow::UBRButtonStyle_MenuRow()
{
	// The inversion. Idle: nothing but type and a dim underline. Hover/selected: solid
	// white plate, black type. Pressed is the same plate -- the flip already reads as a
	// press; dimming it a second time just makes the row look broken.
	NormalBase = NoFill();
	NormalHovered = Fill(BR::Tokens::SelfWhite());
	NormalPressed = Fill(BR::Tokens::SelfWhite());
	SelectedBase = Fill(BR::Tokens::SelfWhite());
	SelectedHovered = Fill(BR::Tokens::SelfWhite());
	SelectedPressed = Fill(BR::Tokens::SelfWhite());
	Disabled = NoFill();

	NormalTextStyle = UBRTextStyle_MenuRow::StaticClass();
	NormalHoveredTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	SelectedTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	SelectedHoveredTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	DisabledTextStyle = UBRTextStyle_Caption::StaticClass();

	// Measured: row height 28, pitch 40 (REFERENCE-EXTRACTION.md Sec.3). The 12px of
	// pitch that is not row is spacing owned by the list, not padding owned by the row.
	MinHeight = BR::Tokens::MenuRowHeight;
	ButtonPadding = FMargin(16.0f, 0.0f, 16.0f, 0.0f);
}

// -- UBRButtonStyle_NavTab -------------------------------------------------------------

UBRButtonStyle_NavTab::UBRButtonStyle_NavTab()
{
	// Tabs do not take a plate. Hover is a whisper of Edge; the active state is the
	// outside border the widget draws at BR::Tokens::StrokeNavTabActive, and the
	// inactive state is the whole component at BR::Tokens::OpacityNavTabInactive.
	NormalBase = NoFill();
	NormalHovered = Fill(BR::Tokens::Edge(0.5f));
	NormalPressed = Fill(BR::Tokens::Edge());
	SelectedBase = NoFill();
	SelectedHovered = Fill(BR::Tokens::Edge(0.5f));
	SelectedPressed = Fill(BR::Tokens::Edge());
	Disabled = NoFill();

	NormalTextStyle = UBRTextStyle_Tab::StaticClass();
	NormalHoveredTextStyle = UBRTextStyle_Tab::StaticClass();
	SelectedTextStyle = UBRTextStyle_Tab::StaticClass();
	SelectedHoveredTextStyle = UBRTextStyle_Tab::StaticClass();
	DisabledTextStyle = UBRTextStyle_Caption::StaticClass();

	MinHeight = BR::Tokens::NavBarHeight;
	ButtonPadding = FMargin(12.0f, 0.0f, 12.0f, 0.0f);
}

// -- UBRButtonStyle_Highlight ----------------------------------------------------------

UBRButtonStyle_Highlight::UBRButtonStyle_Highlight()
{
	NormalBase = Fill(BR::Tokens::Amber());
	NormalHovered = Fill(BR::Tokens::SelfWhite());
	NormalPressed = Fill(BR::Tokens::SelfWhite());
	SelectedBase = Fill(BR::Tokens::SelfWhite());
	SelectedHovered = Fill(BR::Tokens::SelfWhite());
	SelectedPressed = Fill(BR::Tokens::SelfWhite());
	Disabled = Fill(BR::Tokens::Dead(0.35f));

	// Amber and white plates both take black ink -- neither is legible under white type.
	NormalTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	NormalHoveredTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	SelectedTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	SelectedHoveredTextStyle = UBRTextStyle_MenuRowInverted::StaticClass();
	DisabledTextStyle = UBRTextStyle_Caption::StaticClass();

	MinHeight = BR::Tokens::MenuRowHeight;
	ButtonPadding = FMargin(20.0f, 0.0f, 20.0f, 0.0f);
}
