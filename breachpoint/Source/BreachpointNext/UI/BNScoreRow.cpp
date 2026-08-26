#include "UI/BNScoreRow.h"
#include "Components/TextBlock.h"
#include "UI/BNUITypes.h"

void UBNScoreRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBNScoreRow::SetRow(const FBNScoreRowView& View)
{
	if (NameText)
	{
		NameText->SetText(FText::FromString(View.PlayerName));
		// The winner's NAME is cyan even in someone else's row list; the row tint below still
		// says whose screen this is. Two channels, two meanings, no collision.
		NameText->SetColorAndOpacity(FSlateColor(View.bIsWinner ? BNUIColors::Shield : FLinearColor::White));
	}
	if (KillsText) { KillsText->SetText(FText::AsNumber(View.Kills)); }
	if (DeathsText) { DeathsText->SetText(FText::AsNumber(View.Deaths)); }

	// TEAMS (BN16): the relation tints through the SAME whole-row channel today's tint uses —
	// one mapping, no second color path. bIsSelf keeps its authority (your row is white even in
	// a team list; a row that is both Self-related and bIsSelf is the same row twice), and None
	// falls through to today's ink untouched — which IS the teams-OFF proof: in FFA every
	// relation is None and this line computes exactly what it computed before this switch existed.
	FLinearColor RowTint = View.bIsSelf ? BNUIColors::Self : BNUIColors::InkDim;
	if (!View.bIsSelf)
	{
		switch (View.Relation)
		{
		case EBNUITeamRelation::Ally:  RowTint = BNUIColors::Ally;   break;
		case EBNUITeamRelation::Enemy: RowTint = BNUIColors::Threat; break;
		default:                       break; // None (and a Self the director forgot to pair with bIsSelf): today's ink, honestly
		}
	}
	SetColorAndOpacity(RowTint);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBNScoreRow::ClearRow()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
