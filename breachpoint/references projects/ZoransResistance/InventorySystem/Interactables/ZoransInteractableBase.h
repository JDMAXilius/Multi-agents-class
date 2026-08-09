// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "InteractionInterface.h"
#include "GameFramework/Actor.h"
#include "ZoransInteractableBase.generated.h"

class UInteractionInterface;

UCLASS()
class ZORANSRESISTANCE_API AZoransInteractableBase : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:

	AZoransInteractableBase();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Details")
	bool bUseServerInteract = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Details")
	bool bUseClientInteract = true;
	
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	UFUNCTION(BlueprintImplementableEvent)
	void On_Interaction(APawn* InstigatorPawn, bool bHasAuthority);
};