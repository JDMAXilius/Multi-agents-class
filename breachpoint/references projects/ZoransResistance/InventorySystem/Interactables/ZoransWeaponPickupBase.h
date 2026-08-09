// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "ZoransInteractableBase.h"
#include "Engine/DataTable.h"
#include "ZoransWeaponPickupBase.generated.h"

class UCapsuleComponent;

UCLASS()
class ZORANSRESISTANCE_API AZoransWeaponPickupBase : public AZoransInteractableBase
{
	GENERATED_BODY()

public:

	AZoransWeaponPickupBase();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Details")
	FDataTableRowHandle WeaponRowHandle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Details")
	bool IsSpecialWeapon = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Details")
	bool IsRandomize = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Details")
	bool bStartWithCooldownEnabled = false;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Details")
	float CooldownTime = -1.f;
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UStaticMeshComponent* GetPlatformMesh() const;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	USkeletalMeshComponent* GetWeaponMesh() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void StartCooldownTimer();

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_CooldownEndTime")
	float CooldownEndTime = 0.f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_Weaponname")
	FName Weaponname = NAME_None;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetCooldownTimeRemaining() const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnCooldownStarted();

	UFUNCTION(BlueprintImplementableEvent)
	void OnWeaponnameChange();

	UFUNCTION(BlueprintImplementableEvent)
	void OnCooldownEnded();

	UFUNCTION()
	void SetRandomWeapon();

	UPROPERTY()
	int32 OldRandomIndex = -1;

protected:

	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnRep_CooldownEndTime();
	
	UFUNCTION()
	void OnRep_Weaponname();
	
	FTimerHandle CooldownTimerHandle;

	void OnCooldownTimerExpired();
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<USceneComponent> WeaponRoot = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<UStaticMeshComponent> PlatformMesh = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<USkeletalMeshComponent> WeaponMesh = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<UCapsuleComponent> InteractCollisionShape = nullptr;
};