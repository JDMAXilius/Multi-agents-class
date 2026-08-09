// Copyright Zollpa LLC.


#include "ZoransPawnBase.h"
#include "Net/UnrealNetwork.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayEffect.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayAbility.h"
#include "ZoransResistance/AbilitySystem/Attributes/ZoransAttributeSet.h"

AZoransPawnBase::AZoransPawnBase()
{
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UZoransAbilitySystemComponent>(TEXT("AbilitySystemComonent"));
}

void AZoransPawnBase::BeginPlay()
{
	Super::BeginPlay();

	InitAbilitySystem();
}

void AZoransPawnBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransPawnBase, TeamAssignment, COND_None, REPNOTIFY_Always);
}

UAbilitySystemComponent* AZoransPawnBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

void AZoransPawnBase::SetTeamAssignment(const int32 InIndex)
{
	if (GetNetMode() < NM_Client && TeamAssignment != InIndex)
	{
		TeamAssignment = InIndex;

		OnRep_TeamAssignment();
	}
}

int32 AZoransPawnBase::GetTeamAssignment() const
{
	return TeamAssignment;
}

void AZoransPawnBase::InitAbilitySystem()
{
	if (GetNetMode() < NM_Client && AbilitySystemComponent.Get())
	{
		for (const TSubclassOf<UZoransAttributeSet>& AttributeSet : DefaultAttributeSets)
		{
			UAttributeSet* NewAttributeSet = NewObject<UZoransAttributeSet>(this, AttributeSet);

			AbilitySystemComponent->AddSpawnedAttribute(NewAttributeSet);
		}

		AbilitySystemComponent->ForceReplication();

		for (const TSubclassOf<UZoransGameplayEffect>& GameplayEffect : DefaultEffects)
		{
			FGameplayEffectContextHandle EffectContextHandle = AbilitySystemComponent->MakeEffectContext();
			EffectContextHandle.AddSourceObject(this);

			if (const FGameplayEffectSpecHandle GameplayEffectSpecHandle = AbilitySystemComponent->MakeOutgoingSpec(GameplayEffect, 1, EffectContextHandle); GameplayEffectSpecHandle.IsValid())
			{
				AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*GameplayEffectSpecHandle.Data.Get(), AbilitySystemComponent.Get());
			}
		}

		for (const TSubclassOf<UZoransGameplayAbility>& GameplayAbility : DefaultGameplayAbilities)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(GameplayAbility, 1, 0, this));
		}
	}
}