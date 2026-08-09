// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/OSHealthComponent.h"
#include "AbilitySystemComponent.h"
#include "Characters/OSCharacter.h"
#include "GAS/Attributes/OSAttributeSet.h"


// Sets default values for this component's properties
UOSHealthComponent::UOSHealthComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	// ...
}

// Cache owner as AOSCharacter for ASC/AttributeSet access.
void UOSHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Owner = Cast<AOSCharacter>(GetOwner());
}

// True if owner's ASC has any of the configured DeathTags (e.g. Gameplay.State.IsDead).
bool UOSHealthComponent::IsDead() const
{
	AOSCharacter* const C = Owner.Get();
	if (!IsValid(C))
	{
		return false;
	}
	if (UAbilitySystemComponent* const ASC = C->GetAbilitySystemComponent())
	{
		return ASC->HasAnyMatchingGameplayTags(DeathTags);
	}
	return false;
}

