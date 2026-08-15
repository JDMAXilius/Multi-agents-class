#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BNWeapon.generated.h"

class USkeletalMeshComponent;
struct FBNWeaponRow;

/**
 * A carried weapon. RowName is the whole of its replicated identity — mesh, sockets, anim
 * layer and ability set are looked up from the row on every machine, never sent twice.
 * No firing, no damage, no trace: that is G4.
 */
UCLASS()
class BREACHPOINTNEXT_API ABNWeapon : public AActor
{
	GENERATED_BODY()

public:
	ABNWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority only; set before FinishSpawning so the weapon never exists without an identity. */
	void SetRowName(FName InRowName);
	FName GetRowName() const { return RowName; }

	/** Null on a miss — a missing row is an answer, not a failure. */
	const FBNWeaponRow* GetRow() const;

	/** Resolved once when the row is applied: the anim seam reads this every frame. */
	UClass* GetAnimLayerClass() const { return CachedAnimLayerClass; }

	/** The weapon's own muzzle socket; the actor transform when the row or socket is absent.
	 *  Virtual: a beam weapon with a moving emitter overrides one function, not the cue system. */
	virtual FTransform GetMuzzleTransform() const;

	/** Drawn / holstered, called from ApplyCurrentWeapon on EVERY machine (authority and both
	 *  OnReps) — so a subclass's cosmetic reaction is multiplayer-correct by construction with no
	 *  replication of its own. Empty in the base on purpose. */
	virtual void OnEquipped() {}
	virtual void OnUnequipped() {}

	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	// FOUNDER'S RULING, Wave 4: ammo lives on the WEAPON, not as an attribute. Each weapon keeps
	// its own magazine across swaps, but attributes live on the ASC and the ASC sits on the
	// PlayerState — one ASC, many weapons — so an attribute would need N attributes or a map and
	// does neither well. The GAS-purity ledger names ammo as its exception for exactly this reason.
	int32 GetCurrentAmmo() const { return CurrentAmmo; }
	int32 GetAmmoReserve() const { return AmmoReserve; }
	int32 GetMagazineSize() const;

	/** The reference's pure transfer (UBRWeaponInstance::CalcReloadTransfer): how many rounds move
	 *  reserve->magazine. Static and stateless because a pure function is checkable by eye. */
	static int32 CalcReloadTransfer(int32 MagSize, int32 InMag, int32 Reserve)
	{
		return FMath::Max(0, FMath::Min(MagSize - InMag, Reserve));
	}
	bool HasAmmo(int32 Count = 1) const { return Count > 0 && CurrentAmmo >= Count; }

	/** Authority only, both of them. A client decrement would be unrecoverable: property
	 *  replication only sends on CHANGE, so a guess the server never made is never corrected. */
	bool ConsumeAmmo(int32 Count);
	void Reload();

protected:
	UFUNCTION()
	void OnRep_RowName();

	UFUNCTION()
	void OnRep_CurrentAmmo();

	UFUNCTION()
	void OnRep_AmmoReserve();

	void ApplyRow();

	UPROPERTY(ReplicatedUsing = OnRep_RowName)
	FName RowName;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo)
	int32 CurrentAmmo = 0;

	/** Authority-written like CurrentAmmo, for the same unrecoverable-guess reason. */
	UPROPERTY(ReplicatedUsing = OnRep_AmmoReserve)
	int32 AmmoReserve = 0;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** Owner-only copy. Same asset as WeaponMesh; tagged FirstPerson so it rides the camera's
	 *  first-person pass with the character's 1P body. WeaponMesh is the world representation. */
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(Transient)
	TObjectPtr<UClass> CachedAnimLayerClass;
};
