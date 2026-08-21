#include "UI/BNScreen_Scoreboard.h"
#include "BreachpointNext.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "INotifyFieldValueChanged.h"
#include "UI/BNScoreRow.h"
#include "UI/BNUITypes.h"
#include "UI/BNViewModels.h"

UBNScreen_Scoreboard::UBNScreen_Scoreboard()
{
	// EXPLICIT Game, not Inherit (critic): topping the Game stack deactivates the HUD beneath,
	// and with no active widget desiring a config the router would be left holding the last one
	// by luck. The compiled reference set this on both of its screens for exactly that reason.
	InputMode = EBNWidgetInputMode::Game;
}

void UBNScreen_Scoreboard::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	Rows.Reset();
	if (RowContainer)
	{
		for (int32 Index = 0; Index < RowContainer->GetChildrenCount(); ++Index)
		{
			if (UBNScoreRow* Row = Cast<UBNScoreRow>(RowContainer->GetChildAt(Index)))
			{
				Rows.Add(Row);
			}
		}
	}
	if (Rows.Num() == 0)
	{
		UE_LOG(LogBN, Warning, TEXT("BNScoreboard: the WBP placed no UBNScoreRow rows in RowContainer — the board will render nothing. TASK-R7-WBP-HUD names the tree."));
	}
}

void UBNScreen_Scoreboard::BindViewModels()
{
	UBNVM_Match* Match = GetMatchViewModel();
	if (!Match)
	{
		return;
	}
	BoundViewModel = Match;

	RosterHandle = Match->OnRosterViewChanged.AddUObject(this, &UBNScreen_Scoreboard::HandleRosterChanged);

	const UE::FieldNotification::FFieldId BannerId = UBNVM_Match::FFieldNotificationClassDescriptor::PhaseBannerText;
	if (BannerId.IsValid())
	{
		BoundFields.Emplace(BannerId, Match->AddFieldValueChangedDelegate(BannerId,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &UBNScreen_Scoreboard::HandleMatchFieldChanged)));
	}

	Refresh();
}

void UBNScreen_Scoreboard::UnbindViewModels()
{
	if (UBNVM_Match* Match = BoundViewModel.Get())
	{
		Match->OnRosterViewChanged.Remove(RosterHandle);
		for (const TPair<UE::FieldNotification::FFieldId, FDelegateHandle>& Bound : BoundFields)
		{
			if (Bound.Value.IsValid())
			{
				Match->RemoveFieldValueChangedDelegate(Bound.Key, Bound.Value);
			}
		}
	}
	RosterHandle.Reset();
	BoundFields.Reset();
	BoundViewModel.Reset();
}

void UBNScreen_Scoreboard::HandleRosterChanged()
{
	Refresh();
}

void UBNScreen_Scoreboard::HandleMatchFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

void UBNScreen_Scoreboard::Refresh()
{
	const UBNVM_Match* Match = BoundViewModel.Get();
	const TArray<FBNScoreRowView>* Roster = Match ? &Match->GetRoster() : nullptr;
	const int32 EntryCount = Roster ? Roster->Num() : 0;

	const int32 Shown = FMath::Min(EntryCount, Rows.Num());
	if (EntryCount > Rows.Num() && !bWarnedRowShortage)
	{
		bWarnedRowShortage = true;
		UE_LOG(LogBN, Log, TEXT("BNScoreboard: %d players for %d placed rows — the tail is dropped. More rows is a WBP edit."),
			EntryCount, Rows.Num());
	}

	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		UBNScoreRow* Row = Rows[RowIndex];
		if (!Row)
		{
			continue;
		}
		if (RowIndex < Shown)
		{
			Row->SetRow((*Roster)[RowIndex]);
		}
		else
		{
			Row->ClearRow();
		}
	}

	if (BannerText && Match)
	{
		const FText Banner = Match->GetPhaseBannerText();
		BannerText->SetText(Banner);
		BannerText->SetVisibility(Banner.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		BannerText->SetColorAndOpacity(FSlateColor(BNUIColors::Self));
	}
}
