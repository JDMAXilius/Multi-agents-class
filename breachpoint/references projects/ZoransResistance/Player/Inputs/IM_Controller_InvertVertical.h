// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "InputModifiers.h"
#include "IM_Controller_InvertVertical.generated.h"


UCLASS(NotBlueprintable)
class ZORANSRESISTANCE_API UIM_Controller_InvertVertical : public UInputModifier
{
	GENERATED_BODY()
public:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
};
