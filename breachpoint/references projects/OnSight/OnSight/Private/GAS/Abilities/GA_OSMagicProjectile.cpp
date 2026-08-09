// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Abilities/GA_OSMagicProjectile.h"
#include "Characters/OSCharacter.h"
#include "Data/OSGameplayTags.h"
#include "Weapons/Projectiles/OSProjectile.h"

UGA_OSMagicProjectile::UGA_OSMagicProjectile()
{
	// Shared cooldown slot for all magic projectile abilities. BPs only need to set CooldownDuration.
	CooldownTags.AddTag(FOSGameplayTags::Get().Cooldown_Magic_Projectile);
}

// --- Magic Event ---

void UGA_OSMagicProjectile::OnMagicEventReceived_Implementation(FGameplayEventData Payload)
{
	if (!HasAuthority(&CurrentActivationInfo))
		return;

	SpawnProjectile();
}

// --- Projectile Spawning ---

void UGA_OSMagicProjectile::SpawnProjectile()
{
	AOSCharacter* Char = GetOwningCharacter();
	if (!Char)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] SpawnProjectile: no owning character"), *GetName());
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] SpawnProjectile: ProjectileClass is null"), *GetName());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] SpawnProjectile: world is null"), *GetName());
		return;
	}

	const FTransform SpawnTransform = ResolveOriginTransform();

	UE_LOG(LogTemp, Log, TEXT("[ProjectileDebug] SpawnProjectile: spawnLoc=%s charLoc=%s distFromChar=%.1f time=%.3f"),
		*SpawnTransform.GetLocation().ToString(), *Char->GetActorLocation().ToString(),
		FVector::Dist(SpawnTransform.GetLocation(), Char->GetActorLocation()),
		GetWorld()->GetTimeSeconds());

	/* Deferred spawn so we can seed Init.SourceActor BEFORE FinishSpawning. When the projectile
	   origin is at a bone socket (e.g. hand_r), the collision sphere is inside the caster's
	   capsule. During FinishSpawning -> BeginPlay the initial overlap would trigger HandleOverlap
	   -> OnHit -> Destroy() unless the self-check has a valid SourceActor to compare against. */
	AOSProjectile* Projectile = World->SpawnActorDeferred<AOSProjectile>(
		ProjectileClass, SpawnTransform, Char, Char,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Projectile)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] SpawnProjectile: SpawnActorDeferred failed"), *GetName());
		return;
	}

	// Seed Init BEFORE FinishSpawning so the self-overlap check passes during BeginPlay.
	FOSProjectileInit InitData;
	InitData.Dir = SpawnTransform.GetRotation().GetForwardVector();
	InitData.SourceActor = Char;
	InitData.SourceASC = GetAbilitySystemComponentFromActorInfo();
	Projectile->InitializeProjectile(InitData);

	Projectile->FinishSpawning(SpawnTransform);

	if (!IsValid(Projectile))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] SpawnProjectile: FinishSpawning destroyed the actor"), *GetName());
		return;
	}

	/* ApplyInit activates movement, adds MoveIgnoreActors, and sets lifespan. Runs after the actor
	   is fully registered so ProjectileMovement drives off the final world transform. */
	Projectile->ApplyInit();
}
