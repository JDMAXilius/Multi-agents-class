// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Components/NMInteractionComponent.h"
#include "GameFramework/PlayerState.h"

// Sets default values for this component's properties
UNMInteractionComponent::UNMInteractionComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UNMInteractionComponent::Interact()
{
	if (GetOwningPawn())
	{
		//logic
	}
}

APawn* UNMInteractionComponent::GetOwningPawn()
{
	if (!OwningPawn.Get())
	{
		if (AActor* OwningActor = GetOwner())
		{
			OwningPawn = Cast<APawn>(OwningActor);
		}
	}

	return OwningPawn.Get();
}

APlayerController* UNMInteractionComponent::GetOwningPlayerController()
{
	if (!OwningPlayerController.Get())
	{
		if (GetOwningPawn())
		{
			if (const APlayerState* OwningPlayerState = GetOwningPawn()->GetPlayerState())
			{
				OwningPlayerController = OwningPlayerState->GetPlayerController();
			}
		}
	}

	return OwningPlayerController.Get();
}

void UNMInteractionComponent::DestroyComponent(bool bPromoteChildren)
{
	Super::DestroyComponent(bPromoteChildren);
}

// Called when the game starts
void UNMInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	
	if (GetOwningPawn())
	{
		if (const APlayerState* OwningPlayerState = GetOwningPawn()->GetPlayerState())
		{
			OwningPlayerController = OwningPlayerState->GetPlayerController();
		}
	}
	
}

// Called every frame
void UNMInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}
