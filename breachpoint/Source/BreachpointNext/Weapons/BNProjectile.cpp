#include "Weapons/BNProjectile.h"

#include "AbilitySystem/Effects/BNDamage.h"
#include "BreachpointNext.h"
#include "Core/BNGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

ABNProjectile::ABNProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// The server owns the flight and every machine watches the same one. Not predicted: see the
	// header, and UBNGA_Grenade's ServerOnly policy, which is the same call.
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(8.f);
	CollisionComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
	SetRootComponent(CollisionComponent);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	// The sphere collides; the mesh is only ever seen. Two colliding shapes on one grenade is how
	// a thrown object snags on its own hull.
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->ProjectileGravityScale = 1.f;
	ProjectileMovement->bRotationFollowsVelocity = false;
}

void ABNProjectile::BeginPlay()
{
	Super::BeginPlay();

	ProjectileMovement->Bounciness = Bounciness;

	// Cosmetics on every machine — the grenade must look like a grenade on the client watching it
	// as much as on the server simulating it.
	if (UStaticMesh* Loaded = Mesh.IsNull() ? nullptr : Mesh.LoadSynchronous())
	{
		MeshComponent->SetStaticMesh(Loaded);
	}
	if (UNiagaraSystem* Trail = Cast<UNiagaraSystem>(TrailEffect.IsNull() ? nullptr : TrailEffect.LoadSynchronous()))
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			Trail, CollisionComponent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/true);
	}

	// The fuse is the authority's clock alone. A client running its own would explode at a
	// different instant and the two would disagree about who was inside the radius.
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(FuseTimer, this, &ABNProjectile::Explode, FMath::Max(FuseTime, 0.05f), /*bLoop=*/false);
	}
}

void ABNProjectile::Launch(const FVector& Direction)
{
	if (!HasAuthority())
	{
		return;
	}
	ProjectileMovement->Velocity = Direction.GetSafeNormal() * LaunchSpeed;
}

void ABNProjectile::Explode()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = GetActorLocation();
	AActor* Thrower = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();

	// THE radial rule, purity law 3: our own overlap query, then one GE per target through the one
	// door. No AActor::TakeDamage, no ApplyRadialDamage, no FDamageEvent anywhere in this path.
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps, Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius));

	// One overlap per ACTOR: a character answers with capsule and mesh both, and paying twice for
	// one grenade is the kind of bug that reads as "grenades one-shot sometimes".
	TSet<AActor*> Damaged;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Target) || Damaged.Contains(Target))
		{
			continue;
		}
		// No ASC, no damage — and checking here keeps the LOS trace off actors that cannot be hurt.
		if (!UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
		{
			continue;
		}
		Damaged.Add(Target);

		const FVector TargetPoint = Target->GetActorLocation();
		const float Distance = FVector::Dist(Center, TargetPoint);

		if (bRequireLineOfSight)
		{
			FCollisionQueryParams LOSParams(FName(TEXT("BNBlastLOS")), /*bTraceComplex=*/false, this);
			LOSParams.AddIgnoredActor(Target);
			FHitResult Blocker;
			if (World->LineTraceSingleByChannel(Blocker, Center, TargetPoint, ECC_Visibility, LOSParams)
				&& Blocker.bBlockingHit)
			{
				// Something solid between the blast and the target that is neither of them: cover.
				continue;
			}
		}

		// Linear falloff, full inside InnerRadius. Clamped so a bad ini (Inner > Radius) cannot
		// produce a negative multiplier and heal the target.
		const float Falloff = (Distance <= InnerRadius || Radius <= InnerRadius)
			? 1.f
			: FMath::Clamp(1.f - (Distance - InnerRadius) / (Radius - InnerRadius), 0.f, 1.f);
		const float Amount = Damage * Falloff;
		if (Amount <= 0.f)
		{
			continue;
		}

		// The hit the context carries: the blast's own geometry, so the killfeed and any later
		// directional-damage UI have a real direction to read.
		FHitResult BlastHit;
		BlastHit.ImpactPoint = TargetPoint;
		BlastHit.ImpactNormal = (TargetPoint - Center).GetSafeNormal();
		BlastHit.Location = TargetPoint;
		BlastHit.bBlockingHit = true;

		UE_LOG(LogBN, Log, TEXT("BNProjectile: blast — %s for %.0f at %.0fuu."),
			*GetNameSafe(Target), Amount, Distance);

		BNDamage::ApplyDamage(Thrower, Target, Amount, BlastHit);
	}

	// Law 6: the bang is a cue, never a spawn from actor code. Routed through the THROWER's ASC
	// because that is what multicasts it — the projectile has no ASC of its own. A thrower who
	// died before the fuse ran leaves no ASC to route through, so the cue is skipped rather than
	// faked; the damage above has already been paid either way.
	if (UAbilitySystemComponent* ThrowerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Thrower))
	{
		FGameplayCueParameters Params;
		Params.Location = Center;
		Params.Normal = FVector::UpVector;
		Params.Instigator = Thrower;
		Params.SourceObject = this;
		Params.RawMagnitude = Radius;
		ThrowerASC->ExecuteGameplayCue(BNTags::GameplayCue_Grenade_Explode, Params);
	}
	else
	{
		UE_LOG(LogBN, Verbose, TEXT("BNProjectile: no thrower ASC at detonation — blast dealt, cue skipped."));
	}

	Destroy();
}

void ABNProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FuseTimer);
	Super::EndPlay(EndPlayReason);
}
