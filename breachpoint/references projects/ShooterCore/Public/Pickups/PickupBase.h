
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/PickupInterface.h"
#include "Data/Structure/WeaponStructs.h"
#include "PickupBase.generated.h"

class USoundBase;
class APlayerCharacter;
class UStaticMeshComponent;
class USphereComponent;

UCLASS()
class SHOOTERCORE_API APickupBase : public AActor, public IPickupInterface
{
	GENERATED_BODY()

public:
	APickupBase();
	virtual void Tick(float DeltaTime) override;

	//PickupInterface
	virtual void InteractionAreaEntered_Implementation(APlayerCharacter* Player) override;
	virtual void InteractionAreaExited_Implementation() override;
	virtual void InteractedToPickup_Implementation(APlayerCharacter* Player) override;

protected:
	virtual void BeginPlay() override;
	virtual void PlayPickupMontage();

public:
	UPROPERTY(BlueprintReadOnly)
	APlayerCharacter* PlayerCharacter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Data|Pickup")
	FPickupData PickupData;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Mesh")
	UStaticMeshComponent* Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Sphere")
	USphereComponent* InteractionArea;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Audio")
	USoundBase* PickupSound;

};

