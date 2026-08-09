// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "ZoransGameState.h"
#include "Engine/Public/Subsystems/WorldSubsystem.h"
#include "ZoransWorldSubsystem.generated.h"

class AZoransWeaponPickupBase;
class AZoransControlPointBase;
class AZoransBeaconCapturePointBase;
class AZoransAmmoPickupBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameStateReadyDelegate, const AZoransGameState*, ZoransGameState);

UCLASS()
class ZORANSRESISTANCE_API UZoransWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	void SetGameState(const AZoransGameState* InGameState);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	AZoransGameState* GetZoransGameState() const { return ZoransGameState.Get(); }

	UPROPERTY(BlueprintAssignable)
	FOnGameStateReadyDelegate OnGameStateReadyDelegate;

	UFUNCTION(BlueprintCallable)
	void RegisterWeaponPickup(AZoransWeaponPickupBase* InWeaponPickup);

	UFUNCTION(BlueprintCallable)
	void RevalidateWeaponPickups();

	UFUNCTION(BlueprintCallable)
	void RemoveWeaponPickup(AZoransWeaponPickupBase* WeaponPickup);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<AZoransWeaponPickupBase*> GetRegisteredWeaponPickups() const { return RegisteredWeaponPickups; }
	
	UFUNCTION(BlueprintCallable)
	void RegisterHealthPickup(AActor* InHealthPickup);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<AActor*> GetRegisteredHealthPickups() const { return RegisteredHealthPickups; }
	
	UFUNCTION(BlueprintCallable)
	void RegisterControlPoint(AZoransControlPointBase* InControlPoint);
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<AZoransControlPointBase*> GetRegisteredControlPoints() const { return RegisteredControlPoints; }
	
	UFUNCTION(BlueprintCallable)
	void SetControllerInvertVertical(const bool bInverted = false);

	UPROPERTY(BlueprintReadOnly)
	bool bControllerInvertedVertical = false;

	UFUNCTION(BlueprintCallable)
	void RegisterBeaconCapturePointBase(AZoransBeaconCapturePointBase* InBeaconCapturePointBase);
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<AZoransBeaconCapturePointBase*> GetRegisteredBeaconCapturePointBase() const { return RegisteredBeaconCapturePointBase; }

	UFUNCTION(BlueprintCallable)
	void RegisterBeaconPickups(AZoransWeaponPickupBase* InBeaconPickup);

	UFUNCTION(BlueprintCallable)
	void RevalidateBeaconPickups();

	UFUNCTION(BlueprintCallable)
	void RemoveBeaconPickups(AZoransWeaponPickupBase* BeaconPickup);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<AZoransWeaponPickupBase*> GetRegisteredBeaconPickups() const { return RegistereBeaconPickups; }

	UFUNCTION(BlueprintCallable)
	void RegisterBeaconPickupsDrop(AZoransWeaponPickupBase* InBeaconPickupDrop);

	UFUNCTION(BlueprintCallable)
	void RevalidateBeaconPickupsDrop();

	UFUNCTION(BlueprintCallable)
	void RemoveBeaconPickupsDrop(AZoransWeaponPickupBase* BeaconPickupDrop);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<AZoransWeaponPickupBase*> GetRegisteredBeaconPickupsDrop() const { return RegistereBeaconPickupsDrop; }

	UFUNCTION(BlueprintCallable)
	void RegisterIntrestPoint(AActor* InIntrestPoint);
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<AActor*> GetRegisteredIntrestPoints() const { return RegisteredIntrestPoints; }

	UFUNCTION(BlueprintCallable)
	void RegisterAmmoPickup(AZoransAmmoPickupBase* InAmmoPickup);

	//UFUNCTION(BlueprintCallable)
	//void RevalidateAmmoPickups();

	UFUNCTION(BlueprintCallable)
	void RemoveAmmoPickup(AZoransAmmoPickupBase* AmmoPickup);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<AZoransAmmoPickupBase*> GetRegisteredAmmoPickups() const { return RegisteredAmmoPickups; }
	
protected:
	
	TSoftObjectPtr<AZoransGameState> ZoransGameState = nullptr;

	UPROPERTY()
	TArray<AZoransWeaponPickupBase*> RegisteredWeaponPickups;

	UPROPERTY()
	TArray<AActor*> RegisteredHealthPickups;
	
	UPROPERTY()
	TArray<AZoransControlPointBase*> RegisteredControlPoints;

	UPROPERTY()
	TArray<AZoransBeaconCapturePointBase*> RegisteredBeaconCapturePointBase;

	UPROPERTY()
	TArray<AZoransWeaponPickupBase*> RegistereBeaconPickups;

	UPROPERTY()
	TArray<AZoransWeaponPickupBase*> RegistereBeaconPickupsDrop;

	UPROPERTY()
	TArray<AActor*> RegisteredIntrestPoints;

	UPROPERTY()
	TArray<AZoransAmmoPickupBase*> RegisteredAmmoPickups;
};