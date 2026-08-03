#include "UI/HUD/BRMatchBand.h"

#include "CommonTextBlock.h"
#include "Components/Widget.h"
#include "INotifyFieldValueChanged.h"
#include "UI/BRUIManagerSubsystem.h"
#include "UI/BRViewModels.h"
#include "UI/Components/BRComponentTokens.h"
#include "UI/Styles/BRUITokens.h"

UBRVM_Match* UBRMatchBand::ResolveMatchViewModel() const
{
	UBRUIManagerSubsystem* Manager = UBRUIManagerSubsystem::Get(this);
	return Manager ? Manager->GetMatchViewModel(GetOwningLocalPlayer()) : nullptr;
}

void UBRMatchBand::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Channel colours are FIXED per slot, so they are applied once here rather than on every
	// refresh. Ally is `visr/shield` (the "you" channel), enemy is `visr/team-them`. Threat red
	// is deliberately absent: an enemy SCORE is not an incoming-damage event, and spending the
	// threat channel on it means the channel says nothing when a rocket is actually inbound.
	if (AllyScoreText)
	{
		AllyScoreText->SetColorAndOpacity(FSlateColor(BR::Tokens::Shield()));
	}

	if (EnemyScoreText)
	{
		EnemyScoreText->SetColorAndOpacity(FSlateColor(BR::Tokens::TeamThem()));
	}

	// `semantic.hud.clock` -> `visr/amber`: a clock is running. Not a warning. BOTH clocks in
	// this band are that channel -- the match clock is the literal thing the token names, and
	// left uncoloured it drew default white, which reads as plain text where the design says
	// "a timer is counting". The rocket countdown had the token and the match clock did not:
	// an omission, not a distinction. Nothing separates them in `figma_tokens.json`, no
	// comment claimed a reason, and `semantic.hud.clock` has exactly one value.
	if (ClockText)
	{
		ClockText->SetColorAndOpacity(FSlateColor(BR::Tokens::Amber()));
	}

	if (RocketCountdownText)
	{
		RocketCountdownText->SetColorAndOpacity(FSlateColor(BR::Tokens::Amber()));
	}

	// The band is decoration everywhere it appears — HUD frame or death screen — and must
	// never eat a click or take gamepad focus (HUD-CPP-AUDIT H7). HitTestInvisible propagates
	// to every child, so this one call covers the whole band.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// First frame on screen is an honest unknown, before any ViewModel has been consulted.
	ClearToUnknown();
}

void UBRMatchBand::NativeConstruct()
{
	Super::NativeConstruct();

	UBRVM_Match* Match = ResolveMatchViewModel();
	if (!Match)
	{
		// Legal on the first frames after travel: the subsystem may not have built the
		// ViewModel for this local player yet. Show the unknown state rather than inventing
		// a score.
		ClearToUnknown();
		return;
	}

	BoundViewModel = Match;

	// Subscribed fields, and why these six:
	//   MatchState          -- gates the whole band between unknown and live.
	//   MatchClockText      -- the ViewModel's 1Hz re-derivation lands here (see the header).
	//   RocketCountdownText -- also re-broadcast when `bRocketAvailable` flips, because
	//                          `SetRocketAvailable` runs `UpdateClocks`. Subscribing to the
	//                          text covers the bool without a second subscription.
	//   Team0Score/Team1Score -- GameState RepNotify feeds these.
	//   LocalTeamId         -- Refresh() GATES the score display on it (HUD-CPP-AUDIT H13);
	//                          the old "pushed once at possession, always before Live" claim
	//                          was an ordering assumption no producer enforces, and when the
	//                          id landed after the last score push the band showed dashes for
	//                          the rest of the match.
	BindMatchField(Match, UBRVM_Match::FFieldNotificationClassDescriptor::MatchState);
	BindMatchField(Match, UBRVM_Match::FFieldNotificationClassDescriptor::MatchClockText);
	BindMatchField(Match, UBRVM_Match::FFieldNotificationClassDescriptor::RocketCountdownText);
	BindMatchField(Match, UBRVM_Match::FFieldNotificationClassDescriptor::Team0Score);
	BindMatchField(Match, UBRVM_Match::FFieldNotificationClassDescriptor::Team1Score);
	BindMatchField(Match, UBRVM_Match::FFieldNotificationClassDescriptor::LocalTeamId);

	Refresh();
}

