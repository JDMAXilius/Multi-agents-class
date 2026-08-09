// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
// Projectile: self-contained replicated actor. Blueprint configures speed, lifetime, VFX, and payload effects.
// Ability only passes direction + source actor via FOSProjectileInit at spawn time.
// Default components: SphereComponent (collision root), StaticMeshComponent, NiagaraComponent, ProjectileMovementComponent.

#include "CoreMinimal.h"
#include "Data/OSAbilityCostAndEffects.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "OSProjectile.generated.h"


struct FOSGameplayEffectWrapper;
class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
enum class EOSSpellTrigger : uint8;
class UProjectileMovementComponent;

/** Runtime data passed from the ability at spawn time — only things the ability knows. */
USTRUCT()
struct FOSProjectileInit
{
	GENERATED_BODY()

	UPROPERTY() FVector_NetQuantizeNormal Dir = FVector::ForwardVector;
	UPROPERTY() TObjectPtr<UAbilitySystemComponent> SourceASC = nullptr;
	UPROPERTY() TObjectPtr<AActor> SourceActor = nullptr;
};


UCLASS()
class ONSIGHT_API AOSProjectile : public AActor
{
	GENERATED_BODY()

public:
	AOSProjectile();

	/** Runtime init from the spawning ability (direction + source only) */
	UPROPERTY(ReplicatedUsing=OnRep_Init)
	FOSProjectileInit Init;

	void InitializeProjectile(const FOSProjectileInit& InInit);
	void ApplyInit();

	// ========== Default Components ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UNiagaraComponent> NiagaraEffect;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// ========== Blueprint-Configured Defaults ==========

	/** Projectile speed (units/sec) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float Speed = 3000.f;

	/** Lifetime in seconds before the projectile is destroyed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = 0.1))
	float LifeSeconds = 5.f;

	/** Effects to apply per trigger event (OnHit, OnExpire, OnDestroy) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effects")
	TArray<FOSSpellTriggerPayload> Payloads;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Init();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const FOSSpellTriggerPayload* FindPayload(EOSSpellTrigger Trigger) const;
	void ApplyEffectsTo(const AActor* Actor, const TArray<FOSGameplayEffectWrapper>& Array);
	void OnHit(const AActor* OtherActor);

private:
	/** Guard against re-entrant overlap when Destroy is deferred. */
	bool bHitProcessed = false;

	UFUNCTION()
	void HandleOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};
