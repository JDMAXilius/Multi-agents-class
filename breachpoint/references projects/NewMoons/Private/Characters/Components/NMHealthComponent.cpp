// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Components/NMHealthComponent.h"

#include "Game/NMGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
UNMHealthComponent::UNMHealthComponent()
	: Health(100.f)
	, MaxHealth(100.f)
	, DeathState(ENMDeathState::NotDead)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UNMHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UNMHealthComponent, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNMHealthComponent, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME(UNMHealthComponent, DeathState);
}

// Called when the game starts
void UNMHealthComponent::BeginPlay()
{
	Super::BeginPlay();	
}

void UNMHealthComponent::DestroyComponent(bool bPromoteChildren)
{
	OnHealthChanged.Clear();
	OnDeathStarted.Clear();
	OnDeathFinished.Clear();
	
	Super::DestroyComponent(bPromoteChildren);
}

void UNMHealthComponent::OnRep_Health(const float OldValue)
{
	OnHealthChanged.Broadcast(this, OldValue, Health, GetHealthNormalized(), nullptr, nullptr);
}

void UNMHealthComponent::OnRep_MaxHealth(const float OldValue)
{
	OnHealthChanged.Broadcast(this, OldValue, Health, GetHealthNormalized(), nullptr, nullptr);
}

void UNMHealthComponent::OnRep_DeathState(ENMDeathState OldDeathState)
{
	const ENMDeathState NewDeathState = DeathState;

	// Revert the death state for now since we rely on StartDeath and FinishDeath to change it.
	DeathState = OldDeathState;

	if (OldDeathState > NewDeathState)
	{
		// The server is trying to set us back but we've already predicted past the server state.
		UE_LOG(LogTemp, Warning, TEXT("HealthComponent: Predicted past server death state [%d] -> [%d] for owner [%s]."), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		return;
	}

	if (OldDeathState == ENMDeathState::NotDead)
	{
		if (NewDeathState == ENMDeathState::DeathStarted)
		{
			StartDeath();
		}
		else if (NewDeathState == ENMDeathState::DeathFinished)
		{
			StartDeath();
			FinishDeath();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HealthComponent: Invalid death transition [%d] -> [%d] for owner [%s]."), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		}
	}
	else if (OldDeathState == ENMDeathState::DeathStarted)
	{
		if (NewDeathState == ENMDeathState::DeathFinished)
		{
			FinishDeath();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HealthComponent: Invalid death transition [%d] -> [%d] for owner [%s]."), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		}
	}

	ensureMsgf((DeathState == NewDeathState), TEXT("HealthComponent: Death transition failed [%d] -> [%d] for owner [%s]."), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
}

float UNMHealthComponent::GetHealthNormalized() const
{
	return MaxHealth > 0.0f ? (Health / MaxHealth) : 0.0f;
}

void UNMHealthComponent::StartDeath(APlayerState* InstigatorState, APlayerState* VictimState)
{
	if (DeathState != ENMDeathState::NotDead)
	{
		return;
	}

	DeathState = ENMDeathState::DeathStarted;

	AActor* Owner = GetOwner();
	check(Owner);
	//if (InstigatorState && VictimState)
	OnDeathStarted.Broadcast(InstigatorState, VictimState);
	Owner->ForceNetUpdate();
}

void UNMHealthComponent::FinishDeath()
{
	if (DeathState != ENMDeathState::DeathStarted)
	{
		return;
	}

	DeathState = ENMDeathState::DeathFinished;

	AActor* Owner = GetOwner();
	check(Owner);
	//if (InstigatorState && VictimState)
	OnDeathFinished.Broadcast();
	Owner->ForceNetUpdate();
}

void UNMHealthComponent::TakeHeal(const float HealAmount, class AActor* DamageInstigator, AActor* DamageCauser)
{
	if (GetOwnerRole() < ROLE_Authority || HealAmount <= 0.0f || Health >= MaxHealth || DeathState != ENMDeathState::NotDead) return;

	const float OldHealth = GetHealth();
	Health = FMath::Clamp(Health + HealAmount, 0.f, MaxHealth);
	UE_LOG(LogTemp, Warning, TEXT("HealthComponent: Healed by [%.2f] -> Health [%.2f]"), HealAmount, Health);
	OnHealthChanged.Broadcast(this, OldHealth, Health, GetHealthNormalized(), DamageInstigator, DamageCauser);
}

void UNMHealthComponent::TakeDamage(const float DamageAmount, class AActor* DamageInstigator, AActor* DamageCauser, APlayerState* InstigatorState, APlayerState* VictimState)
{
	if (GetOwnerRole() < ROLE_Authority || DamageAmount <= 0.0f || DeathState != ENMDeathState::NotDead) return;

	const float OldHealth = GetHealth();
	Health = FMath::Clamp(Health - DamageAmount, 0.f, MaxHealth);
	UE_LOG(LogTemp, Warning, TEXT("HealthComponent: Damaged by [%.2f] -> Health [%.2f]"), DamageAmount, Health);
	OnHealthChanged.Broadcast(this, OldHealth, Health, GetHealthNormalized(), DamageInstigator, DamageCauser);

	if (Health <= 0.0f)
	{
		StartDeath(InstigatorState, VictimState);
	}
}