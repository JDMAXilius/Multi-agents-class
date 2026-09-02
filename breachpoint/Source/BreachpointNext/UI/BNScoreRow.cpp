#include "UI/BNScoreRow.h"
#include "Components/Image.h"
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
	if (ScoreText) { ScoreText->SetText(FText::AsNumber(View.Score)); }
	if (AssistsText)
	{
		// Honest-unknown: assists are not tracked yet, and a 0 would be a lie dressed as data.
		AssistsText->SetText(View.Assists < 0 ? FText::FromString(TEXT("\u2014")) : FText::AsNumber(View.Assists));
	}
	if (TagText)
	{
		TagText->SetText(View.ServiceTag.IsEmpty() ? FText::GetEmpty()
			: FText::FromString(FString::Printf(TEXT("[%s]"), *View.ServiceTag)));
		TagText->SetVisibility(View.ServiceTag.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	// `43:39/57`: the local row reads as a highlighted plate + accent. Colours are C++'s alone.
	const ESlateVisibility HighlightVis = View.bIsSelf ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (HighlightFill)
	{
		HighlightFill->SetColorAndOpacity(FLinearColor(BNUIColors::Self.R, BNUIColors::Self.G, BNUIColors::Self.B, 0.18f));
		HighlightFill->SetVisibility(HighlightVis);
	}
	if (HighlightAccent)
	{
		HighlightAccent->SetColorAndOpacity(BNUIColors::Shield);
		HighlightAccent->SetVisibility(HighlightVis);
	}

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
