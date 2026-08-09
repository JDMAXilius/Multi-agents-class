// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IInteractable.generated.h"

// This class does not need to be modified.
UINTERFACE(BlueprintType, Blueprintable, MinimalAPI)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class NEWMOONS_API IInteractable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interact Interface")
	void Interact();

	// Declare the function as virtual to allow C++ classes to override it
	virtual void Interact_Implementation() = 0;
};
