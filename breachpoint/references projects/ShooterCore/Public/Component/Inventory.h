
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory.generated.h"

class APlayerCharacter;
class ABagpackBase;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SHOOTERCORE_API UInventory : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventory();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

public:
	//Weapon Data
	bool PickupAmmoRifle(int Amount);
	bool PickupAmmoPistol(int Amount);
	bool PickupAmmoShotgun(int Amount);
	void ReloadingAmmoRifle(int ClipSize);
	void ReloadingAmmoPistol(int ClipSize);
	void ReloadingAmmoShotgun(int ClipSize);

	//Health Data
	bool PickupBandage(int Amount);
	bool PickupEnergyDrink(int Amount);
	void UseBandage();
	void UseEnergyDrink();

	//Grenade Data
	bool PickupGrenade(int Amount);
	void UseGrenade();

	//Bagpack Data
	bool PickupBagpack(ABagpackBase* Bagpack);


public:
	//Weapon Data
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|WeaponData")
	int RemainingRifleAmmo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|WeaponData")
	int RemainingPistolAmmo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|WeaponData")
	int RemainingShotgunAmmo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|WeaponData")
	int MaximumRifleAmmo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|WeaponData")
	int MaximumPistolAmmo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|WeaponData")
	int MaximumShotgunAmmo;

	//Health Data
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|HealthData")
	int RemainingBandage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|HealthData")
	int RemainingEnergyDrink;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|HealthData")
	int MaximumBandage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|HealthData")
	int MaximumEnergyDrink;

	//Grenade Data
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|HealthData")
	int RemainingGrenade;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|HealthData")
	int MaximumGrenade;

	//Bagpack Data
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Inventory|BagpackData")
	bool bHasBagpack;

private:
	APlayerCharacter* Player;

};

