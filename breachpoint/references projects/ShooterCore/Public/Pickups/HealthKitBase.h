
#pragma once

#include "CoreMinimal.h"
#include "Pickups/PickupBase.h"
#include "HealthKitBase.generated.h"

UCLASS()
class SHOOTERCORE_API AHealthKitBase : public APickupBase
{
	GENERATED_BODY()

public:
	AHealthKitBase();

	//PickupInterface
	virtual void InteractedToPickup_Implementation(APlayerCharacter* Player) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Data|Health")
	EHealthKitType HealthType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Data|Health")
	int Amount;
	
};
