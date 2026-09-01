#include "UI/BNScreen_PlaySetup.h"
#include "BreachpointNext.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "UI/BNUITypes.h"

#define LOCTEXT_NAMESPACE "BreachpointNextUI"

UBNScreen_PlaySetup::UBNScreen_PlaySetup()
{
	InputMode = EBNWidgetInputMode::Menu;
	bIsBackHandler = true;   // the hook, per the pause screen; NativeOnKeyDown is the way out
	bIsModal = true;
}

void UBNScreen_PlaySetup::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::Visible);

	if (MapButton)   { MapButton->OnClicked.AddUniqueDynamic(this, &UBNScreen_PlaySetup::HandleMapClicked); }
	if (ModeButton)  { ModeButton->OnClicked.AddUniqueDynamic(this, &UBNScreen_PlaySetup::HandleModeClicked); }
	if (BotsButton)  { BotsButton->OnClicked.AddUniqueDynamic(this, &UBNScreen_PlaySetup::HandleBotsClicked); }
	if (StartButton) { StartButton->OnClicked.AddUniqueDynamic(this, &UBNScreen_PlaySetup::HandleStartClicked); }
	if (BackButton)  { BackButton->OnClicked.AddUniqueDynamic(this, &UBNScreen_PlaySetup::HandleBackClicked); }

	TotalPlayers = DefaultPlayersForMode(bTeams);
	RefreshDisplay();
}

UWidget* UBNScreen_PlaySetup::NativeGetDesiredFocusTarget() const
{
	return StartButton;
}

FReply UBNScreen_PlaySetup::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// The pause screen's lesson verbatim: no UCommonUIInputData ships, so CommonUI's back
	// action binds to nothing and Escape must be caught by hand or the mouse is the only exit.
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		DeactivateWidget();   // pops this stack entry; focus lands back on the front end
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UBNScreen_PlaySetup::HandleMapClicked()
{
	if (Maps.Num() > 0)
	{
		MapIndex = (MapIndex + 1) % Maps.Num();
	}
	RefreshDisplay();
}

void UBNScreen_PlaySetup::HandleModeClicked()
{
	bTeams = !bTeams;
	// A mode change RESETS the lobby size to that mode's shape — the founder's "based on
	// the mode" derivation — and the BOTS row then edits freely from there.
	TotalPlayers = DefaultPlayersForMode(bTeams);
	RefreshDisplay();
}

void UBNScreen_PlaySetup::HandleBotsClicked()
{
	if (PlayerCountPresets.Num() == 0)
	{
		return;
	}
	// Step to the preset after the current total (wrapping), so a hand-set ini list of any
	// length cycles correctly. Odd totals in TEAMS are lawful — BN15's balancer assigns by
	// lowest-population team, so 4v3 is what seven players IS.
	int32 Next = 0;
	for (int32 i = 0; i < PlayerCountPresets.Num(); ++i)
	{
		if (PlayerCountPresets[i] > TotalPlayers)
		{
			Next = i;
			break;
		}
	}
	TotalPlayers = PlayerCountPresets[Next];
	RefreshDisplay();
}

void UBNScreen_PlaySetup::HandleStartClicked()
{
	if (!Maps.IsValidIndex(MapIndex) || Maps[MapIndex].MapPath.IsEmpty())
	{
		// The designed miss, loudly: an empty ini roster must read as "not configured",
		// never as a dead button with no explanation in the log.
		UE_LOG(LogBN, Warning, TEXT("BNScreen_PlaySetup: START with no configured map "
			"(Maps[%d] of %d) — check [/Script/BreachpointNext.BNScreen_PlaySetup] in DefaultGame.ini."),
			MapIndex, Maps.Num());
		if (DescriptionText)
		{
			DescriptionText->SetText(LOCTEXT("NoMaps", "No maps configured."));
			DescriptionText->SetColorAndOpacity(FSlateColor(BNUIColors::Threat));
		}
		return;
	}

	// THE LAUNCH IS A URL. InitGame parses these two; everything downstream (fill to
	// TargetPlayers, team assignment, HUD, bots) already exists and is match-proven.
	// ?listen is deliberate: free in standalone, and the day sessions arrive this same
	// screen hosts one without a change.
	const FString Options = FString::Printf(TEXT("listen?TargetPlayers=%d?Teams=%d"),
		TotalPlayers, bTeams ? 1 : 0);
	UE_LOG(LogBN, Log, TEXT("BNScreen_PlaySetup: launching %s (%s)."),
		*Maps[MapIndex].MapPath, *Options);
	UGameplayStatics::OpenLevel(this, FName(*Maps[MapIndex].MapPath), /*bAbsolute*/ true, Options);
	// No deactivate: the world this widget lives in is about to be torn down with it, and
	// the pause screen's travel-order lesson does not apply — there is no game input config
	// beneath a front-end map worth restoring.
}

void UBNScreen_PlaySetup::HandleBackClicked()
{
	DeactivateWidget();
}

void UBNScreen_PlaySetup::RefreshDisplay()
{
	const bool bHasMap = Maps.IsValidIndex(MapIndex);
	if (MapValueText)
	{
		MapValueText->SetText(bHasMap
			? FText::FromString(Maps[MapIndex].DisplayName.ToUpper())
			: LOCTEXT("NoMapValue", "NONE"));
	}
	if (ModeValueText)
	{
		ModeValueText->SetText(bTeams
			? LOCTEXT("ModeTeams", "TEAM DEATHMATCH")
			: LOCTEXT("ModeFFA", "FREE-FOR-ALL"));
	}
	if (BotsValueText)
	{
		// Printed the way a player thinks about it, not the way the option travels: the
		// URL carries a TOTAL, the row says what that total means in this mode.
		BotsValueText->SetText(bTeams
			? FText::Format(LOCTEXT("BotsTeams", "{0} V {1}"),
				FText::AsNumber((TotalPlayers + 1) / 2), FText::AsNumber(TotalPlayers / 2))
			: FText::Format(LOCTEXT("BotsFFA", "{0}  (YOU + {1} BOTS)"),
				FText::AsNumber(TotalPlayers), FText::AsNumber(FMath::Max(0, TotalPlayers - 1))));
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(bHasMap
			? FText::FromString(Maps[MapIndex].Description)
			: LOCTEXT("PlaySetupHint", "Select a map to play in."));
		DescriptionText->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim));
	}
}

#undef LOCTEXT_NAMESPACE
