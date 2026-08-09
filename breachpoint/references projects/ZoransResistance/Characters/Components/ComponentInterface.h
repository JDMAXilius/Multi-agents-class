// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ComponentInterface.generated.h"

class UZoransWeaponComponent;
class UZoransHealthComponent;
class UZoransHeroComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UComponentInterface : public UInterface
{
	GENERATED_BODY()
};

class ZORANSRESISTANCE_API IComponentInterface
{
	GENERATED_IINTERFACE_BODY()
	
	UFUNCTION(BlueprintCallable)
	virtual UZoransHealthComponent* GetHealthComponent() const = 0;

	UFUNCTION(BlueprintCallable)
	virtual UZoransWeaponComponent* GetWeaponComponent() const = 0;

	UFUNCTION(BlueprintCallable)
	virtual UZoransHeroComponent* GetHeroComponent() const = 0;

	virtual void CheckComponentRequirements();
};