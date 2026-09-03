#include "UI/BNScoreRow.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
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
		NameText->SetColorAndOpacity(FSlateColor(View.bIsWinner ? BNUIColors::Shield : FLinearColor::White));
	}
	if (KillsText) { KillsText->SetText(FText::AsNumber(View.Kills)); }
	if (DeathsText) { DeathsText->SetText(FText::AsNumber(View.Deaths)); }
	if (ScoreText) { ScoreText->SetText(FText::AsNumber(View.Score)); }
	if (AssistsText)
	{
		// Honest-unknown: assists are not tracked yet, and a 0 would be a lie dressed as data.
		AssistsText->SetText(View.Assists < 0 ? FText::FromString(TEXT("—")) : FText::AsNumber(View.Assists));
	}
	if (KdaText)
	{
		// Halo's KDA: kills + assists/3 - deaths. With assists unknown this is K - D, one decimal.
		const float Kda = View.Kills + (View.Assists < 0 ? 0.f : View.Assists / 3.f) - View.Deaths;
		KdaText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Kda)));
	}
	if (TagText)
	{
		TagText->SetText(View.ServiceTag.IsEmpty() ? FText::GetEmpty()
			: FText::FromString(FString::Printf(TEXT("[%s]"), *View.ServiceTag)));
		TagText->SetVisibility(View.ServiceTag.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		TagText->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim));
	}

	// The capture's rows: a team-coloured plate under white type, four lighter value cells, and
	// the local row lifted (lighter plate, 2px white bar and a caret at its left edge).
	FLinearColor Team = FLinearColor(1.f, 1.f, 1.f, 0.12f);
	switch (View.Relation)
	{
	case EBNUITeamRelation::Ally:  Team = BNUIColors::Ally;   break;
	case EBNUITeamRelation::Enemy: Team = BNUIColors::Threat; break;
	default: break;
	}
	const bool bTeam = View.Relation == EBNUITeamRelation::Ally || View.Relation == EBNUITeamRelation::Enemy;
	auto Tint = [this](const TCHAR* Name, const FLinearColor& Colour, ESlateVisibility Vis)
	{
		if (UImage* Img = Cast<UImage>(GetWidgetFromName(Name)))
		{
			Img->SetColorAndOpacity(Colour);
			Img->SetVisibility(Vis);
		}
	};
	const float PlateA = View.bIsSelf ? 0.85f : (bTeam ? 0.55f : 0.12f);
	const float CellA  = View.bIsSelf ? 0.95f : (bTeam ? 0.70f : 0.18f);
	Tint(TEXT("RowFill"), FLinearColor(Team.R, Team.G, Team.B, PlateA), ESlateVisibility::HitTestInvisible);
	for (const TCHAR* Cell : { TEXT("CellTint0"), TEXT("CellTint1"), TEXT("CellTint2"), TEXT("CellTint3") })
	{
		Tint(Cell, FLinearColor(Team.R, Team.G, Team.B, CellA), ESlateVisibility::HitTestInvisible);
	}
	const ESlateVisibility SelfVis = View.bIsSelf ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	Tint(TEXT("SelfBar"), BNUIColors::Self, SelfVis);
	Tint(TEXT("SelfCaret"), BNUIColors::Self, SelfVis);

	// Type is white on every plate now; the old whole-row tint would dim the numbers too.
	SetColorAndOpacity(FLinearColor::White);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBNScoreRow::SetEmblem(const TSoftObjectPtr<UTexture2D>& InEmblem)
{
	if (!Emblem)
	{
		return;
	}
	UTexture2D* Tex = InEmblem.IsNull() ? nullptr : InEmblem.LoadSynchronous();
	if (Tex)
	{
		Emblem->SetBrushFromTexture(Tex, /*bMatchSize*/ false);
	}
	Emblem->SetVisibility(Tex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UBNScoreRow::ClearRow()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
