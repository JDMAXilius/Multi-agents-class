
#pragma once

#include "CoreMinimal.h"
#include "Pickups/PickupBase.h"
#include "BagpackBase.generated.h"


UCLASS()
class SHOOTERCORE_API ABagpackBase : public APickupBase
{
	GENERATED_BODY()

public:
	ABagpackBase();

	//PickupInterface
	virtual void InteractedToPickup_Implementation(APlayerCharacter* Player) override;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Data")
	FName BagpackSocket;
	
};
