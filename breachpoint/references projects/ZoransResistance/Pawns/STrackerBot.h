// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "ZoransPawnBase.h"
#include "AbilitySystemInterface.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"
#include "ZoransResistance/Teams/TeamStateInterface.h"
#include "STrackerBot.generated.h"

class UZoransHealthComponent;
class USphereComponent;
class USoundCue;

UCLASS()
class ZORANSRESISTANCE_API ASTrackerBot : public AZoransPawnBase
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ASTrackerBot();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;

	
	
	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	USphereComponent* SphereComp;
	FVector GetNextPathPoint();

	// Next point in navigation path
	FVector NextPathPoint;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	float MovementForce;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	bool bUseVelocityChange;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	float RequiredDistanceToTarget;

	// Dynamic material to pulse on damage
	UMaterialInstanceDynamic* MatInst;

	void SelfDestruct();

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	UParticleSystem* ExplosionEffect;

	bool bExploded;

	// Did we already kick off self destruct timer
	bool bStartedSelfDestruction;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	float ExplosionRadius;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	float ExplosionDamage;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	float SelfDamageInterval;

	FTimerHandle TimerHandle_SelfDamage;

	void DamageSelf();

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	USoundCue* SelfDestructSound;

	UPROPERTY(EditDefaultsOnly, Category = "TrackerBot")
	USoundCue* ExplodeSound;

	TWeakObjectPtr<UZoransHealthComponent> HealthComponent = nullptr;

	// Find nearby enemies and grow in 'power level' based on the amount.
	void OnCheckNearbyBots();

	// the power boost of the bot, affects damaged caused to enemies and color of the bot (range: 1 to 4)
	int32 PowerLevel;

	FTimerHandle TimerHandle_RefreshPath;

	void RefreshPath();

	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	
};
