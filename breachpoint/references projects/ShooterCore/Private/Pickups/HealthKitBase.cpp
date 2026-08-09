
#include "Pickups/HealthKitBase.h"
#include "Main/PlayerCharacter.h"
#include "Component/Inventory.h"


AHealthKitBase::AHealthKitBase()
{
	PickupData.Type = EPickupType::HealthKit;
	PickupData.Amount = FText::FromString(TEXT("01"));
	PickupData.Name = FText::FromString(TEXT("Health Kit"));
	PickupData.Description = FText::FromString(TEXT("This is Health Kit Description example"));

	HealthType = EHealthKitType::Bandage;
	Amount = 1;
}

void AHealthKitBase::InteractedToPickup_Implementation(APlayerCharacter* Player)
{
	Super::InteractedToPickup_Implementation(Player);
	bool bPickedUp;
	switch (HealthType)
	{
	case EHealthKitType::Bandage:
		bPickedUp = Player->PlayerInventory->PickupBandage(Amount);
		break;
	case EHealthKitType::EnergyDrink:
		bPickedUp = Player->PlayerInventory->PickupEnergyDrink(Amount);
		break;
	default:
		bPickedUp = false;
		break;
	}
	if (!bPickedUp) return;
	Player->PushPickupNotification(EPickupType::HealthKit, PickupData.Icon);
	Destroy();
}
