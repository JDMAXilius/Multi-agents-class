#include "Weapons/BNProjectile.h"

#include "AI/BNBotController.h"
#include "Core/AIBBotController.h"
#include "AbilitySystem/Effects/BNDamage.h"
#include "GameFramework/PlayerState.h"
#include "Perception/AISense_Hearing.h"
#include "BreachpointNext.h"
#include "Core/BNGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
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
	// Assigned, not SetUpdatedComponent(): the setter registers delegates and walks the component
	// hierarchy, which is not safe while the CDO is still being built. The engine's own projectiles
	// assign the property here and let the component bind on registration.
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->ProjectileGravityScale = 1.f;
	ProjectileMovement->bRotationFollowsVelocity = false;
}

void ABNProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABNProjectile, InitialVelocity);
}

void ABNProjectile::OnRep_InitialVelocity()
{
	// The arc, reproduced. Without this a client's grenade spawns at REST — Launch() runs on the
	// server only and a projectile movement component's Velocity is not part of anything that
	// replicates — so it would drop at the thrower's feet and then be yanked along by replicated
	// movement corrections. Same initial velocity + same gravity = the same flight on every
	// machine, with ReplicatedMovement only nudging it.
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = InitialVelocity;
	}
}

void ABNProjectile::BeginPlay()
{
	Super::BeginPlay();

	ProjectileMovement->Bounciness = Bounciness;

	// The grenade leaves the hand INSIDE the thrower's capsule (forward 50, up 50 against a ~34uu
	// radius), so without this it bounces straight off them and back into their face on the first
	// frame. Ignoring the thrower for movement is what lets it get clear.
	if (APawn* Thrower = GetInstigator())
	{
		CollisionComponent->IgnoreActorWhenMoving(Thrower, true);
		// N1: the attribution capture, taken NOW because this is the one moment the
		// pawn link is guaranteed alive — see the member's comment.
		ThrowerPlayerState = Thrower->GetPlayerState();
	}

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
		const float Fuse = FMath::Max(FuseTime, 0.05f);
		GetWorldTimerManager().SetTimer(FuseTimer, this, &ABNProjectile::Explode, Fuse, /*bLoop=*/false);

		// R10.4 — the bots get told a beat before the bang. Armed on the SAME clock as the fuse
		// and never after it: a warning that arrives with the explosion is not a warning.
		const float WarnAt = Fuse - FMath::Max(0.f, BotWarnLeadSeconds);
		if (WarnAt > 0.05f)
		{
			GetWorldTimerManager().SetTimer(WarnTimer, this, &ABNProjectile::WarnNearbyBots, WarnAt, /*bLoop=*/false);
		}
		else
		{
			// A fuse shorter than the lead time: warn immediately rather than not at all. Nobody
			// dodges it, but the bots at least react like something is happening.
			WarnNearbyBots();
		}
	}
}

void ABNProjectile::WarnNearbyBots()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return;
	}

	const FVector Center = GetActorLocation();
	const double DetonateAt = World->GetTimeSeconds() + GetWorldTimerManager().GetTimerRemaining(FuseTimer);

	// THE BLAST'S OWN RADIUS is the danger zone — the same number the damage uses, so a bot is
	// warned exactly when it would have been hurt. Same overlap shape as Explode below it; no
	// line-of-sight test, deliberately: a bot on the far side of a wall is not in danger, but
	// stepping away from a wall a grenade is about to go off behind is not wrong either, and
	// paying for a trace per bot to prevent a harmless move is a poor trade.
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps, Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius));

	TSet<AActor*> Warned;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Target) || Warned.Contains(Target))
		{
			continue;
		}
		Warned.Add(Target);

		const APawn* Pawn = Cast<APawn>(Target);
		AController* Controller = Pawn ? Pawn->GetController() : nullptr;
		if (ABNBotController* Bot = Cast<ABNBotController>(Controller))
		{
			Bot->NotifyIncomingBlast(Center, DetonateAt, Radius);
		}
		// The seam audit's HIGH hazard, closed: without this branch AIB bots shipped
		// with grenade evasion silently dead. The AIB side runs its own perceivability
		// gate and reaction clock — this call is a note, never a dodge.
		else if (AAIBBotController* AIBBot = Cast<AAIBBotController>(Controller))
		{
			AIBBot->NoteIncomingBlast(Center, Radius, DetonateAt);
		}
	}
}

