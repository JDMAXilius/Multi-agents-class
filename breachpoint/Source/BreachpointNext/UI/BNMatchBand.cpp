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

	// Honest dashes at Unknown — a confident "0" and "0:00" before the GameState arrives tells
	// the player something false about a match.
	if (MyKillsText)
	{
		MyKillsText->SetText(bLive ? FText::AsNumber(Match->GetMyKills()) : FText::FromString(TEXT("—")));
	}
	if (TopKillsText)
	{
		TopKillsText->SetText(bLive ? FText::AsNumber(Match->GetTopKills()) : FText::FromString(TEXT("—")));
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
	// that the score is zero, which is exactly the lie the dashes above exist to avoid.
	if (SelfScoreBar)
	{
		SelfScoreBar->SetPercent(bLive ? Match->GetSelfScoreFraction() : 0.f);
		SelfScoreBar->SetVisibility(bLive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (TopScoreBar)
	{
		TopScoreBar->SetPercent(bLive ? Match->GetTopScoreFraction() : 0.f);
		TopScoreBar->SetVisibility(bLive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
