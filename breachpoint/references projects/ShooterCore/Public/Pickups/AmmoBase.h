
#pragma once

#include "CoreMinimal.h"
#include "Pickups/PickupBase.h"
#include "AmmoBase.generated.h"


UCLASS()
class SHOOTERCORE_API AAmmoBase : public APickupBase
{
	GENERATED_BODY()

public:
	AAmmoBase();

	//Interface
	virtual void InteractedToPickup_Implementation(APlayerCharacter* Player) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|AmmoData", meta = (AllowPrivateAccess = "true"))
	EAmmoType AmmoType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|AmmoData", meta = (AllowPrivateAccess = "true"))
	int Amount;
	
};
