#include "UI/BNScreen_PostMatch.h"

#include "BreachpointNext.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameState.h"
#include "Kismet/GameplayStatics.h"
#include "Match/BNPlayerController.h"
#include "UI/BNPromptButton.h"
#include "UI/BNUIManager.h"
#include "UI/BNUITypes.h"
#include "UI/BNViewModels.h"
#include "UI/Components/BRButton.h"

#define LOCTEXT_NAMESPACE "BreachpointNextUI"

UBNScreen_PostMatch::UBNScreen_PostMatch()
{
	// The scoreboard's own reasoning: topping the Game stack must not leave the router holding
	// a stale config. The recap is a menu the player clicks, so it takes Menu input.
	InputMode = EBNWidgetInputMode::Menu;
}

void UBNScreen_PostMatch::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (TabRecap)  { TabRecap->SetLabelText(LOCTEXT("TabRecap", "PLAYER RECAP")); BNButtonEdges::Bind(TabRecap, BNButtonEdges::EChrome::Boxed); TabRecap->SetIsSelected(true); }
	if (TabLineup) { TabLineup->SetLabelText(LOCTEXT("TabLineup", "TEAM LINEUP")); BNButtonEdges::Bind(TabLineup, BNButtonEdges::EChrome::Boxed); }
	if (TabBoard)
	{
		TabBoard->SetLabelText(LOCTEXT("TabBoard", "SCOREBOARD"));
		BNButtonEdges::Bind(TabBoard, BNButtonEdges::EChrome::Boxed);
		TabBoard->OnClicked().AddUObject(this, &UBNScreen_PostMatch::HandleBoardTab);
	}
	if (ClosePrompt)
	{
		ClosePrompt->SetVerbText(LOCTEXT("PromptClose", "Close"));
		ClosePrompt->OnClicked().AddUObject(this, &UBNScreen_PostMatch::HandleClosePrompt);
	}
	if (MatchmakingPrompt) { MatchmakingPrompt->SetVerbText(LOCTEXT("PromptMatchmaking", "Start Matchmaking")); }
	if (ProfilePrompt)     { ProfilePrompt->SetVerbText(LOCTEXT("PromptProfile", "Profile")); }

	// The layout script stores no colour; the chrome is tinted here, once.
	TintChrome(TEXT("NavPillLeft"),  FLinearColor(0.f, 0.f, 0.f, 0.7f));
	TintChrome(TEXT("NavPillRight"), FLinearColor(0.f, 0.f, 0.f, 0.7f));
	TintChrome(TEXT("ScoreRule"),    FLinearColor(1.f, 1.f, 1.f, 0.5f));
	TintChrome(TEXT("RankRule"),     FLinearColor(1.f, 1.f, 1.f, 0.5f));
	TintChrome(TEXT("StatsPlate"),   FLinearColor(0.f, 0.f, 0.f, 0.55f));
	TintChrome(TEXT("StatsHighlight"), FLinearColor(1.f, 1.f, 1.f, 0.12f));
	TintChrome(TEXT("StatsHighlightRule"), BNUIColors::Self);
	TintChrome(TEXT("StatsCaret"),   BNUIColors::Self);
	TintChrome(TEXT("MedalsPlate"),  FLinearColor(0.f, 0.f, 0.f, 0.55f));
	TintChrome(TEXT("MedalFrame"),   BNUIColors::Self);
	TintChrome(TEXT("BottomBand"),   FLinearColor(0.f, 0.f, 0.f, 0.6f));
	for (int32 i = 0; i < 8; ++i)
	{
		TintChrome(*FString::Printf(TEXT("Medal%d"), i), FLinearColor(BNUIColors::InkDim.R, BNUIColors::InkDim.G, BNUIColors::InkDim.B, 0.6f));
	}
	if (RankProgress)
	{
		RankProgress->SetFillColorAndOpacity(BNUIColors::Self);
	}
}

void UBNScreen_PostMatch::TintChrome(const TCHAR* Name, const FLinearColor& Colour)
{
	if (UImage* Img = Cast<UImage>(GetWidgetFromName(Name)))
	{
		Img->SetColorAndOpacity(Colour);
	}
}