void ABNProjectile::Launch(const FVector& Direction)
{
	if (!HasAuthority())
	{
		return;
	}
	// Replicated so every client reproduces the same arc from the same start — see
	// OnRep_InitialVelocity. Set before the first replication of this actor, which is why
	// UBNGA_Grenade launches it in the same frame it spawns it.
	InitialVelocity = Direction.GetSafeNormal() * LaunchSpeed;
	ProjectileMovement->Velocity = InitialVelocity;
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
	AActor* Thrower = GetInstigator() ? Cast<AActor>(GetInstigator()) : GetOwner();

	// N1 (BN15 REFUTER): a thrower killed while this was in flight has been unpossessed —
	// its pawn resolves NO PlayerState, the FF gate reads NoTeam, and the blast would
	// shred teammates with friendly fire off. The captured PlayerState IS the thrower for
	// every purpose downstream (BN's ASC lives on the PlayerState, so spec attribution
	// and the killfeed survive the pawn's death too). Live pawn: nothing changes.
	{
		const APawn* ThrowerPawn = Cast<APawn>(Thrower);
		const bool bPawnRouteDead = !Thrower || (ThrowerPawn && !ThrowerPawn->GetPlayerState());
		if (bPawnRouteDead && ThrowerPlayerState.IsValid())
		{
			Thrower = ThrowerPlayerState.Get();
		}
	}

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

		BNDamage::ApplyDamage(Thrower, Target, Amount, BlastHit, BNDamageSource::Grenade);
	}


	// R10 — and it is HEARD. The loudest event in the game, reported at the blast's own centre on
	// the authority: a grenade going off across the arena is exactly the noise that should pull
	// bots toward a fight they are not yet in.
	UAISense_Hearing::ReportNoiseEvent(World, Center, /*Loudness=*/2.f, Thrower,
		/*MaxRange=*/0.f, /*Tag=*/FName(TEXT("BNGrenadeBlast")));
	// Law 6: the bang is a cue, never a spawn from actor code. But it is multicast from the
	// PROJECTILE, not routed through the thrower's ASC — that route early-returns wherever the
	// thrower's AvatarActor is null on the receiving machine, so an observer who had culled or
	// never seen the thrower would take the blast in silence. This actor is relevant to everyone
	// who can see the explosion, by definition, because it is standing where the explosion is.
	MulticastExplosion(Center);

	// NOT Destroy(): a multicast sent by an actor torn down in the same frame is not reliably
	// delivered. Hide it, stop it, and let it die a beat later.
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	ProjectileMovement->StopMovementImmediately();
	SetLifeSpan(0.5f);
}

void ABNProjectile::MulticastExplosion_Implementation(const FVector Center)
{
	// Runs on every machine including the server. The cue manager is the same door
	// UGameplayCueNotify_Static handlers are reached through, so UBNGameplayCue_Explosion answers
	// here exactly as it would through an ASC — law 6 is satisfied by the handler, not the router.
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!CueManager)
	{
		return;
	}

	FGameplayCueParameters Params;
	Params.Location = Center;
	Params.Normal = FVector::UpVector;
	Params.Instigator = GetInstigator();
	Params.SourceObject = this;
	Params.RawMagnitude = Radius;
	CueManager->HandleGameplayCue(this, BNTags::GameplayCue_Grenade_Explode, EGameplayCueEvent::Executed, Params);
}

void ABNProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FuseTimer);
	GetWorldTimerManager().ClearTimer(WarnTimer);
	Super::EndPlay(EndPlayReason);
}
