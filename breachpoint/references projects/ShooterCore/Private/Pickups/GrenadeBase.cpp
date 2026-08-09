
#include "Pickups/GrenadeBase.h"
#include "Main/PlayerCharacter.h"
#include "Component/Inventory.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AGrenadeBase::AGrenadeBase()
{
	PickupData.Type = EPickupType::Grenade;
	PickupData.Amount = FText::FromString(TEXT("01"));
	PickupData.Name = FText::FromString(TEXT("Grenade"));
	PickupData.Description = FText::FromString(TEXT("This is Grenade Description example"));

	Amount = 1;
	ExplosionTime = 5.0f;
	bThrown = false;
}

void AGrenadeBase::Throw(FVector Velocity, APlayerCharacter* Player)
{
	bThrown = true;

	// Properly create and register projectile movement component
	UProjectileMovementComponent* ProjectileMovement = NewObject<UProjectileMovementComponent>(this);
	if (ProjectileMovement)
	{
		ProjectileMovement->RegisterComponent(); // Important!
		ProjectileMovement->UpdatedComponent = RootComponent;
		ProjectileMovement->InitialSpeed = Velocity.Size();
		ProjectileMovement->MaxSpeed = Velocity.Size();
		ProjectileMovement->Velocity = Velocity;
		ProjectileMovement->ProjectileGravityScale = 1.0f;
	}

	//  Use collision without physics simulation (so projectile movement works)
	Mesh->SetCollisionProfileName(FName("BlockAll"), true);
	Mesh->SetSimulatePhysics(false); //  Don't simulate physics if using projectile movement

	//  Explosion logic
	float WarningTime = ExplosionTime - 1.8f;
	FTimerHandle WarningTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(WarningTimerHandle, [this,Player]()
		{
			Player->LoadingNotification(ELoadingNotificationType::ThrowGrenade);
			UGameplayStatics::PlaySoundAtLocation(this, WarningSound, GetActorLocation());

			FTimerHandle ExplosionTimerHandle;
			GetWorld()->GetTimerManager().SetTimer(ExplosionTimerHandle, [this]()
				{
					UGameplayStatics::SpawnEmitterAtLocation(this, ExplosionParticle, GetActorLocation());
					UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
					Destroy();
				}, 1.8f, false);
		}, WarningTime, false);
}

void AGrenadeBase::InteractedToPickup_Implementation(APlayerCharacter* Player)
{
	Super::InteractedToPickup_Implementation(Player);
	bool bPickedUp = Player->PlayerInventory->PickupGrenade(Amount);
	if (!bPickedUp) return;
	Player->PushPickupNotification(EPickupType::Grenade, PickupData.Icon);
	Destroy();
}
