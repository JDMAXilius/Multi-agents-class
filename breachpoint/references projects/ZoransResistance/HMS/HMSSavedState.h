// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HMSSavedState.generated.h"

// This class does not need to be modified.
UINTERFACE(BlueprintType)
class UHMSSavedState : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ZORANSRESISTANCE_API IHMSSavedState
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HMSSavedState")
	void PreSaveActorState();

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "HMSSavedState")
	void LoadActorState();

};
