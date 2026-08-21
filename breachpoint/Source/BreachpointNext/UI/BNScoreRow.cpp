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

	SetColorAndOpacity(View.bIsSelf ? BNUIColors::Self : BNUIColors::InkDim);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBNScoreRow::ClearRow()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
