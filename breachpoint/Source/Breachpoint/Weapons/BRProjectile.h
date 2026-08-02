// Breachpoint. The server-authoritative projectile — D6's fourth unit of ARCHITECTURE §3.5.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "UObject/WeakObjectPtr.h"

#include "BRProjectile.generated.h"

class UAbilitySystemComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

USTRUCT()
struct FBRProjectileSpawnParams
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> InstigatorActor = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> InstigatorASC;

	FVector LaunchVelocity = FVector::ZeroVector;

	float Bounciness = -1.f;

	float BounceFriction = -1.f;

	float FuseSeconds = -1.f;

	float BlastRadiusMetres = 0.f;

	float BlastCentreDamage = 0.f;

	FName BlastFalloffCurveName = NAME_None;

	FGameplayTag ExplodeCueTag;
};

UCLASS()
class BREACHPOINT_API ABRProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABRProjectile();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static ABRProjectile* SpawnProjectile(
		UWorld* World,
		TSubclassOf<ABRProjectile> ProjectileClass,
		const FTransform& SpawnTransform,
		const FBRProjectileSpawnParams& InParams);

	void InitializeProjectile(const FBRProjectileSpawnParams& InParams);

	static bool ValidateSpawnParams(const FBRProjectileSpawnParams& InParams, FString& OutReason);

	bool HasDetonated() const { return bDetonated; }
	bool IsAtRest() const { return bAtRest; }
	const FBRProjectileSpawnParams& GetSpawnParams() const { return Params; }

protected:
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	void HandleFuseExpired();

	void Detonate();

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float CollisionRadiusCm = 8.f;

private:
	UPROPERTY()
	FBRProjectileSpawnParams Params;

	FTimerHandle FuseTimerHandle;

	bool bInitialized = false;
	bool bDetonated = false;
	bool bAtRest = false;
};
