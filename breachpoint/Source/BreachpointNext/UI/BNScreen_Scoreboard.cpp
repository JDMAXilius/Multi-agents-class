#include "UI/BNScreen_Scoreboard.h"
#include "Components/Image.h"
#include "BreachpointNext.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "INotifyFieldValueChanged.h"
#include "UI/BNScoreRow.h"
#include "UI/BNUITypes.h"
#include "UI/BNViewModels.h"

#define LOCTEXT_NAMESPACE "BreachpointNextUI"

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

	// Banner AND outcome. They are set in the same SetMatchPhase call, so subscribing to the
	// banner alone would USUALLY work — but "usually" is how a screen ends up showing DEFEAT to
	// the winner when a future edit moves one of them. Bind what you read — which now includes
	// the team ledger (BN16): mode and both relative scores, same reasoning, same one handler.
	for (const UE::FieldNotification::FFieldId FieldId : {
		UBNVM_Match::FFieldNotificationClassDescriptor::PhaseBannerText,
		UBNVM_Match::FFieldNotificationClassDescriptor::Outcome,
		UBNVM_Match::FFieldNotificationClassDescriptor::bTeamsMode,
		UBNVM_Match::FFieldNotificationClassDescriptor::MyTeamScore,
		UBNVM_Match::FFieldNotificationClassDescriptor::EnemyTeamScore })
	{
		if (FieldId.IsValid())
		{
			BoundFields.Emplace(FieldId, Match->AddFieldValueChangedDelegate(FieldId,
				INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &UBNScreen_Scoreboard::HandleMatchFieldChanged)));
		}
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

int32 UBNScreen_Scoreboard::EnsureRowCapacity(int32 Needed)
{
	const int32 Capped = FMath::Min(Needed, MaxScoreRows);
	if (!RowContainer || Rows.Num() >= Capped)
	{
		return Rows.Num();
	}
	// The class comes off a row the WBP ALREADY PLACED. That is deliberate: no hard class
	// reference in C++ and no config the designer has to remember to fill (law 3) — clones
	// are by construction the same widget the board was authored with.
	UBNScoreRow* Seed = Rows.Num() > 0 ? Rows[0].Get() : nullptr;
	if (!Seed)
	{
		// Nothing placed means nothing to clone from; NativeOnInitialized already warned.
		return Rows.Num();
	}
	const TSubclassOf<UBNScoreRow> RowClass = Seed->GetClass();
	while (Rows.Num() < Capped)
	{
		UBNScoreRow* Row = CreateWidget<UBNScoreRow>(this, RowClass);
		if (!Row)
		{
			break;
		}
		RowContainer->AddChild(Row);
		Row->ClearRow();   // collapsed until it is claimed, like every placed row
		Rows.Add(Row);
	}
	UE_LOG(LogBN, Log, TEXT("BNScoreboard: row pool grown to %d for a %d-player roster."),
		Rows.Num(), Needed);
	return Rows.Num();
}

void UBNScreen_Scoreboard::LayoutRowBlock(int32 VisibleRows)
{
	UCanvasPanelSlot* ListSlot = RowContainer ? Cast<UCanvasPanelSlot>(RowContainer->Slot) : nullptr;
	if (!ListSlot)
	{
		// A WBP that parents the list some other way keeps whatever layout it authored.
		return;
	}
	UCanvasPanelSlot* RuleSlot = TableBottomRule ? Cast<UCanvasPanelSlot>(TableBottomRule->Slot) : nullptr;
	if (!bCapturedAuthoredLayout)
	{
		AuthoredListHeight = ListSlot->GetSize().Y;
		AuthoredRuleY = RuleSlot ? RuleSlot->GetPosition().Y : 0.0f;
		bCapturedAuthoredLayout = true;
	}

	// ONLY grow, never shrink. The authored box already holds 12 rows of 22 in its 272, and
	// the founder signed off on that spacing — a board that resized itself for every 4-player
	// match would redesign an approved screen. Past what the box holds the rows would simply
	// render through the rule below, so from there the block earns its extra height.
	const float Needed = VisibleRows * ScoreRowHeight;
	const float Height = FMath::Max(Needed, AuthoredListHeight);
	const FVector2D Size = ListSlot->GetSize();
	if (!FMath::IsNearlyEqual(Size.Y, Height))
	{
		ListSlot->SetSize(FVector2D(Size.X, Height));
	}
	if (RuleSlot)
	{
		// Keep the authored gap between the box bottom and the rule, whatever it was.
		const float Gap = AuthoredRuleY - (ListSlot->GetPosition().Y + AuthoredListHeight);
		const float TargetY = ListSlot->GetPosition().Y + Height + Gap;
		const FVector2D Pos = RuleSlot->GetPosition();
		if (!FMath::IsNearlyEqual(Pos.Y, TargetY))
		{
			RuleSlot->SetPosition(FVector2D(Pos.X, TargetY));
		}
	}
}

