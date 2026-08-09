// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectiles/NMProjectileBouncing.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"


// Sets default values
ANMProjectileBouncing::ANMProjectileBouncing()
{
	// Enable physics-based movement and bouncing behavior
	//ProjectileMovement->bShouldBounce = false;  // Disable default bouncing
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->ProjectileGravityScale = 1.0f;  // Normal gravity
	SetLifeSpan(5.0f);  // Destroy after 5 seconds to simulate grenade fuse
	SetActorEnableCollision(true);
}

// Called when the game starts or when spawned
void ANMProjectileBouncing::BeginPlay()
{
	Super::BeginPlay();

	// Set a repeating timer to update movement every 0.01 seconds
	GetWorld()->GetTimerManager().SetTimer(MovementTimerHandle, this, &ANMProjectileBouncing::UpdateMovement, 0.01f, true);
	
	// Schedule the grenade explosion to occur after 5 seconds
	if (HasAuthority()) // Ensure only the server sets the timer
	{
		GetWorld()->GetTimerManager().SetTimer(ExplosionTimerHandle, this, &ANMProjectileBouncing::Explode, ExplosionTimer, false);
	}
}

FVector ANMProjectileBouncing::PredictNextLocation(float DeltaTime) const
{
	// Predict the next location using the current velocity
	return GetActorLocation() + CurrentVelocity * DeltaTime;
}

void ANMProjectileBouncing::HandleCollision(const FHitResult& Hit)
{
	// Calculate new velocity after bounce
	CurrentVelocity = CalculateBouncing(CurrentVelocity, Hit);

	// Move the projectile to the impact point to avoid clipping issues
	SetActorLocation(Hit.ImpactPoint + Hit.ImpactNormal * 2.0f);

	// If the projectile stops moving, trigger explosion (or other logic)
	if (CurrentVelocity.IsNearlyZero())
	{
		UGameplayStatics::ApplyRadialDamage(
			this, 50.0f, GetActorLocation(), 300.0f, UDamageType::StaticClass(),
			{OwnerActor, WeaponActor}, this, GetInstigatorController(), true
		);
		//Destroy();  // Destroy the projectile after explosion
	}
}

FVector ANMProjectileBouncing::CalculateBouncing(const FVector& Velocity, const FHitResult& Hit) const
{
	// Reflect the velocity across the hit surface's normal
	FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	FVector ReflectedVelocity = FVector::VectorPlaneProject(Velocity, Normal) * -1.0f;

	// Apply energy loss factor to simulate energy dissipation on each bounce
	FVector AdjustedVelocity = ReflectedVelocity * EnergyLossFactor;
	 // Adjust further with bounciness and friction
    AdjustedVelocity += Normal * Velocity.Size() * (1.0f - Friction);
	
	// Clamp velocity to avoid tiny infinite bounces
	if (AdjustedVelocity.Size() < MinBounceSpeed)
	{
		return FVector::ZeroVector;  // Stop movement
	}

	return AdjustedVelocity;
}

FVector ANMProjectileBouncing::ApplyGravity(const FVector& Velocity, float DeltaTime) const
{
	// Apply gravity to the Z-axis only
	FVector Gravity(0.0f, 0.0f, -980.0f * GravityScale * DeltaTime);
	return Velocity + Gravity;
}

void ANMProjectileBouncing::UpdateMovement()
{
	if (!HasAuthority()) return;  // Only the server updates movement

	float DeltaTime = 0.01f;  // Use the same interval as the timer

	// Apply gravity to the current velocity
	CurrentVelocity = ApplyGravity(CurrentVelocity, DeltaTime);

	// Predict the next location
	FVector NextLocation = PredictNextLocation(DeltaTime);

	// Perform a line trace to detect potential collisions
	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	//QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(OwnerActor);
	QueryParams.AddIgnoredActor(WeaponActor);
	
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, GetActorLocation(), NextLocation, ECC_Visibility, QueryParams
	);
	
	if (bHit)
	{
		// Handle collision and bounce logic
		HandleCollision(Hit);
	}
	else
	{
		// Move the projectile to the predicted location
		SetActorLocation(NextLocation);
	}
}

void ANMProjectileBouncing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ANMProjectileBouncing, CurrentVelocity);
}

void ANMProjectileBouncing::Explode()
{
	// Trigger the explosion on all clients
	Multicast_Explode();
	
	Destroy();  // Destroy the grenade after explosion
}

