

#pragma once

#include "CoreMinimal.h"
#include "ZoransCharacterBase.h"
#include "ZoransPlayerCharacter.generated.h"

class UZoransInteractionComponent;
class AZoransWeaponActor;
class USkeletalMeshComponent;
class AZoransWeaponMaster;

UCLASS(Blueprintable)
class ZORANSRESISTANCE_API AZoransPlayerCharacter : public AZoransCharacterBase
{
	GENERATED_BODY()

public:

	virtual void BeginPlay() override;

	AZoransPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	UZoransInteractionComponent* GetInteractionComponent() const;

	void PossessedBy(AController* NewController) override;

	virtual void CalculateAimVector_Implementation() override;

protected:
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TObjectPtr<UZoransInteractionComponent> InteractionComponent = nullptr;

	UFUNCTION(Server, Unreliable, Category = "Aiming")
	void SRV_SetAimVector(const FVector InVector);

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
};