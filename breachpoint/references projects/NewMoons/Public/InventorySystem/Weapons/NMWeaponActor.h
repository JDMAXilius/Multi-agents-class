// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/WeaponData.h"
#include "GameFramework/Actor.h"
#include "NMWeaponActor.generated.h"

class USkeletalMeshComponent;
class ANMCharacterBase;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundCue;
class UAnimInstance;
class UAnimMontage;
class UDamageType;
class UCameraShakeBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadedAmmoChangeDelegate, int32, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadedAmmoEmpty);

UCLASS()
class NEWMOONS_API ANMWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ANMWeaponActor();
	
	virtual void PreInitializeComponents() override;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	FWeaponData GetWeaponDefaults() const { return WeaponDefaults; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SettWeaponDefaults(FWeaponData InWeaponDefaults) { WeaponDefaults = InWeaponDefaults; }
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	FORCEINLINE USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	UFUNCTION(BlueprintCallable, BlueprintPure)
	FORCEINLINE FText GetWeaponName() const { return WeaponName; }

	UFUNCTION(BlueprintCallable, BlueprintPure)
	FORCEINLINE UTexture2D* GetWeaponIcon() const { return WeaponIcon; }

	void SetOwningCharacter(ANMCharacterBase* InOwningCharacter);
	void OnOwnerDestroyed(AActor* DestroyedActor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	ANMCharacterBase* GetOwningCharacter();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	UAnimMontage* GetCharacterFireMontage() const { return WeaponDefaults.FireMontage; }

	UFUNCTION(BlueprintCallable, BlueprintPure)
	UAnimMontage* GetCharacterReloadMontage() const { return WeaponDefaults.ReloadMontage; }


	UFUNCTION(BlueprintCallable, Category = "Weapon")
	FTransform GetWeaponMuzzleTransform() const;

	UFUNCTION(BlueprintPure)
	FVector GetMuzzleLocation() const;
	UFUNCTION(BlueprintPure)
	FVector GetlasserLocation() const;
	UFUNCTION(BlueprintPure)
	FVector GetEjectBulletLocation() const;

	void Fire();
	void StartFire();
	void StopFire();
	void HandleBurstFire();
	void HandleAutomaticFire();

	UFUNCTION(BlueprintPure, Category = "Weapon | HitScan")
	FHitResult GetHitScanTraceResult();

	// Utility to calculate the spread direction based on the given rotation and spread angle
	FVector GetSpreadDirection(const FRotator& ViewRotation) const;

	UFUNCTION(BlueprintCallable, Category = "Weapon | HitScan")
	void PlayWeaponEffects(const FHitResult& Hits);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	void SetLoadedAmmo(const int32 InAmmo);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	int32 GetLoadedAmmo() { return LoadedAmmo; }

	UFUNCTION(BlueprintCallable, BlueprintPure)
	int32 GetMaxClipAmmo() const { return WeaponDefaults.MagazineCapacity; }

	bool CanFire() const;

	bool CanReload() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon | HitScan")
	void Reload(const int32& AmmoAmount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	bool ConsumeAmmo(int32 AmmoAmount = 1);

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnLoadedAmmoChangeDelegate OnLoadedAmmoChangeDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnLoadedAmmoEmpty OnLoadedAmmoEmpty;

	UFUNCTION()
	void OnRep_LoadedAmmo(const int32 OldValue);

	UPROPERTY()
	FWeaponData WeaponDefaults;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFire();

	void PlayFireEffects(const FHitResult& Hits);

	void PlayImpactEffects(const FHitResult& Hits);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayWeaponEffects(const FHitResult& Hits);

	void InternalPlayWeaponEffects(const FHitResult& Hits);

	UPROPERTY(BlueprintReadOnly, Category = "Weapon | Properties")
	FText WeaponName;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon | Properties")
	TObjectPtr<UTexture2D>  WeaponIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon | Properties")
	TObjectPtr<UTexture2D> AmmoIcon;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon | Properties")
	USkeletalMeshComponent* WeaponMesh = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon | Properties")
	USceneComponent* WeaponRoot = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon | Properties")
	FName MuzzleSocketName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon | Properties")
	FName LasserSocketName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon | Properties")
	FName EjectBulletSocketName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon")
	FDataTableRowHandle WeaponData = FDataTableRowHandle();

	UPROPERTY(Replicated)
	TWeakObjectPtr<ANMCharacterBase> OwningCharacter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, ReplicatedUsing = OnRep_LoadedAmmo)
	int32 LoadedAmmo;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon | Fire Rate")
	float FireRate = 0.2f;  // Time between shots
	bool bCanFire = true;       // Track fire cooldown state
	
	float LastFireTime;
	FTimerHandle TimerHandle_TimeBetweenShots;
	FTimerHandle FireCooldownTimerHandle;  // Timer for fire rate control

	FTimerHandle FireTimerHandle;
	int32 BurstCounter;
	const int32 BurstCount = 3;  // Number of shots per burst.
	const float FireInterval = 0.1f;  // Interval between burst shots.

};
