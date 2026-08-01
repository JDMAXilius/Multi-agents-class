// Breachpoint. The two ViewModels. Gameplay pushes; widgets never reach back.

#pragma once

#include "MVVMViewModelBase.h"
#include "Types/MVVMEventField.h"
#include "UI/BRUITypes.h"

#include "BRViewModels.generated.h"

class AGameStateBase;
class UAbilitySystemComponent;
class UWorld;
struct FOnAttributeChangeData;

/**
 * BRViewModels -- BREACHPOINT-ARCHITECTURE.md 3.9 (v2: "merged - UBRVM_Combat + UBRVM_Match in
 * one pair; both are FieldNotify ViewModels fed by ASC delegates and RepNotify events - zero
 * polling").
 *
 * ============================ THE BOUNDARY (read this once) ============================
 *
 * The ViewModel is the ONLY thing that touches gameplay. Widgets bind to ViewModel fields and
 * are told when they change. A widget that calls GetPlayerState(), GetASC(), or GetGameState()
 * has crossed the boundary, and every such call is a per-frame poll wearing a different hat.
 *
 * DIRECTION IS ONE-WAY. Gameplay -> ViewModel -> widget. Nothing here has a path back into the
 * simulation: there is no Server RPC on this class, no replicated property, and no setter a
 * widget is allowed to call. A widget that needs to CHANGE something sends intent through the
 * owning PlayerController's interface (ABRPlayerController, Match/) - never through here.
 * That is the multiplayer UI rule, and it is why every field below is `Getter`-only:
 * declaring `Setter` on a FieldNotify property enables MVVM two-way binding, which is exactly
 * the widget-writes-state path this architecture forbids.
 *
 * SKILL CORRECTION (ue5-ui-architecture 3 said `UPROPERTY(FieldNotify, Setter, Getter)`):
 * `Setter` is both unnecessary (one-way binding needs only the getter) and actively wrong here
 * (it opens the back door above). The verified UE 5.8 in-tree shape is
 * `UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetX", meta = (AllowPrivateAccess))`
 * on a private member - see Engine/Plugins/Experimental/TechAudioTools/.../AudioComponentViewModel.h.
 *
 * NO TICK, NO POLL, AND THE ONE HONEST EXCEPTION. Nothing here reads gameplay state on a timer
 * EXCEPT the match clock, which is arithmetic on one replicated float and a world time, re-armed
 * on the next whole-second boundary. A clock has to be driven by something; a self-rescheduling
 * timer is the lawful driver (law 4 names timers explicitly) and it costs one callback per
 * displayed second instead of one per frame. The skill describes the "computed, not replicated"
 * rule but never says what drives the redraw - that gap is closed here.
 *
 * EVERY FIELD STARTS UNKNOWN. See EBRUIDataState in BRUITypes.h. A join-in-progress client sees
 * dashes, not `0 / 100`.
 */

// ---------------------------------------------------------------------------
// Native (non-dynamic) events, for C++ widgets that do not go through the MVVM view.
// ---------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE_OneParam(FBROnHitMarker, EBRHitMarkerKind /*Kind*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FBROnKillfeedEntryAdded, const FBRKillfeedViewEntry& /*Entry*/);
DECLARE_MULTICAST_DELEGATE(FBROnKillfeedChanged);

// ===========================================================================
// UBRVM_Combat
// ===========================================================================

/**
 * The local player's own combat feed: shields over health, ammo, grenades, grapple, hit markers.
 *
 * Fed by (and ONLY by):
 *   - ASC attribute-change delegates          -> health / shields
 *   - ASC gameplay-tag events                 -> State.Shields.Broken
 *   - equipment RepNotify, pushed by Weapons/ -> ammo, weapon names
 *   - the local damage GameplayCue            -> hit markers
 */