void UBRMatchBand::NativeDestruct()
{
	// Unbind from the object the delegates were ADDED to (H9), not a fresh lookup — if the
	// subsystem swapped the ViewModel in between, the lookup removes from the wrong object and
	// the real binding outlives the widget: the crash the death cam finds.
	if (UBRVM_Match* Match = BoundViewModel.Get())
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

void UBRMatchBand::BindMatchField(UBRVM_Match* Match, UE::FieldNotification::FFieldId FieldId)
{
	if (!Match || !FieldId.IsValid())
	{
		return;
	}

	const FDelegateHandle Handle = Match->AddFieldValueChangedDelegate(
		FieldId,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
			this, &UBRMatchBand::HandleMatchFieldChanged));

	BoundFields.Emplace(FieldId, Handle);
}

void UBRMatchBand::HandleMatchFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId)
{
	// Which field changed does not matter: the band shows five values, all from one ViewModel,
	// and re-pushing all of them is two FText compares and three SetText calls. Branching per
	// field would buy nothing and would rot the moment a slot is added.
	Refresh();
}

void UBRMatchBand::Refresh()
{
	const UBRVM_Match* Match = BoundViewModel.Get();
	if (!Match)
	{
		ClearToUnknown();
		return;
	}

	const uint8 LocalTeamId = Match->GetLocalTeamId();

	// GAP (reported): `UBRVM_Match` initialises the scores to 0 rather than to an unknown
	// sentinel, so "not replicated yet" and "genuinely nil-nil" are the same two ints. The
	// only honest discriminators available are `MatchState` and the 255 team sentinel, so
	// both must be known before a number is shown.
	const bool bScoresKnown = (Match->GetMatchState() != EBRUIDataState::Unknown)
		&& (LocalTeamId == 0 || LocalTeamId == 1);

	if (!bScoresKnown)
	{
		if (AllyScoreText)
		{
			AllyScoreText->SetText(BRUI::UnknownValueText());
		}
		if (EnemyScoreText)
		{
			EnemyScoreText->SetText(BRUI::UnknownValueText());
		}
	}
	else
	{
		const int32 AllyScore = (LocalTeamId == 0) ? Match->GetTeam0Score() : Match->GetTeam1Score();
		const int32 EnemyScore = (LocalTeamId == 0) ? Match->GetTeam1Score() : Match->GetTeam0Score();

		if (AllyScoreText)
		{
			AllyScoreText->SetText(FText::AsNumber(AllyScore));
		}
		if (EnemyScoreText)
		{
			EnemyScoreText->SetText(FText::AsNumber(EnemyScore));
		}
	}

	if (ClockText)
	{
		// Already formatted, already `--:--` when unset, and already held at `0:00` at the end
		// of the match (`UBRVM_Match::UpdateClocks` clamps at zero and then stops its own
		// timer, so the final digit does not flicker back to a dash). Nothing to decide here.
		ClockText->SetText(Match->GetMatchClockText());
	}

	// The rocket row shows only while a countdown is actually running. Once the weapon is
	// available the ViewModel reports `INDEX_NONE`, which is the same "no countdown" state as
	// pre-match -- both hide the row. HIDDEN, not collapsed: see the header.
	const bool bRocketCountdownRunning = Match->GetRocketSecondsRemaining() >= 0;

	if (RocketCountdownText)
	{
		RocketCountdownText->SetText(
			bRocketCountdownRunning ? Match->GetRocketCountdownText() : BRUI::UnknownValueText());
	}

	if (RocketCountdownRoot)
	{
		RocketCountdownRoot->SetVisibility(
			bRocketCountdownRunning ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UBRMatchBand::ClearToUnknown()
{
	if (AllyScoreText)
	{
		AllyScoreText->SetText(BRUI::UnknownValueText());
	}

	if (EnemyScoreText)
	{
		EnemyScoreText->SetText(BRUI::UnknownValueText());
	}

	// The clocks use the ViewModel's own unset format ("--:--"), not the em dash: an unknown
	// clock rendered as one glyph when the VM object is missing and another when it exists but
	// is unfed was two different marks for one state (HUD-CPP-AUDIT stale-comment sweep).
	if (ClockText)
	{
		ClockText->SetText(NSLOCTEXT("BRMatchBand", "UnknownClock", "--:--"));
	}

	if (RocketCountdownText)
	{
		RocketCountdownText->SetText(NSLOCTEXT("BRMatchBand", "UnknownClock", "--:--"));
	}

	if (RocketCountdownRoot)
	{
		RocketCountdownRoot->SetVisibility(ESlateVisibility::Hidden);
	}
}
