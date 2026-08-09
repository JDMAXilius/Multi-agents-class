// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "ZoransResistance/InventorySystem/Weapons/Data/ZoransWeaponData.h"
#include "ZoransWeaponComponent.generated.h"

class UZoransAbilitySystemComponent;
class UDataTable;
class AZoransWeaponActor;
class AZoransCharacterBase;
class AZoransPlayerState;
class UChildActorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWeaponInventoryChangeDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCurrentWeaponSlotChangeDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLightAmmoChangeDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMediumAmmoChangeDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHeavyAmmoChangeDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FShotgunAmmoChangeDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSniperAmmoChangeDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExplosiveSmallAmmoChangeDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExplosiveLargeAmmoChangeDelegate);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), Blueprintable)
class ZORANSRESISTANCE_API UZoransWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZoransWeaponComponent();

	AZoransCharacterBase* GetOwningCharacter() const { return OwningCharacter.Get(); }
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapons")
	void InitPlayerLoadout();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon Data")
	TObjectPtr<UDataTable> WeaponDataTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon Data")
	TObjectPtr<UDataTable> WeaponSkinDataTable;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon Data")
	TArray<FName> DefaultWeaponInventory = {"Unarmed", "Unarmed", "Unarmed"};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon Data")
	int32 LightAmmoCapacity = 240;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon Data")
	int32 MediumAmmoCapacity = 180;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon Data")
	int32 HeavyAmmoCapacity = 400;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon Data")
	int32 ShotgunAmmoCapacity = 60;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon Data")
	int32 SniperAmmoCapacity = 32;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon Data")
	int32 ExplosiveSmallAmmoCapacity = 32;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon Data")
	int32 ExplosiveLargeAmmoCapacity = 8;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Weapon Data")
	int32 InventorySize = 3;

	UFUNCTION(BlueprintAuthorityOnly, Category = "Ammo")
	void GiveAmmo(const EAmmoType AmmoType, const int32 Amount);

	UFUNCTION(BlueprintAuthorityOnly, Category = "Ammo")
	bool GiveAmmoForCurrentWeapon(const float Ratio = 0.5f);

	UFUNCTION()
	void Internal_SetWeaponVisibility(FGameplayTag GameplayTag, int TagCount) const;
	
	void InitWeaponComponent(AActor* InOwningActor);
	
	void InitNewWeaponData(const int32 InSlot);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void SetWeaponSlot(int32 InSlot);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	bool TryPickupWeapon(FName WeaponName, bool bInitialSpawn = false);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void TryPickupDroppedWeapon(FWeaponInfo WeaponInfo, bool& bWasSuccessful);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void EquipNewWeaponInSlot(FName WeaponName, int32 InSlot);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void EquipDroppedWeaponInSlot(FWeaponInfo WeaponInfo, int32 InSlot);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapons")
	AZoransWeaponActor* GetCurrentEquippedWeapon() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapons")
	bool HasObjectiveWeaponEquipped() const;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, ReplicatedUsing = "OnRep_WeaponInventory")
	TArray<FWeaponInfo> EquippedWeapons;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_CurrentWeaponSlot")
	int32 CurrentWeaponSlot = -1;

	UPROPERTY(BlueprintReadOnly)
	int32 ObjectiveWeaponSlot = 2;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_LightAmmo")
	int32 LightAmmo = 0;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_MediumAmmo")
	int32 MediumAmmo = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_HeavyAmmo")
	int32 HeavyAmmo = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_ShotgunAmmo")
	int32 ShotgunAmmo = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_SniperAmmo")
	int32 SniperAmmo = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_ExplosiveSmallAmmo")
	int32 ExplosiveSmallAmmo = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_ExplosiveLargeAmmo")
	int32 ExplosiveLargeAmmo = 0;

	UPROPERTY(BlueprintAssignable)
	FWeaponInventoryChangeDelegate WeaponInventoryChanged;

	UPROPERTY(BlueprintAssignable)
	FCurrentWeaponSlotChangeDelegate CurrentWeaponSlotChanged;

	UPROPERTY(BlueprintAssignable)
	FLightAmmoChangeDelegate LightAmmoChanged;

	UPROPERTY(BlueprintAssignable)
	FMediumAmmoChangeDelegate MediumAmmoChanged;

	UPROPERTY(BlueprintAssignable)
	FHeavyAmmoChangeDelegate HeavyAmmoChanged;

	UPROPERTY(BlueprintAssignable)
	FHeavyAmmoChangeDelegate ShotgunAmmoChanged;

	UPROPERTY(BlueprintAssignable)
	FHeavyAmmoChangeDelegate SniperAmmoChanged;

	UPROPERTY(BlueprintAssignable)
	FHeavyAmmoChangeDelegate ExplosiveSmallAmmoChanged;

	UPROPERTY(BlueprintAssignable)
	FHeavyAmmoChangeDelegate ExplosiveLargeAmmoChanged;
	
	UFUNCTION(BlueprintCallable)
	void CheckCanReloadCurrentWeapon(bool& bCanReload);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void TryReloadCurrentWeapon(bool& bWasSuccessful);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void TryUseLoadedAmmo(int32 Amount, bool& bWasSuccessful);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	AZoransWeaponActor* GetWeaponActor(const int32 InSlot) const;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool SwapWeaponOnPickup = true;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void RemoveWeaponInSlot(int32 InSlot);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void RemoveAllEquippedWeapons();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool TrySetWeaponSlot(const int32 InSlot);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	int32 GetNextWeaponSlot(const bool bIncrease) const;

	UFUNCTION(Server, Reliable)
	void InternalSetWeaponSlot(const int32 InWeaponSlot);

	bool IsComponentInitialized() const { return bIsInitialized; }

	AZoransPlayerState* GetOwningPlayerState() { return ZoransPlayerState.Get(); }
	
protected:

	bool bIsInitialized = false;
	
	void RemoveCurrentWeaponData();
	
	UFUNCTION()
	void OnRep_WeaponInventory() const;

	UFUNCTION()
	void OnRep_CurrentWeaponSlot();

	UFUNCTION()
	void OnRep_LightAmmo() const;
	
	UFUNCTION()
	void OnRep_MediumAmmo() const;

	UFUNCTION()
	void OnRep_HeavyAmmo() const;

	UFUNCTION()
	void OnRep_ShotgunAmmo() const;

	UFUNCTION()
	void OnRep_SniperAmmo() const;

	UFUNCTION()
	void OnRep_ExplosiveSmallAmmo() const;

	UFUNCTION()
	void OnRep_ExplosiveLargeAmmo() const;
	
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<AZoransCharacterBase> OwningCharacter;
	TWeakObjectPtr<AZoransPlayerState> ZoransPlayerState;
	
	TArray<FActiveGameplayEffectHandle> CurrentWeaponEffects;
	
	FGameplayAbilitySpecHandle PrimaryWeaponAbilitySpec;
	FGameplayAbilitySpecHandle SecondaryWeaponAbilitySpec;
	TArray<FGameplayAbilitySpecHandle> AddedAbilitySpecHandles;
	
	virtual void DestroyComponent(bool bPromoteChildren) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};