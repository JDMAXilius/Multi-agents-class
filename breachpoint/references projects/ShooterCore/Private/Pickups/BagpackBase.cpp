
#include "Pickups/BagpackBase.h"
#include "Main/PlayerCharacter.h"
#include "Component/Inventory.h"
#include "Components/StaticMeshComponent.h"

ABagpackBase::ABagpackBase()
{
	BagpackSocket = FName("BagpackSocket");

	PickupData.Type = EPickupType::Bagpack;
	PickupData.Amount = FText::FromString(TEXT("01"));
	PickupData.Name = FText::FromString(TEXT("Bagpack"));
	PickupData.Description = FText::FromString(TEXT("This is Bagpack Description example"));
}

void ABagpackBase::InteractedToPickup_Implementation(APlayerCharacter* Player)
{
	Super::InteractedToPickup_Implementation(Player);

	bool bPickedUp = Player->PlayerInventory->PickupBagpack(this);
	if (!bPickedUp) return;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Player->PushPickupNotification(EPickupType::Bagpack, PickupData.Icon);
}
