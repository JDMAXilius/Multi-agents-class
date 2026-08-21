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

protected:
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void BindToWorld(UWorld* World);
	void HandleGameStateSet(AGameStateBase* InGameState);
	void BindGameState(ABNGameState* InGameState);

	void HandleMatchStateChanged(FName NewState);
	void HandleKillfeedChanged();
	void HandleScoreChanged(ABNPlayerState* ChangedPlayerState);
	void HandleRespawnStampChanged(ABNPlayerState* OwnPlayerState);
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleEquippedWeaponChanged(UBNEquipmentComponent* Equipment, ABNWeapon* Current);
	void HandleAmmoChanged(ABNWeapon* Weapon);

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	/** Finds the controller and binds everything player-scoped that is not yet bound. Safe to
	 *  call from every edge — every bind inside is guarded or idempotent. */
	void EnsurePlayerBindings();
	void BindPawn(APawn* Pawn);
	void UnbindWeapon();

	/** Pushes the HUD once both halves exist: a BN GameState and an owning controller. */
	void EnsureHUDShown();

	/** One decision point for the scoreboard: post-match pins it, Tab holds it. */
	void UpdateScoreboardVisibility();
	void ShowDeathScreen(bool bShow);

	/** Everything match-shaped, pushed fresh: phase + winner banner, clock, scores. */
	void PushMatchSnapshot();
	void RecomputeScores();
	FText ComposeKillfeedLine(const struct FBNKillfeedEntry& Entry) const;

	UBNVM_Combat* GetCombatVM() const;
	UBNVM_Match* GetMatchVM() const;
	APlayerController* GetOwnPlayerController() const;

	void UnbindAll();

	// ---- bound objects and their handles (unbind against THE STORED OBJECT — H9) ----
	TWeakObjectPtr<ABNGameState> BoundGameState;
	FDelegateHandle MatchStateHandle;
	FDelegateHandle KillfeedHandle;

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

	// ---- screens this director put up (weak: the stack owns them) ----
	TWeakObjectPtr<UBNActivatableWidget> HUDWidget;
	TWeakObjectPtr<UBNActivatableWidget> DeathScreenWidget;
	TWeakObjectPtr<UBNActivatableWidget> ScoreboardWidget;

	bool bScoreboardHeld = false;
	bool bPostMatch = false;
};
