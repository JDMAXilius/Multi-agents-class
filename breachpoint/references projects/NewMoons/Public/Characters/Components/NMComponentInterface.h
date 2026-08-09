// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NMComponentInterface.generated.h"

class UNMHealthComponent;
class UNMWeaponComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UNMComponentInterface : public UInterface
{
	GENERATED_BODY()
};

class NEWMOONS_API INMComponentInterface
{
	GENERATED_IINTERFACE_BODY()

	UFUNCTION(BlueprintCallable)
	virtual UNMHealthComponent* GetHealthComponent() const = 0;

	UFUNCTION(BlueprintCallable)
	virtual UNMWeaponComponent* GetWeaponComponent() const = 0;

};
