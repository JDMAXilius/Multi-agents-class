// Fill out your copyright notice in the Description page of Project Settings.

#include "InventorySystem/StarMap.h"

AStarMap::AStarMap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AStarMap::BeginPlay()
{
	Super::BeginPlay();
}

void AStarMap::Interact_Implementation(AActor* Interactor)
{
	Super::Interact_Implementation(Interactor);
}

void AStarMap::ShowUI_Implementation(AActor* Interactor)
{
	Super::ShowUI_Implementation(Interactor);

}

void AStarMap::HideUI_Implementation(AActor* Interactor)
{
	Super::HideUI_Implementation(Interactor);

}

void AStarMap::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}