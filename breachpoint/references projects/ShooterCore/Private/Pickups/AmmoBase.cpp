
#include "Pickups/AmmoBase.h"
#include "Main/PlayerCharacter.h"
#include "Component/Inventory.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

AAmmoBase::AAmmoBase()
{
	Mesh->SetRelativeScale3D(FVector(2.0f));
	InteractionArea->InitSphereRadius(35.0f);
	AmmoType = EAmmoType::Rifle;
	Amount = 10;
}

void AAmmoBase::InteractedToPickup_Implementation(APlayerCharacter* Player)
{
	Super::InteractedToPickup_Implementation(Player);
	bool bPickedUp;
	switch (AmmoType)
	{
	case EAmmoType::Rifle:
		bPickedUp = Player->PlayerInventory->PickupAmmoRifle(Amount);
		break;
	case EAmmoType::Pistol:
		bPickedUp = Player->PlayerInventory->PickupAmmoPistol(Amount);
		break;
	case EAmmoType::Shotgun:
		bPickedUp = Player->PlayerInventory->PickupAmmoShotgun(Amount);
		break;
	default:
		bPickedUp = false;
		break;
	}
	if (!bPickedUp) return;
	Player->PushPickupNotification(EPickupType::Ammo, PickupData.Icon);
	Destroy();
}

