// Breachpoint. The PlayerState: the ASC and the attribute set live here.
#include "Match/BRPlayerState.h"

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
	else
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRPlayerState '%s': no AbilitySystemComponent after construction. Nothing in the combat system will work for this player."),
			*GetName());
	}
}
