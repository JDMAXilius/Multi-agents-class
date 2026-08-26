#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "UI/BNUITypes.h"
#include "BNHUDDirector.generated.h"

class ABNGameState;
class ABNPlayerState;
class ABNWeapon;
class APawn;
class APlayerController;
class UAbilitySystemComponent;
class UBNActivatableWidget;
class UBNEquipmentComponent;
class UBNVM_Combat;
class UTexture2D;
class UBNVM_Match;
class UWorld;

/**
 * THE PRODUCER — the one file in UI/ that knows gameplay types, and the one direction data
 * flows: game events in, ViewModel writes out. A ULocalPlayerSubsystem, so split-screen gets
 * one per player with no PlayerController edit, and it self-initializes — nothing calls it.
 *
 * The wiring stands on a property R4 built without knowing it: every BN feed fires its delegate
 * on the AUTHORITY by hand (OnReps don't run there), so this producer behaves identically on a
 * listen host and a remote client — one body, three views.
 *
 * PLAYER-ACQUISITION EDGES: the PlayerController can trail the world on a joining client, so
 * player bindings are (re)attempted from every early edge we own — post-load-map, GameState
 * arrival, and the first match-state broadcast (the initial replication bunch fires it). The
 * compiled reference lived on the same edges. Once the controller is found, possession events
 * drive every rebind.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNHUDDirector : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Wave 4's seam, landed with the spine: the controller's Tab handler calls this — a UI
	 *  verb through the director, never a widget poking layers itself. */
	void SetScoreboardHeld(bool bHeld);

	/** R7.2 — the controller's Esc handler. OPEN only: once the menu owns Menu input, a game
	 *  action is not a dependable way back, so the menu closes ITSELF (Resume, or its own key
	 *  handler). This only raises the REQUEST; UpdateGameMenuLayer decides what the layer shows. */
	void OpenPauseMenu();

	/** The pause screen tells its owner it is gone (its NativeOnDeactivated) — self-closed by
	 *  Resume, by Escape, or removed from under it. Clearing the request here is what stops a
	 *  later layer update from resurrecting a menu nobody asked for. */
	void NotifyPauseClosed();

