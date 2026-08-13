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

	/** The weapon's own muzzle socket; the actor transform when the row or socket is absent. */
	FTransform GetMuzzleTransform() const;

	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

protected:
	UFUNCTION()
	void OnRep_RowName();

	void ApplyRow();

	UPROPERTY(ReplicatedUsing = OnRep_RowName)
	FName RowName;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(Transient)
	TObjectPtr<UClass> CachedAnimLayerClass;
};
