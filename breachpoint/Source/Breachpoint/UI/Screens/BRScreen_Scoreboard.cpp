#include "UI/Screens/BRScreen_Scoreboard.h"

#include "CommonTextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/Widget.h"
#include "INotifyFieldValueChanged.h"
#include "MVVMSubsystem.h"
#include "UI/BRUISettings.h"
#include "UI/BRViewModels.h"
#include "UI/Components/BRComponentTokens.h"
#include "View/MVVMView.h"

UBRScreen_Scoreboard::UBRScreen_Scoreboard()
{
	// The scene behind is BLURRED, not PAUSED. Game input keeps flowing; see GetDesiredInputConfig.
	InputMode = EBRWidgetInputMode::Game;
	bHideCursorDuringViewportCapture = true;

	// TAB-up pops this. Taking the back action would make B / Escape close the scoreboard instead of
	// opening the pause menu -- the stack owns back-handling, not this screen.
	bIsBackHandler = false;

	// Not modal: it does not block what is under it, and it must not make CommonUI treat the layer
	// beneath as suspended.
	bIsModal = false;
}

TOptional<FUIInputConfig> UBRScreen_Scoreboard::GetDesiredInputConfig() const
{
	return FUIInputConfig(
		ECommonInputMode::Game,
		EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown,
		bHideCursorDuringViewportCapture);
}

void UBRScreen_Scoreboard::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (TableSizeBox)
	{
		// 694 x 281. The column tints measure 282 and so run 1px under the bottom rule at y=472 --
		// that is the tint's business, not the body's, so the body is driven from the rules.
		TableSizeBox->SetWidthOverride(TableWidth);
		TableSizeBox->SetHeightOverride(TableBodyBottomRuleY - TableBodyTopY);
	}

	// The overhang, applied once. The bar is an Overlay child anchored top-left at the table body
	// origin; this render translation is what puts it 5px OUTSIDE the table's left edge. It is a
	// RENDER transform, so it moves nothing else in the overlay and no layout pass can undo it.
	if (LocalAccentBar)
	{
		LocalAccentBar->SetRenderTranslation(FVector2D(LocalAccentOverhangX, 0.0f));
	}

	ApplyLocalHighlight();
}

void UBRScreen_Scoreboard::BindViewModels()
{
	Super::BindViewModels();

	PushViewModelsIntoMVVMView();

	// Defensive by default: on the first frames after join or travel there is no match ViewModel yet,
	// and the honest answer then is the same as the honest answer now -- unknown.
	EBRUIDataState MatchState = EBRUIDataState::Unknown;

	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		MatchStateFieldHandle = Match->AddFieldValueChangedDelegate(
			UBRVM_Match::FFieldNotificationClassDescriptor::MatchState,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
				this, &UBRScreen_Scoreboard::HandleViewModelFieldChanged));

		MatchState = Match->GetMatchState();
	}

	ApplyRosterState(MatchState);
}

void UBRScreen_Scoreboard::UnbindViewModels()
{
	if (MatchStateFieldHandle.IsValid())
	{
		if (UBRVM_Match* Match = GetMatchViewModel())
		{
			Match->RemoveFieldValueChangedDelegate(
				UBRVM_Match::FFieldNotificationClassDescriptor::MatchState, MatchStateFieldHandle);
		}
	}
	MatchStateFieldHandle.Reset();

	// A row index from the last match must not survive into the next one.
	ClearLocalPlayerRow();

	Super::UnbindViewModels();
}

void UBRScreen_Scoreboard::PushViewModelsIntoMVVMView()
{
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		return;
	}

	// Clock, phase, team scores and local team id are all live FieldNotify fields on the match
	// ViewModel, which owns its own clock timer. That is why this screen keeps updating over a live
	// match without a single line of Tick.
	if (UBRVM_Match* Match = GetMatchViewModel())
	{
		View->SetViewModel(UBRUISettings::Get().MatchViewModelContextName, Match);
	}
}

void UBRScreen_Scoreboard::HandleViewModelFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	const UBRVM_Match* Match = Cast<UBRVM_Match>(Source);
	if (Match && FieldId == UBRVM_Match::FFieldNotificationClassDescriptor::MatchState)
	{
		ApplyRosterState(Match->GetMatchState());
	}
}

void UBRScreen_Scoreboard::ApplyRosterState(EBRUIDataState InMatchState)
{
	// ---------------------------------------------------------------------------------------------
	// THE ROSTER HAS NO SOURCE. `UBRVM_Match` carries the match frame and no per-player collection
	// (see the CONTRACT GAP in the header), so there is nothing to build rows from and nothing to
	// sort. This function is the whole hook: when the ViewModel grows the roster array, it fills
	// `RosterContainer` here -- sorted here, because order is presentation -- and stops collapsing
	// it. Until then the region says "not known yet" and says it the same way for every state,
	// including Live: a live match with no roster field is still a match whose roster is unknown.
	// ---------------------------------------------------------------------------------------------
	const bool bHasRosterSource = false;

	if (RosterContainer)
	{
		RosterContainer->SetVisibility(bHasRosterSource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (RosterUnavailableText)
	{
		// The em dash, not "No players" and not eight rows of zeroes. An invented zero row is a
		// confident lie; the dash is the true statement.
		RosterUnavailableText->SetText(BRUI::UnknownValueText());
		RosterUnavailableText->SetVisibility(bHasRosterSource ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// Referenced so the parameter is honestly part of the contract the roster hook will read, and so
	// a Stale match frame is distinguishable from a Live one the moment there is anything to stale.
	(void)InMatchState;

	if (!bHasRosterSource)
	{
		ClearLocalPlayerRow();
	}
}

void UBRScreen_Scoreboard::SetLocalPlayerRow(int32 TeamSlot, int32 RowInTeam)
{
	const bool bInRange =
		(TeamSlot == 0 || TeamSlot == 1) &&
		RowInTeam >= 0 && RowInTeam < RowsPerTeam;

	if (!bInRange)
	{
		// Out of range clears rather than clamps: a highlight on the wrong player is worse than none.
		ClearLocalPlayerRow();
		return;
	}

	LocalHighlightTeamSlot = TeamSlot;
	LocalHighlightRowInTeam = RowInTeam;

	ApplyLocalHighlight();
}

void UBRScreen_Scoreboard::ClearLocalPlayerRow()
{
	LocalHighlightTeamSlot = INDEX_NONE;
	LocalHighlightRowInTeam = INDEX_NONE;

	ApplyLocalHighlight();
}

void UBRScreen_Scoreboard::ApplyLocalHighlight()
{
	const bool bVisible = LocalHighlightTeamSlot != INDEX_NONE && LocalHighlightRowInTeam != INDEX_NONE;

	// Both pieces move together, off ONE index, so the bar can never drift a row away from the row it
	// belongs to.
	const float BlockOffsetY = (LocalHighlightTeamSlot == 1) ? TeamFillBottomOffsetY : TeamFillTopOffsetY;
	const float RowOffsetY = bVisible ? BlockOffsetY + (LocalHighlightRowInTeam * RowPitch) : 0.0f;

	if (LocalHighlightRow)
	{
		LocalHighlightRow->SetRenderTranslation(FVector2D(0.0f, RowOffsetY));
		LocalHighlightRow->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (LocalAccentBar)
	{
		LocalAccentBar->SetRenderTranslation(FVector2D(LocalAccentOverhangX, RowOffsetY));
		LocalAccentBar->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