void UBNScreen_PostMatch::BindViewModels()
{
	UBNVM_Match* Match = GetMatchViewModel();
	if (!Match)
	{
		return;
	}
	BoundViewModel = Match;
	RosterHandle = Match->OnRosterViewChanged.AddUObject(this, &UBNScreen_PostMatch::HandleRosterChanged);
	for (const UE::FieldNotification::FFieldId FieldId : {
		UBNVM_Match::FFieldNotificationClassDescriptor::Outcome,
		UBNVM_Match::FFieldNotificationClassDescriptor::bTeamsMode,
		UBNVM_Match::FFieldNotificationClassDescriptor::MyTeamScore,
		UBNVM_Match::FFieldNotificationClassDescriptor::EnemyTeamScore })
	{
		if (FieldId.IsValid())
		{
			BoundFields.Emplace(FieldId, Match->AddFieldValueChangedDelegate(FieldId,
				INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(this, &UBNScreen_PostMatch::HandleMatchFieldChanged)));
		}
	}
	Refresh();
}

void UBNScreen_PostMatch::UnbindViewModels()
{
	if (UBNVM_Match* Match = BoundViewModel.Get())
	{
		Match->OnRosterViewChanged.Remove(RosterHandle);
		for (const TPair<UE::FieldNotification::FFieldId, FDelegateHandle>& Bound : BoundFields)
		{
			Match->RemoveFieldValueChangedDelegate(Bound.Key, Bound.Value);
		}
	}
	BoundFields.Reset();
	RosterHandle.Reset();
	BoundViewModel.Reset();
}

void UBNScreen_PostMatch::HandleRosterChanged() { Refresh(); }
void UBNScreen_PostMatch::HandleMatchFieldChanged(UObject*, UE::FieldNotification::FFieldId) { Refresh(); }

