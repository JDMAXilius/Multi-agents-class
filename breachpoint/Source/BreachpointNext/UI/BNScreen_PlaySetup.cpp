#include "UI/BNScreen_PlaySetup.h"
#include "BreachpointNext.h"
#include "Components/Image.h"
#include "UI/Components/BRButton.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "UI/BNUITypes.h"
#include "UI/BNTeamRoster.h"
#include "UI/BNPromptButton.h"
#include "UI/Components/BRPageTitle.h"

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

	// The NATIVE OnClicked() event, per CommonUI — the dynamic one is private to Blueprint.
	// Labels are set here too, so the WBP still types no strings (the pause screen's law).
	if (MapButton)
	{
		MapButton->OnClicked().AddUObject(this, &UBNScreen_PlaySetup::HandleMapClicked);
		MapButton->SetLabelText(LOCTEXT("RowMap", "MAP"));
	}
	if (ModeButton)
	{
		ModeButton->OnClicked().AddUObject(this, &UBNScreen_PlaySetup::HandleModeClicked);
		ModeButton->SetLabelText(LOCTEXT("RowMode", "MODE"));
	}
	if (BotsButton)
	{
		BotsButton->OnClicked().AddUObject(this, &UBNScreen_PlaySetup::HandleBotsClicked);
		BotsButton->SetLabelText(LOCTEXT("RowPlayers", "PLAYERS"));
	}
	if (ScoreLimitButton)
	{
		ScoreLimitButton->OnClicked().AddUObject(this, &UBNScreen_PlaySetup::HandleScoreLimitClicked);
		ScoreLimitButton->SetLabelText(LOCTEXT("RowScoreLimit", "SCORE LIMIT"));
	}
	if (TimeLimitButton)
	{
		TimeLimitButton->OnClicked().AddUObject(this, &UBNScreen_PlaySetup::HandleTimeLimitClicked);
		TimeLimitButton->SetLabelText(LOCTEXT("RowTimeLimit", "TIME LIMIT"));
	}
	if (StartButton)
	{
		StartButton->OnClicked().AddUObject(this, &UBNScreen_PlaySetup::HandleStartClicked);
		StartButton->SetLabelText(LOCTEXT("RowStart", "START GAME"));
	}
	// The legend (`Button Prompts` `21:43024`) is clickable since 2 Sep: same glyph, same verb,
	// and either one is the way back. Escape in NativeOnKeyDown stays the keyboard route.
	for (UBNPromptButton* Prompt : { BackPrompt.Get(), MenuPrompt.Get() })
	{
		if (Prompt)
		{
			Prompt->OnClicked().AddUObject(this, &UBNScreen_PlaySetup::HandleBackClicked);
		}
	}
	if (BackPrompt) { BackPrompt->SetVerbText(LOCTEXT("PromptBack", "Back")); }
	if (MenuPrompt) { MenuPrompt->SetVerbText(LOCTEXT("PromptMenu", "Menu")); }
	// The Idle→Hover edge transition the shared component drops on the floor — see BNButtonEdges.
	for (UBRButton* Button : { MapButton.Get(), ModeButton.Get(), BotsButton.Get(),
	                           ScoreLimitButton.Get(), TimeLimitButton.Get() })
	{
		BNButtonEdges::Bind(Button, BNButtonEdges::EChrome::MenuRow);
	}

	// The menu opens on the founder's default mode, and the lobby size follows FROM it -
	// same derivation a mode change uses, so boot and a toggle can never disagree.
	if (PageTitle)
	{
		// The band is `UBRPageTitle`'s; we only hand it the two strings. Figma splits the
		// breadcrumb from the page name (`CUSTOMIZE` then `ARMOR HALL` at x134), and the class
		// already models that split, so it goes in as two fields rather than one packed line.
		PageTitle->SetBreadcrumbText(LOCTEXT("CrumbCreate", "CREATE"));
		PageTitle->SetTitleText(LOCTEXT("TitleCustomGame", "CUSTOM GAME"));
	}

		bTeams = bDefaultTeams;
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

void UBNScreen_PlaySetup::HandleScoreLimitClicked()
{
	if (ScoreLimitPresets.Num() > 0)
	{
		ScoreLimitIndex = (ScoreLimitIndex + 1) % ScoreLimitPresets.Num();
	}
	RefreshDisplay();
}

