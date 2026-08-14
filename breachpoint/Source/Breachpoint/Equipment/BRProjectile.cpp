#include "Equipment/BRProjectile.h"

#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/BRCore.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"
#include "Equipment/BRExplosion.h"

namespace
{
	constexpr float ProjectileNetUpdateHz = 10.f;
	constexpr float ProjectileMinNetUpdateHz = 2.f;
}

ABRProjectile::ABRProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;

	SetReplicatingMovement(true);

	NetDormancy = DORM_Awake;
	SetNetUpdateFrequency(ProjectileNetUpdateHz);
	SetMinNetUpdateFrequency(ProjectileMinNetUpdateHz);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(CollisionRadiusCm);

	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(BRCollision::Projectile);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	CollisionComponent->SetCollisionResponseToChannel(BRCollision::Projectile, ECR_Ignore);

	CollisionComponent->SetGenerateOverlapEvents(false);
	CollisionComponent->CanCharacterStepUpOn = ECB_No;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetGenerateOverlapEvents(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));

	ProjectileMovement->bShouldBounce = true;

	ProjectileMovement->Velocity = FVector::ZeroVector;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;

	ProjectileMovement->InitialSpeed = 0.f;

	ProjectileMovement->MaxSpeed = 0.f;
}

bool ABRProjectile::ValidateSpawnParams(const FBRProjectileSpawnParams& InParams, FString& OutReason)
{
	if (!IsValid(InParams.InstigatorActor.Get()))
	{
		OutReason = TEXT("no instigator actor — a projectile with no thrower produces a kill with no killer");
		return false;
	}
	if (!InParams.InstigatorASC.IsValid())
	{
		OutReason = TEXT("no instigator ASC — the damage spec has no source and could not be attributed");
		return false;
	}
	if (InParams.LaunchVelocity.IsNearlyZero())
	{
		OutReason = TEXT("zero launch velocity — the projectile would be dropped on the thrower's boots");
		return false;
	}
	if (InParams.FuseSeconds < 0.f)
	{
		OutReason = FString::Printf(TEXT("fuse is %.3f s; negative is unset, and a projectile with no fuse never detonates"), InParams.FuseSeconds);
		return false;
	}
	if (InParams.BlastRadiusMetres <= 0.f || InParams.BlastCentreDamage <= 0.f)
	{
		OutReason = FString::Printf(TEXT("blast radius %.2f m / centre damage %.2f; both come from data and neither may be invented"),
			InParams.BlastRadiusMetres, InParams.BlastCentreDamage);
		return false;
	}
	if (InParams.BlastFalloffCurveName.IsNone())
	{
		OutReason = TEXT("no falloff curve name — the detonation would have no authored shape and this class may not compute one");
		return false;
	}

	return true;
}

ABRProjectile* ABRProjectile::SpawnProjectile(
	UWorld* World,
	TSubclassOf<ABRProjectile> ProjectileClass,
	const FTransform& SpawnTransform,
	const FBRProjectileSpawnParams& InParams)
{
	if (!World)
	{
		return nullptr;
	}

	if (World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	if (!ProjectileClass)
	{
		return nullptr;
	}

	FString Reason;
	if (!ValidateSpawnParams(InParams, Reason))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = InParams.InstigatorActor.Get();
	SpawnParams.Instigator = Cast<APawn>(InParams.InstigatorActor.Get());

	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABRProjectile* Projectile = World->SpawnActorDeferred<ABRProjectile>(
		ProjectileClass,
		SpawnTransform,
		SpawnParams.Owner,
		SpawnParams.Instigator,
		SpawnParams.SpawnCollisionHandlingOverride);

	if (!Projectile)
	{
		return nullptr;
	}

	Projectile->InitializeProjectile(InParams);
	Projectile->FinishSpawning(SpawnTransform);
	return Projectile;
}

void ABRProjectile::InitializeProjectile(const FBRProjectileSpawnParams& InParams)
{
	if (!ensureMsgf(HasAuthority(), TEXT("ABRProjectile::InitializeProjectile is server-only.")))
	{
		return;
	}
	if (bInitialized)
	{
		return;
	}

	FString Reason;
	if (!ValidateSpawnParams(InParams, Reason))
	{
		return;
	}

	Params = InParams;
	bInitialized = true;

	if (!ProjectileMovement)
	{
		return;
	}

	ProjectileMovement->Velocity = InParams.LaunchVelocity;

	if (InParams.Bounciness >= 0.f)
	{
		ProjectileMovement->Bounciness = InParams.Bounciness;
	}
	if (InParams.BounceFriction >= 0.f)
	{
		ProjectileMovement->Friction = InParams.BounceFriction;
	}
}

void ABRProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionComponent)
	{
		CollisionComponent->SetSphereRadius(CollisionRadiusCm);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ABRProjectile::HandleProjectileStop);
	}

	if (!HasAuthority())
	{
		return;
	}

	if (!bInitialized)
	{
		Destroy();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		Destroy();
		return;
	}

	World->GetTimerManager().SetTimer(
		FuseTimerHandle, this, &ABRProjectile::HandleFuseExpired,
		FMath::Max(Params.FuseSeconds, UE_KINDA_SMALL_NUMBER), false);
}

void ABRProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FuseTimerHandle);
	}
	FuseTimerHandle.Invalidate();

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ABRProjectile::HandleProjectileStop);
	}

	Super::EndPlay(EndPlayReason);
}

void ABRProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	bAtRest = true;

	if (!HasAuthority())
	{
		return;
	}

	SetNetDormancy(DORM_DormantAll);
}

void ABRProjectile::HandleFuseExpired()
{
	Detonate();
}

void ABRProjectile::Detonate()
{
	if (!HasAuthority())
	{
		return;
	}
	if (bDetonated)
	{
		return;
	}
	bDetonated = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FuseTimerHandle);
	}
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
	}

	const FVector Epicentre = GetActorLocation();

	const int32 Damaged = BRExplosion::ApplyExplosionDamage(
		Params.InstigatorASC.Get(),
		Params.InstigatorActor.Get(),
		Epicentre,
		Params.BlastRadiusMetres,
		Params.BlastCentreDamage,
		Params.BlastFalloffCurveName,
		Params.ExplodeCueTag);

	FlushNetDormancy();
	Destroy();
}
