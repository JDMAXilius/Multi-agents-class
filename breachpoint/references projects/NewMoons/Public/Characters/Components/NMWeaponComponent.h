#pragma once

#include "CoreMinimal.h"
#include "InventorySystem/Weapon.h"
#include "Components/ActorComponent.h"
#include "NMWeaponComponent.generated.h"

class ANMWeaponActor;
class ANMCharacterBase;
class ANMProjectileBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCurrentWeaponChangeDelegate, ANMWeaponActor*, NewWeapon);

UCLASS(ClassGroup=(NM), meta=(BlueprintSpawnableComponent), Blueprintable)
class NEWMOONS_API UNMWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UNMWeaponComponent();
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon | Data")
	TObjectPtr<UDataTable> WeaponDataTable;
	
	UFUNCTION(BlueprintPure, Category = "Weapon")
	static UNMWeaponComponent* FindWeaponComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UNMWeaponComponent>() : nullptr); }

	ANMCharacterBase* GetOwningCharacter();
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	FORCEINLINE ANMWeaponActor* GetEquippedWeapon() const { return EquippedWeapon; }

	UFUNCTION(BlueprintCallable, BlueprintPure)
	FORCEINLINE bool IsWeaponEquipped() const { return EquippedWeapon ? true : false; }

	/** Fire the current weapon, if any is equipped */
	void FireWeapon();

	// Server function to handle firing (server-side only)
	UFUNCTION(Server, Reliable)
	void ServerFireWeapon();

	/** Fire the current weapon, if any is equipped */
	void StopFireWeapon();
	
	// Server function to handle firing (server-side only)
	UFUNCTION(Server, Reliable)
	void ServerStopFireWeapon();

	/** Takes a weapon and attaches it to the mesh */
	void EquipWeapon(const FName WeaponName, const int32 WeaponIndex, USkeletalMeshComponent* Mesh, const FName& BoneSocket);

	/** Server Takes a weapon and attaches it to the mesh */
	UFUNCTION(Server, Reliable)
	void ServerEquipWeapon(const FName WeaponName, const int32 WeaponIndex, USkeletalMeshComponent* Mesh, const FName& BoneSocket);

	/** Unequip current weapon, if equipped */
	void UnequipWeapon();
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Weapon | Data")
	int32 BagAmmo = 999; // Infinite ammo in the bag
	
	/** [server] add ammo */
	UFUNCTION(BlueprintAuthorityOnly, Category = "Weapon | Data")
	void GiveAmmo( const int32 Amount);
	
	/** consume a bullet */
	UFUNCTION(BlueprintAuthorityOnly, Category = "Weapon | Data")
	void UseAmmo( const int32 Amount);
	
	//UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void ReloadWeapon();

	UFUNCTION(Server, Reliable)
	void ServerReloadWeapon();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayAnimation(UAnimMontage* AnimMontage, const bool InEquippedWeapon);
	
	UPROPERTY(BlueprintAssignable)
	FCurrentWeaponChangeDelegate CurrentWeaponChanged;
	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	FHitResult GetProjectileTraceResult();
	void ChangeFireMode();

	// Function to throw the grenade
	UFUNCTION(BlueprintCallable, Category = "Weapon | Combat")
	void ThrowGrenade();
	UFUNCTION(Server, Reliable)
	void ServerThrowGrenade();
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon | Character Visuals")
	TObjectPtr<UAnimMontage> ThrowGrenadeMontage;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void DestroyComponent(bool bPromoteChildren) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_CurrentWeapon(const ANMWeaponActor* OldWeapon);

	UFUNCTION()
	void OnRep_WeaponInventory() const;

	bool bIsReloading = false;  // Track reloading state
	
	FTimerHandle ReloadTimerHandle;  // Timer to finish reload
	
	// Class reference for spawning the grenade
	UPROPERTY(EditDefaultsOnly, Category = "Weapon | Combat")
	TSubclassOf<ANMProjectileBase> GrenadeClass;

	// Force to apply when throwing the grenade
	UPROPERTY(EditDefaultsOnly, Category = "Weapon | Combat")
	float GrenadeThrowSpeed = 2500.0f;

private:
	/** The index of the current equipped weapon */
	UPROPERTY(Replicated)
	int32 CurrentWeaponIndex;
	/** Currently equipped Weapon */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentWeapon, Category = "Weapon | Data", meta = (AllowPrivateAccess = "true"))
	ANMWeaponActor* EquippedWeapon;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_WeaponInventory, Category = "Weapon | Data", meta = (AllowPrivateAccess = "true"))
	TArray<ANMWeaponActor*> EquippedWeapons;
	/** Set this in Blueprints for the default Weapon class */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon | Data", meta = (AllowPrivateAccess = "true", EditFixedOrder, NoElementDuplicate))
	TArray<TSubclassOf<ANMWeaponActor>> WeaponsClasses;
	
	TWeakObjectPtr<ANMCharacterBase> OwningCharacter;
	
	/** Spawns a default weapon */
	ANMWeaponActor* SpawnWeapon(const int32 WeaponIndex);
	void PreviewGrenadePath();
};
