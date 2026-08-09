// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayEffect.h"
#include "ZoransResistance/Game/ZoransGameState.h"
#include "ZoransHealthComponent.generated.h"

class UHealthAttributes;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FHealthChangeDelegate, float, CurrentHealthValue, float, OldHealthValue, float, MaxHealth, float, HealthPercentage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FShieldChangeDelegate, float, CurrentShieldValue, float, OldShieldValue, float, MaxShield, float, ShieldPercentage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FDamageReceivedDelegate, const float, DamageAmount, const AActor*, DamageCauser, const AActor*, OptionalEffectSource, const UObject*, SourceAbilitySystemComponent, const bool, bFatal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathDelegate);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ZORANSRESISTANCE_API UZoransHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZoransHealthComponent();

	AZoransGameState* GetZoransGameState();

	void HandleDeathEvent(const FGameplayEffectSpec& DeathSpec) const;
	
	UPROPERTY(BlueprintAssignable)
	FHealthChangeDelegate OnHealthChange;

	UPROPERTY(BlueprintAssignable)
	FShieldChangeDelegate OnShieldChange;

	UPROPERTY(BlueprintAssignable)
	FDamageReceivedDelegate OnDamageReceived;

	UPROPERTY(BlueprintAssignable)
	FDamageReceivedDelegate OnDamageDealt;

	UPROPERTY(BlueprintAssignable)
	FOnDeathDelegate OnDeath;
	
	UFUNCTION(BlueprintCallable)
	void InitHealthComponent(AActor* InOwningActor);
	
	UFUNCTION(BlueprintCallable, Category = "Death")
	void On_DeathFinished(APlayerState* SourcePlayerState, const APlayerState* TargetPlayerState, const UTexture2D* KillfeedIcon, const bool bWeakPoint = false);
	
	void On_DamageReceived(const FGameplayEffectSpec& EffectSpec, AActor* AvatarActor, const float DamageDealt, const bool bFatal, const bool bOverrideHitType = false, const bool bSkipReaction = false);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Death")
	bool IsAlive() const { return EvaluatedHealth > 0.f; }

	bool IsComponentInitialized() const { return bIsInitialized; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Health")
	float GetHealthRatio() const;
	
	UPROPERTY(BlueprintReadOnly)
	float EvaluatedHealth = 100.f;

	UPROPERTY(BlueprintReadOnly)
	float EvaluatedShield = 100.f;
	
protected:
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Kinetic Charge")
	TSubclassOf<UZoransGameplayEffect> KineticChargeEffect;
	
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<AZoransGameState> ZoransGameState;

	UPROPERTY()
	mutable const UHealthAttributes* HealthAttributes;

	bool bIsInitialized = false;
	bool bIsDead = false;
	bool bDeathEventHandle = false;
	
	FDelegateHandle CurrentHealthChangeDelegate;
	FDelegateHandle MaximumHealthChangeDelegate;
	FDelegateHandle CurrentShieldChangeDelegate;
	FDelegateHandle MaximumShieldChangeDelegate;
	
	void On_CurrentHealthChanged(const FOnAttributeChangeData& Data);
	void On_MaxHealthChanged(const FOnAttributeChangeData& Data) const;

	void On_CurrentShieldChanged(const FOnAttributeChangeData& Data);
	void On_MaxShieldChanged(const FOnAttributeChangeData& Data) const;

	virtual void DestroyComponent(bool bPromoteChildren) override;

};