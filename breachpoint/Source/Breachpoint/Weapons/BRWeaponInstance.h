// Breachpoint. One equipped weapon: the row it came from, and its ammunition.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/Object.h"

#include "BRWeaponInstance.generated.h"

struct FBRWeaponRow;

/** Fired on every ammo change, server and client alike, after the new value is in place. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBROnWeaponAmmoChanged, class UBRWeaponInstance* /*Instance*/);

/**
 * UBRWeaponInstance — the runtime object for ONE weapon a player is carrying.
 *
 * Specification: ARCHITECTURE §3.5 ("replicated UObject: row handle + ammo COND_OwnerOnly").
 * Lives as a replicated subobject of UBREquipmentComponent, which owns its lifetime and
 * registers/unregisters it with the replicated-subobject list.
 *
 * WHAT IT IS NOT: it is not an actor, it has no Tick, it holds no mesh, and it never applies
 * damage. It is the state a weapon carries between shots plus the pure arithmetic of an
 * ammunition transaction. Every rule below that decides a number is a static function taking
 * its inputs by value — that is what lets `Breachpoint.Sim.*` pin them with no world at all.
 *
 * AMMO IS A NAMED GAS EXCEPTION (`gas-purity.md` ledger). Ammo is per-WEAPON state, so it is
 * a property here rather than an attribute on the ASC; the bounds that keep the exception
 * honest are exactly three, and they are enforced by this class:
 *   1. mutated ONLY from BRGA_WeaponFire / BRGA_WeaponUtility (step 2),
 *   2. server-authoritative,
 *   3. COND_OwnerOnly replication IS the correction path for a mispredicted decrement.
 *
 * INFORMATION LEAK (netcode.md law 5, and BP03 step 5 re-checks it): another player's
 * magazine count tells you when to push. Both ammo properties are COND_OwnerOnly, so they
 * are absent from the replication layout of every connection that does not own this weapon's
 * actor — an enemy client cannot read them because they never arrive, not because the HUD
 * declines to draw them.
 */
UCLASS(BlueprintType)
class BREACHPOINT_API UBRWeaponInstance : public UObject
{
	GENERATED_BODY()

public:
	UBRWeaponInstance();

	//~ Begin UObject interface — replicated subobject plumbing.
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UWorld* GetWorld() const override;
	//~ End UObject interface

	// -- Identity --------------------------------------------------------------------------

	/**
	 * Server-only. Binds this instance to a DT_Weapons row and fills a full magazine plus the
	 * row's starting reserve. Call exactly once, immediately after NewObject.
	 */
	void InitializeFromRow(const FDataTableRowHandle& InWeaponRow);

	/**
	 * Server-only. Overrides the ammunition after InitializeFromRow — used when a weapon is
	 * picked up off the ground and must keep the ammo it was dropped with (a half-empty
	 * Rocket is the whole point of the power-weapon contest).
	 */
	void OverrideAmmo(int32 InAmmoInMag, int32 InAmmoReserve);

	const FDataTableRowHandle& GetWeaponRowHandle() const { return WeaponRow; }

	/** Row name (`AR`, `Magnum`, `Rocket`) — NAME_None when unbound. */
	FName GetWeaponRowName() const { return WeaponRow.RowName; }

	/**
	 * The row's data, or nullptr when the handle is unset or the row has been removed by a
	 * reimport. Callers MUST null-check: a missing row is a data defect that must surface as
	 * "the weapon refuses to act", never as a silent zero-damage shot.
	 */
	const FBRWeaponRow* GetRow() const;

	// -- Ammunition ------------------------------------------------------------------------

	int32 GetAmmoInMag() const { return AmmoInMag; }
	int32 GetAmmoReserve() const { return AmmoReserve; }

	/** Broadcast after any ammo change on this machine (server write or replicated correction). */
	FBROnWeaponAmmoChanged OnAmmoChanged;

	/**
	 * Spend one round for one shot.
	 *
	 * Callable on the server (authoritative) and on the owning client (prediction only —
	 * `gas-purity.md`'s ledger makes COND_OwnerOnly replication the correction). Any other
	 * caller is a bug and is refused with an ensure rather than quietly tolerated.
	 *
	 * @return true when a round was spent; false when the magazine was already empty, which
	 *         the fire ability must treat as "do not fire" (not "fire for free").
	 */
	bool ConsumeAmmoForShot();

	/**
	 * Move rounds from reserve into the magazine. Called on the
	 * `Event.Weapon.ReloadCommit` notify (ruling R17) — never at reload START, so a cancelled
	 * reload costs and refunds nothing.
	 *
	 * @return the number of rounds actually moved (0 is legal and means nothing changed).
	 */
	int32 CommitReload();

	// -- Pure rules (headless; pinned by Breachpoint.Sim.*) --------------------------------

	/**
	 * How many rounds a reload moves. Pure: same inputs, same output, no world, no clock.
	 *
	 * Invariants the spec suite pins:
	 *   - result >= 0 (a reload never removes rounds)
	 *   - result <= AmmoReserve (a reload never CREATES ammunition)
	 *   - AmmoInMag + result <= MagSize (a reload never overfills)
	 *   - total ammo before == total ammo after (conservation)
	 *   - a zero reserve (the Rocket, ruling R4) always yields 0 — reload is unreachable,
	 *     which is a design position, not an edge case to paper over.
	 *
	 * Out-of-range inputs (negative counts, MagSize <= 0) return 0: refuse, never invent.
	 */
	static int32 CalcReloadTransfer(int32 InMagSize, int32 InAmmoInMag, int32 InAmmoReserve);

	/** Starting ammunition for a freshly spawned weapon of this row. Pure. */
	static void CalcInitialAmmo(const FBRWeaponRow& Row, int32& OutAmmoInMag, int32& OutAmmoReserve);

	// -- Rate-gate seam (BP03 step 2, netcode-builder) --------------------------------------

	/**
	 * Server-side timestamp of the last accepted shot, in server world seconds; negative
	 * means "has never fired". NOT replicated and never read by a rule in this class — it is
	 * storage for step 2's rate check (`interval >= Row.GetMinShotIntervalSeconds() - tolerance`),
	 * which is where the comparison belongs. Kept here because "when did THIS weapon last
	 * fire" is per-weapon state, so a swap cannot launder a rate violation.
	 */
	double GetLastFireServerTimeSeconds() const { return LastFireServerTimeSeconds; }
	void SetLastFireServerTimeSeconds(double InServerTimeSeconds);

protected:
	UFUNCTION()
	void OnRep_Ammo();

	/**
	 * True when this machine may write ammo: the server always, and the owning client for
	 * prediction. Everyone else is refused.
	 */
	bool HasAmmoWriteRights() const;

	/** The actor whose channel replicates this subobject (the equipment component's owner). */
	AActor* GetOwningActor() const;

private:
	/**
	 * Which DT_Weapons row this weapon is. COND_None on purpose: every client needs it to
	 * pick the third-person mesh and the fire cue. It carries no private information — what
	 * gun an enemy is holding is visible in the world anyway.
	 */
	UPROPERTY(Replicated)
	FDataTableRowHandle WeaponRow;

	/** Rounds in the magazine. COND_OwnerOnly — see the class comment's leak note. */
	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	int32 AmmoInMag = 0;

	/** Rounds in reserve. COND_OwnerOnly — see the class comment's leak note. */
	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	int32 AmmoReserve = 0;

	/** See GetLastFireServerTimeSeconds. Server-only, deliberately not a UPROPERTY. */
	double LastFireServerTimeSeconds = -1.0;
};
