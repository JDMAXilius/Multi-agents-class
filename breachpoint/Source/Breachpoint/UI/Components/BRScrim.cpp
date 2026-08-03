#include "UI/Components/BRScrim.h"

UBRScrim::UBRScrim()
{
	HairlineStyle.Edges = static_cast<int32>(EBRBorderEdge::None);
	HairlineStyle.DimmedEdges = static_cast<int32>(EBRBorderEdge::None);
	HairlineStyle.SideTickLength = 0.0f;

	// COMPONENT-SPECS Sec 8: the panel grounds are five discrete black alphas. 0.6 is the modal
	// step. UNMEASURED: no scrim node was read off the reference file (the modal frames in
	// COMPONENT-SPECS Sec 6 record the panels, not the plate behind them) -- filed as a gap, and
	// the token is editable on the WBP instance if the Styles lane measures a different step.
	HairlineStyle.FillToken = EBRUIColorToken::PanelGround60;

	// SCREEN-MANIFEST Sec 7.1: slot [1] of the screen root, collapsed by default.
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBRScrim::SetScrimActive(bool bActive)
{
	SetVisibility(bActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool UBRScrim::IsScrimActive() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}
