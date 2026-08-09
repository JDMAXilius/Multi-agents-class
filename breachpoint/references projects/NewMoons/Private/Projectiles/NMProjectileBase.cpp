 // Fill out your copyright notice in the Description page of Project Settings.


#include "Projectiles/NMProjectileBase.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
ANMProjectileBase::ANMProjectileBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetActorEnableCollision(true);

	// Initialize the collision component
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	CollisionComponent->InitSphereRadius(30.0f);
	CollisionComponent->BodyInstance.SetCollisionProfileName("Projectile");
	CollisionComponent->SetSimulatePhysics(true);
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	//CollisionComponent->SetCollisionProfileName("BlockAll");
	CollisionComponent->OnComponentHit.AddDynamic(this, &ANMProjectileBase::OnHit);
	SetRootComponent(CollisionComponent);

	// Setup components
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(CollisionComponent);
	ProjectileMesh->SetSimulatePhysics(false);
	ProjectileMesh->SetNotifyRigidBodyCollision(false);
	ProjectileMesh->SetCollisionProfileName("NoCollision");
	
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->bAutoActivate = true;
	ProjectileMovement->InitialSpeed = 2000.0f;
	ProjectileMovement->MaxSpeed = 10000;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
}

// Called when the game starts or when spawned
void ANMProjectileBase::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(10.0f);  // Automatic cleanup after 10 seconds
}

void ANMProjectileBase::InitializeProjectile(float Speed, AActor* InOwnerActor, AActor* InWeaponActor)
{
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	InstigatorController = GetInstigatorController();
	ProjectileMovement->Activate();
	OwnerActor = InOwnerActor;
	WeaponActor = InWeaponActor;
}

void ANMProjectileBase::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	//if (OtherActor != OwnerActor || OtherActor != WeaponActor)
	HandleImpact(OtherActor);
	Destroy();  // Destroy the projectile after impact
}

void ANMProjectileBase::HandleImpact(AActor* HitActor)
{
	if (HitActor)
	{
		UGameplayStatics::ApplyPointDamage(HitActor, BaseDamage, HitActor->GetActorLocation(),
										   FHitResult(), InstigatorController, this, UDamageType::StaticClass());
	}
}

void ANMProjectileBase::FireInDirection(const FVector InLaunchDirection)
{
	if (HasAuthority())
	{
		ProjectileMovement->Activate();
		//ProjectileMovement->Velocity = InLaunchDirection;
		//ProjectileMovement->Velocity = InLaunchDirection * ProjectileMovement->InitialSpeed;
		FVector NormalImpulse = OwnerActor->GetActorLocation();
		CollisionComponent->SetPhysicsLinearVelocity(InLaunchDirection);
	}
}

 void ANMProjectileBase::PredictProjectilePath(int32 MaxSimulatedBounces, float SimulationTimeStep,
	 float TotalSimulationTime)
 {
    // Initialize variables for simulation
    FVector CurrentLocation = GetActorLocation();
    FVector CurrentVelocity = ProjectileMovement->Velocity;
    FVector Gravity = FVector(0.0f, 0.0f, -980.0f) * ProjectileMovement->ProjectileGravityScale; // Gravity in cm/s^2
    float TimeElapsed = 0.0f;
    int32 Bounces = 0;

    // Simulate the trajectory
    while (TimeElapsed < TotalSimulationTime && Bounces < MaxSimulatedBounces)
    {
        // Calculate next position
        FVector NextLocation = CurrentLocation + (CurrentVelocity * SimulationTimeStep) + (0.5f * Gravity * FMath::Square(SimulationTimeStep));

        // Perform a line trace to detect collisions
        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);
        QueryParams.AddIgnoredActor(GetOwner()); // Ignore the grenade owner

        bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, CurrentLocation, NextLocation, ECC_Visibility, QueryParams);

        if (bHit)
        {
	        if (IsDebugsActive)
	        {
	            // Draw the hit segment
	            DrawDebugLine(GetWorld(), CurrentLocation, HitResult.Location, FColor::Red, false, 5.0f, 0, 2.0f);
	            DrawDebugSphere(GetWorld(), HitResult.Location, 10.0f, 12, FColor::Yellow, false, 5.0f);
	        }

            // Reflect velocity for the bounce
            FVector ImpactNormal = HitResult.ImpactNormal.GetSafeNormal();
            FVector ReflectedVelocity = FVector::VectorPlaneProject(CurrentVelocity, ImpactNormal) * ProjectileMovement->Bounciness;

        	// Apply energy loss//
        	ReflectedVelocity *= ProjectileMovement->Bounciness;

        	// Apply friction for sliding
        	ReflectedVelocity *= FVector(1.0f - ProjectileMovement->Friction, 1.0f - ProjectileMovement->Friction, 1.0f);

        	// Ensure upward momentum is preserved if necessary
        	ReflectedVelocity.Z = FMath::Max(ReflectedVelocity.Z, CurrentVelocity.Size() * 0.3f);

        	// Stop bouncing if velocity is below threshold
        	if (ReflectedVelocity.Size() < ProjectileMovement->BounceVelocityStopSimulatingThreshold && IsDebugsActive)
        	{
        		DrawDebugSphere(GetWorld(), HitResult.Location, 30.0f, 24, FColor::Red, false, 5.0f);
        		break;
        	}
        	
            // Update location and velocity
            CurrentLocation = HitResult.Location;
            CurrentVelocity = ReflectedVelocity;

            // Increment bounce count
            Bounces++;
        }
        else
        {
            // Draw the path if no hit occurs
        	if (IsDebugsActive)
            DrawDebugLine(GetWorld(), CurrentLocation, NextLocation, FColor::Green, false, 5.0f, 0, 1.0f);

            // Update location
            CurrentLocation = NextLocation;
        }

        // Apply gravity to velocity
        CurrentVelocity += Gravity * SimulationTimeStep;

        // Increment simulation time
        TimeElapsed += SimulationTimeStep;
    }

    // Draw final explosion point if stopped naturally
   // if (Bounces >= MaxSimulatedBounces || TimeElapsed >= TotalSimulationTime)
    {
    	if (IsDebugsActive)
        DrawDebugSphere(GetWorld(), CurrentLocation, 30.0f, 24, FColor::Red, false, 5.0f);
    }
}

 void ANMProjectileBase::SetInitialVelocity(const FVector& Velocity)
 {
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = Velocity;
	}
 }