void UBNScreen_PostMatch::Refresh()
{
	const UBNVM_Match* Match = BoundViewModel.Get();
	const bool bTeams = Match && Match->IsTeamsMode();
	const EBNMatchOutcome Outcome = Match ? Match->GetOutcome() : EBNMatchOutcome::Undecided;

	// The win/lose the founder asked for, in the colours the scoreboard's band already speaks.
	if (OutcomeText)
	{
		FText Word; FLinearColor Tint = BNUIColors::InkDim;
		switch (Outcome)
		{
		case EBNMatchOutcome::Victory: Word = LOCTEXT("RecapVictory", "VICTORY"); Tint = BNUIColors::Shield; break;
		case EBNMatchOutcome::Defeat:  Word = LOCTEXT("RecapDefeat", "DEFEAT");   Tint = BNUIColors::Threat; break;
		case EBNMatchOutcome::Draw:    Word = LOCTEXT("RecapDraw", "DRAW"); break;
		default:                       Word = LOCTEXT("RecapLive", "IN PROGRESS"); break;
		}
		OutcomeText->SetText(Word);
		OutcomeText->SetColorAndOpacity(FSlateColor(Tint));
	}

	// Header: the capture's three lines, right-aligned, with '|' dividers.
	FString Level = UGameplayStatics::GetCurrentLevelName(this, true);
	Level.RemoveFromStart(TEXT("BR_"));
	const AGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGameState>() : nullptr;
	const int32 Elapsed = GS ? FMath::Max(0, GS->ElapsedTime) : 0;
	const FText Duration = FText::FromString(FString::Printf(TEXT("%d:%02d"), Elapsed / 60, Elapsed % 60));
	if (HeaderLine1)
	{
		HeaderLine1->SetText(FText::Format(LOCTEXT("RecapHeader1", "{0}  |  {1}"),
			bTeams ? LOCTEXT("RecapModeTeams", "ARENA: TEAM DEATHMATCH") : LOCTEXT("RecapModeFFA", "ARENA: FREE-FOR-ALL"),
			FText::FromString(Level.ToUpper())));
	}
	if (HeaderLine2)
	{
		HeaderLine2->SetText(bTeams
			? FText::Format(LOCTEXT("RecapHeader2Teams", "DURATION: {0}  |  SCORE: {1}-{2}"), Duration,
				FText::AsNumber(Match->GetMyTeamScore()), FText::AsNumber(Match->GetEnemyTeamScore()))
			: FText::Format(LOCTEXT("RecapHeader2FFA", "DURATION: {0}"), Duration));
	}
	if (HeaderLine3)
	{
		// Relative by law: no literal team names reach a widget until the founder rules on them.
		FText Winner = LOCTEXT("RecapWinnerNone", "—");
		if (Outcome == EBNMatchOutcome::Victory) { Winner = LOCTEXT("RecapWinnerMine", "YOUR TEAM"); }
		else if (Outcome == EBNMatchOutcome::Defeat) { Winner = LOCTEXT("RecapWinnerTheirs", "ENEMY TEAM"); }
		// "YOUR TEAM: <name>" waits for literal team names; "YOUR TEAM: YOUR TEAM" says nothing.
		HeaderLine3->SetText(FText::Format(LOCTEXT("RecapHeader3", "WINNER: {0}"), Winner));
		HeaderLine3->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// My row of the roster: the only per-player numbers this game tracks.
	const FBNScoreRowView* Me = nullptr;
	int32 MyPlace = 0;
	if (Match)
	{
		const TArray<FBNScoreRowView>& Roster = Match->GetRoster();
		for (int32 i = 0; i < Roster.Num(); ++i)
		{
			if (Roster[i].bIsSelf) { Me = &Roster[i]; MyPlace = i + 1; break; }
		}
	}
	const int32 Kills = Me ? Me->Kills : 0, Deaths = Me ? Me->Deaths : 0, Score = Me ? Me->Score : 0;
	if (ScoreValueText)
	{
		ScoreValueText->SetText(FText::AsNumber(Score));   // grouping separators come with the locale
	}
	if (RankLineText)
	{
		RankLineText->SetText(FText::FromString(RankLine));
		RankLineText->SetColorAndOpacity(FSlateColor(BNUIColors::Shield));
	}
	if (RankProgress)
	{
		RankProgress->SetPercent(FMath::Clamp(RankFraction, 0.f, 1.f));
	}

	// Six columns: what exists as numbers, dashes for what does not (assists, medals).
	const FText Dash = FText::FromString(TEXT("—"));
	const FText Labels[6] = { LOCTEXT("StatKills", "KILLS"), LOCTEXT("StatDeaths", "DEATHS"), LOCTEXT("StatAssists", "ASSISTS"),
	                          LOCTEXT("StatKda", "KDA"), LOCTEXT("StatScore", "SCORE"), LOCTEXT("StatMedals", "MEDALS") };
	const FText Values[6] = { FText::AsNumber(Kills), FText::AsNumber(Deaths), Dash,
	                          FText::FromString(FString::Printf(TEXT("%.1f"), static_cast<float>(Kills - Deaths))), FText::AsNumber(Score), Dash };
	for (int32 i = 0; i < 6; ++i)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("StatLabel%d"), i))))
		{
			Label->SetText(Labels[i]);
			Label->SetColorAndOpacity(FSlateColor(i == 3 ? BNUIColors::Self : BNUIColors::InkDim));
		}
		if (UTextBlock* Value = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("StatValue%d"), i))))
		{
			Value->SetText(Values[i]);
		}
	}
	for (int32 i = 0; i < 8; ++i)
	{
		if (UTextBlock* Count = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("MedalCount%d"), i))))
		{
			Count->SetText(Dash);
			Count->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim));
		}
	}
	if (MedalNameText) { MedalNameText->SetText(LOCTEXT("RecapMedalsTitle", "MEDALS")); }
	if (MedalDescText)
	{
		MedalDescText->SetText(LOCTEXT("RecapMedalsBody", "Medal tracking arrives with the match-stats packet."));
		MedalDescText->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim));
	}

	// Nameplate: placement among everyone, gamertag, and the relative team line.
	if (NameplateRank) { NameplateRank->SetText(MyPlace > 0 ? FText::AsNumber(MyPlace) : Dash); NameplateRank->SetColorAndOpacity(FSlateColor(BNUIColors::Threat)); }
	if (NameplateName) { NameplateName->SetText(Me ? FText::FromString(Me->PlayerName.ToUpper()) : Dash); }
	if (NameplateSub)  { NameplateSub->SetText(bTeams ? LOCTEXT("RecapPlateSub", "YOUR TEAM") : LOCTEXT("RecapPlateSubFFA", "FREE-FOR-ALL")); NameplateSub->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim)); }
}

void UBNScreen_PostMatch::HandleClosePrompt()
{
	if (ABNPlayerController* PC = GetOwningPlayer<ABNPlayerController>())
	{
		PC->LeaveMatch();
	}
}

void UBNScreen_PostMatch::HandleBoardTab()
{
	// The scoreboard page rides on top of the recap; its PLAYER RECAP tab pops it again.
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UBNUIManager* Manager = LocalPlayer ? UBNUIManager::Get(LocalPlayer) : nullptr;
	if (Manager)
	{
		Manager->PushWidgetToLayer(LocalPlayer, FBNUITags::Get().Layer_Game, Manager->GetScoreboardClass());
	}
}

#undef LOCTEXT_NAMESPACE
