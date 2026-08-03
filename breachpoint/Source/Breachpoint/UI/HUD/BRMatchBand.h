#pragma once

#include "CommonUserWidget.h"
#include "FieldNotificationId.h"
#include "UI/BRUITypes.h"

#include "BRMatchBand.generated.h"

class UBRVM_Match;
class UCommonTextBlock;
class UWidget;

/**
 * `UBRMatchBand` -- the TOP-CENTRE band: team score, match clock, rocket countdown
 * (GDD Sec 2.9, `ue5-ui-architecture` Sec 4).
 *
 * IT OWNS NO CLOCK. Read this before adding a timer here.
 * `UBRVM_Match` already derives the countdown from ONE replicated float
 * (`MatchEndServerTime`) and already re-derives it on a timer that is re-armed to the next
 * whole server second -- `UBRVM_Match::ScheduleNextClockUpdate`, `Delay = 1 - frac(Now)`.
 * That is the 1Hz display timer the doctrine asks for, it is phase-locked to the server
 * second rather than to whenever this widget happened to be constructed (which is what makes
 * a naive `SetTimer(1.0f, looping)` visibly stutter), and it stops itself when neither clock
 * is running. A second timer in this widget would beat against that one and show a digit
 * change twice per second. So this class is PUSH-ONLY: it subscribes to the FieldNotify
 * broadcasts the ViewModel already emits and re-renders on change. No Tick, no timer.
 *
 * THE THREE VIEWS SEE THE SAME DIGITS because every view runs the same subtraction against
 * `AGameStateBase::GetServerWorldTimeSeconds()`; nothing about the clock is replicated per tick.
 *
 * HONEST UNKNOWN: a late joiner has `MatchState == Unknown` and `LocalTeamId == 255` for one
 * or more frames after possession (`ue5-ui-architecture` Sec 7). Until BOTH are known the
 * scores render the em dash, never a confident `0 - 0` -- a joiner being told the game is tied
 * when it is 3-0 is a false statement about the fight, not a cosmetic nit. The clock text needs
 * no such guard: the ViewModel already formats an unset clock as `--:--`.
 *
 * NO AUTHORITY, NO STATE: this widget reads the ViewModel and nothing else. It never touches
 * GameState, PlayerState or an ASC, sends no RPC, and has no intent path (there is nothing to
 * press). It is also non-interactive, so it takes no focus and cannot trap gamepad navigation:
 * `InputMode` stays `Inherit`, which makes `GetDesiredInputConfig()` return an unset optional.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRMatchBand : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

	//~ Begin UUserWidget interface
	// RE-BASED off UBRActivatableWidget (HUD-CPP-AUDIT §4): the band is never pushed to a
	// layer, so activation fired from its own construct — activation scope WAS construct scope
	// wearing a CommonUI hat, while the activatable base cost it bSupportsActivationFocus=true
	// (the one live focus hazard on the HUD). Bind/unbind now live where they always
	// effectively ran.
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	/** Pushes every displayed value from the ViewModel. The only place text is written. */
	void Refresh();

	/** Dashes in both score slots, `--:--` on the clock, rocket row hidden. */
	void ClearToUnknown();

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_MatchBand`. `Tools/gen_ui/wbp_plan.py` parses these names out
	// of this header; the WBP's widget names must match exactly.
	// -------------------------------------------------------------------------------------------

	/** Your team's score. Coloured `visr/shield` -- the "you" channel. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|HUD")
	TObjectPtr<UCommonTextBlock> AllyScoreText;

	/** The other team's score. Coloured `visr/team-them`, NOT threat red. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|HUD")
	TObjectPtr<UCommonTextBlock> EnemyScoreText;

	/**
	 * Formatted by the ViewModel (`M:SS`, or `--:--` when unset). Never formatted here.
	 * `visr/amber` -- the "a clock is running" channel (`semantic.hud.clock`), same as the
	 * rocket countdown below. The WBP must not restyle it.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|HUD")
	TObjectPtr<UCommonTextBlock> ClockText;

	/** `visr/amber` -- the "a clock is running" channel (`semantic.hud.clock`). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<UCommonTextBlock> RocketCountdownText;

	/**
	 * The rocket row's container. Set HIDDEN, never collapsed, when there is no countdown:
	 * collapsing it re-centres the match clock and the whole band twitches sideways the
	 * instant the power weapon spawns.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
	TObjectPtr<UWidget> RocketCountdownRoot;

private:
	void HandleMatchFieldChanged(UObject* Source, UE::FieldNotification::FFieldId FieldId);

	void BindMatchField(UBRVM_Match* Match, UE::FieldNotification::FFieldId FieldId);

	UBRVM_Match* ResolveMatchViewModel() const;

	/** One handle per subscribed field, so destruct can drop exactly what it took. */
	TArray<TPair<UE::FieldNotification::FFieldId, FDelegateHandle>> BoundFields;

	/**
	 * The object the delegates were actually added to (HUD-CPP-AUDIT H9). Unbinding via a
	 * fresh subsystem lookup removes from whatever the subsystem holds NOW — if the ViewModel
	 * was swapped between construct and destruct, the real binding outlives the widget.
	 */
	TWeakObjectPtr<UBRVM_Match> BoundViewModel;
};
