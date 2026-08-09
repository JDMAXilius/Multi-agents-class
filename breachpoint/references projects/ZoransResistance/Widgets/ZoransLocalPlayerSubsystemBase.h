// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ZoransLocalPlayerSubsystemBase.generated.h"


UCLASS(Abstract, Blueprintable)
class ZORANSRESISTANCE_API UZoransLocalPlayerSubsystemBase : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
};
