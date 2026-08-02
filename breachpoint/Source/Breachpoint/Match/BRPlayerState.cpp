#include "Match/BRPlayerState.h"

#include "AbilitySystem/BRAbilitySet.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/BRAttributeSet.h"
#include "Core/BRCore.h"

namespace
{
	constexpr float BRPlayerStateNetUpdateHz = 100.f;
}

ABRPlayerState::ABRPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<UBRAbilitySystemComponent>(this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = ObjectInitializer.CreateDefaultSubobject<UBRAttributeSet>(this, TEXT("AttributeSet"));

	SetNetUpdateFrequency(BRPlayerStateNetUpdateHz);
}

UAbilitySystemComponent* ABRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABRPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, nullptr);
	}
}

void ABRPlayerState::GiveStartupLoadout()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	// Already granted and still live. Possession runs on every respawn and on seamless
	// travel, so without this the same set would be granted again and every ability would
	// hold two specs.
	if (StartupAbilityHandles.Num() > 0)
	{
		return;
	}

	if (StartupAbilitySet.IsNull())
	{
		return;
	}

	const UBRAbilitySet* Set = StartupAbilitySet.LoadSynchronous();
	if (!Set)
	{
		return;
	}

	Set->GiveToAbilitySystem(AbilitySystemComponent, this, StartupAbilityHandles, StartupEffectHandles);
}

void ABRPlayerState::ClearStartupLoadout()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	UBRAbilitySet::TakeFromAbilitySystem(AbilitySystemComponent, StartupAbilityHandles, StartupEffectHandles);
}
