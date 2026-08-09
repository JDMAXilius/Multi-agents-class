// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapons/Projectiles/OSProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "Data/OSAbilityCostAndEffects.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Utilities/AbilityHelper.h"
#include "Utilities/BlueprintLibrary/OSCombatBlueprintLibrary.h"

AOSProjectile::AOSProjectile()
{
	bReplicates = true;
	SetReplicateMovement(true);

	// Collision — root component
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->SetSphereRadius(20.f);
	Collision->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetRootComponent(Collision);

	// Mesh — visual representation, swappable per Blueprint
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Niagara — VFX (trail, glow, etc.), set the asset per Blueprint
	NiagaraEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraEffect"));
	NiagaraEffect->SetupAttachment(Collision);
	NiagaraEffect->bAutoActivate = true;

	// Projectile movement
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bAutoActivate = false;
}

void AOSProjectile::InitializeProjectile(const FOSProjectileInit& InInit)
{
	Init = InInit;
}

void AOSProjectile::ApplyInit()
{
	ForceNetUpdate();

	UE_LOG(LogTemp, Log, TEXT("[ProjectileDebug] ApplyInit: loc=%s dir=%s speed=%.1f lifeSeconds=%.1f sphereRadius=%.1f"),
		*GetActorLocation().ToString(), *Init.Dir.ToString(), Speed, LifeSeconds,
		Collision ? Collision->GetScaledSphereRadius() : -1.f);

	if (Init.SourceActor)
		UE_LOG(LogTemp, Log, TEXT("[ProjectileDebug] ApplyInit: distFromSource=%.1f source=%s"),
			FVector::Dist(GetActorLocation(), Init.SourceActor->GetActorLocation()),
			*GetNameSafe(Init.SourceActor));

	// Ignore the source actor so the projectile doesn't collide with the caster
	if (Init.SourceActor)
	{
		Collision->MoveIgnoreActors.Add(Init.SourceActor);
	}
	else if (Init.SourceASC)
	{
		if (auto actor = Init.SourceASC->GetAvatarActor())
			Collision->MoveIgnoreActors.Add(actor);
	}

	if (LifeSeconds > 0.f)
	{
		SetLifeSpan(LifeSeconds);
	}

	const FVector Vel = FVector(Init.Dir) * Speed;

	ProjectileMovement->Velocity = Vel;
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = FMath::Max(ProjectileMovement->MaxSpeed, Speed);
	ProjectileMovement->Activate(true);
	ProjectileMovement->UpdateComponentVelocity();

	Collision->ComponentVelocity = Vel;
}

void AOSProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AOSProjectile, Init);
}

const FOSSpellTriggerPayload* AOSProjectile::FindPayload(EOSSpellTrigger Trigger) const
{
	return Payloads.FindByPredicate(
		[Trigger](const FOSSpellTriggerPayload& P) { return P.Trigger == Trigger; });
}

void AOSProjectile::ApplyEffectsTo(const AActor* Actor, const TArray<FOSGameplayEffectWrapper>& Array)
{
	if (!Actor) return;
	auto targetASC = AbilityHelper::GetOSASCFromActor(Actor);
	if (!targetASC) return;
	auto source = Init.SourceASC;
	if (!source) return;
	
	// Build context from SOURCE ASC (not target). The instigator/causer must reference
	// the caster, not the victim. Previous code used targetASC->MakeEffectContext() which
	// made every downstream system think the victim hit themselves.
	auto ctx = source->MakeEffectContext();
	ctx.AddSourceObject(source);

	for (auto effect : Array)
	{
		AbilityHelper::ApplyEffect(source, targetASC, ctx, effect);
	}
}

void AOSProjectile::OnHit(const AActor* OtherActor)
{
	if (!HasAuthority()) return;

	if (const FOSSpellTriggerPayload* P = FindPayload(EOSSpellTrigger::OnHit))
	{
		ApplyEffectsTo(OtherActor, P->Effects);
	}

	Destroy();
}

void AOSProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bHitProcessed) return;
	if (!OtherActor || OtherActor == this) return;

	UE_LOG(LogTemp, Log, TEXT("[Projectile] HandleOverlap: Other=%s Source=%s SourceASC=%s"),
		*GetNameSafe(OtherActor), *GetNameSafe(Init.SourceActor), Init.SourceASC ? TEXT("valid") : TEXT("null"));

	// Skip caster
	if (Init.SourceActor && OtherActor == Init.SourceActor) return;

	// Skip actors without an ASC (trigger volumes, non-combat geometry)
	if (!AbilityHelper::GetOSASCFromActor(OtherActor)) return;

	// Team filter: projectiles pass through teammates without detonating.
	// The projectile continues moving (not destroyed) and OnComponentBeginOverlap fires
	// independently for the next actor it hits. Covers frost bolt, fire bolt, and any
	// future projectile subclass using this HandleOverlap path.
	if (UOSCombatBlueprintLibrary::AreActorsFriendly(Init.SourceActor, OtherActor)) return;

	bHitProcessed = true;
	OnHit(OtherActor);
}

void AOSProjectile::OnRep_Init()
{
	ApplyInit();
}

void AOSProjectile::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("[ProjectileDebug] BeginPlay: loc=%s rot=%s"),
		*GetActorLocation().ToString(), *GetActorRotation().ToString());

	Collision->OnComponentBeginOverlap.AddDynamic(this, &AOSProjectile::HandleOverlap);
}