UCLASS(BlueprintType, DisplayName = "BR Combat Viewmodel")
class BREACHPOINT_API UBRVM_Combat : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// -----------------------------------------------------------------------
	// Feed: the ASC. Called by whoever owns the pawn/PlayerState lifecycle.
	// -----------------------------------------------------------------------

	/**
	 * Subscribe to an ASC's attributes and state tags, and immediately publish current values so
	 * the HUD is correct on the frame it appears rather than on the next damage event.
	 *
	 * Null ASC, or Bindings with nothing valid in it, is NOT an error - it leaves the feed
	 * Unknown, which is the honest answer while a late joiner's ASC has not run
	 * InitAbilityActorInfo. Call again when it has.
	 */
	void BindToAbilitySystem(UAbilitySystemComponent* InASC, const FBRCombatAttributeBindings& InBindings);

	/**
	 * Unsubscribe. Values are KEPT and the state drops to Stale, because a dead player's last
	 * known health is information (the death cam shows it) while a zeroed bar is a lie.
	 */
	void UnbindFromAbilitySystem();

	// -----------------------------------------------------------------------
	// Feed: equipment. Pushed from Weapons/ RepNotify - never pulled from here.
	// -----------------------------------------------------------------------

	void SetAmmo(int32 InMagazine, int32 InReserve);
	void SetWeaponNames(const FText& InActiveWeapon, const FText& InStowedWeapon);
	void SetGrenadeCount(int32 InCount);

	/**
	 * Grapple cooldown. Deliberately NOT a per-frame remaining-time float.
	 *
	 * The WBP plays a ring animation of length InDurationSeconds when GrappleCooldownStarted
	 * fires, and snaps to ready when GrappleReady fires. That is one event, one animation, zero
	 * per-frame work - as opposed to a float the widget would have to sample every frame, which
	 * is the property-binding anti-pattern the rung-2 grep gate exists to catch.
	 */
	void SetGrappleCooldownStarted(float InDurationSeconds);
	void SetGrappleReady();

	// -----------------------------------------------------------------------
	// Feed: hit confirmation. Called from the damage GameplayCue on the shooting client.
	// -----------------------------------------------------------------------

	/**
	 * Shield-vs-flesh hit markers are GAMEPLAY INFORMATION (founder directive, BP10). Each kind
	 * fires its own FMVVMEventField so the WBP maps event -> animation with no branching.
	 * EBRHitMarkerKind::None is ignored.
	 */
	void ReportHitMarker(EBRHitMarkerKind InKind);

	/** C++ subscribers (UBRHUDLayout). Symmetric with RemoveAll(this) on deactivate. */
	FBROnHitMarker& OnHitMarker() { return OnHitMarkerEvent; }

	// -----------------------------------------------------------------------
	// Lifecycle
	// -----------------------------------------------------------------------

	/** Return every field to Unknown. Called on travel and on losing the local pawn. */
	void ClearToUnknown();

	//~ UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	// -----------------------------------------------------------------------
	// Getters (the binding surface; there are deliberately no public setters for fields)
	// -----------------------------------------------------------------------

	EBRUIDataState GetVitalsState() const { return VitalsState; }
	EBRUIDataState GetEquipmentState() const { return EquipmentState; }

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetHealthPercent() const { return HealthPercent; }
	float GetShields() const { return Shields; }
	float GetMaxShields() const { return MaxShields; }
	float GetShieldPercent() const { return ShieldPercent; }
	bool AreShieldsBroken() const { return bShieldsBroken; }

	int32 GetMagazineAmmo() const { return MagazineAmmo; }
	int32 GetReserveAmmo() const { return ReserveAmmo; }
	FText GetActiveWeaponName() const { return ActiveWeaponName; }
	FText GetStowedWeaponName() const { return StowedWeaponName; }
	int32 GetGrenadeCount() const { return GrenadeCount; }

	float GetGrappleCooldownDuration() const { return GrappleCooldownDuration; }
	bool IsGrappleReady() const { return bGrappleReady; }

public:
	// -----------------------------------------------------------------------
	// Event fields. One per distinct piece of information; the WBP picks a visual.
	// -----------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Category = "Breachpoint|Combat|Hit Markers")
	FMVVMEventField ShieldHitConfirmed;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Category = "Breachpoint|Combat|Hit Markers")
	FMVVMEventField FleshHitConfirmed;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Category = "Breachpoint|Combat|Hit Markers")
	FMVVMEventField HeadshotHitConfirmed;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Category = "Breachpoint|Combat|Hit Markers")
	FMVVMEventField KillConfirmed;

	/** Shields just broke on the local player. The audio signature is a gameplay requirement. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Category = "Breachpoint|Combat")
	FMVVMEventField ShieldsBrokenEvent;

	/** Shields came back up. Paired signature with the break. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Category = "Breachpoint|Combat")
	FMVVMEventField ShieldsRestoredEvent;

	/** Grapple went on cooldown; GrappleCooldownDuration is already updated when this fires. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Category = "Breachpoint|Combat")
	FMVVMEventField GrappleCooldownStarted;

	/** Grapple is available again. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Category = "Breachpoint|Combat")
	FMVVMEventField GrappleReady;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void HandleShieldsBrokenTagChanged(const FGameplayTag Tag, int32 NewCount);

	void SetVitalsState(EBRUIDataState InState);
	void SetEquipmentState(EBRUIDataState InState);
	void RecomputeVitalRatios();
	void PublishCurrentAttributeValues();

	// -----------------------------------------------------------------------
	// Vitals
	// -----------------------------------------------------------------------

	/** Unknown until an ASC has actually reported. The WBP renders dashes while this is Unknown. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetVitalsState", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	EBRUIDataState VitalsState = EBRUIDataState::Unknown;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetHealth", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float Health = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMaxHealth", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float MaxHealth = 0.0f;

	/** Precomputed so the WBP does no arithmetic. 0 when MaxHealth is not known yet. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetHealthPercent", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float HealthPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetShields", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float Shields = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMaxShields", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float MaxShields = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetShieldPercent", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float ShieldPercent = 0.0f;

	/**
	 * Mirrors the State.Shields.Broken GE tag (gas-purity: state is a tag, never a bool on an
	 * actor). This bool is a PROJECTION of that tag for binding convenience, not a second source
	 * of truth - nothing writes it except HandleShieldsBrokenTagChanged.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "AreShieldsBroken", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	bool bShieldsBroken = false;

	// -----------------------------------------------------------------------
	// Equipment
	// -----------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetEquipmentState", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	EBRUIDataState EquipmentState = EBRUIDataState::Unknown;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMagazineAmmo", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	int32 MagazineAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetReserveAmmo", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	int32 ReserveAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetActiveWeaponName", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	FText ActiveWeaponName;

	/** The ghosted second weapon in the bottom-right block. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetStowedWeaponName", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	FText StowedWeaponName;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetGrenadeCount", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	int32 GrenadeCount = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetGrappleCooldownDuration", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float GrappleCooldownDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "IsGrappleReady", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	bool bGrappleReady = true;

	// -----------------------------------------------------------------------
	// Subscription bookkeeping. Every handle here has exactly one release site.
	// -----------------------------------------------------------------------

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	/** Reflected because FGameplayAttribute holds a TObjectPtr to the owning attribute-set class. */
	UPROPERTY(Transient)
	FBRCombatAttributeBindings Bindings;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle ShieldsChangedHandle;
	FDelegateHandle MaxShieldsChangedHandle;
	FDelegateHandle ShieldsBrokenTagHandle;

	FBROnHitMarker OnHitMarkerEvent;
};

