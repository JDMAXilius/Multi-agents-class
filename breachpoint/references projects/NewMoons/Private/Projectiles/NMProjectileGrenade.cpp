// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectiles/NMProjectileGrenade.h"
#include "Characters/NMCharacterBase.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
ANMProjectileGrenade::ANMProjectileGrenade()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	// Initialize default values
	CurrentBounces = 0;
	MaxBounces = 3;
	bFirstBounce = true;
	Bounciness = 0.8f;  
	Friction =  0.2f;
	ExplosionRadius = 500.0f;
	MaxDamage = 100.0f;
	MinDamage = 10.0f;

	// Projectile Movement Component
	ProjectileMovement->InitialSpeed = 2500.0f;
	ProjectileMovement->MaxSpeed = 10000.0f;
	ProjectileMovement->ProjectileGravityScale = 1.0f;  // Adjust for natural grenade trajectory
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = Bounciness;
	ProjectileMovement->Friction = Friction;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 20.0f;
}

// Called when the game starts or when spawned
void ANMProjectileGrenade::BeginPlay()
{
	Super::BeginPlay();
	// Set a timer to ensure the grenade explodes after 3 seconds
	GetWorld()->GetTimerManager().SetTimer(ExplosionTimerHandle, this, &ANMProjectileGrenade::Explode, ExplosionTimer, false);
}

void ANMProjectileGrenade::Explode()
{
	// Cancel timers to prevent multiple explosions
	GetWorld()->GetTimerManager().ClearTimer(ExplosionTimerHandle);

	// Get all actors within the explosion radius
	TArray<AActor*> OverlappingActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), OverlappingActors);

	// Apply damage to actors within range
	for (AActor* Actor : OverlappingActors)
	{
		FVector ActorLocation = Actor->GetActorLocation();
		FVector ExplosionOrigin = GetActorLocation();

		float Distance = FVector::Dist(ExplosionOrigin, ActorLocation);
		if (Distance <= ExplosionRadius)
		{
			// Calculate damage based on distance
			float Damage = CalculateDamage(ExplosionOrigin, ActorLocation);

			// Apply damage
			UGameplayStatics::ApplyDamage(Actor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
		}
	}
	if (IsDebugsActive)
	DrawDebugSphere(GetWorld(),GetActorLocation(),ExplosionRadius,32,FColor::Red,false,2.0f, 0, 5.0f);

	// Multicast explosion effects
	Multicast_Explode();

	// Destroy the grenade
	Destroy();
}

float ANMProjectileGrenade::CalculateDamage(const FVector& ExplosionOrigin, const FVector& TargetLocation) const
{
	float Distance = FVector::Dist(ExplosionOrigin, TargetLocation);
	float DamageScale = FMath::Clamp(1.0f - (Distance / ExplosionRadius), 0.0f, 1.0f);
	return FMath::Lerp(MinDamage, MaxDamage, DamageScale);
}

void ANMProjectileGrenade::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                 FVector NormalImpulse, const FHitResult& Hit)
{
	//Super::OnHit(HitComponent, OtherActor, OtherComp, NormalImpulse, Hit);
	// Ensure it's not exploding on the owner
	if (OtherActor && OtherActor == GetOwner()) return;
	if (OtherActor && OtherActor->IsA<ANMCharacterBase>()) // Replace with your custom character class
	{
		Explode();
		return;
	}
	// Check if the grenade should stop bouncing
	if (CurrentBounces >= MaxBounces || ProjectileMovement->Velocity.IsNearlyZero() /*|| ProjectileMovement->Velocity.Size() < ProjectileMovement->BounceVelocityStopSimulatingThreshold*/)
		Explode();

	// Increment bounce count
	CurrentBounces++;

	//CalculateBounce(CurrentVelocity, Hit);
	//CalculateBounceForPMC(ProjectileMovement->Velocity, Hit);
	if (IsDebugsActive)
	DrawDebugSphere(GetWorld(), Hit.Location, 10.0f, 12, FColor::Red, false, 1.0f);
}

void ANMProjectileGrenade::CalculateBounceForPMC(FVector& Velocity, const FHitResult& Hit)
{
	// Add random deviation for natural bounce
	FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	FVector ReflectedVelocity = FVector::VectorPlaneProject(ProjectileMovement->Velocity, Normal) * ProjectileMovement->Bounciness;

	// Apply energy loss (based on bounciness and a custom energy loss factor)
	ReflectedVelocity *= EnergyLossFactor;

	// Add upward boost proportional to the remaining velocity and bounciness
	ReflectedVelocity.Z = FMath::Max(ReflectedVelocity.Z, Velocity.Size() * ProjectileMovement->Bounciness * 0.5f);
	// Reduce sliding using friction
	ReflectedVelocity *= FVector(1.0f - ProjectileMovement->Friction, 1.0f - ProjectileMovement->Friction, 1.0f);

	//ReflectedVelocity.Z += FMath::Clamp(Velocity.Size() * 0.3f, 50.0f, 200.0f);
	// Add random angular deviation
	FVector RandomDeviation = FMath::VRand() * FMath::RandRange(0.0f, 20.0f);
	ProjectileMovement->Velocity += RandomDeviation;
	// Clamp minimum velocity to avoid infinite bouncing with low energy
	const float MinBounceVelocity = 100.0f; // Grenade stops bouncing below this speed
	if (ReflectedVelocity.Size() < MinBounceVelocity)
	{
		ReflectedVelocity = FVector::ZeroVector;
	}
	// Update the velocity
	ProjectileMovement->Velocity = ReflectedVelocity;


	// Apply friction to reduce sliding, but retain forward momentum
	ProjectileMovement->Velocity *= (1.0f - ProjectileMovement->Friction * 1.5f);

	//DrawDebugLine(GetWorld(), Hit.Location, Hit.Location + ReflectedVelocity, FColor::Green, false, 1.0f, 0, 2.0f);
}

void ANMProjectileGrenade::Multicast_Explode_Implementation()
{
	// Spawn particle effect
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionEffect, GetActorLocation());
	}

	// Play explosion sound
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), ExplosionSound, GetActorLocation());
	}
}
