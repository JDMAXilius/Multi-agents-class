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
		ensureMsgf(false, TEXT("BRPlayerState '%s': StartupAbilitySet is unset, so this player is granted no abilities and every ability input will reach an ASC with no matching spec. Expected [/Script/Breachpoint.BRPlayerState] StartupAbilitySet in Config/DefaultGame.ini."),
			*GetName());
		return;
	}

	const UBRAbilitySet* Set = StartupAbilitySet.LoadSynchronous();
	if (!Set)
	{
		ensureMsgf(false, TEXT("BRPlayerState '%s': StartupAbilitySet '%s' failed to load. The ini path is set but does not resolve to an ability set asset."),
			*GetName(), *StartupAbilitySet.ToSoftObjectPath().ToString());
		return;
	}

	Set->GiveToAbilitySystem(AbilitySystemComponent, this, StartupAbilityHandles, StartupEffectHandles);

	ensureMsgf(StartupAbilityHandles.Num() > 0, TEXT("BRPlayerState '%s': ability set '%s' granted zero abilities. The asset loaded but its Abilities array is empty or every entry failed to resolve."),
		*GetName(), *GetNameSafe(Set));
}

void ABRPlayerState::ClearStartupLoadout()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	UBRAbilitySet::TakeFromAbilitySystem(AbilitySystemComponent, StartupAbilityHandles, StartupEffectHandles);
}
