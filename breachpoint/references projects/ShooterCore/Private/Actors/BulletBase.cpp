
//Base bullet actor class responsible for bullet visuals and movement

#include "Actors/BulletBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"

// Constructor: Initializes bullet mesh, particle effect, and movement
ABulletBase::ABulletBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create the bullet's static mesh and set it as root
	BulletMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	RootComponent = BulletMesh;
	BulletMesh->CastShadow = false;

	// Create and attach the trail particle system
	ParticleSystem = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleSystem"));
	ParticleSystem->SetupAttachment(BulletMesh);
	ParticleSystem->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.4f));

	// Create projectile movement component and configure speed and gravity
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile"));
	ProjectileMovement->InitialSpeed = 12000.0f;
	ProjectileMovement->MaxSpeed = 15000.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
}

void ABulletBase::BeginPlay()
{
	Super::BeginPlay();

	// Select the appropriate trail effect based on ammo type
	switch (AmmoType)
	{
	case EAmmoType::Rifle:
		ParticleSystem->SetTemplate(RifleAmmo);
		break;
	case EAmmoType::Pistol:
		ParticleSystem->SetTemplate(PistolAmmo);
		break;
	case EAmmoType::Shotgun:
		ParticleSystem->SetTemplate(ShotgunAmmo);
		break;
	}

	// Automatically destroy the bullet after 0.2 seconds
	SetLifeSpan(0.2f);
}

// Called every frame (currently unused but kept for future logic)
void ABulletBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

