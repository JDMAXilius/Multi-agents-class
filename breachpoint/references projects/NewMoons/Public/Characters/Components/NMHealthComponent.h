// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NMHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDeathEvent, APlayerState*, VictimState, APlayerState*, KillerState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDeathFinishedEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FHealthChanged, UNMHealthComponent*, HealthComponent, float, OldValue, float, NewValue, float, ScalarValue, AActor*, DamageInstigator, AActor*, DamageCauser);


UENUM(BlueprintType)
enum class ENMDeathState : uint8
{
	NotDead = 0,
	DeathStarted,
	DeathFinished
};

UCLASS(ClassGroup=(NM), meta=(BlueprintSpawnableComponent), Blueprintable)
class NEWMOONS_API UNMHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UNMHealthComponent();

	// Returns the health component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Health")
	static UNMHealthComponent* FindHealthComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UNMHealthComponent>() : nullptr); }

public:
	//Get current health
	UFUNCTION(BlueprintCallable, Category = "Health")
	FORCEINLINE float GetHealth() const { return Health; }
	// Get max health
	UFUNCTION(BlueprintCallable, Category = "Health")
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
	// Returns the current health in the range [0.0, 1.0]
	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealthNormalized() const;
	// Returns the current death state
	UFUNCTION(BlueprintCallable, Category = "Health")
	ENMDeathState GetDeathState() const { return DeathState; }
	// Returns true if it's dead or in the process of dying
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Health", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool IsDeadOrDying() const { return (DeathState > ENMDeathState::NotDead); }
	// Begins the death sequence for the owner.
	void StartDeath(APlayerState* InstigatorState = nullptr, APlayerState* VictimState = nullptr);
	// Ends the death sequence for the owner.
	void FinishDeath();
	// Handle Heal
	UFUNCTION(BlueprintCallable, Category = "Health")
	void TakeHeal(const float HealAmount, AActor* DamageInstigator = nullptr, AActor* DamageCauser = nullptr);
	//Handle Take Damage
	UFUNCTION(BlueprintCallable, Category = "Health")
	void TakeDamage(const float DamageAmount, AActor* DamageInstigator = nullptr, AActor* DamageCauser = nullptr, APlayerState* InstigatorState = nullptr, APlayerState* VictimState = nullptr);

public:
	// Delegate fired when health changes
	UPROPERTY(BlueprintAssignable)
	FHealthChanged OnHealthChanged;
	// Delegate fired when the death sequence has started.
	UPROPERTY(BlueprintAssignable)
	FDeathEvent OnDeathStarted;
	// Delegate fired when the death sequence has finished.
	UPROPERTY(BlueprintAssignable)
	FDeathFinishedEvent OnDeathFinished;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void DestroyComponent(bool bPromoteChildren) override;

	//Health replication
	UFUNCTION()
	void OnRep_Health(const float OldValue);
	UFUNCTION()
	void OnRep_MaxHealth(const float OldValue);
	UFUNCTION()
	void OnRep_DeathState(ENMDeathState OldDeathState);

private:
	// The current health. The health will be capped by the max health.
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Health, Category = "Health", Meta = (AllowPrivateAccess = true))
	float Health;
	// The current max health.
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_MaxHealth, Category = "Health", Meta = (AllowPrivateAccess = true))
	float MaxHealth;
	// Replicated state used to handle dying.
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_DeathState, Category = "Health", Meta = (AllowPrivateAccess = true))
	ENMDeathState DeathState;
};
