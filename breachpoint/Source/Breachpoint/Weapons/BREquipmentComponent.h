// Breachpoint. What a player is carrying, which of it is in their hands, and how it got there.

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayAbilitySpecHandle.h"
#include "Templates/SubclassOf.h"

#include "BREquipmentComponent.generated.h"

class ABRWeaponPickup;
class APawn;
class FOutBunch;
class UAbilitySystemComponent;
class UActorChannel;
class UBRAbilitySet;
class UBRWeaponInstance;
class UStaticMeshComponent;
struct FBRWeaponRow;
struct FStreamableHandle;

/** Which hand-slot a weapon occupies. Two, and the count is the enum, not a literal 2. */
UENUM(BlueprintType)
enum class EBRWeaponSlot : uint8
{
	Primary,
	Secondary,

	Count UMETA(Hidden)
};

/** Fired whenever the weapon in hand changes on this machine (server write or replicated change). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBROnEquippedWeaponChanged,
	class UBREquipmentComponent* /*Component*/, UBRWeaponInstance* /*NewWeapon*/);

/**
 * Handles produced by granting one weapon's ability set, kept so the grant can be undone
 * exactly on unequip.
 *
 * Deliberately NOT BP02's handle struct: this header does not include AbilitySystem/, so the
 * two packets cannot break each other's compile in the same wave. Undoing a grant needs only
 * stock GAS calls (ClearAbility / RemoveActiveGameplayEffect), so nothing is lost by owning
 * the storage here.
 */
struct FBRWeaponAbilityGrant
{
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	TArray<FActiveGameplayEffectHandle> EffectHandles;

	bool IsEmpty() const { return AbilityHandles.IsEmpty() && EffectHandles.IsEmpty(); }
	void Reset()
	{
		AbilityHandles.Reset();
		EffectHandles.Reset();
	}
};

/**
 * UBREquipmentComponent — the two-slot loadout on a player pawn.
 *
 * Specification: ARCHITECTURE §3.5 — "two-slot replicated subobjects; equip = grant weapon's
 * BRAbilitySet + async-load soft mesh; pickup/drop RPC server-validated".
 *
 * THE TRUST MODEL, in one sentence: a client may ASK to swap, pick up, or drop; the server
 * checks the request against its own state and decides; the result replicates. There is no
 * path by which a client's message writes a slot.
 *
 * Every `_Validate` below checks something real about the WIRE value (a slot index that is
 * out of range, a null pickup) and nothing else — gameplay legality (is the slot occupied?
 * is the pickup consumed? is the pawn dead?) is decided in the _Implementation with server
 * state, because that is the only place it can be decided honestly. An
 * `_Validate { return true; }` in this file is a defect, not a shortcut.
 *
 * REPLICATION SPLIT (ARCHITECTURE §6.1):
 *   - slots + active index: COND_None. Everyone must see which gun is in whose hands.
 *   - the weapon subobjects themselves: registered COND_None for the same reason.
 *   - AMMO inside those subobjects: COND_OwnerOnly, enforced in UBRWeaponInstance.
 * That split is the whole point: the world sees the weapon, only its owner sees the magazine.
 *
 * NO TICK (law 4): overlap delegates, RepNotifies, async-load callbacks, timers.
 */
