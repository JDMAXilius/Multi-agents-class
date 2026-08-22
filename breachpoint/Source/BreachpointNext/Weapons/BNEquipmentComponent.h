#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySystem/BNAbilitySet.h"
#include "BNEquipmentComponent.generated.h"

class ABNWeapon;
class UBNAbilitySystemComponent;

/** R7 — the hand changed. Broadcast at the tail of ApplyCurrentWeapon, which already runs on
 *  EVERY machine (authority calls plus both OnReps) — so one delegate is multiplayer-correct by
 *  construction, the same free ride OnEquipped/OnUnequipped take. Current may be null (the
 *  Unarmed slot). Re-broadcasts on a re-apply are harmless: ViewModel setters no-op on equal. */
class UBNEquipmentComponent;
DECLARE_MULTICAST_DELEGATE_TwoParams(FBNEquippedWeaponSignature, UBNEquipmentComponent*, ABNWeapon* /*Current*/);

/**
 * The carried set and the current index, on ABNCharacter. The server spawns, attaches, grants
 * and swaps; every machine derives visibility and the linked anim layer from CurrentIndex.
 */
UCLASS(Config = Game)
class BREACHPOINTNEXT_API UBNEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBNEquipmentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Authority only: spawns the StartupWeaponRows, attaches them to the mesh, equips slot 0. */
	void InitializeCarriedWeapons();

	// Authority-side state changes; clients follow through OnRep_CurrentIndex.
	void EquipNext();
	void EquipPrevious();
	void EquipIndex(int32 NewIndex);

	ABNWeapon* GetCurrentWeapon() const;

	/** R7.3 — WHAT ONE SWAP PRESS GIVES YOU, which is the only honest reading of "stowed" in a
	 *  five-slot carry (the design's chassis holds two). Shares GetNextIndex with EquipNext
	 *  deliberately: a HUD that promises a weapon the swap does not deliver is worse than no
	 *  stowed slot at all.
	 *
	 *  NULL IS TWO DIFFERENT ANSWERS, which is why HasNextSlot exists beside this: the carry
	 *  holds a real UNARMED slot as a null entry (Unarmed → Pistol → Rifle → Shotgun → Knife), so
	 *  a null next weapon can mean "the next press empties your hands" — a fact worth printing —
	 *  or "there is nowhere to swap to" on a single-slot carry. Ask HasNextSlot first. */
	ABNWeapon* GetNextWeapon() const;

	/** True when a DISTINCT next slot exists, weapon or the unarmed one. False on a carry of one. */
	bool HasNextSlot() const;

	FBNEquippedWeaponSignature OnEquippedWeaponChanged;

	/** The index EquipNext would move to, or INDEX_NONE with nothing carried. */
	int32 GetNextIndex() const;

	/** The current weapon's row layer, or null — which the character reads as unarmed. */
	UClass* GetCurrentWeaponAnimLayer() const;

protected:
	UFUNCTION()
	void OnRep_Weapons();

	UFUNCTION()
	void OnRep_CurrentIndex();

	/** Every machine: one weapon visible, the rest hidden, and the character re-links its layer. */
	void ApplyCurrentWeapon();

	/** Authority only: revoke the previous weapon's set, grant the current one's. */
	void UpdateGrantedAbilitySet();

	/** Authority only, and always through GrantedASC — see the comment on the definition. */
	void RevokeGrantedAbilitySet();

	UBNAbilitySystemComponent* GetAbilitySystem() const;

	// Row names, not classes — the row is the weapon's identity. Set in DefaultGame.ini,
	// the same Config path ABNPlayerController takes its input assets through.
	UPROPERTY(Config)
	TArray<FName> StartupWeaponRows;

	// RepNotify rather than a bare Replicated: on a late joiner these actor references can
	// arrive unresolved, and the replication system re-fires the notify when the GUIDs resolve.
	// Without it the array would land null and nothing would ever re-apply.
	UPROPERTY(ReplicatedUsing = OnRep_Weapons)
	TArray<TObjectPtr<ABNWeapon>> Weapons;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentIndex)
	int32 CurrentIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UBNAbilitySet> GrantedSet;

	FBNAbilitySetHandles GrantedHandles;

	/** The ASC the grant actually landed on. EndPlay cannot re-derive it: APawn::UnPossessed()
	 *  nulls PlayerState before the corpse is destroyed. */
	TWeakObjectPtr<UBNAbilitySystemComponent> GrantedASC;
};