// ===========================================================================
// UBRVM_Match
// ===========================================================================

/**
 * The shared match feed: clock, team scores, killfeed, rocket countdown, phase.
 *
 * Fed by GameState RepNotify (pushed in, never pulled) plus one self-rescheduling timer for the
 * clock. Everything a scoreboard or a carnage report needs to be honest across three views
 * (server, acting client, observing client) comes through here.
 */
UCLASS(BlueprintType, DisplayName = "BR Match Viewmodel")
class BREACHPOINT_API UBRVM_Match : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// -----------------------------------------------------------------------
	// Feed: the clock
	// -----------------------------------------------------------------------

	/**
	 * The time source. Must be the local GameState; the ViewModel calls
	 * AGameStateBase::GetServerWorldTimeSeconds() on it, which is the engine's own
	 * client-corrected clock and the only number that agrees across all three views.
	 */
	void SetTimeSource(AGameStateBase* InGameState);

	/**
	 * THE match clock input: ONE replicated float, the server world time at which the match ends.
	 *
	 * A replicated ticking counter would be a bandwidth finding AND would desync across the three
	 * views (each client would render its own quantised copy). The remaining time is derived
	 * locally; the only thing that crosses the wire is this end time, once, on change.
	 *
	 * Pass a non-positive value to mean "no clock" (pre-match, post-match).
	 */
	void SetMatchEndServerTime(float InServerTimeSeconds);

	/** Same contract as the match clock, for the power-weapon spawner. */
	void SetRocketSpawnServerTime(float InServerTimeSeconds);

	/** The rocket is on the ground and takeable. Clears the countdown. */
	void SetRocketAvailable(bool bInAvailable);

	// -----------------------------------------------------------------------
	// Feed: scores and phase
	// -----------------------------------------------------------------------

	void SetTeamScores(int32 InTeam0Score, int32 InTeam1Score);

	/** 255 == unknown. Team assignment arrives AFTER GameState on a join-in-progress client. */
	void SetLocalTeamId(uint8 InTeamId);

	/**
	 * Match phase as a gameplay tag, pushed by Match/.
	 *
	 * contract_gap: BREACHPOINT-ARCHITECTURE.md 3.1 enumerates no `Match.Phase.*` leaves, so
	 * this stays an empty tag until 3.1 is amended. The UI treats an empty tag as "phase
	 * unknown" and shows nothing rather than guessing - it does not invent leaf tags inline.
	 */
	void SetMatchPhaseTag(FGameplayTag InPhaseTag);

	// -----------------------------------------------------------------------
	// Feed: killfeed
	// -----------------------------------------------------------------------

	/**
	 * Append one row. The authoritative ring buffer lives on GameState; this is the client-side
	 * window over it.
	 *
	 * When the window is full the OLDEST VISIBLE ROW IS DROPPED AND THE DROP IS LOGGED
	 * (LogBRUI, Warning). A silent cap reads as "covered everything" when it did not - that is
	 * the honesty law, and it is also the only way the critic's "killfeed pool exhaustion" pass
	 * can find anything.
	 */
	void PushKillfeedEntry(const FBRKillfeedViewEntry& InEntry);

	/**
	 * Attach a Spotter line to an already-visible row. No-op if the row has expired, which is the
	 * correct behaviour: the LLM is allowed to be late and the HUD is not allowed to wait.
	 */
	void AppendSpotterLine(int32 InSequenceId, const FText& InLine);

	void ClearKillfeed();

	const TArray<FBRKillfeedViewEntry>& GetKillfeedEntries() const { return KillfeedEntries; }

	FBROnKillfeedEntryAdded& OnKillfeedEntryAdded() { return OnKillfeedEntryAddedEvent; }
	FBROnKillfeedChanged& OnKillfeedChanged() { return OnKillfeedChangedEvent; }

	// -----------------------------------------------------------------------
	// Lifecycle
	// -----------------------------------------------------------------------

	void ClearToUnknown();

	//~ UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	// -----------------------------------------------------------------------
	// Getters
	// -----------------------------------------------------------------------

	EBRUIDataState GetMatchState() const { return MatchState; }
	int32 GetSecondsRemaining() const { return SecondsRemaining; }
	FText GetMatchClockText() const { return MatchClockText; }
	bool IsClockRunning() const { return bClockRunning; }
	int32 GetTeam0Score() const { return Team0Score; }
	int32 GetTeam1Score() const { return Team1Score; }
	uint8 GetLocalTeamId() const { return LocalTeamId; }
	FGameplayTag GetMatchPhaseTag() const { return MatchPhaseTag; }
	int32 GetRocketSecondsRemaining() const { return RocketSecondsRemaining; }
	FText GetRocketCountdownText() const { return RocketCountdownText; }
	bool IsRocketAvailable() const { return bRocketAvailable; }