UCLASS(ClassGroup = (Breachpoint), meta = (BlueprintSpawnableComponent))
class BREACHPOINT_API UBREquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBREquipmentComponent();

	//~ Begin UActorComponent interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ReadyForReplication() override;
	/**
	 * The LEGACY subobject path, implemented on purpose alongside the registered list.
	 * The engine picks between them by the OWNING ACTOR's bReplicateUsingRegisteredSubObjectList
	 * flag, which lives in Character/ and is not mine to set (contract_gap filed). They are
	 * mutually exclusive by construction — the engine never runs both — so implementing both
	 * means the weapons replicate whichever way the pawn ends up configured, instead of
	 * silently replicating nothing.
	 */
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	// -- Queries (any machine) -----------------------------------------------------------------

	UBRWeaponInstance* GetWeaponInSlot(EBRWeaponSlot Slot) const;
	UBRWeaponInstance* GetActiveWeapon() const;
	int32 GetActiveSlotIndex() const { return ActiveSlotIndex; }
	bool IsSlotOccupied(EBRWeaponSlot Slot) const;

	/** Fired after the weapon in hand changes, on every machine. Cosmetic/UI consumers only. */
	FBROnEquippedWeaponChanged OnEquippedWeaponChanged;

	// -- Server authority ----------------------------------------------------------------------

	/**
	 * Server-only. Creates a weapon in Slot from a DT_Weapons row, filled per the row.
	 * Replaces (destroys) whatever occupied the slot — call DropWeapon first if the previous
	 * weapon should survive on the ground.
	 *
	 * @return the new instance, or nullptr when the row is missing (nothing is created).
	 */
	UBRWeaponInstance* GiveWeapon(const FDataTableRowHandle& InWeaponRow, EBRWeaponSlot Slot);

	/**
	 * Server-only. As GiveWeapon, but preserving the exact ammunition a pickup carried.
	 * The instance clamps the magazine to the row's MagSize; reserve is clamped at zero.
	 */
	UBRWeaponInstance* GiveWeaponWithAmmo(const FDataTableRowHandle& InWeaponRow, EBRWeaponSlot Slot,
		int32 InAmmoInMag, int32 InAmmoReserve);

	/**
	 * Server-only. Makes Slot the weapon in hand: revokes the old slot's ability set, grants
	 * the new one, and re-points the mesh.
	 *
	 * @return true when the active slot changed. False for an empty slot or a no-op swap —
	 *         both legal, neither an error.
	 */
	bool SetActiveSlot(EBRWeaponSlot Slot);

	/**
	 * Server-only. Removes the weapon in Slot and leaves it on the ground with the ammunition
	 * it had. Returns the spawned pickup, or nullptr when the slot was empty or no pickup
	 * class is configured.
	 */
	ABRWeaponPickup* DropWeapon(EBRWeaponSlot Slot);

	// -- Client intent (server-validated) ------------------------------------------------------

	/**
	 * "I pressed swap." Validation: the slot index must be a real slot — that is a fact about
	 * the WIRE value and is checked in _Validate. Whether the swap is legal (slot occupied,
	 * pawn alive) is server state and is checked in the implementation.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestSwapSlot(uint8 SlotIndex);

	/**
	 * "I want that pickup." The server asks the PICKUP whether this pawn may have it —
	 * against the server's own overlap set, never against a position the client sent.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestPickup(ABRWeaponPickup* Pickup);

	/** "Drop what is in slot N." Same validation split as the swap. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestDropSlot(uint8 SlotIndex);

protected:
	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_ActiveSlotIndex();

	/**
	 * Runs on every machine after the weapon in hand changes. Server side additionally moves
	 * the ability grant. Idempotent on purpose: Slots and ActiveSlotIndex are separate
	 * properties and may arrive in either order, so both RepNotifies call this.
	 */
	void HandleActiveWeaponChanged();

	// -- Ability set (BP02 seam) ---------------------------------------------------------------

	/**
	 * BP02 SEAM — grants the active weapon's ability set on the server.
	 *
	 * THE EXPECTED SURFACE, which BP03 will call from THIS function and nowhere else:
	 *
	 *     void UBRAbilitySet::GiveToAbilitySystem(
	 *             UAbilitySystemComponent* ASC,
	 *             UObject* SourceObject,
	 *             TArray<FGameplayAbilitySpecHandle>& OutAbilityHandles,
	 *             TArray<FActiveGameplayEffectHandle>& OutEffectHandles) const;
	 *
	 * (If BP02 shipped the Lyra shape instead — a `FBRAbilitySetGrantedHandles*` out-param —
	 * only this one function body changes; no header of mine moves, because the handle storage
	 * is FBRWeaponAbilityGrant, which I own.)
	 *
	 * SourceObject is the UBRWeaponInstance, so BRGA_WeaponFire can reach its row and ammo
	 * through the spec without a second lookup.
	 *
	 * Step 1 does NOT include AbilitySystem/BRAbilitySet.h: BP02 is authoring that file in
	 * this same wave, and guessing at an include is how one packet breaks four builders' tree.
	 */
	void GrantAbilitySetForSlot(int32 SlotIndex);

	/** Server-only. Undoes GrantAbilitySetForSlot with stock GAS calls only. */
	void ClearAbilitySetForSlot(int32 SlotIndex);

	/**
	 * CONTRACT_GAP (BP03 step 1) — which ability set belongs to which weapon has NO data home.
	 * DT_Weapons.csv (the source of truth, landed by BP13) has no AbilitySet column, and this
	 * packet may not invent one. Until a column lands this returns nullptr and says so once
	 * per row: equipping works, the mesh appears, and the weapon cannot fire — a loud, obvious
	 * hole rather than a plausible-looking one.
	 */
	const UBRAbilitySet* ResolveAbilitySetForRow(const FDataTableRowHandle& InWeaponRow) const;

	/** The owner's ASC (PlayerState-owned, per §3.6). Null before possession completes. */
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;

	APawn* GetOwnerPawn() const;

	// -- Cosmetic mesh -------------------------------------------------------------------------

	/** Async-loads the active weapon's SOFT mesh ref and attaches it. Skipped on a dedicated server. */
	void RefreshEquippedMesh();
	void EnsureWeaponMeshComponent();

	/**
	 * Socket the weapon mesh attaches to on the owner's skeletal mesh.
	 * PLACEHOLDER pending the character rig (BP04 owns Character/); a socket name is an asset
	 * fact, not a tuning number, so it stays a property rather than becoming a table column.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	FName WeaponAttachSocket = TEXT("GripPoint");

	/**
	 * What a dropped weapon becomes. Defaults to ABRWeaponPickup in the constructor — a C++
	 * class reference, not an asset reference.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TSubclassOf<ABRWeaponPickup> DroppedPickupClass;

private:
	/** True when this machine is the server for the owning actor. */
	bool HasServerAuthority() const;

	/** INDEX_NONE for anything that is not a real slot. */
	static int32 ToSlotIndex(EBRWeaponSlot Slot);
	static bool IsValidSlotIndex(int32 SlotIndex);

	/** Server-only: destroys the instance in a slot and unregisters it from replication. */
	void DestroyWeaponInSlot(int32 SlotIndex);

	/**
	 * The two slots. Fixed length = EBRWeaponSlot::Count, never resized at runtime, so an
	 * index is stable across the wire.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<TObjectPtr<UBRWeaponInstance>> Slots;

	/** Which slot is in hand; INDEX_NONE = empty hands. */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveSlotIndex)
	int32 ActiveSlotIndex = INDEX_NONE;

	/** Server-only, one per slot. Not replicated: grants are authority state. */
	TArray<FBRWeaponAbilityGrant> SlotGrants;

	/** Cosmetic, created on demand on any machine that renders. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> EquippedMeshComponent;

	/**
	 * Bumped on every equip. An async mesh load that completes after a later equip compares
	 * generations and drops itself — otherwise a slow load from the previous weapon lands on
	 * the new one, which reads as "the swap sometimes shows the wrong gun".
	 */
	uint32 EquipGeneration = 0;

	TSharedPtr<FStreamableHandle> MeshLoadHandle;
};
