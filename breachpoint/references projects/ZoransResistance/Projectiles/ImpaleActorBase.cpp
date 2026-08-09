// Copyright Zollpa LLC


#include "ImpaleActorBase.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"


AImpaleActorBase::AImpaleActorBase()
{
	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	ImpaleActorRoot = CreateDefaultSubobject<USceneComponent>("ImpaleActorRoot");
	RootComponent = ImpaleActorRoot;

	ImpaleActorCollision = CreateDefaultSubobject<UCapsuleComponent>("ImpaleActorCollision");
	ImpaleActorCollision->SetupAttachment(RootComponent);
	ImpaleActorCollision->SetHiddenInGame(false);
	ImpaleActorCollision->SetVisibility(true);

	PhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>("PhysicsHandle");

	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovementComponent");
	ProjectileMovementComponent->bInitialVelocityInLocalSpace = false;
	ProjectileMovementComponent->ProjectileGravityScale = 1.f;
}

void AImpaleActorBase::BeginPlay()
{
	Super::BeginPlay();

	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.AddIgnoredActor(GetInstigator());
	ObjQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	
	PreviousLocation = GetActorLocation();

	ProjectileMovementComponent->InitialSpeed = ImpactForce;
	ProjectileMovementComponent->Velocity = ImpactDirection * ImpactForce;

	if (ImpaledMesh)
	{
		PhysicsHandle->SetInterpolationSpeed(10000000.f);
		PhysicsHandle->SetLinearStiffness(10000000.f);
		PhysicsHandle->SetLinearDamping(50000.f);
		const FName BoneName = HitBone == NAME_None ? ImpaledMesh->FindClosestBone(PreviousLocation, nullptr, 0, true) : HitBone;
		PhysicsHandle->GrabComponentAtLocation(ImpaledMesh, BoneName, ImpaledMesh->GetCenterOfMass(BoneName));
		if (PhysicsHandle->GrabbedComponent)
		{
			PhysicsHandle->SetTargetLocationAndRotation(GetActorLocation(), GetActorRotation());
		}
	}

	TraceDelegate.BindUObject(this, &AImpaleActorBase::PerformTrace);
	GetWorld()->GetTimerManager().SetTimer(TraceTimerHandle, TraceDelegate, TraceTime, false, TraceTime);
}

void AImpaleActorBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (PhysicsHandle->GrabbedComponent)
	{
		PhysicsHandle->SetTargetLocation(GetActorLocation());
	}
}

void AImpaleActorBase::PerformTrace()
{
	FHitResult Hit;
	
	const FVector CurrentLocation = GetActorLocation();
	
	if (GetWorld()->SweepSingleByObjectType(Hit, PreviousLocation, CurrentLocation, ImpaleActorCollision->GetComponentQuat(), ObjQueryParams, FCollisionShape::MakeCapsule(ImpaleActorCollision->GetScaledCapsuleRadius(), ImpaleActorCollision->GetScaledCapsuleHalfHeight()), QueryParams))
	{
		AttachImpaleActor(Hit);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(TraceTimerHandle, TraceDelegate, TraceTime, false);
	}

	PreviousLocation = CurrentLocation;
}

void AImpaleActorBase::AttachImpaleActor(FHitResult InHit)
{
	ProjectileMovementComponent->Deactivate();
	
	if (UPrimitiveComponent* HitComponent = InHit.GetComponent())
	{
		const ECollisionChannel CollisionChannel = HitComponent->GetCollisionObjectType();
		if (CollisionChannel == ECC_WorldDynamic)
		{
			const FAttachmentTransformRules AttachRules = FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepWorld, true);
			AttachToComponent(HitComponent, AttachRules);
		}
		else if (CollisionChannel == ECC_WorldStatic)
		{
			const float TraceLength = ImpaleActorCollision->GetScaledCapsuleHalfHeight();
			const FVector Direction = (InHit.TraceEnd - InHit.TraceStart).GetSafeNormal();
			const FVector EndLocation = InHit.ImpactPoint - Direction * TraceLength;
			SetActorLocation(EndLocation);
		}
	}
}
