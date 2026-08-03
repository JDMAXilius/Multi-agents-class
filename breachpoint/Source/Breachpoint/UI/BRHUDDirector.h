#pragma once

#include "Engine/EngineTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "BRHUDDirector.generated.h"

class ABRGameState;
class APawn;
class APlayerState;
class UBREquipmentComponent;
class UBRVM_Combat;
class UBRVM_Match;
class UBRWeaponInstance;
class UWorld;
struct FBRKillFeedEntry;
struct FOnAttributeChangeData;

/**
 * `UBRHUDDirector` -- THE PRODUCER LAYER (HUD-CPP-AUDIT H1). Before this class, 19 of the 21
 * ViewModel feeds had zero callers and nothing ever called `ShowHUD`: the HUD was a fully
 * wired consumer bolted to a socket with no wire in it. This is the wire.
 *
 * WHAT IT IS. A `ULocalPlayerSubsystem`, so it exists once per local player with no
 * PlayerController edit and is split-screen-correct by construction. It reads game state
 * through the delegates the game already publishes and PUSHES into the two ViewModels the
 * `UBRUIManagerSubsystem` owns for its local player. One direction, always: nothing here
 * writes game state, sends an RPC, or decides anything (CLAUDE.md law 1).
 *
 * WHY THE PRODUCER IS CHEAP TO TRUST. `ABRGameState` is symmetric on host and client — every
 * `Server*` mutator invokes its own `OnRep_` locally — so the four match wires below fire
 * identically on a listen server and on a remote client. The audit's biggest mercy.
 *
 * WHAT IT WIRES TODAY:
 *   GameState  -> SetTimeSource · clock · both team scores · killfeed projection
 *   Possession -> ASC attribute bindings (the four vitals) · grenade count
 *   Equipment  -> weapon display names (active + stowed) · ammo (mag / reserve)
 *   Travel     -> PostLoadMapWithWorld: ClearToUnknown x2, then rebind against the new world
 *                 (HUD-CPP-AUDIT H4 — without this, frozen scores and the old map's kills
 *                 survived into the new match)
 *   Boot       -> CreateLayoutForLocalPlayer + ShowHUD on the first possession in a world
 *                 with an `ABRGameState`
 *
 * CONTRACT GAPS, FILED NOT FAKED. Six signals have no game-side event and are therefore not
 * wired: local team id (nothing on `ABRPlayerState` exposes one), match-phase TAGS (the enum
 * exists, the tag vocabulary does not), grapple cooldown start/ready, rocket spawn/available,
 * hit-marker confirmation, the spotter line. Each keeps its ViewModel feed and waits for its
 * producer; inventing any of them here would be a UI class deciding gameplay facts.
 *
 * KILLFEED IDS. The projection always carries the server-assigned `FBRKillFeedEntry::Sequence`
 * (HUD-CPP-AUDIT H5) — the VM's old per-machine fallback id is gone, and a spotter line can
 * only ever address the same row on every machine.
 */
UCLASS()
class BREACHPOINT_API UBRHUDDirector : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// -- lifecycle --------------------------------------------------------------------------
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void TryBindWorld();
	void HandleGameStateSet(class AGameStateBase* NewGameState);
	void BindGameState(ABRGameState* GameState);
	void UnbindWorld();

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
	void BindPawn(APawn* Pawn);
	void UnbindPawn();

	// -- match wires ------------------------------------------------------------------------
	void HandleMatchClockChanged(float NewEndServerTime);
	void HandleTeamScoreChanged(uint8 TeamId, int32 NewScore);
	void HandleKillFeedEntryAdded(const FBRKillFeedEntry& Entry);
	void PushCurrentMatchState();

	// -- combat wires -----------------------------------------------------------------------
	void HandleEquippedWeaponChanged(UBREquipmentComponent* Equipment, UBRWeaponInstance* NewWeapon);
	void HandleAmmoChanged(UBRWeaponInstance* Weapon);
	void HandleGrenadesChanged(const FOnAttributeChangeData& Data);
	void PushWeaponState(UBREquipmentComponent* Equipment);

	// -- plumbing ---------------------------------------------------------------------------
	UBRVM_Combat* GetCombatViewModel() const;
	UBRVM_Match* GetMatchViewModel() const;
	void EnsureHUDShown();

	UPROPERTY(Transient)
	TWeakObjectPtr<ABRGameState> BoundGameState;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBREquipmentComponent> BoundEquipment;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBRWeaponInstance> BoundWeapon;

	/** The ASC the grenade delegate was added to; teardown targets THIS object (H9's rule). */
	TWeakObjectPtr<class UAbilitySystemComponent> BoundASC;

	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle GameStateSetHandle;
	FDelegateHandle GrenadesChangedHandle;

	bool bHUDShown = false;
};