void UBNScreen_PlaySetup::HandleTimeLimitClicked()
{
	if (TimeLimitPresets.Num() > 0)
	{
		TimeLimitIndex = (TimeLimitIndex + 1) % TimeLimitPresets.Num();
	}
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
	// ScoreLimit / TimeLimit are OMITTED when the presets are empty, so an ini that clears them
	// falls back to the game mode's own numbers rather than sending zero.
	FString Options = FString::Printf(TEXT("listen?TargetPlayers=%d?Teams=%d"), TotalPlayers, bTeams ? 1 : 0);
	if (ScoreLimitPresets.IsValidIndex(ScoreLimitIndex))
	{
		Options += FString::Printf(TEXT("?ScoreLimit=%d"), ScoreLimitPresets[ScoreLimitIndex]);
	}
	if (TimeLimitPresets.IsValidIndex(TimeLimitIndex))
	{
		Options += FString::Printf(TEXT("?TimeLimit=%d"), TimeLimitPresets[TimeLimitIndex] * 60);
	}
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
	if (MapButton)
	{
		MapButton->SetSelectionText(bHasMap
			? FText::FromString(Maps[MapIndex].DisplayName.ToUpper())
			: LOCTEXT("NoMapValue", "NONE"));
	}
	if (ModeButton)
	{
		ModeButton->SetSelectionText(bTeams
			? LOCTEXT("ModeTeams", "TEAM DEATHMATCH")
			: LOCTEXT("ModeFFA", "FREE-FOR-ALL"));
	}
	if (BotsButton)
	{
		// Printed the way a player thinks about it, not the way the option travels: the
		// URL carries a TOTAL, the row says what that total means in this mode.
		BotsButton->SetSelectionText(bTeams
			? FText::Format(LOCTEXT("BotsTeams", "{0} V {1}"),
				FText::AsNumber((TotalPlayers + 1) / 2), FText::AsNumber(TotalPlayers / 2))
			: FText::Format(LOCTEXT("BotsFFA", "{0}  (YOU + {1} BOTS)"),
				FText::AsNumber(TotalPlayers), FText::AsNumber(FMath::Max(0, TotalPlayers - 1))));
	}
	if (ScoreLimitButton)
	{
		ScoreLimitButton->SetSelectionText(ScoreLimitPresets.IsValidIndex(ScoreLimitIndex)
			? FText::Format(LOCTEXT("ScoreLimitValue", "{0} KILLS"), FText::AsNumber(ScoreLimitPresets[ScoreLimitIndex]))
			: LOCTEXT("LimitDefault", "DEFAULT"));
	}
	if (TimeLimitButton)
	{
		TimeLimitButton->SetSelectionText(TimeLimitPresets.IsValidIndex(TimeLimitIndex)
			? FText::Format(LOCTEXT("TimeLimitValue", "{0} MIN"), FText::AsNumber(TimeLimitPresets[TimeLimitIndex]))
			: LOCTEXT("LimitDefault", "DEFAULT"));
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(bHasMap
			? FText::FromString(Maps[MapIndex].Description)
			: LOCTEXT("PlaySetupHint", "Select a map to play in."));
		DescriptionText->SetColorAndOpacity(FSlateColor(BNUIColors::InkDim));
	}
	if (SettingsPanel)
	{
		// The gamemode card mirrors the MODE row, so the centre column and the left rail can
		// never disagree about what is selected.
		const bool bTeamsMode = bTeams;
		SettingsPanel->SetGamemode(
			bTeamsMode ? LOCTEXT("ModeTeamsTitle", "ARENA: TEAM DEATHMATCH")
			           : LOCTEXT("ModeFFATitle", "ARENA: FREE-FOR-ALL"),
			bTeamsMode ? LOCTEXT("ModeTeamsBody", "Two fireteams. Highest score when the clock runs out takes it.")
			           : LOCTEXT("ModeFFABody", "Every player for themselves. First to the score cap wins."),
			bTeamsMode ? TeamsModeIcon : FreeForAllModeIcon,
			LOCTEXT("ModeVersion", "v1.0"));

		// DETAILS is the map's own rows; OVERRIDES is what THIS lobby changed. Two entries in
		// one array, not two panels - the component stacks whatever it is handed.
		TArray<FBNSettingSection> Sections;

		FBNSettingSection& Details = Sections.AddDefaulted_GetRef();
		Details.Header = LOCTEXT("SectionDetails", "DETAILS");
		if (bHasMap)
		{
			Details.Rows = Maps[MapIndex].Details;
		}

		FBNSettingSection& Overrides = Sections.AddDefaulted_GetRef();
		Overrides.Header = LOCTEXT("SectionOverrides", "OVERRIDES");
		FBNSettingRow& CountRow = Overrides.Rows.AddDefaulted_GetRef();
		CountRow.Label = LOCTEXT("OverrideBotCount", "Bot Count");
		CountRow.Value = FText::AsNumber(FMath::Max(0, TotalPlayers - 1));
		FBNSettingRow& TeamRow = Overrides.Rows.AddDefaulted_GetRef();
		TeamRow.Label = LOCTEXT("OverrideTeams", "Team Layout");
		TeamRow.Value = bTeamsMode ? LOCTEXT("OverrideTeamsOn", "TWO TEAMS")
		                           : LOCTEXT("OverrideTeamsOff", "NONE");
		if (ScoreLimitPresets.IsValidIndex(ScoreLimitIndex))
		{
			FBNSettingRow& ScoreRow = Overrides.Rows.AddDefaulted_GetRef();
			ScoreRow.Label = LOCTEXT("OverrideScore", "Score Limit");
			ScoreRow.Value = FText::AsNumber(ScoreLimitPresets[ScoreLimitIndex]);
		}
		if (TimeLimitPresets.IsValidIndex(TimeLimitIndex))
		{
			FBNSettingRow& TimeRow = Overrides.Rows.AddDefaulted_GetRef();
			TimeRow.Label = LOCTEXT("OverrideTime", "Time Limit");
			TimeRow.Value = FText::Format(LOCTEXT("OverrideTimeValue", "{0} MIN"), FText::AsNumber(TimeLimitPresets[TimeLimitIndex]));
		}

		SettingsPanel->SetSections(Sections);
	}

	if (TeamRoster)
	{
		// The lobby list mirrors the PLAYERS row exactly: cycle 4v4 -> 8v8 and the roster grows
		// with it, because both read TotalPlayers rather than keeping their own count.
		const int32 Shown = FMath::Clamp(TotalPlayers, 0, LobbyPlayerNames.Num());
		const int32 TeamCount = bTeams ? FMath::Max(1, TeamNames.Num()) : 1;

		TArray<FBNRosterTeam> Lobby;
		Lobby.SetNum(TeamCount);
		for (int32 Team = 0; Team < TeamCount; ++Team)
		{
			Lobby[Team].Name = TeamNames.IsValidIndex(Team)
				? FText::FromString(TeamNames[Team])
				: LOCTEXT("LobbyFFA", "PLAYERS");
			Lobby[Team].Color = TeamColors.IsValidIndex(Team) ? TeamColors[Team] : FLinearColor::Transparent;
		}

		for (int32 Index = 0; Index < Shown; ++Index)
		{
			// Alternating rather than blocked: it keeps both teams the same size as the count
			// grows, which is what "4v4 and 8v8" means and what a block split would get wrong
			// on an odd count.
			FBNRosterTeam& Team = Lobby[Index % TeamCount];
			FBRRosterMemberView& Member = Team.Members.AddDefaulted_GetRef();
			Member.Gamertag = FText::FromString(LobbyPlayerNames[Index]);
			Member.Emblem = RosterEmblem;
			Member.MicState = EBRRosterMicState::Idle;
			Member.bIsLocalPlayer = Member.bIsPartyLeader = (Index == 0);
			// Transparent: the reference's nameplate banners are 343-owned art and the IP line
			// keeps them out until we author our own.
			Member.TeamFillColor = FLinearColor::Transparent;
		}

		TeamRoster->SetTeams(Lobby);
		TeamRoster->SetHeaderText(LOCTEXT("LobbyHeader", "LOBBY"), Shown);
	}
	if (PreviewImage)
	{
		// Synchronous load, and that is deliberate: this fires on a menu keypress, one 698x393
		// plate at a time, with nothing on screen depending on the frame. An async request here
		// would land AFTER the player had already cycled past the map it belongs to.
		UTexture2D* Plate = bHasMap ? Maps[MapIndex].PreviewTexture.LoadSynchronous() : nullptr;
		if (Plate)
		{
			PreviewImage->SetBrushFromTexture(Plate, /*bMatchSize*/ false);
			PreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// No plate configured is not an error - the panel ground stands on its own.
			PreviewImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

#undef LOCTEXT_NAMESPACE