private:
	/** Recompute both clocks from the one time source, publish changes, and re-arm. */
	void UpdateClocks();

	/**
	 * Arm a ONE-SHOT timer for the fraction of a second until the next whole-second boundary,
	 * then re-arm from UpdateClocks. One callback per displayed second, aligned to the tick the
	 * player actually sees change - not a 10 Hz poll and emphatically not NativeTick.
	 * Does nothing when no clock is running.
	 */
	void ScheduleNextClockUpdate();
	void StopClockUpdates();

	void ExpireKillfeedEntries();
	void ScheduleKillfeedExpiry();
	void PublishKillfeedChanged();

	UWorld* GetTimerWorld() const;
	static FText FormatClock(int32 InSeconds);

	// -----------------------------------------------------------------------
	// Match
	// -----------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchState", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	EBRUIDataState MatchState = EBRUIDataState::Unknown;

	/** INDEX_NONE == no clock. The WBP shows MatchClockText, never this, but bots and tests read it. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetSecondsRemaining", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	int32 SecondsRemaining = INDEX_NONE;

	/** Preformatted "M:SS", or the honest placeholder when there is no clock. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchClockText", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	FText MatchClockText;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "IsClockRunning", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	bool bClockRunning = false;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetTeam0Score", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	int32 Team0Score = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetTeam1Score", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	int32 Team1Score = 0;

	/** 255 until team assignment replicates. Until then the WBP must not colour either score. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetLocalTeamId", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	uint8 LocalTeamId = 255;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchPhaseTag", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	FGameplayTag MatchPhaseTag;

	// -----------------------------------------------------------------------
	// Rocket
	// -----------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetRocketSecondsRemaining", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	int32 RocketSecondsRemaining = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetRocketCountdownText", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	FText RocketCountdownText;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "IsRocketAvailable", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	bool bRocketAvailable = false;

	// -----------------------------------------------------------------------
	// Killfeed
	// -----------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetKillfeedEntries", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	TArray<FBRKillfeedViewEntry> KillfeedEntries;

	/** Fires whenever KillfeedEntries changed for ANY reason (append, expiry, Spotter append). */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	FMVVMEventField KillfeedChanged;

	/** Parallel to KillfeedEntries: world-time at which each row expires. Not reflected; not state. */
	TArray<double> KillfeedExpiryTimes;

	// -----------------------------------------------------------------------
	// Time base
	// -----------------------------------------------------------------------

	UPROPERTY(Transient)
	TWeakObjectPtr<AGameStateBase> TimeSource;

	/** Server world time at which the match ends. <= 0 means no clock. Not replicated FROM here. */
	float MatchEndServerTime = 0.0f;

	float RocketSpawnServerTime = 0.0f;

	FTimerHandle ClockTimerHandle;
	FTimerHandle KillfeedExpiryTimerHandle;

	int32 NextKillfeedSequenceFallback = 0;

	FBROnKillfeedEntryAdded OnKillfeedEntryAddedEvent;
	FBROnKillfeedChanged OnKillfeedChangedEvent;
};
