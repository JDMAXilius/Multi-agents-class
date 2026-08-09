// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "ZoransInteractableBase.h"
#include "ZoransAmmoPickupBase.generated.h"

class UCapsuleComponent;

UCLASS()
class ZORANSRESISTANCE_API AZoransAmmoPickupBase : public AZoransInteractableBase
{
	GENERATED_BODY()

public:
	AZoransAmmoPickupBase();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Details")
	float AmmoRatio = 0.5f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Details")
	float CooldownTime = 10.f;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void StartCooldownTimer();

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_CooldownEndTime")
	float CooldownEndTime = 0.f;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetCooldownTimeRemaining() const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnCooldownStarted();

	UFUNCTION(BlueprintImplementableEvent)
	void OnCooldownEnded();

	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

protected:
	UFUNCTION()
	void OnRep_CooldownEndTime();
	
	FTimerHandle CooldownTimerHandle;

	void OnCooldownTimerExpired();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<USceneComponent> PickupRoot = nullptr;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<UCapsuleComponent> InteractCollisionShape = nullptr;
};
