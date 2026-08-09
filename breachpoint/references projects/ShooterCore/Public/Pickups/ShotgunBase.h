
#pragma once

#include "CoreMinimal.h"
#include "Pickups/WeaponBaseActor.h"
#include "ShotgunBase.generated.h"


UCLASS()
class SHOOTERCORE_API AShotgunBase : public AWeaponBaseActor
{
	GENERATED_BODY()

public:
	AShotgunBase();
	virtual void Shoot(APlayerCharacter* Player, bool bAdsButtonPressed) override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Weapon|Bullet", meta = (AllowPrivateAccess = "true"))
	int32 PelletCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Setup|Weapon|Bullet", meta = (AllowPrivateAccess = "true"))
	float MaxBulletSpreadWhenAiming;
	
};
