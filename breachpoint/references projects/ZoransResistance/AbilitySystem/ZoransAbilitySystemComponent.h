// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTasks/PlayMontageAndWaitForEvent.h"
#include "ZoransResistance/AbilitySystem/TargetData/ZoransTargetActor.h"
#include "ZoransResistance/Projectiles/ZoransProjectileBase.h"
#include "ZoransAbilitySystemComponent.generated.h"

class UMovementAttributes;
class UCharacterAttributes;
class UHealthAttributes;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityGrantedDelegate, const FGameplayAbilitySpec&, GrantedAbilitySpec);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAbilityGrantedDelegate, UZoransGameplayAbility*, NewAbility);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAbilityRemovedDelegate, UZoransGameplayAbility*, RemovedAbility);

UCLASS()
class ZORANSRESISTANCE_API UZoransAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnAbilityGrantedDelegate OnAbilityGrantedDelegate;
	
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	AZoransTargetActor* GetTargetingActor() const { return TargetingActor.Get(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Zorans Projectiles")
	int32 GenerateLocalProjectileID();

	void InternalProcessServerHitResult(const int32 InProjectileIndex,const FHitResult& InHitResult);

	UFUNCTION(Client, Reliable, Category = "Zorans Projectiles")
	void ProcessProjectileHitResult(const int32 InProjectileIndex, const FHitResult& InHitResult);

	UFUNCTION(BlueprintCallable, Category = "Zorans Projectiles")
	void RegisterLocalProjectile(const int32 InProjectileIndex, AZoransProjectileBase* InLocalProjectile);

	void InternalProcessServerProjectileBounce(const int32 InProjectileIndex, const TArray<FBouncingProjectilePathSegment> &InBouncePath);
	
	UFUNCTION(Client, Reliable, Category = "Zorans Projectiles")
	void ProcessServerProjectileBounce(const int32 InProjectileIndex, const TArray<FBouncingProjectilePathSegment> &InBouncePath);
	
	void SetupAbilityEnhancedInputBinds();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Abilities")
	void CancelAbilitiesOnDeath();

	UPROPERTY(BlueprintAssignable)
	FAbilityGrantedDelegate AbilityGrantedDelegate;

	UPROPERTY(BlueprintAssignable)
	FAbilityRemovedDelegate AbilityRemovedDelegate;

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void CheckAttributeSets();

protected:
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	
	virtual void BeginPlay() override;
	
	UPROPERTY()
	TObjectPtr<AZoransTargetActor> TargetingActor;
	
	int32 LocalProjectileIndex = 0;

	static inline int32 ProjectileMaxIndex = 2000000000;

	void UnregisterLocalProjectile(int32 InProjectileIndex);

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Zorans Projectiles")
	TMap<int32, AZoransProjectileBase*> RegisteredProjectiles;

	UPROPERTY()
	const UAttributeSet* HealthAttributeSet;

	UPROPERTY()
	const UAttributeSet* CharacterAttributeSet;
	
	UPROPERTY()
	const UAttributeSet* MovementAttributeSet;
};