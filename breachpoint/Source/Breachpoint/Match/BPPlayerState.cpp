#include "Match/BPPlayerState.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

ABPPlayerState::ABPPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<UBRAbilitySystemComponent>(this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	SetNetUpdateFrequency(100.f);
}

UAbilitySystemComponent* ABPPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABPPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, nullptr);
	}
}

void ABPPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABPPlayerState, Kills);
	DOREPLIFETIME(ABPPlayerState, Deaths);
}

void ABPPlayerState::AddKill()
{
	if (HasAuthority())
	{
		++Kills;
	}
}

void ABPPlayerState::AddDeath()
{
	if (HasAuthority())
	{
		++Deaths;
	}
}
