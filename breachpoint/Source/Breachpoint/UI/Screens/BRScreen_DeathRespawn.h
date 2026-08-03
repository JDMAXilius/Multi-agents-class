#pragma once

#include "UI/BRActivatableWidget.h"

#include "BRScreen_DeathRespawn.generated.h"

class UBRMatchBand;
class UCommonTextBlock;
class UImage;
class UWidget;
struct FBRKillfeedViewEntry;

/**
 * `UBRScreen_DeathRespawn` -- the death / respawn overlay. Figma frame `36:2`
 * (`Tools/gen_ui/figma_screen_layout.json`, block `death_respawn`, file key
 * `yznvnVdOFDADaugZSeomfP`). WBP: `WBP_Screen_DeathRespawn`. Pushed to the game-menu layer by
 * `UBRUIManagerSubsystem::ShowDeathOverlay` (`UBRUISettings::DeathOverlayClass`).
 *
 * THE FRAME IS MARKED "ORIGINAL" AND THAT CHANGES THE BAR. Its own name says only the kill cam
 * is documented; `HUD-AUDIT.md` Sec 5.4 and `REFERENCE-EXTRACTION.md` Sec 8.2 both state that no
 * death/respawn screen exists in the audited reference. Nothing on this screen can be checked
 * against a source, so every string it shows must come from data or from the em dash. This class
 * writes exactly two values -- the killer's name and the countdown -- and invents neither.
 *
 * WHAT THIS SCREEN CANNOT SHOW TODAY (the "C++ GAP" the designer drew INTO the mock at `36:21`)
 * -------------------------------------------------------------------------------------------
 * Two of the frame's elements have no producer anywhere in the repo, and both are already
 * ticketed. They are not worked around here; they render honestly empty:
 *
 *  1. THE RESPAWN COUNTDOWN (ring sweep + `36:2`'s `countdown` slot). `UBRVM_Match` has
 *     `SecondsRemaining` / `MatchClockText` and `RocketSecondsRemaining` / `RocketCountdownText`
 *     and NOTHING for respawn -- `ABRGameMode` holds the float privately and never replicates
 *     it. `TICKET_BP23_RESPAWN_COUNTDOWN.md` step 2 already specifies the exact fields
 *     (`SetRespawnReadyServerTime`, `RespawnSecondsRemaining`, `RespawnCountdownText`, driven off
 *     the existing `UpdateClocks()`). Until they land this screen renders the em dash and hides
 *     the sweep. See `ApplyRespawnCountdown`.
 *  2. THE KILLING WEAPON (`killing_weapon`: an 84x20 silhouette plus a 118x19 name).
 *     `FBRKillfeedViewEntry` carries a `DamageTag`, not a weapon display name, and there is no
 *     tag -> row mapping on any ViewModel; the icon field itself is `TICKET_BP25_WEAPON_ICON.md`
 *     (`FBRWeaponRow::IconSoftPath`, still open). One tag is not a name and this class will not
 *     guess one, so `KillingWeaponRoot` stays collapsed. See `NativeOnInitialized`.
 *
 * NO CLOCK OF ITS OWN. Same law as `UBRMatchBand`: `UBRVM_Match::ScheduleNextClockUpdate` arms a
 * one-shot re-armed at `1.0 - frac(GetServerWorldTimeSeconds())`, phase-locked to the whole
 * server second. When BP23 lands, the respawn value rides that same timer and this widget only
 * re-renders on the FieldNotify broadcast. A `SetTimer` or a Tick here would beat against it and
 * flicker the digit twice a second. Push-only, `DisableNativeTick`, no timer, ever.
 *
 * MATCH STATE PERSISTS THROUGH DEATH (frame `36:15` says so in its own name) -- and it is the
 * SAME data as the in-match band in a different layout, so it is the same C++ class. `MatchState`
 * below is a nested `UBRMatchBand`; this screen never reads `Team0Score`, `Team1Score`,
 * `LocalTeamId` or `MatchClockText` for display. Two layouts, one binding, and no way for the
 * band and the death screen to disagree because there is only one of them.
 *
 * NO AUTHORITY, NO STATE, NO INTENT. It reads `UBRVM_Match` and nothing else: no pawn, no ASC,
 * no PlayerState, no GameState, no RPC. Respawn HAPPENS on the server's timer whatever this
 * screen draws (`netcode.md` law 3), so there is no intent path to own here and none is added.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class BREACHPOINT_API UBRScreen_DeathRespawn : public UBRActivatableWidget
{
	GENERATED_BODY()

public:
	UBRScreen_DeathRespawn();

	/**
	 * Scalar parameter the Tier-4 respawn-ring material must expose. THE MATERIAL DOES NOT EXIST
	 * YET -- this name is the contract for whoever authors it, and this constant is its one copy.
	 *
	 * The mock's ring is a TRACK and a SWEEP: two separate 104x104 circles at (588,410).
	 * `UProgressBar` cannot draw a radial fill and `UBRProgressBar` wraps a `UProgressBar`, so
	 * neither is usable here; a rotated rectangle or a stack of eighth-images would be a
	 * different shape wearing the ring's clothes. Per `BREACHPOINT-AUTHORING-MATRIX.md` Tier 4
	 * this is a material (`M_UI_RadialSweep`, angular gradient thresholded by `Sweep`), and C++
	 * only pushes the scalar. 0 = no sweep, 1 = the full circle, wound clockwise from 12.
	 */
	static const FName RespawnRingSweepParameterName;

	/**
	 * DELIBERATE, AND THE TRAP ON THIS SCREEN IS REAL: a death overlay that takes `Menu` input
	 * strands a dead player. Justified in full at the definition in the .cpp -- read it before
	 * changing this.
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnInitialized() override;

	//~ Begin UBRActivatableWidget interface
	virtual void BindViewModels() override;
	virtual void UnbindViewModels() override;
	//~ End UBRActivatableWidget interface

	/** Pushes every displayed value from the ViewModel. The only place text is written. */
	void Refresh();

	/** Em dash in both slots, sweep hidden. The state the screen is in on its first frame. */
	void ClearToUnknown();

	// -------------------------------------------------------------------------------------------
	// BindWidget contract for `WBP_Screen_DeathRespawn`. `Tools/gen_ui/wbp_plan.py` parses these
	// names out of this header; the WBP's widget names must match exactly.
	//
	// The full-bleed `scrim` and `vignette` layers are NOT here on purpose. They are never
	// toggled by state, so they are static WBP art and carry no C++ member and no colour --
	// their tokens (`PanelGround80` for the scrim) are set on the brush in the asset. A hex
	// literal on either of them is a finding.
	// -------------------------------------------------------------------------------------------

	/** `killer` (0,276,1280,59). The one name this screen states, and only from the killfeed. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> KillerNameText;

	/**
	 * `respawn_ring.countdown` (588,432,104,71). Formatted by the ViewModel when BP23 lands, the
	 * em dash until then. Never formatted here -- one clock idiom in the project, and it is
	 * `UBRVM_Match::FormatClock`.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Breachpoint|UI")
	TObjectPtr<UCommonTextBlock> RespawnCountdownText;

	/**
	 * `respawn_ring.sweep` (588,410,104,104). Its brush must be the radial-sweep MATERIAL; the
	 * static `track` circle beneath it is a separate, unbound WBP image. Optional so the screen
	 * still builds and still tells the truth before the material exists.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UImage> RespawnRingSweep;

	/**
	 * `killing_weapon` (540,340,200,34): silhouette + name. Collapsed for the whole lifetime of
	 * this class as written -- see gap 2 in the class comment. When a weapon display name and
	 * `FBRWeaponRow::IconSoftPath` reach a ViewModel, this is the one visibility to flip.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UWidget> KillingWeaponRoot;

	/**
	 * `match_state` (520,660,240,44) -- score / timer / score, persisting through death.
	 * A nested `UBRMatchBand` in a death-screen WBP layout, NOT a second set of bindings. This
	 * class never touches it: it auto-activates with its parent and binds `UBRVM_Match` itself.
	 * Declared only so the WBP contract is visible in one place.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|UI")
	TObjectPtr<UBRMatchBand> MatchState;

private:
	void HandleKillfeedChanged();

	/**
	 * The newest killfeed entry that is plausibly THIS player's death, or null.
	 *
	 * GAP (reported): `FBRKillfeedViewEntry::bLocalPlayerInvolved` is ONE bit covering TWO roles
	 * -- "I killed them" and "they killed me" are indistinguishable on the struct. The team ids
	 * separate the common case (a trade where my own kill lands after my death) and nothing
	 * separates a teamkill. A `bLocalPlayerIsVictim` bit on the struct would close it exactly.
	 */
	const FBRKillfeedViewEntry* FindLocalDeathEntry() const;

	/**
	 * The countdown half of `Refresh`, alone so the BP23 landing is a single edit in a single
	 * function and nobody has to find the em dash twice.
	 */
	void ApplyRespawnCountdown();
};
