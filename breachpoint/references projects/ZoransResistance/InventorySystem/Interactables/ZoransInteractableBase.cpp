// Copyright Zollpa LLC.


#include "ZoransInteractableBase.h"


AZoransInteractableBase::AZoransInteractableBase()
{
	bReplicates = true;
	
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AZoransInteractableBase::Interact_Implementation(APawn* InstigatorPawn)
{
	if (InstigatorPawn->GetLocalRole() == ROLE_Authority && bUseServerInteract)
	{
		On_Interaction(InstigatorPawn, true);

		return;
	}
	
	if (bUseClientInteract)
	{
		On_Interaction(InstigatorPawn, false);
	}
}