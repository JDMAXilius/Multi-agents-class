// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/NMPlayerState.h"
#include "Characters/Components/NMHealthComponent.h"
#include "Characters/Components/NMWeaponComponent.h"
#include "Net/UnrealNetwork.h"

ANMPlayerState::ANMPlayerState()
{
	
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	NetUpdateFrequency = 60.0f;
	
	// Create health component
	HealthComponent = CreateDefaultSubobject<UNMHealthComponent>(TEXT("HealthComponent"));
	// Create weapon component
	WeaponComponent = CreateDefaultSubobject<UNMWeaponComponent>(TEXT("WeaponComponent"));
}

void ANMPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(ANMPlayerState, PlayerKills, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ANMPlayerState, PlayerDeaths, COND_None, REPNOTIFY_Always);
}

UNMHealthComponent* ANMPlayerState::GetHealthComponent() const
{
	return HealthComponent.Get();	
}

UNMWeaponComponent* ANMPlayerState::GetWeaponComponent() const
{
	return WeaponComponent.Get();
}

void ANMPlayerState::OnRep_Score()
{
	Super::OnRep_Score();
	if (OnPlayerScoreChange.IsBound())
	{
		OnPlayerScoreChange.Broadcast(this);
	}
}

void ANMPlayerState::UpdateKillScore(int32 NewScore)
{
	PlayerKills += NewScore;
	SetScore(GetScore() + NewScore);
	OnRep_PlayerKills();
}

void ANMPlayerState::UpdateDeathScore(int32 NewScore)
{
	PlayerDeaths += NewScore;
	OnRep_PlayerDeaths();
}

void ANMPlayerState::OnRep_PlayerKills()
{
	if (OnPlayerScoreChange.IsBound())
	{
		OnPlayerScoreChange.Broadcast(this);
	}
}

void ANMPlayerState::OnRep_PlayerDeaths()
{
	if (OnPlayerScoreChange.IsBound())
	{
		OnPlayerScoreChange.Broadcast(this);
	}
}