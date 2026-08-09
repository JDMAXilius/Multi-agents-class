
#include "Component/Inventory.h"
#include "Pickups/BagpackBase.h"
#include "Main/PlayerCharacter.h"

UInventory::UInventory()
{
	PrimaryComponentTick.bCanEverTick = true;

	MaximumRifleAmmo = 300;
	MaximumPistolAmmo = 400;
	MaximumShotgunAmmo = 60;
	RemainingRifleAmmo = 200;
	RemainingPistolAmmo = 300;
	RemainingShotgunAmmo = 50;

	MaximumBandage = 3;
	MaximumEnergyDrink = 5;
	RemainingBandage = 0;
	RemainingEnergyDrink = 0;

	MaximumGrenade = 7;
	RemainingGrenade = 0;

	bHasBagpack = false;
}

void UInventory::BeginPlay()
{
	Super::BeginPlay();
	Player = Cast<APlayerCharacter>(GetOwner());
}

bool UInventory::PickupAmmoRifle(int Amount)
{
	if (RemainingRifleAmmo < MaximumRifleAmmo)
	{
		RemainingRifleAmmo = FMath::Clamp(RemainingRifleAmmo + Amount, 0, MaximumRifleAmmo);
		return true;
	}
	return false;
}

bool UInventory::PickupAmmoPistol(int Amount)
{
	if (RemainingPistolAmmo < MaximumPistolAmmo)
	{
		RemainingPistolAmmo = FMath::Clamp(RemainingPistolAmmo + Amount, 0, MaximumPistolAmmo);
		return true;
	}
	return false;
}

bool UInventory::PickupAmmoShotgun(int Amount)
{
	if (RemainingShotgunAmmo < MaximumShotgunAmmo)
	{
		RemainingShotgunAmmo = FMath::Clamp(RemainingShotgunAmmo + Amount, 0, MaximumShotgunAmmo);
		return true;
	}
	return false;
}

void UInventory::ReloadingAmmoRifle(int ClipSize)
{
	RemainingRifleAmmo = FMath::Clamp(RemainingRifleAmmo - ClipSize, 0, MaximumRifleAmmo);
}

void UInventory::ReloadingAmmoPistol(int ClipSize)
{
	RemainingPistolAmmo = FMath::Clamp(RemainingPistolAmmo - ClipSize, 0, MaximumPistolAmmo);
}

void UInventory::ReloadingAmmoShotgun(int ClipSize)
{
	RemainingShotgunAmmo = FMath::Clamp(RemainingShotgunAmmo - ClipSize, 0, MaximumShotgunAmmo);
}

bool UInventory::PickupBandage(int Amount)
{
	if (RemainingBandage < MaximumBandage)
	{
		RemainingBandage = FMath::Clamp(RemainingBandage + Amount, 0, MaximumBandage);
		return true;
	}
	return false;
}

bool UInventory::PickupEnergyDrink(int Amount)
{
	if (RemainingEnergyDrink < MaximumEnergyDrink)
	{
		RemainingEnergyDrink = FMath::Clamp(RemainingEnergyDrink + Amount, 0, MaximumEnergyDrink);
		return true;
	}
	return false;
}

void UInventory::UseBandage()
{
	RemainingBandage = FMath::Clamp(RemainingBandage - 1, 0, MaximumBandage);
}

void UInventory::UseEnergyDrink()
{
	RemainingEnergyDrink = FMath::Clamp(RemainingEnergyDrink - 1, 0, MaximumEnergyDrink);
}

bool UInventory::PickupGrenade(int Amount)
{
	if (RemainingGrenade < MaximumGrenade)
	{
		RemainingGrenade = FMath::Clamp(RemainingGrenade + Amount, 0, MaximumGrenade);
		return true;
	}
	return false;
}

void UInventory::UseGrenade()
{
	RemainingGrenade = FMath::Clamp(RemainingGrenade - 1, 0, MaximumGrenade);
}

bool UInventory::PickupBagpack(ABagpackBase* Bagpack)
{
	if (bHasBagpack) return false;
	bHasBagpack = true;
	Bagpack->AttachToComponent(Player->GetMesh(), FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), Bagpack->BagpackSocket);
	// add function here to Upgrade Inventory space
	return true;
}

void UInventory::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}



