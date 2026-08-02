// Breachpoint. Weapons lying in the world, and the node that puts the Rocket there.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"

#include "BRWeaponPickup.generated.h"

class APawn;
class ABRWeaponPickup;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;
struct FBRWeaponRow;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE_TwoParams(FBROnWeaponPickupCollected, ABRWeaponPickup*, APawn*);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FBROnWeaponPickupAttractRequested,
	ABRWeaponPickup*, const FVector&, AActor*);

UCLASS()
class BREACHPOINT_API ABRWeaponPickup : public AActor
{
	GENERATED_BODY()

public:
	ABRWeaponPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static ABRWeaponPickup* SpawnDroppedWeapon(
		UWorld* World,
		TSubclassOf<ABRWeaponPickup> PickupClass,
		const FDataTableRowHandle& InWeaponRow,
		int32 InAmmoInMag,
		int32 InAmmoReserve,
		const FTransform& SpawnTransform,
		AActor* InInstigatorActor);

	void InitializePickup(const FDataTableRowHandle& InWeaponRow, int32 InAmmoInMag, int32 InAmmoReserve);

	const FDataTableRowHandle& GetWeaponRowHandle() const { return WeaponRow; }
	const FBRWeaponRow* GetRow() const;
	int32 GetAmmoInMag() const { return AmmoInMag; }
	int32 GetAmmoReserve() const { return AmmoReserve; }
	bool IsConsumed() const { return bConsumed; }

	bool CanBeCollectedBy(APawn* Collector) const;

	bool TryCollect(APawn* Collector);

	FBROnWeaponPickupCollected OnCollected;

	virtual bool AttractTo(const FVector& InTargetLocation, AActor* InRequester);

	virtual void CancelAttract();

	bool IsBeingAttracted() const { return bAttracting; }
	const FVector& GetAttractTargetLocation() const { return AttractTargetLocation; }
	AActor* GetAttractRequester() const { return AttractRequester; }

	FBROnWeaponPickupAttractRequested OnAttractRequested;

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnRep_WeaponRow();

	void RequestMeshLoad();
	void OnMeshLoaded();

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Pickup", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float InteractionRadiusCm = 120.f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_WeaponRow)
	FDataTableRowHandle WeaponRow;

	UPROPERTY(Replicated)
	int32 AmmoInMag = 0;

	UPROPERTY(Replicated)
	int32 AmmoReserve = 0;

	UPROPERTY(Replicated)
	bool bConsumed = false;

	UPROPERTY(Replicated)
	bool bAttracting = false;

	UPROPERTY(Replicated)
	FVector AttractTargetLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> AttractRequester;

	TSet<TWeakObjectPtr<APawn>> OverlappingPawns;

	TSharedPtr<FStreamableHandle> MeshLoadHandle;
};

UCLASS()
class BREACHPOINT_API ABRPowerWeaponSpawner : public AActor
{
	GENERATED_BODY()

public:
	ABRPowerWeaponSpawner();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ArmSpawner();

	void DisarmSpawner();

	bool IsPickupAvailable() const;

	float GetSecondsUntilRespawn() const;

	float GetRespawnAvailableServerTime() const { return RespawnAvailableServerTime; }

protected:
	void SpawnPickup();
	void HandlePickupCollected(ABRWeaponPickup* Pickup, APawn* Collector);

	float GetServerTimeSeconds() const;

	UFUNCTION()
	void OnRep_RespawnAvailableServerTime();

	UPROPERTY(EditInstanceOnly, Category = "Power Weapon")
	FDataTableRowHandle WeaponRow;

	UPROPERTY(EditInstanceOnly, Category = "Power Weapon")
	TSubclassOf<ABRWeaponPickup> PickupClass;

	UPROPERTY(EditInstanceOnly, Category = "Power Weapon", meta = (ForceUnits = "s"))
	float RespawnIntervalSeconds = -1.f;

	UPROPERTY(EditInstanceOnly, Category = "Power Weapon")
	bool bArmOnBeginPlay = true;

private:
	UPROPERTY(Replicated)
	TObjectPtr<ABRWeaponPickup> CurrentPickup;

	UPROPERTY(ReplicatedUsing = OnRep_RespawnAvailableServerTime)
	float RespawnAvailableServerTime = -1.f;

	UPROPERTY(Replicated)
	bool bArmed = false;

	FTimerHandle RespawnTimerHandle;
};