void UBNScreen_Scoreboard::Refresh()
{
	const UBNVM_Match* Match = BoundViewModel.Get();
	const TArray<FBNScoreRowView>* Roster = Match ? &Match->GetRoster() : nullptr;
	const int32 EntryCount = Roster ? Roster->Num() : 0;

	// Grow the pool to the roster BEFORE deciding what is shown. The WBP places 8; a 6v6 or
	// 8v8 lobby needs 12 or 16, and dropping the tail would silently hide half a team.
	const int32 Available = EnsureRowCapacity(EntryCount);
	const int32 Shown = FMath::Min(EntryCount, Available);
	if (EntryCount > Available && !bWarnedRowShortage)
	{
		bWarnedRowShortage = true;
		UE_LOG(LogBN, Warning, TEXT("BNScoreboard: %d players but only %d rows available (MaxScoreRows=%d) — the tail is dropped."),
			EntryCount, Available, MaxScoreRows);
	}
	LayoutRowBlock(Shown);

	// TEAMS (BN16): the two blocks, as claim ORDER over the same pooled rows — no second
	// container, no divider the WBP never placed. Stable partition: everything that is not
	// Enemy first (Self, Ally, AND the joining client's honest-unknown None rows — they sit in
	// my block with today's colors until their TeamId lands and the roster rebuilds), the enemy
	// block after. The teams-OFF proof is structural: in FFA every relation is None, the second
	// pass adds nothing, and Order is the identity — the claim loop below indexes the roster
	// exactly as it did before this array existed.
	TArray<int32, TInlineAllocator<8>> Order;
	Order.Reserve(EntryCount);
	for (int32 Index = 0; Index < EntryCount; ++Index)
	{
		if ((*Roster)[Index].Relation != EBNUITeamRelation::Enemy)
		{
			Order.Add(Index);
		}
	}
	for (int32 Index = 0; Index < EntryCount; ++Index)
	{
		if ((*Roster)[Index].Relation == EBNUITeamRelation::Enemy)
		{
			Order.Add(Index);
		}
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
			Row->SetRow((*Roster)[Order[RowIndex]]);
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

	RefreshTeamScores(Match);
	RefreshOutcome(Match);
}

void UBNScreen_Scoreboard::RefreshTeamScores(const UBNVM_Match* Match)
{
	if (!MyTeamScoreText && !EnemyTeamScoreText)
	{
		return;
	}

	// bTeamsMode false IS the FFA/honest-unknown state (SetTeamScores' contract) — the header
	// collapses and the board is today's, byte for byte. No MatchDataState gate on top: the VM
	// never raises the flag before the director has a ledger to report.
	const bool bTeams = Match && Match->IsTeamsMode();
	const ESlateVisibility Vis = bTeams ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	// My side FIRST and blue, theirs red — relation tints, never a team's own color (the same
	// two hues both clients see, each meaning "mine"/"theirs" to its own reader).
	if (MyTeamScoreText)
	{
		MyTeamScoreText->SetText(bTeams ? FText::AsNumber(Match->GetMyTeamScore()) : FText::GetEmpty());
		MyTeamScoreText->SetColorAndOpacity(FSlateColor(BNUIColors::Ally));
		MyTeamScoreText->SetVisibility(Vis);
	}
	if (EnemyTeamScoreText)
	{
		EnemyTeamScoreText->SetText(bTeams ? FText::AsNumber(Match->GetEnemyTeamScore()) : FText::GetEmpty());
		EnemyTeamScoreText->SetColorAndOpacity(FSlateColor(BNUIColors::Threat));
		EnemyTeamScoreText->SetVisibility(Vis);
	}
}

void UBNScreen_Scoreboard::RefreshOutcome(const UBNVM_Match* Match)
{
	if (!OutcomeText && !OutcomeAccent)
	{
		return;
	}

	const EBNMatchOutcome Outcome = Match ? Match->GetOutcome() : EBNMatchOutcome::Undecided;

	FText Word;
	FLinearColor Tint = BNUIColors::InkDim;
	switch (Outcome)
	{
	case EBNMatchOutcome::Victory:
		Word = LOCTEXT("OutcomeVictory", "VICTORY");
		Tint = BNUIColors::Shield;   // hud/self — the same cyan that means YOU everywhere else
		break;
	case EBNMatchOutcome::Defeat:
		Word = LOCTEXT("OutcomeDefeat", "DEFEAT");
		Tint = BNUIColors::Threat;   // hud/threat — red is only ever a threat, and losing is one
		break;
	case EBNMatchOutcome::Draw:
		Word = LOCTEXT("OutcomeDraw", "DRAW");
		break;
	default:
		break;                        // Undecided: the band is absent, not blank
	}

	// Collapsed, not Hidden: mid-match the band must take NO layout, or the table sits 96px
	// down all game waiting for a word that has not happened yet.
	const ESlateVisibility Vis = Word.IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;

	if (OutcomeText)
	{
		OutcomeText->SetText(Word);
		OutcomeText->SetColorAndOpacity(FSlateColor(Tint));
		OutcomeText->SetVisibility(Vis);
	}
	if (OutcomeAccent)
	{
		OutcomeAccent->SetColorAndOpacity(Tint);
		OutcomeAccent->SetVisibility(Vis);
	}
}

#undef LOCTEXT_NAMESPACE