protected:
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void BindToWorld(UWorld* World);
	void HandleGameStateSet(AGameStateBase* InGameState);
	void BindGameState(ABNGameState* InGameState);

	void HandleMatchStateChanged(FName NewState);
	void HandleKillfeedChanged();
	void HandleScoreChanged(ABNPlayerState* ChangedPlayerState);

	/** TEAMS (BN16): the GameState team ledger moved (either team — the VM gets both
	 *  numbers fresh, relative to the reader). */
	void HandleTeamScoreChanged(uint8 Team);

	/** TEAMS (BN16): ANY subscribed PlayerState's TeamId changed. This is the deferred
	 *  subscription that closes the replication race (BR prior art): on a joining client a
	 *  PlayerState can arrive frames before its TeamId, so relations composed at roster
	 *  build are honestly None until this fires and rebuilds them. */
	void HandleAnyTeamChanged(ABNPlayerState* ChangedPlayerState);
	void HandleRespawnStampChanged(ABNPlayerState* OwnPlayerState);
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleEquippedWeaponChanged(UBNEquipmentComponent* Equipment, ABNWeapon* Current);
	void HandleAmmoChanged(ABNWeapon* Weapon);

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	/** Finds the controller and binds everything player-scoped that is not yet bound. Safe to
	 *  call from every edge — every bind inside is guarded or idempotent. */
	void EnsurePlayerBindings();

	/** The MISSING acquisition edge (bn-critic BN16 F1): a joiner during post-match gets no
	 *  kill, no possession and no state change after the initial bunch, so a controller or
	 *  PlayerState that trails those edges would strand the HUD unbound forever. A bounded
	 *  one-shot retry re-runs EnsurePlayerBindings until it binds; it re-arms only while
	 *  the vacuum persists and only in a world with a match to render. */
	void ArmPlayerAcquisitionRetry();
	void BindPawn(APawn* Pawn);
	void UnbindWeapon();

	/** Pushes the HUD once both halves exist: a BN GameState and an owning controller. */
	void EnsureHUDShown();

	/** One decision point for the scoreboard: post-match pins it, Tab holds it. */
	void UpdateScoreboardVisibility();

	/** ONE owner for Layer.GameMenu (critic, blocking): the death screen and the pause menu
	 *  share that stack, and two independent owners produced a real bug — dying while paused
	 *  BURIED the pause widget (a stack deactivates all but its top, it is not removed), so
	 *  IsActivated() read false, a second Esc pushed a SECOND menu, and popping the death
	 *  screen on respawn re-activated the buried one: a menu appearing on a player who never
	 *  opened it. Both callers now only set intent; this decides. Death outranks pause. */
	void SetDeathScreenWanted(bool bWanted);
	void UpdateGameMenuLayer();

	/** R7.3 — the kill's CAUSE, resolved for the two audiences that read it differently: the feed
	 *  wants a glyph (empty when the cause has no row or the row has no Icon), the death screen
	 *  wants words (the row's DisplayName, else the source name itself — so "Melee" reads as
	 *  Melee with no second mapping table, and a Grenade row added to DT_BNWeapons later starts
	 *  drawing its own name and icon with no code change here). */
	const struct FBNWeaponRow* FindWeaponRow(FName RowName) const;
	TSoftObjectPtr<UTexture2D> ResolveWeaponIcon(FName RowName) const;
	FText ComposeWeaponLabel(FName RowName) const;

	/** Everything match-shaped, pushed fresh: phase + winner banner, clock, scores. */
	void PushMatchSnapshot();
	void RecomputeScores();

	/** TEAMS (BN16): who Other is to the local player, from the two PlayerStates' TeamIds.
	 *  Self beats team facts; either side NoTeam answers None (FFA, or the joining
	 *  client's honest-unknown frame — never guessed at). The ONE composer: killfeed
	 *  parts, roster rows and the team ledger all relate through here. */
	EBNUITeamRelation RelationTo(const ABNPlayerState* Other) const;

	/** TEAMS (BN16): the deferred-subscription sweep — called from RecomputeScores' roster
	 *  walk, subscribing OnTeamChanged on every ABNPlayerState not yet held (idempotent;
	 *  stale weak keys dropped in passing; UnbindAll clears the lot). The roster walk is
	 *  the one place every PlayerState already passes through, so late joiners are picked
	 *  up on the same edges the roster itself is. */
	void EnsureTeamSubscriptions();
	FText ComposeKillfeedLine(const struct FBNKillfeedRingEntry& Entry) const;

	UBNVM_Combat* GetCombatVM() const;
	UBNVM_Match* GetMatchVM() const;
	APlayerController* GetOwnPlayerController() const;

	void UnbindAll();

	// ---- bound objects and their handles (unbind against THE STORED OBJECT — H9) ----
	TWeakObjectPtr<ABNGameState> BoundGameState;
	FDelegateHandle MatchStateHandle;
	FDelegateHandle KillfeedHandle;
	FDelegateHandle TeamScoreHandle;

	/** TEAMS (BN16): one OnTeamChanged handle per PlayerState the roster has seen — the
	 *  deferred subscriptions. Weak keys: a leaver's entry goes stale and is swept by
	 *  EnsureTeamSubscriptions; UnbindAll removes against the stored objects (H9). */
	TMap<TWeakObjectPtr<ABNPlayerState>, FDelegateHandle> TeamChangedHandles;

	TWeakObjectPtr<ABNPlayerState> BoundPlayerState;
	FDelegateHandle ScoreHandle;
	FDelegateHandle RespawnStampHandle;

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle DeadTagHandle;

	TWeakObjectPtr<UBNEquipmentComponent> BoundEquipment;
	FDelegateHandle EquippedHandle;

	TWeakObjectPtr<ABNWeapon> BoundWeapon;
	FDelegateHandle AmmoHandle;

	TWeakObjectPtr<APlayerController> BoundController;
	FDelegateHandle PostLoadMapHandle;

	/** Stored so UnbindAll can remove it (critic, H9): PIE creates the player before
	 *  PostLoadMapWithWorld broadcasts for the same world, and an unstored handle would stack a
	 *  second live subscription on the double bind. */
	TWeakObjectPtr<UWorld> BoundWorld;
	FDelegateHandle GameStateSetHandle;

	// ---- screens this director put up (weak: the stack owns them) ----
	TWeakObjectPtr<UBNActivatableWidget> HUDWidget;
	TWeakObjectPtr<UBNActivatableWidget> DeathScreenWidget;
	TWeakObjectPtr<UBNActivatableWidget> ScoreboardWidget;
	TWeakObjectPtr<UBNActivatableWidget> PauseWidget;

	bool bScoreboardHeld = false;
	bool bPostMatch = false;

	/** See ArmPlayerAcquisitionRetry. Cleared with the world's own teardown (the timer
	 *  manager dies with the world; UnbindAll also clears it by hand for the rebind path). */
	FTimerHandle PlayerAcquisitionRetryHandle;
	static constexpr float PlayerAcquisitionRetrySeconds = 0.5f;

	// ---- Layer.GameMenu intent (never read the widgets to decide — see UpdateGameMenuLayer) ----
	bool bDeathScreenWanted = false;
	bool bPauseRequested = false;
};
