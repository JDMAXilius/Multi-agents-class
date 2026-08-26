#include "UI/BNMatchBand.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "INotifyFieldValueChanged.h"
#include "UI/BNUIManager.h"
#include "UI/BNUITypes.h"
#include "UI/BNViewModels.h"

void UBNMatchBand::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	// Captured BEFORE the band writes any color, so a teams→FFA flip can put back exactly what
	// the WBP authored (see the member's comment).
	if (TopKillsText) { DefaultTopKillsTint = TopKillsText->GetColorAndOpacity(); }
	if (MyKillsText) { MyKillsText->SetColorAndOpacity(FSlateColor(BNUIColors::Shield)); }
	Refresh();
}

void UBNMatchBand::NativeConstruct()
{
	Super::NativeConstruct();

	UBNUIManager* Manager = UBNUIManager::Get(this);
	UBNVM_Match* Match = Manager ? Manager->GetMatchViewModel(GetOwningLocalPlayer()) : nullptr;
	if (!Match)
	{
		return;
	}
	BoundViewModel = Match;

	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::MyKills);
	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::TopKills);
	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::ScoreLimit);
	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::MatchClockText);
	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::MatchDataState);
	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::SelfScoreFraction);
	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::TopScoreFraction);

	// TEAMS (BN16): the relative ledger — same mechanism, same one handler, still zero polling.
	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::bTeamsMode);
	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::MyTeamScore);
	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::EnemyTeamScore);
	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::MyTeamScoreFraction);
	BindMatchField(Match, UBNVM_Match::FFieldNotificationClassDescriptor::EnemyTeamScoreFraction);

	// Subscribe, then read once — the state that existed before this widget did.
	Refresh();
}

void UBNMatchBand::NativeDestruct()
{
	if (UBNVM_Match* Match = BoundViewModel.Get())
	{
		for (const TPair<UE::FieldNotification::FFieldId, FDelegateHandle>& Bound : BoundFields)
		{
			if (Bound.Value.IsValid())
			{
				Match->RemoveFieldValueChangedDelegate(Bound.Key, Bound.Value);
			}
		}
	}
	BoundFields.Reset();
	BoundViewModel.Reset();

	Super::NativeDestruct();
}

void UBNMatchBand::BindMatchField(UBNVM_Match* Match, UE::FieldNotification::FFieldId FieldId)
{
	if (FieldId.IsValid())
	{
		BoundFields.Emplace(FieldId, Match->AddFieldValueChangedDelegate(FieldId,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &UBNMatchBand::HandleMatchFieldChanged)));
	}
}

void UBNMatchBand::HandleMatchFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	Refresh();
}

void UBNMatchBand::Refresh()
{
	const UBNVM_Match* Match = BoundViewModel.Get();
	const bool bLive = Match && Match->GetMatchDataState() == EBNUIDataState::Live;

	// TEAMS (BN16): one flag, read from the VM like every other value. In teams mode the SAME
	// two readouts and two bars carry the relative team ledger — my side's points left, where my
	// kills sit; the enemy's where the leader's sat — because the design has one band, not two.
	// False (FFA, or the joining client before the director raises it) takes every today-branch
	// below unchanged.
	const bool bTeams = bLive && Match->IsTeamsMode();

	// Relation tints, applied only when the MODE flips: mine is Ally blue, theirs is Threat red
	// (the enemy is the one thing red was always allowed to name). Flipping back restores
	// Shield — the color this widget itself set at initialize — and the WBP's own TopKillsText
	// tint; the FFA refresh path never touches a color, exactly as it never did.
	if (bTeams != bTeamTintApplied)
	{
		bTeamTintApplied = bTeams;
		if (MyKillsText) { MyKillsText->SetColorAndOpacity(FSlateColor(bTeams ? BNUIColors::Ally : BNUIColors::Shield)); }
		if (TopKillsText) { TopKillsText->SetColorAndOpacity(bTeams ? FSlateColor(BNUIColors::Threat) : DefaultTopKillsTint); }
	}

	// Honest dashes at Unknown — a confident "0" and "0:00" before the GameState arrives tells
	// the player something false about a match.
	if (MyKillsText)
	{
		MyKillsText->SetText(bLive ? FText::AsNumber(bTeams ? Match->GetMyTeamScore() : Match->GetMyKills()) : FText::FromString(TEXT("—")));
	}
	if (TopKillsText)
	{
		TopKillsText->SetText(bLive ? FText::AsNumber(bTeams ? Match->GetEnemyTeamScore() : Match->GetTopKills()) : FText::FromString(TEXT("—")));
	}
	if (ScoreLimitText)
	{
		ScoreLimitText->SetText(bLive ? FText::AsNumber(Match->GetScoreLimit()) : FText::GetEmpty());
	}
	if (ClockText)
	{
		ClockText->SetText(Match && bLive ? Match->GetMatchClockText() : FText::FromString(TEXT("—:——")));
	}

	// R7.7 — the two bars. HIDDEN at Unknown rather than drawn empty: an empty bar is a claim
	// that the score is zero, which is exactly the lie the dashes above exist to avoid. In teams
	// mode they read the TEAM fractions (mine, theirs) — the VM recomputes both against the same
	// limit, so the pair stays comparable.
	if (SelfScoreBar)
	{
		SelfScoreBar->SetPercent(bLive ? (bTeams ? Match->GetMyTeamScoreFraction() : Match->GetSelfScoreFraction()) : 0.f);
		SelfScoreBar->SetVisibility(bLive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (TopScoreBar)
	{
		TopScoreBar->SetPercent(bLive ? (bTeams ? Match->GetEnemyTeamScoreFraction() : Match->GetTopScoreFraction()) : 0.f);
		TopScoreBar->SetVisibility(bLive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
