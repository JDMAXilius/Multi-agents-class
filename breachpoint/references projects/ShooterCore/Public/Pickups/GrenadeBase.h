
#pragma once

#include "CoreMinimal.h"
#include "Pickups/PickupBase.h"
#include "GrenadeBase.generated.h"

class USoundBase;
class UParticleSystem;
class APlayerCharacter;

UCLASS()
class SHOOTERCORE_API AGrenadeBase : public APickupBase
{
	GENERATED_BODY()

public:
	AGrenadeBase();
	void Throw(FVector Velocity, APlayerCharacter* Player);

	//PickupInterface
	virtual void InteractedToPickup_Implementation(APlayerCharacter* Player) override;

protected:
	bool bThrown;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Data|Grenade")
	UParticleSystem* ExplosionParticle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Data|Grenade")
	USoundBase* WarningSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Data|Grenade")
	USoundBase* ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Data|Grenade")
	int Amount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Data|Grenade")
	float ExplosionTime;

	
};
