// Copyright Zollpa LLC


#include "ZoransWorldSubsystem.h"
#include "ZoransGameState.h"
#include "ZoransResistance/InventorySystem/Interactables/ZoransWeaponPickupBase.h"

void UZoransWorldSubsystem::SetGameState(const AZoransGameState* InGameState)
{
	ensure(InGameState);

	ZoransGameState = InGameState;
	
	if (OnGameStateReadyDelegate.IsBound())
	{
		OnGameStateReadyDelegate.Broadcast(GetZoransGameState());
	}
}

void UZoransWorldSubsystem::RegisterWeaponPickup(AZoransWeaponPickupBase* InWeaponPickup)
{
	ensure(InWeaponPickup);

	RegisteredWeaponPickups.AddUnique(InWeaponPickup);
}

void UZoransWorldSubsystem::RevalidateWeaponPickups()
{
	RegisteredWeaponPickups.RemoveAll([](const AZoransWeaponPickupBase* WeaponPickup)
	{
		return !IsValid(WeaponPickup);
	});
}

void UZoransWorldSubsystem::RemoveWeaponPickup(AZoransWeaponPickupBase* WeaponPickup)
{
	RegisteredWeaponPickups.Remove(WeaponPickup);
}

void UZoransWorldSubsystem::RegisterHealthPickup(AActor* InHealthPickup)
{
	ensure(InHealthPickup);

	RegisteredHealthPickups.AddUnique(InHealthPickup);
}

void UZoransWorldSubsystem::RegisterControlPoint(AZoransControlPointBase* InControlPoint)
{
	ensure(InControlPoint);

	RegisteredControlPoints.AddUnique(InControlPoint);
}

void UZoransWorldSubsystem::SetControllerInvertVertical(const bool bInverted)
{
	bControllerInvertedVertical = bInverted;
}

void UZoransWorldSubsystem::RegisterBeaconCapturePointBase(AZoransBeaconCapturePointBase* InBeaconCapturePointBase)
{
	ensure(InBeaconCapturePointBase);

	RegisteredBeaconCapturePointBase.AddUnique(InBeaconCapturePointBase);
}

void UZoransWorldSubsystem::RegisterBeaconPickups(AZoransWeaponPickupBase* InBeaconPickup)
{
	ensure(InBeaconPickup);

	RegistereBeaconPickups.AddUnique(InBeaconPickup);
}

void UZoransWorldSubsystem::RevalidateBeaconPickups()
{
	RegistereBeaconPickups.RemoveAll([](const AZoransWeaponPickupBase* BeaconPickup)
	{
		return !IsValid(BeaconPickup);
	});
}

void UZoransWorldSubsystem::RemoveBeaconPickups(AZoransWeaponPickupBase* BeaconPickup)
{
	RegistereBeaconPickups.Remove(BeaconPickup);
}

void UZoransWorldSubsystem::RegisterBeaconPickupsDrop(AZoransWeaponPickupBase* InBeaconPickupDrop)
{
	ensure(InBeaconPickupDrop);

	RegistereBeaconPickupsDrop.AddUnique(InBeaconPickupDrop);
}

void UZoransWorldSubsystem::RevalidateBeaconPickupsDrop()
{
	RegistereBeaconPickupsDrop.RemoveAll([](const AZoransWeaponPickupBase* BeaconPickupDrop)
	{
		return !IsValid(BeaconPickupDrop);
	});
}

void UZoransWorldSubsystem::RemoveBeaconPickupsDrop(AZoransWeaponPickupBase* BeaconPickupDrop)
{
	RegistereBeaconPickupsDrop.Remove(BeaconPickupDrop);
}

void UZoransWorldSubsystem::RegisterIntrestPoint(AActor* InIntrestPoint)
{
	ensure(InIntrestPoint);

	RegisteredIntrestPoints.AddUnique(InIntrestPoint);
}

void UZoransWorldSubsystem::RegisterAmmoPickup(AZoransAmmoPickupBase* InAmmoPickup)
{
	ensure(InAmmoPickup);

	RegisteredAmmoPickups.AddUnique(InAmmoPickup);
}


void UZoransWorldSubsystem::RemoveAmmoPickup(AZoransAmmoPickupBase* AmmoPickup)
{
	RegisteredAmmoPickups.Remove(AmmoPickup);
}