void ANMProjectileBouncing::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (ProjectileMovement->Velocity.IsNearlyZero()) Explode();
	// Get the current velocity
    FVector Velocity = CollisionComponent->GetPhysicsLinearVelocity();
    // Set the updated velocity to the component
    CollisionComponent->SetPhysicsLinearVelocity(Velocity);
    // Optional: Check if the projectile should explode (if the velocity is nearly zero)
    const float MinVelocity = 150.0f;  // Threshold for explosion
    if (Velocity.Size() < MinVelocity)
    {
        Explode();  // Trigger explosion if the grenade has come to rest
    }
	// Optional: Log bounce for debugging
	UE_LOG(LogTemp, Log, TEXT("New Velocity after Bounce: %s"), *Velocity.ToString());

	/*// Get the current velocity
	FVector Velocity = CurrentVelocity;
	// Reflect and adjust velocity based on the hit surface
	FVector NewVelocity = CalculateBouncing(Velocity, Hit);
	// Apply gravity to the new velocity after bounce
	NewVelocity = ApplyGravity(NewVelocity, GetWorld()->GetDeltaSeconds());
	// Set the new velocity to the projectile
	CurrentVelocity = NewVelocity;
	// Move the projectile to the impact point to avoid clipping issues
	SetActorLocation(Hit.ImpactPoint + Hit.ImpactNormal * 2.0f);
	// Optional: Log the new velocity for debugging
	UE_LOG(LogTemp, Log, TEXT("New Velocity after Bounce: %s"), *CurrentVelocity.ToString());
	// Check if the projectile should stop (energy loss makes it too slow)
	if (CurrentVelocity.Size() < MinBounceSpeed)
	{
		// Trigger an explosion or stop the movement
		UGameplayStatics::ApplyRadialDamage(
			this, 100.0f, GetActorLocation(), 300.0f, UDamageType::StaticClass(),
			{}, this, GetInstigatorController(), true
		);
		//Destroy();  // Destroy the projectile after explosion
	}*/
}

void ANMProjectileBouncing::CalculateBounce(FVector& Velocity, const FHitResult& Hit)
{
	/*// Reflect the velocity around the surface normal
	FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	FVector ReflectedVelocity = FVector::VectorPlaneProject(Velocity, Normal) * -1.0f;

	// Adjust with bounciness and friction factors
	Velocity = (ReflectedVelocity * Bounciness) + (Normal * Velocity.Size() * (1.0f - Friction));

	// Clamp the velocity to avoid too low speeds after repeated bounces
	const float MinVelocity = 100.0f;
	if (Velocity.Size() < MinVelocity)
	{
		Velocity = FVector::ZeroVector;  // Stop if the velocity is too low
	}*/
	if (bFirstBounce)
	{
		// On the first bounce, project the velocity forward along the hit surface with no reflection
		FVector Normal = Hit.ImpactNormal.GetSafeNormal();
		FVector ForwardDirection = GetActorForwardVector();
		FVector ForwardBounce = FVector::VectorPlaneProject(ForwardDirection, Normal) * Velocity.Size();

		Velocity = ForwardBounce;  // Set the forward-projected velocity

		// Mark the first bounce as complete
		bFirstBounce = false;
	}
	else
	{
		// Normalize the impact normal
		FVector Normal = Hit.ImpactNormal.GetSafeNormal();

		// Reflect velocity across the surface normal
		FVector ReflectedVelocity = FVector::VectorPlaneProject(Velocity, Normal) * -1.0f;

		// Calculate energy loss based on bounce angle
		float ImpactAngle = FMath::Abs(FVector::DotProduct(Velocity.GetSafeNormal(), Normal));
		float EnergyLoss = FMath::Lerp(0.1f, 0.5f, ImpactAngle);  // Higher loss at grazing angles

		// Adjust with bounciness and friction factors
		FVector AdjustedVelocity = (ReflectedVelocity * Bounciness) + (Normal * Velocity.Size() * (1.0f - Friction));

		// Apply energy loss to simulate realistic bouncing
		Velocity = AdjustedVelocity * (1.0f - EnergyLoss);

		// Apply random deviation for natural bouncing behavior
		FVector RandomDeviation = FMath::VRand() * FMath::RandRange(0.0f, 10.0f);  // Small random offset
		Velocity += RandomDeviation;

		// Gravity compensation between bounces to keep the motion realistic
		const FVector Gravity(0.0f, 0.0f, -980.0f * GetWorld()->GetDeltaSeconds());  // Simulate gravity
		Velocity += Gravity;

		// Clamp velocity to avoid low speeds causing infinite small bounces
		const float MinVelocity = 150.0f;
		if (Velocity.Size() < MinVelocity)
		{
			Velocity = FVector::ZeroVector;  // Stop movement if the velocity drops too low
		}
	}

	// Optional: Log the new velocity for debugging
	UE_LOG(LogTemp, Log, TEXT("New Velocity: %s"), *Velocity.ToString());
}

void ANMProjectileBouncing::Multicast_Explode_Implementation()
{
	// Play explosion effects at the grenade's location
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionEffect, GetActorLocation());
	}

	// Apply radial damage to all nearby actors
	UGameplayStatics::ApplyRadialDamage(
		this, ExplosionDamage, GetActorLocation(), ExplosionRadius,
		UDamageType::StaticClass(), {OwnerActor, WeaponActor}, this, InstigatorController, true);
	
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), ExplosionSound, GetActorLocation());
}
